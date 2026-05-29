"""LORA-UP-001~013, CFS-CMD-001~008, REC-006~008: UP 프레임 파싱 통합테스트

bridge/lora_uplink_bridge.py의 parse_frame_line(), build_process_uplink_payload() 검증.
기존 test_lora_uplink_bridge.py에서 이미 커버된 항목은 제외한다:
  - LORA-UP-001 (valid frame): test_parse_frame_line_accepts_valid_frame
  - LORA-UP-002 (CRC mismatch): test_parse_frame_line_rejects_crc_mismatch
  - LORA-UP-010 (oversize): test_parse_frame_line_rejects_oversize_payload
  - LORA-UP-012/013/014 (seq regression): test_process_line_rejects_sequence_regression
"""

import socket
import struct
import unittest
from unittest import mock

from bridge.lora_uplink_bridge import (
    ParsedUplinkFrame,
    build_process_uplink_payload,
    crc16_ccitt,
    parse_frame_line,
)


def _build_frame(
    payload_hex: str = "",
    version: int = 1,
    command_class: int = 2,
    sequence: int = 1,
    flags: int = 0,
    crc_override: int | None = None,
) -> str:
    canonical = f"UP,{version},{command_class},{sequence},{flags},{payload_hex.upper()}"
    crc = crc_override if crc_override is not None else crc16_ccitt(canonical.encode("ascii"))
    return f"{canonical},{crc:04X}"


MAX_PAYLOAD = 196


class UpFrameFieldsTest(unittest.TestCase):

    # LORA-UP-003: version 불일치 — parse는 통과, payload 빌드 시 거부
    def test_version_mismatch_rejected_at_payload(self) -> None:
        frame = parse_frame_line(_build_frame(version=2))
        assert frame.version == 2
        with self.assertRaises(ValueError):
            build_process_uplink_payload(frame)

    # LORA-UP-004: command_class 범위 초과 (256) 거부
    def test_command_class_out_of_range(self) -> None:
        with self.assertRaises(ValueError):
            parse_frame_line(_build_frame(command_class=256))

    # LORA-UP-005: sequence 범위 초과 (65536) 거부
    def test_sequence_out_of_range(self) -> None:
        with self.assertRaises(ValueError):
            parse_frame_line(_build_frame(sequence=65536))

    # LORA-UP-006: flags 범위 초과 (256) 거부
    def test_flags_out_of_range(self) -> None:
        with self.assertRaises(ValueError):
            parse_frame_line(_build_frame(flags=256))

    # LORA-UP-007: payload hex 홀수 길이 거부
    def test_odd_payload_hex_rejected(self) -> None:
        with self.assertRaises(ValueError):
            parse_frame_line(_build_frame(payload_hex="ABC"))

    # LORA-UP-008: payload hex 비정상 문자 거부
    def test_invalid_payload_hex_chars_rejected(self) -> None:
        with self.assertRaises(ValueError):
            parse_frame_line(_build_frame(payload_hex="ZZ"))

    # LORA-UP-009: payload 196 byte 최대 허용
    def test_max_payload_accepted(self) -> None:
        payload_hex = "AA" * MAX_PAYLOAD
        frame = parse_frame_line(_build_frame(payload_hex=payload_hex))
        assert len(frame.payload) == MAX_PAYLOAD

    # LORA-UP-011: seq 증가 허용 (parse 수준 — seq 자체는 파싱됨)
    def test_sequence_parsed_correctly(self) -> None:
        for seq in [1, 2, 100, 65535]:
            frame = parse_frame_line(_build_frame(sequence=seq))
            assert frame.sequence == seq

    # REC-006: malformed frame 반복 — ValueError로 일관되게 거부
    def test_malformed_frames_consistently_rejected(self) -> None:
        bad_frames = [
            "NOTAFRAME",
            "UP",
            "UP,1",
            "UP,1,2,3",
            ",,,,,",
            "",
        ]
        for frame in bad_frames:
            with self.assertRaises((ValueError, Exception)):
                parse_frame_line(frame)

    # REC-007: payload 초과 frame 반복 — 일관되게 거부
    def test_oversized_payload_consistently_rejected(self) -> None:
        for size in [197, 200, 255]:
            payload_hex = "AA" * size
            with self.assertRaises(ValueError):
                parse_frame_line(_build_frame(payload_hex=payload_hex))


class CfsCmdPacketTest(unittest.TestCase):
    """CFS-CMD-001~008: build_process_uplink_payload() 필드 검증"""

    def setUp(self) -> None:
        self.frame = ParsedUplinkFrame(
            version=1,
            command_class=2,
            sequence=7,
            flags=3,
            payload=bytes([0xDE, 0xAD, 0xBE, 0xEF]),
        )
        self.raw = build_process_uplink_payload(self.frame)

    # CFS-CMD-004: PayloadLength == 4
    def test_payload_length_stored(self) -> None:
        # layout: version(1) + class(1) + payload_len(1) + flags(1) + seq(2) + reserved(2) + payload(196)
        payload_len = self.raw[2]
        assert payload_len == 4

    # CFS-CMD-006: CommandClass
    def test_command_class_stored(self) -> None:
        assert self.raw[1] == 2

    # CFS-CMD-007: Sequence
    def test_sequence_stored(self) -> None:
        seq = struct.unpack_from("<H", self.raw, 4)[0]
        assert seq == 7

    # CFS-CMD-008: Flags
    def test_flags_stored(self) -> None:
        assert self.raw[3] == 3

    # CFS-CMD-003: payload bytes 정확히 저장
    def test_payload_bytes_correct(self) -> None:
        payload_start = 8
        assert self.raw[payload_start:payload_start + 4] == bytes([0xDE, 0xAD, 0xBE, 0xEF])

    # CFS-CMD-005: 남은 영역 0 padding
    def test_payload_padding_zero(self) -> None:
        payload_start = 8
        padded = self.raw[payload_start + 4: payload_start + MAX_PAYLOAD]
        assert all(b == 0 for b in padded)

    # CFS-CMD-001: 전체 패킷 길이 고정
    def test_total_length_fixed(self) -> None:
        assert len(self.raw) == 8 + MAX_PAYLOAD

    # CFS-CMD-002: version 1 저장
    def test_version_stored(self) -> None:
        assert self.raw[0] == 1

    # REC-008: seq regression 반복 — parse는 통과, Bridge가 거부
    def test_seq_regression_rejected_by_bridge(self) -> None:
        from bridge.lora_uplink_bridge import Bridge
        import socket

        mock_sock = mock.MagicMock(spec=socket.socket)
        bridge = Bridge(
            serial_path="-",
            baudrate=57600,
            udp_host="127.0.0.1",
            udp_port=19999,
            serial_timeout_ms=100,
            uplink_app_cmd_mid=0x18D0,
            process_uplink_cc=2,
            allow_seq_regression=False,
        )
        bridge.sock = mock_sock

        frame_a = _build_frame(sequence=5).encode()
        frame_b = _build_frame(sequence=3).encode()

        bridge.process_line(frame_a)
        count_a = mock_sock.sendto.call_count

        bridge.process_line(frame_b)
        count_b = mock_sock.sendto.call_count

        # seq regression → sendto 추가 호출 없음
        assert count_b == count_a


if __name__ == "__main__":
    unittest.main()
