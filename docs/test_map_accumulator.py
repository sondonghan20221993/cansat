from __future__ import annotations

import os
import sys
import tempfile
import unittest
import json

DOCS_DIR = os.path.dirname(os.path.abspath(__file__))
if DOCS_DIR not in sys.path:
    sys.path.insert(0, DOCS_DIR)

from reconstruction.map_accumulator_cli import main as map_cli_main
from reconstruction.map_manifest import ChunkTransform, MapChunk, create_manifest, load_manifest, save_manifest


def write_ascii_ply(path: str, offset: float = 0.0) -> None:
    with open(path, "w", encoding="utf-8") as fp:
        fp.write("ply\n")
        fp.write("format ascii 1.0\n")
        fp.write("element vertex 2\n")
        fp.write("property float x\n")
        fp.write("property float y\n")
        fp.write("property float z\n")
        fp.write("property uchar red\n")
        fp.write("property uchar green\n")
        fp.write("property uchar blue\n")
        fp.write("end_header\n")
        fp.write(f"{offset} 0 0 255 0 0\n")
        fp.write(f"{offset + 1} 0 0 0 255 0\n")


class MapAccumulatorTest(unittest.TestCase):
    def test_manifest_append_update_and_invalidate(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path = os.path.join(tmpdir, "map_manifest.json")
            manifest = create_manifest("map-a", "world")
            manifest.append_chunk(MapChunk(
                chunk_id="chunk-1",
                job_id="job-1",
                image_set_id="set-1",
                artifact_ref="chunk-1.ply",
                output_format="ply",
            ))
            save_manifest(manifest, manifest_path)

            loaded = load_manifest(manifest_path)
            loaded.update_chunk_transform(
                "chunk-1",
                ChunkTransform(
                    scale=2.0,
                    linear=[[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
                    translate=[10.0, 0.0, 0.0],
                ),
                "ALIGNED",
            )
            loaded.invalidate_chunk("chunk-1")
            save_manifest(loaded, manifest_path)

            final = load_manifest(manifest_path)
            self.assertEqual(final.map_id, "map-a")
            self.assertEqual(final.chunks[0].alignment_status, "ALIGNED")
            self.assertTrue(final.chunks[0].invalidated)

    def test_unaligned_chunk_serializes_null_transform_and_duplicate_job_id_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path = os.path.join(tmpdir, "map_manifest.json")
            manifest = create_manifest("map-duplicates")
            manifest.append_chunk(MapChunk(
                chunk_id="chunk-1",
                job_id="job-1",
                image_set_id="set-1",
                artifact_ref="chunk-1.ply",
                output_format="ply",
            ))
            with self.assertRaises(ValueError):
                manifest.append_chunk(MapChunk(
                    chunk_id="chunk-2",
                    job_id="job-1",
                    image_set_id="set-2",
                    artifact_ref="chunk-2.ply",
                    output_format="ply",
                ))
            save_manifest(manifest, manifest_path)

            with open(manifest_path, "r", encoding="utf-8") as fp:
                payload = json.load(fp)
            self.assertIsNone(payload["chunks"][0]["transform"])

    def test_cli_renders_multiple_active_chunks_without_mutating_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path = os.path.join(tmpdir, "map_manifest.json")
            first_ply = os.path.join(tmpdir, "first.ply")
            second_ply = os.path.join(tmpdir, "second.ply")
            output_html = os.path.join(tmpdir, "viewer.html")
            write_ascii_ply(first_ply, 0.0)
            write_ascii_ply(second_ply, 10.0)
            with open(first_ply, "rb") as fp:
                before = fp.read()

            self.assertEqual(map_cli_main(["create_manifest", "--manifest", manifest_path, "--map-id", "map-b"]), 0)
            self.assertEqual(map_cli_main([
                "append_chunk",
                "--manifest", manifest_path,
                "--chunk-id", "chunk-1",
                "--job-id", "job-1",
                "--image-set-id", "set-1",
                "--artifact-ref", "first.ply",
                "--output-format", "ply",
            ]), 0)
            self.assertEqual(map_cli_main([
                "append_chunk",
                "--manifest", manifest_path,
                "--chunk-id", "chunk-2",
                "--job-id", "job-2",
                "--image-set-id", "set-2",
                "--artifact-ref", "second.ply",
                "--output-format", "ply",
                "--alignment-status", "ALIGNED",
                "--translate", "1", "2", "3",
            ]), 0)
            self.assertEqual(map_cli_main([
                "render_map",
                "--manifest", manifest_path,
                "--output-html", output_html,
                "--max-points-per-chunk", "10",
            ]), 0)

            self.assertTrue(os.path.exists(output_html))
            with open(output_html, "r", encoding="utf-8") as fp:
                html = fp.read()
            self.assertIn("chunk-1", html)
            self.assertIn("chunk-2", html)
            with open(first_ply, "rb") as fp:
                self.assertEqual(fp.read(), before)

    def test_cli_duplicate_job_id_and_missing_update_return_contract_status(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path = os.path.join(tmpdir, "map_manifest.json")
            self.assertEqual(map_cli_main(["create_manifest", "--manifest", manifest_path, "--map-id", "map-c"]), 0)
            self.assertEqual(map_cli_main([
                "append_chunk",
                "--manifest", manifest_path,
                "--chunk-id", "chunk-1",
                "--job-id", "job-1",
                "--image-set-id", "set-1",
                "--artifact-ref", "first.ply",
                "--output-format", "ply",
            ]), 0)
            self.assertEqual(map_cli_main([
                "append_chunk",
                "--manifest", manifest_path,
                "--chunk-id", "chunk-2",
                "--job-id", "job-1",
                "--image-set-id", "set-2",
                "--artifact-ref", "second.ply",
                "--output-format", "ply",
            ]), 0)
            self.assertEqual(map_cli_main([
                "update_chunk_transform",
                "--manifest", manifest_path,
                "--chunk-id", "missing",
                "--alignment-status", "ALIGNED",
            ]), 0)


if __name__ == "__main__":
    unittest.main()
