"""bridge/lora_downlink_decoder.py waypoint readback 디코딩 테스트 (BL-64, BL-71).

기체(lora_tdm_app)가 DL2_FLAG_WAYPOINT 첨부 시 붙이는 waypoint 페이지
블록을 지상 디코더가 실제로 파싱하는지 검증. openMCT 레포
(lora_protocol_v2.py, BL-61/BL-71)의 이미 검증된 참조 구현을 이식한 것 —
tests/test_lora_protocol_v2_waypoint.py와 동일 구조.

BL-71(2026-07-28): waypoint 튜플에 CmdType(u8)이 선두로 추가됨
(cmd_type, lat_e7, lon_e7, z) — 12바이트/waypoint에서 13바이트로 확장,
지상이 위치 기반으로 시작/호버/랜딩을 임의 추측하지 않고 실제
CmdType을 READ해서 구분할 수 있게 됨.
"""

import unittest

from bridge.lora_downlink_decoder import (
    Dl2Frame,
    DownlinkStream,
    RouteReadbackAssembler,
    decode_dl2,
    encode_dl2,
    DL2_FLAG_WAYPOINT,
)


def make_base_frame(**overrides):
    base = dict(
        seq=1, flags=0, ufb=0, ts_ms=1000,
        roll_rad=0.0, pitch_rad=0.0, yaw_rad=0.0,
        x_m=0.0, y_m=0.0, z_m=0.0,
        vx_mps=0.0, vy_mps=0.0, vz_mps=0.0,
        lat_e7=0, lon_e7=0, alt_mm=0,
        fix=3, sats=8, health=0, fault=0, linkstate=1,
    )
    base.update(overrides)
    return Dl2Frame(**base)


class WaypointPageRoundTripTest(unittest.TestCase):
    def test_encode_decode_full_page(self):
        frame = make_base_frame(
            wp_route_type=1, wp_page_index=0, wp_total_pages=8,
            wp_waypoints=[(17, 371234567, 1271234567, 3.5), (16, 381234567, 1281234567, 6.5)],
        )
        wire = encode_dl2(frame)
        decoded = decode_dl2(wire)

        self.assertTrue(decoded.has_waypoint_page)
        self.assertEqual(decoded.wp_route_type, 1)
        self.assertEqual(decoded.wp_page_index, 0)
        self.assertEqual(decoded.wp_total_pages, 8)
        self.assertEqual(len(decoded.wp_waypoints), 2)
        self.assertIsInstance(decoded.wp_waypoints[0][1], int)
        self.assertIsInstance(decoded.wp_waypoints[0][2], int)
        # (cmd_type, lat_e7, lon_e7, z)
        self.assertEqual(decoded.wp_waypoints[0][0], 17)
        self.assertEqual(decoded.wp_waypoints[0][1], 371234567)
        self.assertEqual(decoded.wp_waypoints[0][2], 1271234567)
        self.assertEqual(decoded.wp_waypoints[1][0], 16)
        self.assertAlmostEqual(decoded.wp_waypoints[1][3], 6.5, places=5)

    def test_encode_decode_negative_lat_lon(self):
        frame = make_base_frame(
            wp_route_type=1, wp_page_index=7, wp_total_pages=8,
            wp_waypoints=[(19, -338765432, -709876543, 9.0)],
        )
        wire = encode_dl2(frame)
        decoded = decode_dl2(wire)

        self.assertEqual(len(decoded.wp_waypoints), 1)
        self.assertEqual(decoded.wp_waypoints[0][0], 19)
        self.assertEqual(decoded.wp_waypoints[0][1], -338765432)
        self.assertEqual(decoded.wp_waypoints[0][2], -709876543)
        self.assertAlmostEqual(decoded.wp_waypoints[0][3], 9.0, places=5)

    def test_no_waypoint_flag_when_absent(self):
        frame = make_base_frame()
        wire = encode_dl2(frame)
        decoded = decode_dl2(wire)

        self.assertFalse(decoded.has_waypoint_page)
        self.assertIsNone(decoded.wp_waypoints)

    def test_downlink_stream_parses_waypoint_frame(self):
        """BL-64/BL-71 핵심: max_len이 waypoint 블록(30B)을 반영 안 하면 이 프레임은
        'bad DL2 len'으로 CRC 검증도 못 해보고 즉시 폐기된다."""
        frame = make_base_frame(
            wp_route_type=1, wp_page_index=0, wp_total_pages=1,
            wp_waypoints=[(16, 100000000, 200000000, 3.0), (16, 400000000, 500000000, 6.0)],
        )
        wire = encode_dl2(frame)
        stream = DownlinkStream()
        events = stream.feed(wire)

        self.assertEqual(len(events), 1)
        self.assertIsInstance(events[0], Dl2Frame)
        self.assertTrue(events[0].has_waypoint_page)
        self.assertEqual(events[0].wp_waypoints[0][1], 100000000)


class RouteReadbackAssemblerTest(unittest.TestCase):
    def test_assembles_full_mission_16wp_8pages(self):
        asm = RouteReadbackAssembler()
        result = None
        for page in range(8):
            wps = [(16, page * 2, 0, 0.0), (16, page * 2 + 1, 0, 0.0)]
            event = make_base_frame(
                wp_route_type=1, wp_page_index=page, wp_total_pages=8, wp_waypoints=wps,
            )
            event.flags |= DL2_FLAG_WAYPOINT
            result = asm.feed(event)

        self.assertIsNotNone(result)
        self.assertEqual(len(result), 16)
        self.assertEqual(result[0], (16, 0, 0, 0.0))
        self.assertEqual(result[15], (16, 15, 0, 0.0))
        self.assertIsInstance(result[15][1], int)

    def test_incomplete_until_all_pages_received(self):
        asm = RouteReadbackAssembler()
        event = make_base_frame(
            wp_route_type=1, wp_page_index=0, wp_total_pages=2, wp_waypoints=[(16, 1, 1, 1.0)],
        )
        event.flags |= DL2_FLAG_WAYPOINT
        result = asm.feed(event)
        self.assertIsNone(result)
        self.assertEqual(asm.progress, "1/2")

    def test_new_session_discards_previous_progress(self):
        asm = RouteReadbackAssembler()
        e1 = make_base_frame(wp_route_type=1, wp_page_index=0, wp_total_pages=2,
                              wp_waypoints=[(16, 1, 1, 1.0)])
        e1.flags |= DL2_FLAG_WAYPOINT
        asm.feed(e1)
        self.assertEqual(asm.progress, "1/2")

        e2 = make_base_frame(wp_route_type=1, wp_page_index=0, wp_total_pages=1,
                              wp_waypoints=[(16, 2, 2, 2.0), (16, 3, 3, 3.0)])
        e2.flags |= DL2_FLAG_WAYPOINT
        result = asm.feed(e2)
        self.assertEqual(result, [(16, 2, 2, 2.0), (16, 3, 3, 3.0)])

    def test_ignores_non_waypoint_frame(self):
        asm = RouteReadbackAssembler()
        event = make_base_frame()
        result = asm.feed(event)
        self.assertIsNone(result)
        self.assertEqual(asm.progress, "0/0")


if __name__ == "__main__":
    unittest.main()
