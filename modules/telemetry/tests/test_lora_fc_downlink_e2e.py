"""B 그룹: lora_fc_downlink_app C 경로 end-to-end 테스트

cFS가 실행 중인 상태에서 SB mock 메시지를 주입하고
PTY mock serial로 lora_fc_downlink_app의 실제 LoRa 출력을 캡처하여 검증한다.

실행 방법:
    pytest tests/test_lora_fc_downlink_e2e.py --cfs [--cfs-host 127.0.0.1]

검증 TC:
    - LORA-FRAME C 실제 출력: FC/SH 패킷 포맷 확인
    - LORA-FC-006: AttitudeValid + LocalValid → FC 패킷 전송
    - LORA-FC-007: AttitudeValid=0 → FC 패킷 미전송
"""

import os
import pty
import re
import select
import time
import pytest


@pytest.fixture
def pty_pair():
    """PTY master/slave 쌍 생성. slave를 cFS LoRa serial로 사용."""
    master_fd, slave_fd = pty.openpty()
    slave_path = os.ttyname(slave_fd)
    yield master_fd, slave_fd, slave_path
    os.close(master_fd)
    os.close(slave_fd)


def _read_pty_lines(master_fd: int, timeout: float = 0.5) -> list[str]:
    """PTY master에서 timeout 초 동안 수신된 줄 목록 반환."""
    lines = []
    buf = b""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        r, _, _ = select.select([master_fd], [], [], remaining)
        if not r:
            break
        chunk = os.read(master_fd, 256)
        if not chunk:
            break
        buf += chunk
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            lines.append(line.decode("utf-8", errors="replace").strip())
    return lines


