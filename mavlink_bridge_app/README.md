# mavlink_bridge_app

ArduPilot 호환 비행제어기(FC)와의 MAVLink 통신을 담당하는 cFS 앱이다. FC에서 들어오는 MAVLink 메시지를 파싱해 cFS Software Bus에 게시하고, 지상국 route update를 FC로 MAVLink MISSION 프로토콜로 업로드한다.

## MID 인터페이스

| 방향 | 심볼 | 값 | 설명 |
| --- | --- | --- | --- |
| CMD 수신 | `MAVLINK_BRIDGE_APP_CMD_MID` | `0x18A0` | NOOP, RESET_COUNTERS, MISSION_QUERY_CC |
| CMD 수신 | `MAVLINK_BRIDGE_APP_SEND_HK_MID` | `0x18A1` | HK 요청 |
| SB 수신 | `ROUTE_UPDATE_MID` | `0x190B` | 검증된 route update → FC MISSION 업로드 트리거 |
| 게시 | `MAVLINK_BRIDGE_APP_HK_TLM_MID` | `0x08A0` | HK 텔레메트리 |
| 게시 | `FC_EKF_LOCAL_STATE_MID` | `0x1905` | LOCAL_POSITION_NED 또는 GLOBAL_POSITION_INT 기반 위치/속도 |
| 게시 | `FC_ATTITUDE_STATE_MID` | `0x1906` | ATTITUDE 기반 롤/피치/요/속도 |
| 게시 | `FC_GPS_RAW_STATE_MID` | `0x1907` | GPS_RAW_INT 기반 위치/fix/위성 수 |
| 게시 | `FC_EKF_STATUS_MID` | `0x1908` | EKF_STATUS_REPORT 기반 health flags |

## 구현 기능

### MAVLink 수신 (FC → cFS)
- UART 시리얼 포트에서 MAVLink v1/v2 프레임 파싱
- 지원 메시지: `ATTITUDE`, `LOCAL_POSITION_NED`, `GLOBAL_POSITION_INT`, `GPS_RAW_INT`, `EKF_STATUS_REPORT`, `HEARTBEAT`, `COMMAND_ACK`
- CRC 검증 후 cFS SB에 게시
- UART 재연결 (링크 손실/timeout 시 자동 재시도)
- stale timeout: 3초 이상 수신 없으면 출력 TLM `Stale=1`

### FC MISSION 업로드 (§22, ROUTE_UPDATE_MID → FC)
- `ROUTE_UPDATE_MID` 수신 시 MAVLink MISSION upload 프로토콜 시작
- 프로토콜: `MISSION_COUNT` → FC의 `MISSION_REQUEST_INT` 대기 → `MISSION_ITEM_INT` 전송 × N → `MISSION_ACK` 수신
- 좌표 인코딩: x/y = `int32(meters × 10000)`, z = `float` meters (LOCAL_NED 변환 포함)
- 프레임: `MAV_FRAME_LOCAL_NED`, 명령: `MAV_CMD_NAV_WAYPOINT`
- timeout 2000ms, retry 최대 3회
- `MISSION_ACK accepted` 수신 시 success 카운터 및 HK 반영

### FC MISSION 조회 (MISSION_QUERY_CC)
- cFS 명령(`CC=2`)으로 트리거
- MAVLink download 프로토콜: `MISSION_REQUEST_LIST` → FC의 `MISSION_COUNT` → `MISSION_REQUEST_INT(seq)` × N → `MISSION_ITEM_INT` 수신 → `MISSION_ACK`
- 수신된 waypoint를 EVS 이벤트로 출력: `[wp N] x=... y=... z=... cmd=...`
- timeout 3000ms

### LoRa 텔레메트리 송신
- ATTITUDE + LOCAL_POSITION_NED 수신 시 LoRa 시리얼 포트로 ASCII 텔레메트리 전송
- 형식: `FC,<seq>,<timestamp_ms>,<roll>,<pitch>,<yaw>,<x>,<y>,<z>,<vx>,<vy>,<vz>`
- EAGAIN 시 skip (재연결 없음), 일반 write 오류 시 포트 재열기

## 설정 파일

| 파일 | 주요 값 |
| --- | --- |
| `config/default_mavlink_bridge_app_interface_cfg_values.h` | CMD MID, HK MID, FC state MID 값 |
| `config/default_mavlink_bridge_app_msgid_values.h` | HK TLM MID, ROUTE_UPDATE_MID |
| `config/default_mavlink_bridge_app_fcncode_values.h` | NOOP=0, RESET_COUNTERS=1, MISSION_QUERY_CC=2 |
| `config/default_mavlink_bridge_app_internal_cfg_values.h` | SERIAL_PATH, BAUDRATE, LORA_PATH, timeout 값 |

## 동작 명세 참조

- 전체 시스템 MID 계약: `notes/mission_app_runtime_spec.md` §5.1.1
- FC 업로드 프로토콜 상세 (MISSION_ITEM_INT 매핑, z 좌표계, 재시도, FC 호환성 제약): `notes/mavlink_bridge_app_behavior_spec.md`
- 앱 간 책임 분리 및 ROUTE_UPDATE_MID 계약: `notes/cfs_core_app_behavior_spec.md` §22
