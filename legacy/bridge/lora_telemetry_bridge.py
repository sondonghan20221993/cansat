#!/usr/bin/env python3
import argparse
import socket
import struct
import sys
import time
from dataclasses import dataclass
from typing import Optional

import serial


DEFAULT_SERIAL_PATH = "/dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0"
DEFAULT_BAUDRATE = 57600
DEFAULT_MONITOR_MID = 0x0847
DEFAULT_ACTIVE_TRANSPORT_ID = 1
DEFAULT_MONITOR_PERIOD_MS = 500
DEFAULT_UDP_HOST = "127.0.0.1"
DEFAULT_UDP_PORT = 1234
DEFAULT_SERIAL_TIMEOUT_MS = 100


@dataclass
class ParsedHeartbeat:
    seq: Optional[int]
    tx_time_ms: Optional[int]


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


def ccsds_primary(mid: int, size: int) -> bytes:
    return struct.pack(">HHH", mid, 0xC000, size - 7)


def build_monitor_packet(mid: int, active_transport_id: int, valid: int, update_age_ms: int) -> bytes:
    header = ccsds_primary(mid, 24) + (b"\x00" * 10)
    payload = struct.pack("<BBHI", active_transport_id, valid, 0, update_age_ms)
    return header + payload


def parse_manual_frame(text: str) -> Optional[ParsedHeartbeat]:
    upper = text.strip().upper()
    if upper == "HB" or upper == "HELLO":
        return ParsedHeartbeat(seq=None, tx_time_ms=None)

    parts = [part.strip() for part in text.split(",")]
    if len(parts) == 2 and parts[0].upper() in {"HB", "HELLO"}:
        try:
            return ParsedHeartbeat(seq=int(parts[1], 0), tx_time_ms=None)
        except ValueError:
            return None

    return None


def parse_canonical_frame(text: str) -> Optional[ParsedHeartbeat]:
    parts = [part.strip() for part in text.split(",")]
    if len(parts) != 6 or parts[0].upper() != "HB":
        return None

    node_id, seq, tx_time_ms, sensor_ok, crc_hex = parts[1:]
    try:
        seq_val = int(seq, 0)
        tx_time_val = int(tx_time_ms, 0)
        sensor_ok_val = int(sensor_ok, 0)
        crc_val = int(crc_hex, 16)
        int(node_id, 0)
    except ValueError:
        return None

    canonical_without_crc = f"HB,{node_id},{seq},{tx_time_ms},{sensor_ok}".encode("utf-8")
    if crc16_ccitt(canonical_without_crc) != crc_val:
        return None

    if sensor_ok_val == 0:
        return None

    return ParsedHeartbeat(seq=seq_val, tx_time_ms=tx_time_val)


def parse_heartbeat_line(text: str) -> Optional[ParsedHeartbeat]:
    return parse_canonical_frame(text) or parse_manual_frame(text)


class Bridge:
    def __init__(
        self,
        serial_path: str,
        baudrate: int,
        monitor_mid: int,
        active_transport_id: int,
        monitor_period_ms: int,
        udp_host: str,
        udp_port: int,
        serial_timeout_ms: int,
    ) -> None:
        self.serial_path = serial_path
        self.baudrate = baudrate
        self.monitor_mid = monitor_mid
        self.active_transport_id = active_transport_id
        self.monitor_period_ms = monitor_period_ms
        self.udp_host = udp_host
        self.udp_port = udp_port
        self.serial_timeout_ms = serial_timeout_ms
        self.last_valid_rx_time_monotonic: Optional[float] = None
        self.last_seq: Optional[int] = None
        self.serial_port: Optional[serial.Serial] = None
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def open_serial(self) -> None:
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
            except serial.SerialException as exc:
                print(f"serial open failed: {exc}", flush=True)
                time.sleep(1.0)

    def sequence_ok(self, seq: Optional[int]) -> bool:
        if seq is None:
            return True
        if self.last_seq is None:
            self.last_seq = seq
            return True
        if seq > self.last_seq:
            self.last_seq = seq
            return True
        return False

    def process_line(self, raw: bytes) -> None:
        text = raw.decode("utf-8", errors="replace").strip()
        if not text:
            return

        parsed = parse_heartbeat_line(text)
        if parsed is None:
            print(f"discard invalid frame: {text}", flush=True)
            return

        if not self.sequence_ok(parsed.seq):
            print(f"discard seq regression frame: {text}", flush=True)
            return

        self.last_valid_rx_time_monotonic = time.monotonic()
        print(f"accepted heartbeat: {text}", flush=True)

    def send_monitor(self) -> None:
        if self.last_valid_rx_time_monotonic is None:
            update_age_ms = 0xFFFFFFFF & int(time.monotonic() * 1000)
            valid = 0
        else:
            update_age_ms = int((time.monotonic() - self.last_valid_rx_time_monotonic) * 1000)
            valid = 1

        packet = build_monitor_packet(
            self.monitor_mid,
            self.active_transport_id,
            valid,
            update_age_ms,
        )
        self.sock.sendto(packet, (self.udp_host, self.udp_port))

    def run(self) -> int:
        print(
            "starting lora bridge: "
            f"serial={self.serial_path} baud={self.baudrate} "
            f"monitor_mid=0x{self.monitor_mid:04X} transport_id={self.active_transport_id} "
            f"period_ms={self.monitor_period_ms} udp={self.udp_host}:{self.udp_port}",
            flush=True,
        )

        next_send = time.monotonic()
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

            now = time.monotonic()
            if now >= next_send:
                self.send_monitor()
                next_send = now + (self.monitor_period_ms / 1000.0)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial-path", default=DEFAULT_SERIAL_PATH, help="serial path")
    parser.add_argument("--baudrate", type=int, default=DEFAULT_BAUDRATE, help="serial baudrate")
    parser.add_argument("--monitor-mid", type=lambda value: int(value, 0), default=DEFAULT_MONITOR_MID, help="monitor mid")
    parser.add_argument(
        "--active-transport-id",
        type=int,
        default=DEFAULT_ACTIVE_TRANSPORT_ID,
        help="active transport id for telemetry monitor payload",
    )
    parser.add_argument(
        "--monitor-period-ms",
        type=int,
        default=DEFAULT_MONITOR_PERIOD_MS,
        help="monitor publication period in milliseconds",
    )
    parser.add_argument("--udp-host", default=DEFAULT_UDP_HOST, help="cFS UDP host")
    parser.add_argument("--udp-port", type=int, default=DEFAULT_UDP_PORT, help="cFS UDP port")
    parser.add_argument(
        "--serial-timeout-ms",
        type=int,
        default=DEFAULT_SERIAL_TIMEOUT_MS,
        help="serial read timeout in milliseconds",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    bridge = Bridge(
        serial_path=args.serial_path,
        baudrate=args.baudrate,
        monitor_mid=args.monitor_mid,
        active_transport_id=args.active_transport_id,
        monitor_period_ms=args.monitor_period_ms,
        udp_host=args.udp_host,
        udp_port=args.udp_port,
        serial_timeout_ms=args.serial_timeout_ms,
    )
    return bridge.run()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
