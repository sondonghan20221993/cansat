"""
tools/mission_upload_diag.py 단위 테스트

검증 항목:
 - MAVLink X.25 CRC (_crc_acc, _compute_crc)
 - MAVLink V2 프레임 빌더 (_build_v2)
 - MISSION_CLEAR_ALL / MISSION_COUNT / MISSION_ITEM_INT / MISSION_ITEM 패킷 구조
 - _Parser 상태 머신 (V2 round-trip, V1 STX reset)
"""

import struct
import unittest

from tools.mission_upload_diag import (
    STX_V1,
    STX_V2,
    MSG_MISSION_CLEAR_ALL,
    MSG_MISSION_COUNT,
    MSG_MISSION_ITEM_INT,
    MSG_MISSION_ITEM,
    MAV_FRAME_LOCAL_NED,
    MAV_CMD_NAV_WAYPOINT,
    SRC_SYSID,
    SRC_COMPID,
    CRC_EXTRA,
    _crc_acc,
    _compute_crc,
    _build_v2,
    build_mission_clear_all,
    build_mission_count,
    build_mission_item_int,
    build_mission_item,
    _Parser,
)


class CrcTest(unittest.TestCase):
    def test_crc_acc_known_value(self):
        # MAVLink 표준 crc_accumulate(data=0, crcAccum=0xFFFF) 레퍼런스 트레이스:
        # tmp = 0 ^ 0xFF = 0xFF; tmp ^= (tmp<<4)&0xFF → 0x0F
        # crc = (0xFFFF>>8) ^ (0x0F<<8) ^ (0x0F<<3) ^ (0x0F>>4) = 0xFF^0x0F00^0x78 = 0x0F87
        self.assertEqual(_crc_acc(0x00, 0xFFFF), 0x0F87)

    def test_crc_acc_returns_16bit(self):
        for byte in range(256):
            result = _crc_acc(byte, 0xFFFF)
            self.assertLessEqual(result, 0xFFFF)
            self.assertGreaterEqual(result, 0)

    def test_compute_crc_empty_data(self):
        # 빈 데이터, crc_extra=0 → _crc_acc(0, 0xFFFF)
        result = _compute_crc(b"", 0)
        self.assertEqual(result, _crc_acc(0, 0xFFFF))

    def test_compute_crc_deterministic(self):
        data = bytes([0xFD, 0x03, 0x00, 0x00, 0x01, 0xFF, 0xBE, 0x01, 0x00, 0x00])
        r1 = _compute_crc(data, 50)
        r2 = _compute_crc(data, 50)
        self.assertEqual(r1, r2)

    def test_compute_crc_changes_with_extra(self):
        data = b"\x01\x02\x03"
        self.assertNotEqual(_compute_crc(data, 50), _compute_crc(data, 51))


class BuildV2Test(unittest.TestCase):
    def setUp(self):
        # _build_v2는 전역 _tx_seq를 증가시키므로 직접 호출 후 구조 검증
        self._frame = _build_v2(MSG_MISSION_CLEAR_ALL, b"\x01\x02\x03")

    def test_starts_with_stx_v2(self):
        self.assertEqual(self._frame[0], STX_V2)

    def test_payload_length_field(self):
        self.assertEqual(self._frame[1], 3)

    def test_sysid_and_compid(self):
        self.assertEqual(self._frame[5], SRC_SYSID)
        self.assertEqual(self._frame[6], SRC_COMPID)

    def test_msgid_in_frame(self):
        msgid = self._frame[7] | (self._frame[8] << 8) | (self._frame[9] << 16)
        self.assertEqual(msgid, MSG_MISSION_CLEAR_ALL)

    def test_payload_bytes_correct(self):
        payload_start = 10
        payload_len = self._frame[1]
        self.assertEqual(self._frame[payload_start:payload_start + payload_len], b"\x01\x02\x03")

    def test_crc_appended(self):
        # 마지막 2바이트가 CRC
        payload_len = self._frame[1]
        total = 12 + payload_len  # STX(1)+hdr(9)+payload+crc(2)
        self.assertEqual(len(self._frame), total)

    def test_crc_valid(self):
        payload_len = self._frame[1]
        hdr_payload = self._frame[1:10 + payload_len]  # hdr(9) + payload
        crc_in_frame = struct.unpack_from('<H', self._frame, -2)[0]
        expected = _compute_crc(hdr_payload, CRC_EXTRA[MSG_MISSION_CLEAR_ALL])
        self.assertEqual(crc_in_frame, expected)


