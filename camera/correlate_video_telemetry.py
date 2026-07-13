#!/usr/bin/env python3
"""fpv4win 녹화 영상과 openMCT 텔레메트리 CSV를 절대시각으로 매칭.

전제: 영상 파일명(`<epoch_ms>.mp4`, 지상 PC 시계 기준)과 CSV의 timestamp
(`datetime.now().isoformat()`, fc_serial_ws_server.py 수신 시각, 같은 지상
PC 시계 기준)가 동일한 시간축이라는 것 — OSD 번인이 녹화 파일에 반영되지
않는 문제(camera/README.md 트러블슈팅 참조)의 대안으로 채택.

사용법:
    python3 camera/correlate_video_telemetry.py <video.mp4> <telemetry.csv> [--out matched.csv]

동작:
    1. 영상 파일명에서 시작 epoch(ms) 파싱, ffprobe로 길이(초) 조회
    2. CSV의 timestamp(ISO)를 epoch로 변환, 영상 구간에 속하는 행만 필터
    3. 각 행에 video_offset_sec(영상 재생 시점, 초) 컬럼 추가해서 출력
"""
import argparse
import csv
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path


def video_start_epoch_ms(video_path: Path) -> int:
    m = re.match(r"(\d+)\.mp4$", video_path.name)
    if not m:
        raise ValueError(f"파일명이 <epoch_ms>.mp4 형식이 아님: {video_path.name}")
    return int(m.group(1))


def video_duration_sec(video_path: Path) -> float:
    result = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration",
         "-of", "default=noprint_wrappers=1:nokey=1", str(video_path)],
        capture_output=True, text=True, check=True,
    )
    return float(result.stdout.strip())


def row_epoch_ms(timestamp_iso: str) -> int:
    return int(datetime.fromisoformat(timestamp_iso).timestamp() * 1000)


def correlate(video_path: Path, csv_path: Path, out_path: Path) -> int:
    start_ms = video_start_epoch_ms(video_path)
    duration_s = video_duration_sec(video_path)
    end_ms = start_ms + int(duration_s * 1000)

    start_dt = datetime.fromtimestamp(start_ms / 1000)
    end_dt = datetime.fromtimestamp(end_ms / 1000)
    print(f"영상: {video_path.name}")
    print(f"구간: {start_dt.isoformat()} ~ {end_dt.isoformat()} ({duration_s:.1f}초)")

    with open(csv_path, newline="") as f:
        reader = csv.DictReader(f)
        fieldnames = list(reader.fieldnames or []) + ["video_offset_sec"]
        matched = []
        for row in reader:
            ts = row.get("timestamp")
            if not ts:
                continue
            try:
                row_ms = row_epoch_ms(ts)
            except ValueError:
                continue
            if start_ms <= row_ms <= end_ms:
                row["video_offset_sec"] = f"{(row_ms - start_ms) / 1000:.3f}"
                matched.append(row)

    if not matched:
        print("매칭된 텔레메트리 행 없음 — 영상/CSV 시각 구간이 안 겹침 (PC 시계 확인 필요)")
        return 1

    with open(out_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(matched)

    print(f"매칭 행 수: {len(matched)} -> {out_path}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("video", type=Path)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--out", type=Path, default=None)
    args = parser.parse_args()

    out_path = args.out or args.video.with_suffix(".matched.csv")
    return correlate(args.video, args.csv, out_path)


if __name__ == "__main__":
    sys.exit(main())
