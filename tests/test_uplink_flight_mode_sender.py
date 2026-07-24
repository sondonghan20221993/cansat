import struct
import unittest

from tools.uplink_flight_mode_sender import (
    UPLINK_CLASS_FLIGHT_MODE,
    UPLINK_PROTOCOL_VERSION,
    MODE_NAMES,
    build_flight_mode_payload,
    build_process_uplink_payload,
    build_command_packet,
    crc16_ccitt,
)


class BuildFlightModePayloadTest(unittest.TestCase):
    def test_layout_hover(self):
        payload = build_flight_mode_payload(MODE_NAMES["hover"], 0, 0x11223344)
        self.assertEqual(len(payload), 6)
        mode, wp_index, token = struct.unpack("<BBI", payload)
        self.assertEqual(mode, 0)
        self.assertEqual(wp_index, 0)
        self.assertEqual(token, 0x11223344)

    def test_layout_waypoint_with_index(self):
        payload = build_flight_mode_payload(MODE_NAMES["waypoint"], 7, 1)
        mode, wp_index, token = struct.unpack("<BBI", payload)
        self.assertEqual(mode, 1)
        self.assertEqual(wp_index, 7)
        self.assertEqual(token, 1)

    def test_layout_land(self):
        payload = build_flight_mode_payload(MODE_NAMES["land"], 0, 99)
        mode, _wp_index, _token = struct.unpack("<BBI", payload)
        self.assertEqual(mode, 2)


class BuildProcessUplinkPayloadTest(unittest.TestCase):
    def test_class_is_flight_mode(self):
        fm_payload = build_flight_mode_payload(MODE_NAMES["hover"], 0, 1)
        uplink = build_process_uplink_payload(sequence=1, fm_payload=fm_payload, flags=0)
        # layout: version(1)+class(1)+len(1)+flags(1)+seq(2)+checksum(2)+payload(196)
        self.assertEqual(len(uplink), 8 + 196)
        version, command_class, payload_len, flags = struct.unpack_from("<BBBB", uplink)
        self.assertEqual(version, UPLINK_PROTOCOL_VERSION)
        self.assertEqual(command_class, UPLINK_CLASS_FLIGHT_MODE)
        self.assertEqual(payload_len, len(fm_payload))
        self.assertEqual(flags, 0)

    def test_auth_level_encoded_in_flags(self):
        fm_payload = build_flight_mode_payload(MODE_NAMES["hover"], 0, 1)
        uplink = build_process_uplink_payload(sequence=1, fm_payload=fm_payload, flags=(3 << 6))
        _version, _command_class, _payload_len, flags = struct.unpack_from("<BBBB", uplink)
        self.assertEqual((flags >> 6) & 0x3, 3)

    def test_sequence_stored(self):
        fm_payload = build_flight_mode_payload(MODE_NAMES["land"], 0, 2)
        uplink = build_process_uplink_payload(sequence=42, fm_payload=fm_payload, flags=0)
        seq = struct.unpack_from("<H", uplink, 4)[0]
        self.assertEqual(seq, 42)

    def test_checksum_is_correct_crc16(self):
        fm_payload = build_flight_mode_payload(MODE_NAMES["waypoint"], 3, 5)
        uplink = build_process_uplink_payload(sequence=7, fm_payload=fm_payload, flags=(3 << 6))
        checksum_in_packet = struct.unpack_from("<H", uplink, 6)[0]
        crc_input = struct.pack("<BBBBH", UPLINK_PROTOCOL_VERSION, UPLINK_CLASS_FLIGHT_MODE,
                                len(fm_payload), (3 << 6), 7) + fm_payload
        self.assertEqual(checksum_in_packet, crc16_ccitt(crc_input))
        self.assertNotEqual(checksum_in_packet, 0)

    def test_payload_embedded_in_fixed_area(self):
        fm_payload = build_flight_mode_payload(MODE_NAMES["hover"], 0, 9)
        uplink = build_process_uplink_payload(sequence=1, fm_payload=fm_payload, flags=0)
        embedded = uplink[8: 8 + len(fm_payload)]
        self.assertEqual(embedded, fm_payload)


class BuildCommandPacketTest(unittest.TestCase):
    def test_ccsds_header_and_checksum(self):
        fm_payload = build_flight_mode_payload(MODE_NAMES["hover"], 0, 1)
        uplink = build_process_uplink_payload(sequence=1, fm_payload=fm_payload, flags=0)
        packet = build_command_packet(2, uplink)
        # CCSDS primary(6) + secondary(2) + payload
        self.assertEqual(len(packet), 6 + 2 + len(uplink))
        mid = struct.unpack_from(">H", packet, 0)[0]
        self.assertEqual(mid, 0x18D0)
        function_code = packet[6]
        self.assertEqual(function_code, 2)


if __name__ == "__main__":
    unittest.main()
