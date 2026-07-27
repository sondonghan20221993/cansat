import struct
import sys
import unittest
from unittest import mock

from tools.uplink_config_sender import (
    SCOPE_CFS_CORE_APP,
    SCOPE_MAVLINK_BRIDGE,
    CONFIG_VERSION,
    VALUE_TYPE_UINT32,
    UPLINK_CLASS_CONFIG,
    UPLINK_PROTOCOL_VERSION,
    build_config_payload,
    build_process_uplink_payload,
    build_lora_frame,
    config_checksum,
    crc16_ccitt,
    main,
)
import tools.uplink_config_sender as config_sender_module


class ConfigChecksumTest(unittest.TestCase):
    def test_matches_c_algorithm(self):
        # scope=1, version=1, param_id=5(PUBLISH_PERIOD_MS), value=1000
        value_bytes = struct.pack("<I", 1000)  # 0xE8 0x03 0x00 0x00
        cs = config_checksum(1, 1, 5, VALUE_TYPE_UINT32, 4, value_bytes)
        # manual: 1+1+5+0+0+4+0xE8+0x03+0x00+0x00 = 246
        self.assertEqual(cs, 246)

    def test_param_id_split_into_two_bytes(self):
        # param_id=0x0102: lo=0x02, hi=0x01
        cs = config_checksum(1, 1, 0x0102, 0, 0, b"")
        self.assertEqual(cs, 1 + 1 + 0x02 + 0x01)

    def test_scope2(self):
        value_bytes = struct.pack("<I", 50000)
        cs = config_checksum(SCOPE_MAVLINK_BRIDGE, CONFIG_VERSION, 0, VALUE_TYPE_UINT32, 4, value_bytes)
        expected = 0
        expected += SCOPE_MAVLINK_BRIDGE
        expected += CONFIG_VERSION
        expected += 0  # param_id lo
        expected += 0  # param_id hi
        expected += VALUE_TYPE_UINT32
        expected += 4
        for b in value_bytes:
            expected += b
        self.assertEqual(cs, expected & 0xFFFF)


class BuildConfigPayloadTest(unittest.TestCase):
    def _parse_hdr(self, payload: bytes):
        scope, version, param_id, value_type, value_length, checksum = struct.unpack_from("<BBHBBH", payload)
        value = struct.unpack_from("<I", payload, 8)[0]
        return scope, version, param_id, value_type, value_length, checksum, value

    def test_cfs_core_publish_period(self):
        payload = build_config_payload(SCOPE_CFS_CORE_APP, 5, 1000)
        self.assertEqual(len(payload), 12)  # 8 byte hdr + 4 byte value
        scope, version, param_id, value_type, value_length, checksum, value = self._parse_hdr(payload)
        self.assertEqual(scope, SCOPE_CFS_CORE_APP)
        self.assertEqual(version, CONFIG_VERSION)
        self.assertEqual(param_id, 5)
        self.assertEqual(value_type, VALUE_TYPE_UINT32)
        self.assertEqual(value_length, 4)
        self.assertEqual(value, 1000)
        # verify checksum
        value_bytes = struct.pack("<I", 1000)
        expected_cs = config_checksum(SCOPE_CFS_CORE_APP, CONFIG_VERSION, 5, VALUE_TYPE_UINT32, 4, value_bytes)
        self.assertEqual(checksum, expected_cs)

    def test_mavlink_bridge_attitude_interval(self):
        payload = build_config_payload(SCOPE_MAVLINK_BRIDGE, 0, 50000)
        scope, version, param_id, _, _, checksum, value = self._parse_hdr(payload)
        self.assertEqual(scope, SCOPE_MAVLINK_BRIDGE)
        self.assertEqual(param_id, 0)
        self.assertEqual(value, 50000)
        value_bytes = struct.pack("<I", 50000)
        self.assertEqual(checksum,
                         config_checksum(SCOPE_MAVLINK_BRIDGE, CONFIG_VERSION, 0, VALUE_TYPE_UINT32, 4, value_bytes))


