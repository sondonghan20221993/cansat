from __future__ import annotations

import argparse
import json
import os
from typing import List

from reconstruction.backends.dust3r_backend import Dust3rBackend
from reconstruction.backends.feature_sfm_backend import FeatureSfmBackend
from reconstruction.client.server_client import ServerClient
from reconstruction.config import ReconstructionConfig
from reconstruction.core.orchestrator import ReconstructionOrchestrator
from reconstruction.exporters.glb_exporter import GlbExporter
from reconstruction.models.job import ImageDescriptor
from reconstruction.server.service import ReconstructionService
from reconstruction.validation.image_validator import ImageValidator


def main(argv: List[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Run reconstruction skeleton with real image inputs.")
    parser.add_argument("images", nargs="+", help="Input image paths")
    parser.add_argument("--image-set-id", default="prototype", help="Logical image set identifier")
    parser.add_argument(
        "--backend",
        default="feature_sfm",
        choices=["feature_sfm", "dust3r"],
        help="feature_sfm runs a real sparse image-based prototype; dust3r preserves the future model boundary only.",
    )
    args = parser.parse_args(argv)

    descriptors = [
        ImageDescriptor(
            image_id=f"img-{idx + 1}",
            timestamp=idx,
            source_path=os.path.abspath(path),
            metadata={},
        )
        for idx, path in enumerate(args.images)
    ]

    config = ReconstructionConfig(backend_name=args.backend, output_format="glb")
    validator = ImageValidator(config.min_image_count)
    backend = FeatureSfmBackend() if config.backend_name == "feature_sfm" else Dust3rBackend(model_name=config.backend_name)
    service = ReconstructionService(backend=backend, exporter=GlbExporter())
    client = ServerClient(service=service)
    orchestrator = ReconstructionOrchestrator(validator=validator, server_client=client)
    result = orchestrator.run(
        images=descriptors,
        image_set_id=args.image_set_id,
        output_format=config.output_format,
    )

    print(json.dumps({
        "job_id": result.job_id,
        "image_set_id": result.image_set_id,
        "status": result.status.value,
        "output_ref": result.output_ref,
        "output_format": result.output_format,
        "error_code": result.error_code,
        "quality": {
            "images_used": result.quality.images_used,
            "processing_status": result.quality.processing_status.value,
            "quality_indicators": result.quality.quality_indicators,
        },
        "extra": result.extra,
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
