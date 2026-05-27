#!/usr/bin/env python3
import argparse
import socket
import struct
import sys
import time
from dataclasses import dataclass
from typing import Optional

try:
    import serial
except ModuleNotFoundError:  # pragma: no cover - exercised in environments without pyserial
    serial = None


DEFAULT_SERIAL_PATH = "/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0"
DEFAULT_BAUDRATE = 57600
DEFAULT_UDP_HOST = "127.0.0.1"
DEFAULT_UDP_PORT = 1234
DEFAULT_SERIAL_TIMEOUT_MS = 100
DEFAULT_UPLINK_APP_CMD_MID = 0x18D0
DEFAULT_PROCESS_UPLINK_CC = 2
DEFAULT_PROTOCOL_VERSION = 1
MAX_PAYLOAD_LENGTH = 192
STDIN_SERIAL_PATH = "-"


@dataclass
class ParsedUplinkFrame:
    version: int
    command_class: int
    sequence: int
    flags: int
    payload: bytes


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def calc_checksum(packet: bytes) -> int:
    checksum = 0xFF
    for byte in packet:
        checksum ^= byte
    return checksum


def ccsds_primary(mid: int, total_packet_size: int) -> bytes:
    return struct.pack(">HHH", mid, 0xC000, total_packet_size - 7)


def build_command_packet(mid: int, function_code: int, payload: bytes) -> bytes:
    cmd_secondary = bytearray(2)
    cmd_secondary[0] = function_code

    total_size = 6 + 2 + len(payload)
    primary = bytearray(ccsds_primary(mid, total_size))

    packet_without_checksum = bytearray()
    packet_without_checksum.extend(primary)
    packet_without_checksum.extend(cmd_secondary)
    packet_without_checksum.extend(payload)

    cmd_secondary[1] = calc_checksum(packet_without_checksum)

    packet = bytearray()
    packet.extend(primary)
    packet.extend(cmd_secondary)
    packet.extend(payload)
    return bytes(packet)


def build_process_uplink_payload(frame: ParsedUplinkFrame) -> bytes:
    if frame.version != DEFAULT_PROTOCOL_VERSION:
        raise ValueError(f"unsupported protocol version: {frame.version}")
    if len(frame.payload) > MAX_PAYLOAD_LENGTH:
        raise ValueError(f"payload too large: {len(frame.payload)} > {MAX_PAYLOAD_LENGTH}")

    fixed_payload = frame.payload + bytes(MAX_PAYLOAD_LENGTH - len(frame.payload))
    return struct.pack(
        "<BBBBHH",
        frame.version,
        frame.command_class,
        len(frame.payload),
        frame.flags,
        frame.sequence,
        0,
    ) + fixed_payload


def parse_frame_line(text: str) -> ParsedUplinkFrame:
    parts = [part.strip() for part in text.split(",")]
    if len(parts) != 7 or parts[0].upper() != "UP":
        raise ValueError("expected frame format UP,<version>,<command_class>,<sequence>,<flags>,<payload_hex>,<crc16_hex>")

    _, version_text, class_text, sequence_text, flags_text, payload_hex, crc_hex = parts
    canonical_without_crc = f"UP,{version_text},{class_text},{sequence_text},{flags_text},{payload_hex}".encode("ascii")

    try:
        version = int(version_text, 0)
        command_class = int(class_text, 0)
        sequence = int(sequence_text, 0)
        flags = int(flags_text, 0)
        expected_crc = int(crc_hex, 16)
    except ValueError as exc:
        raise ValueError(f"invalid numeric field: {exc}") from exc

    if version < 0 or version > 0xFF:
        raise ValueError("version out of range")
    if command_class < 0 or command_class > 0xFF:
        raise ValueError("command_class out of range")
    if sequence < 0 or sequence > 0xFFFF:
        raise ValueError("sequence out of range")
    if flags < 0 or flags > 0xFF:
        raise ValueError("flags out of range")

    if len(payload_hex) % 2 != 0:
        raise ValueError("payload_hex must have even length")

    try:
        payload = bytes.fromhex(payload_hex)
    except ValueError as exc:
        raise ValueError(f"invalid payload_hex: {exc}") from exc

    if len(payload) > MAX_PAYLOAD_LENGTH:
        raise ValueError(f"payload too large: {len(payload)} > {MAX_PAYLOAD_LENGTH}")

    actual_crc = crc16_ccitt(canonical_without_crc)
    if actual_crc != expected_crc:
        raise ValueError(f"crc mismatch expected=0x{expected_crc:04X} actual=0x{actual_crc:04X}")

    return ParsedUplinkFrame(
        version=version,
        command_class=command_class,
        sequence=sequence,
        flags=flags,
        payload=payload,
    )


