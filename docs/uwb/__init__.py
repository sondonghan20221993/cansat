"""
UWB module skeleton.

Provides the first implementation pass for the UWB distance handling and
position estimation flow described in 04-uwb-requirements.md.
"""

from .config import UwbConfig
from .models import AnchorDistance, AnchorPosition, DistanceSet, ErrorCode, PositionResult
from .processor import UwbProcessor

__all__ = [
    "AnchorDistance",
    "AnchorPosition",
    "DistanceSet",
    "ErrorCode",
    "PositionResult",
    "UwbConfig",
    "UwbProcessor",
]