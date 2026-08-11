"""TDM-DOWN-001~006: lora_tdm_app LoRa 다운링크 패킷 포맷 테스트

C의 LORA_TDM_APP_BuildFcDownlinkLine() snprintf 포맷과 동일한 패킷 포맷 스펙을
Python으로 정의하고 검증한다. 이 파일 자체가 C 소스와 별개로 스펙을 재구현하는
자기참조 구조이므로(포맷 상수를 C에서 직접 파싱해오지 않음), 필드 수/정밀도를
바꿀 때는 반드시 `lora_tdm_app/fsw/src/lora_tdm_app_utils.c`의 실제 snprintf 포맷
문자열과 대조할 것 (BL-107, 2026-07-28 감사에서 17필드/%.6f·%.3f로 stale 발견됨).

FC 패킷 포맷 (lora_tdm_app_utils.c LORA_TDM_APP_BuildFcDownlinkLine, 18필드):
  FC,<seq>,<ts>,<roll>,<pitch>,<yaw>,<x>,<y>,<z>,<vx>,<vy>,<vz>,<lat_e7>,<lon_e7>,<alt_mm>,<fix>,<ufb>,<sats>\\n
  float 정밀도: %.4f (roll/pitch/yaw/x/y/z/vx/vy/vz 전부 동일 — angle과 position을
  다른 정밀도로 나누던 옛 포맷은 폐기됨)
  <sats>는 2026-07-13에 필드 18로 추가(기존 17필드 순서는 안 바꾸고 끝에 추가) —
  구 17필드 프레임과의 호환을 위해 지상 파서는 len(parts)>=18일 때만 읽음.

SH 패킷 포맷 (변경 없음):
  SH,<seq>,<ts>,<state>,<fault>,<linkstate>,<ufb>\\n
"""

import re
import unittest
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

_C_SRC = (
    Path(__file__).resolve().parent.parent
    / "lora_tdm_app" / "fsw" / "src" / "lora_tdm_app_utils.c"
)


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
    sats: int = 0  # SatellitesVisible — 필드 18 (2026-07-13 추가)


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
    """C LORA_TDM_APP_BuildFcDownlinkLine() snprintf 포맷 재현: 18필드, float 전부 %.4f."""
    return (
        f"FC,{s.seq},{s.ts_ms},"
        f"{s.roll_rad:.4f},{s.pitch_rad:.4f},{s.yaw_rad:.4f},"
        f"{s.x_m:.4f},{s.y_m:.4f},{s.z_m:.4f},"
        f"{s.vx_mps:.4f},{s.vy_mps:.4f},{s.vz_mps:.4f},"
        f"{s.lat_e7},{s.lon_e7},{s.alt_mm},{s.fix},{s.ufb},{s.sats}\n"
    )


def build_sh_packet(s: ShState) -> str:
    """C BuildShDownlinkLine() 포맷 재현."""
    return f"SH,{s.seq},{s.ts_ms},{s.health_state},{s.fault_code},{s.link_state},{s.ufb}\n"


# ---- 파서 ----

