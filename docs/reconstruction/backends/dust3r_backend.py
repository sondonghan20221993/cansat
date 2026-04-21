"""
DUSt3R-family backend placeholder.

This class defines the real backend boundary for future integration while
keeping model-specific details isolated from orchestration, transport, and
export logic.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional

from reconstruction.backend.base import ReconstructionBackend
from reconstruction.models.job import ImageDescriptor


class Dust3rBackend(ReconstructionBackend):
    """Placeholder backend for future DUSt3R-family integration."""

    def __init__(self, model_name: str = "dust3r", model_path: Optional[str] = None) -> None:
        self._model_name = model_name
        self._model_path = model_path
        self._loaded = False
        self._current_job_id = "dust3r-output"

    def load(self) -> None:
        self._loaded = True

    def unload(self) -> None:
        self._loaded = False

    def preprocess(
        self,
        images: List[ImageDescriptor],
        aux_pose: Optional[Any] = None,
    ) -> Any:
        if not self._loaded:
            raise RuntimeError("Dust3rBackend must be loaded before preprocess().")
        return {
            "image_paths": [img.source_path for img in images],
            "image_ids": [img.image_id for img in images],
            "timestamps": [img.timestamp for img in images],
            "job_id": getattr(self, "_current_job_id", "dust3r-output"),
            "aux_pose": aux_pose,
        }

    def infer(self, preprocessed: Any) -> Any:
        dust3r_repo = os.environ.get("DUST3R_REPO", os.path.expanduser("~/Desktop/dust3r"))
        dust3r_python = os.environ.get("DUST3R_PYTHON", os.path.expanduser("~/miniforge3/envs/dust3r-conda/bin/python"))
        artifact_root = os.environ.get("RECONSTRUCTION_ARTIFACT_ROOT", "artifacts/reconstruction")
        image_size = os.environ.get("DUST3R_IMAGE_SIZE", "512")
        niter = os.environ.get("DUST3R_NITER", "300")

        if not os.path.isdir(dust3r_repo):
            raise RuntimeError(f"DUST3R_REPO does not exist: {dust3r_repo}")
        if not os.path.exists(dust3r_python):
            raise RuntimeError(f"DUST3R_PYTHON does not exist: {dust3r_python}")

        job_id = preprocessed.get("job_id", "dust3r-output")
        output_path = os.path.abspath(os.path.join(artifact_root, f"{job_id}.ply"))
        runner = Path(__file__).resolve().parents[1] / "tools" / "dust3r_export_ply.py"
        cmd = [
            dust3r_python,
            str(runner),
            "--dust3r-repo",
            os.path.abspath(dust3r_repo),
            "--output",
            output_path,
            "--image-size",
            image_size,
            "--niter",
            niter,
            *preprocessed["image_paths"],
        ]
        completed = subprocess.run(
            cmd,
            cwd=os.path.abspath(dust3r_repo),
            text=True,
            capture_output=True,
            check=False,
        )
        if completed.returncode != 0:
            raise RuntimeError(
                "DUSt3R subprocess failed.\n"
                f"stdout:\n{completed.stdout}\n"
                f"stderr:\n{completed.stderr}"
            )
        return {
            "artifact_ref": output_path,
            "output_format": "ply",
            "stdout": completed.stdout,
            "images_used": len(preprocessed["image_paths"]),
        }

    def postprocess(
        self,
        raw_result: Any,
        output_format: str,
        job_id: str,
        image_set_id: Any,
    ) -> Dict[str, Any]:
        return {
            "normalized_scene": {},
            "artifact_ref": raw_result["artifact_ref"],
            "output_format": raw_result.get("output_format", output_format),
            "job_id": job_id,
            "image_set_id": image_set_id,
            "quality_indicators": {
                "backend": self.backend_name,
                "images_used": raw_result.get("images_used"),
                "artifact_format": raw_result.get("output_format"),
                "runner_stdout_tail": raw_result.get("stdout", "")[-2000:],
            },
        }

    @property
    def backend_name(self) -> str:
        return self._model_name

    @property
    def supports_aux_pose(self) -> bool:
        return True
