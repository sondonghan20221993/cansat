# cfs_core_app 동작 명세

## 1. 목적

이 문서는 현재 이 저장소에서 구현된 `cfs_core_app`의 동작을 정의한다.
코드와 정합된 명세서로, 리뷰, 통합, 테스트 수행에 활용한다.

이 문서는 코드에 존재하지 않는 목표 동작을 정의하지 않는다.
코드와 이 문서가 서로 다르면, 코드를 조사의 원본으로 취급해야 한다.

## 2. 범위

이 명세는 다음을 다룬다.

- `cfs_core_app` 메시지 구독
- 각 구독 메시지 수신 시 갱신되는 내부 캐시 상태
- `SYSTEM_HEALTH_MID` 출력 필드 및 게시 타이밍
- 헬스 상태 분류 로직
- bridge, GPS, EKF, local-state, attitude-state 입력의 타임아웃 처리
- 시작 후 및 입력 손실 후 동작
- 기존 단위 테스트 커버리지 및 런타임 검증 권고

이 명세는 다음을 다루지 않는다.

- `mavlink_bridge_app` 내부 파싱 규칙
- `downlink_app` 패킷 형식
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

- bridge HK, FC 상태 메시지, 경로 갱신, 앱 명령/HK 메시지 구독
- 수신된 최신 bridge, attitude, local, GPS, EKF, mission-route, landing-route 데이터 캐시
- 구독된 상태 메시지 수신 시마다 시스템 헬스 재계산
- 입력이 없을 때 주기적으로 시스템 헬스 재계산
- `SYSTEM_HEALTH_MID` 게시
- HK 요청 수신 시 HK 텔레메트리 게시

`cfs_core_app`은 현재 다음을 수행하지 않는다.

- 다른 앱 재시작
- 시리얼 장치 재열기
- 외부 컴포넌트 재설정
- 별도 복구 명령 게시
- 재시작 후 헬스 상태 지속

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
| `CFS_CORE_APP_FC_EKF_LOCAL_STATE_MID_VALUE` | `0x1905` | FC local-state 입력 |
| `CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE` | `0x1906` | FC attitude-state 입력 |
| `CFS_CORE_APP_FC_GPS_RAW_STATE_MID_VALUE` | `0x1907` | FC GPS-state 입력 |
| `CFS_CORE_APP_FC_EKF_STATUS_MID_VALUE` | `0x1908` | FC EKF-status 입력 |
| `ROUTE_UPDATE_MID` | `0x190B` | 경로 갱신 입력 |

### 6.2 게시 메시지

| 심볼 | 값 | 목적 |
| --- | --- | --- |
| `CFS_CORE_APP_HK_TLM_MID` | `0x08C0` | HK 텔레메트리 |
| `SYSTEM_HEALTH_MID` | `0x1904` | 시스템 헬스 텔레메트리 |

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

시퀀스 단조 검사는 구현되어 있지 않다.
타임스탬프 출처 유효성 검사는 구현되어 있지 않다.
이 텔레메트리 입력에 대한 페이로드 길이 검증은 수행되지 않는다.

### 7.2 Bridge HK 입력

`cfs_core_app`이 소비하는 `CFS_CORE_APP_BridgeHkMirror_t` 필드는 다음과 같다.

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
| `RouteType` | `uint8` | mission-route 또는 landing-route 선택 |
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

- mission-route 캐시 1개
- landing-route 캐시 1개

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

## 10. 헬스 출력 계약

`SYSTEM_HEALTH_MID`는 다음 필드를 게시한다.

| 필드 | 의미 |
| --- | --- |
| `Seq` | 모든 헬스 게시 시마다 증가하는 단조 앱-로컬 게시 카운터 |
| `TimestampMs` | 게시 시점의 cFE 시간 (밀리초) |
| `LastValidInputTimestampMs` | 수신된 attitude/local/GPS/EKF 캐시 중 최대 타임스탬프. 아무것도 수신되지 않은 경우 `NowMs` |
| `HealthState` | 현재 구현에서 `NOMINAL`, `DEGRADED`, 또는 `RECOVERY` |
| `FaultCode` | `NONE`, `BRIDGE_TIMEOUT`, `GPS_STALE`, `EKF_INVALID`, `LOCAL_TIMEOUT`, 또는 `ATTITUDE_TIMEOUT` |
| `RecoveryRequested` | bridge 타임아웃 조건에서만 `1`, 그 외에는 `0` |

