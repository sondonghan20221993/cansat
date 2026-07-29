# cfs_core_app 동작 명세

## 1. 목적

이 문서는 현재 이 저장소에서 구현된 `cfs_core_app`의 동작을 정의한다.
코드와 정합된 명세서로, 리뷰, 통합, 테스트 수행에 활용한다.

이 문서는 코드에 존재하지 않는 목표 동작을 정의하지 않는다.
코드와 이 문서가 서로 다르면, 코드를 조사의 원본으로 취급해야 한다.

## 2. 범위

이 명세는 다음을 다룬다.

- `cfs_core_app` 메시지 구독 (상태·경로·viewpoint·config 명령 포함)
- 각 구독 메시지 수신 시 갱신되는 내부 캐시 상태
- timestamp/sequence 유효성 검사
- `SYSTEM_HEALTH_MID` 출력 필드(per-input 상태 포함) 및 게시 타이밍
- 헬스 상태 분류 로직 (NOMINAL/DEGRADED/RECOVERY/FAILED)
- bridge, EKF, local-state, attitude-state 입력의 타임아웃 처리 (GPS는 헬스 비반영, 보고 전용 — §12.7)
- uplink_app / lora_tdm_app HK 수신 타임아웃 감시 (§12.5, §12.6, §13.6, §13.7)
- RECOVERY_CMD_MID / MODE_CMD_MID 수신 처리 (§17)
- bridge 타임아웃 시 `mavlink_bridge_app` 재시작 및 FAILED 에스컬레이션
- `CONFIG_CMD_MID` 런타임 파라미터 적용 (pending/active 이중버퍼)
- `VIEWPOINT_CMD_MID` 캐시
- 헬스 상태 파일 영속화 및 재시작 복원
- 시작 후 및 입력 손실 후 동작
- 기존 단위 테스트 커버리지 및 런타임 검증 권고

이 명세는 다음을 다루지 않는다.

- `mavlink_bridge_app` 내부 파싱 규칙
- `lora_fc_downlink_app` 패킷 형식
- 다른 앱이나 장치의 능동적 재시작
- `cfs_core_app`에 존재하지 않는 오류 처리 동작

## 3. 참조

- Source: [cfs_core_app.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app.c)
- Source: [cfs_core_app_dispatch.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app_dispatch.c)
- Source: [cfs_core_app_utils.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app_utils.c)
- Source: [cfs_core_app.h](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app.h)
- Source: [cfs_core_app_utils.h](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app_utils.h)
- Config: [default_cfs_core_app_internal_cfg_values.h](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/config/default_cfs_core_app_internal_cfg_values.h)
- Config: [default_cfs_core_app_topicid_values.h](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/config/default_cfs_core_app_topicid_values.h)
- Config: [default_cfs_core_app_msgid_values.h](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/config/default_cfs_core_app_msgid_values.h)
- Config: [default_cfs_core_app_msgdefs.h](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/config/default_cfs_core_app_msgdefs.h)
- Tests: [coveragetest_cfs_core_app_utils.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app_utils.c)

## 4. 책임

`cfs_core_app`의 현재 구현 책임은 다음과 같다.

- bridge HK, FC 상태 메시지, 경로 갱신, viewpoint/config 명령, 앱 명령/HK 메시지 구독
- 수신된 최신 bridge, attitude, local, GPS, EKF, mission-route 데이터 캐시
- 구독된 상태 메시지의 timestamp/sequence 유효성 검사 (미래 timestamp·중복·역행·갭 감지)
- 구독된 상태 메시지 수신 시마다 시스템 헬스 재계산
- 입력이 없을 때 주기적으로 시스템 헬스 재계산
- `SYSTEM_HEALTH_MID` 게시 (per-input 상태 구조 포함)
- bridge 타임아웃 지속 시 `mavlink_bridge_app` 재시작 시도 (5초 쿨다운, 무한 재시도 — BL-38) 및 FAILED 에스컬레이션
- uplink_app·lora_tdm_app HK 수신 시각 추적, 5초 타임아웃 시 DEGRADED 보고 + bridge와 동일 패턴 자동 재시작 (5초 간격, 무한 재시도 — §13.6/§13.7, BL-38)
- RECOVERY 명령(`RECOVERY_CMD_MID`) 수신 시 action별 처리(§17), MODE 명령(`MODE_CMD_MID`) 수신 시 상태 전이 검증 후 모드 갱신(§17)
- config 명령으로 런타임 타임아웃/게시주기 파라미터 변경 (pending/active 이중버퍼)
- viewpoint 명령 캐시
- 헬스 상태를 파일에 영속화하여 재시작 후 복원
- HK 요청 수신 시 HK 텔레메트리 게시

`cfs_core_app`은 현재 다음을 수행하지 않는다.

- 시리얼 장치 재열기
- 외부 컴포넌트(FC/센서) 재설정
- viewpoint 명령의 실제 실행 (현재는 캐시만)

## 5. 용어 정의

이 문서에서 일관되게 사용하는 용어는 다음과 같다.

- `state cache`: 구독된 FC 상태 메시지의 최신 in-memory 복사본
- `bridge cache`: 구독된 bridge HK 요약 필드의 최신 in-memory 복사본
- `received`: 앱 초기화 이후 해당 타입의 메시지가 최소 하나 이상 처리됐음을 나타내는 내부 boolean
- `expired`: `NowMs - TimestampMs > TimeoutMs` 조건이거나, 해당 캐시에 메시지가 한 번도 수신되지 않은 상태
- `unavailable`: 헬스 로직이 해당 입력을 정상 운용에 부적합하다고 표시하는 입력 조건
- `force publish`: 구독된 상태 메시지 처리 직후 트리거되는 직접 헬스 게시 요청
- `periodic publish`: 앱 메인 루프의 타임아웃 경로에서 트리거되는 헬스 게시 시도

## 6. 인터페이스

### 6.1 구독 메시지

`cfs_core_app`은 초기화 중 다음 메시지 ID를 구독한다.

| 심볼 | 값 | 목적 |
| --- | --- | --- |
| `CFS_CORE_APP_CMD_MID_VALUE` | `0x18C0` | 명령 입력 |
| `CFS_CORE_APP_SEND_HK_MID_VALUE` | `0x18C1` | HK 요청 |
| `CFS_CORE_APP_BRIDGE_HK_MID_VALUE` | `0x08A0` | Bridge HK 미러 입력 |
| `CFS_CORE_APP_UPLINK_HK_MID_VALUE` | `0x08D0` | uplink_app HK 생존 감시 입력 |
| `CFS_CORE_APP_LORA_HK_MID_VALUE` | `0x08E0` | lora_tdm_app HK 생존 감시 입력 |
| `CFS_CORE_APP_FC_EKF_LOCAL_STATE_MID_VALUE` | `0x1905` | FC local-state 입력 |
| `CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE` | `0x1906` | FC attitude-state 입력 |
| `CFS_CORE_APP_FC_GPS_RAW_STATE_MID_VALUE` | `0x1907` | FC GPS-state 입력 |
| `CFS_CORE_APP_FC_EKF_STATUS_MID_VALUE` | `0x1908` | FC EKF-status 입력 |
| `ROUTE_UPDATE_MID` | `0x190B` | 경로 갱신 입력 |
| `VIEWPOINT_CMD_MID` | `0x190D` | viewpoint 명령 입력 |
| `CONFIG_CMD_MID` | `0x190E` | 런타임 config 명령 입력 |
| `RECOVERY_CMD_MID` | `0x190C` | 복구 명령 입력 (bridge 재시작 카운터 리셋) |
| `MODE_CMD_MID` | `0x190F` | 모드 명령 입력 (모드 값 캐시) |
| `DIAGNOSTIC_CMD_MID` | `0x1910` | 진단 명령 입력 (`DiagTarget=1`이면 처리, waypoint readback 요청 등 — §4.3 `lora_protocol_v2_spec.md`, 2026-07-27 추가 기재) |

