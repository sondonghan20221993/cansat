#!/usr/bin/env python3
import argparse
import socket
import struct
import sys
import time
from dataclasses import dataclass
from typing import List, Optional, Sequence

try:
    import serial
except ModuleNotFoundError:
    serial = None


UPLINK_APP_CMD_MID = 0x18D0
UPLINK_APP_PROCESS_UPLINK_CC = 2
UPLINK_APP_MAX_PAYLOAD_LENGTH = 196

UPLINK_CLASS_ROUTE_UPDATE = 2

# route_op (BL-56, 2026-07-25 재설계 — REPLACE/APPEND/DELETE 3종에서 갱신)
ROUTE_OP_REPLACE = 1
ROUTE_OP_ADD = 2
ROUTE_OP_DELETE = 3
ROUTE_OP_MODIFY = 4

ROUTE_VERSION = 2  # BL-56: payload 포맷 전면 변경으로 v1->v2

ROUTE_WAYPOINT_WIRE_SIZE = 29  # CmdType(1)+Param1..4(4x4)+LatE7(4)+LonE7(4)+Z(4)

MAV_CMD_NAV_WAYPOINT = 16


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

    crc_input = struct.pack("<BBBBH", version, command_class,
                            len(proxy_payload), flags, sequence) + proxy_payload
    checksum = crc16_ccitt(crc_input)

    fixed_payload = proxy_payload + bytes(UPLINK_APP_MAX_PAYLOAD_LENGTH - len(proxy_payload))

    return struct.pack(
        f"{host_endian}BBBBHH",
        version,
        command_class,
        len(proxy_payload),
        flags,
        sequence,
        checksum,
    ) + fixed_payload


@dataclass(frozen=True)
class Waypoint:
    """BL-56(2026-07-25): 항상 절대좌표(LatE7/LonE7). 로컬 X/Y 모드는 폐기됨."""
    lat_e7: int
    lon_e7: int
    z_m: float
    cmd_type: int = MAV_CMD_NAV_WAYPOINT
    param1: float = 0.0
    param2: float = 0.0
    param3: float = 0.0
    param4: float = 0.0

    def pack(self) -> bytes:
        return struct.pack(
            "<Bffffiif",
            self.cmd_type, self.param1, self.param2, self.param3, self.param4,
            self.lat_e7, self.lon_e7, self.z_m,
        )


def build_route_payload(route_op: int, index_or_count: int, waypoints: Sequence[Waypoint]) -> bytes:
    """route_op=REPLACE/ADD: waypoints 전체 실어보냄(index_or_count=len(waypoints)).
    route_op=DELETE: waypoints 없음(index_or_count=삭제할 index).
    route_op=MODIFY: waypoints 1개(index_or_count=수정할 index)."""
    payload = struct.pack("<BBBB", route_op, ROUTE_VERSION, index_or_count, 0)
    for wp in waypoints:
        payload += wp.pack()
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


def pretty_waypoints(waypoints: Sequence[Waypoint]) -> str:
    return ", ".join(
        f"(cmd={wp.cmd_type} lat={wp.lat_e7/1e7:.7f} lon={wp.lon_e7/1e7:.7f} z={wp.z_m:.2f})"
        for wp in waypoints
    )


# 더미 기준점(실 GPS 없이 기능 검증용) — 임의 좌표, 실제 위치와 무관
_DUMMY_LAT_E7 = 375665000   # 37.5665000
_DUMMY_LON_E7 = 1269780000  # 126.9780000


def _dummy_wp(dlat_e7: int, dlon_e7: int, z: float, **kw) -> Waypoint:
    return Waypoint(lat_e7=_DUMMY_LAT_E7 + dlat_e7, lon_e7=_DUMMY_LON_E7 + dlon_e7, z_m=z, **kw)


def preset_case(case_name: str):
    """반환: (route_op, index_or_count, waypoints)"""
    # 절대좌표 REPLACE(전체 교체) — 더미 기준점 근처 2점
    if case_name == "route-replace-good":
        wps = [_dummy_wp(0, 0, 3.0), _dummy_wp(20, 0, 3.0)]
        return ROUTE_OP_REPLACE, len(wps), wps
    if case_name == "route-replace-bad-alt":
        wps = [_dummy_wp(0, 0, 1.0), _dummy_wp(20, 0, 3.0)]  # 고도 1.0m < MIN(2m)
        return ROUTE_OP_REPLACE, len(wps), wps
    # ADD(끝에 추가) — BL-56: index_or_count=추가 개수, 세그먼트 거리 제약 폐지됨
    if case_name == "route-add-good":
        wps = [_dummy_wp(40, 0, 3.0), _dummy_wp(60, 0, 3.0)]
        return ROUTE_OP_ADD, len(wps), wps
    if case_name == "route-add-single":
        wps = [_dummy_wp(80, 0, 3.0)]
        return ROUTE_OP_ADD, len(wps), wps
    # DELETE(index) — waypoint 데이터 없음, index_or_count=삭제할 인덱스
    if case_name == "route-delete-index0":
        return ROUTE_OP_DELETE, 0, []
    if case_name == "route-delete-index1":
        return ROUTE_OP_DELETE, 1, []
    # MODIFY(index) — waypoint 1개, index_or_count=수정할 인덱스, 전체 필드 통째 교체
    if case_name == "route-modify-index0":
        wps = [_dummy_wp(5, 5, 4.0)]
        return ROUTE_OP_MODIFY, 0, wps
    if case_name == "route-modify-loiter":
        wps = [_dummy_wp(5, 5, 4.0, cmd_type=17, param1=10.0)]  # NAV_LOITER_UNLIM, radius=10
        return ROUTE_OP_MODIFY, 1, wps
    raise ValueError(f"unknown case: {case_name}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Send UPLINK_APP PROCESS_UPLINK route-update commands to cFS/CI_LAB "
                    "(BL-56 v2 protocol: REPLACE/ADD/DELETE/MODIFY, absolute lat/lon waypoints)."
    )
    parser.add_argument(
        "case_name",
        choices=["route-replace-good", "route-replace-bad-alt",
                 "route-add-good", "route-add-single",
                 "route-delete-index0", "route-delete-index1",
                 "route-modify-index0", "route-modify-loiter"],
        help="Preset route-update test case to send.",
    )
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=1234)
    parser.add_argument("--sequence", type=int, default=1)
    parser.add_argument("--auth", type=int, default=2, choices=[0,1,2,3], help="Auth level in Flags[7:6] (ROUTE_UPDATE=2)")
    parser.add_argument("--force", action="store_true",
                        help="Set UPLINK_APP_FORCE_FLAG (Flags bit 0) to bypass health gate (bench-only)")
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

    route_op, index_or_count, waypoints = preset_case(args.case_name)
    route_payload = build_route_payload(route_op, index_or_count, waypoints)
    flags = (args.auth << 6) | (0x01 if args.force else 0)
    proxy_payload = build_process_uplink_payload(args.sequence, route_payload, flags=flags)

    print(
        f"prepare {args.case_name}: seq={args.sequence} route_op={route_op} "
        f"route_version={ROUTE_VERSION} index_or_count={index_or_count} "
        f"waypoints=[{pretty_waypoints(waypoints)}] transport={args.transport}"
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
