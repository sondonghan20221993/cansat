from __future__ import annotations

import json
import os
import struct
from typing import Any, Dict

from reconstruction.exporters.base import ReconstructionExporter


class GlbExporter(ReconstructionExporter):
    def __init__(self, artifact_root: str = "artifacts/reconstruction") -> None:
        self._artifact_root = artifact_root

    def export(self, normalized_scene: Any, artifact_name: str) -> Dict[str, Any]:
        os.makedirs(self._artifact_root, exist_ok=True)
        artifact_path = os.path.join(self._artifact_root, f"{artifact_name}.glb")
        points = normalized_scene.get("points") or []
        colors = normalized_scene.get("colors") or []
        if not points:
            raise RuntimeError("GLB export requires a non-empty point set.")
        if colors and len(colors) != len(points):
            raise RuntimeError("Point/color count mismatch in normalized scene.")
        if not colors:
            colors = [(255, 255, 255)] * len(points)
        self._write_glb_points(artifact_path, points, colors)
        return {
            "output_ref": artifact_path,
            "output_format": self.format_name,
        }

    @property
    def format_name(self) -> str:
        return "glb"

    def _write_glb_points(self, path: str, points: Any, colors: Any) -> None:
        position_bytes = b"".join(struct.pack("<fff", *map(float, point)) for point in points)
        color_bytes = b"".join(struct.pack("<BBB", *map(int, color)) for color in colors)
        bin_chunk = self._pad4(position_bytes + color_bytes, b"\x00")

        min_vals = [min(float(p[i]) for p in points) for i in range(3)]
        max_vals = [max(float(p[i]) for p in points) for i in range(3)]
        json_dict = {
            "asset": {"version": "2.0", "generator": "cansat_2 glb exporter"},
            "scene": 0,
            "scenes": [{"nodes": [0]}],
            "nodes": [{"mesh": 0}],
            "meshes": [{
                "primitives": [{
                    "attributes": {"POSITION": 0, "COLOR_0": 1},
                    "mode": 0,
                }]
            }],
            "buffers": [{"byteLength": len(bin_chunk)}],
            "bufferViews": [
                {"buffer": 0, "byteOffset": 0, "byteLength": len(position_bytes), "target": 34962},
                {"buffer": 0, "byteOffset": len(position_bytes), "byteLength": len(color_bytes), "target": 34962},
            ],
            "accessors": [
                {
                    "bufferView": 0,
                    "byteOffset": 0,
                    "componentType": 5126,
                    "count": len(points),
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
        json_chunk = self._pad4(json.dumps(json_dict, separators=(",", ":")).encode("utf-8"), b" ")
        total_length = 12 + 8 + len(json_chunk) + 8 + len(bin_chunk)
        glb = bytearray()
        glb.extend(struct.pack("<4sII", b"glTF", 2, total_length))
        glb.extend(struct.pack("<I4s", len(json_chunk), b"JSON"))
        glb.extend(json_chunk)
        glb.extend(struct.pack("<I4s", len(bin_chunk), b"BIN\x00"))
        glb.extend(bin_chunk)

        with open(path, "wb") as fp:
            fp.write(glb)

    def _pad4(self, data: bytes, pad_byte: bytes) -> bytes:
        remainder = len(data) % 4
        if remainder == 0:
            return data
        return data + (pad_byte * (4 - remainder))