def parse_fc_packet(line: str) -> Optional[FcState]:
    # 구 17필드(sats 없음) 프레임과의 호환: len(parts)>=18일 때만 파싱(C 주석과 동일 규칙).
    parts = line.strip().split(",")
    if len(parts) < 18 or parts[0] != "FC":
        return None
    try:
        return FcState(
            seq=int(parts[1]), ts_ms=int(parts[2]),
            roll_rad=float(parts[3]), pitch_rad=float(parts[4]), yaw_rad=float(parts[5]),
            x_m=float(parts[6]), y_m=float(parts[7]), z_m=float(parts[8]),
            vx_mps=float(parts[9]), vy_mps=float(parts[10]), vz_mps=float(parts[11]),
            lat_e7=int(parts[12]), lon_e7=int(parts[13]),
            alt_mm=int(parts[14]), fix=int(parts[15]), ufb=int(parts[16]),
            sats=int(parts[17]),
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

    # TDM-DOWN-001: FC 패킷 필드 수 — 18개 (ufb+sats 포함, 2026-07-13 sats 추가)
    def test_fc_packet_field_count(self) -> None:
        pkt = build_fc_packet(self._sample_fc())
        assert len(pkt.strip().split(",")) == 18

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

    # TDM-DOWN-003: float 정밀도 — angle/position/velocity 전부 %.4f
    def test_fc_float_precision(self) -> None:
        s = self._sample_fc()
        pkt = build_fc_packet(s)
        # roll=0.1 → "0.1000" (%.4f) — angle과 position을 다른 정밀도로 나누던
        # 옛 포맷(%.6f/%.3f)은 실제 C(BuildFcDownlinkLine)와 달라 폐기됨
        assert "0.1000" in pkt
        assert "0.2000" in pkt
        assert "0.3000" in pkt
        # x=1.0 → "1.0000" (%.4f)
        assert "1.0000" in pkt
        assert "2.0000" in pkt
        assert "3.0000" in pkt

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

    def test_fc_pre_sats_17_field_format_rejected(self) -> None:
        # 2026-07-13 이전 포맷(17필드, ufb는 있으나 sats 없음) — C 주석대로
        # len(parts)>=18일 때만 읽으므로 이 포맷도 거부돼야 함(BL-107 회귀 케이스)
        pre_sats_format = (
            "FC,1,100,0.1000,0.2000,0.3000,1.0000,2.0000,3.0000,"
            "0.5000,0.6000,0.7000,374530000,1269850000,50000,3,0\n"
        )
        assert parse_fc_packet(pre_sats_format) is None

    def test_sh_wrong_field_count_rejected(self) -> None:
        # 구 포맷 (5필드, linkstate/ufb 없음)
        old_format = "SH,1,5000,1,2\n"
        assert parse_sh_packet(old_format) is None

    # 줄바꿈 종단
    def test_fc_ends_with_newline(self) -> None:
        assert build_fc_packet(self._sample_fc()).endswith("\n")

    def test_sh_ends_with_newline(self) -> None:
        assert build_sh_packet(ShState(1, 100, 0, 0, 1, 0)).endswith("\n")

    # TDM-DOWN-006: 파서 거부 — 빈 줄
    def test_fc_parser_rejects_empty(self) -> None:
        assert parse_fc_packet("") is None
        assert parse_fc_packet("\n") is None

    def test_sh_parser_rejects_empty(self) -> None:
        assert parse_sh_packet("") is None
        assert parse_sh_packet("\n") is None

    # TDM-DOWN-006: 파서 거부 — 잘못된 prefix
    def test_fc_parser_rejects_wrong_prefix(self) -> None:
        s = self._sample_fc()
        valid = build_fc_packet(s)
        broken = "XX" + valid[2:]
        assert parse_fc_packet(broken) is None

    def test_sh_parser_rejects_wrong_prefix(self) -> None:
        sh = ShState(seq=1, ts_ms=100, health_state=0, fault_code=0, link_state=1, ufb=0)
        valid = build_sh_packet(sh)
        broken = "XX" + valid[2:]
        assert parse_sh_packet(broken) is None

    # TDM-DOWN-006: 파서 거부 — 비숫자 필드
    def test_fc_parser_rejects_non_numeric(self) -> None:
        bad = "FC,one,two,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0,0,0,0,0,0\n"
        assert parse_fc_packet(bad) is None

    # TDM-DOWN-007: 음수 좌표 — 남반구/서경
    def test_fc_negative_coordinates(self) -> None:
        s = self._sample_fc(lat_e7=-337490000, lon_e7=-703690000, alt_mm=-500)
        pkt = build_fc_packet(s)
        parsed = parse_fc_packet(pkt)
        assert parsed is not None
        assert parsed.lat_e7 == -337490000
        assert parsed.lon_e7 == -703690000
        assert parsed.alt_mm == -500

    # TDM-DOWN-007: velocity 필드 왕복
    def test_fc_velocity_roundtrip(self) -> None:
        s = FcState(
            seq=10, ts_ms=5000,
            roll_rad=0.0, pitch_rad=0.0, yaw_rad=0.0,
            x_m=0.0, y_m=0.0, z_m=0.0,
            vx_mps=-1.5, vy_mps=2.25, vz_mps=-0.75,
            lat_e7=0, lon_e7=0, alt_mm=0, fix=0, ufb=0,
        )
        parsed = parse_fc_packet(build_fc_packet(s))
        assert parsed is not None
        assert abs(parsed.vx_mps - (-1.5)) < 1e-3
        assert abs(parsed.vy_mps - 2.25) < 1e-3
        assert abs(parsed.vz_mps - (-0.75)) < 1e-3

    # TDM-DOWN-008: ufb=2 (SEQ_FAIL) 왕복
    def test_fc_ufb_seq_fail(self) -> None:
        s = self._sample_fc(ufb=2)
        parsed = parse_fc_packet(build_fc_packet(s))
        assert parsed is not None
        assert parsed.ufb == 2

    # TDM-DOWN-008: SH link_state / ufb 범위
    def test_sh_link_state_values(self) -> None:
        for ls in (0, 1, 2):
            sh = ShState(seq=1, ts_ms=100, health_state=0, fault_code=0, link_state=ls, ufb=0)
            parsed = parse_sh_packet(build_sh_packet(sh))
            assert parsed is not None
            assert parsed.link_state == ls

    def test_sh_ufb_values(self) -> None:
        for ufb in (0, 1, 2):
            sh = ShState(seq=1, ts_ms=100, health_state=0, fault_code=0, link_state=1, ufb=ufb)
            parsed = parse_sh_packet(build_sh_packet(sh))
            assert parsed is not None
            assert parsed.ufb == ufb

    # TDM-DOWN-009: FC/SH 교대 패턴 (짝수 seq → FC, 홀수 seq → SH)
    def test_fc_sh_alternation_pattern(self) -> None:
        """C ServiceLoRa()는 DownlinkSeq%2==0이면 FC, 홀수면 SH를 전송."""
        fc_state = FcState(
            seq=0, ts_ms=0,
            roll_rad=0.0, pitch_rad=0.0, yaw_rad=0.0,
            x_m=0.0, y_m=0.0, z_m=0.0,
            vx_mps=0.0, vy_mps=0.0, vz_mps=0.0,
            lat_e7=0, lon_e7=0, alt_mm=0, fix=3, ufb=0,
        )
        sh_state = ShState(seq=1, ts_ms=0, health_state=0, fault_code=0, link_state=1, ufb=0)

        # seq 짝수 → FC 파싱 성공, SH 파싱 실패
        fc_pkt = build_fc_packet(fc_state)
        assert parse_fc_packet(fc_pkt) is not None
        assert parse_sh_packet(fc_pkt) is None

        # seq 홀수 → SH 파싱 성공, FC 파싱 실패
        sh_pkt = build_sh_packet(sh_state)
        assert parse_sh_packet(sh_pkt) is not None
        assert parse_fc_packet(sh_pkt) is None


class FcPacketCSourceCrossCheckTest(unittest.TestCase):
    """이 파일의 build_fc_packet()이 실제 C snprintf 포맷과 계속 일치하는지
    C 소스를 직접 파싱해 대조한다. BL-107(2026-07-28) 발견: 이 파일 전체가
    자기참조 구조(C를 안 보고 Python이 재구현한 스펙을 스스로 되파싱)라
    필드수(17↔18)/정밀도(%.6f,%.3f↔%.4f) drift를 아무도 못 잡았던 사례의
    재발 방지용 — C가 바뀌면 이 테스트가 먼저 죽어야 한다."""

    def _extract_snprintf_format(self) -> str:
        text = _C_SRC.read_text()
        match = re.search(
            r'LORA_TDM_APP_BuildFcDownlinkLine.*?snprintf\(Buf, BufLen,\s*"([^"]+)"',
            text, re.DOTALL,
        )
        self.assertIsNotNone(
            match, msg=f"{_C_SRC}에서 BuildFcDownlinkLine snprintf 포맷을 못 찾음"
        )
        return match.group(1)

    def test_c_format_field_count_matches_python(self) -> None:
        fmt = self._extract_snprintf_format()
        # 포맷 문자열 자체의 콤마 수 = 필드 수 - 1 (마지막 \n 앞까지)
        field_count = fmt.count(",") + 1
        self.assertEqual(
            field_count, 18,
            msg=(
                f"C snprintf 포맷 필드 수가 {field_count}로 바뀜 — "
                "build_fc_packet()/parse_fc_packet()/FcState도 같이 갱신할 것"
            ),
        )

    def test_c_format_all_floats_use_dot4f(self) -> None:
        fmt = self._extract_snprintf_format()
        # 9개 float 필드(roll/pitch/yaw/x/y/z/vx/vy/vz) 전부 %.4f여야 함 —
        # angle=%.6f, position=%.3f로 나뉘던 옛 포맷이 되돌아오면 여기서 실패.
        self.assertEqual(
            fmt.count("%.4f"), 9,
            msg=f"C 포맷의 %.4f 개수가 예상(9)과 다름: {fmt!r}",
        )
        self.assertNotIn("%.6f", fmt)
        self.assertNotIn("%.3f", fmt)


if __name__ == "__main__":
    unittest.main()
