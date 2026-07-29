#!/usr/bin/env python3
"""
uplink_flight_mode_sender.py — 지상국에서 FLIGHT_MODE(class=8, BL-44) 명령을
uplink_app으로 전송하는 툴. §18.4.6.8 payload: flight_mode(1) +
waypoint_start_index(1) + request_token(4 LE) = 6바이트, auth Level 3 필요.
"""
import argparse
import socket
import struct

UPLINK_APP_CMD_MID            = 0x18D0
UPLINK_APP_PROCESS_UPLINK_CC  = 2
UPLINK_APP_MAX_PAYLOAD_LENGTH = 196
UPLINK_CLASS_FLIGHT_MODE      = 8
UPLINK_PROTOCOL_VERSION       = 1

MODE_NAMES = {"hover": 0, "waypoint": 1, "land": 2}


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


def build_command_packet(function_code: int, payload: bytes) -> bytes:
    cmd_secondary = bytearray(2)
    cmd_secondary[0] = function_code
    total_size = 6 + 2 + len(payload)
    primary = bytearray(ccsds_primary(UPLINK_APP_CMD_MID, total_size))
    packet_wo_checksum = bytes(primary) + bytes(cmd_secondary) + payload
    cmd_secondary[1] = calc_checksum(packet_wo_checksum)
    return bytes(primary) + bytes(cmd_secondary) + payload


def build_flight_mode_payload(flight_mode: int, waypoint_start_index: int, request_token: int) -> bytes:
    return struct.pack("<BBI", flight_mode, waypoint_start_index, request_token)


def build_process_uplink_payload(sequence: int, fm_payload: bytes, flags: int) -> bytes:
    crc_input = struct.pack("<BBBBH", UPLINK_PROTOCOL_VERSION, UPLINK_CLASS_FLIGHT_MODE,
                            len(fm_payload), flags, sequence) + fm_payload
    checksum = crc16_ccitt(crc_input)
    pad = bytes(UPLINK_APP_MAX_PAYLOAD_LENGTH - len(fm_payload))
    return struct.pack("<BBBBHH",
                       UPLINK_PROTOCOL_VERSION, UPLINK_CLASS_FLIGHT_MODE,
                       len(fm_payload), flags, sequence, checksum) + fm_payload + pad


def main() -> int:
    parser = argparse.ArgumentParser(description="Send FLIGHT_MODE uplink command to cFS uplink_app.")
    parser.add_argument("mode", choices=list(MODE_NAMES))
    parser.add_argument("--host", default="127.0.0.1", help="cFS UDP host")
    parser.add_argument("--port", type=int, default=1234, help="cFS UDP port")
    parser.add_argument("--sequence", type=int, required=True, help="Uplink sequence number")
    parser.add_argument("--auth", type=int, default=3, choices=[0, 1, 2, 3],
                        help="Auth level in Flags[7:6] (FLIGHT_MODE requires Level 3)")
    parser.add_argument("--waypoint-index", type=int, default=0,
                        help="WAYPOINT 전용 MISSION_SET_CURRENT 대상 인덱스 (HOVER/LAND는 0 고정)")
    parser.add_argument("--token", type=int, default=1, help="request_token (Level 3 non-zero 필수)")
    args = parser.parse_args()

    waypoint_index = args.waypoint_index if args.mode == "waypoint" else 0
    flags = args.auth << 6
    fm_payload = build_flight_mode_payload(MODE_NAMES[args.mode], waypoint_index, args.token)
    uplink_payload = build_process_uplink_payload(args.sequence, fm_payload, flags)
    packet = build_command_packet(UPLINK_APP_PROCESS_UPLINK_CC, uplink_payload)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(packet, (args.host, args.port))
    print(f"flight_mode: mode={args.mode} seq={args.sequence} auth={args.auth} "
          f"sent {len(packet)} bytes to {args.host}:{args.port}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
