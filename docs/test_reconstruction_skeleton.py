from __future__ import annotations

import os
import sys
import tempfile
import unittest

DOCS_DIR = os.path.dirname(os.path.abspath(__file__))
if DOCS_DIR not in sys.path:
    sys.path.insert(0, DOCS_DIR)

from reconstruction.backends.dust3r_backend import Dust3rBackend
from reconstruction.backends.feature_sfm_backend import FeatureSfmBackend
from reconstruction.client.server_client import ServerClient
from reconstruction.core.orchestrator import ReconstructionOrchestrator
from reconstruction.exporters.glb_exporter import GlbExporter
from reconstruction.models.job import ImageDescriptor, JobStatus, ReconstructionRequest
from reconstruction.server.service import ReconstructionService
from reconstruction.validation.image_validator import ImageValidator


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
        self.assertEqual(result.error_code, "BACKEND_NOT_IMPLEMENTED")
        self.assertEqual(result.output_format, None)

    def test_feature_backend_can_emit_success_and_glb_path(self) -> None:
        import cv2
        import numpy as np

        validator = ImageValidator(min_image_count=2)
        service = ReconstructionService(FeatureSfmBackend(), GlbExporter())
        client = ServerClient(service)
        orchestrator = ReconstructionOrchestrator(validator, client)

        with tempfile.TemporaryDirectory() as tmpdir:
            img1 = np.zeros((240, 320, 3), dtype=np.uint8)
            cv2.circle(img1, (80, 120), 20, (255, 255, 255), -1)
            cv2.circle(img1, (160, 80), 18, (255, 255, 255), -1)
            cv2.circle(img1, (220, 170), 16, (255, 255, 255), -1)
            cv2.rectangle(img1, (40, 30), (90, 60), (255, 255, 255), -1)
            cv2.rectangle(img1, (200, 30), (260, 65), (255, 255, 255), -1)
            matrix = np.float32([[1, 0, 8], [0, 1, 4]])
            img2 = cv2.warpAffine(img1, matrix, (320, 240))

            path1 = os.path.join(tmpdir, "a.png")
            path2 = os.path.join(tmpdir, "b.png")
            cv2.imwrite(path1, img1)
            cv2.imwrite(path2, img2)

            result = orchestrator.run(
                images=[self._make_image(path1, "a"), self._make_image(path2, "b")],
                image_set_id="set-d",
                output_format="glb",
            )

        self.assertEqual(result.status, JobStatus.SUCCESS)
        self.assertEqual(result.output_format, "glb")
        self.assertTrue(str(result.output_ref).endswith(".glb"))


if __name__ == "__main__":
    unittest.main()
