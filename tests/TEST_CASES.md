# 테스트 케이스 정리

이 문서는 현재 `cfs-telemetry-app` 저장소에 구현된 unit test와 런타임 시험 항목을 정리한다.

목적:
- 어떤 테스트가 이미 구현되었는지 한눈에 확인한다.
- 각 테스트가 무엇을 검증하는지 명시한다.
- 이후 통합 테스트 또는 하드웨어 연동 시험을 설계할 때 기준 문서로 사용한다.

주의:
- 이 문서는 **구현된 테스트 목록**을 정리하는 문서이며, 요구사항 원문을 대체하지 않는다.
- 실제 요구사항과 시험 범위의 기준은 [notes/mission_app_runtime_spec.md](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/notes/mission_app_runtime_spec.md)이다.
- 하드웨어 미연결 시험은 내부 계약 검증까지만 포함하며, 실제 FC mission 반영 및 LoRa 물리 송신은 포함하지 않는다.

## Unit Test

### `cfs_core_app`

테스트 위치:
- [cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app.c)
- [cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app_cmds.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app_cmds.c)
- [cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app_dispatch.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app_dispatch.c)
- [cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app_utils.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app_utils.c)

구현된 테스트:

| 테스트 이름 | 검증 내용 |
| --- | --- |
| `CFS_CORE_APP_Init` | 앱 초기화가 성공하고 `RunStatus`가 `APP_RUN`으로 설정되는지 확인 |
| `CFS_CORE_APP_Init_SubscribeError` | `CFE_SB_Subscribe` 실패 시 init이 오류를 반환하는지 확인 |
| `CFS_CORE_APP_Noop` | `NOOP` 처리 시 command counter가 증가하는지 확인 |
| `CFS_CORE_APP_ResetCounters` | `RESET_COUNTERS` 처리 시 command/error counter가 초기화되는지 확인 |
| `CFS_CORE_APP_VerifyCmdLength` (`dispatch`) | dispatch 경로에서 길이 검증 helper 호출 결과에 따라 분기가 가능한지 확인 |
| `CFS_CORE_APP_TaskPipe_SendHk` | `SEND_HK` MID 수신 시 HK 보고 경로로 진입하는지 확인 |
| `CFS_CORE_APP_VerifyCmdLength_Impl` | 실제 `VerifyCmdLength` 구현이 정상 길이/비정상 길이를 올바르게 판단하는지 확인 |
| `CFS_CORE_APP_UpdateHealth_Nominal` | 정상 attitude/local/gps/ekf/bridge 입력에서 health가 `NOMINAL`이 되는지 확인 |
| `CFS_CORE_APP_UpdateHealth_Recovery` | bridge timeout 상태에서 health가 `RECOVERY`로 전이되고 recovery flag가 설정되는지 확인 |
| `CFS_CORE_APP_ProcessStateMessage_RouteUpdate` | `ROUTE_UPDATE_MID` 입력 시 mission route cache가 갱신되는지 확인 |
| `CFS_CORE_APP_ReportHousekeeping` | HK 생성 경로가 실행되고 route/HK 필드 계산 경로가 동작하는지 확인 |

### `uplink_app`

테스트 위치:
- [uplink_app/unit-test/coveragetest/coveragetest_uplink_app.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/uplink_app/unit-test/coveragetest/coveragetest_uplink_app.c)
- [uplink_app/unit-test/coveragetest/coveragetest_uplink_app_cmds.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/uplink_app/unit-test/coveragetest/coveragetest_uplink_app_cmds.c)
- [uplink_app/unit-test/coveragetest/coveragetest_uplink_app_dispatch.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/uplink_app/unit-test/coveragetest/coveragetest_uplink_app_dispatch.c)
- [uplink_app/unit-test/coveragetest/coveragetest_uplink_app_utils.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/uplink_app/unit-test/coveragetest/coveragetest_uplink_app_utils.c)

구현된 테스트:

| 테스트 이름 | 검증 내용 |
| --- | --- |
| `UPLINK_APP_Init` | 앱 초기화 성공 및 `RunStatus` 설정 확인 |
| `UPLINK_APP_Init_SubscribeError` | subscribe 실패 시 init 오류 반환 확인 |
| `UPLINK_APP_Noop` | `NOOP` 처리 시 command counter 증가 확인 |
| `UPLINK_APP_ResetCounters` | command/error/accept/reject/routing failure counter 초기화 확인 |
| `UPLINK_APP_ProcessUplink_Accept` | 정상 config class 입력이 수락되고 `LastCommandResult`가 `ROUTED`로 반영되는지 확인 |
| `UPLINK_APP_ProcessUplink_RouteUpdate` | 정상 route update 명령이 core target으로 라우팅되는지 확인 |
| `UPLINK_APP_VerifyCmdLength` (`dispatch`) | dispatch 경로에서 길이 검증 helper 결과에 따라 분기 가능한지 확인 |
| `UPLINK_APP_TaskPipe_SendHk` | `SEND_HK` MID 수신 시 HK 보고 경로 진입 확인 |
| `UPLINK_APP_VerifyCmdLength_Impl` | 실제 `VerifyCmdLength` 구현이 정상 길이/비정상 길이를 올바르게 판단하는지 확인 |
| `UPLINK_APP_ValidateProxyCommand` | protocol version 오류와 정상 route class 입력을 올바르게 판정하는지 확인 |
| `UPLINK_APP_ParseRouteUpdatePayload` | 정상 route payload를 파싱하고, 잘못된 고도 route를 거부하는지 확인 |
| `UPLINK_APP_ResolveRouteTarget` | command class별 route target(core/downlink/none)이 올바르게 반환되는지 확인 |
| `UPLINK_APP_ReportHousekeeping` | HK 생성 경로가 실행되는지 확인 |

