"""TDM-DOWN-001~006: lora_tdm_app LoRa 다운링크 패킷 포맷 테스트

C의 BuildFcDownlinkLine() / BuildShDownlinkLine() static 함수와 동일한 패킷 포맷 스펙을
Python으로 정의하고 검증한다.

FC 패킷 포맷 (lora_tdm_app_utils.c BuildFcDownlinkLine):
  FC,<seq>,<ts>,<roll>,<pitch>,<yaw>,<x>,<y>,<z>,<vx>,<vy>,<vz>,<lat_e7>,<lon_e7>,<alt_mm>,<fix>,<ufb>\\n
  float 정밀도: %.4f

SH 패킷 포맷 (BuildShDownlinkLine):
  SH,<seq>,<ts>,<state>,<fault>,<linkstate>,<ufb>\\n
"""

import unittest
from dataclasses import dataclass
from typing import Optional


# ---- 데이터 클래스 ----

@dataclass
class FcState:
    seq: int
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
    fix: int
    ufb: int  # UplinkFeedback: 0=OK, 1=CRC_FAIL, 2=SEQ_FAIL


@dataclass
class ShState:
    seq: int
    ts_ms: int
    health_state: int
    fault_code: int
    link_state: int
    ufb: int


# ---- 빌더 (C snprintf 포맷 동일 재현) ----

def build_fc_packet(s: FcState) -> str:
    """C BuildFcDownlinkLine() 포맷 재현."""
    return (
        f"FC,{s.seq},{s.ts_ms},"
        f"{s.roll_rad:.4f},{s.pitch_rad:.4f},{s.yaw_rad:.4f},"
        f"{s.x_m:.4f},{s.y_m:.4f},{s.z_m:.4f},"
        f"{s.vx_mps:.4f},{s.vy_mps:.4f},{s.vz_mps:.4f},"
        f"{s.lat_e7},{s.lon_e7},{s.alt_mm},{s.fix},{s.ufb}\n"
    )


def build_sh_packet(s: ShState) -> str:
    """C BuildShDownlinkLine() 포맷 재현."""
    return f"SH,{s.seq},{s.ts_ms},{s.health_state},{s.fault_code},{s.link_state},{s.ufb}\n"


# ---- 파서 ----

def parse_fc_packet(line: str) -> Optional[FcState]:
    parts = line.strip().split(",")
    if len(parts) != 17 or parts[0] != "FC":
        return None
    try:
        return FcState(
            seq=int(parts[1]), ts_ms=int(parts[2]),
            roll_rad=float(parts[3]), pitch_rad=float(parts[4]), yaw_rad=float(parts[5]),
            x_m=float(parts[6]), y_m=float(parts[7]), z_m=float(parts[8]),
            vx_mps=float(parts[9]), vy_mps=float(parts[10]), vz_mps=float(parts[11]),
            lat_e7=int(parts[12]), lon_e7=int(parts[13]),
            alt_mm=int(parts[14]), fix=int(parts[15]), ufb=int(parts[16]),
        )
    except (ValueError, IndexError):
        return None


def parse_sh_packet(line: str) -> Optional[ShState]:
    parts = line.strip().split(",")
    if len(parts) != 7 or parts[0] != "SH":
        return None
    try:
        return ShState(
            seq=int(parts[1]), ts_ms=int(parts[2]),
            health_state=int(parts[3]), fault_code=int(parts[4]),
            link_state=int(parts[5]), ufb=int(parts[6]),
        )
    except (ValueError, IndexError):
        return None


# ---- 테스트 ----

