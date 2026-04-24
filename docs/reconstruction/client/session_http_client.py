from __future__ import annotations

import json
import os
import urllib.request
from typing import Any, Optional

from reconstruction.models.job import ImageDescriptor, SessionTransformUpdate
from reconstruction.models.wire import session_response_from_dict


class SessionHttpClient:
    """HTTP client for the prototype session-oriented reconstruction API."""

    def __init__(self, endpoint: str, request_timeout_s: float = 900.0) -> None:
        self._endpoint = endpoint.rstrip("/")
        self._request_timeout_s = request_timeout_s

    def is_available(self) -> bool:
        try:
            payload = self._json_request("GET", "/health", None)
        except Exception:  # noqa: BLE001
            return False
        return payload.get("status") == "ok"

    def start_session(self, image_sequence_id: str | None, session_config: dict[str, Any] | None) -> dict[str, Any]:
        payload = self._json_request("POST", "/sessions", {
            "image_sequence_id": image_sequence_id,
            "session_config": session_config or {},
        })
        return self._response_dict(payload)

    def append_frames(self, session_id: str, ordered_frames: list[ImageDescriptor]) -> dict[str, Any]:
        payload = self._json_request("POST", f"/sessions/{session_id}/frames", {
            "ordered_frames": [
                {
                    "image_id": frame.image_id,
                    "timestamp": frame.timestamp,
                    "source_path": frame.source_path,
                    "metadata": frame.metadata,
                }
                for frame in ordered_frames
            ]
        })
        return self._response_dict(payload)

    def get_session_state(self, session_id: str) -> dict[str, Any]:
        payload = self._json_request("GET", f"/sessions/{session_id}/state", None)
        return self._response_dict(payload)

    def update_session_transform(self, session_id: str, update: SessionTransformUpdate) -> dict[str, Any]:
        payload = self._json_request("POST", f"/sessions/{session_id}/transform", {
            "alignment_status": update.alignment_status,
            "world_transform": update.world_transform,
            "updated_by": update.updated_by,
        })
        return self._response_dict(payload)

    def export_session_artifact(self, session_id: str, output_format: str) -> dict[str, Any]:
        payload = self._json_request("POST", f"/sessions/{session_id}/export", {
            "output_format": output_format,
        })
        return self._response_dict(payload)

    def end_session(self, session_id: str, mode: str) -> dict[str, Any]:
        payload = self._json_request("POST", f"/sessions/{session_id}/end", {
            "mode": mode,
        })
        return self._response_dict(payload)

    def download_artifact(self, session_id: str, destination_dir: str) -> Optional[str]:
        os.makedirs(destination_dir, exist_ok=True)
        request = urllib.request.Request(
            f"{self._endpoint}/sessions/{session_id}/artifact",
            headers={"Accept": "application/octet-stream"},
            method="GET",
        )
        with urllib.request.urlopen(request, timeout=self._request_timeout_s) as response:
            content_disposition = response.headers.get("Content-Disposition", "")
            filename = _filename_from_headers(content_disposition) or f"{session_id}.artifact"
            destination = os.path.abspath(os.path.join(destination_dir, filename))
            with open(destination, "wb") as fp:
                fp.write(response.read())
            return destination

    def _response_dict(self, payload: dict[str, Any]) -> dict[str, Any]:
        response = session_response_from_dict(payload)
        return {
            "session_id": response.session_id,
            "status": response.status,
            "frame_count": response.frame_count,
            "keyframe_count": response.keyframe_count,
            "rendered_point_count": response.rendered_point_count,
            "pose_stream_ref": response.pose_stream_ref,
            "map_state_ref": response.map_state_ref,
            "current_frame_ref": response.current_frame_ref,
            "alignment_status": response.alignment_status,
            "world_transform": response.world_transform,
            "tracking_state": response.tracking_state,
            "last_updated": response.last_updated.isoformat() if response.last_updated is not None else None,
            "artifact_ref": response.artifact_ref,
            "output_format": response.output_format,
            "error_code": response.error_code,
        }

    def _json_request(self, method: str, path: str, payload: Optional[dict[str, Any]]) -> dict[str, Any]:
        data = None
        headers = {"Accept": "application/json"}
        if payload is not None:
            data = json.dumps(payload).encode("utf-8")
            headers["Content-Type"] = "application/json"
        request = urllib.request.Request(
            f"{self._endpoint}{path}",
            data=data,
            headers=headers,
            method=method,
        )
        with urllib.request.urlopen(request, timeout=self._request_timeout_s) as response:
            return json.loads(response.read().decode("utf-8"))


def _filename_from_headers(content_disposition: str) -> str | None:
    if not content_disposition:
        return None
    for part in content_disposition.split(";"):
        part = part.strip()
        if part.startswith("filename="):
            return part.split("=", 1)[1].strip('"')
    return None