구독은 단일 파이프 `CFS_CORE_CMD` (`CFS_CORE_APP_PLATFORM_PIPE_DEPTH = 16`)로 수신한다.

### 6.2 게시 메시지

| 심볼 | 값 | 목적 |
| --- | --- | --- |
| `CFS_CORE_APP_HK_TLM_MID` | `0x08C0` | HK 텔레메트리 |
| `SYSTEM_HEALTH_MID` | `0x1904` | 시스템 헬스 텔레메트리 |
| `ROUTE_SNAPSHOT_MID` | `0x1913` | waypoint readback 시 `MissionRoute` 캐시를 `lora_tdm_app`으로 발행 (`ROUTE_UPDATE_TLM_t`와 동일 레이아웃 재사용, 2026-07-27 추가 기재) |

## 7. 입력 페이로드

### 7.1 일반 FC 상태 입력

다음 구독 입력은 `CFS_CORE_APP_GenericStateTlm_t`로 처리된다.

- attitude state
- local state
- GPS state
- EKF state

`cfs_core_app`이 소비하는 페이로드 필드는 다음과 같다.

| 필드 | 형식 | 용도 |
| --- | --- | --- |
| `TimestampMs` | `uint32` | 만료 및 마지막 유효 입력 선택 |
| `Seq` | `uint32` | 추적용 캐시만 |
| `Valid` | `uint8` | 입력 가용성 판단 |
| `Stale` | `uint8` | 입력 가용성 판단 |
| `ErrorCode` | `uint8` | 추적용 캐시만 |

시퀀스 검사가 구현되어 있다 (`CFS_CORE_APP_UpdateStateCache`): 중복(`Seq == 직전`)·역행(`Seq < 직전`)은 캐시 갱신을 거부하고 `SeqRejectedCount` 증가 및 `SEQ_ERR_EID` 발생, 갭(`Seq > 직전+1`)은 수락하되 `SeqGapCount` 증가 및 `SEQ_GAP_EID` 발생. (첫 수신 시에는 검사 생략.)
타임스탬프 유효성 검사가 구현되어 있다: 미래 timestamp(`Msg->TimestampMs > NowMs + CFS_CORE_APP_TIMESTAMP_MAX_FUTURE_MS(5000)`)는 거부하고 `TimestampRejectedCount` 증가 및 `TIMESTAMP_ERR_EID` 발생.

**time base 검증(BL-42, 2026-07-24)**: `TimestampMs`는 FC의 `time_boot_ms`(**FC 부팅 기준**)이고 `NowMs`는 Pi cFE 미션 시각(**Pi 기준**)으로 *두 시계의 기준이 다르다*. 따라서 만료 판정에 두 값을 직접 빼면(구 구현) FC 재부팅 시 오프셋이 급변해 판정이 무너진다. 이를 해소하기 위해:
- **만료 판정은 Pi 도착시각 기준**: `StateCache_t.ArrivalMs`(수신 시점 `NowMs` 기록)를 사용해 `StateExpired = (NowMs - ArrivalMs) > TimeoutMs`. 단조 증가하는 Pi 로컬 시계라 링크 두절도 정상 감지. `TimestampMs`는 만료 판정에서 제외(순서·재부팅 감지용으로만 유지). uplink/lora의 `LastHkRxMs` 패턴과 동일.
- **FC 재부팅/기준 어긋남 감지**: 수신 시 `Msg->TimestampMs + CFS_CORE_APP_TIMEBASE_SHIFT_MS(10000) < 직전 TimestampMs`(FC 시각이 10s 이상 역행)이면 time base 불연속으로 판단 — `TimebaseShiftCount` 증가 및 `TIMEBASE_SHIFT_EID` 발생. 메시지 자체는 신선하므로 **거부하지 않고** 새 기준으로 캐시 갱신(Seq는 Pi측 bridge 카운터라 FC 재부팅에 무관하게 단조 유지). `TimebaseShiftCount`는 HK에 노출.
이 텔레메트리 입력에 대한 페이로드 길이 검증은 수행되지 않는다.

### 7.2 Bridge HK 입력

`CFS_CORE_APP_BridgeHkMirror_t`는 `shared_msgs/bridge_hk_msg.h`의 `BRIDGE_HK_TLM_t` typedef이다
(2026-07 미러 병합 — 단일 진실, `NonFiniteValueCount` 등 발행측 전체 필드 포함).
`cfs_core_app`이 소비하는 필드는 다음과 같다.

| 필드 | 형식 | 용도 |
| --- | --- | --- |
| `LinkState` | `uint8` | 캐시 전용 |
| `LastErrorCode` | `uint8` | 캐시 전용 |
| `LastRxTimestampMs` | `uint32` | bridge 타임아웃 판단 |

bridge 명령/오류 카운터와 바이트 카운터는 헬스 분류에 사용하지 않는다.

### 7.3 경로 갱신 입력

`ROUTE_UPDATE_MID`는 `CFS_CORE_APP_RouteUpdateTlm_t`로 처리된다.

소비되는 필드는 다음과 같다.

| 필드 | 형식 | 용도 |
| --- | --- | --- |
| `TimestampMs` | `uint32` | 경로 캐시 타임스탬프 |
| `SourceSequence` | `uint32` | 추적용 캐시 |
| `RouteType` | `uint8` | route_op(REPLACE/ADD/DELETE/MODIFY) 값 — 옛 mission/landing 세그먼트 선택 개념은 2026-07-29 제거됨(§16) |
| `RouteVersion` | `uint8` | 캐시 |
| `WaypointCount` | `uint8` | 캐시 및 HK에 보고 |
| `Waypoints[]` | 배열 | 캐시 |

다음 경로 타입만 캐시를 갱신한다.

- `CFS_CORE_APP_ROUTE_SEGMENT_MISSION_EXTENSION`
- `CFS_CORE_APP_ROUTE_SEGMENT_LANDING`

그 외 경로 타입은 경로 캐시를 갱신하지 않는다.
경로 갱신은 헬스 상태 분류에 영향을 주지 않는다.

## 8. 내부 상태

### 8.1 캐시된 FC 상태

각 FC 상태 캐시는 다음을 저장한다.

- `TimestampMs`
- `Seq`
- `Valid`
- `Stale`
- `ErrorCode`
- `Received`

앱은 다음 각각에 대해 하나의 캐시를 유지한다.

- attitude state
- local state
- GPS state
- EKF state

### 8.2 캐시된 bridge 상태

bridge 캐시는 다음을 저장한다.

- `LinkState`
- `LastErrorCode`
- `LastRxTimestampMs`
- `Received`

### 8.3 캐시된 경로 상태

앱은 다음을 유지한다.

- mission-route 캐시 1개(`LandingRoute` 캐시는 2026-07-29 제거 — FC에 landing 세그먼트 개념 자체가 없어 항상 도달 불가 상태였음, BL-56/BL-61)

각 경로 캐시는 다음을 저장한다.

- `TimestampMs`
- `SourceSequence`
- `UpdateCount`
- `RouteType`
- `RouteVersion`
- `WaypointCount`
- `Valid`
- `Waypoints[]`

## 9. 타이밍 설정

다음 타이밍 상수가 구현되어 있다.

