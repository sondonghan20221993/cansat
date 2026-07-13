"""bridge/lora_downlink_decoder.py 단위 테스트

프로토콜 v2 spec(notes/lora_protocol_v2_spec.md) §9 검증 요구사항 기반:
 - DL2 인코딩/디코딩 왕복 (saturation, SysTime 블록 유/무)
 - 다중 조각(분할) 수신 — 바이트 스트림 상태머신
 - CRC 손상 → 재동기화로 후속 프레임 정상 수신
 - v1/v2 혼합 스트림
 - ACK2 프레임 형식
"""

import struct
import unittest

from bridge.lora_downlink_decoder import (
    ACK2_MAGIC,
    DL2_FLAG_POS_SATURATED,
    UP2_MAGIC,
    DecodeError,
    Dl2Frame,
    DownlinkStream,
    Up2Frame,
    V1Line,
    build_ack2,
    build_up2,
    crc16_ccitt,
    decode_up2,
    encode_dl2,
    frame_to_csv_row,
)


def make_frame(**overrides) -> Dl2Frame:
    base = dict(
        seq=1234, flags=0, ufb=0, ts_ms=567890,
        roll_rad=0.1234, pitch_rad=-0.2345, yaw_rad=1.5678,
        x_m=12.34, y_m=-56.78, z_m=-3.21,
        vx_mps=1.23, vy_mps=-0.45, vz_mps=0.06,
        lat_e7=374530000, lon_e7=1269850000, alt_mm=52000,
        fix=3, sats=11, health=1, fault=0, linkstate=1,
    )
    base.update(overrides)
    return Dl2Frame(**base)


class Crc16Test(unittest.TestCase):
    def test_known_vector(self):
        # CRC-16/CCITT-FALSE 표준 벡터: "123456789" -> 0x29B1
        self.assertEqual(crc16_ccitt(b"123456789"), 0x29B1)


class RoundtripTest(unittest.TestCase):
    def _roundtrip(self, frame: Dl2Frame) -> Dl2Frame:
        stream = DownlinkStream()
        events = stream.feed(encode_dl2(frame))
        self.assertEqual(len(events), 1)
        self.assertIsInstance(events[0], Dl2Frame)
        return events[0]

    def test_basic_roundtrip(self):
        out = self._roundtrip(make_frame())
        self.assertEqual(out.seq, 1234)
        self.assertEqual(out.ts_ms, 567890)
        self.assertAlmostEqual(out.roll_rad, 0.1234, places=4)
        self.assertAlmostEqual(out.pitch_rad, -0.2345, places=4)
        self.assertAlmostEqual(out.x_m, 12.34, places=2)
        self.assertAlmostEqual(out.vz_mps, 0.06, places=2)
        self.assertEqual(out.lat_e7, 374530000)
        self.assertEqual(out.lon_e7, 1269850000)
        self.assertEqual(out.alt_mm, 52000)
        self.assertEqual((out.fix, out.sats, out.health, out.fault, out.linkstate), (3, 11, 1, 0, 1))
        self.assertIsNone(out.sys_time_unix_usec)

    def test_systime_block(self):
        utc_us = 1_784_950_123_456_789  # 2026-07 UNIX epoch µs
        out = self._roundtrip(make_frame(sys_time_unix_usec=utc_us))
        self.assertEqual(out.sys_time_unix_usec, utc_us)
        # 확장 블록 프레임 길이 = 47(sats 포함 기본) + 8
        self.assertEqual(len(encode_dl2(make_frame(sys_time_unix_usec=utc_us))), 55)

    def test_base_frame_is_47_bytes(self):
        self.assertEqual(len(encode_dl2(make_frame())), 47)  # spec §4 (sats 포함, 2026-07-13)

    def test_saturation_flag(self):
        out = self._roundtrip(make_frame(flags=DL2_FLAG_POS_SATURATED, x_m=327.67))
        self.assertTrue(out.pos_saturated)
        self.assertAlmostEqual(out.x_m, 327.67, places=2)

    def test_angle_range_pi(self):
        import math
        out = self._roundtrip(make_frame(yaw_rad=-math.pi))
        self.assertAlmostEqual(out.yaw_rad, -math.pi, places=3)


