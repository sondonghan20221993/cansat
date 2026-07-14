#!/usr/bin/env python3
"""
MAVLink mission upload diagnostic tool.

Runs the full mission upload sequence directly via serial (no cFS),
logging every TX/RX frame for comparison with the cFS bridge.

Usage:
    python3 tools/mission_upload_diag.py --port /dev/serial0 --baud 57600
"""
import argparse
import struct
import time
import sys

try:
    import serial
except ModuleNotFoundError:
    print("error: pyserial not installed. Run: pip install pyserial")
    sys.exit(1)

STX_V1 = 0xFE
STX_V2 = 0xFD

MSG_HEARTBEAT          = 0
MSG_MISSION_REQUEST    = 40
MSG_MISSION_ITEM       = 39
MSG_MISSION_COUNT      = 44
MSG_MISSION_CLEAR_ALL  = 45
MSG_MISSION_ACK        = 47
MSG_MISSION_REQUEST_INT = 51
MSG_MISSION_ITEM_INT   = 73

CRC_EXTRA = {
    MSG_MISSION_ITEM:       254,
    MSG_MISSION_REQUEST:    230,
    MSG_MISSION_COUNT:      221,
    MSG_MISSION_CLEAR_ALL:  232,
    MSG_MISSION_ACK:        153,
    MSG_MISSION_REQUEST_INT: 196,
    MSG_MISSION_ITEM_INT:    38,
    MSG_HEARTBEAT:           50,
}

MSG_NAMES = {
    MSG_HEARTBEAT:           "HEARTBEAT",
    MSG_MISSION_REQUEST:     "MISSION_REQUEST",
    MSG_MISSION_ITEM:        "MISSION_ITEM",
    MSG_MISSION_COUNT:       "MISSION_COUNT",
    MSG_MISSION_CLEAR_ALL:   "MISSION_CLEAR_ALL",
    MSG_MISSION_ACK:         "MISSION_ACK",
    MSG_MISSION_REQUEST_INT: "MISSION_REQUEST_INT",
    MSG_MISSION_ITEM_INT:    "MISSION_ITEM_INT",
}

MAV_FRAME_LOCAL_NED   = 1
MAV_CMD_NAV_WAYPOINT  = 16
MISSION_TYPE_MISSION  = 0

SRC_SYSID  = 255
SRC_COMPID = 190

DEFAULT_WAYPOINTS = [
    (0.0, -10.0, 3.0),
    (2.0, -10.0, 3.0),
]

_tx_seq = 0


def _crc_acc(byte, crc):
    tmp = byte ^ (crc & 0xFF)
    tmp ^= (tmp << 4) & 0xFF
    return ((crc >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4)) & 0xFFFF


def _compute_crc(data, crc_extra):
    crc = 0xFFFF
    for b in data:
        crc = _crc_acc(b, crc)
    return _crc_acc(crc_extra, crc)


def _build_v2(msgid, payload):
    global _tx_seq
    seq = _tx_seq & 0xFF
    _tx_seq += 1
    hdr = bytes([
        len(payload), 0, 0, seq,
        SRC_SYSID, SRC_COMPID,
        msgid & 0xFF, (msgid >> 8) & 0xFF, (msgid >> 16) & 0xFF,
    ])
    crc_val = _compute_crc(hdr + payload, CRC_EXTRA[msgid])
    return bytes([STX_V2]) + hdr + payload + struct.pack('<H', crc_val)


def build_mission_clear_all(target_sys, target_comp):
    payload = bytes([target_sys, target_comp, MISSION_TYPE_MISSION])
    return _build_v2(MSG_MISSION_CLEAR_ALL, payload)


def build_mission_count(count, target_sys, target_comp):
    payload = struct.pack('<H', count) + bytes([target_sys, target_comp, MISSION_TYPE_MISSION])
    return _build_v2(MSG_MISSION_COUNT, payload)