| 상수 | 값 | 의미 |
| --- | --- | --- |
| `CFS_CORE_APP_SB_POLL_TIMEOUT_MS` | `200` | 메인 루프 SB 수신 타임아웃 |
| `CFS_CORE_APP_PROTOTYPE_PERIOD_MS` | `1000` | 주기적 헬스 게시의 최소 간격 |
| `CFS_CORE_APP_ATTITUDE_TIMEOUT_MS` | `2000` | attitude-state 만료 임계값 |
| `CFS_CORE_APP_LOCAL_TIMEOUT_MS` | `2000` | local-state 만료 임계값 |
| `CFS_CORE_APP_GPS_TIMEOUT_MS` | `3000` | GPS-state 만료 임계값 |
| `CFS_CORE_APP_EKF_TIMEOUT_MS` | `2000` | EKF-state 만료 임계값 |
| `CFS_CORE_APP_BRIDGE_TIMEOUT_MS` | `3000` | bridge 타임아웃 임계값 |
| `CFS_CORE_APP_NOMINAL_STABILITY_MS` | `10000` | 비-NOMINAL→NOMINAL 복귀 안정화 창 |
| `CFS_CORE_APP_FAILED_ESCALATION_MS` | `30000` | bridge 타임아웃 지속 시 FAILED 에스컬레이션 임계값 |
| `CFS_CORE_APP_TIMESTAMP_MAX_FUTURE_MS` | `5000` | 미래 timestamp 거부 임계값 |
| `CFS_CORE_APP_BRIDGE_RESTART_INTERVAL_MS` | `5000` | bridge 재시작 시도 간격 |

attitude/local/gps/ekf/bridge 타임아웃 및 게시 주기(`PROTOTYPE_PERIOD_MS`)는 init 시 `ActiveConfig` 기본값으로 로드되며, `CONFIG_CMD_MID`로 런타임 변경 가능하다(§17, §21.2). 헬스 평가는 상수가 아닌 `ActiveConfig` 값을 사용한다.

## 10. 헬스 출력 계약

`SYSTEM_HEALTH_MID`는 다음 필드를 게시한다.

| 필드 | 의미 |
| --- | --- |
| `Seq` | 모든 헬스 게시 시마다 증가하는 단조 앱-로컬 게시 카운터 |
| `TimestampMs` | 게시 시점의 cFE 시간 (밀리초) |
| `LastValidInputTimestampMs` | 수신된 attitude/local/GPS/EKF 캐시 중 최대 타임스탬프. 아무것도 수신되지 않은 경우 `NowMs` |
| `HealthState` | `NOMINAL`, `DEGRADED`, `RECOVERY`, `FAILED` (bridge 타임아웃이 `FAILED_ESCALATION_MS` 초과 시 `FAILED`) |
| `FaultCode` | `NONE(0)`, `BRIDGE_TIMEOUT(1)`, `EKF_INVALID(3)`, `LOCAL_TIMEOUT(4)`, `ATTITUDE_TIMEOUT(5)`, `UPLINK_TIMEOUT(6)`, `LORA_TIMEOUT(7)` (※ `GPS_STALE(2)`는 enum 유지하나 헬스 저하용으로 미사용 — §12.7) |
| `AttitudeStatus` / `LocalStatus` / `GpsStatus` / `EkfStatus` | per-input 상태 구조: `Valid`, `Stale`, `ErrorCode`, `TimedOut`(만료 여부). GPS는 보고 전용으로 헬스 비반영 — §12.7 |
| `BridgeStatus` | bridge 상태 구조: `LinkState`, `ErrorCode`, `TimedOut` |
| `RecoveryRequested` | bridge 타임아웃 조건에서만 `1`, 그 외에는 `0` |

현재 구현은 매 게시 전 텔레메트리 구조체를 0으로 초기화한다.
현재 구현은 매 게시 전 메시지 헤더를 재초기화한다.

## 11. 게시 조건

### 11.1 즉시 헬스 게시

`cfs_core_app`은 다음 처리 직후 `SYSTEM_HEALTH_MID`를 즉시 게시한다.

- bridge HK
- uplink_app HK
- lora_tdm_app HK
- attitude state
- local state
- GPS state
- EKF state
- 경로 갱신

### 11.2 주기적 헬스 게시

SB 메시지가 `200 ms` 동안 수신되지 않으면 앱은 타임아웃 경로로 진입한다.
이 경로에서 앱은 헬스 갱신을 시도한다.

헬스 갱신은 다음 조건에서만 게시된다.

`NowMs - LastPublishTimeMs >= 1000`

### 11.3 HK 게시

HK는 `CFS_CORE_APP_SEND_HK_MID_VALUE` 수신 시에만 게시된다.

## 12. 헬스 분류 규칙

헬스 분류는 엄격한 우선순위 순서로 평가된다.
가장 높은 우선순위의 일치 조건만 출력을 결정한다.

### 12.1 우선순위 1: Bridge 타임아웃

조건:

- bridge 캐시가 한 번도 수신되지 않음
- 또는 `NowMs - BridgeState.LastRxTimestampMs > 3000`

출력:

- `HealthState = CFS_CORE_APP_HEALTH_RECOVERY`
- `FaultCode = CFS_CORE_APP_FAULT_BRIDGE_TIMEOUT`
- `RecoveryRequested = 1`

### 12.2 우선순위 2: EKF 무효 또는 stale

조건:

- EKF 상태 만료 (`NowMs - EkfState.TimestampMs > 2000`, 또는 수신된 적 없음)
- 또는 EKF `Valid == 0`
- 또는 EKF `Stale != 0`

출력:

- `HealthState = CFS_CORE_APP_HEALTH_DEGRADED`
- `FaultCode = CFS_CORE_APP_FAULT_EKF_INVALID`
- `RecoveryRequested = 0`

### 12.3 우선순위 3: Local state 타임아웃 또는 무효

조건:

- local state 만료 (`NowMs - LocalState.TimestampMs > 2000`, 또는 수신된 적 없음)
- 또는 local `Valid == 0`
- 또는 local `Stale != 0`

출력:

- `HealthState = CFS_CORE_APP_HEALTH_DEGRADED`
- `FaultCode = CFS_CORE_APP_FAULT_LOCAL_TIMEOUT`
- `RecoveryRequested = 0`

### 12.4 우선순위 4: Attitude state 타임아웃 또는 무효

조건:

- attitude state 만료 (`NowMs - AttitudeState.TimestampMs > 2000`, 또는 수신된 적 없음)
- 또는 attitude `Valid == 0`
- 또는 attitude `Stale != 0`

출력:

- `HealthState = CFS_CORE_APP_HEALTH_DEGRADED`
- `FaultCode = CFS_CORE_APP_FAULT_ATTITUDE_TIMEOUT`
- `RecoveryRequested = 0`

### 12.5 우선순위 5: uplink_app 타임아웃

조건:

- uplink_app HK가 한 번도 수신되지 않음
- 또는 `NowMs - UplinkAppState.LastHkRxMs > 5000`

출력:

- `HealthState = CFS_CORE_APP_HEALTH_DEGRADED`
- `FaultCode = CFS_CORE_APP_FAULT_UPLINK_TIMEOUT`
- `RecoveryRequested = 0`

### 12.6 우선순위 6: lora_tdm_app 타임아웃

조건:

- lora_tdm_app HK가 한 번도 수신되지 않음
- 또는 `NowMs - LoraAppState.LastHkRxMs > 5000`

출력:

- `HealthState = CFS_CORE_APP_HEALTH_DEGRADED`
- `FaultCode = CFS_CORE_APP_FAULT_LORA_TIMEOUT`
- `RecoveryRequested = 0`

### 12.7 GPS 가용성: 헬스 분류에서 제외 (보고 전용)

GPS 가용성(만료 / `Valid == 0` / `Stale != 0`)은 **HealthState를 저하시키지 않는다.**

근거:

- cFS health는 Pi/cFS **통신·파이프라인 상태**를 나타낸다(§15, mission_app_runtime_spec §15). GPS fix는 **센서/비행 조건**이므로 통신-계층 health 게이트의 입력이 아니다.
- GPS no-fix는 실내/프리플라이트에서 정상적으로 발생하며, 이를 DEGRADED로 처리하면 GPS와 무관한 비위험 uplink 명령(CONFIG/DIAGNOSTIC)까지 차단되어 운용이 불가능해진다.

