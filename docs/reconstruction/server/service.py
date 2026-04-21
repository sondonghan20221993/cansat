from __future__ import annotations

from typing import Dict

from reconstruction.backends.base import ReconstructionBackend
from reconstruction.exporters.base import ReconstructionExporter
from reconstruction.models.job import JobStatus, ReconstructionRequest, ReconstructionResponse


class ReconstructionService:
    """
    Server-side reconstruction entrypoint.

    Responsibilities:
      - receive job requests
      - execute backend lifecycle
      - call exporter
      - store and return stable response objects
    """

    def __init__(self, backend: ReconstructionBackend, exporter: ReconstructionExporter) -> None:
        self._backend = backend
        self._exporter = exporter
        self._responses: Dict[str, ReconstructionResponse] = {}

    def submit(self, request: ReconstructionRequest) -> str:
        try:
            self._backend.load()
            if hasattr(self._backend, "_current_job_id"):
                setattr(self._backend, "_current_job_id", request.job_id)
            preprocessed = self._backend.preprocess(request.images, aux_pose=request.aux_pose)
            raw_result = self._backend.infer(preprocessed)
            packaged = self._backend.postprocess(
                raw_result=raw_result,
                output_format=request.output_format,
                job_id=request.job_id,
                image_set_id=request.image_set_id,
            )
            if packaged.get("artifact_ref"):
                exported = {
                    "output_ref": packaged["artifact_ref"],
                    "output_format": packaged.get("output_format") or request.output_format,
                }
            else:
                exported = self._exporter.export(
                    normalized_scene=packaged["normalized_scene"],
                    artifact_name=request.job_id,
                )
            response = ReconstructionResponse(
                job_id=request.job_id,
                status=JobStatus.SUCCESS,
                result_ref=exported["output_ref"],
                output_format=exported["output_format"],
                quality_meta=packaged.get("quality_indicators", {}),
            )
        except NotImplementedError as exc:
            response = ReconstructionResponse(
                job_id=request.job_id,
                status=JobStatus.FAILED,
                error_code="BACKEND_NOT_IMPLEMENTED",
                quality_meta={},
                extra={"reason": str(exc)},
            )
        except Exception as exc:  # noqa: BLE001
            response = ReconstructionResponse(
                job_id=request.job_id,
                status=JobStatus.FAILED,
                error_code="SERVER_EXECUTION_FAILED",
                quality_meta={},
                extra={"reason": str(exc)},
            )
        finally:
            self._backend.unload()

        self._responses[request.job_id] = response
        return request.job_id

    def fetch_result(self, job_id: str) -> ReconstructionResponse:
        return self._responses.get(
            job_id,
            ReconstructionResponse(
                job_id=job_id,
                status=JobStatus.PENDING,
                quality_meta={},
            ),
        )
