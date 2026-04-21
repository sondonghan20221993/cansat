from __future__ import annotations

import json
import os
import struct
from dataclasses import dataclass, field
from typing import Any

import numpy as np


@dataclass
class LoadedReconstructionArtifact:
    points: np.ndarray
    colors: np.ndarray
    output_format: str
    source_path: str
    camera_trajectory: list[dict[str, Any]] = field(default_factory=list)
    quality: dict[str, Any] = field(default_factory=dict)


def load_reconstruction_artifact(path: str, metadata_path: str | None = None) -> LoadedReconstructionArtifact:
    """Load a server-side reconstruction artifact for fixed-frame visualization."""
    abs_path = os.path.abspath(path)
    ext = os.path.splitext(abs_path)[1].lower()
    if ext == ".glb":
        artifact = _load_point_glb(abs_path)
    elif ext == ".ply":
        artifact = _load_ascii_ply(abs_path)
    else:
        raise ValueError(f"Unsupported reconstruction artifact format: {ext or '<none>'}")

    metadata = _load_metadata(metadata_path)
    artifact.camera_trajectory = metadata.get("camera_trajectory", [])
    artifact.quality.update(metadata.get("quality", {}))
    artifact.quality.update({
        "artifact_source": abs_path,
        "artifact_format": artifact.output_format,
        "point_count": int(len(artifact.points)),
    })
    return artifact


def _load_metadata(metadata_path: str | None) -> dict[str, Any]:
    if not metadata_path:
        return {}
    with open(os.path.abspath(metadata_path), "r", encoding="utf-8") as fp:
        payload = json.load(fp)
    if not isinstance(payload, dict):
        raise ValueError("Artifact metadata JSON must be an object.")
    return payload


def _load_point_glb(path: str) -> LoadedReconstructionArtifact:
    with open(path, "rb") as fp:
        blob = fp.read()

    if len(blob) < 20:
        raise ValueError("Invalid GLB: file is too small.")
    magic, version, total_length = struct.unpack_from("<4sII", blob, 0)
    if magic != b"glTF" or version != 2:
        raise ValueError("Invalid GLB: expected glTF 2.0 binary header.")
    if total_length > len(blob):
        raise ValueError("Invalid GLB: declared length exceeds file size.")

    offset = 12
    json_chunk = None
    bin_chunk = None
    while offset + 8 <= total_length:
        chunk_len, chunk_type = struct.unpack_from("<I4s", blob, offset)
        offset += 8
        chunk_data = blob[offset:offset + chunk_len]
        offset += chunk_len
        if chunk_type == b"JSON":
            json_chunk = chunk_data
        elif chunk_type == b"BIN\x00":
            bin_chunk = chunk_data

    if json_chunk is None or bin_chunk is None:
        raise ValueError("Invalid GLB: missing JSON or BIN chunk.")

    gltf = json.loads(json_chunk.decode("utf-8").rstrip(" \x00"))
    primitive = gltf["meshes"][0]["primitives"][0]
    position_accessor = gltf["accessors"][primitive["attributes"]["POSITION"]]
    point_count = int(position_accessor["count"])
    position_view = gltf["bufferViews"][position_accessor["bufferView"]]
    position_offset = int(position_view.get("byteOffset", 0)) + int(position_accessor.get("byteOffset", 0))
    position_bytes = bin_chunk[position_offset:position_offset + point_count * 12]
    points = np.frombuffer(position_bytes, dtype="<f4").reshape(point_count, 3).astype(np.float64)

    colors = np.full((point_count, 3), 255, dtype=np.uint8)
    color_accessor_idx = primitive["attributes"].get("COLOR_0")
    if color_accessor_idx is not None:
        color_accessor = gltf["accessors"][color_accessor_idx]
        color_view = gltf["bufferViews"][color_accessor["bufferView"]]
        color_offset = int(color_view.get("byteOffset", 0)) + int(color_accessor.get("byteOffset", 0))
        color_bytes = bin_chunk[color_offset:color_offset + point_count * 3]
        colors = np.frombuffer(color_bytes, dtype=np.uint8).reshape(point_count, 3).copy()

    return LoadedReconstructionArtifact(
        points=points,
        colors=colors,
        output_format="glb",
        source_path=path,
    )


def _load_ascii_ply(path: str) -> LoadedReconstructionArtifact:
    with open(path, "r", encoding="utf-8") as fp:
        lines = fp.readlines()

    vertex_count = None
    header_end = None
    for idx, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("element vertex "):
            vertex_count = int(stripped.split()[-1])
        if stripped == "end_header":
            header_end = idx + 1
            break
    if vertex_count is None or header_end is None:
        raise ValueError("Unsupported PLY: expected ASCII vertex-only point cloud.")

    points = []
    colors = []
    for line in lines[header_end:header_end + vertex_count]:
        values = line.split()
        if len(values) < 3:
            continue
        points.append([float(values[0]), float(values[1]), float(values[2])])
        if len(values) >= 6:
            colors.append([int(values[3]), int(values[4]), int(values[5])])
        else:
            colors.append([255, 255, 255])

    return LoadedReconstructionArtifact(
        points=np.asarray(points, dtype=np.float64),
        colors=np.asarray(colors, dtype=np.uint8),
        output_format="ply",
        source_path=path,
    )
