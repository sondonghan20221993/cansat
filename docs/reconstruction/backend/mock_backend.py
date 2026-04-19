"""
reconstruction/backend/mock_backend.py

Prototype reconstruction backend.

This backend does not perform real 3D reconstruction. Instead, it builds a
simple coordinate-frame scene and a synthetic point set derived from the input
image count, then exports it as a minimal GLB artifact.

Purpose:
  - prove the end-to-end reconstruction pipeline shape
  - provide something visible in a 3D viewer quickly
  - preserve the replaceable backend contract
"""

from __future__ import annotations

import json
import math
import os
import struct
from typing import Any, Dict, List, Optional, Tuple

from reconstruction.backend.base import ReconstructionBackend
from reconstruction.models.job import ImageDescriptor


class MockReconstructionBackend(ReconstructionBackend):
    """Synthetic backend for early prototype demos."""

    def __init__(self) -> None:
        self._loaded = False

    def load(self) -> None:
        self._loaded = True

    def unload(self) -> None:
        self._loaded = False

    def preprocess(
        self,
        images: List[ImageDescriptor],
        aux_pose: Optional[Any] = None,
    ) -> Any:
        if not self._loaded:
            raise RuntimeError("MockReconstructionBackend must be loaded before preprocess().")
        return {
            "images": images,
            "aux_pose": aux_pose,
            "image_count": len(images),
        }

    def infer(self, preprocessed: Any) -> Any:
        image_count = max(1, int(preprocessed["image_count"]))
        image_ids = [img.image_id for img in preprocessed["images"]]
        return {
            "image_count": image_count,
            "image_ids": image_ids,
        }

    def postprocess(
        self,
        raw_result: Any,
        output_format: str,
        job_id: str,
        image_set_id: Any,
    ) -> Dict[str, Any]:
        if output_format.lower() != "glb":
            raise RuntimeError(
                f"MockReconstructionBackend currently supports only glb for prototype output, got: {output_format}"
            )

        artifact_dir = os.path.join("artifacts", "reconstruction")
        os.makedirs(artifact_dir, exist_ok=True)
        output_ref = os.path.join(artifact_dir, f"{job_id}.glb")

        image_count = raw_result["image_count"]
        image_ids = raw_result["image_ids"]
        positions, colors = _build_mock_scene(image_count)
        _write_glb_points(output_ref, positions, colors)

        quality_indicators = {
            "prototype_backend": "mock",
            "images_used": image_count,
            "synthetic_point_count": len(positions),
            "image_ids": image_ids,
        }
        return {
            "output_ref": output_ref,
            "output_format": output_format,
            "quality_indicators": quality_indicators,
        }

    @property
    def backend_name(self) -> str:
        return "mock"


def _build_mock_scene(image_count: int) -> Tuple[List[Tuple[float, float, float]], List[Tuple[int, int, int]]]:
    """
    Build a synthetic scene that is easy to spot in a viewer.

    Contents:
      - origin marker
      - X/Y/Z axis point markers
      - synthetic circular point cloud based on image count
    """
    positions: List[Tuple[float, float, float]] = []
    colors: List[Tuple[int, int, int]] = []

    # Origin and axis markers
    axis_points = [
        ((0.0, 0.0, 0.0), (255, 255, 255)),  # origin
        ((1.0, 0.0, 0.0), (255, 0, 0)),      # +X
        ((0.0, 1.0, 0.0), (0, 255, 0)),      # +Y
        ((0.0, 0.0, 1.0), (0, 128, 255)),    # +Z
    ]
    for pos, col in axis_points:
        positions.append(pos)
        colors.append(col)

    radius = 1.5
    for idx in range(max(4, image_count * 8)):
        theta = (2.0 * math.pi * idx) / max(4, image_count * 8)
        z = 0.15 * math.sin(theta * 2.0)
        ring = radius + 0.05 * math.cos(theta * 3.0)
        positions.append((ring * math.cos(theta), ring * math.sin(theta), z))
        colors.append((255, 180, 0))

    return positions, colors


def _write_glb_points(
    path: str,
    positions: List[Tuple[float, float, float]],
    colors: List[Tuple[int, int, int]],
) -> None:
    """
    Write a minimal GLB file containing a point primitive.

    Uses:
      - POSITION: float32 vec3
      - COLOR_0:  normalized uint8 vec3
      - primitive mode: POINTS
    """
    position_bytes = b"".join(struct.pack("<fff", *pos) for pos in positions)
    color_bytes = b"".join(struct.pack("<BBB", *col) for col in colors)
    bin_chunk = position_bytes + color_bytes
    bin_chunk = _pad4(bin_chunk, b"\x00")

    min_vals = [min(p[i] for p in positions) for i in range(3)]
    max_vals = [max(p[i] for p in positions) for i in range(3)]

    json_dict = {
        "asset": {"version": "2.0", "generator": "cansat_2 mock backend"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{
            "primitives": [{
                "attributes": {
                    "POSITION": 0,
                    "COLOR_0": 1,
                },
                "mode": 0,
            }]
        }],
        "buffers": [{"byteLength": len(bin_chunk)}],
        "bufferViews": [
            {
                "buffer": 0,
                "byteOffset": 0,
                "byteLength": len(position_bytes),
                "target": 34962,
            },
            {
                "buffer": 0,
                "byteOffset": len(position_bytes),
                "byteLength": len(color_bytes),
                "target": 34962,
            },
        ],
        "accessors": [
            {
                "bufferView": 0,
                "byteOffset": 0,
                "componentType": 5126,
                "count": len(positions),
                "type": "VEC3",
                "min": min_vals,
                "max": max_vals,
            },
            {
                "bufferView": 1,
                "byteOffset": 0,
                "componentType": 5121,
                "count": len(colors),
                "type": "VEC3",
                "normalized": True,
            },
        ],
    }
    json_chunk = json.dumps(json_dict, separators=(",", ":")).encode("utf-8")
    json_chunk = _pad4(json_chunk, b" ")

    total_length = 12 + 8 + len(json_chunk) + 8 + len(bin_chunk)
    glb = bytearray()
    glb.extend(struct.pack("<4sII", b"glTF", 2, total_length))
    glb.extend(struct.pack("<I4s", len(json_chunk), b"JSON"))
    glb.extend(json_chunk)
    glb.extend(struct.pack("<I4s", len(bin_chunk), b"BIN\x00"))
    glb.extend(bin_chunk)

    with open(path, "wb") as fp:
        fp.write(glb)


def _pad4(data: bytes, pad_byte: bytes) -> bytes:
    remainder = len(data) % 4
    if remainder == 0:
        return data
    return data + (pad_byte * (4 - remainder))
