from __future__ import annotations

import os
import sys
import tempfile
import unittest
import json
import threading
import time
import urllib.request

DOCS_DIR = os.path.dirname(os.path.abspath(__file__))
if DOCS_DIR not in sys.path:
    sys.path.insert(0, DOCS_DIR)

from reconstruction.inbox_monitor import InboxMonitor, InboxMonitorConfig
from reconstruction.map_accumulator_cli import build_map_state, main as map_cli_main, serve_live_map
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

    def test_live_map_state_reports_status_panel_fields(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            manifest_path = os.path.join(tmpdir, "map_manifest.json")
            first_ply = os.path.join(tmpdir, "first.ply")
            second_ply = os.path.join(tmpdir, "second.ply")
            write_ascii_ply(first_ply, 0.0)
            write_ascii_ply(second_ply, 1.0)
            manifest = create_manifest("map-live")
            manifest.append_chunk(MapChunk(
                chunk_id="chunk-1",
                job_id="job-1",
                image_set_id="set-1",
                artifact_ref="first.ply",
                output_format="ply",
            ))
            manifest.append_chunk(MapChunk(
                chunk_id="chunk-2",
                job_id="job-2",
                image_set_id="set-2",
                artifact_ref="second.ply",
                output_format="ply",
            ))
            save_manifest(manifest, manifest_path)
            state = build_map_state(manifest_path, max_points_per_chunk=10)
            self.assertEqual(state["chunk_count"], 2)
            self.assertEqual(state["rendered_point_count"], 4)
            self.assertTrue(state["last_updated"])

    def test_inbox_monitor_dispatches_and_moves_files(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            inbox = os.path.join(tmpdir, "inbox")
            processed = os.path.join(tmpdir, "processed")
            rejected = os.path.join(tmpdir, "rejected")
            os.makedirs(inbox, exist_ok=True)
            calls: list[tuple[list[str], str]] = []

            def dispatch(images: list[str], image_set_id: str) -> dict:
                calls.append((list(images), image_set_id))
                return {"job_id": image_set_id, "status": "success"}

            for idx in range(3):
                with open(os.path.join(inbox, f"{idx:03d}.png"), "wb") as fp:
                    fp.write(b"\x89PNG\r\n\x1a\nfakepngdata")

            monitor = InboxMonitor(
                InboxMonitorConfig(
                    inbox_dir=inbox,
                    processed_dir=processed,
                    rejected_dir=rejected,
                    chunk_size=3,
                ),
                dispatch,
            )
            monitor.run_once()

            self.assertEqual(len(calls), 1)
            self.assertEqual(len(os.listdir(inbox)), 0)
            self.assertEqual(len(os.listdir(processed)), 3)
            self.assertEqual(len(monitor.state.buffer), 0)

    def test_inbox_monitor_rejects_invalid_images_without_stopping(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            inbox = os.path.join(tmpdir, "inbox")
            processed = os.path.join(tmpdir, "processed")
            rejected = os.path.join(tmpdir, "rejected")
            os.makedirs(inbox, exist_ok=True)
            with open(os.path.join(inbox, "bad.png"), "wb") as fp:
                fp.write(b"not-an-image")
            monitor = InboxMonitor(
                InboxMonitorConfig(
                    inbox_dir=inbox,
                    processed_dir=processed,
                    rejected_dir=rejected,
                    chunk_size=2,
                ),
                lambda images, image_set_id: {},
            )
            monitor.run_once()
            self.assertEqual(len(os.listdir(rejected)), 1)
            self.assertEqual(len(os.listdir(inbox)), 0)


if __name__ == "__main__":
    unittest.main()