class BuildProcessUplinkPayloadTest(unittest.TestCase):
    def test_class_is_config(self):
        config_payload = build_config_payload(SCOPE_CFS_CORE_APP, 5, 1000)
        uplink = build_process_uplink_payload(sequence=1, config_payload=config_payload)
        # uplink layout: version(1)+class(1)+len(1)+flags(1)+seq(2)+checksum(2)+payload(196)
        self.assertEqual(len(uplink), 8 + 196)
        version, command_class, payload_len, flags = struct.unpack_from("<BBBB", uplink)
        self.assertEqual(version, UPLINK_PROTOCOL_VERSION)
        self.assertEqual(command_class, UPLINK_CLASS_CONFIG)
        self.assertEqual(payload_len, len(config_payload))
        self.assertEqual(flags, 0)

    def test_sequence_stored(self):
        config_payload = build_config_payload(SCOPE_CFS_CORE_APP, 0, 2000)
        uplink = build_process_uplink_payload(sequence=42, config_payload=config_payload)
        seq = struct.unpack_from("<H", uplink, 4)[0]
        self.assertEqual(seq, 42)

    def test_checksum_is_correct_crc16(self):
        config_payload = build_config_payload(SCOPE_CFS_CORE_APP, 0, 3000)
        uplink = build_process_uplink_payload(sequence=7, config_payload=config_payload)
        checksum_in_packet = struct.unpack_from("<H", uplink, 6)[0]
        crc_input = struct.pack("<BBBBH", UPLINK_PROTOCOL_VERSION, UPLINK_CLASS_CONFIG,
                                len(config_payload), 0, 7) + config_payload
        self.assertEqual(checksum_in_packet, crc16_ccitt(crc_input))
        self.assertNotEqual(checksum_in_packet, 0)

    def test_payload_embedded_in_fixed_area(self):
        config_payload = build_config_payload(SCOPE_CFS_CORE_APP, 0, 500)
        uplink = build_process_uplink_payload(sequence=1, config_payload=config_payload)
        embedded = uplink[8: 8 + len(config_payload)]
        self.assertEqual(embedded, config_payload)


class BuildLoraFrameTest(unittest.TestCase):
    def test_frame_format(self):
        config_payload = build_config_payload(SCOPE_CFS_CORE_APP, 5, 1000)
        frame = build_lora_frame(sequence=3, config_payload=config_payload)
        parts = frame.split(",")
        self.assertEqual(len(parts), 7)
        self.assertEqual(parts[0], "UP")
        self.assertEqual(int(parts[1]), UPLINK_PROTOCOL_VERSION)
        self.assertEqual(int(parts[2]), UPLINK_CLASS_CONFIG)
        self.assertEqual(int(parts[3]), 3)  # sequence
        self.assertEqual(int(parts[4]), 0)  # flags

    def test_crc_is_valid(self):
        config_payload = build_config_payload(SCOPE_CFS_CORE_APP, 5, 1000)
        frame = build_lora_frame(sequence=3, config_payload=config_payload)
        parts = frame.split(",")
        claimed_crc = int(parts[-1], 16)
        canonical = ",".join(parts[:-1])
        actual_crc = crc16_ccitt(canonical.encode("ascii"))
        self.assertEqual(claimed_crc, actual_crc)

    def test_payload_hex_matches(self):
        config_payload = build_config_payload(SCOPE_CFS_CORE_APP, 5, 1000)
        frame = build_lora_frame(sequence=3, config_payload=config_payload)
        parts = frame.split(",")
        payload_hex = parts[5]
        self.assertEqual(bytes.fromhex(payload_hex), config_payload)


class MainLoraSerialFlagsTest(unittest.TestCase):
    """BL-63: lora-serial 분기가 --auth/--force로 계산한 flags를 실제로
    전송 프레임에 반영하는지 — main() CLI 진입점 레벨에서 회귀 방지."""

    def _make_mock_serial_module(self):
        mock_port = mock.MagicMock()
        mock_serial_cls = mock.MagicMock(return_value=mock.MagicMock(
            __enter__=mock.Mock(return_value=mock_port),
            __exit__=mock.Mock(return_value=False),
        ))
        mock_serial_module = mock.MagicMock()
        mock_serial_module.Serial = mock_serial_cls
        return mock_serial_module, mock_port

    def test_lora_serial_sends_auth_and_force_in_flags(self):
        mock_serial_module, mock_port = self._make_mock_serial_module()
        argv = [
            "uplink_config_sender.py", "cfs_core", "publish_period_ms", "1000",
            "--transport", "lora-serial", "--serial-path", "/dev/ttyUSB0",
            "--auth", "2", "--force",
        ]
        with mock.patch.object(sys, "argv", argv), \
             mock.patch.object(config_sender_module, "serial", mock_serial_module), \
             mock.patch("tools.uplink_config_sender.time.sleep"):
            main()

        written = mock_port.write.call_args[0][0]
        frame = written.decode("ascii").rstrip("\n")
        flags = int(frame.split(",")[4])
        self.assertEqual(flags, (2 << 6) | 1)  # auth=2, force=1

    def test_lora_serial_zero_auth_no_force(self):
        mock_serial_module, mock_port = self._make_mock_serial_module()
        argv = [
            "uplink_config_sender.py", "cfs_core", "publish_period_ms", "1000",
            "--transport", "lora-serial", "--serial-path", "/dev/ttyUSB0",
            "--auth", "0",
        ]
        with mock.patch.object(sys, "argv", argv), \
             mock.patch.object(config_sender_module, "serial", mock_serial_module), \
             mock.patch("tools.uplink_config_sender.time.sleep"):
            main()

        written = mock_port.write.call_args[0][0]
        frame = written.decode("ascii").rstrip("\n")
        flags = int(frame.split(",")[4])
        self.assertEqual(flags, 0)


if __name__ == "__main__":
    unittest.main()
