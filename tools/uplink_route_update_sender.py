#!/usr/bin/env python3
import argparse
import socket
import struct
import sys
import time
from typing import Iterable, List, Sequence, Tuple

try:
    import serial
except ModuleNotFoundError:
    serial = None


UPLINK_APP_CMD_MID = 0x18D0
UPLINK_APP_PROCESS_UPLINK_CC = 2
UPLINK_APP_MAX_PAYLOAD_LENGTH = 196

UPLINK_CLASS_ROUTE_UPDATE = 2

ROUTE_TYPE_MISSION_EXTENSION = 1
ROUTE_TYPE_LANDING = 2


def calc_checksum(packet: bytes) -> int:
    checksum = 0xFF
    for b in packet:
        checksum ^= b
    return checksum


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


def ccsds_primary(mid: int, total_packet_size: int) -> bytes:
    return struct.pack(">HHH", mid, 0xC000, total_packet_size - 7)


def build_command_packet(function_code: int, payload: bytes, host_endian: str = "<") -> bytes:
    cmd_secondary = bytearray(2)
    cmd_secondary[0] = function_code

    total_size = 6 + 2 + len(payload)
    primary = bytearray(ccsds_primary(UPLINK_APP_CMD_MID, total_size))

    packet_wo_checksum = bytearray()
    packet_wo_checksum.extend(primary)
    packet_wo_checksum.extend(cmd_secondary)
    packet_wo_checksum.extend(payload)

    cmd_secondary[1] = calc_checksum(packet_wo_checksum)

    packet = bytearray()
    packet.extend(primary)
    packet.extend(cmd_secondary)
    packet.extend(payload)
    return bytes(packet)


def build_process_uplink_payload(
    sequence: int,
    proxy_payload: bytes,
    command_class: int = UPLINK_CLASS_ROUTE_UPDATE,
    version: int = 1,
    flags: int = 0,
    host_endian: str = "<",
) -> bytes:
    if len(proxy_payload) > UPLINK_APP_MAX_PAYLOAD_LENGTH:
        raise ValueError(
            f"route payload too large: {len(proxy_payload)} > {UPLINK_APP_MAX_PAYLOAD_LENGTH}"
        )

    fixed_payload = proxy_payload + bytes(UPLINK_APP_MAX_PAYLOAD_LENGTH - len(proxy_payload))

    return struct.pack(
        f"{host_endian}BBBBHH",
        version,
        command_class,
        len(proxy_payload),
        flags,
        sequence,
        0,
    ) + fixed_payload


def build_route_payload(route_type: int, route_version: int, waypoints: Sequence[Tuple[float, float, float]]) -> bytes:
    payload = struct.pack("<BBBB", route_type, route_version, len(waypoints), 0)
    for x, y, z in waypoints:
        payload += struct.pack("<fff", x, y, z)
    return payload


def build_lora_uplink_frame(
    sequence: int,
    proxy_payload: bytes,
    command_class: int = UPLINK_CLASS_ROUTE_UPDATE,
    version: int = 1,
    flags: int = 0,
) -> str:
    payload_hex = proxy_payload.hex().upper()
    canonical_without_crc = f"UP,{version},{command_class},{sequence},{flags},{payload_hex}"
    crc = crc16_ccitt(canonical_without_crc.encode("ascii"))
    return f"{canonical_without_crc},{crc:04X}"


def send_packet(packet: bytes, host: str, port: int) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(packet, (host, port))


def send_lora_serial(frame_line: str, serial_path: str, baudrate: int, write_delay_s: float = 0.05) -> None:
    if serial is None:
        raise RuntimeError("pyserial is required for lora-serial transport: pip install pyserial")
    with serial.Serial(serial_path, baudrate, timeout=2.0) as port:
        time.sleep(write_delay_s)
        port.write((frame_line + "\n").encode("ascii"))
        port.flush()


def pretty_waypoints(waypoints: Iterable[Tuple[float, float, float]]) -> str:
    return ", ".join(f"({x:.2f}, {y:.2f}, {z:.2f})" for x, y, z in waypoints)


