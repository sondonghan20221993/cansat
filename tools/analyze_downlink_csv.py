#!/usr/bin/env python3
"""openMCT telemetry_logs/*.csv 실측 분석 — LoRa 단계별(Stage 1/2/3) 판정.

사용법:
    python3 tools/analyze_downlink_csv.py <csv경로> [--cycle-ms 1000]

측정치:
    - 관측 다운링크 레이트 (Hz) — seq 파싱 성공 행 수 / 관측 구간(초)
    - ACK 송신 소요시간 통계 (_ack_send_ms) — 지상 처리 지연
    - RX 처리 총 소요시간 통계 (_rx_total_ms) — RX 윈도우 잠식 정도
    - source별(FC/SH) seq gap 기반 손실률
    - link_state / uplink_fb 분포

판정 기준(runbook과 동일 수치, notes/lora_stage_measurement_runbook.md 참조):
    Stage 1: 손실률 < 5%, ACK 송신 후 기체 link_state 값 관찰(정상화 여부는 기체 HK로 별도 확인)
    Stage 2: 관측 레이트 >= (1000/CYCLE_PERIOD_MS)*0.9 Hz, RX 총 소요시간 p95 < RX_WINDOW_MS
"""
import argparse
import csv
import statistics
import sys
from collections import defaultdict


def analyze(path: str, cycle_ms: int) -> int:
    rows = []
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)

    if not rows:
        print("행 없음 — 빈 CSV")
        return 1

    def to_float(v):
        try:
            return float(v)
        except (TypeError, ValueError):
            return None

    timestamps = [r["timestamp"] for r in rows if r.get("timestamp")]
    print(f"총 행 수: {len(rows)}")
    if len(timestamps) >= 2:
        print(f"관측 구간: {timestamps[0]} ~ {timestamps[-1]}")

    # source별 집계
    by_source = defaultdict(list)
    for r in rows:
        by_source[r.get("source", "?")].append(r)

    for source, srows in sorted(by_source.items()):
        print(f"\n=== source={source} ({len(srows)}건) ===")

        seqs = [int(r["seq"]) for r in srows if r.get("seq", "").strip().lstrip("-").isdigit()]
        if len(seqs) >= 2:
            gaps = [b - a for a, b in zip(seqs, seqs[1:]) if b > a]
            expected = sum(g for g in gaps) if gaps else 0
            received = len(gaps)
            loss_pct = round((expected - received) / expected * 100, 2) if expected > 0 else 0.0
            print(f"seq gap 기반 손실률: {loss_pct}% (expected={expected}, received={received})")

        ack_ms = [to_float(r.get("_ack_send_ms")) for r in srows]
        ack_ms = [v for v in ack_ms if v is not None]
        if ack_ms:
            print(f"ACK 송신 소요(ms): mean={statistics.mean(ack_ms):.1f} "
                  f"p95={sorted(ack_ms)[int(len(ack_ms)*0.95)]:.1f} max={max(ack_ms):.1f}")

        rx_ms = [to_float(r.get("_rx_total_ms")) for r in srows]
        rx_ms = [v for v in rx_ms if v is not None]
        if rx_ms:
            p95 = sorted(rx_ms)[int(len(rx_ms) * 0.95)]
            print(f"RX 총 소요(ms): mean={statistics.mean(rx_ms):.1f} p95={p95:.1f} max={max(rx_ms):.1f}")
            rx_window_ms = cycle_ms * 0.3  # 기본 비율(300ms/1000ms) 참고치, 실제 RX_WINDOW_MS와 별도 확인 필요
            if p95 > rx_window_ms:
                print(f"  ⚠ p95({p95:.1f}ms)가 참고 RX창({rx_window_ms:.0f}ms)을 초과 — "
                      f"실제 RX_WINDOW_MS 설정과 대조 필요")

        link_states = [r.get("link_state", "") for r in srows if r.get("link_state", "").strip() != ""]
        if link_states:
            counts = defaultdict(int)
            for v in link_states:
                counts[v] += 1
            print(f"link_state 분포: {dict(counts)}")

        ufb = [r.get("uplink_fb", "") for r in srows if r.get("uplink_fb", "").strip() != ""]
        if ufb:
            counts = defaultdict(int)
            for v in ufb:
                counts[v] += 1
            print(f"uplink_fb 분포: {dict(counts)} (0=OK 1=CRC_FAIL 2=SEQ_FAIL)")

    # 전체 관측 레이트 추정 (timestamp 첫/끝 구간 기준)
    if len(timestamps) >= 2:
        from datetime import datetime
        try:
            t0 = datetime.fromisoformat(timestamps[0])
            t1 = datetime.fromisoformat(timestamps[-1])
            span_s = (t1 - t0).total_seconds()
            if span_s > 0:
                rate_hz = len(rows) / span_s
                target_hz = 1000.0 / cycle_ms
                print(f"\n전체 관측 레이트: {rate_hz:.2f} Hz "
                      f"(목표 {target_hz:.2f} Hz 대비 {rate_hz/target_hz*100:.0f}%)")
        except ValueError:
            pass

    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path")
    parser.add_argument("--cycle-ms", type=int, default=1000,
                         help="현재 실측 중인 CYCLE_PERIOD_MS (기본 1000=Stage1, 400=Stage2 등)")
    args = parser.parse_args(argv)
    return analyze(args.csv_path, args.cycle_ms)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
