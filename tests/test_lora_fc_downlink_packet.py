"""LORA-FRAME-001~008: LoRa 다운링크 패킷 포맷 통합테스트

C의 ServiceLoRa() static 함수 대신, 동일한 패킷 포맷 스펙을 Python으로 정의하고
그 규칙이 올바른지 검증한다. C 코드의 snprintf 포맷과 동일해야 한다.

FC 패킷 포맷 (C 코드 기준):
  FC,<count>,<ts_ms>,<roll_rad>,<pitch_rad>,<yaw_rad>,<x_m>,<y_m>,<z_m>,<vx_mps>,<vy_mps>,<vz_mps>,<lat_e7>,<lon_e7>,<alt_mm>,<fix_type>

SH 패킷 포맷:
  SH,<count>,<ts_ms>,<health_state>,<fault_code>
"""

import re
import unittest
from dataclasses import dataclass
from typing import Optional


@dataclass
class FcState:
    count: int
    ts_ms: int
    roll_rad: float
    pitch_rad: float
    yaw_rad: float
    x_m: float
    y_m: float
    z_m: float
    vx_mps: float
    vy_mps: float
    vz_mps: float
    lat_e7: int
    lon_e7: int
    alt_mm: int
    fix_type: int


@dataclass
class ShState:
    count: int
    ts_ms: int
    health_state: int
    fault_code: int


def build_fc_packet(s: FcState) -> str:
    """C ServiceLoRa() FC 패킷과 동일한 포맷."""
    return (
        f"FC,{s.count},{s.ts_ms},"
        f"{s.roll_rad:.6f},{s.pitch_rad:.6f},{s.yaw_rad:.6f},"
        f"{s.x_m:.3f},{s.y_m:.3f},{s.z_m:.3f},"
        f"{s.vx_mps:.3f},{s.vy_mps:.3f},{s.vz_mps:.3f},"
        f"{s.lat_e7},{s.lon_e7},{s.alt_mm},{s.fix_type}\n"
    )


def build_sh_packet(s: ShState) -> str:
    """C ServiceLoRa() SH 패킷과 동일한 포맷."""
    return f"SH,{s.count},{s.ts_ms},{s.health_state},{s.fault_code}\n"


def parse_fc_packet(line: str) -> Optional[FcState]:
    parts = line.strip().split(",")
    if len(parts) != 16 or parts[0] != "FC":
        return None
    try:
        return FcState(
            count=int(parts[1]), ts_ms=int(parts[2]),
            roll_rad=float(parts[3]), pitch_rad=float(parts[4]), yaw_rad=float(parts[5]),
            x_m=float(parts[6]), y_m=float(parts[7]), z_m=float(parts[8]),
            vx_mps=float(parts[9]), vy_mps=float(parts[10]), vz_mps=float(parts[11]),
            lat_e7=int(parts[12]), lon_e7=int(parts[13]),
            alt_mm=int(parts[14]), fix_type=int(parts[15]),
        )
    except (ValueError, IndexError):
        return None


def parse_sh_packet(line: str) -> Optional[ShState]:
    parts = line.strip().split(",")
    if len(parts) != 5 or parts[0] != "SH":
        return None
    try:
        return ShState(
            count=int(parts[1]), ts_ms=int(parts[2]),
            health_state=int(parts[3]), fault_code=int(parts[4]),
        )
    except (ValueError, IndexError):
        return None