@pytest.mark.cfs_required
class TestLoraFcDownlinkE2E:
    """lora_fc_downlink_app C ServiceLoRa() 실제 출력 검증."""

    # LORA-FRAME-001: FC 패킷 포맷 검증
    def test_fc_packet_format_from_c(self, pty_pair):
        """
        PTY slave를 LORA_FC_DOWNLINK_APP_LORA_SERIAL_PATH로 설정한 cFS에서
        attitude + local SB 메시지 주입 → PTY master에서 FC 패킷 캡처.

        사전 조건:
        - cFS가 LORA_FC_DOWNLINK_APP_LORA_SERIAL_PATH=<slave_path>로 실행 중
        - attitude + local valid SB 메시지가 주입됨
        """
        master_fd, slave_fd, slave_path = pty_pair
        pytest.skip(f"PTY slave: {slave_path} — cFS 실행 후 수동 검증 필요")

        lines = _read_pty_lines(master_fd, timeout=2.0)
        fc_lines = [l for l in lines if l.startswith("FC,")]
        assert len(fc_lines) > 0, f"FC 패킷 미수신. lines={lines}"

        # FC,count,ts,roll,pitch,yaw,x,y,z,vx,vy,vz,lat_e7,lon_e7,alt_mm,fix_type,ufb
        parts = fc_lines[0].split(",")
        assert len(parts) == 17, f"FC 패킷 필드 수 오류: {fc_lines[0]}"
        assert parts[0] == "FC"

    # LORA-FRAME-005: SH 패킷 포맷 검증
    def test_sh_packet_format_from_c(self, pty_pair):
        master_fd, slave_fd, slave_path = pty_pair
        pytest.skip(f"PTY slave: {slave_path} — cFS 실행 후 수동 검증 필요")

        lines = _read_pty_lines(master_fd, timeout=2.0)
        sh_lines = [l for l in lines if l.startswith("SH,")]
        assert len(sh_lines) > 0

        # SH,seq,ts,health_state,fault_code,link_state,ufb
        parts = sh_lines[0].split(",")
        assert len(parts) == 7, f"SH 패킷 필드 수 오류: {sh_lines[0]}"
        assert parts[0] == "SH"
        health_state = int(parts[3])
        fault_code = int(parts[4])
        link_state = int(parts[5])
        ufb = int(parts[6])
        assert 0 <= health_state <= 3
        assert 0 <= fault_code <= 5
        assert link_state in (0, 1, 2)
        assert ufb in (0, 1, 2)

    # LORA-FC-006: AttitudeValid && LocalValid → FC 패킷 전송
    def test_fc_sent_when_both_valid(self, pty_pair):
        master_fd, slave_fd, slave_path = pty_pair
        pytest.skip("cFS attitude + local SB 주입 환경 필요")

    # LORA-FC-007: AttitudeValid=0 → FC 패킷 미전송
    def test_fc_not_sent_when_attitude_invalid(self, pty_pair):
        master_fd, slave_fd, slave_path = pty_pair
        pytest.skip("cFS SB 주입 환경 필요")

    # LORA-FRAME-006: seq 단조 증가
    def test_fc_seq_monotonic_from_c(self, pty_pair):
        master_fd, slave_fd, slave_path = pty_pair
        pytest.skip("cFS 실행 환경 필요")

        lines = _read_pty_lines(master_fd, timeout=3.0)
        fc_lines = [l for l in lines if l.startswith("FC,")]
        assert len(fc_lines) >= 2

        counts = [int(l.split(",")[1]) for l in fc_lines]
        for i in range(1, len(counts)):
            assert counts[i] > counts[i - 1], f"seq 역행: {counts}"

    # LORA-FC-008: 500ms rate limit — 0.5초 내 중복 TX 없음
    def test_rate_limit_500ms(self, pty_pair):
        """
        SB 메시지를 100ms 간격으로 연속 주입해도
        LoRa TX는 500ms 이상 간격으로만 발생해야 한다.
        검증: 연속 10개 패킷 사이 최소 간격 >= 450ms (5% 허용).
        """
        master_fd, slave_fd, slave_path = pty_pair
        pytest.skip("cFS SB 주입 + PTY 환경 필요")

        lines_with_ts: list[tuple[float, str]] = []
        deadline = time.monotonic() + 5.0
        buf = b""
        while time.monotonic() < deadline:
            r, _, _ = select.select([master_fd], [], [], 0.1)
            if r:
                chunk = os.read(master_fd, 256)
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    decoded = line.decode("utf-8", errors="replace").strip()
                    if decoded.startswith(("FC,", "SH,")):
                        lines_with_ts.append((time.monotonic(), decoded))

        assert len(lines_with_ts) >= 2, "TX 패킷 2개 이상 필요"
        gaps_ms = [
            (lines_with_ts[i][0] - lines_with_ts[i - 1][0]) * 1000
            for i in range(1, len(lines_with_ts))
        ]
        for gap in gaps_ms:
            assert gap >= 450.0, f"rate limit 위반: {gap:.1f}ms < 450ms"

    # LORA-FC-009: TDM RX window — TX 후 HB 수신 시 HbLinkValid 갱신
    def test_tdm_rx_window_hb_detection(self, pty_pair):
        """
        TX 직후 PTY master에서 HB 응답을 300ms 이내에 쓰면
        cFS lora_fc_downlink_app의 HbLinkValid=1, HbLastRxMs 갱신 확인.
        검증: cFS 텔레메트리 MID에서 HbLinkValid 필드 읽기 (수동).
        """
        master_fd, slave_fd, slave_path = pty_pair
        pytest.skip("cFS PTY + HB MID 텔레메트리 읽기 환경 필요")

        # TX 발생 후 150ms 내에 HB 프레임 주입
        time.sleep(0.05)
        hb_frame = b"HB,1,12345,0\n"
        os.write(master_fd, hb_frame)
        time.sleep(0.4)
        # TODO: CFE_EVS 또는 SB 텔레메트리에서 HbLinkValid=1 확인

    # LORA-FC-010: FC/SH 교대 — 짝수 TX → FC, 홀수 TX → SH
    def test_fc_sh_alternation_from_c(self, pty_pair):
        """
        lora_fc_downlink_app ServiceLoRa() 교대 규칙:
          DownlinkSeq % 2 == 0 → FC 전송
          DownlinkSeq % 2 == 1 → SH 전송
        연속 4개 패킷이 FC,SH,FC,SH 순서인지 검증.
        """
        master_fd, slave_fd, slave_path = pty_pair
        pytest.skip("cFS 실행 환경 필요")

        lines = _read_pty_lines(master_fd, timeout=4.0)
        typed = [l[:2] for l in lines if l.startswith(("FC,", "SH,"))]
        assert len(typed) >= 4, f"패킷 4개 이상 필요: {typed}"
        for i, t in enumerate(typed[:6]):
            expected = "FC" if i % 2 == 0 else "SH"
            assert t == expected, f"인덱스 {i}: 기대={expected}, 실제={t}, 전체={typed}"

    # LORA-FC-011: FC ufb 필드 — uplink 미수신 시 0
    def test_fc_ufb_zero_when_no_uplink(self, pty_pair):
        """
        uplink 명령이 없는 상태에서 FC 패킷의 ufb 필드는 0이어야 한다.
        """
        master_fd, slave_fd, slave_path = pty_pair
        pytest.skip("cFS 실행 환경 필요")

        lines = _read_pty_lines(master_fd, timeout=2.0)
        fc_lines = [l for l in lines if l.startswith("FC,")]
        assert len(fc_lines) > 0
        for line in fc_lines:
            parts = line.split(",")
            assert len(parts) == 17
            assert int(parts[16]) == 0, f"ufb!=0 when no uplink: {line}"

    # LORA-FC-012: SH 패킷 link_state 및 ufb 기본값
    def test_sh_default_fields_from_c(self, pty_pair):
        """
        정상 동작 시 SH 패킷: link_state=0 (또는 구현 정의), ufb=0.
        """
        master_fd, slave_fd, slave_path = pty_pair
        pytest.skip("cFS 실행 환경 필요")

        lines = _read_pty_lines(master_fd, timeout=3.0)
        sh_lines = [l for l in lines if l.startswith("SH,")]
        assert len(sh_lines) > 0
        for line in sh_lines:
            parts = line.split(",")
            assert len(parts) == 7
            ufb = int(parts[6])
            assert ufb == 0, f"ufb!=0 when no uplink: {line}"