class Bridge:
    def __init__(
        self,
        serial_path: str,
        baudrate: int,
        udp_host: str,
        udp_port: int,
        serial_timeout_ms: int,
        uplink_app_cmd_mid: int,
        process_uplink_cc: int,
        allow_seq_regression: bool,
    ) -> None:
        self.serial_path = serial_path
        self.baudrate = baudrate
        self.udp_host = udp_host
        self.udp_port = udp_port
        self.serial_timeout_ms = serial_timeout_ms
        self.uplink_app_cmd_mid = uplink_app_cmd_mid
        self.process_uplink_cc = process_uplink_cc
        self.allow_seq_regression = allow_seq_regression
        self.serial_port: Optional[serial.Serial] = None
        self.last_sequence: Optional[int] = None
        self.accepted_count = 0
        self.rejected_count = 0
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def open_serial(self) -> None:
        if serial is None:
            raise RuntimeError("pyserial is required for serial mode")
        self.serial_port = serial.Serial(
            self.serial_path,
            self.baudrate,
            timeout=self.serial_timeout_ms / 1000.0,
        )
        print(f"serial open: {self.serial_path}", flush=True)

    def ensure_serial(self) -> None:
        while self.serial_port is None:
            try:
                self.open_serial()
            except RuntimeError as exc:
                print(f"serial support unavailable: {exc}", flush=True)
                raise
            except serial.SerialException as exc:
                print(f"serial open failed: {exc}", flush=True)
                time.sleep(1.0)

    def sequence_ok(self, sequence: int) -> bool:
        if self.allow_seq_regression:
            self.last_sequence = sequence
            return True

        if self.last_sequence is None:
            self.last_sequence = sequence
            return True

        if sequence > self.last_sequence:
            self.last_sequence = sequence
            return True

        return False

    def forward_frame(self, frame: ParsedUplinkFrame) -> None:
        payload = build_process_uplink_payload(frame)
        packet = build_command_packet(self.uplink_app_cmd_mid, self.process_uplink_cc, payload)
        self.sock.sendto(packet, (self.udp_host, self.udp_port))

    def process_line(self, raw: bytes) -> None:
        text = raw.decode("ascii", errors="replace").strip()
        if not text:
            return

        try:
            frame = parse_frame_line(text)
            if not self.sequence_ok(frame.sequence):
                raise ValueError(f"sequence regression detected: last={self.last_sequence} current={frame.sequence}")
            self.forward_frame(frame)
        except ValueError as exc:
            self.rejected_count += 1
            print(f"discard uplink frame: {exc}; raw={text}", flush=True)
            return

        self.accepted_count += 1
        print(
            f"forwarded uplink frame: class={frame.command_class} seq={frame.sequence} "
            f"flags={frame.flags} payload_len={len(frame.payload)} "
            f"accepted={self.accepted_count} rejected={self.rejected_count}",
            flush=True,
        )

    def run(self) -> int:
        print(
            "starting lora uplink bridge: "
            f"serial={self.serial_path} baud={self.baudrate} "
            f"udp={self.udp_host}:{self.udp_port} "
            f"cmd_mid=0x{self.uplink_app_cmd_mid:04X} cc={self.process_uplink_cc}",
            flush=True,
        )

        if self.serial_path == STDIN_SERIAL_PATH:
            return self.run_stdin()

        while True:
            self.ensure_serial()

            try:
                assert self.serial_port is not None
                raw = self.serial_port.readline()
                if raw:
                    self.process_line(raw)
            except serial.SerialException as exc:
                print(f"serial read failed: {exc}", flush=True)
                try:
                    self.serial_port.close()
                except Exception:
                    pass
                self.serial_port = None
                time.sleep(1.0)

    def run_stdin(self) -> int:
        print("stdin mode enabled: paste canonical UP frames, one per line", flush=True)
        for line in sys.stdin.buffer:
            self.process_line(line)
        return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--serial-path",
        default=DEFAULT_SERIAL_PATH,
        help="serial path, or '-' to read frames from stdin",
    )
    parser.add_argument("--baudrate", type=int, default=DEFAULT_BAUDRATE, help="serial baudrate")
    parser.add_argument("--udp-host", default=DEFAULT_UDP_HOST, help="cFS UDP host")
    parser.add_argument("--udp-port", type=int, default=DEFAULT_UDP_PORT, help="cFS UDP port")
    parser.add_argument(
        "--serial-timeout-ms",
        type=int,
        default=DEFAULT_SERIAL_TIMEOUT_MS,
        help="serial read timeout in milliseconds",
    )
    parser.add_argument(
        "--uplink-app-cmd-mid",
        type=lambda value: int(value, 0),
        default=DEFAULT_UPLINK_APP_CMD_MID,
        help="uplink app command MID",
    )
    parser.add_argument(
        "--process-uplink-cc",
        type=int,
        default=DEFAULT_PROCESS_UPLINK_CC,
        help="uplink app process uplink command code",
    )
    parser.add_argument(
        "--allow-seq-regression",
        action="store_true",
        help="disable strict monotonic sequence check",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    bridge = Bridge(
        serial_path=args.serial_path,
        baudrate=args.baudrate,
        udp_host=args.udp_host,
        udp_port=args.udp_port,
        serial_timeout_ms=args.serial_timeout_ms,
        uplink_app_cmd_mid=args.uplink_app_cmd_mid,
        process_uplink_cc=args.process_uplink_cc,
        allow_seq_regression=args.allow_seq_regression,
    )
    return bridge.run()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
