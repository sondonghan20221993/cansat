import socket
import unittest
from unittest import mock

from legacy.bridge.lora_uplink_bridge import (
    Bridge,
    ParsedUplinkFrame,
    build_process_uplink_payload,
    crc16_ccitt,
    parse_frame_line,
)


def build_frame_line(
    payload_hex: str,
    *,
    version: int = 1,
    command_class: int = 2,
    sequence: int = 7,
    flags: int = 0,
) -> str:
    canonical_without_crc = f"UP,{version},{command_class},{sequence},{flags},{payload_hex.upper()}"
    crc = crc16_ccitt(canonical_without_crc.encode("ascii"))
    return f"{canonical_without_crc},{crc:04X}"


class LoraUplinkBridgeParsingTest(unittest.TestCase):
    def test_parse_frame_line_accepts_valid_frame(self) -> None:
        frame = parse_frame_line(
            build_frame_line("0101020000000000000020C1000040400000A040000040C100008040", sequence=8)
        )

        self.assertEqual(frame.version, 1)
        self.assertEqual(frame.command_class, 2)
        self.assertEqual(frame.sequence, 8)
        self.assertEqual(frame.flags, 0)
        self.assertEqual(len(frame.payload), 28)

    def test_parse_frame_line_rejects_crc_mismatch(self) -> None:
        with self.assertRaisesRegex(ValueError, "crc mismatch"):
            parse_frame_line(
                "UP,1,2,8,0,0101020000000000000020C1000040400000A040000040C100008040,0000"
            )

    def test_parse_frame_line_rejects_invalid_format(self) -> None:
        with self.assertRaisesRegex(ValueError, "expected frame format"):
            parse_frame_line("python3 tools/uplink_route_update_sender.py route-good")

    def test_parse_frame_line_rejects_oversize_payload(self) -> None:
        payload_hex = ("AA" * 197).upper()
        frame_line = build_frame_line(payload_hex, sequence=9)

        with self.assertRaisesRegex(ValueError, "payload too large"):
            parse_frame_line(frame_line)

    def test_build_process_uplink_payload_rejects_wrong_version(self) -> None:
        with self.assertRaisesRegex(ValueError, "unsupported protocol version"):
            build_process_uplink_payload(
                ParsedUplinkFrame(version=2, command_class=2, sequence=1, flags=0, payload=b"\x01")
            )

    def test_build_process_uplink_payload_sets_checksum(self) -> None:
        import struct
        frame = ParsedUplinkFrame(version=1, command_class=2, sequence=7, flags=0, payload=b"\xAA\xBB")
        result = build_process_uplink_payload(frame)

        # fixed header is 8 bytes: version(1)+class(1)+len(1)+flags(1)+seq(2)+checksum(2)
        checksum_in_packet = struct.unpack_from("<H", result, 6)[0]
        crc_input = struct.pack("<BBBBH", 1, 2, 2, 0, 7) + b"\xAA\xBB"
        expected = crc16_ccitt(crc_input)
        self.assertEqual(checksum_in_packet, expected)
        self.assertNotEqual(checksum_in_packet, 0)


class LoraUplinkBridgeBehaviorTest(unittest.TestCase):
    def setUp(self) -> None:
        self.bridge = Bridge(
            serial_path="-",
            baudrate=57600,
            udp_host="127.0.0.1",
            udp_port=1234,
            serial_timeout_ms=100,
            uplink_app_cmd_mid=0x18D0,
            process_uplink_cc=2,
            allow_seq_regression=False,
        )
        self.bridge.sock.close()
        self.bridge.sock = mock.Mock(spec=socket.socket)

    def test_process_line_forwards_valid_frame_once(self) -> None:
        line = build_frame_line("0101020000000000000020C1000040400000A040000040C100008040", sequence=10)

        self.bridge.process_line(line.encode("ascii") + b"\n")

        self.assertEqual(self.bridge.accepted_count, 1)
        self.assertEqual(self.bridge.rejected_count, 0)
        self.bridge.sock.sendto.assert_called_once()
        packet, address = self.bridge.sock.sendto.call_args.args
        self.assertEqual(address, ("127.0.0.1", 1234))
        self.assertEqual(packet[0:2], b"\x18\xd0")

    def test_process_line_rejects_sequence_regression(self) -> None:
        line = build_frame_line("0101020000000000000020C1000040400000A040000040C100008040", sequence=11)

        self.bridge.process_line(line.encode("ascii") + b"\n")
        self.bridge.process_line(line.encode("ascii") + b"\n")

        self.assertEqual(self.bridge.accepted_count, 1)
        self.assertEqual(self.bridge.rejected_count, 1)
        self.bridge.sock.sendto.assert_called_once()

    def test_process_line_allows_sequence_regression_when_disabled(self) -> None:
        self.bridge.allow_seq_regression = True
        line = build_frame_line("0101020000000000000020C1000040400000A040000040C100008040", sequence=12)

        self.bridge.process_line(line.encode("ascii") + b"\n")
        self.bridge.process_line(line.encode("ascii") + b"\n")

        self.assertEqual(self.bridge.accepted_count, 2)
        self.assertEqual(self.bridge.rejected_count, 0)
        self.assertEqual(self.bridge.sock.sendto.call_count, 2)

    def test_process_line_rejects_non_frame_text(self) -> None:
        self.bridge.process_line(b"HELLO\n")

        self.assertEqual(self.bridge.accepted_count, 0)
        self.assertEqual(self.bridge.rejected_count, 1)
        self.bridge.sock.sendto.assert_not_called()


if __name__ == "__main__":
    unittest.main()
