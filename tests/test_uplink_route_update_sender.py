import sys
import unittest
from unittest import mock

import tools.uplink_route_update_sender as sender


class SendLoraSerialTest(unittest.TestCase):
    def _make_mock_serial_module(self):
        mock_port = mock.MagicMock()
        mock_serial_cls = mock.MagicMock(return_value=mock.MagicMock(
            __enter__=mock.Mock(return_value=mock_port),
            __exit__=mock.Mock(return_value=False),
        ))
        mock_serial_module = mock.MagicMock()
        mock_serial_module.Serial = mock_serial_cls
        return mock_serial_module, mock_serial_cls, mock_port

    def test_writes_frame_as_ascii_with_newline(self):
        """Pi bridge uses readline(), so frame must end with \\n encoded as ASCII."""
        mock_serial_module, mock_serial_cls, mock_port = self._make_mock_serial_module()
        with mock.patch.object(sender, "serial", mock_serial_module), \
             mock.patch("tools.uplink_route_update_sender.time.sleep"):
            sender.send_lora_serial("UP,1,2,1,0,AABB,1234", "/dev/ttyUSB0", 57600)

        mock_port.write.assert_called_once_with(b"UP,1,2,1,0,AABB,1234\n")

    def test_calls_flush_after_write(self):
        """flush() must be called so bytes are not held in the OS UART buffer."""
        mock_serial_module, mock_serial_cls, mock_port = self._make_mock_serial_module()
        with mock.patch.object(sender, "serial", mock_serial_module), \
             mock.patch("tools.uplink_route_update_sender.time.sleep"):
            sender.send_lora_serial("UP,1,2,1,0,AABB,1234", "/dev/ttyUSB0", 57600)

        mock_port.flush.assert_called_once()

    def test_opens_serial_with_correct_path_and_baudrate(self):
        """Serial port must be opened with the specified path and baudrate."""
        mock_serial_module, mock_serial_cls, mock_port = self._make_mock_serial_module()
        with mock.patch.object(sender, "serial", mock_serial_module), \
             mock.patch("tools.uplink_route_update_sender.time.sleep"):
            sender.send_lora_serial("UP,1,2,1,0,AABB,1234", "COM3", 57600)

        mock_serial_cls.assert_called_once_with("COM3", 57600, timeout=2.0)

    def test_raises_runtime_error_when_pyserial_unavailable(self):
        """send_lora_serial must raise RuntimeError with install hint when pyserial is missing."""
        with mock.patch.object(sender, "serial", None):
            with self.assertRaises(RuntimeError) as ctx:
                sender.send_lora_serial("UP,1,2,1,0,AABB,1234", "/dev/ttyUSB0", 57600)

        self.assertIn("pyserial", str(ctx.exception))


class MainLoraFlagsTest(unittest.TestCase):
    """BL-63: lora-text/lora-serial 두 분기 모두 --auth/--force로 계산한
    flags를 실제 전송 프레임에 반영하는지 — main() CLI 진입점 레벨 회귀 방지."""

    def _make_mock_serial_module(self):
        mock_port = mock.MagicMock()
        mock_serial_cls = mock.MagicMock(return_value=mock.MagicMock(
            __enter__=mock.Mock(return_value=mock_port),
            __exit__=mock.Mock(return_value=False),
        ))
        mock_serial_module = mock.MagicMock()
        mock_serial_module.Serial = mock_serial_cls
        return mock_serial_module, mock_port

    def test_lora_text_prints_auth_and_force_in_flags(self):
        argv = [
            "uplink_route_update_sender.py", "route-replace-good",
            "--transport", "lora-text", "--auth", "3", "--force",
        ]
        with mock.patch.object(sys, "argv", argv), \
             mock.patch("builtins.print") as mock_print:
            sender.main()

        frame = mock_print.call_args[0][0]
        flags = int(frame.split(",")[4])
        self.assertEqual(flags, (3 << 6) | 1)  # auth=3, force=1

    def test_lora_serial_sends_auth_and_force_in_flags(self):
        mock_serial_module, mock_port = self._make_mock_serial_module()
        argv = [
            "uplink_route_update_sender.py", "route-replace-good",
            "--transport", "lora-serial", "--serial-path", "/dev/ttyUSB0",
            "--auth", "2", "--force",
        ]
        with mock.patch.object(sys, "argv", argv), \
             mock.patch.object(sender, "serial", mock_serial_module), \
             mock.patch("tools.uplink_route_update_sender.time.sleep"):
            sender.main()

        written = mock_port.write.call_args[0][0]
        frame = written.decode("ascii").rstrip("\n")
        flags = int(frame.split(",")[4])
        self.assertEqual(flags, (2 << 6) | 1)  # auth=2, force=1

    def test_lora_serial_zero_auth_no_force(self):
        mock_serial_module, mock_port = self._make_mock_serial_module()
        argv = [
            "uplink_route_update_sender.py", "route-replace-good",
            "--transport", "lora-serial", "--serial-path", "/dev/ttyUSB0",
            "--auth", "0",
        ]
        with mock.patch.object(sys, "argv", argv), \
             mock.patch.object(sender, "serial", mock_serial_module), \
             mock.patch("tools.uplink_route_update_sender.time.sleep"):
            sender.main()

        written = mock_port.write.call_args[0][0]
        frame = written.decode("ascii").rstrip("\n")
        flags = int(frame.split(",")[4])
        self.assertEqual(flags, 0)


if __name__ == "__main__":
    unittest.main()
