from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from typing import List

from reconstruction.client.http_polling_client import HttpPollingClient
from reconstruction.models.job import ImageDescriptor, JobStatus, ReconstructionRequest


def main(argv: List[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Submit a reconstruction job to a remote server, poll for completion, and download the artifact."
    )
    parser.add_argument("images", nargs="+", help="Image paths as seen by the remote server")
    parser.add_argument("--endpoint", default="http://127.0.0.1:8765", help="Reconstruction server endpoint")
    parser.add_argument("--image-set-id", default="remote-prototype", help="Logical image set identifier")
    parser.add_argument("--output-format", default="glb", help="Requested output format token")
    parser.add_argument("--download-dir", default="artifacts/reconstruction/downloads", help="Local artifact download directory")
    parser.add_argument("--poll-interval-s", type=float, default=2.0, help="Polling interval")
    parser.add_argument("--timeout-s", type=float, default=600.0, help="Client polling timeout")
    parser.add_argument("--open-viewer", action="store_true", help="Generate and open fixed-frame viewer after download")
    parser.add_argument("--frame", default="enu", choices=["opencv", "enu"], help="Viewer frame preset")
    args = parser.parse_args(argv)

    client = HttpPollingClient(
        endpoint=args.endpoint,
        poll_interval_s=args.poll_interval_s,
        timeout_s=args.timeout_s,
    )
    if not client.is_available():
        print(json.dumps({
            "status": "failed",
            "error_code": "SERVER_UNAVAILABLE",
            "endpoint": args.endpoint,
        }, indent=2))
        return 1

    request = ReconstructionRequest(
        image_set_id=args.image_set_id,
        images=[
            ImageDescriptor(
                image_id=f"img-{idx + 1}",
                timestamp=idx,
                source_path=path,
                metadata={},
            )
            for idx, path in enumerate(args.images)
        ],
        output_format=args.output_format,
    )

    job_id = client.submit(request)
    response = client.wait_for_result(job_id)
    artifact_path = None
    if response.status == JobStatus.SUCCESS:
        artifact_path = client.download_artifact(job_id, args.download_dir)

    viewer_exit_code = None
    if artifact_path and args.open_viewer:
        cmd = [
            sys.executable,
            "-m",
            "reconstruction.prototype_ui_cli",
            "--artifact",
            artifact_path,
            "--frame",
            args.frame,
            "--image-set-id",
            args.image_set_id,
            "--open",
        ]
        viewer_exit_code = subprocess.call(cmd)

    print(json.dumps({
        "job_id": job_id,
        "status": response.status.value,
        "output_format": response.output_format,
        "remote_result_ref": response.result_ref,
        "downloaded_artifact": os.path.abspath(artifact_path) if artifact_path else None,
        "error_code": response.error_code,
        "quality_meta": response.quality_meta,
        "viewer_exit_code": viewer_exit_code,
    }, indent=2))

    return 0 if response.status == JobStatus.SUCCESS else 1


if __name__ == "__main__":
    raise SystemExit(main())
