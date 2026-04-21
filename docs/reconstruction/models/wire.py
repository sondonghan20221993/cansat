from __future__ import annotations

from dataclasses import asdict
from datetime import datetime, timezone
from typing import Any

from reconstruction.models.job import ImageDescriptor, JobStatus, ReconstructionRequest, ReconstructionResponse


def _to_wire_value(value: Any) -> Any:
    if isinstance(value, datetime):
        return value.isoformat()
    if isinstance(value, JobStatus):
        return value.value
    if isinstance(value, list):
        return [_to_wire_value(item) for item in value]
    if isinstance(value, dict):
        return {key: _to_wire_value(item) for key, item in value.items()}
    return value


def request_to_dict(request: ReconstructionRequest) -> dict[str, Any]:
    return _to_wire_value(asdict(request))


def request_from_dict(payload: dict[str, Any]) -> ReconstructionRequest:
    images = [
        ImageDescriptor(
            image_id=item["image_id"],
            timestamp=item.get("timestamp"),
            source_path=item["source_path"],
            metadata=item.get("metadata", {}),
        )
        for item in payload.get("images", [])
    ]
    submitted_at = payload.get("submitted_at")
    if isinstance(submitted_at, str):
        try:
            submitted_at = datetime.fromisoformat(submitted_at)
        except ValueError:
            submitted_at = None
    if submitted_at is None:
        submitted_at = datetime.now(timezone.utc)
    return ReconstructionRequest(
        job_id=payload["job_id"],
        image_set_id=payload.get("image_set_id"),
        images=images,
        output_format=payload.get("output_format", "glb"),
        aux_pose=payload.get("aux_pose"),
        submitted_at=submitted_at,
        extra=payload.get("extra", {}),
    )


def response_to_dict(response: ReconstructionResponse) -> dict[str, Any]:
    return _to_wire_value(asdict(response))


def response_from_dict(payload: dict[str, Any]) -> ReconstructionResponse:
    completed_at = payload.get("completed_at")
    if isinstance(completed_at, str):
        try:
            completed_at = datetime.fromisoformat(completed_at)
        except ValueError:
            completed_at = None
    return ReconstructionResponse(
        job_id=payload["job_id"],
        status=JobStatus(payload["status"]),
        result_ref=payload.get("result_ref"),
        output_format=payload.get("output_format"),
        quality_meta=payload.get("quality_meta", {}),
        error_code=payload.get("error_code"),
        processing_duration_s=payload.get("processing_duration_s"),
        completed_at=completed_at,
        extra=payload.get("extra", {}),
    )