대신:

- GPS 상태는 `SYSTEM_HEALTH_MID`의 per-input 요약 필드(`GpsValid`)로 **계속 보고**한다(관측 가능).
- 헬스 분류 우선순위 사다리(§12.1~§12.6)에서 GPS 분기는 제거한다.
- `CFS_CORE_APP_FAULT_GPS_STALE` enum은 호환을 위해 정의를 유지하되 HealthState 저하용으로는 더 이상 생성하지 않는다.

### 12.8 우선순위 7: 정상

조건:

- §12.1~§12.6 조건 중 어느 것도 해당하지 않음 (bridge / EKF / local / attitude / uplink HK / lora HK 모두 신선)
- GPS 가용성은 NOMINAL 판정에 영향을 주지 않는다

출력:

- `HealthState = CFS_CORE_APP_HEALTH_NOMINAL`
- `FaultCode = CFS_CORE_APP_FAULT_NONE`
- `RecoveryRequested = 0`
- `GpsValid`는 실제 GPS 상태를 그대로 반영 (NOMINAL이어도 0일 수 있음)

### 12.9 FAILED 에스컬레이션

`CFS_CORE_APP_HEALTH_FAILED`는 bridge 타임아웃이 `CFS_CORE_APP_FAILED_ESCALATION_MS` (30000ms) 이상 지속될 때 생성된다. bridge 타임아웃이 시작되면 `RecoveryStartMs`가 설정되고, `NowMs - RecoveryStartMs >= 30000`이면 `HealthState = FAILED`, 그 이전에는 `RECOVERY`로 게시한다 (§13.1, §14.4 참조).

## 13. 타임아웃 및 오류 처리 상세

### 13.1 Bridge 타임아웃

`bridge timeout`은 FC 상태 메시지의 도착 시각이 아닌 bridge HK의 `LastRxTimestampMs`를 기준으로 평가된다.

효과:

- `RECOVERY` 생성 (단, 타임아웃이 `FAILED_ESCALATION_MS(30000)` 이상 지속되면 `FAILED`로 에스컬레이션 — §12.9)
- `FAULT_BRIDGE_TIMEOUT` 생성
- `RecoveryRequested = 1` 설정
- `BRIDGE_RESTART_INTERVAL_MS(5000)` 경과마다 `mavlink_bridge_app` 재시작 시도(`CFE_ES_RestartApp`), 횟수 상한 없이 쿨다운만으로 빈도 제한(BL-38, 2026-07-23 — `MAX_RESTARTS` 제거). 시도 시 `BRIDGE_RESTART_EID` 발생 (§14.4)
- bridge 타임아웃이 더 높은 우선순위를 가지므로 EKF 오류 보고 억제
- 비-bridge 분기로 전이 시 `RecoveryStartMs`/`BridgeRestartCount`/`NextBridgeRestartMs`는 0으로 리셋

### 13.2 GPS 가용성 (헬스 비반영, 보고 전용)

`gps unavailable`은 다음을 포함한다.

- GPS 메시지가 한 번도 수신되지 않음
- GPS 타임스탬프 경과 시간이 `3000 ms` 초과
- GPS `Valid == 0`
- GPS `Stale != 0`

효과:

- **HealthState 변화 없음** (DEGRADED를 생성하지 않음)
- `GpsValid = 0`으로 per-input 요약 필드에 반영 (보고 전용)
- `FaultCode`는 GPS 사유로 설정하지 않음
- 근거 및 정책: §12.7 참조

### 13.3 EKF invalid

`ekf invalid`는 다음을 모두 포함한다.

- EKF 메시지가 한 번도 수신되지 않음
- EKF 타임스탬프 경과 시간이 `2000 ms` 초과
- EKF `Valid == 0`
- EKF `Stale != 0`

효과:

- `DEGRADED` 생성
- `FAULT_EKF_INVALID` 생성
- `RecoveryRequested` 미설정

### 13.4 Local 타임아웃

`local timeout`은 전용 오류 코드 `FAULT_LOCAL_TIMEOUT`으로 표현된다.

`local timeout` 조건:

- local-state 메시지가 한 번도 수신되지 않음
- local-state 타임스탬프 경과 시간이 `2000 ms` 초과

효과:

- `DEGRADED` 생성
- `FAULT_LOCAL_TIMEOUT` 생성
- `RecoveryRequested` 미설정

참고: local `Valid == 0` 또는 `Stale != 0`은 우선순위 3에서 `FAULT_LOCAL_TIMEOUT`으로 분류된다.
이는 EKF 우선순위(2)와 독립적으로 평가되며, 타임스탬프 만료와 동일한 조건 블록에서 처리된다.

### 13.5 Attitude 타임아웃

`attitude timeout`은 전용 오류 코드 `FAULT_ATTITUDE_TIMEOUT`으로 표현된다.

`attitude timeout` 조건:

- attitude-state 메시지가 한 번도 수신되지 않음
- attitude-state 타임스탬프 경과 시간이 `2000 ms` 초과

효과:

- `DEGRADED` 생성
- `FAULT_ATTITUDE_TIMEOUT` 생성
- `RecoveryRequested` 미설정

참고: attitude `Valid == 0` 또는 `Stale != 0`은 우선순위 4에서 `FAULT_ATTITUDE_TIMEOUT`으로 분류된다.
이는 EKF 우선순위(2)와 독립적으로 평가되며, 타임스탬프 만료와 동일한 조건 블록에서 처리된다.

### 13.6 uplink_app 타임아웃

`uplink_app`의 HK (`UPLINK_APP_HK_TLM_MID = 0x08D0`)가 `5000 ms` 이내에 수신되지 않으면 타임아웃으로 판정한다.

조건:

- `UplinkAppState.Received == false` (한 번도 수신되지 않음)
- 또는 `NowMs - UplinkAppState.LastHkRxMs > CFS_CORE_APP_UPLINK_TIMEOUT_MS (5000)`

효과:

- `DEGRADED` 생성
- `FAULT_UPLINK_TIMEOUT` 생성
- `RecoveryRequested = 0`
- **자동 재시작** (2026-07 구현, bridge와 동일 패턴 — `cfs_core_app_utils.c:353-384`): 타임아웃 지속 시 `UPLINK_RESTART_INTERVAL_MS(5000)` 간격으로 `CFE_ES_RestartApp("uplink_app")` 시도, 횟수 상한 없이 쿨다운만으로 빈도 제한(BL-38, 2026-07-23 — `MAX_RESTARTS` 제거). 시도마다 `UPLINK_RESTART_EID (15)` 발생. 타임아웃 해소 또는 상위 우선순위 fault 발생 시 카운터 리셋

`UplinkAppState.LastHkRxMs`는 HK를 수신한 시점의 `NowMs` (cFS wall-clock ms)로 갱신된다. uplink_app의 내부 timestamp를 사용하지 않는다.

### 13.7 lora_tdm_app 타임아웃

`lora_tdm_app`의 HK (`LORA_TDM_APP_HK_TLM_MID = 0x08E0`)가 `5000 ms` 이내에 수신되지 않으면 타임아웃으로 판정한다.

조건:

- `LoraAppState.Received == false`
- 또는 `NowMs - LoraAppState.LastHkRxMs > CFS_CORE_APP_LORA_TIMEOUT_MS (5000)`

효과:

- `DEGRADED` 생성
- `FAULT_LORA_TIMEOUT` 생성
- `RecoveryRequested = 0`
- **자동 재시작** (2026-07 구현, bridge와 동일 패턴 — `cfs_core_app_utils.c:385-415`): 타임아웃 지속 시 `LORA_RESTART_INTERVAL_MS(5000)` 간격으로 `CFE_ES_RestartApp("lora_tdm_app")` 시도, 횟수 상한 없이 쿨다운만으로 빈도 제한(BL-38, 2026-07-23 — `MAX_RESTARTS` 제거). 시도마다 `LORA_RESTART_EID (16)` 발생. 타임아웃 해소 또는 상위 우선순위 fault 발생 시 카운터 리셋