class FcPacketFormatTest(unittest.TestCase):

    def _sample_fc(self, seq: int = 1, ts_ms: int = 12345,
                   lat_e7: int = 374530000, lon_e7: int = 1269850000,
                   alt_mm: int = 50000, fix: int = 3, ufb: int = 0) -> FcState:
        return FcState(
            seq=seq, ts_ms=ts_ms,
            roll_rad=0.1, pitch_rad=0.2, yaw_rad=0.3,
            x_m=1.0, y_m=2.0, z_m=3.0,
            vx_mps=0.5, vy_mps=0.6, vz_mps=0.7,
            lat_e7=lat_e7, lon_e7=lon_e7, alt_mm=alt_mm, fix=fix, ufb=ufb,
        )

    # TDM-DOWN-001: FC 패킷 prefix
    def test_fc_packet_prefix(self) -> None:
        assert build_fc_packet(self._sample_fc()).startswith("FC,")

    # TDM-DOWN-001: FC 패킷 필드 수 — 17개 (ufb 포함)
    def test_fc_packet_field_count(self) -> None:
        pkt = build_fc_packet(self._sample_fc())
        assert len(pkt.strip().split(",")) == 17

    # TDM-DOWN-001: FC 패킷 왕복 — 원본 값 복원
    def test_fc_packet_roundtrip(self) -> None:
        s = self._sample_fc(seq=5, ts_ms=99999, ufb=0)
        pkt = build_fc_packet(s)
        parsed = parse_fc_packet(pkt)
        assert parsed is not None
        assert parsed.seq == 5
        assert parsed.ts_ms == 99999
        assert abs(parsed.roll_rad - 0.1) < 1e-3
        assert parsed.lat_e7 == 374530000
        assert parsed.lon_e7 == 1269850000
        assert parsed.alt_mm == 50000
        assert parsed.fix == 3
        assert parsed.ufb == 0

    # TDM-DOWN-001: ufb 필드 값 전달
    def test_fc_packet_ufb_crc_fail(self) -> None:
        s = self._sample_fc(ufb=1)
        pkt = build_fc_packet(s)
        parsed = parse_fc_packet(pkt)
        assert parsed is not None
        assert parsed.ufb == 1

    # TDM-DOWN-002: GPS 좌표 포함
    def test_fc_packet_contains_gps(self) -> None:
        s = self._sample_fc(lat_e7=123456789, lon_e7=987654321, alt_mm=10000, fix=3)
        pkt = build_fc_packet(s)
        assert "123456789" in pkt
        assert "987654321" in pkt
        assert "10000" in pkt

    # TDM-DOWN-002: GPS fix=0 패킷 생성 허용 (lora_tdm_app은 항상 전송)
    def test_fc_packet_gps_no_fix_still_sent(self) -> None:
        s = self._sample_fc(fix=0)
        pkt = build_fc_packet(s)
        parsed = parse_fc_packet(pkt)
        assert parsed is not None
        assert parsed.fix == 0

    # TDM-DOWN-003: float 정밀도 %.4f
    def test_fc_float_precision(self) -> None:
        s = self._sample_fc()
        pkt = build_fc_packet(s)
        # roll=0.1 → "0.1000", not "0.100000"
        assert "0.1000" in pkt
        assert "0.2000" in pkt
        assert "0.3000" in pkt

    # TDM-DOWN-003: seq 단조 증가
    def test_fc_seq_monotonic(self) -> None:
        pkts = [build_fc_packet(self._sample_fc(seq=i)) for i in range(1, 6)]
        parsed = [parse_fc_packet(p) for p in pkts]
        for i in range(1, len(parsed)):
            assert parsed[i].seq == parsed[i - 1].seq + 1

    # TDM-DOWN-003: timestamp 필드
    def test_fc_timestamp_present(self) -> None:
        s = self._sample_fc(ts_ms=123456)
        parsed = parse_fc_packet(build_fc_packet(s))
        assert parsed.ts_ms == 123456

    # TDM-DOWN-004: SH 패킷 포맷 — 7필드
    def test_sh_packet_field_count(self) -> None:
        s = ShState(seq=1, ts_ms=5000, health_state=1, fault_code=2, link_state=1, ufb=0)
        pkt = build_sh_packet(s)
        assert pkt.startswith("SH,")
        assert len(pkt.strip().split(",")) == 7

    # TDM-DOWN-004: SH 패킷 왕복
    def test_sh_packet_roundtrip(self) -> None:
        s = ShState(seq=3, ts_ms=8888, health_state=2, fault_code=1, link_state=2, ufb=1)
        pkt = build_sh_packet(s)
        parsed = parse_sh_packet(pkt)
        assert parsed is not None
        assert parsed.seq == 3
        assert parsed.health_state == 2
        assert parsed.fault_code == 1
        assert parsed.link_state == 2
        assert parsed.ufb == 1

    # TDM-DOWN-005: 잘못된 필드 수 파서 거부
    def test_fc_wrong_field_count_rejected(self) -> None:
        # 구 포맷 (16필드, ufb 없음) — 새 파서는 거부
        old_format = "FC,1,100,0.1000,0.2000,0.3000,1.0000,2.0000,3.0000,0.5000,0.6000,0.7000,374530000,1269850000,50000,3\n"
        assert parse_fc_packet(old_format) is None

    def test_sh_wrong_field_count_rejected(self) -> None:
        # 구 포맷 (5필드, linkstate/ufb 없음)
        old_format = "SH,1,5000,1,2\n"
        assert parse_sh_packet(old_format) is None

    # 줄바꿈 종단
    def test_fc_ends_with_newline(self) -> None:
        assert build_fc_packet(self._sample_fc()).endswith("\n")

    def test_sh_ends_with_newline(self) -> None:
        assert build_sh_packet(ShState(1, 100, 0, 0, 1, 0)).endswith("\n")


if __name__ == "__main__":
    unittest.main()
