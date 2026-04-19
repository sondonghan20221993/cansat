"""
reconstruction/backend/base.py

Abstract base class for reconstruction backends.

The system SHALL use a DUSt3R-family method as the primary reconstruction
pipeline (REC-PROC-04), but the selected model SHALL be replaceable without
changing the module boundary contract (REC-PROC-06).

All concrete backends (DUSt3R, MASt3R, or future variants) MUST subclass
ReconstructionBackend and implement the three abstract methods below.
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any, Dict, List, Optional

from reconstruction.models.job import ImageDescriptor


class ReconstructionBackend(ABC):
    """
    Module boundary contract for reconstruction backends.

    Implementations MUST NOT change this interface.  Swapping a backend
    means providing a new subclass and updating ReconstructionConfig.backend_name;
    no other module code should need to change (REC-PROC-06, REC-OUT-04).
    """

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    @abstractmethod
    def load(self) -> None:
        """
        Load model weights and initialise the backend.

        Called once before any inference.  Implementations should raise
        RuntimeError if the backend cannot be initialised.
        """

    @abstractmethod
    def unload(self) -> None:
        """
        Release model weights and free resources.

        Called when the backend is no longer needed.
        """

    # ------------------------------------------------------------------
    # Preprocessing  (REC-PROC-03)
    # ------------------------------------------------------------------

    @abstractmethod
    def preprocess(
        self,
        images: List[ImageDescriptor],
        aux_pose: Optional[Any] = None,
    ) -> Any:
        """
        Prepare inputs for inference.

        Parameters
        ----------
        images   : validated image descriptors (REC-IN-01)
        aux_pose : optional camera pose / localization data (REC-IN-06).
                   MUST be treated as auxiliary only; reconstruction SHALL
                   proceed without it (REC-PROC-07, REC-PROC-08).

        Returns an opaque preprocessed batch whose type is backend-specific.
        """

    # ------------------------------------------------------------------
    # Inference  (REC-PROC-04, REC-PROC-05)
    # ------------------------------------------------------------------

    @abstractmethod
    def infer(self, preprocessed: Any) -> Any:
        """
        Run reconstruction inference on the preprocessed batch.

        Returns an opaque raw result whose type is backend-specific.
        The caller (RemoteExecutor or a local runner) is responsible for
        passing this to postprocess().
        """

    # ------------------------------------------------------------------
    # Postprocessing / output packaging  (REC-PROC-14, REC-OUT-01)
    # ------------------------------------------------------------------

    @abstractmethod
    def postprocess(
        self,
        raw_result: Any,
        output_format: str,
        job_id: str,
        image_set_id: Any,
    ) -> Dict[str, Any]:
        """
        Convert raw inference output into a format-agnostic result dict.

        Parameters
        ----------
        raw_result    : output of infer()
        output_format : format token from ReconstructionConfig (e.g. "glb").
                        The backend MUST respect this token and SHALL NOT
                        hardcode a single format (REC-OUT-04, OI-REC-03).
        job_id        : originating job identifier (REC-OUT-02)
        image_set_id  : identifier of the input image set (REC-OUT-03)

        Returns a dict with at minimum:
          {
            "output_ref":         <artifact reference — type TBD, OI-REC-03>,
            "output_format":      <str, mirrors output_format argument>,
            "quality_indicators": <dict — keys TBD, OI-REC-04>,
          }
        """

    # ------------------------------------------------------------------
    # Metadata
    # ------------------------------------------------------------------

    @property
    @abstractmethod
    def backend_name(self) -> str:
        """Human-readable name of this backend (e.g. 'dust3r', 'mast3r')."""

    @property
    def supports_aux_pose(self) -> bool:
        """
        True if this backend can make use of optional auxiliary pose input.
        Defaults to False; override in backends that support it (REC-PROC-07).
        """
        return False
