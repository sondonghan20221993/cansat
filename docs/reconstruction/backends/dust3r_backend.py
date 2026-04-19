"""
DUSt3R-family backend placeholder.

This class defines the real backend boundary for future integration while
keeping model-specific details isolated from orchestration, transport, and
export logic.
"""

from __future__ import annotations

from typing import Any, Dict, List, Optional

from reconstruction.backend.base import ReconstructionBackend
from reconstruction.models.job import ImageDescriptor


class Dust3rBackend(ReconstructionBackend):
    """Placeholder backend for future DUSt3R-family integration."""

    def __init__(self, model_name: str = "dust3r", model_path: Optional[str] = None) -> None:
        self._model_name = model_name
        self._model_path = model_path
        self._loaded = False

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
            "aux_pose": aux_pose,
        }

    def infer(self, preprocessed: Any) -> Any:
        raise NotImplementedError(
            "DUSt3R-family inference is not integrated yet. "
            "This backend placeholder preserves the model boundary contract."
        )

    def postprocess(
        self,
        raw_result: Any,
        output_format: str,
        job_id: str,
        image_set_id: Any,
    ) -> Dict[str, Any]:
        return {
            "normalized_scene": raw_result,
            "output_format": output_format,
            "job_id": job_id,
            "image_set_id": image_set_id,
            "quality_indicators": {},
        }

    @property
    def backend_name(self) -> str:
        return self._model_name

    @property
    def supports_aux_pose(self) -> bool:
        return True
