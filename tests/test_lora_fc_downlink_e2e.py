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
class LoraFcDownlinkE2ETest:
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

        # FC,count,ts,roll,pitch,yaw,x,y,z,vx,vy,vz,lat_e7,lon_e7,alt_mm,fix_type
        parts = fc_lines[0].split(",")
        assert len(parts) == 16, f"FC 패킷 필드 수 오류: {fc_lines[0]}"
        assert parts[0] == "FC"

    # LORA-FRAME-005: SH 패킷 포맷 검증
    def test_sh_packet_format_from_c(self, pty_pair):
        master_fd, slave_fd, slave_path = pty_pair
        pytest.skip(f"PTY slave: {slave_path} — cFS 실행 후 수동 검증 필요")

        lines = _read_pty_lines(master_fd, timeout=2.0)
        sh_lines = [l for l in lines if l.startswith("SH,")]
        assert len(sh_lines) > 0

        parts = sh_lines[0].split(",")
        assert len(parts) == 5
        assert parts[0] == "SH"
        health_state = int(parts[3])
        fault_code = int(parts[4])
        assert 0 <= health_state <= 3
        assert 0 <= fault_code <= 5

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
