from __future__ import annotations

import json
import os
import shutil
import time
from dataclasses import dataclass, field
from typing import Callable


DispatchFn = Callable[[list[str], str], dict]
LogFn = Callable[[dict], None]


@dataclass
class InboxMonitorConfig:
    inbox_dir: str
    processed_dir: str
    rejected_dir: str
    chunk_size: int
    poll_interval_s: float = 2.0
    image_set_prefix: str = "inbox-chunk"


@dataclass
class InboxMonitorState:
    buffer: list[str] = field(default_factory=list)
    seen_paths: set[str] = field(default_factory=set)
    dispatched_jobs: list[dict] = field(default_factory=list)
    rejected_files: list[str] = field(default_factory=list)


class InboxMonitor:
    def __init__(
        self,
        config: InboxMonitorConfig,
        dispatch_fn: DispatchFn,
        log_fn: LogFn | None = None,
    ) -> None:
        if config.chunk_size <= 0:
            raise ValueError("chunk_size must be > 0")
        self.config = config
        self.dispatch_fn = dispatch_fn
        self.log_fn = log_fn or (lambda payload: None)
        self.state = InboxMonitorState()

    def run_forever(self) -> None:
        while True:
            self.run_once()
            time.sleep(self.config.poll_interval_s)

    def run_once(self) -> None:
        os.makedirs(self.config.inbox_dir, exist_ok=True)
        os.makedirs(self.config.processed_dir, exist_ok=True)
        os.makedirs(self.config.rejected_dir, exist_ok=True)

        inbox_entries = sorted(
            os.path.join(self.config.inbox_dir, name)
            for name in os.listdir(self.config.inbox_dir)
        )
        for path in inbox_entries:
            if os.path.isdir(path):
                continue
            if path in self.state.seen_paths:
                continue
            if not self._validate_image(path):
                rejected_path = self._move_atomic(path, self.config.rejected_dir)
                self.state.rejected_files.append(rejected_path)
                self.log_fn({
                    "status": "rejected",
                    "source": path,
                    "destination": rejected_path,
                    "reason": "UNREADABLE_IMAGE",
                })
                continue
            self.state.buffer.append(path)
            self.state.seen_paths.add(path)
            self.log_fn({
                "status": "buffered",
                "path": path,
                "buffer_size": len(self.state.buffer),
            })

        while len(self.state.buffer) >= self.config.chunk_size:
            batch = self.state.buffer[:self.config.chunk_size]
            image_set_id = f"{self.config.image_set_prefix}-{len(self.state.dispatched_jobs) + 1:03d}"
            dispatch_result = self.dispatch_fn(list(batch), image_set_id)
            moved = [self._move_atomic(path, self.config.processed_dir) for path in batch]
            self.state.buffer = self.state.buffer[self.config.chunk_size:]
            self.state.dispatched_jobs.append({
                "image_set_id": image_set_id,
                "images": moved,
                "dispatch_result": dispatch_result,
            })
            self.log_fn({
                "status": "dispatched",
                "image_set_id": image_set_id,
                "image_count": len(moved),
                "processed_paths": moved,
                "dispatch_result": dispatch_result,
            })

    def _validate_image(self, path: str) -> bool:
        try:
            with open(path, "rb") as fp:
                header = fp.read(16)
        except OSError:
            return False
        if len(header) < 8:
            return False
        png_signature = b"\x89PNG\r\n\x1a\n"
        jpeg_signature = b"\xff\xd8"
        return header.startswith(png_signature) or header.startswith(jpeg_signature)

    def _move_atomic(self, source_path: str, destination_dir: str) -> str:
        destination_path = os.path.join(destination_dir, os.path.basename(source_path))
        if os.path.exists(destination_path):
            raise FileExistsError(f"Destination already exists: {destination_path}")
        try:
            os.replace(source_path, destination_path)
        except OSError:
            shutil.copy2(source_path, destination_path)
            os.remove(source_path)
        self.state.seen_paths.discard(source_path)
        return destination_path


def jsonl_logger(path: str) -> LogFn:
    abs_path = os.path.abspath(path)
    os.makedirs(os.path.dirname(abs_path) or ".", exist_ok=True)

    def _log(payload: dict) -> None:
        with open(abs_path, "a", encoding="utf-8") as fp:
            fp.write(json.dumps(payload) + "\n")

    return _log
