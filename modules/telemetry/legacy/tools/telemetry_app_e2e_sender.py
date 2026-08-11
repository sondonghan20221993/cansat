#!/usr/bin/env python3
import socket
import struct
import sys

MONITOR_MID = 0x0847
SEND_HK_MID = 0x1883


def ccsds_primary(mid: int, size: int) -> bytes:
    return struct.pack(">HHH", mid, 0xC000, size - 7)


def send_packet(packet: bytes, host: str = "127.0.0.1", port: int = 1234) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(packet, (host, port))


def send_monitor(active_transport_id: int, valid: int, update_age_ms: int) -> None:
    header = ccsds_primary(MONITOR_MID, 24) + (b"\x00" * 10)
    payload = struct.pack("<BBHI", active_transport_id, valid, 0, update_age_ms)
    packet = header + payload
    print(
        f"send monitor mid=0x{MONITOR_MID:04x} active_transport_id={active_transport_id} "
        f"valid={valid} update_age_ms={update_age_ms}"
    )
    send_packet(packet)


def send_hk() -> None:
    packet = ccsds_primary(SEND_HK_MID, 8) + b"\x00\x00"
    print(f"send hk_request mid=0x{SEND_HK_MID:04x}")
    send_packet(packet)


def main() -> int:
    if len(sys.argv) == 2 and sys.argv[1] == "hk":
        send_hk()
        return 0

    if len(sys.argv) == 5 and sys.argv[1] == "monitor":
        send_monitor(int(sys.argv[2], 0), int(sys.argv[3], 0), int(sys.argv[4], 0))
        return 0

    print(f"usage: {sys.argv[0]} hk | monitor <active_transport_id> <valid> <update_age_ms>", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
