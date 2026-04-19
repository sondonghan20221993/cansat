from __future__ import annotations

from typing import List, Optional

from reconstruction.models.job import ImageDescriptor, ReconstructionRequest
from reconstruction.models.result import ReconstructionResult
from reconstruction.validation.image_validator import ImageValidator


class ReconstructionOrchestrator:
    """
    Ground-side flow controller.

    Responsibilities:
      - validate inputs
      - build request
      - submit request to server client
      - fetch result
      - return a stable downstream-facing result contract
    """

    def __init__(self, validator: ImageValidator, server_client: object) -> None:
        self._validator = validator
        self._server_client = server_client

    def run(
        self,
        images: List[ImageDescriptor],
        image_set_id: str,
        output_format: str,
        aux_pose: Optional[object] = None,
    ) -> ReconstructionResult:
        report = self._validator.validate(images)
        if not report.is_valid:
            return ReconstructionResult.make_failed(
                job_id="validation-failed",
                image_set_id=image_set_id,
                images_used=len(report.accepted),
                error_code="INSUFFICIENT_VALID_IMAGES",
            )

        request = ReconstructionRequest(
            image_set_id=image_set_id,
            images=report.accepted,
            output_format=output_format,
            aux_pose=aux_pose,
            extra={"validation_stats": report.recorded_stats},
        )
        job_id = self._server_client.submit(request)
        response = self._server_client.fetch_result(job_id)
        return ReconstructionResult.from_response(
            response=response,
            image_set_id=image_set_id,
            images_used=len(report.accepted),
        )
