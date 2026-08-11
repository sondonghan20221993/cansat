"""
bridge/mavlink_uart_bridge.py 단위 테스트

검증 항목:
 - describe_message: 각 MAVLink 메시지 타입별 포맷 문자열
 - parse_args: 기본값 및 커스텀 인수 파싱
"""

import unittest
from unittest.mock import MagicMock

from legacy.bridge.mavlink_uart_bridge import describe_message, parse_args, DEFAULT_SERIAL_PATH, DEFAULT_BAUDRATE


def _mock_msg(msg_type, **fields):
    msg = MagicMock()
    msg.get_type.return_value = msg_type
    for k, v in fields.items():
        setattr(msg, k, v)
    return msg


class DescribeMessageTest(unittest.TestCase):
    def test_heartbeat_format(self):
        msg = _mock_msg(
            "HEARTBEAT",
            custom_mode=0,
            base_mode=81,
            system_status=4,
        )
        msg.get_srcSystem.return_value = 1
        msg.get_srcComponent.return_value = 1
        text = describe_message(msg)
        self.assertIn("HEARTBEAT", text)
        self.assertIn("sys=1", text)
        self.assertIn("base_mode=81", text)

    def test_attitude_format(self):
        msg = _mock_msg(
            "ATTITUDE",
            time_boot_ms=12000,
            roll=0.1,
            pitch=-0.2,
            yaw=1.5,
        )
        text = describe_message(msg)
        self.assertIn("ATTITUDE", text)
        self.assertIn("boot_ms=12000", text)
        self.assertIn("roll=0.100", text)
        self.assertIn("pitch=-0.200", text)
        self.assertIn("yaw=1.500", text)

    def test_local_position_ned_format(self):
        msg = _mock_msg(
            "LOCAL_POSITION_NED",
            time_boot_ms=5000,
            x=1.0, y=-2.0, z=0.5,
            vx=0.1, vy=0.0, vz=-0.1,
        )
        text = describe_message(msg)
        self.assertIn("LOCAL_POSITION_NED", text)
        self.assertIn("x=1.000", text)
        self.assertIn("z=0.500", text)

    def test_gps_raw_int_format(self):
        msg = _mock_msg(
            "GPS_RAW_INT",
            time_usec=123456789,
            fix_type=3,
            satellites_visible=12,
            lat=374530000,
            lon=1269850000,
            alt=50000,
        )
        text = describe_message(msg)
        self.assertIn("GPS_RAW_INT", text)
        self.assertIn("fix=3", text)
        self.assertIn("sats=12", text)

    def test_ekf_status_report_format(self):
        msg = _mock_msg(
            "EKF_STATUS_REPORT",
            flags=0x1F,
            velocity_variance=0.01,
            pos_horiz_variance=0.02,
            pos_vert_variance=0.03,
            compass_variance=0.04,
            terrain_alt_variance=0.05,
        )
        text = describe_message(msg)
        self.assertIn("EKF_STATUS_REPORT", text)
        self.assertIn("flags=31", text)

    def test_unknown_type_fallback(self):
        msg = _mock_msg("PARAM_VALUE")
        msg.to_dict.return_value = {"param_id": "ATC_RAT_RLL_P"}
        text = describe_message(msg)
        self.assertIn("PARAM_VALUE", text)


class ParseArgsTest(unittest.TestCase):
    def test_defaults(self):
        args = parse_args([])
        self.assertEqual(args.serial_path, DEFAULT_SERIAL_PATH)
        self.assertEqual(args.baudrate, DEFAULT_BAUDRATE)

    def test_custom_serial_path(self):
        args = parse_args(["--serial-path", "/dev/ttyUSB0"])
        self.assertEqual(args.serial_path, "/dev/ttyUSB0")

    def test_custom_baudrate(self):
        args = parse_args(["--baudrate", "115200"])
        self.assertEqual(args.baudrate, 115200)

    def test_custom_source_system(self):
        args = parse_args(["--source-system", "100"])
        self.assertEqual(args.source_system, 100)


if __name__ == "__main__":
    unittest.main()