## 14. 시작, 입력 손실, 복구 동작

### 14.1 시작

초기화 시 전체 앱 데이터 구조는 0으로 초기화된다.
모든 캐시는 `Received = false`로 시작한다.

결과:

- 첫 번째 bridge HK 처리 전에는 bridge 조건이 타임아웃으로 평가된다
- 첫 번째 헬스 관련 FC 상태 처리 전에는 해당 캐시도 만료로 평가될 수 있다

bridge 타임아웃이 최우선 순위를 가지므로, 유효한 bridge HK가 수신될 때까지 앱은 시작 중에 `RECOVERY`를 게시할 수 있다.

### 14.2 입력 손실

입력이 수신되지 않으면:

- 앱 메인 루프는 계속 실행된다
- 매 `200 ms` SB 타임아웃마다 주기 서비스 경로가 트리거된다
- 헬스는 최대 `1000 ms`에 한 번 재게시된다
- 만료된 캐시는 결국 `DEGRADED` 또는 `RECOVERY`를 유발한다

### 14.3 입력 복원

별도의 복구 상태 머신은 없다.

비-NOMINAL 상태에서 `NOMINAL`로 전이하려면 10초 안정화 타이머(`CFS_CORE_APP_NOMINAL_STABILITY_MS = 10000`)가 필요하다.

모든 오류 조건이 해소되면 앱은 안정화 창에 진입한다.
- 창 동안에는 `HealthState = DEGRADED`, `FaultCode = NONE`
- 10초 연속 오류 없는 운용 후 앱은 `NOMINAL`로 전이한다

안정화 창 중 오류가 재발하면 타이머가 리셋된다.

비-NOMINAL 에피소드 이전에 이미 `NOMINAL` 상태였다면, 비-NOMINAL 재진입 시 타이머가 `0`으로 리셋되고 다음 오류 없는 주기에 다시 시작된다.

### 14.4 능동 복구 조치

구현된 복구 조치:

- bridge 타임아웃 시 `SYSTEM_HEALTH_MID`에 `RecoveryRequested = 1` 설정
- bridge 타임아웃이 `BRIDGE_RESTART_INTERVAL_MS(5000)` 이상 지속되면 `CFE_ES_GetAppIDByName("mavlink_bridge_app")` 후 `CFE_ES_RestartApp()`로 bridge 앱 재시작 시도. 인터벌마다 1회, 횟수 상한 없이 쿨다운만으로 빈도 제한(BL-38, 2026-07-23 — `MAX_RESTARTS` 제거). `BridgeRestartCount` 증가(누계 기록용, 상한 아님), `BRIDGE_RESTART_EID` 발생
- bridge 타임아웃이 `FAILED_ESCALATION_MS(30000)` 이상 지속되면 `HealthState = FAILED`

bridge 앱 외 다른 앱·FC·센서·시리얼 장치에 대한 능동 복구는 구현되어 있지 않다.

### 14.5 헬스 상태·CONFIG 영속화 (BL-41, 2026-07-23 구현)

`HealthState`와 `ActiveConfig`(6필드)는 파일에 영속화되어 앱 재시작 후 복원된다.

- 레코드 구조: `{Magic, LastHealthState, ConfigVersion, Reserved[2], AttitudeTimeoutMs, LocalTimeoutMs, GpsTimeoutMs, EkfTimeoutMs, BridgeTimeoutMs, PublishPeriodMs, Checksum}`(BL-43 추가 필드는 아래 참조).
  **Checksum 공식(2026-07-27 정정, 코드 기준)**: `Magic + LastHealthState + ConfigVersion + AttitudeTimeoutMs + LocalTimeoutMs + GpsTimeoutMs + EkfTimeoutMs + BridgeTimeoutMs + PublishPeriodMs + BridgeRestartCount + UplinkRestartCount + LoraRestartCount + LastFaultCode`(`CFS_CORE_APP_ComputeStateChecksum`) — BL-43(아래) 추가 필드 4개도 checksum에 포함됨. 이전 서술("6필드 합"만)은 BL-43 이전 스냅샷이라 낡음.
- 저장(`CFS_CORE_APP_SaveState`): ① `HealthState` 전이 시, ② CONFIG 적용 성공 시(`ConfigGeneration++` 직후) 호출. `STATE_FILE_PATH.tmp`에 쓰고 `fsync` 후 `rename()`으로 원자적 교체, 이어서 부모 디렉터리 fd도 `fsync`(BL-18 — 정전 시 rename 유실 방지). `STATE_FILE_PATH = "cf/cfs_core_app_state.bin"`(BL-39로 상대경로, runtime spec §12.1 참조), 테스트에선 env var `CFS_CORE_APP_STATE_FILE_PATH`로 경로 주입.
- 복원(`CFS_CORE_APP_LoadState`): init 끝에서 호출. `Magic == STATE_MAGIC(0xCF5C0A00)`·Checksum·`ConfigVersion == CFS_CORE_APP_CONFIG_VERSION` 검증 통과 시 `LastHealthState` + `ActiveConfig` 6필드 복원, `STARTUP_EID`로 보고.
- 손상 처리: 파일 없음(ENOENT)만 침묵(첫 부팅 정상), 그 외(truncated/bad magic/checksum 불일치/open 오류)는 `STATE_CORRUPT_EID` ERROR 보고 후 기본값 유지(uplink_app §12.1 표와 동일 정책). `ConfigVersion` 불일치는 구버전 파일로 간주해 range 재검증 없이 전체 기본값 폴백(필드 오해석 방지).
- 복원값은 range 재검증하지 않는다 — ActiveConfig 승격 전에 이미 검증된 값만 저장되기 때문.

**앱 상태 영속화 (BL-43, 2026-07-23 설계 확정)** — 레코드에 추가:
`BridgeRestartCount`/`UplinkRestartCount`/`LoraRestartCount`(u32 누계) +
`LastFaultCode`(u8). 저장 시점: ① `CheckAppRestarts()`의
`CFE_ES_RestartApp()` 발행 직후(3분기 각각), ② RECOVERY `RESET_COUNTER`
처리 시(리셋 동기화), ③ health 전이 시(기존 SaveState에 동승). 복원:
Init `LoadState()` 자동 — 재부팅 후에도 재시작 누계 지속. **보고 전용**
(복원값이 동작을 바꾸지 않음, 지상국 판단 재료). HK에
`BridgeRestartCount`/`UplinkRestartCount`/`LoraRestartCount`/`LastFaultCode`
신규 노출(종전 RAM 전용 — BL-38 당시 "HK 노출" 기술은 미구현이었음).
상세: runtime spec §12.3.

**⚠️ `PublishPeriodMs` 확장 시 재시작 감시 타이밍도 같이 지연되는 부작용
(2026-07-27 기재)**: `CFS_CORE_APP_UpdateHealth()`는 함수 시작부에서
`(NowMs - LastPublishTimeMs) < PublishPeriodMs`이면 조기 리턴하는데,
`CFS_CORE_APP_CheckAppRestarts()` 호출이 이 가드 **뒤**에 있어 같은
주기로만 실행된다. 즉 `PublishPeriodMs`를 늘리면(예: 60000ms) HK 게시
주기뿐 아니라 `BRIDGE_RESTART_INTERVAL_MS`(5000ms)/`FAILED_ESCALATION_MS`
(30000ms) 등 재시작·에스컬레이션 감시 주기도 사실상 그 값으로 밀려나
설계 의도와 다르게 둔감해질 수 있다 — CONFIG로 이 파라미터를 조정할 때
반드시 고려해야 함.

