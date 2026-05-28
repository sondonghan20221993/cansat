#!/usr/bin/env python3
"""
Send MISSION_QUERY_CC to mavlink_bridge_app via cFS CI_LAB UDP.

Usage:
    python3 tools/query_fc_mission.py [host] [port]

Default host: 127.0.0.1 (companion computer running cFS)
Default port: 1234       (CI_LAB default for processor 1)

After sending, watch cFS EVS log for:
    MAVLINK_BRIDGE_APP: MISSION_QUERY started
    MAVLINK_BRIDGE_APP: MISSION_COUNT from FC: N waypoints
    MAVLINK_BRIDGE_APP: [wp 0] x=... y=... z=... cmd=...
    MAVLINK_BRIDGE_APP: MISSION download complete: N waypoints
"""

import socket
import struct
import sys

CI_LAB_HOST = "127.0.0.1"
CI_LAB_PORT = 1234

# mavlink_bridge_app command MID (0x18A0) and function code
MAVLINK_BRIDGE_CMD_MID   = 0x18A0
MISSION_QUERY_CC         = 2


def build_cfs_command(mid: int, fc: int) -> bytes:
    """
    Build a minimal cFS v7 (Draco) command packet with no payload.

    CCSDS primary header (6 bytes, big-endian):
      [0-1] stream ID : MID value directly (type=cmd, sec-hdr=1, APID embedded)
      [2-3] sequence  : 0xC000 (standalone packet, count=0)
      [4-5] data len  : number of octets after primary header - 1
                        = secondary_header(2) - 1 = 1

    CCSDS secondary header for commands (2 bytes):
      [0] function code
      [1] checksum : XOR of all bytes including this field must equal 0xFF
    """
    stream_id   = mid
    sequence    = 0xC000
    data_length = 1          # 2 bytes secondary header - 1

    primary   = struct.pack(">HHH", stream_id, sequence, data_length)
    secondary = struct.pack("BB", fc, 0x00)   # placeholder checksum

    packet_no_chk = primary + secondary
    xor_acc = 0
    for b in packet_no_chk:
        xor_acc ^= b
    checksum = 0xFF ^ xor_acc

    return primary + struct.pack("BB", fc, checksum)


def main() -> None:
    host = sys.argv[1] if len(sys.argv) > 1 else CI_LAB_HOST
    port = int(sys.argv[2]) if len(sys.argv) > 2 else CI_LAB_PORT

    packet = build_cfs_command(MAVLINK_BRIDGE_CMD_MID, MISSION_QUERY_CC)

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(packet, (host, port))

    print(f"Sent MISSION_QUERY_CC ({len(packet)} bytes) → {host}:{port}")
    print()
    print("Watch cFS EVS log for results:")
    print("  MAVLINK_BRIDGE_APP: MISSION_QUERY started")
    print("  MAVLINK_BRIDGE_APP: MISSION_COUNT from FC: N waypoints")
    print("  MAVLINK_BRIDGE_APP: [wp 0] x=... y=... z=... cmd=16")
    print("  MAVLINK_BRIDGE_APP: MISSION download complete: N waypoints")
    print()
    print("If 'FC link not connected' appears: mavlink_bridge_app has no heartbeat from FC yet.")
    print("If 'MISSION_QUERY timeout' appears: FC did not respond within 3 seconds.")


if __name__ == "__main__":
    main()
