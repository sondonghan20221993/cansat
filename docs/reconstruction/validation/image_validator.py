"""
reconstruction/validation/image_validator.py

Image input validation skeleton.

Corresponds to:
  REC-IN-01   — accept a set of input images
  REC-IN-02   — each image carries unique ID and acquisition timestamp
  REC-IN-03   — reject corrupted or undecodable images before job submission
  REC-IN-04   — require at least the system-defined minimum image count
  REC-ERR-01  — stop and reject when minimum image count is not satisfied
  REC-ERR-02  — report corrupted/unusable images in logs or status metadata
  REC-PROC-01 — validate image completeness and metadata consistency
  REC-PROC-02 — record image count, resolution, metadata availability at job start
"""

from __future__ import annotations

import logging
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

from reconstruction.models.job import ImageDescriptor

logger = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Validation result
# ---------------------------------------------------------------------------

@dataclass
class ImageValidationReport:
    """
    Summary produced by ImageValidator.validate().

    accepted        — images that passed all checks
    rejected        — (descriptor, reason) pairs for images that failed
    is_valid        — True only when accepted count >= min_image_count
                      and no unrecoverable errors occurred
    recorded_stats  — metadata recorded at validation time (REC-PROC-02)
    """

    accepted: List[ImageDescriptor] = field(default_factory=list)
    rejected: List[Tuple[ImageDescriptor, str]] = field(default_factory=list)
    is_valid: bool = False
    recorded_stats: dict = field(default_factory=dict)


# ---------------------------------------------------------------------------
# Validator
# ---------------------------------------------------------------------------

class ImageValidator:
    """
    Validates a batch of ImageDescriptors before a reconstruction job is submitted.

    Parameters
    ----------
    min_image_count : int
        Minimum number of valid images required to proceed.
        Source: ReconstructionConfig.min_image_count (OI-REC-01 TBD).
    """

    def __init__(self, min_image_count: int) -> None:
        self._min_image_count = min_image_count

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def validate(self, images: List[ImageDescriptor]) -> ImageValidationReport:
        """
        Run all validation checks on the supplied image list.

        Steps:
          1. Check each image for required fields (REC-IN-02).
          2. Attempt to decode / probe each image (REC-IN-03).
          3. Enforce minimum image count (REC-IN-04, REC-ERR-01).
          4. Record stats (REC-PROC-02).

        Returns an ImageValidationReport.  Callers MUST check report.is_valid
        before submitting a reconstruction job.
        """
        report = ImageValidationReport()

        for img in images:
            ok, reason = self._check_image(img)
            if ok:
                report.accepted.append(img)
            else:
                report.rejected.append((img, reason))
                logger.warning(
                    "Image rejected [id=%s]: %s", img.image_id, reason
                )  # REC-ERR-02

        # Minimum count check (REC-IN-04, REC-ERR-01)
        if len(report.accepted) < self._min_image_count:
            logger.error(
                "Insufficient valid images: %d accepted, %d required.",
                len(report.accepted),
                self._min_image_count,
            )
            report.is_valid = False
        else:
            report.is_valid = True

        # Record stats at validation time (REC-PROC-02)
        report.recorded_stats = self._record_stats(report.accepted, images)

        return report

    # ------------------------------------------------------------------
    # Internal checks
    # ------------------------------------------------------------------

    def _check_image(
        self, img: ImageDescriptor
    ) -> Tuple[bool, Optional[str]]:
        """
        Run per-image checks.  Returns (passed, failure_reason).

        Checks implemented here are structural / metadata checks.
        Pixel-level decode check is delegated to _probe_decodable().
        """
        # Required field: unique identifier (REC-IN-02)
        if not img.image_id or not str(img.image_id).strip():
            return False, "missing image_id"

        # Required field: acquisition timestamp (REC-IN-02)
        if img.timestamp is None:
            return False, "missing acquisition timestamp"

        # Required field: source reference
        if not img.source_path or not str(img.source_path).strip():
            return False, "missing source_path"

        # Decodability check (REC-IN-03)
        decodable, decode_reason = self._probe_decodable(img)
        if not decodable:
            return False, decode_reason

        return True, None

    def _probe_decodable(
        self, img: ImageDescriptor
    ) -> Tuple[bool, Optional[str]]:
        """
        Attempt to verify that the image at source_path is decodable.

        This is a skeleton implementation.  The actual decode strategy
        (e.g. PIL.Image.verify, cv2.imdecode, or a remote probe) SHALL be
        filled in during implementation.

        Returns (decodable, failure_reason).
        """
        # TODO: implement actual image decode probe
        # Example:
        #   from PIL import Image
        #   try:
        #       with Image.open(img.source_path) as im:
        #           im.verify()
        #       return True, None
        #   except Exception as exc:
        #       return False, f"decode error: {exc}"
        return True, None  # placeholder — always passes until implemented

    def _record_stats(
        self,
        accepted: List[ImageDescriptor],
        all_images: List[ImageDescriptor],
    ) -> dict:
        """
        Record job-start statistics (REC-PROC-02).

        Fields recorded:
          - total_submitted: total images submitted
          - accepted_count:  images that passed validation
          - rejected_count:  images that failed validation
          - metadata_available: fraction of accepted images with non-empty metadata
          - resolution_info: placeholder (requires actual image probe — TBD)
        """
        metadata_available = sum(
            1 for img in accepted if img.metadata
        )

        return {
            "total_submitted": len(all_images),
            "accepted_count": len(accepted),
            "rejected_count": len(all_images) - len(accepted),
            "metadata_available_count": metadata_available,
            # TODO(REC-PROC-02): add per-image resolution once decode probe
            # is implemented.
            "resolution_info": "TBD",
        }