class MissionClearAllTest(unittest.TestCase):
    def setUp(self):
        self._frame = build_mission_clear_all(1, 1)

    def test_msgid(self):
        msgid = self._frame[7] | (self._frame[8] << 8) | (self._frame[9] << 16)
        self.assertEqual(msgid, MSG_MISSION_CLEAR_ALL)

    def test_target_sysid(self):
        payload_start = 10
        self.assertEqual(self._frame[payload_start], 1)

    def test_target_compid(self):
        self.assertEqual(self._frame[11], 1)


class MissionCountTest(unittest.TestCase):
    def test_count_field(self):
        frame = build_mission_count(3, 1, 1)
        payload_start = 10
        count = struct.unpack_from('<H', frame, payload_start)[0]
        self.assertEqual(count, 3)

    def test_msgid(self):
        frame = build_mission_count(1, 1, 1)
        msgid = frame[7] | (frame[8] << 8) | (frame[9] << 16)
        self.assertEqual(msgid, MSG_MISSION_COUNT)

    def test_count_zero(self):
        frame = build_mission_count(0, 1, 1)
        count = struct.unpack_from('<H', frame, 10)[0]
        self.assertEqual(count, 0)


class MissionItemIntTest(unittest.TestCase):
    def _parse_payload(self, frame):
        payload_len = frame[1]
        return frame[10:10 + payload_len]

    def test_msgid(self):
        frame = build_mission_item_int(0, (1.0, -2.0, 3.0), 1, 1)
        msgid = frame[7] | (frame[8] << 8) | (frame[9] << 16)
        self.assertEqual(msgid, MSG_MISSION_ITEM_INT)

    def test_z_sign_inverted(self):
        # route Z=5.0 → LOCAL_NED down → stored as -5.0
        frame = build_mission_item_int(0, (0.0, -10.0, 5.0), 1, 1)
        payload = self._parse_payload(frame)
        z = struct.unpack_from('<f', payload, 24)[0]
        self.assertAlmostEqual(z, -5.0, places=4)

    def test_seq_field(self):
        frame = build_mission_item_int(2, (0.0, 0.0, 3.0), 1, 1)
        payload = self._parse_payload(frame)
        seq = struct.unpack_from('<H', payload, 28)[0]
        self.assertEqual(seq, 2)

    def test_command_nav_waypoint(self):
        frame = build_mission_item_int(0, (0.0, 0.0, 3.0), 1, 1)
        payload = self._parse_payload(frame)
        cmd = struct.unpack_from('<H', payload, 30)[0]
        self.assertEqual(cmd, MAV_CMD_NAV_WAYPOINT)

    def test_frame_type_local_ned(self):
        frame = build_mission_item_int(0, (0.0, 0.0, 3.0), 1, 1)
        payload = self._parse_payload(frame)
        self.assertEqual(payload[34], MAV_FRAME_LOCAL_NED)

    def test_x_stored_as_int_times_10000(self):
        frame = build_mission_item_int(0, (2.5, 0.0, 3.0), 1, 1)
        payload = self._parse_payload(frame)
        x_int = struct.unpack_from('<i', payload, 16)[0]
        self.assertEqual(x_int, 25000)


class MissionItemTest(unittest.TestCase):
    def _parse_payload(self, frame):
        payload_len = frame[1]
        return frame[10:10 + payload_len]

    def test_msgid(self):
        frame = build_mission_item(0, (0.0, 0.0, 3.0), 1, 1)
        msgid = frame[7] | (frame[8] << 8) | (frame[9] << 16)
        self.assertEqual(msgid, MSG_MISSION_ITEM)

    def test_z_sign_inverted(self):
        frame = build_mission_item(0, (0.0, -10.0, 4.0), 1, 1)
        payload = self._parse_payload(frame)
        z = struct.unpack_from('<f', payload, 24)[0]
        self.assertAlmostEqual(z, -4.0, places=4)

    def test_x_y_stored(self):
        frame = build_mission_item(0, (1.5, -3.0, 3.0), 1, 1)
        payload = self._parse_payload(frame)
        x = struct.unpack_from('<f', payload, 16)[0]
        y = struct.unpack_from('<f', payload, 20)[0]
        self.assertAlmostEqual(x, 1.5, places=4)
        self.assertAlmostEqual(y, -3.0, places=4)


