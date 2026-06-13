# cfs-telemetry-app

Raspberry Pi에서 동작하는 cFS 기반 UAV 텔레메트리/명령 시스템이다. FC(비행제어기) MAVLink 연결, 지상국 uplink 처리, LoRa 텔레메트리 downlink를 통합한다.

## 아키텍처 개요

```
FC (ArduPilot)
    ↓ UART MAVLink
mavlink_bridge_app   →  FC_ATTITUDE_STATE_MID  (0x1906)
                     →  FC_EKF_LOCAL_STATE_MID (0x1905)
                     →  FC_GPS_RAW_STATE_MID   (0x1907)
                     →  FC_EKF_STATUS_MID      (0x1908)
                     →  MAVLINK_BRIDGE_APP_HK  (0x08A0)
                     ←  ROUTE_UPDATE_MID       (0x190B) → FC MAVLink MISSION upload

cfs_core_app         →  SYSTEM_HEALTH_MID      (0x1904)
                     ←  (위 FC 상태 MID 전부 구독)

lora_tdm_app         ←  FC 상태 MID + SYSTEM_HEALTH_MID
                     →  LoRa serial TX (downlink, TDM)
                     ←  LoRa serial RX (UP frame, TDM 300ms 창)
                     →  UPLINK_APP_CMD_MID     (0x18D0) → uplink_app (UP frame SB 전달)
                     →  LORA_TDM_APP_LINK_STATUS_MID (0x190F)

uplink_app           ←  UPLINK_APP_CMD_MID (lora_tdm_app SB) / UDP (테스트용)
                     →  ROUTE_UPDATE_MID       (0x190B) → cfs_core_app + mavlink_bridge_app
```

## 앱 목록

| 앱 | MID 범위 | 역할 |
| --- | --- | --- |
| `mavlink_bridge_app` | CMD `0x18A0`, HK `0x08A0`, 게시 `0x1905-0x1908` | FC MAVLink 수신·파싱·게시, FC MISSION 업로드 (§22) |
| `cfs_core_app` | CMD `0x18C0`, HK `0x08C0`, 게시 `0x1904` | FC 상태 종합, 헬스 판단, SYSTEM_HEALTH 게시 |
| `uplink_app` | CMD `0x18D0`, HK `0x18D1`, 게시 `0x190A` | 지상국 명령 수신·검증·라우팅 |
| `lora_tdm_app` | CMD `0x18E0`, HK `0x18E1`, 게시 `0x190F` | LoRa serial 독점 TDM — downlink TX + uplink RX → uplink_app SB 전달 |

## 주요 기능 구현 상태

| 기능 | 상태 |
| --- | --- |
| FC MAVLink 수신 (ATTITUDE, LOCAL_POSITION_NED, GPS_RAW_INT, EKF_STATUS_REPORT) | 구현됨 |
| FC UART 재연결 + stale timeout | 구현됨 |
| LoRa 텔레메트리 downlink (serial ASCII) | 구현됨 |
| uplink 패킷 검증 (CRC, 길이, sequence, route geometry) | 구현됨 |
| route update → cfs_core_app 캐시 | 구현됨 |
| route update → FC MAVLink MISSION 업로드 (§22) | 구현됨 |
| FC MISSION 재조회 (MISSION_QUERY_CC) | 구현됨 |
| uplink_app 지속 상태 (SaveState/LoadState, atomic write) | 구현됨 |
| lora_tdm_app LoRa TDM (TX downlink + RX UP frame → uplink_app SB 전달, bridge 프로세스 불필요) | 구현됨 |
| cfs_core_app CFS_FAILED 상태 + bridge 자동 재시작 (최대 3회) | 구현됨 |
| cfs_core_app 헬스 상태 파일 지속 (재시작 후 복원) | 구현됨 |
| cfs_core_app 미래 타임스탬프 거부 | 구현됨 |
| runtime configuration 전달 (uplink → cfs_core_app 검증·적용) | 구현됨 |
| viewpoint payload 검증 (범위·finite·버전) + cfs_core_app 캐시 | 구현됨 |
| DEGRADED 상태 CONFIG/VIEWPOINT 차단, ROUTE_UPDATE 허용 (§18.10.1) | 구현됨 |
| 시퀀스 갭 감지 (cfs_core_app SeqGapCount, SEQ_GAP_EID) | 구현됨 |
| landing route FC 업로드 (mavlink_bridge_app) | 범위 제외 (2026-06-08) |
| MISSION_ITEM_INT 경로 MAV_FRAME_GLOBAL_RELATIVE_ALT 변환 | 범위 제외 (2026-06-08) |
| 미션 업로드 후 자동 시작 (MAV_CMD_DO_SET_MISSION_CURRENT) | 범위 제외 (2026-06-08) |
| viewpoint 수신 후 FC MAVLink 명령 실행 | 범위 제외 (2026-06-07) |