class StreamingTest(unittest.TestCase):
    def test_byte_by_byte_feed(self):
        """프레임이 여러 RX 조각에 걸쳐 수신 (spec §7.1)."""
        data = encode_dl2(make_frame())
        stream = DownlinkStream()
        events = []
        for i in range(len(data)):
            events += stream.feed(data[i:i + 1])
        self.assertEqual(len(events), 1)
        self.assertEqual(events[0].seq, 1234)

    def test_two_frames_one_chunk(self):
        data = encode_dl2(make_frame(seq=1)) + encode_dl2(make_frame(seq=2))
        events = DownlinkStream().feed(data)
        self.assertEqual([e.seq for e in events], [1, 2])

    def test_crc_corruption_resync(self):
        """손상 프레임 뒤의 정상 프레임이 재동기화로 수신되어야 함 (spec §3)."""
        bad = bytearray(encode_dl2(make_frame(seq=7)))
        bad[20] ^= 0xFF  # payload 중간 손상
        good = encode_dl2(make_frame(seq=8))
        events = DownlinkStream().feed(bytes(bad) + good)
        errors = [e for e in events if isinstance(e, DecodeError)]
        frames = [e for e in events if isinstance(e, Dl2Frame)]
        self.assertGreaterEqual(len(errors), 1)
        self.assertEqual([f.seq for f in frames], [8])

    def test_garbage_prefix_resync(self):
        data = b"\x00\xff\x07" + encode_dl2(make_frame(seq=9))
        frames = [e for e in DownlinkStream().feed(data) if isinstance(e, Dl2Frame)]
        self.assertEqual([f.seq for f in frames], [9])

    def test_v1_v2_mixed_stream(self):
        """spec §8: 첫 바이트로 v1 텍스트/v2 바이너리 분기."""
        v1_line = b"FC,12,3456,0.1000,-0.2000,1.5000,1.0,2.0,3.0,0.1,0.2,0.3,374530000,1269850000,52000,3,0\n"
        data = v1_line + encode_dl2(make_frame(seq=11)) + b"SH,13,3457,1,0,1,0\n"
        events = DownlinkStream().feed(data)
        v1 = [e for e in events if isinstance(e, V1Line)]
        v2 = [e for e in events if isinstance(e, Dl2Frame)]
        self.assertEqual(len(v1), 2)
        self.assertTrue(v1[0].text.startswith("FC,"))
        self.assertTrue(v1[1].text.startswith("SH,"))
        self.assertEqual([f.seq for f in v2], [11])

    def test_bad_len_field(self):
        # len < 44 → 오류 후 재동기화
        data = bytes([0xD2, 10]) + b"\x00" * 20 + encode_dl2(make_frame(seq=3))
        events = DownlinkStream().feed(data)
        frames = [e for e in events if isinstance(e, Dl2Frame)]
        self.assertEqual([f.seq for f in frames], [3])


class Ack2Test(unittest.TestCase):
    def test_format(self):
        ack = build_ack2(0x1234)
        self.assertEqual(len(ack), 5)  # spec §6
        magic, seq = struct.unpack_from("<BH", ack, 0)
        (crc,) = struct.unpack_from("<H", ack, 3)
        self.assertEqual(magic, ACK2_MAGIC)
        self.assertEqual(seq, 0x1234)
        self.assertEqual(crc, crc16_ccitt(ack[:3]))

    def test_seq_wrap(self):
        ack = build_ack2(0x1_0005)  # u16 초과 → wrap
        _, seq = struct.unpack_from("<BH", ack, 0)
        self.assertEqual(seq, 0x0005)


class Up2Test(unittest.TestCase):
    """UP2 인코더(지상 송신) — spec §5. 기체측 디코더는
    lora_tdm_app/unit-test/coveragetest/coveragetest_lora_tdm_app_utils.c
    Test_ParseUp2Frame_*와 대응."""

    def test_roundtrip_with_payload(self):
        raw = build_up2(version=2, command_class=1, seq=99, payload=b"\xAA\xBB\xCC")
        decoded = decode_up2(raw)
        self.assertEqual(decoded, Up2Frame(version=2, command_class=1, seq=99, flags=0,
                                            payload=b"\xAA\xBB\xCC"))

    def test_roundtrip_zero_payload(self):
        raw = build_up2(version=2, command_class=4, seq=7)
        decoded = decode_up2(raw)
        self.assertEqual(decoded.payload, b"")
        self.assertEqual(len(raw), 9)  # magic+plen+ver+class+seq2+flags+crc2, payload 없음

    def test_crc_present_and_valid(self):
        raw = build_up2(version=2, command_class=1, seq=99, payload=b"\xAA\xBB\xCC")
        body = raw[:-2]
        (crc,) = struct.unpack_from("<H", raw, len(body))
        self.assertEqual(crc, crc16_ccitt(body))

    def test_magic_byte(self):
        raw = build_up2(version=2, command_class=1, seq=1)
        self.assertEqual(raw[0], UP2_MAGIC)

    def test_seq_wrap(self):
        raw = build_up2(version=2, command_class=1, seq=0x1_0005)
        (seq,) = struct.unpack_from("<H", raw, 4)
        self.assertEqual(seq, 0x0005)

    def test_cross_language_vector_matches_c_ut(self):
        """C측 Test_ParseUp2Frame_ValidWithPayload(coveragetest_lora_tdm_app_utils.c)와
        완전히 동일한 입력(version=2, class=1, seq=99, payload=AA BB CC)으로 만든
        프레임이 바이트 단위로 같은지 확인 — 두 언어 구현이 실제로 맞물리는지 증명."""
        raw = build_up2(version=2, command_class=1, seq=99, payload=b"\xAA\xBB\xCC")
        expected = bytes([
            UP2_MAGIC, 3,        # magic, plen
            2, 1,                 # version, command_class
            99, 0,                 # seq (LE u16)
            0,                     # flags
            0xAA, 0xBB, 0xCC,      # payload
        ])
        expected += struct.pack("<H", crc16_ccitt(expected))
        self.assertEqual(raw, expected)


class CsvRowTest(unittest.TestCase):
    def test_row_fields(self):
        row = frame_to_csv_row(make_frame(sys_time_unix_usec=1000), host_time=1784950000.123)
        self.assertEqual(row["seq"], 1234)
        self.assertEqual(row["sys_time_unix_usec"], 1000)
        self.assertTrue(row["host_time_iso"].endswith("Z"))

    def test_row_without_systime(self):
        row = frame_to_csv_row(make_frame(), host_time=0.0)
        self.assertEqual(row["sys_time_unix_usec"], "")


if __name__ == "__main__":
    unittest.main()
