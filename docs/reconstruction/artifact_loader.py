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
        artifact = _load_ply(abs_path)
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


def _load_ply(path: str) -> LoadedReconstructionArtifact:
    with open(path, "rb") as fp:
        blob = fp.read()

    header_end = blob.find(b"end_header")
    if header_end < 0:
        raise ValueError("Unsupported PLY: missing end_header.")
    newline_end = blob.find(b"\n", header_end)
    if newline_end < 0:
        newline_end = len(blob)
    header_bytes = blob[:newline_end + 1]
    payload = blob[newline_end + 1:]
    header_lines = header_bytes.decode("ascii", errors="strict").splitlines()

    fmt = None
    vertex_count = None
    properties: list[str] = []
    in_vertex_element = False
    for line in header_lines:
        stripped = line.strip()
        if stripped.startswith("format "):
            parts = stripped.split()
            if len(parts) >= 2:
                fmt = parts[1]
        elif stripped.startswith("element vertex "):
            vertex_count = int(stripped.split()[-1])
            in_vertex_element = True
        elif stripped.startswith("element "):
            in_vertex_element = False
        elif in_vertex_element and stripped.startswith("property "):
            parts = stripped.split()
            if len(parts) >= 3:
                properties.append(parts[-1])

    if vertex_count is None or fmt is None:
        raise ValueError("Unsupported PLY: expected vertex count and format.")

    if fmt == "ascii":
        return _load_ascii_ply_payload(path, payload, vertex_count)
    if fmt == "binary_little_endian":
        return _load_binary_little_endian_ply(path, payload, vertex_count, properties)
    raise ValueError(f"Unsupported PLY format: {fmt}")


def _load_ascii_ply_payload(path: str, payload: bytes, vertex_count: int) -> LoadedReconstructionArtifact:
    lines = payload.decode("utf-8").splitlines()
    points = []
    colors = []
    for line in lines[:vertex_count]:
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


def _load_binary_little_endian_ply(
    path: str,
    payload: bytes,
    vertex_count: int,
    properties: list[str],
) -> LoadedReconstructionArtifact:
    if len(properties) < 3 or properties[:3] != ["x", "y", "z"]:
        raise ValueError("Unsupported binary PLY: expected x/y/z as the first vertex properties.")

    property_formats: dict[str, str] = {
        "char": "i1",
        "uchar": "u1",
        "short": "i2",
        "ushort": "u2",
        "int": "i4",
        "uint": "u4",
        "float": "f4",
        "double": "f8",
    }

    # Re-parse types from header to keep the public function signature small.
    with open(path, "rb") as fp:
        header_blob = fp.read().split(b"end_header", 1)[0] + b"end_header"
    header_lines = header_blob.decode("ascii", errors="strict").splitlines()
    descriptors: list[tuple[str, str]] = []
    in_vertex_element = False
    for line in header_lines:
        stripped = line.strip()
        if stripped.startswith("element vertex "):
            in_vertex_element = True
            continue
        if stripped.startswith("element "):
            in_vertex_element = False
        if in_vertex_element and stripped.startswith("property "):
            parts = stripped.split()
            if len(parts) >= 3:
                prop_type = parts[1]
                prop_name = parts[2]
                if prop_type == "list":
                    raise ValueError("Unsupported binary PLY: list properties are not supported.")
                fmt = property_formats.get(prop_type)
                if fmt is None:
                    raise ValueError(f"Unsupported binary PLY property type: {prop_type}")
                descriptors.append((prop_name, fmt))

    dtype = np.dtype([(name, "<" + fmt) for name, fmt in descriptors])
    expected_size = vertex_count * dtype.itemsize
    if len(payload) < expected_size:
        raise ValueError("Unsupported binary PLY: payload shorter than expected vertex data.")

    data = np.frombuffer(payload[:expected_size], dtype=dtype, count=vertex_count)
    points = np.column_stack([data["x"], data["y"], data["z"]]).astype(np.float64, copy=False)

    if all(name in data.dtype.names for name in ("red", "green", "blue")):
        colors = np.column_stack([data["red"], data["green"], data["blue"]]).astype(np.uint8, copy=False)
    else:
        colors = np.full((vertex_count, 3), 255, dtype=np.uint8)

    return LoadedReconstructionArtifact(
        points=np.asarray(points, dtype=np.float64),
        colors=np.asarray(colors, dtype=np.uint8),
        output_format="ply",
        source_path=path,
    )
