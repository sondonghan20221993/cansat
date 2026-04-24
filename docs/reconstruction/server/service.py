from __future__ import annotations

import os
from typing import Dict
from datetime import datetime, timezone

from reconstruction.backends.base import ReconstructionBackend
from reconstruction.backends.mast3r_slam_session_backend import Mast3rSlamSessionBackend
from reconstruction.exporters.base import ReconstructionExporter
from reconstruction.models.job import (
    ImageDescriptor,
    JobStatus,
    ReconstructionRequest,
    ReconstructionResponse,
    ReconstructionSession,
    SessionOperationResponse,
    SessionTransformUpdate,
    generate_job_id,
)


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
        self._sessions: Dict[str, ReconstructionSession] = {}
        self._session_backends: Dict[str, Mast3rSlamSessionBackend] = {}

    @staticmethod
    def _utc_now() -> datetime:
        return datetime.now(timezone.utc)

    def _session_response(self, session: ReconstructionSession) -> SessionOperationResponse:
        return SessionOperationResponse(
            session_id=session.session_id,
            status=session.status,
            frame_count=session.frame_count,
            keyframe_count=session.keyframe_count,
            rendered_point_count=session.rendered_point_count,
            pose_stream_ref=session.pose_stream_ref,
            map_state_ref=session.map_state_ref,
            current_frame_ref=session.current_frame_ref,
            alignment_status=session.alignment_status,
            world_transform=session.world_transform,
            tracking_state=session.tracking_state,
            last_updated=session.last_updated,
            artifact_ref=session.exported_artifact_ref,
            output_format=session.exported_output_format,
            error_code=session.error_code,
        )

    def _session_response_with_status(self, session: ReconstructionSession, status: str) -> SessionOperationResponse:
        response = self._session_response(session)
        response.status = status
        return response

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

    def start_session(self, image_sequence_id: str | None, session_config: dict | None) -> SessionOperationResponse:
        session_id = generate_job_id()
        now = self._utc_now()
        session_config = session_config or {}
        session = ReconstructionSession(
            session_id=session_id,
            image_sequence_id=image_sequence_id,
            status="active",
            session_config=session_config,
            pose_stream_ref={"poses": []},
            map_state_ref={"type": "session_map", "session_id": session_id, "frame_count": 0, "point_count": 0},
            tracking_state="initializing",
            last_updated=now,
        )
        self._sessions[session_id] = session
        if str(session_config.get("backend_name", "")).lower() == "mast3r_slam":
            self._session_backends[session_id] = Mast3rSlamSessionBackend()
        return self._session_response(session)

    def append_frames(self, session_id: str, ordered_frames: list[ImageDescriptor]) -> SessionOperationResponse:
        session = self._sessions.get(session_id)
        if session is None:
            return SessionOperationResponse(session_id=session_id, status="session_not_found", error_code="SESSION_NOT_FOUND")
        if session.status != "active":
            return SessionOperationResponse(session_id=session_id, status="session_closed", error_code="SESSION_NOT_ACTIVE")
        session.ordered_frames.extend(ordered_frames)
        session.frame_count = len(session.ordered_frames)
        session.keyframe_count = max(1, (session.frame_count + 4) // 5) if session.frame_count else 0
        session.rendered_point_count = session.keyframe_count * 1500
        session.current_frame_ref = session.ordered_frames[-1].source_path if session.ordered_frames else None
        session.tracking_state = "tracking" if session.frame_count > 1 else "initializing"
        session.last_updated = self._utc_now()
        session.pose_stream_ref = {
            "poses": [
                {"image_id": frame.image_id, "source_path": frame.source_path, "index": idx}
                for idx, frame in enumerate(session.ordered_frames)
            ]
        }
        session.map_state_ref = {
            "type": "session_map",
            "session_id": session_id,
            "frame_count": session.frame_count,
            "keyframe_count": session.keyframe_count,
            "point_count": session.rendered_point_count,
        }
        session_backend = self._session_backends.get(session_id)
        if session_backend is not None:
            session_backend.refresh_session(session)
        return self._session_response_with_status(session, "accepted")

    def get_session_state(self, session_id: str) -> SessionOperationResponse:
        session = self._sessions.get(session_id)
        if session is None:
            return SessionOperationResponse(session_id=session_id, status="not_found", error_code="SESSION_NOT_FOUND")
        return self._session_response(session)

    def update_session_transform(self, session_id: str, update: SessionTransformUpdate) -> SessionOperationResponse:
        session = self._sessions.get(session_id)
        if session is None:
            return SessionOperationResponse(session_id=session_id, status="not_found", error_code="SESSION_NOT_FOUND")
        session.alignment_status = update.alignment_status
        session.world_transform = update.world_transform
        session.last_updated = self._utc_now()
        return self._session_response_with_status(session, "updated")

    def export_session_artifact(self, session_id: str, output_format: str) -> SessionOperationResponse:
        session = self._sessions.get(session_id)
        if session is None:
            return SessionOperationResponse(session_id=session_id, status="not_found", error_code="SESSION_NOT_FOUND")
        artifact_ref = None
        if session.frame_count > 0:
            session_backend = self._session_backends.get(session_id)
            if session_backend is not None:
                artifact_ref = session_backend.export_artifact(session, output_format)
            else:
                artifact_name = f"{session_id}.{output_format}"
                artifact_dir = getattr(self._exporter, "_artifact_root", "artifacts/reconstruction")
                os.makedirs(os.path.abspath(artifact_dir), exist_ok=True)
                artifact_ref = os.path.abspath(os.path.join(artifact_dir, artifact_name))
                with open(artifact_ref, "w", encoding="utf-8") as fp:
                    fp.write("session export placeholder\n")
                    fp.write(f"session_id={session_id}\n")
                    fp.write(f"frame_count={session.frame_count}\n")
            session.exported_artifact_ref = artifact_ref
            session.exported_output_format = output_format
            session.status = "exported"
            session.last_updated = self._utc_now()
            return self._session_response(session)
        return SessionOperationResponse(
            session_id=session_id,
            status="failed",
            error_code="SESSION_EMPTY",
        )

    def end_session(self, session_id: str, mode: str) -> SessionOperationResponse:
        session = self._sessions.get(session_id)
        if session is None:
            return SessionOperationResponse(session_id=session_id, status="not_found", error_code="SESSION_NOT_FOUND")
        if mode == "finalize":
            session.status = "completed"
            session.tracking_state = "completed"
            session.last_updated = self._utc_now()
            if session.session_config.get("output_policy") == "session_plus_export":
                exported = self.export_session_artifact(session_id, session.session_config.get("output_format", "ply"))
                if exported.status == "exported":
                    return self._session_response(session)
            return self._session_response(session)
        session.status = "failed"
        session.tracking_state = "discarded"
        session.error_code = "SESSION_DISCARDED"
        session.last_updated = self._utc_now()
        self._session_backends.pop(session_id, None)
        return self._session_response(session)
