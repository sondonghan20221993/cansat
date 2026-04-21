"""
UWB configuration values.

Open items such as geometry thresholds and cycle timing remain configurable so
the implementation can be tightened once the system-level decisions are frozen.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Optional

from .models import AnchorPosition


@dataclass
class UwbConfig:
    anchor_positions: Dict[str, AnchorPosition]
    anchor_ids: Optional[List[str]] = None
    extra_wait_ms: float = 30.0
    residual_warning_threshold_cm: float = 20.0
    plane_tolerance_cm: float = 1e-6
    geometry_tolerance_cm: float = 1e-6
    numeric_tolerance_cm: float = 1e-9
    geometry_condition_threshold: Optional[float] = None
    output_cycle_name: str = "Output_Cycle_Timer"
    metadata: Dict[str, object] = field(default_factory=dict)

    def ordered_anchor_ids(self) -> List[str]:
        if self.anchor_ids:
            return list(self.anchor_ids)
        return sorted(self.anchor_positions.keys())