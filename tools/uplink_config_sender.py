#!/usr/bin/env python3
"""
uplink_config_sender.py — 지상국에서 cfs_core_app 또는 mavlink_bridge_app의
런타임 설정을 변경하는 CONFIG 명령을 전송하는 툴.

지원 transport:
  udp         uplink_app UDP 입력으로 직접 전송 (기본값)
  lora-text   LoRa 텍스트 프레임을 stdout에 출력
  lora-serial LoRa serial 포트를 통해 전송

Config payload 구조 (MAVLINK_BRIDGE_APP_ConfigPayloadHdr_t / CFS_CORE_APP_ConfigPayloadHdr_t):
  scope(1) + version(1) + param_id(2 LE) + value_type(1) + value_length(1) + checksum(2 LE) + value(4 LE)
  checksum = additive uint16 sum of: scope + version + param_id_lo + param_id_hi
             + value_type + value_length + value_bytes
"""
import argparse
import socket
import struct
import sys
import time

try:
    import serial
except ModuleNotFoundError:
    serial = None

UPLINK_APP_CMD_MID          = 0x18D0
UPLINK_APP_PROCESS_UPLINK_CC = 2
UPLINK_APP_MAX_PAYLOAD_LENGTH = 196
UPLINK_CLASS_CONFIG         = 1
UPLINK_PROTOCOL_VERSION     = 1

SCOPE_CFS_CORE_APP   = 1
SCOPE_MAVLINK_BRIDGE = 2

CONFIG_VERSION    = 1
VALUE_TYPE_UINT32 = 0

CFS_CORE_PARAMS = {
    "attitude_timeout_ms": 0,
    "local_timeout_ms":    1,
    "gps_timeout_ms":      2,
    "ekf_timeout_ms":      3,
    "bridge_timeout_ms":   4,
    "publish_period_ms":   5,
}

MAVLINK_BRIDGE_PARAMS = {
    "attitude_interval_us":        0,
    "local_position_interval_us":  1,
    "global_position_interval_us": 2,
    "gps_raw_interval_us":         3,
    "ekf_status_interval_us":      4,
    "reconnect_interval_ms":       5,
    "heartbeat_interval_ms":       6,
}


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


def config_checksum(scope: int, version: int, param_id: int,
                    value_type: int, value_length: int, value_bytes: bytes) -> int:
    s = 0
    s += scope & 0xFF
    s += version & 0xFF
    s += param_id & 0xFF
    s += (param_id >> 8) & 0xFF
    s += value_type & 0xFF
    s += value_length & 0xFF
    for b in value_bytes:
        s += b
    return s & 0xFFFF


def build_config_payload(scope: int, param_id: int, value: int) -> bytes:
    value_bytes = struct.pack("<I", value)
    checksum = config_checksum(scope, CONFIG_VERSION, param_id,
                               VALUE_TYPE_UINT32, len(value_bytes), value_bytes)
    hdr = struct.pack("<BBHBBH",
                      scope, CONFIG_VERSION, param_id,
                      VALUE_TYPE_UINT32, len(value_bytes), checksum)
    return hdr + value_bytes


def build_process_uplink_payload(sequence: int, config_payload: bytes, flags: int = 0) -> bytes:
    if len(config_payload) > UPLINK_APP_MAX_PAYLOAD_LENGTH:
        raise ValueError(f"config payload too large: {len(config_payload)}")
    crc_input = struct.pack("<BBBBH", UPLINK_PROTOCOL_VERSION, UPLINK_CLASS_CONFIG,
                            len(config_payload), flags, sequence) + config_payload
    checksum = crc16_ccitt(crc_input)
    pad = bytes(UPLINK_APP_MAX_PAYLOAD_LENGTH - len(config_payload))
    return struct.pack("<BBBBHH",
                       UPLINK_PROTOCOL_VERSION, UPLINK_CLASS_CONFIG,
                       len(config_payload), flags, sequence, checksum) + config_payload + pad


def build_lora_frame(sequence: int, config_payload: bytes, flags: int = 0) -> str:
    payload_hex = config_payload.hex().upper()
    canonical = f"UP,{UPLINK_PROTOCOL_VERSION},{UPLINK_CLASS_CONFIG},{sequence},{flags},{payload_hex}"
    crc = crc16_ccitt(canonical.encode("ascii"))
    return f"{canonical},{crc:04X}"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Send CONFIG uplink command to cFS uplink_app."
    )
    parser.add_argument(
        "target",
        choices=["cfs_core", "mavlink_bridge"],
        help="Target app: cfs_core (scope=1) or mavlink_bridge (scope=2)",
    )
    parser.add_argument("param", help="Parameter name (see --list)")
    parser.add_argument("value", type=int, help="New value (uint32)")
    parser.add_argument("--host", default="127.0.0.1", help="cFS UDP host")
    parser.add_argument("--port", type=int, default=1234, help="cFS UDP port")
    parser.add_argument("--sequence", type=int, default=1, help="Uplink sequence number")
    parser.add_argument(
        "--transport",
        choices=["udp", "lora-text", "lora-serial"],
        default="udp",
        help="udp sends to cFS, lora-text prints LoRa frame, lora-serial sends via serial port",
    )
    parser.add_argument("--serial-path", default=None, help="Serial port for lora-serial")
    parser.add_argument("--baudrate", type=int, default=57600, help="Serial baudrate")
    parser.add_argument("--list", action="store_true", help="List available parameters and exit")
    args = parser.parse_args()

    if args.target == "cfs_core":
        scope = SCOPE_CFS_CORE_APP
        params = CFS_CORE_PARAMS
    else:
        scope = SCOPE_MAVLINK_BRIDGE
        params = MAVLINK_BRIDGE_PARAMS

    if args.list:
        print(f"Parameters for target='{args.target}' (scope={scope}):")
        for name, pid in params.items():
            print(f"  {name} (id={pid})")
        return 0

    if args.param not in params:
        print(f"error: unknown param '{args.param}' for target '{args.target}'", file=sys.stderr)
        print(f"available: {', '.join(params)}", file=sys.stderr)
        return 1

    param_id = params[args.param]
    config_payload = build_config_payload(scope, param_id, args.value)

    print(
        f"config: target={args.target} scope={scope} param={args.param}(id={param_id}) "
        f"value={args.value} seq={args.sequence} transport={args.transport}"
    )

    if args.transport == "udp":
        uplink_payload = build_process_uplink_payload(args.sequence, config_payload)
        packet = build_command_packet(UPLINK_APP_PROCESS_UPLINK_CC, uplink_payload)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.sendto(packet, (args.host, args.port))
        print(f"sent {len(packet)} bytes to {args.host}:{args.port}")

    elif args.transport == "lora-text":
        frame = build_lora_frame(args.sequence, config_payload)
        print(frame)

    else:
        if not args.serial_path:
            print("error: --serial-path required for lora-serial", file=sys.stderr)
            return 1
        if serial is None:
            print("error: pyserial required for lora-serial: pip install pyserial", file=sys.stderr)
            return 1
        frame = build_lora_frame(args.sequence, config_payload)
        print(f"sending via {args.serial_path} at {args.baudrate} baud")
        print(f"frame: {frame}")
        with serial.Serial(args.serial_path, args.baudrate, timeout=2.0) as port:
            time.sleep(0.05)
            port.write((frame + "\n").encode("ascii"))
            port.flush()
        print("sent")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
