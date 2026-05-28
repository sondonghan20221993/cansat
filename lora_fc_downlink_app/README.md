# lora_fc_downlink_app

cFS Software Bus에서 FC 상태 메시지와 시스템 헬스를 구독하고 LoRa 시리얼로 텔레메트리를 전송하는 cFS 앱이다.

## MID 인터페이스

| 방향 | 심볼 | 값 | 설명 |
| --- | --- | --- | --- |
| CMD 수신 | `LORA_FC_DOWNLINK_APP_CMD_MID` | `0x18B0` (topic-id 기반) | NOOP, RESET_COUNTERS |
| CMD 수신 | `LORA_FC_DOWNLINK_APP_SEND_HK_MID` | `0x18B1` (topic-id 기반) | HK 요청 |
| SB 수신 | `FC_ATTITUDE_STATE_MID` | `0x1906` | attitude (roll/pitch/yaw) |
| SB 수신 | `FC_EKF_LOCAL_STATE_MID` | `0x1905` | local position/velocity |
| SB 수신 | `FC_GPS_RAW_STATE_MID` | `0x1907` | GPS raw 상태 |
| SB 수신 | `FC_EKF_STATUS_MID` | `0x1908` | EKF health flags |
| SB 수신 | `SYSTEM_HEALTH_MID` | `0x1904` | 시스템 헬스 |
| 게시 | `LORA_FC_DOWNLINK_APP_HK_TLM_MID` | topic-id 기반 | HK 텔레메트리 |

## 구현 기능

- 구독된 FC 상태 메시지 수신 시 downlink 패킷 구성
- LoRa 시리얼 포트로 텔레메트리 전송
- FC state 패킷 타입(1)과 system health 패킷 타입(2) 구분 전송
- HK 1 Hz 주기 publish

## 패킷 구조

| 패킷 타입 | 포함 데이터 |
| --- | --- |
| FC State (타입 1) | attitude (roll/pitch/yaw), local pos (x/y/z), velocity (vx/vy/vz), GPS (lat/lon/alt/fix), EKF flags |
| System Health (타입 2) | cFS 헬스 상태, 각 입력 유효성, 오류 코드 |

## 동작 명세 참조

- 시스템 MID 계약: `notes/mission_app_runtime_spec.md` §5.1.1
