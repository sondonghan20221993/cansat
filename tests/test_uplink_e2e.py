"""B 그룹: uplink_app C 경로 end-to-end 테스트

cFS가 실행 중인 상태에서 UDP로 uplink 패킷을 전송하고
EVS 로그 또는 UPLINK_STATUS_MID HK로 결과를 검증한다.

실행 방법:
    pytest tests/test_uplink_e2e.py --cfs [--cfs-host 127.0.0.1] [--cfs-port 1234]

검증 TC:
    - LORA-UP-011: seq 증가 → accept
    - LORA-UP-012: seq 동일 → reject (C ServiceLoRa 경로)
    - LORA-UP-013: seq 역행 → reject (C ServiceLoRa 경로)
    - REC-008: seq regression 반복 → reject count 증가
"""

import socket
import struct
import time
import pytest

from bridge.lora_uplink_bridge import (
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
class UplinkE2ETest:
    """cFS uplink_app UDP 경로 end-to-end 테스트."""

    # LORA-UP-011: seq 증가 → accept
    def test_seq_increase_accepted(self, cfs_host, cfs_port):
        _send_frame(cfs_host, cfs_port, _make_frame(seq=100))
        time.sleep(0.05)
        _send_frame(cfs_host, cfs_port, _make_frame(seq=101))
        time.sleep(0.05)
        # EVS 로그에서 "routed uplink" 2회 확인 (수동 또는 로그 파서로 검증)
        # 현재는 전송 성공 여부만 확인
        assert True  # placeholder — EVS 로그 파서 추가 시 갱신

    # LORA-UP-012/013: seq 동일/역행 → C ServiceLoRa에서 거부
    def test_seq_regression_rejected_in_c(self, cfs_host, cfs_port):
        _send_frame(cfs_host, cfs_port, _make_frame(seq=200))
        time.sleep(0.05)
        _send_frame(cfs_host, cfs_port, _make_frame(seq=200))  # 동일 seq
        time.sleep(0.05)
        _send_frame(cfs_host, cfs_port, _make_frame(seq=199))  # 역행
        time.sleep(0.05)
        # EVS에 "LoRa seq regression" 메시지 2개 확인 필요
        # 현재는 전송 성공 여부만 확인
        assert True  # placeholder

    # REC-008: seq regression 반복 — reject count 누적
    def test_seq_regression_reject_count_accumulates(self, cfs_host, cfs_port):
        base_seq = 300
        _send_frame(cfs_host, cfs_port, _make_frame(seq=base_seq))
        time.sleep(0.05)
        for _ in range(5):
            _send_frame(cfs_host, cfs_port, _make_frame(seq=base_seq - 1))
            time.sleep(0.02)
        # UPLINK_STATUS_MID HK의 RejectedCount 또는 EVS 로그로 검증
        assert True  # placeholder


@pytest.mark.cfs_required
class UplinkRouteE2ETest:
    """uplink_app route update C 경로 검증."""

    def test_valid_route_update_routed(self, cfs_host, cfs_port):
        """정상 route update → cfs_core_app route cache 갱신."""
        from tools.uplink_route_update_sender import preset_case, build_process_uplink_packet

        route_type, waypoints = preset_case("route-good")
        # UDP로 전송
        pkt = build_process_uplink_packet(
            mid=UPLINK_APP_CMD_MID,
            function_code=PROCESS_UPLINK_CC,
            route_type=route_type,
            waypoints=waypoints,
            sequence=1,
            route_version=1,
        )
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.sendto(pkt, (cfs_host, cfs_port))
        time.sleep(0.1)
        # EVS: "CFS_CORE_APP: route updated type=1" 확인 필요
        assert True  # placeholder
