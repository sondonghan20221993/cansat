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
| `CFS_CORE_APP_UpdateHealth_GpsStale` | GPS stale 입력에서 health가 `DEGRADED`로 전이되고 `GPS_STALE` fault가 설정되는지 확인 |
| `CFS_CORE_APP_UpdateHealth_EkfInvalid` | EKF invalid 입력에서 health가 `DEGRADED`로 전이되는지 확인 |
| `CFS_CORE_APP_UpdateHealth_LocalTimeout` | local state timeout에서 health가 `DEGRADED`로 전이되는지 확인 |
| `CFS_CORE_APP_UpdateHealth_AttitudeTimeout` | attitude timeout에서 health가 `DEGRADED`로 전이되는지 확인 |
| `CFS_CORE_APP_ProcessStateMessage_RouteUpdate` | `ROUTE_UPDATE_MID` 입력 시 mission route cache가 갱신되는지 확인 |
| `CFS_CORE_APP_ProcessStateMessage_LandingRouteUpdate` | `ROUTE_UPDATE_MID`의 landing route 입력 시 landing route cache가 갱신되는지 확인 |
| `CFS_CORE_APP_ProcessStateMessage_BridgeHk` | bridge HK 입력 시 bridge cache가 갱신되는지 확인 |
| `CFS_CORE_APP_ReportHousekeeping` | HK 생성 경로가 실행되고 route/HK 필드 계산 경로가 동작하는지 확인 |
| `CFS_CORE_APP_ServicePrototype` | prototype service 경로가 호출 가능하고 publish 주기 계산 경로를 타는지 확인 |

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
| `UPLINK_APP_ProcessUplink_Reject` | validate reject 경로에서 reject/error counter와 상태가 반영되는지 확인 |
| `UPLINK_APP_ProcessUplink_RouteMiss` | route target을 찾지 못하는 경우 routing failure가 증가하는지 확인 |
| `UPLINK_APP_ProcessUplink_RouteParseReject` | route payload 파싱 실패 시 reject 처리되는지 확인 |
| `UPLINK_APP_ProcessUplink_RoutePublishFail` | route publish 실패 시 routing failure 및 오류 상태가 반영되는지 확인 |
| `UPLINK_APP_VerifyCmdLength` (`dispatch`) | dispatch 경로에서 길이 검증 helper 결과에 따라 분기 가능한지 확인 |
| `UPLINK_APP_TaskPipe_SendHk` | `SEND_HK` MID 수신 시 HK 보고 경로 진입 확인 |
| `UPLINK_APP_VerifyCmdLength_Impl` | 실제 `VerifyCmdLength` 구현이 정상 길이/비정상 길이를 올바르게 판단하는지 확인 |
| `UPLINK_APP_ValidateProxyCommand` | protocol version 오류, 잘못된 class, zero payload route/viewpoint, 과도한 payload 길이를 올바르게 거부하는지 확인 |
| `UPLINK_APP_ParseRouteUpdatePayload` | 정상 route payload를 파싱하고 잘못된 route type, waypoint 수, 무한대 좌표, 고도 오류, 최소/최대 거리 위반을 거부하는지 확인 |
| `UPLINK_APP_ResolveRouteTarget` | command class별 route target(core/downlink/none)이 올바르게 반환되는지 확인 |
| `UPLINK_APP_UpdateStatusTelemetry` | accepted/rejected/routing failure count와 last command/result/link state/route target이 상태 텔레메트리에 반영되는지 확인 |
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
| `LORA_FC_DOWNLINK_APP_ProcessInputMessage` | attitude, local, gps, ekf, system health 입력이 timestamp/valid/health/downlink count와 packet type을 갱신하는지 확인 |

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

### PC 수신 시험

시험 목적:
- Raspberry Pi에서 수신한 FC 텔레메트리가 PC까지 도달하는지 검증한다.
- `mavlink_bridge_app -> cFS publish -> downlink 경로 -> PC 수신기` 구간의 연속성을 확인한다.
- FC 미송신과 브리지/다운링크 문제를 구분할 수 있도록 판정 기준을 명확히 한다.

사전 조건:
- Raspberry Pi의 `native_std/cpu1`가 `OPERATIONAL` 상태다.
- `MAV_BRIDGE_APP`가 `/dev/serial0`를 `57600` baud로 open했다.
- FC와 Raspberry Pi 사이 UART 링크가 연결되어 있다.
- Pi 로그에서 다음 중 하나 이상이 이미 확인됐다.
  - `ATTITUDE (30)`
  - `GPS_RAW_INT (24)`
  - `GLOBAL_POSITION_INT (33)`
  - `EKF_STATUS_REPORT (193)`
