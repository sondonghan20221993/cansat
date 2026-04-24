from __future__ import annotations

import os
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from reconstruction.models.job import ImageDescriptor, ReconstructionSession


@dataclass
class Mast3rSlamSessionBackend:
    """
    Prototype adapter that maps a MASt3R-SLAM run into the common session-state contract.

    The current implementation is intentionally conservative:
    - it rewrites the current ordered frame set into a session-specific input folder
    - it runs MASt3R-SLAM on the full current session snapshot
    - it translates `.txt` trajectory output and `.ply` map output into
      `pose_stream_ref` and `map_state_ref`

    This preserves the external session API while keeping backend-specific
    file layout and invocation details isolated.
    """

    def __init__(
        self,
        repo_root: str | None = None,
        python_bin: str | None = None,
        config_path: str | None = None,
        artifact_root: str = "artifacts/reconstruction/session_runs",
    ) -> None:
        self._repo_root = os.path.abspath(repo_root or os.environ.get("MAST3R_SLAM_REPO", os.path.expanduser("~/Desktop/MASt3R-SLAM")))
        self._python_bin = os.path.abspath(python_bin or os.environ.get("MAST3R_SLAM_PYTHON", os.path.expanduser("~/miniforge3/envs/mast3r-slam/bin/python")))
        self._config_path = config_path or os.environ.get("MAST3R_SLAM_CONFIG", "config/base.yaml")
        self._artifact_root = os.path.abspath(artifact_root)

    @property
    def backend_name(self) -> str:
        return "mast3r_slam"

    def refresh_session(self, session: ReconstructionSession) -> None:
        if not os.path.isdir(self._repo_root):
            raise RuntimeError(f"MAST3R_SLAM_REPO does not exist: {self._repo_root}")
        if not os.path.exists(self._python_bin):
            raise RuntimeError(f"MAST3R_SLAM_PYTHON does not exist: {self._python_bin}")
        if not session.ordered_frames:
            session.pose_stream_ref = {"poses": []}
            session.map_state_ref = {"type": "session_map", "session_id": session.session_id, "frame_count": 0, "point_count": 0}
            session.current_frame_ref = None
            session.keyframe_count = 0
            session.rendered_point_count = 0
            session.tracking_state = "initializing"
            return

        session_dir = os.path.join(self._artifact_root, session.session_id)
        input_dir = os.path.join(session_dir, "input_frames")
        output_dir = os.path.join(session_dir, "outputs")
        os.makedirs(input_dir, exist_ok=True)
        os.makedirs(output_dir, exist_ok=True)
        self._rewrite_session_input(input_dir, session.ordered_frames)

        dataset_name = os.path.basename(input_dir.rstrip(os.sep))
        logs_dir = os.path.join(self._repo_root, "logs")
        os.makedirs(logs_dir, exist_ok=True)
        self._clear_previous_logs(logs_dir, dataset_name)

        cmd = [
            self._python_bin,
            "main.py",
            "--dataset",
            input_dir,
            "--config",
            self._config_path,
            "--no-viz",
        ]
        completed = subprocess.run(
            cmd,
            cwd=self._repo_root,
            text=True,
            capture_output=True,
            check=False,
        )
        if completed.returncode != 0:
            session.tracking_state = "failed"
            session.error_code = "SESSION_BACKEND_FAILED"
            session.map_state_ref = {
                "type": "session_map",
                "session_id": session.session_id,
                "frame_count": session.frame_count,
                "point_count": 0,
                "backend": self.backend_name,
                "stderr_tail": completed.stderr[-2000:],
            }
            session.pose_stream_ref = {"poses": []}
            return

        pose_file = os.path.join(logs_dir, f"{dataset_name}.txt")
        ply_file = os.path.join(logs_dir, f"{dataset_name}.ply")
        copied_pose = None
        copied_ply = None
        if os.path.exists(pose_file):
            copied_pose = os.path.join(output_dir, f"{session.session_id}.txt")
            shutil.copy2(pose_file, copied_pose)
        if os.path.exists(ply_file):
            copied_ply = os.path.join(output_dir, f"{session.session_id}.ply")
            shutil.copy2(ply_file, copied_ply)

        poses = self._parse_pose_file(copied_pose, session.ordered_frames) if copied_pose else []
        point_count = self._read_ply_vertex_count(copied_ply) if copied_ply else 0

        session.pose_stream_ref = {
            "backend": self.backend_name,
            "path": copied_pose,
            "poses": poses,
        }
        session.map_state_ref = {
            "type": "session_map",
            "backend": self.backend_name,
            "session_id": session.session_id,
            "path": copied_ply,
            "frame_count": session.frame_count,
            "point_count": point_count,
            "stdout_tail": completed.stdout[-2000:],
        }
        session.current_frame_ref = session.ordered_frames[-1].source_path
        session.keyframe_count = sum(1 for pose in poses if pose.get("is_keyframe"))
        session.rendered_point_count = point_count
        session.tracking_state = "tracking" if poses else "initializing"
        session.error_code = None

    def export_artifact(self, session: ReconstructionSession, output_format: str) -> str | None:
        if output_format != "ply":
            raise RuntimeError(f"MASt3R-SLAM prototype export currently supports only ply, got: {output_format}")
        if not isinstance(session.map_state_ref, dict):
            return None
        source = session.map_state_ref.get("path")
        if not source or not os.path.exists(source):
            return None
        export_dir = os.path.join(self._artifact_root, session.session_id, "exports")
        os.makedirs(export_dir, exist_ok=True)
        destination = os.path.join(export_dir, f"{session.session_id}.{output_format}")
        shutil.copy2(source, destination)
        return destination

    def _rewrite_session_input(self, input_dir: str, ordered_frames: list[ImageDescriptor]) -> None:
        for child in Path(input_dir).glob("*"):
            if child.is_file():
                child.unlink()
        for idx, frame in enumerate(ordered_frames):
            suffix = Path(frame.source_path).suffix or ".png"
            destination = os.path.join(input_dir, f"{idx:06d}{suffix}")
            shutil.copy2(frame.source_path, destination)

    def _clear_previous_logs(self, logs_dir: str, dataset_name: str) -> None:
        for ext in ("txt", "ply"):
            path = os.path.join(logs_dir, f"{dataset_name}.{ext}")
            if os.path.exists(path):
                os.remove(path)

    def _parse_pose_file(self, path: str, ordered_frames: list[ImageDescriptor]) -> list[dict[str, Any]]:
        poses: list[dict[str, Any]] = []
        if not path or not os.path.exists(path):
            return poses
        with open(path, "r", encoding="utf-8") as fp:
            for idx, line in enumerate(fp):
                parts = line.strip().split()
                if len(parts) != 8:
                    continue
                t, x, y, z, qx, qy, qz, qw = [float(value) for value in parts]
                source_path = ordered_frames[idx].source_path if idx < len(ordered_frames) else None
                image_id = ordered_frames[idx].image_id if idx < len(ordered_frames) else f"frame-{idx:06d}"
                poses.append({
                    "time_s": t,
                    "image_id": image_id,
                    "index": idx,
                    "source_path": source_path,
                    "position": [x, y, z],
                    "orientation": [qx, qy, qz, qw],
                    "is_keyframe": idx == 0 or idx % 5 == 0,
                })
        return poses

    def _read_ply_vertex_count(self, path: str | None) -> int:
        if not path or not os.path.exists(path):
            return 0
        with open(path, "rb") as fp:
            while True:
                line = fp.readline()
                if not line:
                    break
                try:
                    decoded = line.decode("utf-8", errors="ignore").strip()
                except Exception:  # noqa: BLE001
                    decoded = ""
                if decoded.startswith("element vertex "):
                    try:
                        return int(decoded.split()[-1])
                    except ValueError:
                        return 0
                if decoded == "end_header":
                    break
        return 0