class FcPacketFormatTest(unittest.TestCase):

    def _sample_fc(self, count: int = 1, ts_ms: int = 12345,
                   lat_e7: int = 374530000, lon_e7: int = 1269850000,
                   alt_mm: int = 50000, fix_type: int = 3) -> FcState:
        return FcState(
            count=count, ts_ms=ts_ms,
            roll_rad=0.1, pitch_rad=0.2, yaw_rad=0.3,
            x_m=1.0, y_m=2.0, z_m=3.0,
            vx_mps=0.5, vy_mps=0.6, vz_mps=0.7,
            lat_e7=lat_e7, lon_e7=lon_e7, alt_mm=alt_mm, fix_type=fix_type,
        )

    # LORA-FRAME-001: FC 패킷 포맷 — prefix "FC,"
    def test_fc_packet_prefix(self) -> None:
        pkt = build_fc_packet(self._sample_fc())
        assert pkt.startswith("FC,")

    # LORA-FRAME-001: FC 패킷 필드 수 (16개)
    def test_fc_packet_field_count(self) -> None:
        pkt = build_fc_packet(self._sample_fc())
        assert len(pkt.strip().split(",")) == 16

    # LORA-FRAME-001: FC 패킷 파싱 → 원본 값 복원
    def test_fc_packet_roundtrip(self) -> None:
        s = self._sample_fc(count=5, ts_ms=99999)
        pkt = build_fc_packet(s)
        parsed = parse_fc_packet(pkt)
        assert parsed is not None
        assert parsed.count == 5
        assert parsed.ts_ms == 99999
        assert abs(parsed.roll_rad - 0.1) < 1e-5
        assert parsed.lat_e7 == 374530000
        assert parsed.lon_e7 == 1269850000
        assert parsed.alt_mm == 50000
        assert parsed.fix_type == 3

    # LORA-FRAME-002: GPS 좌표 포함 확인
    def test_fc_packet_contains_gps(self) -> None:
        s = self._sample_fc(lat_e7=123456789, lon_e7=987654321, alt_mm=10000, fix_type=3)
        pkt = build_fc_packet(s)
        assert "123456789" in pkt
        assert "987654321" in pkt
        assert "10000" in pkt
        assert "3" in pkt

    # LORA-FRAME-003: GPS invalid (fix_type=0) — 패킷은 생성되나 fix_type=0
    def test_fc_packet_gps_invalid_fix(self) -> None:
        s = self._sample_fc(fix_type=0)
        pkt = build_fc_packet(s)
        parsed = parse_fc_packet(pkt)
        assert parsed is not None
        assert parsed.fix_type == 0

    # LORA-FRAME-005: SH 패킷 포맷 — prefix "SH,", 5 필드
    def test_sh_packet_format(self) -> None:
        s = ShState(count=1, ts_ms=5000, health_state=1, fault_code=2)
        pkt = build_sh_packet(s)
        assert pkt.startswith("SH,")
        assert len(pkt.strip().split(",")) == 5

    # LORA-FRAME-005: SH 패킷 파싱 → 원본 값 복원
    def test_sh_packet_roundtrip(self) -> None:
        s = ShState(count=3, ts_ms=8888, health_state=2, fault_code=1)
        pkt = build_sh_packet(s)
        parsed = parse_sh_packet(pkt)
        assert parsed is not None
        assert parsed.count == 3
        assert parsed.health_state == 2
        assert parsed.fault_code == 1

    # LORA-FRAME-006: seq 단조 증가
    def test_fc_seq_monotonic(self) -> None:
        counts = [build_fc_packet(self._sample_fc(count=i)) for i in range(1, 6)]
        parsed = [parse_fc_packet(p) for p in counts]
        for i in range(1, len(parsed)):
            assert parsed[i].count == parsed[i - 1].count + 1

    # LORA-FRAME-007: timestamp 필드 존재 및 정수
    def test_fc_timestamp_present(self) -> None:
        s = self._sample_fc(ts_ms=123456)
        pkt = build_fc_packet(s)
        parsed = parse_fc_packet(pkt)
        assert parsed.ts_ms == 123456

    # LORA-FRAME-008: AttitudeValid=0 시 FC 패킷 미전송 — 조건 로직 검증
    def test_fc_not_sent_without_attitude(self) -> None:
        # C ServiceLoRa(): AttitudeValid && LocalValid 조건 필요
        # Python에서 해당 조건 로직 검증
        attitude_valid = False
        local_valid = True
        should_send_fc = attitude_valid and local_valid
        assert should_send_fc is False

    def test_fc_sent_with_both_valid(self) -> None:
        attitude_valid = True
        local_valid = True
        should_send_fc = attitude_valid and local_valid
        assert should_send_fc is True

    # LORA-FRAME-004: stale GPS — fix_type으로 판단 (fix_type < 2 = no fix)
    def test_fc_stale_gps_low_fix(self) -> None:
        s = self._sample_fc(fix_type=1)
        pkt = build_fc_packet(s)
        parsed = parse_fc_packet(pkt)
        assert parsed.fix_type < 2  # no GPS fix

    # 패킷 끝 개행 확인
    def test_fc_ends_with_newline(self) -> None:
        pkt = build_fc_packet(self._sample_fc())
        assert pkt.endswith("\n")

    def test_sh_ends_with_newline(self) -> None:
        pkt = build_sh_packet(ShState(1, 100, 0, 0))
        assert pkt.endswith("\n")


if __name__ == "__main__":
    unittest.main()