class ParserRoundTripTest(unittest.TestCase):
    def _parse_frame(self, frame):
        p = _Parser()
        result = None
        for byte in frame:
            r = p.feed(byte)
            if r is not None:
                result = r
        return result

    def test_roundtrip_mission_clear_all(self):
        frame = build_mission_clear_all(1, 1)
        msg = self._parse_frame(frame)
        self.assertIsNotNone(msg)
        self.assertEqual(msg['msgid'], MSG_MISSION_CLEAR_ALL)

    def test_roundtrip_mission_count(self):
        frame = build_mission_count(3, 1, 1)
        msg = self._parse_frame(frame)
        self.assertIsNotNone(msg)
        self.assertEqual(msg['msgid'], MSG_MISSION_COUNT)
        count = struct.unpack_from('<H', msg['payload'], 0)[0]
        self.assertEqual(count, 3)

    def test_roundtrip_mission_item_int(self):
        frame = build_mission_item_int(1, (2.0, -10.0, 3.0), 1, 1)
        msg = self._parse_frame(frame)
        self.assertIsNotNone(msg)
        self.assertEqual(msg['msgid'], MSG_MISSION_ITEM_INT)
        seq = struct.unpack_from('<H', msg['payload'], 28)[0]
        self.assertEqual(seq, 1)

    def test_v1_stx_resets_parser_when_idle(self):
        # 유휴 상태(직전 프레임 미시작)에서 V1 STX(0xFE) 수신 → 새 프레임 시작
        p = _Parser()
        p.feed(STX_V2)
        p.feed(0x03)  # LEN=3
        p.feed(0x00)  # INCOMPAT
        p.feed(0x00)  # COMPAT
        p.feed(0x00)  # SEQ
        p.feed(0x00)  # SYSID
        p.feed(0x00)  # COMPID
        p.feed(0x00)  # MSGID1
        p.feed(0x00)  # MSGID2
        p.feed(0x00)  # MSGID3
        p.feed(0x01)  # PAYLOAD[0]
        p.feed(0x02)  # PAYLOAD[1]
        p.feed(0x03)  # PAYLOAD[2] → payload 완료, state=CRC1
        p.feed(0x00)  # CRC1
        p.feed(0x00)  # CRC2 → 프레임 완결, state는 다시 'STX'로 리셋됨
        p.feed(STX_V1)
        self.assertEqual(p.state, 'LEN')
        self.assertFalse(p.is_v2)

    def test_stx_byte_mid_frame_is_consumed_as_data(self):
        # 프레임 파싱 도중(state != 'STX')에는 0xFD/0xFE 값도 정상 데이터로 소비되어야 함 —
        # 페이로드에 우연히 이 값이 섞여도 프레임을 잃지 않는다 (2026-07-14 계약 변경)
        p = _Parser()
        p.feed(STX_V2)
        p.feed(0x01)  # LEN=1
        p.feed(0x00)  # INCOMPAT
        p.feed(0x00)  # COMPAT
        p.feed(0x00)  # SEQ
        p.feed(0x00)  # SYSID
        p.feed(0x00)  # COMPID
        p.feed(0x00)  # MSGID1
        p.feed(0x00)  # MSGID2
        p.feed(0x00)  # MSGID3
        p.feed(0xFE)  # PAYLOAD[0] == STX_V1 값이지만 데이터로 소비되어야 함
        self.assertEqual(p.state, 'CRC1')
        self.assertEqual(bytes(p.payload), bytes([0xFE]))

    def test_partial_frame_returns_none(self):
        frame = build_mission_clear_all(1, 1)
        p = _Parser()
        for byte in frame[:-1]:  # CRC 마지막 1바이트 빼고
            r = p.feed(byte)
            self.assertIsNone(r)

    def test_sysid_compid_in_parsed(self):
        frame = build_mission_count(2, 5, 10)
        msg = self._parse_frame(frame)
        self.assertEqual(msg['sysid'], SRC_SYSID)
        self.assertEqual(msg['compid'], SRC_COMPID)


if __name__ == "__main__":
    unittest.main()