## 15. HK 동작

HK 요청 시 앱은 다음을 보고한다.

- 명령 카운터
- 명령 오류 카운터
- mission route waypoint 수
- 게시 횟수
- 마지막 게시 타임스탬프
- 마지막 경로 갱신 타임스탬프
- 총 경로 갱신 횟수

앱은 경로 관련 HK 필드를 요약하는 EVS 정보 이벤트도 발생시킨다.

## 16. 경로 처리 규칙

허용된 경로 갱신 시마다 mission route 캐시의 `UpdateCount`가 증가한다.

경로 캐시 입력은 2채널이다 (BL-41 route, 2026-07-23):

- `ROUTE_UPDATE_MID`(0x190B) — 지상국 발 경로 갱신. `RouteType`은 route_op
  (REPLACE=1/ADD=2/DELETE=3/MODIFY=4, BL-61)이며 항상 `MissionRoute`
  캐시 대상(mission/landing 캐시 선택 개념은 BL-56에서 애초에 존재한 적
  없었음이 확인돼 폐기됨).
- `FC_MISSION_READBACK_MID`(0x1914) — **FC 실물 미션 readback**
  (mavlink_bridge_app 게시, mavlink spec §10). `RouteType` 검사 없이
  **`MissionRoute` 캐시 고정 갱신**(FC에 landing 세그먼트 개념 없음),
  `ROUTE_READBACK_EID`(18) INFO. `MissionRoute`는 RAM 전용 미러 —
  파일 영속화하지 않으며, 재부팅 후 비어있는 것이 정상(FC 링크
  CONNECTED 전이 시 readback이 곧 다시 채움 — FC가 유일 진실원본,
  runtime spec §12.2와 대비되는 의도적 비영속 항목).

경로 갱신이 영향을 주는 항목:

- 경로 캐시
- HK 카운터
- 경로 갱신 EVS 이벤트 발생
- 즉시 헬스 재게시 타이밍

경로 갱신이 영향을 주지 않는 항목:

- 헬스 상태 분류
- 오류 코드 선택
- `RecoveryRequested`

## 17. 명령 처리

`CFS_CORE_APP_CMD_MID` (0x18C0) 함수 코드:

- NOOP (CC=0)
- 카운터 리셋 (CC=1)

추가로 별도 MID로 수신·처리하는 명령:

- **`CONFIG_CMD_MID` (0x190E)** — 런타임 config 명령 (`CFS_CORE_APP_ProcessConfigCommand`). payload 헤더(`ConfigScope`/`ConfigVersion`/`ParameterId`/`ValueType`/`ValueLength`/`Checksum`) + `uint32` 값을 단계 검증: ① 길이 ② scope(`CONFIG_SCOPE=1`) ③ version(`CONFIG_VERSION=1`) ④ checksum ⑤ 값 범위(`PARAM_MIN_MS 100` ~ `PARAM_MAX_MS 60000`). 통과 시 `ActiveConfig` 기반 `PendingConfig`에 6개 파라미터(attitude/local/gps/ekf/bridge 타임아웃, publish 주기) 중 해당 항목 기록 → 교차 검증 → `PreviousConfig` 백업 후 `ActiveConfig`로 활성화, `ConfigGeneration` 증가. 각 실패는 `ConfigPendingState=REJECTED` + `LastConfigResult`(BAD_LENGTH/SCOPE/VERSION/CHECKSUM/VALUE/PARAM)로 기록. (§21.2) **scope 불일치는 다른 앱 대상 브로드캐스트이므로 EXEC_RESULT를 발행하지 않음**(그 외 실패/성공은 전부 발행, BL-08).
- **`VIEWPOINT_CMD_MID` (0x190D)** — viewpoint 명령 (`CFS_CORE_APP_ProcessViewpointCommand`). type/frame/X/Y/Z/Yaw/Pitch/HoldTime를 `ViewpointCmd` 캐시에 저장(`Valid=true`)하고 `VIEWPOINT_EID` 발생. 실제 실행은 짐벌 미탑재로 범위 제외 확정(BL-10). **`EXEC_RESULT_MID`로 uplink_app에 명시적 FAILED 회신**(2026-07-29, BL-82) — `CommandClass=3`(`UPLINK_APP_CLASS_VIEWPOINT`), uplink_app의 무한 ROUTED 대기를 해소.
- **`RECOVERY_CMD_MID` (0x190C)** — 복구 명령 (`CFS_CORE_APP_ProcessRecoveryCommand`, `cfs_core_app_utils.c:740-`). `RecoveryAction` 6종을 switch 분기 처리: `RESET_COUNTER`는 `RecoveryStartMs`뿐 아니라 `BridgeRestartCount`/`UplinkRestartCount`/`LoraRestartCount`/`NextBridgeRestartMs`/`NextUplinkRestartMs`/`NextLoraRestartMs`를 전부 대칭 리셋(BL-65, 2026-07-27 — 이전엔 bridge 카운터만 리셋해 지상 명령이 운영자 기대와 다르게 동작하던 비대칭 버그가 있었음), **`RESTART_BRIDGE`/`RESTART_UPLINK`/`RESTART_LORA`는 실제로 `CFE_ES_RestartApp()`를 호출**해 해당 앱을 재시작함(2026-07-21, BL-09 — 이전엔 로그만 찍었음; 자동 재시작(bridge/uplink/lora timeout) 로직과 동일 메커니즘을 지상 명령으로 즉시 트리거, 자동 경로의 인터벌/최대재시도 게이트는 적용 안 함), **`PARSER_RESET`/`SERIAL_RECONNECT`도 실제로 연결됨**(2026-07-22, P1-a) — `mavlink_bridge_app`의 기존 `CMD_MID`(`0x18A0`)를 신규 MID 신설 없이 FcnCode로 재사용(`PARSER_RESET_CC=3`/`SERIAL_RECONNECT_CC=4`, `mavlink_bridge_app` 자신이 NOOP/RESET_COUNTERS/MISSION_QUERY를 구분하던 방식과 동일 관례)해 `CFS_CORE_APP_SendBridgeCtrlCmd()`로 발행 → `mavlink_bridge_app`이 실제 `ResetParser()`/`CloseSerial()+OpenSerial()`을 호출, unknown action은 ERROR 이벤트로 거부. 모든 경로에서 `RecoveryRequestedCount` 증가, `RECOVERY_CMD_EID (13)` 발생 (seq/target/reason/token 포함); RESTART 3종은 추가로 각각 `BRIDGE_RESTART_EID(10)`/`UPLINK_RESTART_EID(15)`/`LORA_RESTART_EID(16)`도 발생. **모든 action 분기 후 `EXEC_RESULT_MID`로 uplink_app에 회신**(2026-07-22, BL-08) — RESTART 3종은 앱을 못 찾으면 FAILED, unknown action도 FAILED로 회신한다. **⚠️ `PARSER_RESET`/`SERIAL_RECONNECT`는 "항상 OK"가 아님(2026-07-27 정정)**: `CFS_CORE_APP_SendBridgeCtrlCmd()`의 반환값(`Ok`)을 그대로 `GenericResult`에 반영하므로, `mavlink_bridge_app`으로의 발행 자체가 실패하면(예: SB send 실패) 이 두 action도 FAILED로 회신될 수 있다 — 발행이 성공한 경우에만 OK. `RESET_COUNTER`는 SB 발행 없이 로컬 처리라 항상 OK. `DetailCode`엔 세 action 공통으로 `RecoveryAction` 원값을 싣는다.
- **`MODE_CMD_MID` (0x190F)** — 모드 명령 (`CFS_CORE_APP_ProcessModeCommand`, `cfs_core_app_utils.c:786-829`). 상태 전이 검증 구현됨 (2026-07): `ENTER`는 NORMAL→RECOVERY, `EXIT`는 RECOVERY→NORMAL만 허용, 허용 전이는 `CurrentModeState` 갱신 + `MODE_CMD_EID (14)` INFO, 불허 조합은 REJECTED ERROR 이벤트(`ErrCounter++`). **`EXEC_RESULT_MID`로 uplink_app에 회신**(2026-07-29, BL-81) — `CommandClass=5`(`UPLINK_APP_CLASS_MODE`), `TransitionAllowed`/`RequestedState` 반영. 단, `CurrentModeState`가 다른 로직에서 실제로 읽혀 게이팅에 반영되는지는 이번 스코프 밖(EXEC_RESULT 회신으로 uplink_app dead-end만 해소).

