"""B 그룹: serial 장애/복구 테스트 (REC-001~004)

cFS 실행 중 serial 장치를 강제로 닫거나 재연결하여
lora_fc_downlink_app / uplink_app의 장애 처리 및 재연결 동작을 검증한다.

실행 방법:
    pytest tests/test_rec_serial.py --cfs

검증 TC:
    - REC-001: LoRa serial open 실패 시 retry
    - REC-002: LoRa serial 실행 중 disconnect → close 후 재연결 시도
    - REC-003: FC serial open 실패 시 retry
    - REC-004: FC heartbeat 끊김 → FC link stale
"""

import os
import pty
import select
import time
import pytest


@pytest.fixture
def pty_pair():
    master_fd, slave_fd = pty.openpty()
    slave_path = os.ttyname(slave_fd)
    yield master_fd, slave_fd, slave_path
    try:
        os.close(master_fd)
    except OSError:
        pass
    try:
        os.close(slave_fd)
    except OSError:
        pass


@pytest.mark.cfs_required
class SerialRecoveryTest:

    # REC-001: LoRa serial open 실패 → cFS 앱 alive 유지 (crash 없음)
    def test_lora_serial_open_failure_no_crash(self):
        """
        존재하지 않는 serial 경로로 cFS 실행 시
        lora_fc_downlink_app이 crash 없이 계속 동작해야 한다.
        """
        pytest.skip("cFS를 LORA_SERIAL_PATH=/dev/nonexistent로 실행 후 검증 필요")

    # REC-002: LoRa serial 실행 중 disconnect → reopen 시도
    def test_lora_serial_disconnect_triggers_reopen(self, pty_pair):
        """
        PTY slave를 cFS LoRa serial로 사용 중 slave fd를 닫으면
        다음 write 시 errno 발생 → close() 후 재열기 시도해야 한다.
        """
        master_fd, slave_fd, slave_path = pty_pair
        pytest.skip(f"PTY slave: {slave_path} — cFS 실행 후 slave close → reopen EVS 로그 확인")

    # REC-003: FC serial open 실패 → mavlink_bridge_app retry
    def test_fc_serial_open_failure_retry(self):
        """
        FC UART serial 없을 때 mavlink_bridge_app이 retry 루프를 유지해야 한다.
        """
        pytest.skip("cFS를 MAVLINK_SERIAL_PATH=/dev/nonexistent로 실행 후 EVS 로그 확인")

    # REC-004: FC heartbeat 끊김 → FC link stale → cfs_core_app RECOVERY
    def test_fc_heartbeat_timeout_triggers_recovery(self):
        """
        mavlink_bridge_app이 FC heartbeat를 일정 시간 못 받으면
        LinkState가 DISCONNECTED로 전환되고
        cfs_core_app이 bridge timeout → RECOVERY를 게시해야 한다.
        """
        pytest.skip("FC heartbeat 차단 후 SYSTEM_HEALTH_MID 구독 확인 필요")

    # REC-002 variant: write 오류 후 LoRa fd 재열기
    def test_lora_write_error_triggers_close_reopen(self, pty_pair):
        """
        PTY slave를 닫은 상태에서 cFS가 write 시도 →
        EAGAIN 이외 errno → fd close() 후 다음 메시지에서 재open 시도.
        """
        master_fd, slave_fd, slave_path = pty_pair
        os.close(slave_fd)  # slave 강제 close

        # cFS가 이 slave_path로 LoRa 포트를 열고 write 시도하면
        # EIO 또는 ENXIO → ServiceLoRa에서 fd close + 재열기 대기
        pytest.skip(f"PTY slave closed: {slave_path} — cFS write error EVS 확인 필요")
