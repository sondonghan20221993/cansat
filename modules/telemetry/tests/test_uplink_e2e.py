"""B 그룹: uplink_app C 경로 end-to-end 테스트

cFS가 실행 중인 상태에서 UDP로 uplink 패킷을 전송하고
EVS 로그 또는 UPLINK_STATUS_MID HK로 결과를 검증한다.

실행 방법:
    pytest tests/test_uplink_e2e.py --cfs [--cfs-host 127.0.0.1] [--cfs-port 1234]

검증 TC:
    - LORA-UP-011: seq 증가 → accept
    - LORA-UP-012: seq 동일 → reject (ProcessUplink IsSequenceAccepted)
    - LORA-UP-013: seq 역행 → reject (ProcessUplink IsSequenceAccepted)
    - REC-008: seq regression 반복 → reject count 증가

    주: uplink serial 직접 경로(ServiceLoRa)는 제거됨. LoRa UP 프레임은
    lora_fc_downlink_app이 UPLINK_RAW_MID(0x1909)로 SB publish → uplink_app 구독.
    seq 거부는 영구 LastAcceptedSequence 기반 ProcessUplink에서 수행.
"""

import socket
import struct
import time
import pytest

from legacy.bridge.lora_uplink_bridge import (
    build_process_uplink_payload,
    crc16_ccitt,
    parse_frame_line,
    ParsedUplinkFrame,
)


UPLINK_APP_CMD_MID = 0x18D0
PROCESS_UPLINK_CC = 2
UPLINK_APP_CMD_MID_VALUE = 0x18D0


def _ccsds_primary(mid: int, total_size: int) -> bytes:
    return struct.pack(">HHH", mid, 0xC000, total_size - 7)


def _calc_checksum(packet: bytes) -> int:
    checksum = 0xFF
    for b in packet:
        checksum ^= b
    return checksum


def _build_udp_packet(frame: ParsedUplinkFrame) -> bytes:
    payload = build_process_uplink_payload(frame)
    total = 6 + 2 + len(payload)
    primary = _ccsds_primary(UPLINK_APP_CMD_MID, total)
    secondary = bytearray(2)
    secondary[0] = PROCESS_UPLINK_CC
    pre_checksum = bytes(primary) + bytes(secondary) + payload
    secondary[1] = _calc_checksum(pre_checksum)
    return bytes(primary) + bytes(secondary) + payload


def _send_frame(host: str, port: int, frame: ParsedUplinkFrame) -> None:
    pkt = _build_udp_packet(frame)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(pkt, (host, port))


def _make_frame(seq: int, command_class: int = 2, payload: bytes = b"\x01\x00") -> ParsedUplinkFrame:
    return ParsedUplinkFrame(
        version=1,
        command_class=command_class,
        sequence=seq,
        flags=0,
        payload=payload,
    )


@pytest.mark.cfs_required
class TestUplinkE2E:
    """cFS uplink_app UDP 경로 end-to-end 테스트."""

    # LORA-UP-011: seq 증가 → accept
    def test_seq_increase_accepted(self, cfs_host, cfs_port):
        _send_frame(cfs_host, cfs_port, _make_frame(seq=100))
        time.sleep(0.05)
        _send_frame(cfs_host, cfs_port, _make_frame(seq=101))
        time.sleep(0.05)
        # UDP 전송 성공은 수신/수락을 보장하지 않음 — EVS 로그 또는
        # UPLINK_STATUS_MID HK(AcceptedCount)를 읽는 검증 경로가 아직 없어
        # BL-106 전까지는 "전송했다"만 확인 가능하고 "수락됐다"는 미검증.
        pytest.skip("BL-106: EVS/HK 판독 경로 미구현 — 전송만 확인, 수락 여부 미검증")

    # LORA-UP-012/013: seq 동일/역행 → C ServiceLoRa에서 거부
    def test_seq_regression_rejected_in_c(self, cfs_host, cfs_port):
        _send_frame(cfs_host, cfs_port, _make_frame(seq=200))
        time.sleep(0.05)
        _send_frame(cfs_host, cfs_port, _make_frame(seq=200))  # 동일 seq
        time.sleep(0.05)
        _send_frame(cfs_host, cfs_port, _make_frame(seq=199))  # 역행
        time.sleep(0.05)
        pytest.skip("BL-106: EVS 로그(\"LoRa seq regression\") 판독 경로 미구현")

    # REC-008: seq regression 반복 — reject count 누적
    def test_seq_regression_reject_count_accumulates(self, cfs_host, cfs_port):
        base_seq = 300
        _send_frame(cfs_host, cfs_port, _make_frame(seq=base_seq))
        time.sleep(0.05)
        for _ in range(5):
            _send_frame(cfs_host, cfs_port, _make_frame(seq=base_seq - 1))
            time.sleep(0.02)
        pytest.skip("BL-106: UPLINK_STATUS_MID HK(RejectedCount) 판독 경로 미구현")


@pytest.mark.cfs_required
class TestUplinkRouteE2E:
    """uplink_app route update C 경로 검증."""

    def test_valid_route_update_routed(self, cfs_host, cfs_port):
        """정상 route update → cfs_core_app route cache 갱신 (BL-56 v2 프로토콜)."""
        from tools.uplink_route_update_sender import (
            preset_case, build_route_payload, build_process_uplink_payload,
            build_command_packet, UPLINK_APP_PROCESS_UPLINK_CC,
        )

        route_op, index_or_count, waypoints = preset_case("route-replace-good")
        route_payload = build_route_payload(route_op, index_or_count, waypoints)
        proxy_payload = build_process_uplink_payload(sequence=1, proxy_payload=route_payload)
        pkt = build_command_packet(UPLINK_APP_PROCESS_UPLINK_CC, proxy_payload)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.sendto(pkt, (cfs_host, cfs_port))
        time.sleep(0.1)
        pytest.skip("BL-106: EVS 로그(\"CFS_CORE_APP: route updated op=1\") 판독 경로 미구현")