현재 구현은 매 게시 전 텔레메트리 구조체를 0으로 초기화한다.
현재 구현은 매 게시 전 메시지 헤더를 재초기화한다.

## 11. 게시 조건

### 11.1 즉시 헬스 게시

`cfs_core_app`은 다음 처리 직후 `SYSTEM_HEALTH_MID`를 즉시 게시한다.

- bridge HK
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

### 12.5 우선순위 5: GPS 불가용

조건:

- GPS state 만료
- 또는 GPS `Valid == 0`
- 또는 GPS `Stale != 0`

출력:

- `HealthState = CFS_CORE_APP_HEALTH_DEGRADED`
- `FaultCode = CFS_CORE_APP_FAULT_GPS_STALE`
- `RecoveryRequested = 0`

### 12.6 우선순위 6: 정상

조건:

- 이전 조건 중 어느 것도 해당하지 않음

출력:

- `HealthState = CFS_CORE_APP_HEALTH_NOMINAL`
- `FaultCode = CFS_CORE_APP_FAULT_NONE`
- `RecoveryRequested = 0`

### 12.7 미사용 enum 상태

`CFS_CORE_APP_HEALTH_FAILED`는 메시지 정의에 정의되어 있으나 현재 코드에서 생성되지 않는다.

## 13. 타임아웃 및 오류 처리 상세

### 13.1 Bridge 타임아웃

`bridge timeout`은 FC 상태 메시지의 도착 시각이 아닌 bridge HK의 `LastRxTimestampMs`를 기준으로 평가된다.

효과:

- `RECOVERY` 생성
- `FAULT_BRIDGE_TIMEOUT` 생성
- `RecoveryRequested = 1` 설정
- bridge 타임아웃이 더 높은 우선순위를 가지므로 GPS 및 EKF 오류 보고 억제

### 13.2 GPS stale

`gps stale`은 다음을 모두 포함한다.

- GPS 메시지가 한 번도 수신되지 않음
- GPS 타임스탬프 경과 시간이 `3000 ms` 초과
- GPS `Valid == 0`
- GPS `Stale != 0`

효과:

- `DEGRADED` 생성
- `FAULT_GPS_STALE` 생성
- `RecoveryRequested` 미설정

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

유일하게 구현된 복구 조치는 다음이다.

- bridge 타임아웃 시 `SYSTEM_HEALTH_MID`에 `RecoveryRequested = 1` 설정

추가 복구 부작용은 구현되어 있지 않다.

## 15. HK 동작

HK 요청 시 앱은 다음을 보고한다.

- 명령 카운터
- 명령 오류 카운터
- mission route waypoint 수
- landing route waypoint 수
- 게시 횟수
- 마지막 게시 타임스탬프
- 마지막 경로 갱신 타임스탬프
- 총 경로 갱신 횟수

앱은 경로 관련 HK 필드를 요약하는 EVS 정보 이벤트도 발생시킨다.

## 16. 경로 처리 규칙

mission route와 landing route는 독립적으로 캐시된다.
허용된 경로 갱신 시마다 선택된 경로 캐시의 `UpdateCount`가 증가한다.

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

앱은 현재 다음만 지원한다.

- NOOP
- 카운터 리셋

텔레메트리 입력은 명령 길이 검사로 유효성을 검사하지 않는다.
알 수 없는 명령 코드는 명령 오류 카운터를 증가시킨다.

## 18. 기존 단위 테스트 커버리지

현재 단위 테스트는 다음을 검증한다.

