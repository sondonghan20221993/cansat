from __future__ import annotations

import argparse
import json
import mimetypes
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any
from urllib.parse import urlparse

from reconstruction.backends.dust3r_backend import Dust3rBackend
from reconstruction.backends.feature_sfm_backend import FeatureSfmBackend
from reconstruction.exporters.glb_exporter import GlbExporter
from reconstruction.models.wire import (
    image_descriptor_from_dict,
    request_from_dict,
    response_to_dict,
    session_response_to_dict,
    session_transform_update_from_dict,
)
from reconstruction.server.service import ReconstructionService


def make_server(host: str, port: int, backend_name: str, artifact_root: str) -> ThreadingHTTPServer:
    class ReconstructionHttpHandler(BaseHTTPRequestHandler):
        service = _make_service(backend_name, artifact_root)

        def do_GET(self) -> None:  # noqa: N802
            parsed = urlparse(self.path)
            if parsed.path == "/health":
                self._send_json({"status": "ok", "backend": backend_name})
                return
            parts = _path_parts(parsed.path)
            if len(parts) == 2 and parts[0] == "jobs":
                response = self.service.fetch_result(parts[1])
                self._send_json(response_to_dict(response))
                return
            if len(parts) == 2 and parts[0] == "sessions":
                response = self.service.get_session_state(parts[1])
                self._send_json(session_response_to_dict(response))
                return
            if len(parts) == 3 and parts[0] == "sessions" and parts[2] == "state":
                response = self.service.get_session_state(parts[1])
                self._send_json(session_response_to_dict(response))
                return
            if len(parts) == 3 and parts[0] == "jobs" and parts[2] == "artifact":
                response = self.service.fetch_result(parts[1])
                if not response.result_ref:
                    self._send_json({"error": "artifact_not_available"}, status=404)
                    return
                self._send_file(str(response.result_ref))
                return
            if len(parts) == 3 and parts[0] == "sessions" and parts[2] == "artifact":
                response = self.service.get_session_state(parts[1])
                if not response.artifact_ref:
                    self._send_json({"error": "artifact_not_available"}, status=404)
                    return
                self._send_file(str(response.artifact_ref))
                return
            self._send_json({"error": "not_found"}, status=404)

        def do_POST(self) -> None:  # noqa: N802
            parsed = urlparse(self.path)
            if parsed.path == "/jobs":
                payload = self._read_json()
                request = request_from_dict(payload)
                job_id = self.service.submit(request)
                response = self.service.fetch_result(job_id)
                self._send_json({
                    "job_id": job_id,
                    "status": response.status.value,
                    "poll_url": f"/jobs/{job_id}",
                    "artifact_url": f"/jobs/{job_id}/artifact",
                }, status=202)
                return
            if parsed.path == "/sessions":
                payload = self._read_json()
                response = self.service.start_session(payload.get("image_sequence_id"), payload.get("session_config"))
                self._send_json(session_response_to_dict(response), status=202)
                return
            parts = _path_parts(parsed.path)
            if len(parts) == 3 and parts[0] == "jobs" and parts[2] == "cancel":
                self._send_json({"job_id": parts[1], "accepted": False, "reason": "cancel_not_supported"})
                return
            if len(parts) == 3 and parts[0] == "sessions" and parts[2] == "frames":
                payload = self._read_json()
                ordered_frames = [image_descriptor_from_dict(item) for item in payload.get("ordered_frames", [])]
                response = self.service.append_frames(parts[1], ordered_frames)
                self._send_json(session_response_to_dict(response), status=202)
                return
            if len(parts) == 3 and parts[0] == "sessions" and parts[2] == "transform":
                payload = self._read_json()
                response = self.service.update_session_transform(parts[1], session_transform_update_from_dict(payload))
                self._send_json(session_response_to_dict(response))
                return
            if len(parts) == 3 and parts[0] == "sessions" and parts[2] == "export":
                payload = self._read_json()
                response = self.service.export_session_artifact(parts[1], str(payload.get("output_format", "ply")))
                self._send_json(session_response_to_dict(response))
                return
            if len(parts) == 3 and parts[0] == "sessions" and parts[2] == "end":
                payload = self._read_json()
                response = self.service.end_session(parts[1], str(payload.get("mode", "finalize")))
                self._send_json(session_response_to_dict(response))
                return
            self._send_json({"error": "not_found"}, status=404)

        def log_message(self, fmt: str, *args: Any) -> None:
            print(f"[reconstruction-http] {self.address_string()} - {fmt % args}")

        def _read_json(self) -> dict[str, Any]:
            length = int(self.headers.get("Content-Length", "0"))
            if length <= 0:
                return {}
            return json.loads(self.rfile.read(length).decode("utf-8"))

        def _send_json(self, payload: dict[str, Any], status: int = 200) -> None:
            body = json.dumps(payload, indent=2).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def _send_file(self, path: str) -> None:
            abs_path = os.path.abspath(path)
            if not os.path.exists(abs_path) or not os.path.isfile(abs_path):
                self._send_json({"error": "artifact_file_missing"}, status=404)
                return
            with open(abs_path, "rb") as fp:
                body = fp.read()
            filename = os.path.basename(abs_path)
            content_type = mimetypes.guess_type(filename)[0] or "application/octet-stream"
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Disposition", f'attachment; filename="{filename}"')
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    return ThreadingHTTPServer((host, port), ReconstructionHttpHandler)


def _make_service(backend_name: str, artifact_root: str) -> ReconstructionService:
    backend = FeatureSfmBackend() if backend_name == "feature_sfm" else Dust3rBackend(model_name=backend_name)
    return ReconstructionService(backend=backend, exporter=GlbExporter(artifact_root=artifact_root))


def _path_parts(path: str) -> list[str]:
    return [part for part in path.split("/") if part]


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the prototype reconstruction HTTP polling server.")
    parser.add_argument("--host", default="0.0.0.0", help="Bind host")
    parser.add_argument("--port", type=int, default=8765, help="Bind port")
    parser.add_argument("--backend", default="feature_sfm", choices=["feature_sfm", "dust3r"], help="Server backend")
    parser.add_argument("--artifact-root", default="artifacts/reconstruction", help="Directory for generated artifacts")
    args = parser.parse_args()

    server = make_server(args.host, args.port, args.backend, args.artifact_root)
    print(f"Reconstruction HTTP server listening on http://{args.host}:{args.port} backend={args.backend}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("Stopping reconstruction HTTP server.")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
