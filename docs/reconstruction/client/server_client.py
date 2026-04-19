from __future__ import annotations

from reconstruction.models.job import ReconstructionRequest, ReconstructionResponse
from reconstruction.server.service import ReconstructionService


class ServerClient:
    """
    Ground-side client boundary for the fixed remote server architecture.

    The transport can change later, but the client exposes a stable submit/fetch
    contract to the orchestrator.
    """

    def __init__(self, service: ReconstructionService) -> None:
        self._service = service

    def submit(self, request: ReconstructionRequest) -> str:
        return self._service.submit(request)

    def fetch_result(self, job_id: str) -> ReconstructionResponse:
        return self._service.fetch_result(job_id)