- HK 함수 실행
- 명령 길이 검증 성공 및 실패
- 정상 헬스 분류
- bridge 타임아웃 헬스 분류
- GPS stale 헬스 분류
- EKF invalid 헬스 분류
- `FAULT_EKF_INVALID`로서의 local 타임아웃 분류
- `FAULT_EKF_INVALID`로서의 attitude 타임아웃 분류
- mission-route 캐시 갱신
- landing-route 캐시 갱신
- bridge HK 캐시 갱신
- service prototype 실행 경로
- 초기화 성공
- 구독 오류 시 초기화 실패
- NOOP 명령
- 카운터 리셋 명령

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
| CORE-RUN-003 | GPS stale 플래그 | 신선한 bridge, attitude, local, EKF 전달; GPS `Stale=1` 설정 | `SYSTEM_HEALTH_MID`가 `DEGRADED`, `FAULT_GPS_STALE`, `RecoveryRequested=0` 보고 |
| CORE-RUN-004 | GPS 타임아웃 | bridge와 EKF 관련 입력은 신선하게 유지하면서 GPS 갱신을 `3000 ms` 이상 중단 | `SYSTEM_HEALTH_MID`가 `DEGRADED`, `FAULT_GPS_STALE` 보고 |
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

- `FAILED` 헬스 출력 상태
- 시퀀스 갭 또는 중복 감지
- 타임스탬프 기준 유효성 검사
- bridge 또는 peer 앱의 능동적 재시작
- 앱 재시작 후 마지막 헬스 상태 지속

다음 항목은 이전에 미구현으로 나열됐으나 현재 구현 완료되었다.

- local 타임아웃 전용 오류 코드 → `FAULT_LOCAL_TIMEOUT = 4` (A2에서 구현)
- attitude 타임아웃 전용 오류 코드 → `FAULT_ATTITUDE_TIMEOUT = 5` (A2에서 구현)
- 복구 중 디바운스 또는 대기 시간 로직 → 10초 `NOMINAL_STABILITY_MS` 안정화 타이머 (A4에서 구현)
- 헬스 상태 전이 EVS 이벤트 → `CFS_CORE_APP_HEALTH_TRANSITION_EID (7)` (A5에서 구현)
- uplink_app cFS 상태 기반 라우팅 차단 → `SYSTEM_HEALTH_MID` 구독 및 §18.10 블로킹 매트릭스 (A5에서 구현)
- uplink_app CLASS_MODE/CLASS_DIAGNOSTIC 디스패치 → `ForwardModeCommand` / `ForwardDiagnosticCommand` (A5에서 구현)

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

`UPLINK_APP_CLASS_CONFIG`는 인식된 명령 클래스이나, 게시 주기, 타임아웃 값 등 mission-app 런타임 파라미터에 구성 페이로드를 실제로 적용하는 구현은 확인되지 않았다.

현재 상태:

- config 클래스 수락은 명령 검증 수준에서 존재한다
- config 페이로드를 디코딩하여 `cfs_core_app`, `telemetry_app` 또는 다른 mission 앱의 활성 설정을 갱신하는 end-to-end 구현은 확인되지 않았다

의미:

- route-update 테스트는 현재 지원된다
- 출력 주기 또는 타임아웃 변경 테스트는 현재 구현된 운용 기능으로 지원되지 않는다

### 21.3 LoRa downlink 안정성

`mavlink_bridge_app`의 LoRa downlink 출력은 현재 런타임 조건에서 아직 안정적이지 않다.

확인된 런타임 증상:

- 반복적인 `LoRa write failed errno=11, forcing reopen`

관련 구현 동작:

- LoRa 포트는 `O_NONBLOCK`으로 열린다
- 단일 `write()` 실패 시 즉시 닫기 및 재열기가 트리거된다
- 일시적 backpressure와 지속적 링크 오류가 구별되지 않는다

의미:

- LoRa downlink 경로는 존재하며 전송을 시도한다
- LoRa downlink 경로는 완전히 운용 가능한 것으로 취급하기에 아직 충분히 안정적이지 않다

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
| EKF 타임아웃/무효/stale | DEGRADED | FAULT_EKF_INVALID (2) |
| Local 타임아웃/무효/stale | DEGRADED | FAULT_LOCAL_TIMEOUT (4) |
| Attitude 타임아웃/무효/stale | DEGRADED | FAULT_ATTITUDE_TIMEOUT (5) |
| GPS 불가용 | DEGRADED | FAULT_GPS_STALE (3) |

이전 버전에서 local/attitude/EKF 조건이 모두 `FAULT_EKF_INVALID`로 통합되었던 동작은 A2에서 수정되었다.