## 디렉터리 구조

```
mavlink_bridge_app/   FC MAVLink 브리지 앱
cfs_core_app/         상태 종합·헬스 관리 앱
uplink_app/           지상국 uplink 처리 앱
lora_tdm_app/         LoRa serial 독점 TDM 앱 (downlink TX + uplink RX)
lora_fc_downlink_app/ [deprecated] lora_tdm_app으로 대체됨
bridge/               Raspberry Pi host-side 브리지
                        lora_uplink_bridge.py     — [deprecated] lora_tdm_app으로 대체됨
                        lora_telemetry_bridge.py  — [deprecated] lora_tdm_app으로 대체됨
                        mavlink_uart_bridge.py    — [deprecated] mavlink_bridge_app으로 대체됨
tools/                개발용 CLI 스크립트
tests/                Python 단위 테스트 + TEST_CASES.md
notes/                설계 문서 및 명세
```

## 관련 프로젝트

| 프로젝트 | 역할 |
| --- | --- |
| `optimalpath` | 이 앱에 입력할 경로를 생성하는 경로 계획 프로젝트 |
| `cansat_2` | 전체 시스템 통합 프로젝트 (이 앱 포함) |

## 빌드 및 테스트

```bash
# 소스 동기화 (WSL → cFS 빌드 트리)
rsync -a --delete mavlink_bridge_app/ ~/cFS_clean/apps/mavlink_bridge_app/
rsync -a --delete cfs_core_app/      ~/cFS_clean/apps/cfs_core_app/
rsync -a --delete uplink_app/        ~/cFS_clean/apps/uplink_app/
rsync -a --delete lora_tdm_app/        ~/cFS_clean/apps/lora_tdm_app/

# 단위 테스트 빌드 및 실행
cd ~/cFS_clean/build-ut
make cfs_core_app_ut && ./coverage-cfs_core_app-testrunner
make uplink_app_ut   && ./coverage-uplink_app-testrunner
make lora_tdm_app_ut && ./coverage-lora_tdm_app-testrunner

# Python 테스트
cd ~/cfs-telemetry-app
python3 -m pytest tests/ -v
```

## 런타임 도구

```bash
# FC mission 조회 (cFS → FC MAVLink MISSION_REQUEST_LIST)
python3 tools/query_fc_mission.py

# route update 전송 (UDP 직접 주입)
python3 tools/uplink_route_update_sender.py --transport udp
```

## 관련 문서

- `notes/mission_app_runtime_spec.md` — 전체 시스템 MID 계약 및 동작 사양
- `notes/cfs_core_app_behavior_spec.md` — cfs_core_app 구현 기준 동작 명세
- `notes/lora_tdm_app_behavior_spec.md` — lora_tdm_app TDM 동작 명세
- `notes/lora_uplink_bridge_design.md` — [deprecated] Python bridge 프로토콜 (참고용)
- `tests/TEST_CASES.md` — 단위 테스트 및 런타임 테스트 케이스 목록
