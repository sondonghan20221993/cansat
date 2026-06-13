"""
tools/query_fc_mission.py, tools/telemetry_app_e2e_sender.py 단위 테스트

검증 항목:
 - build_cfs_command: CFS 커맨드 패킷 구조 및 XOR 체크섬
 - ccsds_primary: CCSDS 주 헤더 빌드
 - send_monitor / send_hk 패킷 구조
"""

import struct
import unittest
from unittest.mock import patch, call

from tools.query_fc_mission import build_cfs_command, MAVLINK_BRIDGE_CMD_MID, MISSION_QUERY_CC
from tools.telemetry_app_e2e_sender import ccsds_primary, MONITOR_MID, SEND_HK_MID


class BuildCfsCommandTest(unittest.TestCase):
    def setUp(self):
        self._pkt = build_cfs_command(MAVLINK_BRIDGE_CMD_MID, MISSION_QUERY_CC)

    def test_total_length(self):
        self.assertEqual(len(self._pkt), 8)

    def test_stream_id(self):
        stream_id = struct.unpack_from('>H', self._pkt, 0)[0]
        self.assertEqual(stream_id, MAVLINK_BRIDGE_CMD_MID)

    def test_sequence_field(self):
        seq = struct.unpack_from('>H', self._pkt, 2)[0]
        self.assertEqual(seq, 0xC000)

    def test_data_length_field(self):
        data_len = struct.unpack_from('>H', self._pkt, 4)[0]
        self.assertEqual(data_len, 1)  # 2 byte 세컨더리 헤더 - 1

    def test_function_code(self):
        self.assertEqual(self._pkt[6], MISSION_QUERY_CC)

    def test_checksum_valid(self):
        # XOR 체크섬: 모든 바이트 XOR 결과 = 0xFF
        xor = 0
        for b in self._pkt:
            xor ^= b
        self.assertEqual(xor, 0xFF)

    def test_different_mid_and_fc(self):
        pkt = build_cfs_command(0x18C0, 3)
        xor = 0
        for b in pkt:
            xor ^= b
        self.assertEqual(xor, 0xFF)


class CcsdsPrimaryTest(unittest.TestCase):
    def test_length(self):
        hdr = ccsds_primary(MONITOR_MID, 24)
        self.assertEqual(len(hdr), 6)

    def test_stream_id(self):
        hdr = ccsds_primary(MONITOR_MID, 24)
        mid = struct.unpack_from('>H', hdr, 0)[0]
        self.assertEqual(mid, MONITOR_MID)

    def test_sequence_field(self):
        hdr = ccsds_primary(MONITOR_MID, 24)
        seq = struct.unpack_from('>H', hdr, 2)[0]
        self.assertEqual(seq, 0xC000)

    def test_data_length_field(self):
        # size=24 → data_len = 24 - 7 = 17
        hdr = ccsds_primary(SEND_HK_MID, 24)
        data_len = struct.unpack_from('>H', hdr, 4)[0]
        self.assertEqual(data_len, 17)

    def test_hk_mid_packet(self):
        hdr = ccsds_primary(SEND_HK_MID, 8)
        mid = struct.unpack_from('>H', hdr, 0)[0]
        self.assertEqual(mid, SEND_HK_MID)
        data_len = struct.unpack_from('>H', hdr, 4)[0]
        self.assertEqual(data_len, 1)


if __name__ == "__main__":
    unittest.main()
