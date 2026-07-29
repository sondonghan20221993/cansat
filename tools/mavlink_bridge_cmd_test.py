#!/usr/bin/env python3
"""
Send mavlink_bridge_app ground commands directly via cFS CI_LAB UDP,
bypassing the uplink_app/lora_tdm_app chain entirely. Useful for bench
testing against a connected FC when no LoRa/telemetry link is available.

Usage:
    python3 tools/mavlink_bridge_cmd_test.py noop
    python3 tools/mavlink_bridge_cmd_test.py reset_counters
    python3 tools/mavlink_bridge_cmd_test.py parser_reset
    python3 tools/mavlink_bridge_cmd_test.py serial_reconnect
    python3 tools/mavlink_bridge_cmd_test.py set_flight_mode <mode> [waypoint_start_index]
        mode: 0=HOVER 1=WAYPOINT 2=LAND
    python3 tools/mavlink_bridge_cmd_test.py set_flight_mode_badlen
        Sends an undersized SET_FLIGHT_MODE_CC payload (missing 2 bytes) to
        verify VerifyCmdLength rejects it (regression test for the buffer
        overread found in the 2026-07-29 audit).

Default host/port: 127.0.0.1:1234 (CI_LAB default for processor 1), override
with a trailing "host port" pair.

Watch cFS EVS log (journalctl -u cfs.service -f) for:
    MAVLINK_BRIDGE_APP: NOOP command
    MAVLINK_BRIDGE_APP: Reset counters command
    MAVLINK_BRIDGE_APP: parser reset (ground-triggered via cfs_core_app RECOVERY)
    MAVLINK_BRIDGE_APP: opened serial path ... / serial reconnect (...)
    MAVLINK_BRIDGE_APP: flight mode set mode=<n> waypoint_idx=<n> seq=<n>
    MAVLINK_BRIDGE_APP: Invalid cmd length expected=<n> actual=<n>   (badlen case)
"""
import socket
import struct
import sys

CI_LAB_HOST = "127.0.0.1"
CI_LAB_PORT = 1234

MAVLINK_BRIDGE_CMD_MID = 0x18A0

CC = {
    "noop": 0,
    "reset_counters": 1,
    "parser_reset": 3,
    "serial_reconnect": 4,
    "set_flight_mode": 5,
    "set_flight_mode_badlen": 5,
}


def build_cfs_command(mid: int, fc: int, payload: bytes = b"") -> bytes:
    """Build a cFS v7 (Draco) command packet (see query_fc_mission.py for the
    no-payload derivation; this extends it with an optional payload)."""
    stream_id = mid
    sequence = 0xC000
    data_length = 2 + len(payload) - 1

    primary = struct.pack(">HHH", stream_id, sequence, data_length)
    body = struct.pack("BB", fc, 0x00) + payload

    xor_acc = 0
    for b in primary + body:
        xor_acc ^= b
    checksum = 0xFF ^ xor_acc

    body = struct.pack("BB", fc, checksum) + payload
    return primary + body


def main() -> None:
    if len(sys.argv) < 2 or sys.argv[1] not in CC:
        print(__doc__)
        sys.exit(1)

    cmd = sys.argv[1]
    rest = sys.argv[2:]

    payload = b""
    if cmd == "set_flight_mode":
        mode = int(rest[0]) if len(rest) > 0 and rest[0].isdigit() else 0
        wp_idx = int(rest[1]) if len(rest) > 1 and rest[1].isdigit() else 0
        # SourceSequence(u16 LE) + FlightMode(u8) + WaypointStartIndex(u8)
        payload = struct.pack("<HBB", 4242, mode, wp_idx)
        rest = [a for a in rest if not a.isdigit()]
    elif cmd == "set_flight_mode_badlen":
        # Only SourceSequence -- missing FlightMode/WaypointStartIndex on purpose.
        payload = struct.pack("<H", 9999)

    host = rest[0] if len(rest) > 0 else CI_LAB_HOST
    port = int(rest[1]) if len(rest) > 1 else CI_LAB_PORT

    packet = build_cfs_command(MAVLINK_BRIDGE_CMD_MID, CC[cmd], payload)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(packet, (host, port))

    print(f"Sent {cmd} ({len(packet)} bytes, payload={payload.hex()}) -> {host}:{port}")


if __name__ == "__main__":
    main()
