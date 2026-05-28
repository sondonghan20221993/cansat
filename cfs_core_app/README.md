# cfs_core_app

FC 상태 입력을 종합해 시스템 헬스를 판단하고 `SYSTEM_HEALTH_MID`를 게시하는 cFS 앱이다. route update를 수신해 내부 캐시에 저장하는 역할도 담당한다.

## MID 인터페이스

| 방향 | 심볼 | 값 | 설명 |
| --- | --- | --- | --- |
| CMD 수신 | `CFS_CORE_APP_CMD_MID` | `0x18C0` | NOOP, RESET_COUNTERS |
| CMD 수신 | `CFS_CORE_APP_SEND_HK_MID` | `0x18C1` | HK 요청 |
| SB 수신 | `MAVLINK_BRIDGE_APP_HK_TLM_MID` | `0x08A0` | Bridge HK 미러 입력 |
| SB 수신 | `FC_EKF_LOCAL_STATE_MID` | `0x1905` | FC local-state 입력 |
| SB 수신 | `FC_ATTITUDE_STATE_MID` | `0x1906` | FC attitude-state 입력 |
| SB 수신 | `FC_GPS_RAW_STATE_MID` | `0x1907` | FC GPS-state 입력 |
| SB 수신 | `FC_EKF_STATUS_MID` | `0x1908` | FC EKF-status 입력 |
| SB 수신 | `ROUTE_UPDATE_MID` | `0x190B` | 경로 갱신 입력 (캐시 저장) |
| 게시 | `CFS_CORE_APP_HK_TLM_MID` | `0x08C0` | HK 텔레메트리 |
| 게시 | `SYSTEM_HEALTH_MID` | `0x1904` | 시스템 헬스 텔레메트리 |

## 구현 기능

- FC 상태 메시지(attitude, local pos, GPS, EKF) 수신 및 내부 캐시 유지
- 각 입력의 freshness/유효성 기반 시스템 헬스 재계산
- `CFS_NOMINAL` / `CFS_DEGRADED` / `CFS_RECOVERY` 상태 게시
- route update(`ROUTE_UPDATE_MID`) 수신 시 mission route 캐시 갱신
- 헬스 상태 구독 메시지 처리 후 즉시 force-publish + 1 Hz periodic publish

## 현재 미구현

- 다른 앱 재시작
- 시리얼 장치 재열기
- 외부 컴포넌트 재설정
- 별도 복구 명령 게시
- 재시작 후 헬스 상태 지속

## 동작 명세 참조

- 구현 기준 상세 명세: `notes/cfs_core_app_behavior_spec.md`
- 시스템 MID 계약: `notes/mission_app_runtime_spec.md` §5.1.1
