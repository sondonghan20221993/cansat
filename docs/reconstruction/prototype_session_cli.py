from __future__ import annotations

import argparse
import json
import os
from typing import List

from reconstruction.client.session_http_client import SessionHttpClient
from reconstruction.models.job import ImageDescriptor, SessionTransformUpdate


def _parse_linear(values: List[float] | None) -> list[list[float]]:
    if values is None:
        return [
            [1.0, 0.0, 0.0],
            [0.0, 1.0, 0.0],
            [0.0, 0.0, 1.0],
        ]
    if len(values) != 9:
        raise ValueError("--linear requires exactly 9 values")
    return [
        [float(values[0]), float(values[1]), float(values[2])],
        [float(values[3]), float(values[4]), float(values[5])],
        [float(values[6]), float(values[7]), float(values[8])],
    ]


def _parse_translate(values: List[float] | None) -> list[float]:
    if values is None:
        return [0.0, 0.0, 0.0]
    if len(values) != 3:
        raise ValueError("--translate requires exactly 3 values")
    return [float(values[0]), float(values[1]), float(values[2])]


def _print(payload: dict) -> None:
    print(json.dumps(payload, indent=2))


def main(argv: List[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Call the prototype session-oriented reconstruction API.")
    parser.add_argument("--endpoint", default="http://127.0.0.1:8765", help="Reconstruction server endpoint")
    parser.add_argument("--request-timeout-s", type=float, default=900.0, help="Per-request HTTP timeout")
    subparsers = parser.add_subparsers(dest="command", required=True)

    start = subparsers.add_parser("start")
    start.add_argument("--image-sequence-id")
    start.add_argument("--output-policy", default="session_state_only", choices=["session_state_only", "session_plus_export"])
    start.add_argument("--output-format", default="ply")

    append = subparsers.add_parser("append-frames")
    append.add_argument("--session-id", required=True)
    append.add_argument("images", nargs="+", help="Ordered frame paths as seen by the server")
    append.add_argument("--start-index", type=int, default=0)

    state = subparsers.add_parser("state")
    state.add_argument("--session-id", required=True)

    transform = subparsers.add_parser("update-transform")
    transform.add_argument("--session-id", required=True)
    transform.add_argument("--alignment-status", required=True, choices=["ALIGNED", "PARTIAL_ALIGNMENT", "UNALIGNED"])
    transform.add_argument("--scale", type=float, default=1.0)
    transform.add_argument("--linear", nargs=9, type=float)
    transform.add_argument("--translate", nargs=3, type=float)
    transform.add_argument("--updated-by", default="prototype_session_cli")

    export = subparsers.add_parser("export")
    export.add_argument("--session-id", required=True)
    export.add_argument("--output-format", default="ply")
    export.add_argument("--download-dir", default="artifacts/reconstruction/downloads")

    end = subparsers.add_parser("end")
    end.add_argument("--session-id", required=True)
    end.add_argument("--mode", default="finalize", choices=["finalize", "discard"])

    args = parser.parse_args(argv)

    client = SessionHttpClient(endpoint=args.endpoint, request_timeout_s=args.request_timeout_s)
    if not client.is_available():
        _print({
            "status": "failed",
            "error_code": "SERVER_UNAVAILABLE",
            "endpoint": args.endpoint,
        })
        return 1

    if args.command == "start":
        payload = client.start_session(
            args.image_sequence_id,
            {
                "output_policy": args.output_policy,
                "output_format": args.output_format,
            },
        )
        _print(payload)
        return 0

    if args.command == "append-frames":
        frames = [
            ImageDescriptor(
                image_id=f"frame-{args.start_index + idx:06d}",
                timestamp=args.start_index + idx,
                source_path=path,
                metadata={},
            )
            for idx, path in enumerate(args.images)
        ]
        payload = client.append_frames(args.session_id, frames)
        _print(payload)
        return 0

    if args.command == "state":
        payload = client.get_session_state(args.session_id)
        _print(payload)
        return 0

    if args.command == "update-transform":
        world_transform = None
        if args.alignment_status != "UNALIGNED":
            world_transform = {
                "scale": args.scale,
                "linear": _parse_linear(args.linear),
                "translate": _parse_translate(args.translate),
            }
        payload = client.update_session_transform(
            args.session_id,
            SessionTransformUpdate(
                alignment_status=args.alignment_status,
                world_transform=world_transform,
                updated_by=args.updated_by,
            ),
        )
        _print(payload)
        return 0

    if args.command == "export":
        payload = client.export_session_artifact(args.session_id, args.output_format)
        downloaded_artifact = None
        if payload.get("status") == "exported":
            downloaded_artifact = client.download_artifact(args.session_id, args.download_dir)
        payload["downloaded_artifact"] = os.path.abspath(downloaded_artifact) if downloaded_artifact else None
        _print(payload)
        return 0 if payload.get("status") == "exported" else 1

    payload = client.end_session(args.session_id, args.mode)
    _print(payload)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
