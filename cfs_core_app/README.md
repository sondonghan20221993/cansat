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
| SB 수신 | `CONFIG_CMD_MID` | `0x190E` | runtime configuration 적용 |
| SB 수신 | `VIEWPOINT_CMD_MID` | `0x190D` | viewpoint 명령 캐시 저장 |
| 게시 | `CFS_CORE_APP_HK_TLM_MID` | `0x08C0` | HK 텔레메트리 |
| 게시 | `SYSTEM_HEALTH_MID` | `0x1904` | 시스템 헬스 텔레메트리 |

## 구현 기능

- FC 상태 메시지(attitude, local pos, GPS, EKF) 수신 및 내부 캐시 유지
- 각 입력의 freshness/유효성 기반 시스템 헬스 재계산
- `CFS_NOMINAL` / `CFS_DEGRADED` / `CFS_RECOVERY` / `CFS_FAILED` 상태 게시
- route update(`ROUTE_UPDATE_MID`) 수신 시 mission route 캐시 갱신
- 헬스 상태 구독 메시지 처리 후 즉시 force-publish + 1 Hz periodic publish
- bridge timeout 30s 이상 지속 시 `RECOVERY` → `FAILED` 에스컬레이션 (`CFS_CORE_APP_FAILED_ESCALATION_MS`)
- 미래 타임스탬프 거부 (현재 시각 + 5s 초과 시 `TimestampRejectedCount` 증가, EVS EID 9)
- bridge timeout 발생 시 `mavlink_bridge_app` 자동 재시작 (`CFE_ES_RestartApp`, 5s 인터벌, 최대 3회)
- 헬스 상태 파일 지속 (`SaveState`/`LoadState`, `/cf/cfs_core_app_state.bin`, atomic tmp+rename)
- 헬스 상태 전이 EVS 이벤트 (`CFS_CORE_APP_HEALTH_TRANSITION_EID (7)`)
- `SYSTEM_HEALTH_MID` 구독 기반 uplink CLASS_MODE/CLASS_DIAGNOSTIC 블로킹 매트릭스
- `CONFIG_CMD_MID` 수신 시 scope/version/param ID/checksum 검증 후 `ActiveConfig` 즉시 적용 (6개 파라미터: Attitude/Local/GPS/EKF/Bridge timeout, PublishPeriod)
- `VIEWPOINT_CMD_MID` 수신 시 typed 필드를 `ViewpointCmd` 캐시에 저장 (`CFS_CORE_APP_VIEWPOINT_EID` 로깅)

## 미구현

- 시퀀스 갭 또는 중복 감지 (CCSDS 패킷 시퀀스 카운터 검사)
- 시리얼 장치 직접 재열기 (serial 재연결은 `mavlink_bridge_app` 자체 처리)
- 외부 컴포넌트 재설정
- 별도 복구 명령 MID 게시

## 동작 명세 참조

- 구현 기준 상세 명세: `notes/cfs_core_app_behavior_spec.md`
- 시스템 MID 계약: `notes/mission_app_runtime_spec.md` §5.1.1
