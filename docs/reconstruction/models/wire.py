from __future__ import annotations

from dataclasses import asdict
from datetime import datetime, timezone
from typing import Any

from reconstruction.models.job import (
    ImageDescriptor,
    JobStatus,
    ReconstructionRequest,
    ReconstructionResponse,
    ReconstructionSession,
    SessionOperationResponse,
    SessionTransformUpdate,
)


def _parse_cfs_time(value: Any) -> datetime:
    if isinstance(value, datetime):
        return value
    if isinstance(value, (int, float)):
        return datetime.fromtimestamp(value, tz=timezone.utc)
    if isinstance(value, str):
        try:
            parsed = datetime.fromisoformat(value)
            if parsed.tzinfo is None:
                return parsed.replace(tzinfo=timezone.utc)
            return parsed
        except ValueError:
            pass
    return datetime.now(timezone.utc)


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
            timestamp=_parse_cfs_time(item.get("timestamp")),
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
        image_set_id=str(payload["image_set_id"]),
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
        poll_url=payload.get("poll_url"),
        artifact_url=payload.get("artifact_url"),
        quality_meta=payload.get("quality_meta", {}),
        error_code=payload.get("error_code"),
        processing_duration_s=payload.get("processing_duration_s"),
        completed_at=completed_at,
        extra=payload.get("extra", {}),
    )


def image_descriptor_from_dict(payload: dict[str, Any]) -> ImageDescriptor:
    return ImageDescriptor(
        image_id=payload["image_id"],
        timestamp=_parse_cfs_time(payload.get("timestamp")),
        source_path=payload["source_path"],
        metadata=payload.get("metadata", {}),
    )


def session_response_to_dict(response: SessionOperationResponse) -> dict[str, Any]:
    return _to_wire_value(asdict(response))


def session_response_from_dict(payload: dict[str, Any]) -> SessionOperationResponse:
    last_updated = payload.get("last_updated")
    if isinstance(last_updated, str):
        try:
            last_updated = datetime.fromisoformat(last_updated)
        except ValueError:
            last_updated = None
    return SessionOperationResponse(
        session_id=payload["session_id"],
        status=payload["status"],
        frame_count=payload.get("frame_count"),
        keyframe_count=payload.get("keyframe_count"),
        rendered_point_count=payload.get("rendered_point_count"),
        pose_stream_ref=payload.get("pose_stream_ref"),
        map_state_ref=payload.get("map_state_ref"),
        current_frame_ref=payload.get("current_frame_ref"),
        alignment_status=payload.get("alignment_status"),
        world_transform=payload.get("world_transform"),
        tracking_state=payload.get("tracking_state"),
        last_updated=last_updated,
        artifact_ref=payload.get("artifact_ref"),
        output_format=payload.get("output_format"),
        error_code=payload.get("error_code"),
    )


def session_transform_update_from_dict(payload: dict[str, Any]) -> SessionTransformUpdate:
    return SessionTransformUpdate(
        alignment_status=str(payload["alignment_status"]),
        world_transform=payload.get("world_transform"),
        updated_by=payload.get("updated_by"),
    )