def build_mission_item_int(seq, waypoint, target_sys, target_comp):
    x, y, z = waypoint
    payload = bytearray(38)
    struct.pack_into('<i', payload, 16, int(x * 10000))
    struct.pack_into('<i', payload, 20, int(y * 10000))
    struct.pack_into('<f', payload, 24, -z)
    struct.pack_into('<H', payload, 28, seq)
    struct.pack_into('<H', payload, 30, MAV_CMD_NAV_WAYPOINT)
    payload[32] = target_sys
    payload[33] = target_comp
    payload[34] = MAV_FRAME_LOCAL_NED
    payload[36] = 1
    payload[37] = MISSION_TYPE_MISSION
    return _build_v2(MSG_MISSION_ITEM_INT, bytes(payload))


def build_mission_item(seq, waypoint, target_sys, target_comp):
    x, y, z = waypoint
    payload = bytearray(37)
    struct.pack_into('<f', payload, 16, x)
    struct.pack_into('<f', payload, 20, y)
    struct.pack_into('<f', payload, 24, -z)
    struct.pack_into('<H', payload, 28, seq)
    struct.pack_into('<H', payload, 30, MAV_CMD_NAV_WAYPOINT)
    payload[32] = target_sys
    payload[33] = target_comp
    payload[34] = MAV_FRAME_LOCAL_NED
    payload[36] = 1
    return _build_v2(MSG_MISSION_ITEM, bytes(payload))


class _Parser:
    def __init__(self):
        self._reset()

    def _reset(self):
        self.state = 'STX'
        self.is_v2 = False
        self.payload_len = 0
        self.payload_idx = 0
        self.seq = self.sysid = self.compid = self.msgid = 0
        self.payload = bytearray()
        self.crc_low = 0

    def feed(self, byte):
        if byte in (STX_V1, STX_V2) and self.state == 'STX':
            self._reset()
            self.is_v2 = (byte == STX_V2)
            self.state = 'LEN'
            return None

        s = self.state
        if s == 'STX':
            pass
        elif s == 'LEN':
            self.payload_len = byte
            self.state = 'INCOMPAT' if self.is_v2 else 'SEQ'
        elif s == 'INCOMPAT':
            self.state = 'COMPAT'
        elif s == 'COMPAT':
            self.state = 'SEQ'
        elif s == 'SEQ':
            self.seq = byte
            self.state = 'SYSID'
        elif s == 'SYSID':
            self.sysid = byte
            self.state = 'COMPID'
        elif s == 'COMPID':
            self.compid = byte
            self.state = 'MSGID1'
        elif s == 'MSGID1':
            self.msgid = byte
            self.state = 'MSGID2' if self.is_v2 else ('PAYLOAD' if self.payload_len else 'CRC1')
        elif s == 'MSGID2':
            self.msgid |= byte << 8
            self.state = 'MSGID3'
        elif s == 'MSGID3':
            self.msgid |= byte << 16
            self.state = 'PAYLOAD' if self.payload_len else 'CRC1'
        elif s == 'PAYLOAD':
            self.payload.append(byte)
            self.payload_idx += 1
            if self.payload_idx >= self.payload_len:
                self.state = 'CRC1'
        elif s == 'CRC1':
            self.crc_low = byte
            self.state = 'CRC2'
        elif s == 'CRC2':
            crc_rcv = (byte << 8) | self.crc_low
            msg = dict(msgid=self.msgid, sysid=self.sysid, compid=self.compid,
                       seq=self.seq, payload=bytes(self.payload), crc=crc_rcv,
                       is_v2=self.is_v2)
            self._reset()
            return msg
        return None


def _send(port, frame, label):
    name = MSG_NAMES.get(frame[7] | (frame[8] << 8) | (frame[9] << 16), f"msg{frame[7]}")
    print(f"  TX {label} [{name}] {frame.hex()}")
    port.write(frame)


def _read_until(port, want_msgids, timeout=5.0, show_all=False):
    parser = _Parser()
    deadline = time.time() + timeout
    while time.time() < deadline:
        for b in port.read(256):
            msg = parser.feed(b)
            if msg is None:
                continue
            name = MSG_NAMES.get(msg['msgid'], f"msgid={msg['msgid']}")
            if show_all or msg['msgid'] in want_msgids:
                print(f"  RX {name} sysid={msg['sysid']} compid={msg['compid']} "
                      f"payload={msg['payload'].hex()}")
            if msg['msgid'] in want_msgids:
                return msg
    return None


