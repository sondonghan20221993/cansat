"""
reconstruction/executor/remote_executor.py

Remote executor interface skeleton.

The ground-side computer submits reconstruction jobs to a remote NVIDIA
RTX A6000-class GPU server and receives results back (REC-PROC-09 through
REC-PROC-13, REC-PERF-01, REC-PERF-02).

The transport protocol between ground-side and server is NOT yet determined
(OI-REC-07).  This module defines the abstract contract only.  Concrete
implementations (e.g. HTTP, gRPC, shared filesystem) MUST subclass
RemoteExecutor without changing the interface.
"""

from __future__ import annotations

import logging
from abc import ABC, abstractmethod
from typing import Optional

from reconstruction.models.job import ReconstructionRequest, ReconstructionResponse

logger = logging.getLogger(__name__)


class RemoteExecutor(ABC):
    """
    Abstract interface for submitting reconstruction jobs to the remote server.

    Concrete subclasses implement the actual transport (OI-REC-07).
    The module boundary contract is defined here and SHALL NOT change when
    the transport is swapped.
    """

    # ------------------------------------------------------------------
    # Core operations
    # ------------------------------------------------------------------

    @abstractmethod
    def submit(self, request: ReconstructionRequest) -> str:
        """
        Submit a reconstruction job to the remote server.

        Parameters
        ----------
        request : fully constructed ReconstructionRequest

        Returns
        -------
        str
            The job_id echoed back (or a server-assigned tracking token).
            The caller uses this to match the eventual response (REC-PROC-12).

        Raises
        ------
        ExecutorSubmitError
            If the job could not be submitted (network failure, auth error, etc.)
        """

    @abstractmethod
    def fetch_result(self, job_id: str) -> ReconstructionResponse:
        """
        Retrieve the result for a previously submitted job.

        Implementations MAY block until the result is available, or MAY
        return a PENDING response immediately and require polling.

        Parameters
        ----------
        job_id : the identifier returned by submit()

        Returns
        -------
        ReconstructionResponse
            status == PENDING  : result not yet available
            status == SUCCESS  : reconstruction completed successfully
            status == DEGRADED : reconstruction completed with low confidence
            status == FAILED   : reconstruction failed on the server
            status == TIMEOUT  : server did not respond within configured timeout
                                 (OI-REC-06 TBD)

        Raises
        ------
        ExecutorFetchError
            If the result could not be retrieved for reasons other than
            job status (e.g. network failure after submission).
        """

    @abstractmethod
    def cancel(self, job_id: str) -> bool:
        """
        Request cancellation of a pending or running job.

        Returns True if the cancellation was accepted, False otherwise.
        Not all transport implementations may support cancellation.
        """

    # ------------------------------------------------------------------
    # Health / connectivity
    # ------------------------------------------------------------------

    @abstractmethod
    def is_available(self) -> bool:
        """
        Return True if the remote server is reachable and ready.

        Used by the ground-side system to gate job submission.
        Implementation is transport-dependent (OI-REC-07).
        """

    # ------------------------------------------------------------------
    # Metadata
    # ------------------------------------------------------------------

    @property
    @abstractmethod
    def executor_name(self) -> str:
        """Human-readable name of this executor implementation."""

    @property
    def endpoint(self) -> Optional[str]:
        """
        The configured endpoint string, if applicable.
        Returns None for implementations that do not use a network endpoint.
        """
        return None


# ---------------------------------------------------------------------------
# Executor-specific exceptions
# ---------------------------------------------------------------------------

class ExecutorError(RuntimeError):
    """Base class for remote executor errors."""

    def __init__(self, message: str, job_id: Optional[str] = None) -> None:
        super().__init__(message)
        self.job_id = job_id


class ExecutorSubmitError(ExecutorError):
    """Raised when a job cannot be submitted to the remote server."""


class ExecutorFetchError(ExecutorError):
    """Raised when a result cannot be fetched from the remote server."""
