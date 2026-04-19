from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any, Dict


class ReconstructionExporter(ABC):
    """Output-format boundary for reconstruction artifacts."""

    @abstractmethod
    def export(self, normalized_scene: Any, artifact_name: str) -> Dict[str, Any]:
        """
        Export a backend-normalized reconstruction scene.

        Returns a dict with at least:
          - output_ref
          - output_format
        """

    @property
    @abstractmethod
    def format_name(self) -> str:
        """Human-readable output format token, e.g. 'glb'."""
