from __future__ import annotations

import argparse
import json
import os
from typing import List

from reconstruction.client.http_polling_client import HttpPollingClient
from reconstruction.inbox_monitor import InboxMonitor, InboxMonitorConfig, jsonl_logger
from reconstruction.models.job import ImageDescriptor, JobStatus, ReconstructionRequest
from reconstruction.map_manifest import create_manifest, load_manifest, save_manifest
from reconstruction.map_accumulator_cli import render_map


def _dispatch_remote(
    image_paths: list[str],
    image_set_id: str,
    endpoint: str,
    download_dir: str,
    output_format: str,
    poll_interval_s: float,
    timeout_s: float,
    request_timeout_s: float,
) -> dict:
    client = HttpPollingClient(
        endpoint=endpoint,
        poll_interval_s=poll_interval_s,
        timeout_s=timeout_s,
        request_timeout_s=request_timeout_s,
    )
    request = ReconstructionRequest(
        image_set_id=image_set_id,
        images=[
            ImageDescriptor(
                image_id=f"img-{idx + 1}",
                timestamp=idx,
                source_path=path,
                metadata={},
            )
            for idx, path in enumerate(image_paths)
        ],
        output_format=output_format,
    )
    job_id = client.submit(request)
    response = client.wait_for_result(job_id)
    artifact_path = client.download_artifact(job_id, download_dir) if response.status == JobStatus.SUCCESS else None
    return {
        "job_id": job_id,
        "status": response.status.value,
        "downloaded_artifact": os.path.abspath(artifact_path) if artifact_path else None,
        "error_code": response.error_code,
    }


def main(argv: List[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Monitor an inbox directory and auto-dispatch reconstruction jobs.")
    parser.add_argument("--inbox-dir", required=True)
    parser.add_argument("--processed-dir", required=True)
    parser.add_argument("--rejected-dir", required=True)
    parser.add_argument("--chunk-size", required=True, type=int)
    parser.add_argument("--endpoint", default="http://127.0.0.1:8765")
    parser.add_argument("--output-format", default="ply")
    parser.add_argument("--download-dir", default="artifacts/reconstruction/downloads")
    parser.add_argument("--poll-interval-s", type=float, default=2.0)
    parser.add_argument("--timeout-s", type=float, default=600.0)
    parser.add_argument("--request-timeout-s", type=float, default=900.0)
    parser.add_argument("--manifest", default=None)
    parser.add_argument("--map-id", default="live-map")
    parser.add_argument("--viewer-html", default=None)
    parser.add_argument("--log-jsonl", default=None)
    parser.add_argument("--run-once", action="store_true")
    args = parser.parse_args(argv)

    logger = jsonl_logger(args.log_jsonl) if args.log_jsonl else (lambda payload: None)

    if args.manifest and not os.path.exists(args.manifest):
        save_manifest(create_manifest(args.map_id), args.manifest)

    def dispatch_fn(image_paths: list[str], image_set_id: str) -> dict:
        result = _dispatch_remote(
            image_paths=image_paths,
            image_set_id=image_set_id,
            endpoint=args.endpoint,
            download_dir=args.download_dir,
            output_format=args.output_format,
            poll_interval_s=args.poll_interval_s,
            timeout_s=args.timeout_s,
            request_timeout_s=args.request_timeout_s,
        )
        if args.manifest and result.get("downloaded_artifact"):
            from reconstruction.map_manifest import MapChunk

            manifest = load_manifest(args.manifest)
            artifact_ref = os.path.relpath(result["downloaded_artifact"], os.path.dirname(os.path.abspath(args.manifest)))
            manifest.append_chunk(MapChunk(
                chunk_id=image_set_id,
                job_id=str(result["job_id"]),
                image_set_id=image_set_id,
                artifact_ref=artifact_ref,
                output_format=args.output_format,
            ))
            save_manifest(manifest, args.manifest)
            if args.viewer_html:
                render_map(args.manifest, args.viewer_html, max_points_per_chunk=15000)
        return result

    monitor = InboxMonitor(
        config=InboxMonitorConfig(
            inbox_dir=args.inbox_dir,
            processed_dir=args.processed_dir,
            rejected_dir=args.rejected_dir,
            chunk_size=args.chunk_size,
            poll_interval_s=args.poll_interval_s,
        ),
        dispatch_fn=dispatch_fn,
        log_fn=logger,
    )

    if args.run_once:
        monitor.run_once()
    else:
        monitor.run_forever()

    print(json.dumps({
        "status": "ok",
        "buffer_size": len(monitor.state.buffer),
        "dispatched_job_count": len(monitor.state.dispatched_jobs),
        "rejected_count": len(monitor.state.rejected_files),
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
