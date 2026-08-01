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

## 대역폭 예산 (계산치, 실측 전)

MAVLink v2 프레임 오버헤드 12B(STX/LEN/INCOMPAT/COMPAT/SEQ/SYSID/COMPID/MSGID×3/CRC×2,
서명 없음) + 메시지별 payload. LR24-F 예산 2.4KB/s = 2400B/s.

| 메시지 | payload(B) | 프레임 크기(B) | 레이트 | 대역폭(B/s) |
| --- | --- | --- | --- | --- |
| `HEARTBEAT` | 9 | 21 | 1Hz | 21 |
| `ATTITUDE` | 28 | 40 | 5Hz | 200 |
| `LOCAL_POSITION_NED` | 28 | 40 | 5Hz | 200 |
| `GPS_RAW_INT` | 30 | 42 | 5Hz | 210 |
| `SYSTEM_TIME` | 12 | 24 | 1Hz | 24 |
| `BATTERY_STATUS` | 36 | 48 | 1Hz | 48 |
| `VFR_HUD` | 20 | 32 | 2Hz | 64 |
| `RC_CHANNELS` | 42 | 54 | 1Hz | 54 |
| `EKF_STATUS_REPORT` | 26 | 38 | 1Hz | 38 |
| `STATUSTEXT` | ~54 | ~66 | 이벤트성, 예산 제외 | - |
| **합계(기준안)** | | | | **≈ 859 B/s** (2400 B/s의 36%, 여유 ≈ 1541 B/s) |

## 레이트 배분안

| 항목 | 레이트 | 근거 |
| --- | --- | --- |
| ATTITUDE, LOCAL_POSITION_NED, GPS_RAW_INT | **10Hz로 상향** | 기준안(5Hz) 기준 859B/s로 여유 1541B/s 확보 — 세 메시지 5→10Hz 증분 +610B/s해도 합계 ≈ 1469B/s(2400의 61%), 마진 충분 |
| BATTERY_STATUS, VFR_HUD, RC_CHANNELS, EKF_STATUS_REPORT | 1~2Hz 유지 | 안전/보조 정보, 고빈도 불필요 |
| SYSTEM_TIME, HEARTBEAT | 1Hz 유지 | |
| STATUSTEXT | 이벤트 기반 | |

10Hz 상향 후 합계 ≈ **1469 B/s (2400 B/s의 61%)**, 마진 ≈ 931 B/s(39%).
실측 전 계산치이므로 필드테스트에서 실제 패킷 손실률/지연 확인 후 조정.

## 확인 필요

- LR24-F 투명 모드 통과 시 MAVLink 메시지 프레이밍(0xFD 등)이 그대로 유지되는지 실측.
- 위 계산치는 이론값 — 실제 LR24-F 프로토콜 오버헤드(FHSS 호핑, 자체 헤더 등) 포함 시
  유효 처리량이 명목 2.4KB/s보다 낮을 수 있어 실측 필수.
- 10Hz 상향분(자세/위치/GPS) 실기체 soak 테스트로 패킷 손실률 확인.
