from __future__ import annotations

import os
import sys
import tempfile
import unittest
import json
import struct
import threading
from unittest.mock import patch

DOCS_DIR = os.path.dirname(os.path.abspath(__file__))
if DOCS_DIR not in sys.path:
    sys.path.insert(0, DOCS_DIR)

from reconstruction.backends.dust3r_backend import Dust3rBackend
from reconstruction.backends.feature_sfm_backend import FeatureSfmBackend
from reconstruction.artifact_loader import load_reconstruction_artifact
from reconstruction.client.server_client import ServerClient
from reconstruction.core.orchestrator import ReconstructionOrchestrator
from reconstruction.exporters.glb_exporter import GlbExporter
from reconstruction.models.job import ImageDescriptor, JobStatus, ReconstructionRequest, SessionTransformUpdate
from reconstruction.models.wire import (
    request_from_dict,
    request_to_dict,
    response_from_dict,
    response_to_dict,
    session_response_from_dict,
    session_response_to_dict,
)
from reconstruction.client.session_http_client import SessionHttpClient
from reconstruction.server.service import ReconstructionService
from reconstruction.server.http_server import make_server
from reconstruction.validation.image_validator import ImageValidator
from reconstruction.map_accumulator_cli import build_session_state


class ReconstructionSkeletonTest(unittest.TestCase):
    def _make_image(self, path: str, image_id: str = "img-1") -> ImageDescriptor:
        return ImageDescriptor(image_id=image_id, timestamp=1, source_path=path, metadata={})

    def test_validator_rejects_missing_path(self) -> None:
        validator = ImageValidator(min_image_count=1)
        report = validator.validate([ImageDescriptor(image_id="x", timestamp=1, source_path="", metadata={})])
        self.assertFalse(report.is_valid)
        self.assertEqual(len(report.accepted), 0)
        self.assertEqual(len(report.rejected), 1)

    def test_orchestrator_blocks_when_minimum_images_not_met(self) -> None:
        validator = ImageValidator(min_image_count=2)
        service = ReconstructionService(Dust3rBackend(), GlbExporter())
        client = ServerClient(service)
        orchestrator = ReconstructionOrchestrator(validator, client)

        with tempfile.NamedTemporaryFile(delete=False) as fp:
            fp.write(b"not-a-real-image-but-a-real-file")
            image_path = fp.name
        try:
            result = orchestrator.run(
                images=[self._make_image(image_path)],
                image_set_id="set-a",
                output_format="glb",
            )
        finally:
            os.unlink(image_path)

        self.assertEqual(result.status, JobStatus.FAILED)
        self.assertEqual(result.error_code, "INSUFFICIENT_VALID_IMAGES")

    def test_job_id_is_preserved_between_submit_and_response(self) -> None:
        service = ReconstructionService(Dust3rBackend(), GlbExporter())
        client = ServerClient(service)
        with tempfile.NamedTemporaryFile(delete=False) as fp:
            fp.write(b"real-file")
            image_path = fp.name
        try:
            request = ReconstructionRequest(
                image_set_id="set-b",
                images=[self._make_image(image_path)],
                output_format="glb",
            )
            job_id = client.submit(request)
            response = client.fetch_result(job_id)
        finally:
            os.unlink(image_path)

        self.assertEqual(job_id, request.job_id)
        self.assertEqual(response.job_id, request.job_id)

    def test_failed_backend_returns_stable_failed_result(self) -> None:
        import cv2
        import numpy as np

        validator = ImageValidator(min_image_count=1)
        service = ReconstructionService(Dust3rBackend(), GlbExporter())
        client = ServerClient(service)
        orchestrator = ReconstructionOrchestrator(validator, client)

        with tempfile.TemporaryDirectory() as tmpdir:
            image_path = os.path.join(tmpdir, "valid.png")
            image = np.zeros((64, 64, 3), dtype=np.uint8)
            cv2.circle(image, (32, 32), 10, (255, 255, 255), -1)
            cv2.imwrite(image_path, image)

            result = orchestrator.run(
                images=[self._make_image(image_path)],
                image_set_id="set-c",
                output_format="glb",
            )
        try:
            pass
        finally:
            pass

        self.assertEqual(result.status, JobStatus.FAILED)
        self.assertEqual(result.error_code, "SERVER_EXECUTION_FAILED")
        self.assertEqual(result.output_format, None)

    def test_feature_backend_can_emit_success_and_glb_path(self) -> None:
        import cv2
        import numpy as np

        validator = ImageValidator(min_image_count=2)
        service = ReconstructionService(FeatureSfmBackend(), GlbExporter())
        client = ServerClient(service)
        orchestrator = ReconstructionOrchestrator(validator, client)

        with tempfile.TemporaryDirectory() as tmpdir:
            base = np.zeros((240, 320, 3), dtype=np.uint8)
            cv2.circle(base, (80, 120), 20, (255, 255, 255), -1)
            cv2.circle(base, (160, 80), 18, (255, 255, 255), -1)
            cv2.circle(base, (220, 170), 16, (255, 255, 255), -1)
            cv2.rectangle(base, (40, 30), (90, 60), (255, 255, 255), -1)
            cv2.rectangle(base, (200, 30), (260, 65), (255, 255, 255), -1)

            paths = []
            for idx, shift in enumerate([0, 4, 8, 12, 16], start=1):
                matrix = np.float32([[1, 0, shift], [0, 1, shift // 2]])
                frame = cv2.warpAffine(base, matrix, (320, 240))
                path = os.path.join(tmpdir, f"{idx}.png")
                cv2.imwrite(path, frame)
                paths.append(path)

            result = orchestrator.run(
                images=[self._make_image(path, f"img-{idx}") for idx, path in enumerate(paths, start=1)],
                image_set_id="set-d",
                output_format="glb",
            )

        self.assertEqual(result.status, JobStatus.SUCCESS)
        self.assertEqual(result.output_format, "glb")
        self.assertTrue(str(result.output_ref).endswith(".glb"))
        self.assertGreater(result.quality.quality_indicators.get("successful_pairs", 0), 0)

        loaded = load_reconstruction_artifact(str(result.output_ref))
        self.assertEqual(loaded.output_format, "glb")
        self.assertGreater(len(loaded.points), 0)
        self.assertEqual(len(loaded.points), len(loaded.colors))

    def test_wire_contract_preserves_request_and_response_identity(self) -> None:
        request = ReconstructionRequest(
            image_set_id="wire-set",
            images=[self._make_image("server/path/image.png", "img-wire")],
            output_format="glb",
        )
        decoded_request = request_from_dict(request_to_dict(request))

        self.assertEqual(decoded_request.job_id, request.job_id)
        self.assertEqual(decoded_request.images[0].source_path, "server/path/image.png")
        self.assertEqual(decoded_request.output_format, "glb")

        response = ReconstructionService(Dust3rBackend(), GlbExporter()).fetch_result(request.job_id)
        decoded_response = response_from_dict(response_to_dict(response))

        self.assertEqual(decoded_response.job_id, request.job_id)
        self.assertEqual(decoded_response.status, JobStatus.PENDING)

    def test_session_lifecycle_contract(self) -> None:
        service = ReconstructionService(Dust3rBackend(), GlbExporter())
        started = service.start_session("seq-1", {"output_policy": "session_state_only"})
        self.assertEqual(started.status, "active")

        appended = service.append_frames(started.session_id, [self._make_image("server/path/a.png", "img-a")])
        self.assertEqual(appended.status, "accepted")
        self.assertEqual(appended.frame_count, 1)

        state = service.get_session_state(started.session_id)
        self.assertEqual(state.frame_count, 1)
        self.assertEqual(state.keyframe_count, 1)
        self.assertEqual(state.alignment_status, "UNALIGNED")
        self.assertEqual(state.tracking_state, "initializing")

        updated = service.update_session_transform(
            started.session_id,
            SessionTransformUpdate(
                alignment_status="ALIGNED",
                world_transform={"scale": 1.0, "linear": [[1, 0, 0], [0, 1, 0], [0, 0, 1]], "translate": [0, 0, 0]},
            ),
        )
        self.assertEqual(updated.status, "updated")
        self.assertEqual(updated.alignment_status, "ALIGNED")

        ended = service.end_session(started.session_id, "finalize")
        self.assertEqual(ended.status, "completed")

        closed_append = service.append_frames(started.session_id, [self._make_image("server/path/b.png", "img-b")])
        self.assertEqual(closed_append.status, "session_closed")

    def test_session_wire_roundtrip(self) -> None:
        payload = {
            "session_id": "session-1",
            "status": "active",
            "frame_count": 3,
            "keyframe_count": 1,
            "rendered_point_count": 42,
            "alignment_status": "UNALIGNED",
            "tracking_state": "tracking",
        }
        decoded = session_response_from_dict(session_response_to_dict(session_response_from_dict(payload)))
        self.assertEqual(decoded.session_id, "session-1")
        self.assertEqual(decoded.status, "active")
        self.assertEqual(decoded.frame_count, 3)
        self.assertEqual(decoded.keyframe_count, 1)
        self.assertEqual(decoded.rendered_point_count, 42)
        self.assertEqual(decoded.tracking_state, "tracking")

    def test_build_session_state_shapes_session_for_live_viewer(self) -> None:
        service = ReconstructionService(Dust3rBackend(), GlbExporter())
        started = service.start_session("seq-2", {"output_policy": "session_state_only"})
        service.append_frames(
            started.session_id,
            [
                self._make_image("server/path/a.png", "img-a"),
                self._make_image("server/path/b.png", "img-b"),
                self._make_image("server/path/c.png", "img-c"),
            ],
        )

        with tempfile.TemporaryDirectory() as tmpdir:
            state_path = os.path.join(tmpdir, "session-state.json")
            with open(state_path, "w", encoding="utf-8") as fp:
                fp.write(json.dumps(session_response_to_dict(service.get_session_state(started.session_id))))
            payload = build_session_state(f"file:///{state_path.replace(os.sep, '/')}", started.session_id, 100)

        self.assertEqual(payload["session_id"], started.session_id)
        self.assertEqual(payload["frame_count"], 3)
        self.assertEqual(len(payload["pose_stream_ref"]["poses"]), 3)
        self.assertEqual(len(payload["map_points"]), 0)

    def test_build_session_state_reads_real_ply_map(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            ply_path = os.path.join(tmpdir, "sample.ply")
            state_path = os.path.join(tmpdir, "session-state.json")
            with open(ply_path, "w", encoding="utf-8") as fp:
                fp.write(
                    "ply\n"
                    "format ascii 1.0\n"
                    "element vertex 2\n"
                    "property float x\n"
                    "property float y\n"
                    "property float z\n"
                    "property uchar red\n"
                    "property uchar green\n"
                    "property uchar blue\n"
                    "end_header\n"
                    "0 0 0 255 0 0\n"
                    "1 2 3 0 255 0\n"
                )
            payload = {
                "session_id": "session-xyz",
                "status": "active",
                "frame_count": 2,
                "keyframe_count": 1,
                "rendered_point_count": 2,
                "pose_stream_ref": {
                    "poses": [
                        {"image_id": "frame-000000", "index": 0, "position": [0, 0, 0], "is_keyframe": True},
                        {"image_id": "frame-000001", "index": 1, "position": [1, 0, 0], "is_keyframe": False},
                    ]
                },
                "map_state_ref": {"path": ply_path},
                "alignment_status": "UNALIGNED",
            }
            with open(state_path, "w", encoding="utf-8") as fp:
                json.dump(payload, fp)
            state = build_session_state(f"file:///{state_path.replace(os.sep, '/')}", "session-xyz", 100)

        self.assertEqual(len(state["map_points"]), 2)
        self.assertEqual(state["map_points"][1], [1.0, 2.0, 3.0])

    def test_artifact_loader_reads_binary_little_endian_ply(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            ply_path = os.path.join(tmpdir, "sample_binary.ply")
            header = (
                "ply\n"
                "format binary_little_endian 1.0\n"
                "element vertex 2\n"
                "property float x\n"
                "property float y\n"
                "property float z\n"
                "property uchar red\n"
                "property uchar green\n"
                "property uchar blue\n"
                "end_header\n"
            ).encode("ascii")
            vertex_blob = (
                struct.pack("<fffBBB", 0.0, 0.0, 0.0, 255, 0, 0)
                + struct.pack("<fffBBB", 1.0, 2.0, 3.0, 0, 255, 0)
            )
            with open(ply_path, "wb") as fp:
                fp.write(header)
                fp.write(vertex_blob)

            artifact = load_reconstruction_artifact(ply_path)

        self.assertEqual(artifact.output_format, "ply")
        self.assertEqual(len(artifact.points), 2)
        self.assertEqual(artifact.points[1].tolist(), [1.0, 2.0, 3.0])
        self.assertEqual(artifact.colors[1].tolist(), [0, 255, 0])

    def test_session_http_client_happy_path(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            server = make_server("127.0.0.1", 0, "feature_sfm", tmpdir)
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            endpoint = f"http://127.0.0.1:{server.server_address[1]}"
            client = SessionHttpClient(endpoint=endpoint, request_timeout_s=5.0)
            try:
                started = client.start_session("seq-http", {"output_policy": "session_plus_export", "output_format": "ply"})
                self.assertEqual(started["status"], "active")

                appended = client.append_frames(started["session_id"], [
                    self._make_image("/server/a.png", "img-a"),
                    self._make_image("/server/b.png", "img-b"),
                ])
                self.assertEqual(appended["status"], "accepted")
                self.assertEqual(appended["frame_count"], 2)

                state = client.get_session_state(started["session_id"])
                self.assertEqual(state["frame_count"], 2)
                self.assertEqual(state["tracking_state"], "tracking")

                exported = client.export_session_artifact(started["session_id"], "ply")
                self.assertEqual(exported["status"], "exported")
                artifact_path = client.download_artifact(started["session_id"], tmpdir)
                self.assertTrue(os.path.exists(str(artifact_path)))
            finally:
                server.shutdown()
                server.server_close()
                thread.join(timeout=2)

    def test_mast3r_slam_session_backend_branch_is_selected(self) -> None:
        service = ReconstructionService(Dust3rBackend(), GlbExporter())
        started = service.start_session("seq-mast3r", {"backend_name": "mast3r_slam", "output_policy": "session_state_only"})

        self.assertIn(started.session_id, getattr(service, "_session_backends"))

        with patch("reconstruction.server.service.Mast3rSlamSessionBackend.refresh_session") as refresh_mock:
            service.append_frames(
                started.session_id,
                [self._make_image("/server/frame-a.png", "img-a")],
            )
            refresh_mock.assert_called_once()


if __name__ == "__main__":
    unittest.main()
