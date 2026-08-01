# 페이로드 명세 (QGC/MAVLink 기반, 2026-08-02)

`docs/02-system-architecture.md`의 후속 문서. LR24-F 투명 시리얼 링크 위에서
QGC가 수신할 MAVLink 메시지 구성과, 구 DL2 커스텀 프레임과의 대응 관계를 정리한다.

## 대역폭 전제

LR24-F air rate 2.4KB/s, UART 57600 — 구 DL2 기본 프레임(50B, SysTime 포함 58B)을
5Hz(200ms 주기)로 실측 통과시킨 이력 있음. MAVLink 표준 메시지는 필드가 더 많고
헤더 오버헤드도 있어 전 메시지를 동일 레이트로 뿌리면 대역폭 초과 가능 —
메시지별로 `MAV_CMD_SET_MESSAGE_INTERVAL`로 레이트를 분리한다.

## 구 DL2 필드 → MAVLink 대응

| 구 DL2 필드 | 내용 | MAVLink 메시지 | 비고 |
| --- | --- | --- | --- |
| roll, pitch, yaw | 자세 | `ATTITUDE` | |
| x, y, z, vx, vy, vz | 로컬 위치/속도 | `LOCAL_POSITION_NED` | |
| lat, lon, alt_mm, fix, sats | GPS | `GPS_RAW_INT` | |
| sys_time_unix_usec | GPS UNIX epoch | `SYSTEM_TIME` | |
| health, fault | 커스텀 헬스 판정 | 대응 없음 | `cfs_core_app` 로직, 컴패니언 컴퓨터 전제라 이관 불가 |
| linkstate | LoRa 링크 상태 | `RADIO_STATUS` (라디오가 지원 시) 또는 QGC heartbeat 타임아웃 표시 | LR24-F 자체 지원 여부 확인 필요 |
| seq, ufb, uplink_last_seq, uplink_boot_count | 커스텀 링크 프로토콜(재전송/ACK) | 불필요 | MAVLink 시퀀스 번호로 대체 |
| waypoint readback | 미션 회수 | `MISSION_REQUEST_LIST` 등 (QGC 내장) | QGC "Plan → Download from vehicle"로 대체 |

## 신규 추가 페이로드 (DL2엔 없었으나 QGC 표준으로 쉽게 확보 가능)

| 메시지 | 내용 | 추천 레이트 | 사유 |
| --- | --- | --- | --- |
| `BATTERY_STATUS` | 전압/전류/잔량 | 1Hz | DL2에 없었던 항목, 비행 안전상 필수 |
| `VFR_HUD` | throttle/climb rate/groundspeed | 2Hz | 조종 판단 보조 |
| `RC_CHANNELS` | RC 채널값/RSSI | 1Hz | 조종기 링크 품질 확인 |
| `STATUSTEXT` | PX4 경고/에러 텍스트 | 이벤트 발생 시 | `cfs_core_app` health 판정 공백을 부분적으로 대체 |
| `EKF_STATUS_REPORT` | EKF 신뢰도 | 1Hz | mavlink_bridge_app이 원래 수신하던 항목, DL2엔 미포함이었음 |

## 레이트 배분안 (초안)

| 항목 | 레이트 |
| --- | --- |
| ATTITUDE, LOCAL_POSITION_NED, GPS_RAW_INT | 5Hz (구 DL2와 동일 유지) |
| BATTERY_STATUS, VFR_HUD, RC_CHANNELS, EKF_STATUS_REPORT | 1~2Hz |
| SYSTEM_TIME | 1Hz |
| STATUSTEXT | 이벤트 기반 (레이트 설정 대상 아님) |

실측 후 2.4KB/s 초과 시 저빈도 항목부터 레이트 하향.

## 확인 필요

- LR24-F 투명 모드 통과 시 MAVLink 메시지 프레이밍(0xFD 등)이 그대로 유지되는지 실측.
- 5Hz 자세/위치 + 1~2Hz 보조 메시지 합산 에어타임이 2.4KB/s 내에 들어오는지 실측
  (구 DL2 50~58B@5Hz ≈ 250~290B/s 대비 MAVLink 표준 메시지는 페이로드가 더 큼).