### `lora_fc_downlink_app`

테스트 위치:
- [lora_fc_downlink_app/unit-test/coveragetest/coveragetest_lora_fc_downlink_app.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/lora_fc_downlink_app/unit-test/coveragetest/coveragetest_lora_fc_downlink_app.c)
- [lora_fc_downlink_app/unit-test/coveragetest/coveragetest_lora_fc_downlink_app_cmds.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/lora_fc_downlink_app/unit-test/coveragetest/coveragetest_lora_fc_downlink_app_cmds.c)
- [lora_fc_downlink_app/unit-test/coveragetest/coveragetest_lora_fc_downlink_app_dispatch.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/lora_fc_downlink_app/unit-test/coveragetest/coveragetest_lora_fc_downlink_app_dispatch.c)
- [lora_fc_downlink_app/unit-test/coveragetest/coveragetest_lora_fc_downlink_app_utils.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/lora_fc_downlink_app/unit-test/coveragetest/coveragetest_lora_fc_downlink_app_utils.c)

구현된 테스트:

| 테스트 이름 | 검증 내용 |
| --- | --- |
| `LORA_FC_DOWNLINK_APP_Init` | 앱 초기화 성공 및 `RunStatus` 설정 확인 |
| `LORA_FC_DOWNLINK_APP_Init_SubscribeError` | subscribe 실패 시 init 오류 반환 확인 |
| `LORA_FC_DOWNLINK_APP_NoopCmd` | `NOOP` 처리 시 command counter 증가 확인 |
| `LORA_FC_DOWNLINK_APP_ResetCountersCmd` | command/error/downlink counter 초기화 확인 |
| `LORA_FC_DOWNLINK_APP_VerifyCmdLength` | command MID + function code 기반 길이 검증 구현 확인 |
| `LORA_FC_DOWNLINK_APP_TaskPipe` | `SEND_HK`, command, FC attitude input이 올바른 처리 경로로 분기되는지 확인 |
| `LORA_FC_DOWNLINK_APP_ReportHousekeeping` | HK payload에 downlink count, valid flag, health state가 반영되는지 확인 |
| `LORA_FC_DOWNLINK_APP_ProcessInputMessage` | attitude 입력과 system health 입력이 timestamp/valid/health/downlink count를 갱신하는지 확인 |

## 런타임 시험

### 기본 명령 시험

검증 스크립트/도구:
- `cmdUtil`

구현 및 확인된 항목:

| 시험 항목 | 검증 내용 | 확인 방법 |
| --- | --- | --- |
| `UPLINK_APP NOOP` | uplink command path가 살아 있고 `NOOP`가 처리되는지 확인 | EVS 로그에서 `UPLINK_APP: NOOP` 확인 |
| `UPLINK_APP RESET` | uplink command path가 살아 있고 `RESET`이 처리되는지 확인 | EVS 로그에서 `UPLINK_APP: RESET` 확인 |

### Route Update 시험

시험 도구:
- [tools/uplink_route_update_sender.py](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/tools/uplink_route_update_sender.py)

구현 및 확인된 항목:

| 시험 항목 | 검증 내용 | 기대 결과 |
| --- | --- | --- |
| `route-good` | 정상 mission extension route payload 수신/검증/publish | `UPLINK_APP: routed ...`, `CFS_CORE_APP: route updated type=1 ...` |
| `route-landing` | 정상 landing route payload 수신/검증/publish | `UPLINK_APP: routed ...`, `CFS_CORE_APP: route updated type=2 ...` |
| `route-bad-alt` | 고도 2m 미만 route 거부 | `UPLINK_APP: invalid route update payload ...`, core route update 없음 |
| `route-bad-distance` | waypoint 간 거리 제약 위반 route 거부 | invalid route 로그, core route update 없음 |

이 시험으로 현재 확인된 것:
- `ROUTE_UPDATE` payload 파싱
- route payload 검증
- `ROUTE_UPDATE_MID` publish
- `cfs_core_app` route cache 반영
- 잘못된 route payload reject

## 아직 미구현 또는 미검증인 테스트

현재 문서에 정의됐지만 아직 unit test 또는 런타임 시험으로 충분히 보강되지 않은 항목:

- `uplink_app` CRC 검사
- `uplink_app` sequence 증가/중복/replay 검사
- `uplink_app` 권한 수준 검사
- `uplink_app` viewpoint payload 상세 검증
- `cfs_core_app`의 `GPS stale -> DEGRADED` 분기
- `cfs_core_app`의 `EKF invalid -> DEGRADED` 분기
- `lora_fc_downlink_app` mock sink 기반 송신 성공/실패 시험
- 실제 FC mission 반영 직전 candidate packet 생성 시험

## 운영 원칙

- unit test는 각 app 내부 `unit-test/`에 유지한다.
- 통합 또는 런타임 시험 관련 문서와 스크립트는 루트 `tests/` 및 `tools/`에서 관리한다.
- 하드웨어 미연결 시험은 내부 계약 검증까지만 포함한다.
- 실제 FC mission 반영, 비행 결과, LoRa 물리 송신 성능은 별도 하드웨어 통합 시험 범위로 둔다.