NOOP/RESET은 명령 길이 검사(`VerifyCmdLength`)로 유효성을 검사한다. 텔레메트리 상태 입력은 길이 검사를 하지 않는다. 알 수 없는 함수/MID는 명령 오류 카운터를 증가시킨다.

## 18. 기존 단위 테스트 커버리지

현재 단위 테스트는 다음을 검증한다.

- HK 함수 실행 및 필드 검증 (`ReportHousekeeping`, `ReportHousekeeping_Fields`)
- 명령 길이 검증 성공/실패 (`VerifyCmdLength`)
- 헬스 분류: NOMINAL, bridge Recovery, EKF invalid
- local 타임아웃/invalid/stale → `FAULT_LOCAL_TIMEOUT`
- attitude 타임아웃/invalid/stale → `FAULT_ATTITUDE_TIMEOUT`
- GPS stale → 헬스 비저하(NOMINAL) 확인 (§12.7)
- uplink_app / lora_tdm_app HK 타임아웃 → `FAULT_UPLINK_TIMEOUT`/`FAULT_LORA_TIMEOUT` (`UpdateHealth_UplinkTimeout`, `UpdateHealth_LoraTimeout`)
- FAILED 에스컬레이션 (bridge 타임아웃 30s 경과)
- NOMINAL 안정화 타이머 (10s, `NominalStabilization`)
- per-input status 필드 (`InputStatus`)
- 헬스 상태 전이 이벤트 (`HealthTransition`)
- 주기 게시 rate limit (`PeriodicRateLimit`)
- timestamp 검사: 정상 / 미래 초과 거부 / 경계값 / GPS·EKF 거부 / seq 검사 전 평가
- mission-route 캐시 갱신, bridge HK 캐시 갱신
- config 명령: attitude timeout·publish period 적용, bad checksum/scope/version/param 거부
- service prototype 실행 경로, 초기화 성공, 구독 오류 시 초기화 실패, NOOP, 카운터 리셋

## 19. 검증 절차 권고

### 19.1 정적 검증

런타임 테스트 전 다음 소스 위치를 검토한다.

- [cfs_core_app.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app.c:52)의 초기화 및 구독
- [cfs_core_app_dispatch.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app_dispatch.c:21)의 메시지 라우팅
- [cfs_core_app_utils.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app_utils.c:91)의 캐시 갱신
- [cfs_core_app_utils.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app_utils.c:154)의 헬스 분류

### 19.2 런타임 검증 매트릭스

| ID | 시나리오 | 자극 | 기대 출력 |
| --- | --- | --- | --- |
| CORE-RUN-001 | 정상 | bridge HK와 모든 FC 상태가 `Valid=1`, `Stale=0`으로 신선하게 도착 | `SYSTEM_HEALTH_MID`가 `NOMINAL`, `FAULT_NONE`, `RecoveryRequested=0` 보고 |
| CORE-RUN-002 | Bridge 타임아웃 | bridge HK 갱신을 `3000 ms` 이상 중단 | `SYSTEM_HEALTH_MID`가 `RECOVERY`, `FAULT_BRIDGE_TIMEOUT`, `RecoveryRequested=1` 보고 |
| CORE-RUN-003 | GPS stale 플래그 (헬스 비반영) | 신선한 bridge, attitude, local, EKF 전달; GPS `Stale=1`(또는 `Valid=0`) 설정 | `SYSTEM_HEALTH_MID`가 **`NOMINAL`, `FAULT_NONE`** 보고, `GpsValid=0` 반영 (GPS는 헬스 저하 안 함 — §12.7) |
| CORE-RUN-004 | GPS 타임아웃 (헬스 비반영) | bridge와 EKF 관련 입력은 신선하게 유지하면서 GPS 갱신을 `3000 ms` 이상 중단 | `SYSTEM_HEALTH_MID`가 **`NOMINAL`, `FAULT_NONE`** 보고, `GpsValid=0` 반영 |
| CORE-RUN-005 | EKF invalid 플래그 | 다른 입력은 신선하게 유지하면서 EKF `Valid=0` 설정 | `SYSTEM_HEALTH_MID`가 `DEGRADED`, `FAULT_EKF_INVALID` 보고 |
| CORE-RUN-006 | Local 타임아웃 | bridge, attitude, GPS, EKF는 신선하게 유지하면서 local-state 갱신을 `2000 ms` 이상 중단 | `SYSTEM_HEALTH_MID`가 `DEGRADED`, `FAULT_LOCAL_TIMEOUT` 보고 |
| CORE-RUN-007 | Attitude 타임아웃 | bridge, local, GPS, EKF는 신선하게 유지하면서 attitude-state 갱신을 `2000 ms` 이상 중단 | `SYSTEM_HEALTH_MID`가 `DEGRADED`, `FAULT_ATTITUDE_TIMEOUT` 보고 |
| CORE-RUN-008 | 우선순위 확인 | bridge 타임아웃과 GPS stale를 동시에 강제 | `SYSTEM_HEALTH_MID`가 `RECOVERY`, `FAULT_BRIDGE_TIMEOUT` 보고 |
| CORE-RUN-009 | 복구 후 정상 | CORE-RUN-002 또는 CORE-RUN-003 이후 신선한 유효 입력 재개 | 다음 헬스 평가 시 `NOMINAL` 복귀 |
| CORE-RUN-010 | 시작 워밍업 | 첫 번째 bridge HK 이전에 앱 시작 | bridge HK 도착 전까지 첫 헬스 출력에서 `RECOVERY` 보고 가능 |

### 19.3 로그 및 텔레메트리 관찰 지점

다음을 관찰한다.

- 소비자 경로 또는 텔레메트리 표시의 `SYSTEM_HEALTH_MID` 필드
- `CFS_CORE_APP_HK_TLM_MID`의 HK 필드
- EVS 이벤트 `CFS_CORE_APP Initialized`
- EVS 경로 갱신 로그
- EVS HK 로그

헬스 상태 전이는 `SYSTEM_HEALTH_MID` 텔레메트리와 EVS 이벤트 `CFS_CORE_APP_HEALTH_TRANSITION_EID (7)` 양쪽으로 확인 가능하다.
EVS 이벤트는 `HealthState` 값이 변경될 때마다 발생하며, 형식은 `CFS_CORE_APP: health %u->%u fault=%u` 이다.

## 20. 알려진 미구현 항목

다음 동작은 구현되어 있지 않으며 테스트 또는 운용 시 가정해서는 안 된다.

- 타임스탬프 *기준/출처*(time base) 유효성 검사 (미래 timestamp 거부는 구현됨 — §7.1)
- viewpoint 명령의 실제 실행 (현재는 캐시만 — §17)
- FC/센서/시리얼 장치의 능동 재설정 (bridge **앱** 재시작은 구현됨 — §14.4)

다음 항목은 이전에 미구현으로 나열됐으나 현재 구현 완료되었다.