def preset_case(case_name: str) -> Tuple[int, List[Tuple[float, float, float]]]:
    # GPS 있음 케이스: bridge가 GLOBAL_POSITION_INT lat/lon 기준으로 좌표 변환
    if case_name == "route-good":
        return ROUTE_TYPE_MISSION_EXTENSION, [(0.0, -10.0, 3.0), (2.0, -10.0, 3.0)]
    if case_name == "route-landing":
        return ROUTE_TYPE_LANDING, [(2.0, -8.0, 4.0), (2.0, -8.0, 2.0)]
    if case_name == "route-bad-alt":
        return ROUTE_TYPE_MISSION_EXTENSION, [(0.0, -10.0, 1.0), (2.0, -10.0, 3.0)]
    if case_name == "route-bad-distance":
        return ROUTE_TYPE_MISSION_EXTENSION, [(0.0, -10.0, 3.0), (2.01, -10.0, 3.0)]
    # GPS 없음 케이스: bridge가 (0,0) 원점 기준으로 변환 (기능 검증 전용)
    if case_name == "route-good-no-gps":
        return ROUTE_TYPE_MISSION_EXTENSION, [(0.0, -10.0, 3.0), (2.0, -10.0, 3.0)]
    if case_name == "route-landing-no-gps":
        return ROUTE_TYPE_LANDING, [(2.0, -8.0, 4.0), (2.0, -8.0, 2.0)]
    # Mission Planner 시각 검증용: 뚜렷하게 다른 좌표
    if case_name == "route-mp-verify-a":
        return ROUTE_TYPE_MISSION_EXTENSION, [(50.0, 30.0, 10.0), (100.0, 60.0, 15.0)]
    if case_name == "route-mp-verify-b":
        return ROUTE_TYPE_MISSION_EXTENSION, [(-30.0, 20.0, 5.0), (-60.0, 40.0, 8.0)]
    raise ValueError(f"unknown case: {case_name}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Send UPLINK_APP PROCESS_UPLINK route-update commands to cFS/CI_LAB."
    )
    parser.add_argument(
        "case_name",
        choices=["route-good", "route-landing", "route-bad-alt", "route-bad-distance",
                 "route-good-no-gps", "route-landing-no-gps",
                 "route-mp-verify-a", "route-mp-verify-b"],
        help="Preset route-update test case to send.",
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1234)
    parser.add_argument("--sequence", type=int, default=1)
    parser.add_argument("--route-version", type=int, default=1)
    parser.add_argument(
        "--transport",
        choices=["udp", "lora-text", "lora-serial"],
        default="udp",
        help="udp sends directly to cFS, lora-text prints the LoRa frame, lora-serial sends via serial port",
    )
    parser.add_argument(
        "--serial-path",
        default=None,
        help="serial port path for lora-serial transport (e.g. COM3 or /dev/ttyUSB0)",
    )
    parser.add_argument(
        "--baudrate",
        type=int,
        default=57600,
        help="serial baudrate for lora-serial transport (default: 57600)",
    )
    args = parser.parse_args()

    route_type, waypoints = preset_case(args.case_name)
    route_payload = build_route_payload(route_type, args.route_version, waypoints)
    proxy_payload = build_process_uplink_payload(args.sequence, route_payload)

    print(
        f"prepare {args.case_name}: seq={args.sequence} route_type={route_type} "
        f"route_version={args.route_version} waypoints=[{pretty_waypoints(waypoints)}] "
        f"transport={args.transport}"
    )

    if args.transport == "udp":
        packet = build_command_packet(UPLINK_APP_PROCESS_UPLINK_CC, proxy_payload)
        send_packet(packet, args.host, args.port)
    elif args.transport == "lora-text":
        print(build_lora_uplink_frame(args.sequence, route_payload))
    else:
        if not args.serial_path:
            print("error: --serial-path is required for lora-serial transport", file=sys.stderr)
            return 1
        frame_line = build_lora_uplink_frame(args.sequence, route_payload)
        print(f"sending via serial {args.serial_path} at {args.baudrate} baud")
        print(f"frame: {frame_line}")
        send_lora_serial(frame_line, args.serial_path, args.baudrate)
        print("sent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
