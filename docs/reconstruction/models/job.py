"""
reconstruction/models/job.py

Job request / response models and job status definitions.

Corresponds to:
  REC-PROC-12  — job identity preserved between request and response
  REC-IFC-01   — reconstruction job request message structure (to be finalized
                 in 03-interface-specification.md)
  REC-IFC-02   — reconstruction result return message structure (to be finalized)
  REC-IFC-03   — status/error code enumeration
"""

from __future__ import annotations

import uuid
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import Enum
from typing import Any, Dict, List, Optional


# ---------------------------------------------------------------------------
# Job status enumeration  (REC-IFC-03 / REC-PROC-15)
# ---------------------------------------------------------------------------

class JobStatus(str, Enum):
    """
    Reconstruction job outcome states.

    SUCCESS   — reconstruction completed and result meets quality threshold.
    DEGRADED  — reconstruction completed but result is partial or low-confidence.
                Criteria: OI-REC-05 (TBD).
    FAILED    — reconstruction could not be completed.
    TIMEOUT   — remote server did not respond within the configured timeout.
                Timeout value: OI-REC-06 (TBD).
    PENDING   — job has been submitted but no result has been received yet.
    """

    SUCCESS  = "success"
    DEGRADED = "degraded"
    FAILED   = "failed"
    TIMEOUT  = "timeout"
    PENDING  = "pending"


def generate_job_id() -> str:
    """Return a unique job identifier (UUID4 string)."""
    return str(uuid.uuid4())


# ---------------------------------------------------------------------------
# Image descriptor  (REC-IN-01, REC-IN-02)
# ---------------------------------------------------------------------------

@dataclass
class ImageDescriptor:
    """
    Lightweight reference to a single input image.

    image_id    — unique identifier for this image (REC-IN-02)
    timestamp   — acquisition timestamp as ISO-8601 string or epoch float.
                  Exact timestamp convention: OI-IFC-06 / 03-interface-specification.md
    source_path — local path or URI; interpretation is transport-dependent (OI-REC-07)
    metadata    — arbitrary key-value pairs for future extension
                  (e.g. camera intrinsics when available — REC-IN-05)
    """

    image_id: str
    timestamp: Any                          # TODO(OI-IFC-06): fix timestamp type
    source_path: str
    metadata: Dict[str, Any] = field(default_factory=dict)


# ---------------------------------------------------------------------------
# Reconstruction job request  (REC-IFC-01)
# ---------------------------------------------------------------------------

@dataclass
class ReconstructionRequest:
    """
    Encapsulates everything the ground-side computer sends to the remote server.

    job_id          — unique identifier; used to match response to this request
                      (REC-PROC-12)
    images          — ordered list of image descriptors (REC-IN-01)
    output_format   — format token, e.g. "glb".  NOT hardcoded; comes from
                      ReconstructionConfig.output_format (REC-OUT-04, OI-REC-03)
    aux_pose        — optional camera pose / localization data (REC-IN-06).
                      None when not available; SHALL NOT block reconstruction
                      (REC-IN-07, REC-IN-08).
    submitted_at    — UTC timestamp of job submission
    extra           — forward-compatible extension dict for fields not yet
                      defined in the interface spec (REC-IFC-01 TBD fields)
    """

    images: List[ImageDescriptor]
    output_format: str
    job_id: str = field(default_factory=generate_job_id)
    aux_pose: Optional[Any] = None          # TODO(REC-IFC-01): define pose type
    submitted_at: datetime = field(
        default_factory=lambda: datetime.now(timezone.utc)
    )
    extra: Dict[str, Any] = field(default_factory=dict)


# ---------------------------------------------------------------------------
# Reconstruction job response  (REC-IFC-02)
# ---------------------------------------------------------------------------

@dataclass
class ReconstructionResponse:
    """
    Encapsulates everything the remote server returns to the ground-side computer.

    job_id          — MUST match the originating ReconstructionRequest.job_id
                      (REC-PROC-12)
    status          — outcome of the reconstruction job (REC-PROC-15)
    result_ref      — opaque reference to the reconstruction output artifact
                      (e.g. file path, URI, or in-memory handle).
                      Interpretation is transport- and format-dependent.
                      (REC-OUT-01, OI-REC-03, OI-REC-07)
    quality_meta    — quality metadata dict; exact fields TBD (OI-REC-04).
                      SHALL include at minimum: images_used, processing_status,
                      and at least one quality indicator (REC-OUT-06).
    error_code      — machine-readable error token when status != SUCCESS.
                      Enumeration to be finalized in interface spec (REC-IFC-03).
    processing_duration_s — wall-clock seconds for remote execution (REC-PERF-03)
    completed_at    — UTC timestamp when the server finished processing
    extra           — forward-compatible extension dict
    """

    job_id: str
    status: JobStatus
    result_ref: Optional[Any] = None        # TODO(OI-REC-03, OI-REC-07): define
    quality_meta: Dict[str, Any] = field(default_factory=dict)
    error_code: Optional[str] = None        # TODO(REC-IFC-03): define enum
    processing_duration_s: Optional[float] = None
    completed_at: Optional[datetime] = None
    extra: Dict[str, Any] = field(default_factory=dict)