- PC에는 LoRa 수신기 또는 최종 수신기와 그에 맞는 시리얼/로그 확인 도구가 준비되어 있다.

입력:
- FC가 송신하는 MAVLink 텔레메트리
- Raspberry Pi에서 실행 중인 baseline app set
- PC 측 수신기에서 출력하는 raw 프레임 또는 디코드 로그

절차:
1. Raspberry Pi에서 `core-cpu1` 로그를 저장하면서 실행한다.
2. Pi 로그에서 bridge가 telemetry stream request를 수행했고 목표 메시지를 수신 중인지 확인한다.
3. 같은 시간대에 PC 측 수신기 로그를 저장한다.
4. FC 자세 또는 위치가 변하지 않는 정지 상태에서 PC가 최소 30초 동안 프레임을 계속 수신하는지 확인한다.
5. 가능하면 기체를 소폭 움직이거나 자세를 바꿔서 PC에서 연속 샘플 간 값 변화가 보이는지 확인한다.
6. Pi 로그 시각과 PC 로그 시각을 대조해 동일 시험 구간에서 양쪽 모두 수신이 지속됐는지 확인한다.

관찰 항목:
- Pi 로그의 bridge 입력 메시지 존재 여부
- PC 측 수신 프레임 존재 여부
- PC 측 수신 중단 또는 간헐 손실 여부
- 연속 샘플 간 자세/위치 값 변화 여부

기대 결과:
- Pi 로그에 `ATTITUDE`, `GPS_RAW_INT`, `GLOBAL_POSITION_INT`, `EKF_STATUS_REPORT` 중 하나 이상이 반복적으로 나타난다.
- PC 측 로그에 동일 시험 시간대의 downlink 수신 흔적이 반복적으로 나타난다.
- 정지 상태에서도 최소 30초 동안 수신 공백 없이 프레임이 계속 도착한다.
- 자세 또는 위치를 실제로 변화시키면 PC 측 연속 샘플 값에도 변화가 반영된다.

판정 기준:
- `PASS`: Pi에서 bridge 입력이 반복 확인되고, 같은 구간에 PC에서도 수신이 반복 확인되며, 값 변화 시험 시 PC 데이터가 함께 변한다.
- `FAIL-PI-IN`: Pi에서 목표 메시지 수신이 보이지 않는다. 이 경우 PC 수신 시험 실패 원인은 FC 송신 조건 또는 Pi bridge 입력 단계다.
- `FAIL-DOWNLINK`: Pi에서는 목표 메시지 수신이 반복 확인되지만 PC에서 수신이 없다. 이 경우 원인은 `cFS publish -> downlink -> PC` 구간이다.
- `FAIL-DATA-STALE`: PC에서 수신은 되지만 자세/위치 변화 시험에서 값이 계속 고정된다. 이 경우 downlink payload 매핑 또는 갱신 주기를 점검해야 한다.

에러 및 엣지 케이스:
- `LOCAL_POSITION_NED (32)`가 보이지 않아도 즉시 실패로 판정하지 않는다. 현재 FC local-position 생성 조건 미충족이면 정상일 수 있다.
- `HEARTBEAT (0)`와 `TIMESYNC (111)`만 보이면 FC companion stream 요청이 충분히 적용되지 않은 것으로 보고 PC 수신 시험을 계속 진행하지 않는다.
- PC 수신기가 연결됐지만 수신 로그에 타임스탬프가 없으면 Pi 로그와 시간 대조가 불가능하므로 시험 결과를 보류한다.
- PC 수신기 버퍼 누락 가능성을 줄이기 위해 동일 시험 구간 로그는 파일로 저장해야 한다.

## 아직 미구현 또는 미검증인 테스트

현재 문서에 정의됐지만 아직 unit test 또는 런타임 시험으로 충분히 보강되지 않은 항목:

- `uplink_app` CRC 검사
- `uplink_app` sequence 증가/중복/replay 검사
- `uplink_app` 권한 수준 검사
- `uplink_app` viewpoint payload 상세 검증
- `lora_fc_downlink_app` mock sink 기반 송신 성공/실패 시험
- 실제 FC mission 반영 직전 candidate packet 생성 시험

## 운영 원칙

- unit test는 각 app 내부 `unit-test/`에 유지한다.
- 통합 또는 런타임 시험 관련 문서와 스크립트는 루트 `tests/` 및 `tools/`에서 관리한다.
- 하드웨어 미연결 시험은 내부 계약 검증까지만 포함한다.
- 실제 FC mission 반영, 비행 결과, LoRa 물리 송신 성능은 별도 하드웨어 통합 시험 범위로 둔다.
