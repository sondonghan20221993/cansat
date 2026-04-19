from __future__ import annotations

import logging
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

import cv2

from reconstruction.models.job import ImageDescriptor

logger = logging.getLogger(__name__)


@dataclass
class ImageValidationReport:
    accepted: List[ImageDescriptor] = field(default_factory=list)
    rejected: List[Tuple[ImageDescriptor, str]] = field(default_factory=list)
    is_valid: bool = False
    recorded_stats: dict = field(default_factory=dict)


class ImageValidator:
    def __init__(self, min_image_count: int) -> None:
        self._min_image_count = min_image_count

    def validate(self, images: List[ImageDescriptor]) -> ImageValidationReport:
        report = ImageValidationReport()

        for img in images:
            ok, reason = self._check_image(img)
            if ok:
                report.accepted.append(img)
            else:
                report.rejected.append((img, reason))
                logger.warning("Image rejected [id=%s]: %s", img.image_id, reason)

        if len(report.accepted) < self._min_image_count:
            logger.error(
                "Insufficient valid images: %d accepted, %d required.",
                len(report.accepted),
                self._min_image_count,
            )
            report.is_valid = False
        else:
            report.is_valid = True

        report.recorded_stats = self._record_stats(report.accepted, images)
        return report

    def _check_image(self, img: ImageDescriptor) -> Tuple[bool, Optional[str]]:
        if not img.image_id or not str(img.image_id).strip():
            return False, "missing image_id"
        if img.timestamp is None:
            return False, "missing acquisition timestamp"
        if not img.source_path or not str(img.source_path).strip():
            return False, "missing source_path"

        decodable, decode_reason = self._probe_decodable(img)
        if not decodable:
            return False, decode_reason
        return True, None

    def _probe_decodable(self, img: ImageDescriptor) -> Tuple[bool, Optional[str]]:
        image = cv2.imread(img.source_path, cv2.IMREAD_UNCHANGED)
        if image is None:
            return False, "decode error: image unreadable or unsupported"
        return True, None

    def _record_stats(
        self,
        accepted: List[ImageDescriptor],
        all_images: List[ImageDescriptor],
    ) -> dict:
        metadata_available = sum(1 for img in accepted if img.metadata)
        return {
            "total_submitted": len(all_images),
            "accepted_count": len(accepted),
            "rejected_count": len(all_images) - len(accepted),
            "metadata_available_count": metadata_available,
            "resolution_info": "TBD",
        }