def main(argv=None):
    parser = argparse.ArgumentParser(description="MAVLink mission upload diagnostic")
    parser.add_argument('--port', default='/dev/serial0')
    parser.add_argument('--baud', type=int, default=57600)
    parser.add_argument('--timeout', type=float, default=5.0, help='per-step timeout seconds')
    parser.add_argument('--show-all', action='store_true', help='print all received frames')
    args = parser.parse_args(argv)

    print(f"[open] {args.port} @ {args.baud} baud  sysid={SRC_SYSID} compid={SRC_COMPID}")
    port = serial.Serial(args.port, args.baud, timeout=0.05)

    print("\n[1] waiting for FC heartbeat ...")
    msg = _read_until(port, {MSG_HEARTBEAT}, timeout=10.0, show_all=False)
    if not msg:
        print("  ERROR: no heartbeat")
        port.close()
        return 1
    tgt_sys  = msg['sysid']
    tgt_comp = msg['compid']
    print(f"  heartbeat: sysid={tgt_sys} compid={tgt_comp}")

    print(f"\n[2] MISSION_CLEAR_ALL → target={tgt_sys}/{tgt_comp}")
    _send(port, build_mission_clear_all(tgt_sys, tgt_comp), "CLEAR_ALL")
    msg = _read_until(port, {MSG_MISSION_ACK}, timeout=args.timeout, show_all=args.show_all)
    if not msg:
        print("  WARNING: no MISSION_ACK for CLEAR_ALL (continuing)")
    else:
        result = msg['payload'][2] if len(msg['payload']) >= 3 else '?'
        print(f"  MISSION_ACK result={result}")

    wps = DEFAULT_WAYPOINTS
    print(f"\n[3] MISSION_COUNT({len(wps)}) → target={tgt_sys}/{tgt_comp}")
    _send(port, build_mission_count(len(wps), tgt_sys, tgt_comp), "COUNT")
    msg = _read_until(port, {MSG_MISSION_REQUEST_INT, MSG_MISSION_REQUEST, MSG_MISSION_ACK},
                      timeout=args.timeout, show_all=args.show_all)
    if not msg:
        print("  ERROR: no response to MISSION_COUNT (timeout)")
        port.close()
        return 1

    while msg and msg['msgid'] in (MSG_MISSION_REQUEST_INT, MSG_MISSION_REQUEST):
        req_seq = struct.unpack_from('<H', msg['payload'], 0)[0] if len(msg['payload']) >= 2 else 0
        use_int = (msg['msgid'] == MSG_MISSION_REQUEST_INT)
        print(f"\n[4] FC requests seq={req_seq} via {'MISSION_REQUEST_INT' if use_int else 'MISSION_REQUEST'}")

        if req_seq < len(wps):
            if use_int:
                frame = build_mission_item_int(req_seq, wps[req_seq], tgt_sys, tgt_comp)
                _send(port, frame, f"ITEM_INT[{req_seq}]")
            else:
                frame = build_mission_item(req_seq, wps[req_seq], tgt_sys, tgt_comp)
                _send(port, frame, f"ITEM[{req_seq}]")
        else:
            print(f"  WARNING: requested seq={req_seq} out of range")

        msg = _read_until(port, {MSG_MISSION_REQUEST_INT, MSG_MISSION_REQUEST, MSG_MISSION_ACK},
                          timeout=args.timeout, show_all=args.show_all)

    if msg and msg['msgid'] == MSG_MISSION_ACK:
        result = msg['payload'][2] if len(msg['payload']) >= 3 else msg['payload'][0] if msg['payload'] else '?'
        ok = (result == 0)
        print(f"\n{'✓' if ok else '✗'} MISSION_ACK result={result} ({'ACCEPTED' if ok else 'REJECTED'})")
        port.close()
        return 0 if ok else 1
    else:
        print("\n✗ no MISSION_ACK (timeout)")
        port.close()
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
