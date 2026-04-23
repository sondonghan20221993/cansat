from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any, Literal


AlignmentStatus = Literal["ALIGNED", "PARTIAL_ALIGNMENT", "UNALIGNED"]


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


@dataclass
class ChunkTransform:
    scale: float = 1.0
    linear: list[list[float]] = field(default_factory=lambda: [
        [1.0, 0.0, 0.0],
        [0.0, 1.0, 0.0],
        [0.0, 0.0, 1.0],
    ])
    translate: list[float] = field(default_factory=lambda: [0.0, 0.0, 0.0])

    def validate(self) -> None:
        if not isinstance(self.scale, (int, float)):
            raise ValueError("transform.scale must be numeric")
        if len(self.linear) != 3 or any(len(row) != 3 for row in self.linear):
            raise ValueError("transform.linear must be a 3x3 matrix")
        if len(self.translate) != 3:
            raise ValueError("transform.translate must contain 3 values")


@dataclass
class MapChunk:
    chunk_id: str
    job_id: str
    image_set_id: str
    artifact_ref: str
    output_format: str
    alignment_status: AlignmentStatus = "UNALIGNED"
    transform: ChunkTransform | None = None
    invalidated: bool = False
    metadata: dict[str, Any] = field(default_factory=dict)

    def validate(self) -> None:
        if not self.chunk_id:
            raise ValueError("chunk_id must not be empty")
        if not self.artifact_ref:
            raise ValueError("artifact_ref must not be empty")
        if self.output_format not in {"ply", "glb"}:
            raise ValueError("output_format must be 'ply' or 'glb'")
        if self.alignment_status not in {"ALIGNED", "PARTIAL_ALIGNMENT", "UNALIGNED"}:
            raise ValueError("alignment_status is invalid")
        if self.alignment_status == "UNALIGNED":
            if self.transform is not None:
                raise ValueError("transform must be null when alignment_status is UNALIGNED")
        elif self.transform is None:
            raise ValueError("transform is required when alignment_status is aligned or partial")
        else:
            self.transform.validate()


@dataclass
class MapManifest:
    map_id: str
    created_at: str
    updated_at: str
    display_frame_id: str
    chunks: list[MapChunk] = field(default_factory=list)

    def validate(self) -> None:
        if not self.map_id:
            raise ValueError("map_id must not be empty")
        if not self.display_frame_id:
            raise ValueError("display_frame_id must not be empty")
        seen_chunk_ids: set[str] = set()
        seen_job_ids: set[str] = set()
        for chunk in self.chunks:
            chunk.validate()
            if chunk.chunk_id in seen_chunk_ids:
                raise ValueError(f"duplicate chunk_id: {chunk.chunk_id}")
            if chunk.job_id in seen_job_ids:
                raise ValueError(f"duplicate job_id: {chunk.job_id}")
            seen_chunk_ids.add(chunk.chunk_id)
            seen_job_ids.add(chunk.job_id)

    def append_chunk(self, chunk: MapChunk) -> None:
        if any(existing.chunk_id == chunk.chunk_id for existing in self.chunks):
            raise ValueError(f"chunk_id already exists: {chunk.chunk_id}")
        if any(existing.job_id == chunk.job_id for existing in self.chunks):
            raise ValueError(f"job_id already exists: {chunk.job_id}")
        chunk.validate()
        self.chunks.append(chunk)
        self.touch()

    def update_chunk_transform(
        self,
        chunk_id: str,
        transform: ChunkTransform | None,
        alignment_status: AlignmentStatus,
    ) -> None:
        chunk = self.get_chunk(chunk_id)
        if transform is not None:
            transform.validate()
        if alignment_status not in {"ALIGNED", "PARTIAL_ALIGNMENT", "UNALIGNED"}:
            raise ValueError("alignment_status is invalid")
        chunk.transform = transform
        chunk.alignment_status = alignment_status
        self.touch()

    def invalidate_chunk(self, chunk_id: str) -> None:
        chunk = self.get_chunk(chunk_id)
        chunk.invalidated = True
        self.touch()

    def get_chunk(self, chunk_id: str) -> MapChunk:
        for chunk in self.chunks:
            if chunk.chunk_id == chunk_id:
                return chunk
        raise KeyError(f"unknown chunk_id: {chunk_id}")

    def active_chunks(self) -> list[MapChunk]:
        return [chunk for chunk in self.chunks if not chunk.invalidated]

    def touch(self) -> None:
        self.updated_at = utc_now_iso()


def create_manifest(map_id: str, display_frame_id: str = "map") -> MapManifest:
    now = utc_now_iso()
    manifest = MapManifest(
        map_id=map_id,
        created_at=now,
        updated_at=now,
        display_frame_id=display_frame_id,
    )
    manifest.validate()
    return manifest


def load_manifest(path: str) -> MapManifest:
    with open(os.path.abspath(path), "r", encoding="utf-8") as fp:
        payload = json.load(fp)
    manifest = manifest_from_dict(payload)
    manifest.validate()
    return manifest


def save_manifest(manifest: MapManifest, path: str) -> str:
    manifest.validate()
    abs_path = os.path.abspath(path)
    os.makedirs(os.path.dirname(abs_path) or ".", exist_ok=True)
    with open(abs_path, "w", encoding="utf-8") as fp:
        json.dump(manifest_to_dict(manifest), fp, indent=2)
        fp.write("\n")
    return abs_path


def manifest_to_dict(manifest: MapManifest) -> dict[str, Any]:
    return {
        "map_id": manifest.map_id,
        "created_at": manifest.created_at,
        "updated_at": manifest.updated_at,
        "display_frame_id": manifest.display_frame_id,
        "chunks": [
            {
                "chunk_id": chunk.chunk_id,
                "job_id": chunk.job_id,
                "image_set_id": chunk.image_set_id,
                "artifact_ref": chunk.artifact_ref,
                "output_format": chunk.output_format,
                "alignment_status": chunk.alignment_status,
                "transform": None if chunk.transform is None else {
                    "scale": chunk.transform.scale,
                    "linear": chunk.transform.linear,
                    "translate": chunk.transform.translate,
                },
                "invalidated": chunk.invalidated,
                "metadata": chunk.metadata,
            }
            for chunk in manifest.chunks
        ],
    }


def manifest_from_dict(payload: dict[str, Any]) -> MapManifest:
    chunks = []
    for raw in payload.get("chunks", []):
        raw_transform = raw.get("transform")
        transform = None
        if raw_transform is not None:
            transform = ChunkTransform(
                scale=float(raw_transform.get("scale", 1.0)),
                linear=[[float(value) for value in row] for row in raw_transform.get("linear", ChunkTransform().linear)],
                translate=[float(value) for value in raw_transform.get("translate", [0.0, 0.0, 0.0])],
            )
        chunks.append(MapChunk(
            chunk_id=str(raw.get("chunk_id", "")),
            job_id=str(raw.get("job_id", "")),
            image_set_id=str(raw.get("image_set_id", "")),
            artifact_ref=str(raw.get("artifact_ref", "")),
            output_format=str(raw.get("output_format", "")),
            alignment_status=raw.get("alignment_status", "UNALIGNED"),
            transform=transform,
            invalidated=bool(raw.get("invalidated", False)),
            metadata=dict(raw.get("metadata", {})),
        ))
    return MapManifest(
        map_id=str(payload.get("map_id", "")),
        created_at=str(payload.get("created_at", "")),
        updated_at=str(payload.get("updated_at", "")),
        display_frame_id=str(payload.get("display_frame_id", "")),
        chunks=chunks,
    )
