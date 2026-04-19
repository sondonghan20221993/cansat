"""
reconstruction/config.py

Configuration for the reconstruction module.

Fields marked TBD correspond to Open Items in 05-reconstruction-requirements.md
and SHALL be finalized before system verification.
"""

from dataclasses import dataclass, field
from typing import Optional


@dataclass
class ReconstructionConfig:
    # -------------------------------------------------------------------------
    # OI-REC-01: Minimum image count — TBD, placeholder value used
    # -------------------------------------------------------------------------
    min_image_count: int = 2  # TODO(OI-REC-01): finalize minimum image count

    # -------------------------------------------------------------------------
    # OI-REC-03: Primary external output format — GLB is the current candidate
    # but SHALL NOT be hardcoded as the only option.
    # The format identifier is passed through the pipeline as a string token
    # so it can be changed without redesigning the module.
    # -------------------------------------------------------------------------
    output_format: str = "glb"  # TODO(OI-REC-03): confirm frozen output format

    # -------------------------------------------------------------------------
    # OI-REC-04: Quality indicators and thresholds — TBD
    # -------------------------------------------------------------------------
    quality_threshold: Optional[float] = None  # TODO(OI-REC-04): set threshold

    # -------------------------------------------------------------------------
    # OI-REC-05: Degraded vs. failed criteria — TBD
    # -------------------------------------------------------------------------
    degraded_threshold: Optional[float] = None  # TODO(OI-REC-05): set criteria

    # -------------------------------------------------------------------------
    # OI-REC-06: Runtime / throughput targets — TBD
    # -------------------------------------------------------------------------
    job_timeout_seconds: Optional[float] = None  # TODO(OI-REC-06): set timeout

    # -------------------------------------------------------------------------
    # OI-REC-07: Remote execution protocol — TBD
    # The executor_endpoint is intentionally untyped here; the RemoteExecutor
    # implementation will interpret it according to the chosen transport.
    # -------------------------------------------------------------------------
    executor_endpoint: Optional[str] = None  # TODO(OI-REC-07): set endpoint

    # -------------------------------------------------------------------------
    # Backend selection — allows swapping DUSt3R-family model (REC-PROC-06)
    # -------------------------------------------------------------------------
    backend_name: str = "dust3r"  # e.g. "dust3r", "mast3r", or future variants
    server_mode: str = "remote-fixed"

    # -------------------------------------------------------------------------
    # Extra backend-specific kwargs forwarded to the backend at construction.
    # Keeps the config extensible without changing the module boundary.
    # -------------------------------------------------------------------------
    backend_kwargs: dict = field(default_factory=dict)
