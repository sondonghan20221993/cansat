"""LORA-HB-001~010, REC-005: HB 프레임 파싱 통합테스트

bridge/lora_telemetry_bridge.py의 parse_heartbeat_line() 로직 검증.
C의 ParseHb() static 함수와 동등한 로직을 Python에서 검증한다.
"""

import unittest

from legacy.bridge.lora_telemetry_bridge import parse_canonical_frame, parse_heartbeat_line, parse_manual_frame


def _crc16(data: str) -> int:
    crc = 0xFFFF
    for b in data.encode("utf-8"):
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def _canonical_frame(node: int, seq: int, tx_ms: int, sensor_ok: int) -> str:
    canonical = f"HB,{node},{seq},{tx_ms},{sensor_ok}"
    crc = _crc16(canonical)
    return f"{canonical},{crc:04X}"


class HbParseTest(unittest.TestCase):

    # LORA-HB-001: 단순 "HB" 허용
    def test_hb_simple(self) -> None:
        assert parse_heartbeat_line("HB") is not None
        assert parse_heartbeat_line("HELLO") is not None

    # LORA-HB-001 variant: "HB,<seq>" 단순 형식 허용
    def test_hb_short_with_seq(self) -> None:
        assert parse_heartbeat_line("HB,1") is not None
        assert parse_heartbeat_line("HB, 42") is not None

    # LORA-HB-002: canonical HB 정상 수신 (sensor_ok=1)
    def test_canonical_valid(self) -> None:
        frame = _canonical_frame(node=1, seq=1, tx_ms=1000, sensor_ok=1)
        result = parse_heartbeat_line(frame)
        assert result is not None
        assert result.seq == 1
        assert result.tx_time_ms == 1000

    # LORA-HB-003: CRC 불일치 거부
    def test_canonical_crc_mismatch(self) -> None:
        frame = _canonical_frame(node=1, seq=2, tx_ms=1000, sensor_ok=1)
        corrupted = frame[:-4] + "0000"
        assert parse_canonical_frame(corrupted) is None

    # LORA-HB-004: sensor_ok=0 거부
    def test_canonical_sensor_not_ok(self) -> None:
        frame = _canonical_frame(node=1, seq=3, tx_ms=1000, sensor_ok=0)
        assert parse_canonical_frame(frame) is None

    # LORA-HB-005: seq 증가 허용
    def test_seq_increase_accepted(self) -> None:
        for seq in [1, 2, 3, 100]:
            frame = _canonical_frame(node=1, seq=seq, tx_ms=1000, sensor_ok=1)
            assert parse_heartbeat_line(frame) is not None

    # LORA-HB-006: seq 역행 — Bridge 수준에서 검증 (parse_heartbeat_line 자체는 seq 체크 안 함)
    def test_seq_regression_handled_by_bridge(self) -> None:
        from legacy.bridge.lora_telemetry_bridge import Bridge
        bridge = Bridge(
            serial_path="/dev/null",
            baudrate=57600,
            monitor_mid=0x0847,
            active_transport_id=1,
            monitor_period_ms=500,
            udp_host="127.0.0.1",
            udp_port=19999,
            serial_timeout_ms=100,
        )
        frame_a = _canonical_frame(node=1, seq=5, tx_ms=1000, sensor_ok=1)
        frame_b = _canonical_frame(node=1, seq=3, tx_ms=2000, sensor_ok=1)

        bridge.process_line(frame_a.encode())
        ts_after_a = bridge.last_valid_rx_time_monotonic

        bridge.process_line(frame_b.encode())
        ts_after_b = bridge.last_valid_rx_time_monotonic

        # seq 역행이면 accept 안 해야 함
        assert ts_after_b == ts_after_a

    # LORA-HB-007: 빈 줄 무시
    def test_empty_line_rejected(self) -> None:
        assert parse_heartbeat_line("") is None
        assert parse_heartbeat_line("   ") is None

    # LORA-HB-008: 잘못된 prefix 거부
    def test_bad_prefix_rejected(self) -> None:
        assert parse_heartbeat_line("XX,1,2,1000,1,ABCD") is None
        assert parse_heartbeat_line("FC,1,2,3") is None
        assert parse_heartbeat_line("UP,1,2,3,4,AA,1234") is None

    # LORA-HB-009: 필드 개수 부족 (canonical 형식에서)
    def test_insufficient_fields(self) -> None:
        # canonical은 6 필드 필요: HB,node,seq,tx_ms,sensor_ok,crc
        # 5 필드만 있으면 canonical parse 실패 → manual로 fallback
        result = parse_canonical_frame("HB,1,2,1000")
        assert result is None

    # LORA-HB-010: 숫자 필드 오류
    def test_non_numeric_fields(self) -> None:
        # canonical에서 숫자가 아닌 필드
        assert parse_canonical_frame("HB,node,abc,1000,1,FFFF") is None

    # REC-005: HB timeout — last_valid_rx_time_monotonic 없을 때 valid=0
    def test_hb_timeout_valid_zero(self) -> None:
        from legacy.bridge.lora_telemetry_bridge import Bridge
        bridge = Bridge(
            serial_path="/dev/null",
            baudrate=57600,
            monitor_mid=0x0847,
            active_transport_id=1,
            monitor_period_ms=500,
            udp_host="127.0.0.1",
            udp_port=19999,
            serial_timeout_ms=100,
        )
        # HB 한 번도 수신 안 한 상태 → valid=0
        assert bridge.last_valid_rx_time_monotonic is None


if __name__ == "__main__":
    unittest.main()
