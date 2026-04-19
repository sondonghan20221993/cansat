"""
reconstruction/models/result.py

Result packaging for the reconstruction module.

Corresponds to:
  REC-OUT-01  — 3D reconstruction result in system-defined representation
  REC-OUT-02  — output includes job ID and processing timestamp
  REC-OUT-03  — output includes input image set identifier
  REC-OUT-05  — quality metadata included in every output
  REC-OUT-06  — quality metadata minimum fields
  REC-OUT-08  — failure result structure detectable by downstream modules
  REC-OUT-09  — degraded result marking
  REC-OUT-10  — error/status code on failure and degraded outputs
"""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional

from reconstruction.models.job import JobStatus


# ---------------------------------------------------------------------------
# Quality metadata  (REC-OUT-05, REC-OUT-06)
# ---------------------------------------------------------------------------

@dataclass
class QualityMetadata:
    """
    Minimum required quality metadata fields (REC-OUT-06).

    images_used         — number of input images actually used in reconstruction
    processing_status   — mirrors JobStatus for downstream consumers
    quality_indicators  — dict of named quality metrics.
                          Exact keys and value types: OI-REC-04 (TBD).
                          Example placeholder: {"confidence": 0.87}
    """

    images_used: int
    processing_status: JobStatus
    quality_indicators: Dict[str, Any] = field(default_factory=dict)
    # TODO(OI-REC-04): replace quality_indicators with typed fields once
    # quality metrics are finalized in the interface specification.


# ---------------------------------------------------------------------------
# Reconstruction result  (REC-OUT-01 through REC-OUT-10)
# ---------------------------------------------------------------------------

@dataclass
class ReconstructionResult:
    """
    Packaged output returned to downstream alignment / integration modules.

    job_id          — matches the originating request (REC-OUT-02, REC-PROC-12)
    image_set_id    — identifier of the input image set used (REC-OUT-03).
                      May be a list of image_ids or a single set token.
    status          — SUCCESS / DEGRADED / FAILED / TIMEOUT (REC-PROC-15)
    output_ref      — opaque reference to the 3D artifact (REC-OUT-01).
                      Format is determined by ReconstructionConfig.output_format;
                      GLB is the current primary candidate (OI-REC-03) but is
                      NOT hardcoded here.
    output_format   — format token used for this result (e.g. "glb").
                      Stored explicitly so downstream modules can branch on it
                      without inspecting output_ref (REC-OUT-04).
    quality         — quality metadata (REC-OUT-05, REC-OUT-06)
    error_code      — machine-readable error token; None on SUCCESS (REC-OUT-10)
    processing_timestamp — UTC time when the result was packaged (REC-OUT-02)
    extra           — forward-compatible extension dict
    """

    job_id: str
    image_set_id: Any                       # List[str] or opaque set token
    status: JobStatus
    quality: QualityMetadata
    output_ref: Optional[Any] = None        # TODO(OI-REC-03): define artifact type
    output_format: Optional[str] = None     # TODO(OI-REC-03): confirm frozen format
    error_code: Optional[str] = None        # TODO(REC-IFC-03): define enum
    processing_timestamp: datetime = field(
        default_factory=lambda: datetime.now(timezone.utc)
    )
    extra: Dict[str, Any] = field(default_factory=dict)

    # ------------------------------------------------------------------
    # Convenience predicates for downstream modules (REC-OUT-08, REC-OUT-09)
    # ------------------------------------------------------------------

    @property
    def is_valid(self) -> bool:
        """True only when reconstruction succeeded and result is usable."""
        return self.status == JobStatus.SUCCESS

    @property
    def is_degraded(self) -> bool:
        """True when reconstruction completed but result is partial/low-confidence."""
        return self.status == JobStatus.DEGRADED

    @property
    def is_failed(self) -> bool:
        """True when reconstruction could not be completed."""
        return self.status in (JobStatus.FAILED, JobStatus.TIMEOUT)

    # ------------------------------------------------------------------
    # Factory helpers
    # ------------------------------------------------------------------

    @classmethod
    def make_failed(
        cls,
        job_id: str,
        image_set_id: Any,
        images_used: int,
        error_code: Optional[str] = None,
        status: JobStatus = JobStatus.FAILED,
    ) -> "ReconstructionResult":
        """
        Construct a consistently structured failure result (REC-OUT-08).
        Downstream modules can detect failure via is_failed or status field.
        """
        return cls(
            job_id=job_id,
            image_set_id=image_set_id,
            status=status,
            quality=QualityMetadata(
                images_used=images_used,
                processing_status=status,
            ),
            error_code=error_code,
        )

    @classmethod
    def make_degraded(
        cls,
        job_id: str,
        image_set_id: Any,
        images_used: int,
        output_ref: Any,
        output_format: str,
        quality_indicators: Optional[Dict[str, Any]] = None,
    ) -> "ReconstructionResult":
        """
        Construct a degraded result (REC-OUT-09).
        Criteria for degraded vs. failed: OI-REC-05 (TBD).
        """
        return cls(
            job_id=job_id,
            image_set_id=image_set_id,
            status=JobStatus.DEGRADED,
            output_ref=output_ref,
            output_format=output_format,
            quality=QualityMetadata(
                images_used=images_used,
                processing_status=JobStatus.DEGRADED,
                quality_indicators=quality_indicators or {},
            ),
        )
