from __future__ import annotations

import json
import os
import time
import urllib.error
import urllib.request
from typing import Optional

from reconstruction.executor.remote_executor import ExecutorFetchError, ExecutorSubmitError, RemoteExecutor
from reconstruction.models.job import JobStatus, ReconstructionRequest, ReconstructionResponse
from reconstruction.models.wire import request_to_dict, response_from_dict


class HttpPollingClient(RemoteExecutor):
    """HTTP polling implementation of the ground-side reconstruction client."""

    def __init__(self, endpoint: str, poll_interval_s: float = 2.0, timeout_s: float = 600.0) -> None:
        self._endpoint = endpoint.rstrip("/")
        self._poll_interval_s = poll_interval_s
        self._timeout_s = timeout_s

    def submit(self, request: ReconstructionRequest) -> str:
        try:
            payload = self._json_request("POST", "/jobs", request_to_dict(request))
        except Exception as exc:  # noqa: BLE001
            raise ExecutorSubmitError(f"Failed to submit reconstruction job: {exc}", job_id=request.job_id) from exc
        return str(payload["job_id"])

    def fetch_result(self, job_id: str) -> ReconstructionResponse:
        try:
            payload = self._json_request("GET", f"/jobs/{job_id}", None)
        except Exception as exc:  # noqa: BLE001
            raise ExecutorFetchError(f"Failed to fetch reconstruction result: {exc}", job_id=job_id) from exc
        return response_from_dict(payload)

    def wait_for_result(self, job_id: str) -> ReconstructionResponse:
        deadline = time.monotonic() + self._timeout_s
        while time.monotonic() < deadline:
            response = self.fetch_result(job_id)
            if response.status != JobStatus.PENDING:
                return response
            time.sleep(self._poll_interval_s)
        return ReconstructionResponse(job_id=job_id, status=JobStatus.TIMEOUT, error_code="CLIENT_POLL_TIMEOUT")

    def download_artifact(self, job_id: str, destination_dir: str) -> Optional[str]:
        os.makedirs(destination_dir, exist_ok=True)
        url = f"{self._endpoint}/jobs/{job_id}/artifact"
        try:
            with urllib.request.urlopen(url, timeout=30) as response:
                filename = _filename_from_headers(response.headers.get("Content-Disposition")) or f"{job_id}.artifact"
                destination = os.path.abspath(os.path.join(destination_dir, filename))
                with open(destination, "wb") as fp:
                    fp.write(response.read())
                return destination
        except urllib.error.HTTPError as exc:
            if exc.code == 404:
                return None
            raise ExecutorFetchError(f"Failed to download artifact: HTTP {exc.code}", job_id=job_id) from exc
        except Exception as exc:  # noqa: BLE001
            raise ExecutorFetchError(f"Failed to download artifact: {exc}", job_id=job_id) from exc

    def cancel(self, job_id: str) -> bool:
        payload = self._json_request("POST", f"/jobs/{job_id}/cancel", {})
        return bool(payload.get("accepted", False))

    def is_available(self) -> bool:
        try:
            payload = self._json_request("GET", "/health", None)
        except Exception:  # noqa: BLE001
            return False
        return payload.get("status") == "ok"

    @property
    def executor_name(self) -> str:
        return "http_polling"

    @property
    def endpoint(self) -> Optional[str]:
        return self._endpoint

    def _json_request(self, method: str, path: str, payload: Optional[dict]) -> dict:
        data = None
        headers = {"Accept": "application/json"}
        if payload is not None:
            data = json.dumps(payload).encode("utf-8")
            headers["Content-Type"] = "application/json"
        request = urllib.request.Request(
            f"{self._endpoint}{path}",
            data=data,
            headers=headers,
            method=method,
        )
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.loads(response.read().decode("utf-8"))


def _filename_from_headers(content_disposition: Optional[str]) -> Optional[str]:
    if not content_disposition:
        return None
    for part in content_disposition.split(";"):
        part = part.strip()
        if part.startswith("filename="):
            return part.split("=", 1)[1].strip('"')
    return None
