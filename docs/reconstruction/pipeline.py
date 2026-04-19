"""
reconstruction/pipeline.py

Minimal synchronous prototype pipeline.

This keeps the current module boundaries but runs locally so we can quickly
produce a visible 3D artifact before remote executor details are finalized.
"""

from __future__ import annotations

from typing import List, Optional

from reconstruction.backend.base import ReconstructionBackend
from reconstruction.config import ReconstructionConfig
from reconstruction.models.job import ImageDescriptor, JobStatus
from reconstruction.models.result import QualityMetadata, ReconstructionResult
from reconstruction.validation.image_validator import ImageValidator


class PrototypeReconstructionPipeline:
    """Local prototype runner for the reconstruction skeleton."""

    def __init__(
        self,
        config: ReconstructionConfig,
        backend: ReconstructionBackend,
    ) -> None:
        self._config = config
        self._backend = backend
        self._validator = ImageValidator(config.min_image_count)

    def run(
        self,
        images: List[ImageDescriptor],
        image_set_id: str = "prototype-image-set",
        aux_pose: Optional[object] = None,
    ) -> ReconstructionResult:
        report = self._validator.validate(images)
        if not report.is_valid:
            return ReconstructionResult.make_failed(
                job_id="prototype-validation-failed",
                image_set_id=image_set_id,
                images_used=len(report.accepted),
                error_code="INSUFFICIENT_VALID_IMAGES",
            )

        self._backend.load()
        try:
            preprocessed = self._backend.preprocess(report.accepted, aux_pose=aux_pose)
            raw_result = self._backend.infer(preprocessed)
            packaged = self._backend.postprocess(
                raw_result=raw_result,
                output_format=self._config.output_format,
                job_id=f"{image_set_id}-job",
                image_set_id=image_set_id,
            )
        finally:
            self._backend.unload()

        return ReconstructionResult(
            job_id=f"{image_set_id}-job",
            image_set_id=image_set_id,
            status=JobStatus.SUCCESS,
            output_ref=packaged["output_ref"],
            output_format=packaged["output_format"],
            quality=QualityMetadata(
                images_used=len(report.accepted),
                processing_status=JobStatus.SUCCESS,
                quality_indicators=packaged.get("quality_indicators", {}),
            ),
            extra={"validation_stats": report.recorded_stats},
        )