- `FAILED` 헬스 출력 상태 → bridge 타임아웃 `FAILED_ESCALATION_MS(30000)` 초과 시 생성 (§12.9)
- 시퀀스 중복/역행/갭 감지 → `SEQ_ERR_EID`/`SEQ_GAP_EID`, `SeqRejectedCount`/`SeqGapCount` (§7.1)
- 미래 타임스탬프 거부 → `TIMESTAMP_ERR_EID`, `TimestampRejectedCount` (§7.1)
- bridge 앱 능동 재시작 → `CFE_ES_RestartApp`, 횟수 상한 없이 쿨다운만으로 빈도 제한(BL-38, §14.4)
- 앱 재시작 후 마지막 헬스 상태 지속 → `STATE_FILE_PATH` 파일 영속화 (§14.5)
- 런타임 config 적용 → `CONFIG_CMD_MID` pending/active 이중버퍼 (§17, §21.2)
- viewpoint 명령 수신 캐시 → `VIEWPOINT_CMD_MID` (§17)
- local/attitude 타임아웃 전용 오류 코드 → `FAULT_LOCAL_TIMEOUT(4)`/`FAULT_ATTITUDE_TIMEOUT(5)` (A2)
- 복구 중 10초 `NOMINAL_STABILITY_MS` 안정화 타이머 (A4)
- 헬스 상태 전이 EVS 이벤트 → `CFS_CORE_APP_HEALTH_TRANSITION_EID (7)` (A5)
- uplink_app cFS 상태 기반 라우팅 차단, CLASS_MODE/CLASS_DIAGNOSTIC 디스패치 (A5)

## 21. 시스템 수준 미구현 영역

다음 항목은 Raspberry Pi 런타임 검증 중 확인된 것으로, 운용 end-to-end 기능으로 완전히 구현되어 있지 않다.

### 21.1 LoRa uplink 전송 경로

**[2026-05-27 실물 검증 완료]**

`PC LoRa (COM7) → 무선 → Pi /dev/ttyUSB0 → lora_uplink_bridge.py → uplink_app UDP:1234 → cfs_core_app` 경로가 실물에서 end-to-end 확인되었다.

검증 시 발견 및 수정 사항:

- `lora_uplink_bridge.py`의 `MAX_PAYLOAD_LENGTH`가 `192`로 설정되어 있어 `uplink_app`이 기대하는 `212` bytes 대신 `208` bytes 패킷을 생성했다.
- `uplink_route_update_sender.py`의 `UPLINK_APP_MAX_PAYLOAD_LENGTH = 196`에 맞춰 `MAX_PAYLOAD_LENGTH`를 `196`으로 수정했다 (commit 7a63a98).

확인된 정상 동작:

- `lora_uplink_bridge.py --serial-path /dev/ttyUSB0 --allow-seq-regression` 실행 후 `serial open: /dev/ttyUSB0` 출력
- PC에서 `route-good` 전송 시 Pi에서 `forwarded uplink frame: class=2 seq=1 flags=0 payload_len=28` 출력
- cFS 로그에서 `UPLINK_APP: routed uplink class=2 seq=1 target=1 payload_len=28` 및 `CFS_CORE_APP: route updated type=1 version=1 count=2 src_seq=1` 확인

운용 시 주의사항:

- `--allow-seq-regression` 없이 운용 시 동일 sequence 번호 반복 전송은 거부된다. 실운용에서는 매 전송마다 sequence를 증가시켜야 한다.
- Pi의 LoRa 모듈은 `/dev/ttyUSB0` (CP2102 USB-UART), FC UART는 `/dev/ttyAMA0`으로 분리 확인되었다.

### 21.2 런타임 구성 적용 경로

`cfs_core_app`은 `CONFIG_CMD_MID`를 통한 런타임 config 적용이 **구현되어 있다**(§17, `CFS_CORE_APP_ProcessConfigCommand`). config 페이로드를 검증·디코딩하여 `ActiveConfig`(attitude/local/gps/ekf/bridge 타임아웃, publish 주기)를 pending/active 이중버퍼로 갱신하며, 헬스 평가(`UpdateHealth`)가 이 `ActiveConfig` 값을 사용한다.

현재 상태:

- `cfs_core_app` config 적용: **구현됨** (6개 타임아웃/주기 파라미터, checksum·범위 검증 포함)
- 다른 mission 앱(`mavlink_bridge_app`/`uplink_app`/`lora_tdm_app`)의 config 적용 end-to-end는 별도 확인 필요

의미:

- `cfs_core_app` 타임아웃/게시주기 변경 테스트가 지원된다
- route-update 테스트도 지원된다

### 21.3 LoRa downlink 안정성 — C1에서 수정

**[수정 완료 — C1에서 구현]**

`mavlink_bridge_app`의 LoRa downlink write 안정성 문제는 C1 커밋에서 수정되었다.

수정 전 증상 (구 동작):

- 반복적인 `LoRa write failed errno=11 (EAGAIN), forcing reopen`
- 일시적 backpressure와 지속적 링크 오류가 구별되지 않음

현재 구현 동작:

- LoRa 포트는 `O_NONBLOCK`으로 열기 시도 (open() 블로킹 방지)
- 열기 성공 후 `fcntl(F_SETFL, LoRaFlags & ~O_NONBLOCK)`으로 블로킹 모드로 전환
- 블로킹 모드이므로 `write()` 는 EAGAIN을 반환하지 않음
- `write()` 가 `EAGAIN` / `EWOULDBLOCK` 을 반환하면: 패킷 skip, 포트 유지 (reopen 없음)
- 그 외 `write()` 오류: EVS 로그 후 포트 닫기 및 재열기

의미:

- 일시적 backpressure → packet skip, 포트 재열기 없음
- 지속적 링크 오류 → 포트 재열기 트리거
- 두 경우가 명확히 구별된다

### 21.4 헬스 상태 가시성 — 구현 완료

**[구현 완료 — A5에서 구현]**

`CFS_CORE_APP_HEALTH_TRANSITION_EID (7)` EVS 이벤트가 `cfs_core_app_utils.c`에 구현되어 있으며, `HealthState` 값이 변경될 때마다 발생한다.
이벤트 형식: `CFS_CORE_APP: health %u->%u fault=%u`

현재 상태:

- 헬스 상태 전이는 `SYSTEM_HEALTH_MID` 텔레메트리와 EVS 이벤트 모두로 관찰 가능하다
- EVS 이벤트는 전이 발생 시 1회만 발생하며, 상태가 유지되는 동안 반복되지 않는다

Pi 런타임 로그 노출 여부는 EVS 필터 설정에 따라 달라질 수 있다.

### 21.5 오류 세부 정보 세밀도 — 구현 완료

**[구현 완료 — A2에서 구현]**

각 오류 조건은 별도의 `FaultCode` 값으로 구별된다.

현재 매핑:

| 조건 | HealthState | FaultCode |
| --- | --- | --- |
| Bridge 타임아웃 | RECOVERY | FAULT_BRIDGE_TIMEOUT (1) |
| EKF 타임아웃/무효/stale | DEGRADED | FAULT_EKF_INVALID (3) |
| Local 타임아웃/무효/stale | DEGRADED | FAULT_LOCAL_TIMEOUT (4) |
| Attitude 타임아웃/무효/stale | DEGRADED | FAULT_ATTITUDE_TIMEOUT (5) |
| uplink_app HK 타임아웃 (5s) | DEGRADED | FAULT_UPLINK_TIMEOUT (6) |
| lora_tdm_app HK 타임아웃 (5s) | DEGRADED | FAULT_LORA_TIMEOUT (7) |
| GPS 불가용 | (헬스 비반영) | `GpsStatus.TimedOut=1` 보고만, FaultCode 미설정 — §12.7. `FAULT_GPS_STALE(2)` enum은 정의되나 미생성 |
| Bridge 타임아웃 30s 초과 | FAILED | FAULT_BRIDGE_TIMEOUT (1) — §12.9 |

이전 버전에서 local/attitude/EKF 조건이 모두 `FAULT_EKF_INVALID`로 통합되었던 동작은 A2에서 수정되었다.
