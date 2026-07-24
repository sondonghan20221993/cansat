# 테스트 케이스 정리

이 문서는 `cfs-telemetry-app` 저장소의 테스트 계획, 구현 현황, 미구현 항목을 정리한다.

## 테스트 레이어 구조

| 레이어 | 도구 | 목적 | 작성 시점 |
|---|---|---|---|
| **단위테스트** | cFS coverage test (C) | 공개 API 기준 함수 수준 검증 | 기능 구현 시 함께 |
| **통합테스트** | Python pytest (`tests/`) | 큰 기능 완성 시 end-to-end 흐름 검증 | 기능 단위 완성 시 |
| **런타임 시험** | 실물/도구 직접 실행 | 하드웨어 연동 최종 검증 | Pi 연결 환경에서 |

**단위테스트 원칙:**
- 기능 구현과 동시에 작성
- 공개 API만 호출 (static 함수 노출 금지)
- 하드웨어 의존 없음

**통합테스트 원칙:**
- 큰 기능(LoRa 포팅, bridge 경로 등) 완성 시 작성
- Python pytest + PTY/mock serial 기반
- 실제 serial 없이 검증 가능한 범위까지

---

## Unit Test 현황 요약

| 앱 | 테스트 수 | assertion 수 | 마지막 확인 |
|---|---|---|---|
| `cfs_core_app` | ~92 | — | 2026-06-16 |
| `uplink_app` | 35 | 63+ | 2026-06-07 |
| ~~`lora_fc_downlink_app`~~ (baseline 제거됨, 2026-06-16 — `lora_tdm_app`으로 대체) | 14 | 40+ | 2026-06-07 |
| `lora_tdm_app` (baseline 등록됨, 2026-06-16) | 75 (4개 testrunner) | — | 2026-06-16 |
| `mavlink_bridge_app` | 25 | — | 2026-06-02 |

> **2026-06-15 통합 변경 (Task A/B/C)** — 아래 항목은 unit-test 갱신 대기(coveragetest 미반영):
> - **명명**: `.so` = `lora_fc_dl_app.so`, entry = `LORA_FC_DL_Main`, app명 = `LORA_FC_DOWNLINK`
>   (OSAL `OS_MAX_FILE_NAME`/`OS_MAX_API_NAME` = 20 한계 대응). 자세히는 `notes/integration_steps.md §4`.
> - **Task B (포트 충돌)**: `lora_fc_downlink_app`이 RX에서 "UP,..." 원문을
>   `UPLINK_RAW_MID`(0x1909)로 publish(`ForwardUplinkFrame`). `uplink_app`은 serial 직접
>   open 제거 → `UPLINK_RAW_MID` 구독 → `UPLINK_APP_ParseLoRaFrame` → `ProcessUplink`.
>   seq 거부는 `ProcessUplink`의 `IsSequenceAccepted`(영구 `LastAcceptedSequence`)가 유지.
> - **Task C (starvation)**: `lora_fc_downlink_app` CommandPipe depth 10→32
>   (300ms 블로킹 RX 윈도우 동안 FC 누적 대비). `lora_tdm_app`은 startup에서 제거.

---

## Unit Test 상세

### `cfs_core_app`

테스트 위치:
- `cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app.c`
- `cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app_cmds.c`
- `cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app_dispatch.c`
- `cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app_utils.c`

#### `coveragetest_cfs_core_app.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `CFS_CORE_APP_Init` | 앱 초기화 성공 및 `RunStatus == APP_RUN` 확인 |
| `CFS_CORE_APP_Init_SubscribeError` | `CFE_SB_Subscribe` 실패 시 오류 반환 확인 |

#### `coveragetest_cfs_core_app_cmds.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `CFS_CORE_APP_Noop` | `NOOP` 처리 시 command counter 증가 |
| `CFS_CORE_APP_ResetCounters` | `RESET_COUNTERS` 처리 시 command/error counter 초기화 |

#### `coveragetest_cfs_core_app_dispatch.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `CFS_CORE_APP_VerifyCmdLength` | dispatch 경로에서 길이 검증 helper 분기 |
| `CFS_CORE_APP_TaskPipe_SendHk` | `SEND_HK` MID 수신 시 HK 보고 경로 진입 |

#### `coveragetest_cfs_core_app_utils.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `CFS_CORE_APP_VerifyCmdLength_Impl` | 정상/비정상 길이 판단 |
| `CFS_CORE_APP_UpdateHealth_Nominal` | 전 입력 정상 시 health `NOMINAL` |
| `CFS_CORE_APP_UpdateHealth_Recovery` | bridge timeout → health `RECOVERY`, recovery flag 설정 |
| `CFS_CORE_APP_UpdateHealth_GpsStale` | GPS stale → health **`NOMINAL`** (GPS 헬스 비반영 — §12.7; `GpsStatus.TimedOut=1`만 보고) |
| `CFS_CORE_APP_UpdateHealth_EkfInvalid` | EKF invalid → health `DEGRADED`, fault `EKF_INVALID` |
| `CFS_CORE_APP_UpdateHealth_LocalTimeout` | local position timeout → health `DEGRADED`, fault `LOCAL_TIMEOUT` |
| `CFS_CORE_APP_UpdateHealth_LocalInvalid` | local position invalid → health `DEGRADED`, fault `LOCAL_TIMEOUT` |
| `CFS_CORE_APP_UpdateHealth_LocalStale` | local position stale → health `DEGRADED`, fault `LOCAL_TIMEOUT` |
| `CFS_CORE_APP_UpdateHealth_AttitudeTimeout` | attitude timeout → health `DEGRADED`, fault `ATTITUDE_TIMEOUT` |
| `CFS_CORE_APP_UpdateHealth_AttitudeInvalid` | attitude invalid → health `DEGRADED`, fault `ATTITUDE_TIMEOUT` |
| `CFS_CORE_APP_UpdateHealth_AttitudeStale` | attitude stale → health `DEGRADED`, fault `ATTITUDE_TIMEOUT` |
| `CFS_CORE_APP_UpdateHealth_NominalStabilization` | DEGRADED → NOMINAL 전이 시 안정화 구간(stabilization window) 확인 |
| `CFS_CORE_APP_UpdateHealth_InputStatus` | attitude/local/gps/ekf/bridge 상태별 status 필드가 올바르게 반영되는지 확인 |
| `CFS_CORE_APP_UpdateHealth_HealthTransition` | 상태 변화 시 EVS 이벤트가 1회 발생하고, 동일 상태 유지 시 재발생하지 않는지 확인 |
| `CFS_CORE_APP_ProcessStateMessage_RouteUpdate` | `ROUTE_UPDATE_MID` 수신 → mission route cache 갱신 |
| `CFS_CORE_APP_ProcessStateMessage_LandingRouteUpdate` | `ROUTE_UPDATE_MID` landing type → landing route cache 갱신 |
| `CFS_CORE_APP_ProcessStateMessage_BridgeHk` | bridge HK 수신 → bridge state cache 갱신 (2026-07-15: `shared_msgs/bridge_hk_msg.h`(`BRIDGE_HK_TLM_t`) 공유 정의로 로컬 fake struct 제거 — `notes/temp/mirror_struct_layout_audit.md` TC-MRG-BRIDGEHK-1/TC-MRG-COMMON-3) |

> **2026-06-16 추가 테스트** (config·seq/timestamp·bridge 재시작·영속화·FAILED 기능 반영). 위 목록은 구버전이며 실제 coveragetest는 아래 그룹을 포함한다.

추가 헬스 분류:
| 테스트 | 검증 |
|---|---|
| `UpdateHealth_Failed` / `UpdateHealth_FailedRecovery` | bridge timeout `FAILED_ESCALATION_MS(30s)` 초과 → `FAILED` 및 복구 |
| `UpdateHealth_LocalInvalid` / `LocalStale` | local invalid/stale → `FAULT_LOCAL_TIMEOUT` |
| `UpdateHealth_AttitudeInvalid` / `AttitudeStale` | attitude invalid/stale → `FAULT_ATTITUDE_TIMEOUT` |
| `UpdateHealth_GPS_Timeout` | GPS 타임아웃 → **`NOMINAL`** (헬스 비반영, §12.7) |
| `UpdateHealth_UplinkTimeout` / `LoraTimeout` | uplink/lora HK 5s 타임아웃 → `DEGRADED`, `FAULT_UPLINK_TIMEOUT(6)`/`FAULT_LORA_TIMEOUT(7)` |
| `UpdateHealth_Priority_{BridgeOverGps,EkfOverLocal,LocalOverAttitude,AttitudeOverGps}` | 우선순위 사다리 |
| `UpdateHealth_NominalStabilization` / `StabilityTimerReset` / `RecoveryToNominal` | 10s 안정화 타이머 |
| `UpdateHealth_Startup_NoBridge` | 시작 시 bridge 미수신 → RECOVERY |
| `UpdateHealth_InputStatus` / `HealthTransition` / `PeriodicRateLimit` | per-input status, 전이 EVS 1회, 주기 rate limit |

seq/timestamp 검사:
| 테스트 | 검증 |
|---|---|
| `SeqCheck_Normal` / `Duplicate` / `Regression` / `Gap` / `Gap_FirstReceive` | 중복·역행 거부, 갭 카운트, 첫 수신 예외 |
| `TimestampCheck_Normal` / `FutureTooFar` / `FutureBoundary` / `GPS_Rejected` / `EKF_Rejected` / `BeforeSeqCheck` | 미래 timestamp(>5s) 거부, seq 검사보다 선평가 |

config 명령 (`CONFIG_CMD_MID`):
| 테스트 | 검증 |
|---|---|
| `ProcessConfig_AttitudeTimeout` / `PublishPeriod` | 정상 파라미터 적용 (ActiveConfig 갱신) |
| `ProcessConfig_BadLength` / `BadScope` / `BadVersion` / `BadChecksum` / `BadValue` / `BadParam` | 단계별 검증 실패 거부 |
| (dispatch) `TaskPipe_ConfigCmd` / `TaskPipe_ViewpointCmd` | CONFIG/VIEWPOINT MID 처리 경로 |
| `ProcessViewpointCommand` | viewpoint 캐시 저장 |

상태 영속화 / bridge 재시작:
| 테스트 | 검증 |
|---|---|
| `LoadState_NoFile` / `SaveState_NoDir` / `SaveState_OnTransition` | 상태 파일 로드/저장, 전이 시 저장 |
| `BridgeRestart_FirstAttempt` / `SecondAttempt` / `MaxReached` / `AllAttemptsExhausted` / `GetAppIdFail` / `ResetOnRecovery` / `RecoverAfterAttempt` | bridge 재시작 시도·최대 3회 한도·복구 시 리셋 |

Init/dispatch 추가:
| 테스트 | 검증 |
|---|---|
| `Init_DefaultTimeouts` | ActiveConfig 기본 타임아웃 로드 |
| `Init_EVSRegisterError` / `MsgInitError` / `CreatePipeError` / `Subscribe2~9Error` | 각 init 단계 실패 주입 |
| `TaskPipe_GetMsgIdError` / `GetFcnCodeError` / `AttitudeState` / `Noop_LengthFail` | dispatch 오류/분기 경로 |
| (eds) `TaskPipe` | EDS dispatch 경로 |
| `ServicePrototype` | 주기 서비스 경로 |

---

### `uplink_app`

테스트 위치:
- `uplink_app/unit-test/coveragetest/coveragetest_uplink_app.c`
- `uplink_app/unit-test/coveragetest/coveragetest_uplink_app_cmds.c`
- `uplink_app/unit-test/coveragetest/coveragetest_uplink_app_dispatch.c`
- `uplink_app/unit-test/coveragetest/coveragetest_uplink_app_utils.c`

#### `coveragetest_uplink_app.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `UPLINK_APP_Init` | 앱 초기화 성공 및 `RunStatus == APP_RUN` |
| `UPLINK_APP_Init_SubscribeError` | subscribe 실패 시 오류 반환 |

#### `coveragetest_uplink_app_cmds.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `UPLINK_APP_Noop` | `NOOP` 처리 시 command counter 증가 |
| `UPLINK_APP_ResetCounters` | command/error/accept/reject/routing failure counter 초기화 |
| `UPLINK_APP_ProcessUplink_Accept` | 정상 config class 명령 수락 → `LastCommandResult == ROUTED` |
| `UPLINK_APP_ProcessUplink_RejectSequence` | sequence 중복/역행 → `REJECT_SEQUENCE`, `DEGRADED` 전이 |
| `UPLINK_APP_ProcessUplink_Reject` | validate 실패 → reject/error counter 증가 |
| `UPLINK_APP_ProcessUplink_RouteMiss` | route target 미확인 → routing failure 증가, `ROUTE_MISS` |
| `UPLINK_APP_ProcessUplink_RouteUpdate` | 정상 route update → core target으로 라우팅 |
| `UPLINK_APP_ProcessUplink_RouteReject` | route payload 검증 실패 → `REJECT_ROUTE` |
| `UPLINK_APP_ProcessUplink_RoutePublishFail` | route publish 실패 → routing failure, `FAILED` |
| `UPLINK_APP_ProcessUplink_RecoveryAccept` | recovery class 명령 수락 → core target 라우팅 |
| `UPLINK_APP_ProcessUplink_RecoveryForwardFail` | recovery forward 실패 → routing failure |
| `UPLINK_APP_ProcessUplink_ViewpointAccept` | viewpoint class 명령 수락 |
| `UPLINK_APP_ProcessUplink_ViewpointForwardFail` | viewpoint forward 실패 → routing failure |
| `UPLINK_APP_ProcessUplink_ConfigAccept` | config class 명령 수락 → sequence 반영 |
| `UPLINK_APP_ProcessUplink_ConfigForwardFail` | config forward 실패 → routing failure |
| `UPLINK_APP_ProcessUplink_ModeAccept` | mode class 명령 수락 |
| `UPLINK_APP_ProcessUplink_ModeForwardFail` | mode forward 실패 → routing failure |
| `UPLINK_APP_ProcessUplink_DiagnosticAccept` | diagnostic class → downlink target 라우팅 |
| `UPLINK_APP_ProcessUplink_DiagnosticForwardFail` | diagnostic forward 실패 → routing failure |
| `UPLINK_APP_ProcessUplink_BlockedDegraded` | DEGRADED 상태에서 ROUTE_UPDATE/VIEWPOINT → `REJECT_STATE` |
| `UPLINK_APP_ProcessUplink_BlockedRecovery` | RECOVERY 상태에서 ROUTE_UPDATE → `REJECT_STATE` |
| `UPLINK_APP_ProcessUplink_AllowedRecoveryDiagnostic` | RECOVERY 상태에서 DIAGNOSTIC class → 허용 (RECOVERY 상태는 DIAGNOSTIC 외 전 클래스 차단 — RECOVERY class 포함) |
| `UPLINK_APP_ProcessUplink_BlockedFailed` | FAILED 상태에서 일반 명령 → `REJECT_STATE` |
| `UPLINK_APP_ProcessUplink_FailOpenBefore` | FAILED 상태 진입 전(fail-open 구간) 명령 → 허용 |

#### `coveragetest_uplink_app_dispatch.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `UPLINK_APP_VerifyCmdLength` | dispatch 경로 길이 검증 |
| `UPLINK_APP_TaskPipe_SendHk` | `SEND_HK` MID → HK 보고 경로 진입 |
| `UPLINK_APP_TaskPipe_SystemHealth` | `SYSTEM_HEALTH_MID` 수신 → `CfsHealthReceived`, `CfsHealthState` 갱신 |
| `UPLINK_APP_TaskPipe_UnknownMid` | 알 수 없는 MID → error counter 증가 |

#### `coveragetest_uplink_app_utils.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `UPLINK_APP_ValidateProxyCommand` | version 오류/잘못된 class/zero payload/과도한 payload 길이 거부 |
| `UPLINK_APP_VerifyCmdLength_Impl` | 정상/비정상 길이 판단 |
| `UPLINK_APP_ParseRouteUpdatePayload` | 정상 파싱, route type/waypoint 수/무한대 좌표/고도/거리 위반 거부 |
| `UPLINK_APP_ResolveRouteTarget` | class별 route target(core/downlink/none) 반환 |
| `UPLINK_APP_UpdateStatusTelemetry` | 각 카운터, 마지막 명령/결과/route target이 상태 TLM에 반영 |
| `UPLINK_APP_LoadState_NoFile` | 상태 파일 없을 때 초기값(0) 유지 |
| `UPLINK_APP_SaveState_NoDir` | 디렉터리 없어도 저장 시도하고 counter는 유지 |
| `UPLINK_APP_ForwardModeCommand` | mode 명령 SB publish 성공/실패 경로 |
| `UPLINK_APP_ForwardDiagnosticCommand` | diagnostic 명령 SB publish 성공/실패 경로 |
| `UPLINK_APP_ForwardViewpointCommand` | viewpoint 명령 SB publish 성공/실패/zero-payload 경로 |

---

### ~~`lora_fc_downlink_app`~~ (baseline 제거됨, 2026-06-16 — `lora_tdm_app`으로 대체)

> startup script에서 제거됨(`lora_tdm_app`이 prio 58 슬롯 대체). **앱 코드·테스트 파일은 저장소에서 삭제됨** (commit `7c080f1`, 2026-06-30) — 아래 목록·TC는 이력 기록일 뿐, 파일은 더 이상 존재하지 않는다.

테스트 위치:
- `lora_fc_downlink_app/unit-test/coveragetest/coveragetest_lora_fc_downlink_app.c`
- `lora_fc_downlink_app/unit-test/coveragetest/coveragetest_lora_fc_downlink_app_cmds.c`
- `lora_fc_downlink_app/unit-test/coveragetest/coveragetest_lora_fc_downlink_app_dispatch.c`
- `lora_fc_downlink_app/unit-test/coveragetest/coveragetest_lora_fc_downlink_app_utils.c`

#### `coveragetest_lora_fc_downlink_app.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `LORA_FC_DOWNLINK_APP_Init` | 앱 초기화 성공 및 `RunStatus == APP_RUN` |
| `LORA_FC_DOWNLINK_APP_Init_SubscribeError` | subscribe 실패 시 오류 반환 |

#### `coveragetest_lora_fc_downlink_app_cmds.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `LORA_FC_DOWNLINK_APP_NoopCmd` | `NOOP` 처리 시 command counter 증가 |
| `LORA_FC_DOWNLINK_APP_ResetCountersCmd` | command/error/downlink counter 초기화 |

#### `coveragetest_lora_fc_downlink_app_dispatch.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `LORA_FC_DOWNLINK_APP_VerifyCmdLength` | CMD_MID + 각 function code 기반 길이 검증 |
| `LORA_FC_DOWNLINK_APP_TaskPipe` | SEND_HK/CMD/FC attitude 입력이 올바른 처리 경로로 분기 |
| `LORA_FC_DOWNLINK_APP_TaskPipe_UnknownMid` | 알 수 없는 MID → error counter 증가 |
| `LORA_FC_DOWNLINK_APP_TaskPipe_InvalidCC` | 알 수 없는 function code → error counter 증가 |
| `LORA_FC_DOWNLINK_APP_TaskPipe_ResetCC` | `RESET_COUNTERS_CC` dispatch 경로 진입 확인 |

#### `coveragetest_lora_fc_downlink_app_utils.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `LORA_FC_DOWNLINK_APP_ReportHousekeeping` | HK payload에 downlink count, valid flag, health state 반영 |
| `LORA_FC_DOWNLINK_APP_ProcessInputMessage` | attitude(roll/pitch/yaw float 캐시), local(x/y/z/vx/vy/vz float 캐시), gps(LatE7/LonE7/AltMm/FixType), ekf, system health(HealthState/FaultCode) 입력별 캐시 갱신 확인 |
| `LORA_FC_DOWNLINK_APP_ProcessInputMessage_InvalidInputs` | attitude/local Valid=0 → AttitudeValid/LocalValid=0 반영 (LORA-FC-005/006) |
| `LORA_FC_DOWNLINK_APP_ProcessInputMessage_GpsEdgeCases` | GPS Valid=0 → GpsValid=0; Valid=1+FixType=0 → GpsFixType=0 캐시 (LORA-FC-007) |
| `LORA_FC_DOWNLINK_APP_ForwardUplinkFrame` ⏳ | "UP,..." → `UPLINK_RAW_MID` publish (Task B). static + `read()` fd 의존이라 단위테스트는 리팩터 후 가능 — 현재 e2e/통합으로 검증 |

---

### `lora_tdm_app` (baseline 등록됨, 2026-06-16)

> **2026-06-16: startup script에 등록됨** (`lora_fc_dl_app` 대체, prio 58). 구버전은 pipe depth 200(>cFS 최대 50)으로 즉시 종료됐으나 **현재 코드 깊이는 50**(`lora_tdm_app.c:268`)으로 수정 완료.
> `lora_fc_downlink_app`(downlink TX) + `bridge/lora_uplink_bridge.py`(uplink RX, Pi 별도 프로세스)가 하던 역할을 이 앱 하나가 흡수. **Pi에서 python 프로세스 종료 및 cFS 프레임워크(이 저장소 밖) 앱 목록 갱신은 별도 운영 작업으로 필요**.
>
> **2026-06-16 재검증**: 이 앱은 `cFS_clean` 빌드 환경(targets.cmake)에 등록된 적이 없어 native unit-test가 한 번도 실제로 빌드·실행되지 않은 상태였다(아래 "✓" 표시는 코드 정독 기준, 실행 검증 아니었음). `tdm_refactor` 브랜치에서 임시로 등록해 처음으로 빌드/실행한 결과 버그 3건 발견 및 수정:
> 1. `coveragetest_lora_tdm_app_utils.c`: `LORA_TDM_APP_ProcessRxLine` 호출에 문자열 리터럴(`const char*`) 전달 시 `char*` 시그니처와 불일치 → 컴파일 에러(`-Werror=discarded-qualifiers`). 함수가 실제로는 읽기 전용이므로 시그니처를 `const char *Line`으로 수정.
> 2. `coveragetest_lora_tdm_app_dispatch.c`: `CmdNoop`/`CmdReset` 테스트가 `LORA_TDM_APP_Data.CmdCounter`를 직접 검증했으나, 이 dispatch 단독 테스트 바이너리에서는 `LORA_TDM_APP_Noop`/`ResetCounters`가 스텁(실제 카운터 미반영)이라 항상 실패. `UtAssert_STUB_COUNT`로 정정(다른 앱 dispatch 테스트와 동일 패턴).
> 3. **실제 fsw 버그**: `lora_tdm_app_utils.c`의 `ProcessUpFrame`이 `sscanf("%[^,]", ...)`로 payload_hex를 파싱하는데, 이 지정자는 1자 이상 매칭을 요구해 **payload 없는(빈 문자열) UP 프레임을 전부 CRC_FAIL로 오판**하던 버그. `,,` 리터럴 포함 대체 포맷으로 재시도하도록 수정 (`Test_ProcessRxLine_ValidUp`로 검증). §9.1 참고.
>
> 수정 후 native 테스트 4개 바이너리 전부 재실행: **75/75 PASS, 0 FAIL**.

테스트 위치:
- `lora_tdm_app/unit-test/coveragetest/coveragetest_lora_tdm_app.c`
- `lora_tdm_app/unit-test/coveragetest/coveragetest_lora_tdm_app_cmds.c`
- `lora_tdm_app/unit-test/coveragetest/coveragetest_lora_tdm_app_dispatch.c`
- `lora_tdm_app/unit-test/coveragetest/coveragetest_lora_tdm_app_utils.c`

#### `coveragetest_lora_tdm_app.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `Init` | 앱 초기화 성공 및 `RunStatus == APP_RUN` |
| `Init_SubscribeError` | `CFE_SB_Subscribe` 실패 주입 → `CFE_SB_BAD_ARGUMENT` 반환 |

#### `coveragetest_lora_tdm_app_cmds.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `Noop` | `NOOP` 처리 시 CmdCounter 증가 |
| `ResetCounters` | CmdCounter/ErrCounter/TxCount/RxAckCount/RxCmdCount/RxErrorCount/NoAckCount 전체 0 초기화 |

#### `coveragetest_lora_tdm_app_dispatch.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `VerifyCmdLength` | 정상 길이 → dispatch 통과 |
| `ProcessCommandPacket_SendHk` | `SEND_HK_MID` → HK 보고 경로 진입 |
| `ProcessCommandPacket_CmdNoop` | `CMD_MID` + CC=0 → `LORA_TDM_APP_Noop` 호출 1회 (`UtAssert_STUB_COUNT`) |
| `ProcessCommandPacket_CmdReset` | `CMD_MID` + CC=1 → `LORA_TDM_APP_ResetCounters` 호출 1회 (`UtAssert_STUB_COUNT`) |
| `ProcessCommandPacket_UnknownMid` | 알 수 없는 MID(0x9999) → ErrCounter=1 |
| `ProcessCommandPacket_InvalidCC` | CC=99 → ErrCounter=1 |

#### `coveragetest_lora_tdm_app_utils.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `Crc16_KnownVector` | `"123456789"` → `0x29B1` (CRC-16/CCITT-FALSE 표준 벡터) |
| `ParseAckFrame_Valid` | `"ACK,42\n"` → SeqEcho=42, OK |
| `ParseAckFrame_ZeroSeq` | `"ACK,0\n"` → SeqEcho=0, OK |
| `ParseAckFrame_WrongPrefix` | `"NAK,42\n"` → INVALID |
| `ParseAckFrame_MalformedNoSeq` | `"ACK,\n"` → INVALID |
| `BuildFcDownlinkLine_Basic` | `"FC,"` 시작, `"\n"` 종료, 반환값 > 0 |
| `BuildFcDownlinkLine_UplinkFeedbackField` | `PendingUplinkFeedback=CRC_FAIL(1)` → 라인에 `,1\n` 포함 |
| `BuildFcDownlinkLine_BufferTooSmall` | 4바이트 버퍼 → 반환값 < 0 |
| `BuildShDownlinkLine_Basic` | `"SH,"` 시작, `"\n"` 종료, 반환값 > 0 |
| `UpdateLinkState_Connected` | NoAckCount=0, elapsed < timeout → CONNECTED |
| `UpdateLinkState_Degraded` | NoAckCount=THRESHOLD, elapsed < timeout → DEGRADED |
| `UpdateLinkState_Disconnected` | elapsed > LINK_TIMEOUT_MS → DISCONNECTED |
| `ProcessRxLine_Ack` | `"ACK,7\n"` → RxAckCount=1, NoAckCount=0 |
| `ProcessRxLine_CrcFail` | UP 프레임 CRC 오류 → `PendingUplinkFeedback=CRC_FAIL`, RxErrorCount=1 |
| `ProcessRxLine_ValidUp` | 올바른 CRC UP 프레임 → RxCmdCount=1, `PendingUplinkFeedback=OK` |
| `UpdateCacheFromMsg_Attitude` | ATTITUDE MID → `FcState.RollRad/PitchRad/YawRad` 갱신 |
| `UpdateCacheFromMsg_EkfLocal` | EKF_LOCAL MID → `PosX/VelX` 등 갱신 |
| `UpdateCacheFromMsg_Gps` | GPS MID → `LatE7/LonE7/AltMm/GpsFix` 갱신 |
| `UpdateCacheFromMsg_SystemHealth` | SYSTEM_HEALTH MID → `SystemHealthState/FaultCode` 갱신 |
| `UpdateCacheFromMsg_EkfStatus` | EKF_STATUS MID → `EkfValid=1`, `PacketType=FC_STATE` |

---

### `mavlink_bridge_app`

테스트 위치:
- `mavlink_bridge_app/unit-test/coveragetest/coveragetest_mavlink_bridge_app.c`
- `mavlink_bridge_app/unit-test/coveragetest/coveragetest_mavlink_bridge_app_cmds.c`
- `mavlink_bridge_app/unit-test/coveragetest/coveragetest_mavlink_bridge_app_dispatch.c`
- `mavlink_bridge_app/unit-test/coveragetest/coveragetest_mavlink_bridge_app_utils.c`

#### `coveragetest_mavlink_bridge_app.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `MAVLINK_BRIDGE_APP_Init` | 앱 초기화 성공, `RunStatus == APP_RUN`, `SerialFd == -1`, `LinkState == DISCONNECTED` |
| `MAVLINK_BRIDGE_APP_Init_SubscribeError` | `CFE_SB_Subscribe` 실패 시 오류 반환 |

#### `coveragetest_mavlink_bridge_app_cmds.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `MAVLINK_BRIDGE_APP_Noop` | `NOOP` 처리 시 `CmdCounter` 증가 |
| `MAVLINK_BRIDGE_APP_ResetCounters` | `CmdCounter`, `ErrCounter`, `ParseErrorCount` 초기화 |

#### `coveragetest_mavlink_bridge_app_dispatch.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `MAVLINK_BRIDGE_APP_VerifyCmdLength` | dispatch 경로 길이 검증 helper |
| `MAVLINK_BRIDGE_APP_TaskPipe_SendHk` | `SEND_HK_MID` → `ReportHousekeeping` 호출 |
| `MAVLINK_BRIDGE_APP_TaskPipe_RouteUpdate` | `ROUTE_UPDATE_MID` → `StartMissionUpload` 호출 |
| `MAVLINK_BRIDGE_APP_TaskPipe_Noop` | CMD_MID + NOOP_CC → `Noop` 호출, ErrCounter 불변 |
| `MAVLINK_BRIDGE_APP_TaskPipe_ResetCounters` | CMD_MID + RESET_COUNTERS_CC → `ResetCountersCmd` 호출 |
| `MAVLINK_BRIDGE_APP_TaskPipe_MissionQuery` | CMD_MID + MISSION_QUERY_CC → `MissionQuery` 호출 |
| `MAVLINK_BRIDGE_APP_TaskPipe_UnknownMid` | 알 수 없는 MID → `ErrCounter` 증가 |
| `MAVLINK_BRIDGE_APP_TaskPipe_UnknownCC` | 알 수 없는 CC → `ErrCounter` 증가 |

#### `coveragetest_mavlink_bridge_app_utils.c`

| 테스트 이름 | 검증 내용 |
|---|---|
| `UpdateFromHeartbeat_Armed` | bit7 설정 → `IsArmed == 1` |
| `UpdateFromHeartbeat_Disarmed` | bit7 미설정 → `IsArmed == 0` |
| `UpdateFromHeartbeat_OtherBitsIgnored` | bit7 외 다른 비트 무시 |
| `UpdateFromHeartbeat_StateTransition` | ARMED → DISARMED 전이 |
| `UpdateFromHeartbeat_SystemStatus` | `FcSystemStatus` 저장 확인 |
| `StartMissionUpload_BlockedWhenArmed` | ARMED 상태에서 업로드 차단 + `ARMED_WARN_EID` 발생 |
| `StartMissionUpload_AllowedWhenDisarmed` | DISARMED 상태에서 업로드 → `CLEARING` 전이 |
| `StartMissionUpload_LinkNotConnectedBeforeArmedCheck` | 링크 미연결 시 ARMED 여부 무관하게 차단 |
| `MissionQuery_LinkNotConnected` | 링크 미연결 → `MISSION_DOWNLOAD_ERR_EID`, ErrCounter 증가 |
| `MissionQuery_Connected` | 링크 연결 → download `WAIT_COUNT` 상태 전이, CmdCounter 증가 |
| `MissionQuery_LengthCheckFail` | 길이 불일치 → 즉시 반환 (download 미시작) |

---

## Python 단위 테스트

테스트 위치: `tests/`

실행: `.venv/bin/python -m pytest tests/ -v`

### `test_lora_uplink_bridge.py`

| 테스트 이름 | 검증 내용 |
|---|---|
| `test_parse_frame_line_accepts_valid_frame` | 정상 `UP,...` 프레임 파싱 — version/class/sequence/payload length 추출 |
| `test_parse_frame_line_rejects_crc_mismatch` | CRC 오류 프레임 거부 |
| `test_parse_frame_line_rejects_invalid_format` | 프레임 아닌 일반 문자열 거부 |
| `test_parse_frame_line_rejects_oversize_payload` | 192 byte 초과 payload 거부 |
| `test_build_process_uplink_payload_rejects_wrong_version` | 미지원 protocol version 거부 |
| `test_process_line_forwards_valid_frame_once` | 정상 프레임 → LoRa serial 전송 1회, accept count 증가 |
| `test_process_line_rejects_sequence_regression` | 동일 sequence 재입력 → replay 거부 |
| `test_process_line_allows_sequence_regression_when_disabled` | strict sequence 비활성화 시 동일 sequence 허용 |
| `test_process_line_rejects_non_frame_text` | 비프레임 텍스트 입력 → 전송 없음 |

### `test_uplink_config_sender.py`

| 테스트 이름 | 검증 내용 |
|---|---|
| `ConfigChecksumTest.test_matches_c_algorithm` | config checksum이 C 구현과 동일한 값 산출 |
| `ConfigChecksumTest.test_param_id_split_into_two_bytes` | param_id lo/hi 바이트 분리 확인 |
| `ConfigChecksumTest.test_scope2` | SCOPE_MAVLINK_BRIDGE 범위 checksum 확인 |
| `BuildConfigPayloadTest.test_cfs_core_publish_period` | config payload 8바이트 헤더 + 4바이트 값, checksum 검증 |
| `BuildConfigPayloadTest.test_mavlink_bridge_attitude_interval` | SCOPE_MAVLINK_BRIDGE param 패킷 구조 |
| `BuildProcessUplinkPayloadTest.test_class_is_config` | ProcessUplink payload의 CommandClass == CONFIG |
| `BuildProcessUplinkPayloadTest.test_sequence_stored` | sequence 필드 정확히 저장 |
| `BuildProcessUplinkPayloadTest.test_checksum_is_correct_crc16` | proxy CRC-16/CCITT-FALSE 검증 |
| `BuildProcessUplinkPayloadTest.test_payload_embedded_in_fixed_area` | config payload가 고정 위치에 삽입 |
| `BuildLoraFrameTest.test_frame_format` | LoRa UP 프레임 7개 필드, 값 확인 |
| `BuildLoraFrameTest.test_crc_is_valid` | 프레임 CRC 유효성 확인 |
| `BuildLoraFrameTest.test_payload_hex_matches` | hex payload가 원본 bytes와 일치 |

### `test_lora_downlink_decoder.py` (2026-07-20 표 전체 갱신 — 기존 1건만 기재됐던 것을 실제 23건으로 정정)

| 테스트 이름 | 검증 내용 |
|---|---|
| `Crc16Test.test_known_vector` | CRC-16 알려진 벡터 검증 |
| `RoundtripTest.test_basic_roundtrip` | DL2 인코딩→디코딩 기본 라운드트립 |
| `RoundtripTest.test_systime_block` | SysTime 블록 포함 라운드트립 |
| `RoundtripTest.test_base_frame_is_47_bytes` | 기본 프레임 47B 크기 확인 |
| `RoundtripTest.test_saturation_flag` | 위치 saturation flag 확인 |
| `RoundtripTest.test_angle_range_pi` | 각도 범위(±π) 인코딩 확인 |
| `StreamingTest.test_byte_by_byte_feed` | 1바이트씩 피드해도 프레임 완성 |
| `StreamingTest.test_two_frames_one_chunk` | 한 청크에 프레임 2개 → 둘 다 파싱 |
| `StreamingTest.test_crc_corruption_resync` | CRC 오류 프레임 후 재동기화 |
| `StreamingTest.test_garbage_prefix_resync` | 쓰레기 바이트 프리픽스 후 재동기화 |
| `StreamingTest.test_v1_v2_mixed_stream` | v1(텍스트)/v2(바이너리) 혼합 스트림 파싱 |
| `StreamingTest.test_bad_len_field` | 잘못된 길이 필드 처리 |
| `StreamingTest.test_systime_flag_set_but_block_missing_returns_none_not_crash` | DL2 프레임의 `flags` SYSTIME 비트는 켜져 있는데 `body_len`이 SysTime 블록을 포함하지 않는 (짧은) 길이인 손상/불일치 프레임 — `decode_dl2()`가 `struct.error`로 크래시하지 않고 `sys_time_unix_usec=None`으로 안전 처리하며 나머지 필드(자세/위치 등)는 정상 디코드됨을 확인 (2026-07-14 수정한 크래시 버그의 회귀 방지) |
| `Ack2Test.test_format` | ACK2 프레임 포맷 확인 |
| `Ack2Test.test_seq_wrap` | ACK2 seq wrap-around 처리 |
| `Up2Test.test_roundtrip_with_payload` | UP2 payload 포함 라운드트립 |
| `Up2Test.test_roundtrip_zero_payload` | UP2 zero-payload 라운드트립 |
| `Up2Test.test_crc_present_and_valid` | UP2 CRC 존재·유효성 확인 |
| `Up2Test.test_magic_byte` | UP2 매직바이트 확인 |
| `Up2Test.test_seq_wrap` | UP2 seq wrap-around 처리 |
| `Up2Test.test_cross_language_vector_matches_c_ut` | Python 디코더 결과가 C 단위테스트 벡터와 일치 (cross-language 검증) |
| `CsvRowTest.test_row_fields` | CSV row 필드 구성 확인 |
| `CsvRowTest.test_row_without_systime` | SysTime 없는 경우 CSV row 처리 |

### `test_uplink_route_update_sender.py`

| 테스트 이름 | 검증 내용 |
|---|---|
| `test_opens_serial_with_correct_path_and_baudrate` | 올바른 시리얼 경로/baud rate로 open |
| `test_writes_frame_as_ascii_with_newline` | 프레임이 ASCII+개행으로 기록 |
| `test_calls_flush_after_write` | write 후 flush 호출 확인 |
| `test_raises_runtime_error_when_pyserial_unavailable` | pyserial 없을 때 RuntimeError 발생 |

---

## 런타임 시험

### PC 수신 텔레메트리 시험

시험 목적: FC → mavlink_bridge_app → cFS SB → lora_fc_downlink_app → LoRa → PC 전 구간 검증

| 시험 항목 | 판정 기준 |
|---|---|
| 30초 이상 연속 수신 | 공백 없이 프레임 도착 |
| 자세 변화 반영 | FC 자세 변경 시 PC 수신 roll/pitch/yaw 값 변화 |
| GPS 좌표 수신 | FC GPS 패킷 수신 시 `FC,...,lat_e7,lon_e7,alt_mm,fix_type` 포함 확인 |
| SH 패킷 수신 | `SYSTEM_HEALTH_MID` 수신 시 `SH,count,ts,health,fault` 형식 확인 |
| HB 링크 상태 갱신 | 지상국에서 `HB,1,1,1000,1,<crc>` 전송 시 `HbLinkValid=1` HK 확인 |
| FAIL-PI-IN | Pi에서 bridge 입력 없음 → FC 또는 serial 입력 단계 문제 |
| FAIL-DOWNLINK | Pi 수신 정상, PC 수신 없음 → publish~LoRa 구간 문제 |

### LoRa uplink 직접 수신 시험 (uplink_app ServiceLoRa)

시험 목적: LoRa serial → uplink_app → cFS SB 경로 검증 (Python bridge 없이)

| 시험 항목 | 검증 내용 | 기대 결과 |
|---|---|---|
| 정상 UP 프레임 수신 | `lora_uplink_bridge.py --transport lora-serial` 전송 | `UPLINK_APP: routed uplink class=2 seq=N` EVS 확인 |
| CRC 오류 프레임 | 임의 변조 후 전송 | `LoRa frame parse failed` EVS, route update 없음 |
| sequence 역행 | 동일 seq 재전송 | `LoRa seq regression` EVS, route update 없음 |

---

### lora_tdm_app TDM 연동 시험

시험 목적: `lora_tdm_app` 단독으로 serial 포트를 점유하여 TX→RX 사이클이 정상 동작하는지 검증.  
**전제 조건:** Pi에 `lora_tdm_app` 빌드 배포, LoRa serial 연결, 지상국(GS) PC에서 serial 터미널 또는 스크립트 실행.

#### TDM-RT-001 — 다운링크 주기 수신

| 항목 | 내용 |
|---|---|
| 시험 방법 | GS PC에서 LoRa serial 포트를 열고 수신 라인 모니터링 |
| 기대 결과 | 약 200ms 간격(Stage 3 `CYCLE_PERIOD_MS`, 구 1초)으로 `FC,<seq>,...` 또는 `SH,<seq>,...` 라인 수신 (기본 v1 텍스트 — `UseV2Downlink` 활성화 시 DL2 바이너리) |
| 판정 기준 | 30초 이상 공백 없이 수신, seq 단조 증가 |

#### TDM-RT-002 — ACK 응답 시 링크 CONNECTED 확인

| 항목 | 내용 |
|---|---|
| 시험 방법 | GS에서 각 다운링크 수신 후 `ACK,<seq>\n` 즉시 전송 |
| 기대 결과 | HK TLM `LinkState==CONNECTED(1)`, `NoAckCount==0` |
| 판정 기준 | `RxAckCount` 증가, `NoAckCount` 0 유지 |

#### TDM-RT-003 — ACK 무응답 시 DEGRADED 전이

| 항목 | 내용 |
|---|---|
| 시험 방법 | GS에서 ACK 전송 중단 (3사이클 이상) |
| 기대 결과 | HK TLM `LinkState==DEGRADED(2)`, `NoAckCount>=3` |
| 판정 기준 | EVS `LORA_TDM_APP: Link degraded` 또는 HK 확인 |

#### TDM-RT-004 — ACK 무응답 5초 후 DISCONNECTED 전이

| 항목 | 내용 |
|---|---|
| 시험 방법 | GS ACK 완전 중단 후 5초 대기 |
| 기대 결과 | HK TLM `LinkState==DISCONNECTED(0)` |
| 판정 기준 | `LastAckTimestampMs` 갱신 멈춤 확인 |

#### TDM-RT-005 — ACK 재개 시 CONNECTED 복구

| 항목 | 내용 |
|---|---|
| 시험 방법 | DISCONNECTED 상태에서 GS ACK 재전송 시작 |
| 기대 결과 | `LinkState==CONNECTED`, `NoAckCount==0` 복구 |
| 판정 기준 | `RxAckCount` 다시 증가 |

#### TDM-RT-006 — 정상 UP 프레임 수신 → uplink_app 라우팅

| 항목 | 내용 |
|---|---|
| 시험 방법 | GS에서 RX 윈도우 내(다운링크 직후 `RX_WINDOW_MS(100)`, Stage 3 — 구 300ms) 유효한 UP 프레임 전송 |
| 기대 결과 | `uplink_app` EVS: `routed uplink class=N seq=N` |
| 판정 기준 | HK `RxCmdCount` 증가, `PendingUplinkFeedback==0` |

#### TDM-RT-007 — CRC 오류 UP 프레임 → UFB=1 다음 다운링크 반영

| 항목 | 내용 |
|---|---|
| 시험 방법 | GS에서 CRC를 임의 변조한 UP 프레임 전송 |
| 기대 결과 | 다음 `FC,...` 라인 마지막 필드가 `1` (CRC_FAIL) |
| 판정 기준 | GS에서 동일 seq UP 프레임 재전송 후 정상 처리 확인 |

#### TDM-RT-008 — RX 윈도우 외 수신 무시

| 항목 | 내용 |
|---|---|
| 시험 방법 | 다운링크 전송 직전(TX 중) UP 프레임 전송 |
| 기대 결과 | `RxCmdCount` 증가 없음, 프레임 무시 |
| 판정 기준 | HK `RxCmdCount` 변화 없음 |

#### TDM-RT-009 — serial open 실패 후 재시도

| 항목 | 내용 |
|---|---|
| 시험 방법 | 시작 시 serial 장치 미연결 → 연결 후 대기 |
| 기대 결과 | EVS `LORA_TDM_APP: open serial failed`, 이후 재시도하여 정상 동작 |
| 판정 기준 | 연결 후 다운링크 재개 확인 |

---

---

## TC 분류 — 단위테스트 vs 통합테스트 vs 런타임

### 분류 기준

| 구분 | 조건 |
|---|---|
| **단위테스트** | 공개 C API로 직접 호출 가능, 하드웨어 불필요 |
| **통합테스트** | static 함수 또는 Python bridge 로직 검증, pytest + mock |
| **런타임 시험** | 하드웨어(Pi/FC/LoRa) 또는 실제 serial 필요 |

---

### LORA-HB (HB 파싱)

| TC ID | 항목 | 분류 | 파일 |
|---|---|---|---|
| LORA-HB-001 | `HB` 단순 수신 | 통합 | `test_hb_parse.py` |
| LORA-HB-002 | canonical HB 정상 수신 | 통합 | `test_hb_parse.py` |
| LORA-HB-003 | CRC 불일치 거부 | 통합 | `test_hb_parse.py` |
| LORA-HB-004 | sensor_ok=0 거부 | 통합 | `test_hb_parse.py` |
| LORA-HB-005 | seq 증가 허용 | 통합 | `test_hb_parse.py` |
| LORA-HB-006 | seq 역행 거부 | 통합 | `test_hb_parse.py` |
| LORA-HB-007 | 빈 줄 무시 | 통합 | `test_hb_parse.py` |
| LORA-HB-008 | 잘못된 prefix 거부 | 통합 | `test_hb_parse.py` |
| LORA-HB-009 | 필드 개수 부족 거부 | 통합 | `test_hb_parse.py` |
| LORA-HB-010 | 숫자 필드 오류 거부 | 통합 | `test_hb_parse.py` |

> `ParseHb()` static → Python `lora_telemetry_bridge.py`의 `parse_heartbeat_line()` 동등 로직으로 검증

---

### LORA-UP (UP 프레임 파싱)

| TC ID | 항목 | 분류 | 파일 |
|---|---|---|---|
| LORA-UP-001 | 정상 UP 프레임 파싱 | 통합 | `test_uplink_lora_frame.py` |
| LORA-UP-002 | CRC 불일치 거부 | 통합 | `test_uplink_lora_frame.py` |
| LORA-UP-003 | version 불일치 거부 | 통합 | `test_uplink_lora_frame.py` |
| LORA-UP-004 | command_class 범위 초과 거부 | 통합 | `test_uplink_lora_frame.py` |
| LORA-UP-005 | sequence 범위 초과 거부 | 통합 | `test_uplink_lora_frame.py` |
| LORA-UP-006 | flags 범위 초과 거부 | 통합 | `test_uplink_lora_frame.py` |
| LORA-UP-007 | payload hex 홀수 길이 거부 | 통합 | `test_uplink_lora_frame.py` |
| LORA-UP-008 | payload hex 비정상 문자 거부 | 통합 | `test_uplink_lora_frame.py` |
| LORA-UP-009 | payload 196 byte 최대 허용 | 통합 | `test_uplink_lora_frame.py` |
| LORA-UP-010 | payload 197 byte 초과 거부 | 통합 | `test_uplink_lora_frame.py` |
| LORA-UP-011 | seq 증가 허용 | 통합 | `test_uplink_lora_frame.py` |
| LORA-UP-012 | seq 동일 거부 | 통합 | `test_uplink_lora_frame.py` |
| LORA-UP-013 | seq 역행 거부 | 통합 | `test_uplink_lora_frame.py` |
| LORA-UP-014 | allow_seq_regression 옵션 | 통합 | `test_lora_uplink_bridge.py` (기존) |

> `ParseLoRaFrame()` / `CRC16()` static → Python `lora_uplink_bridge.py`의 `parse_frame_line()` 동등 로직으로 검증

---

### CFS-CMD (command packet 생성)

| TC ID | 항목 | 분류 | 파일 |
|---|---|---|---|
| CFS-CMD-001 | command MID 반영 | 통합 | `test_uplink_lora_frame.py` |
| CFS-CMD-002 | function code 반영 | 통합 | `test_uplink_lora_frame.py` |
| CFS-CMD-003 | checksum 계산 | 통합 | `test_uplink_lora_frame.py` |
| CFS-CMD-004 | PayloadLength 저장 | 통합 | `test_uplink_lora_frame.py` |
| CFS-CMD-005 | payload 0 padding | 통합 | `test_uplink_lora_frame.py` |
| CFS-CMD-006~008 | class/seq/flags 필드 전달 | 통합 | `test_uplink_lora_frame.py` |

---

### MAV-* (MAVLink 수신/캐시)

| TC ID | 항목 | 분류 | 비고 |
|---|---|---|---|
| MAV-001~009 | HEARTBEAT/ATTITUDE/GPS/EKF 캐시, serial 오류 | 런타임 | `mavlink_bridge_app` unit-test 미구성, 하드웨어 필요 |

---

### TDM-ACK-* (ACK 프레임 파싱)

| TC ID | 항목 | 분류 | 파일 |
|---|---|---|---|
| TDM-ACK-001 | 정상 ACK 파싱 → SeqEcho 추출 | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-ACK-002 | zero seq ACK 허용 | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-ACK-003 | 잘못된 prefix 거부 | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-ACK-004 | seq 필드 누락 거부 | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |

---

### TDM-DOWN-* (다운링크 라인 빌드)

| TC ID | 항목 | 분류 | 파일 |
|---|---|---|---|
| TDM-DOWN-001 | FC 라인 기본 포맷 (`FC,` 시작, `\n` 종료) | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-DOWN-002 | FC 라인에 UplinkFeedback 필드 포함 (CRC_FAIL=1) | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-DOWN-003 | 버퍼 부족 시 오류 반환 | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-DOWN-004 | SH 라인 기본 포맷 (`SH,` 시작, `\n` 종료) | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |

---

### TDM-LINK-* (링크 상태 전이)

| TC ID | 항목 | 분류 | 파일 |
|---|---|---|---|
| TDM-LINK-001 | NoAckCount=0 → CONNECTED | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-LINK-002 | NoAckCount≥THRESHOLD, elapsed<timeout → DEGRADED | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-LINK-003 | elapsed>LINK_TIMEOUT_MS → DISCONNECTED | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |

---

### TDM-RX-* (수신 라인 처리)

| TC ID | 항목 | 분류 | 파일 |
|---|---|---|---|
| TDM-RX-001 | ACK 수신 → RxAckCount++, NoAckCount=0 | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-RX-002 | UP 프레임 CRC 오류 → PendingUplinkFeedback=CRC_FAIL, RxErrorCount++ | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-RX-003 | UP 프레임 정상 → RxCmdCount++, PendingUplinkFeedback=OK, SB transmit | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-RX-004 | UP 프레임 SEQ_FAIL 경로 | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ (2026-07-14 구현 — `A3_unittest_cases.md` C.1/C.2, UFB=SEQ_FAIL은 `UPLINK_STATUS_MID` 경유) |

---

### TDM-CACHE-* (SB 메시지 캐시)

| TC ID | 항목 | 분류 | 파일 |
|---|---|---|---|
| TDM-CACHE-001 | ATTITUDE MID → roll/pitch/yaw 캐시 (2026-07-15: `shared_msgs/fc_state_msg.h`(`FC_ATTITUDE_TLM_t`) 공유 정의로 로컬 fake struct 제거 — TC-MRG-FCSTATE-1) | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-CACHE-002 | EKF_LOCAL MID → pos/vel 캐시 (2026-07-15: `FC_EKF_LOCAL_TLM_t` 공유 정의 전환) | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-CACHE-003 | GPS MID → lat/lon/alt/fix 캐시 (2026-07-15: `FC_GPS_RAW_TLM_t` 공유 정의 전환) | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-CACHE-004 | SYSTEM_HEALTH MID → HealthState/FaultCode 캐시 (2026-07-15: `shared_msgs/system_health_msg.h`(`SYSTEM_HEALTH_TLM_t`) 공유 정의로 로컬 fake struct 제거 — `notes/temp/mirror_struct_layout_audit.md` TC-MRG-SYSHEALTH-1) | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-CACHE-005 | EKF_STATUS MID → EkfValid=1, PacketType=FC_STATE (2026-07-15: `FC_STATE_PREFIX_t` 공유 정의 전환) | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ |
| TDM-CACHE-006 | FC_SYS_TIME MID → TimeUnixUsec/TimeValid 캐시 (2026-07-14, §16.4) | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ (`UpdateCacheFromMsg_SysTime`) |

---

### TDM-DL2-* (v2 바이너리 다운링크 인코딩, `LORA_TDM_APP_BuildDl2Frame`)

| TC ID | 항목 | 분류 | 파일 |
|---|---|---|---|
| TDM-DL2-001 | 기본 47B 인코딩 — 필드/CRC 정합 | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ (`BuildDl2Frame_Basic`) |
| TDM-DL2-002 | 위치 saturation(±327.67m 초과) → clamp + flags bit1 | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ (`BuildDl2Frame_PositionSaturation`) |
| TDM-DL2-003 | 버퍼 부족(47B 미만) → 음수 반환 | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ (`BuildDl2Frame_BufferTooSmall`) |
| TDM-DL2-004 | SysTime 유효 → flags bit0 + 55B, TimeUnixUsec u64 LE round-trip (2026-07-14) | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ (`BuildDl2Frame_SysTimeIncluded`) |
| TDM-DL2-005 | SysTime 무효(TimeValid=0) → 기존과 동일 47B, flags bit0 미설정 (2026-07-14) | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ (`BuildDl2Frame_SysTimeNotValid_Excluded`) |
| TDM-DL2-006 | SysTime 유효하나 버퍼가 47B뿐 → SysTime 생략 폴백(크래시 대신) (2026-07-14) | 단위 | `coveragetest_lora_tdm_app_utils.c` ✓ (`BuildDl2Frame_SysTimeValid_BufferOnlyBaseSize_Fallback`) |

---

### LORA-FC-* (lora_fc_downlink_app 캐시)

| TC ID | 항목 | 분류 | 파일/비고 |
|---|---|---|---|
| LORA-FC-001 | HEARTBEAT 캐시 | 단위 | 해당 없음 (별도 HEARTBEAT MID 없음) |
| LORA-FC-002 | ATTITUDE float 캐시 | 단위 | `coveragetest_lora_fc_downlink_app_utils.c` ✓ 구현 |
| LORA-FC-003 | GPS 좌표 캐시 | 단위 | `coveragetest_lora_fc_downlink_app_utils.c` ✓ 구현 |
| LORA-FC-004 | SYSTEM_HEALTH FaultCode 캐시 | 단위 | `coveragetest_lora_fc_downlink_app_utils.c` ✓ 구현 |
| LORA-FC-005 | stale flag 처리 | 단위 | 미구현 (cfs_core_app에서 처리, downlink_app은 Valid만 캐시) |
| LORA-FC-006 | 일부 메시지만 수신 시 partial valid | 단위 | `ProcessInputMessage` 기반 추가 가능 |
| LORA-FC-007 | invalid GPS (fix 없음) | 단위 | `ProcessInputMessage` 기반 추가 가능 |
| LORA-FC-008 | SB pipe timeout 시 앱 alive | 단위 | main loop 테스트 — 미구현 |

---

### LORA-FRAME-* (LoRa 패킷 포맷)

| TC ID | 항목 | 분류 | 파일 |
|---|---|---|---|
| LORA-FRAME-001 | FC 상태 패킷 포맷 | 통합 | `test_lora_fc_downlink_packet.py` |
| LORA-FRAME-002 | GPS 포함 여부 | 통합 | `test_lora_fc_downlink_packet.py` |
| LORA-FRAME-003 | GPS invalid 처리 | 통합 | `test_lora_fc_downlink_packet.py` |
| LORA-FRAME-004 | stale GPS 처리 | 통합 | `test_lora_fc_downlink_packet.py` |
| LORA-FRAME-005 | SH 패킷 포맷 | 통합 | `test_lora_fc_downlink_packet.py` |
| LORA-FRAME-006 | seq 단조 증가 | 통합 | `test_lora_fc_downlink_packet.py` |
| LORA-FRAME-007 | timestamp 필드 포함 | 통합 | `test_lora_fc_downlink_packet.py` |
| LORA-FRAME-008 | AttitudeValid=0 시 FC 패킷 미전송 | 통합 | `test_lora_fc_downlink_packet.py` |

> `ServiceLoRa()` static → Python에서 패킷 포맷 규칙 직접 검증

---

### REC-* (장애/복구)

| TC ID | 항목 | 분류 | 비고 |
|---|---|---|---|
| REC-001~002 | LoRa serial open 실패/disconnect | 런타임 | 하드웨어 또는 mock serial 필요 |
| REC-003~004 | FC serial open 실패/heartbeat 끊김 | 런타임 | 하드웨어 필요 |
| REC-005 | LoRa HB 끊김 | 통합 | `test_hb_parse.py` (timeout 시뮬레이션) |
| REC-006~007 | malformed/초과 frame 폭주 | 통합 | `test_uplink_lora_frame.py` / `test_hb_parse.py` |
| REC-008 | seq regression 반복 | 통합 | `test_uplink_lora_frame.py` |

---

## 통합테스트 계획 (pytest, `tests/`)

큰 기능 완성 시 추가한다. 하드웨어 없이 Python bridge 동등 로직으로 검증한다.

### 그룹 A — Python 동등 구현 (cFS 불필요)

| 파일 | 검증 TC | 상태 |
|---|---|---|
| `test_lora_uplink_bridge.py` | LORA-UP-014 | ✓ 구현 |
| `test_uplink_route_update_sender.py` | route update sender 전송 경로 | ✓ 구현 |
| `test_uplink_lora_frame.py` | LORA-UP-003~011, CFS-CMD-001~008, REC-006~008 | ✓ 구현 |
| `test_lora_fc_downlink_packet.py` | LORA-FRAME-001~008 | ✓ 구현 |
| `test_hb_parse.py` | LORA-HB-001~010, REC-005 | ✓ 구현 |
| `test_uplink_config_sender.py` | CONFIG payload/checksum/LoRa 프레임 빌드 (12개 테스트) | ✓ 구현 |
| `test_mission_upload_diag.py` | MAVLink X.25 CRC, V2 프레임 빌더, ITEM_INT z부호반전, Parser 라운드트립 (25개) | ✓ 구현 |
| `test_tools_packet_builders.py` | CFS 커맨드 XOR 체크섬, CCSDS 주헤더 구조 (9개) | ✓ 구현 |
| `test_mavlink_uart_bridge.py` | describe_message 포맷, parse_args 기본값/커스텀 (10개) | ✓ 구현 |

### 그룹 B — cFS 실행 + mock (cFS 필요, PTY mock serial)

cFS 프로세스가 실행 중인 상태에서 PTY mock serial로 입력을 넣고
EVS 로그/HK/serial 출력으로 결과를 검증한다.

| 파일 | 검증 TC | 방법 | 상태 |
|---|---|---|---|
| `test_uplink_e2e.py` | LORA-UP seq regression C 경로, REC-008 C 검증 | LoRa serial → cFS → EVS 로그 확인 | 구현됨 (테스트 4건, `--cfs` 게이트/skip — 아래 매트릭스와 동일) |
| `test_lora_fc_downlink_e2e.py` | LORA-FRAME C 실제 출력, LORA-FC-006~007 | SB mock → cFS → PTY serial 캡처 | 구현됨 (테스트 10건, `--cfs` 게이트/skip) |
| `test_rec_serial.py` | REC-001~004 장애/복구 | PTY close/reopen 시뮬레이션 | 구현됨 (테스트 5건, `--cfs` 게이트/skip) |

**B 그룹 실행 조건:**
- cFS 빌드 완료 (`build/` 또는 `cFS_clean/build/`)
- `pytest --cfs` 또는 별도 마커로 A와 분리 실행
- Pi 환경 또는 Linux native 실행 환경 필요

### 그룹 B 실구현 계획 (2026-07-09 결정)

현재 B그룹은 전부 `pytest.skip()` 스텁. 실제 C 앱의 예외처리(serial 장애/복구 등)를
자동 검증하도록 아래 순서로 구현한다.

**설계 결정 — serial 경로 env var override:**

serial 경로가 컴파일타임 `#define`이라 테스트가 PTY mock 경로를 주입할 수 없었다.
아래 env var를 C 코드에 추가한다. **미설정 시 기존 `#define` 경로를 그대로 사용하므로
Pi 실환경 동작은 불변** — env var는 테스트 전용이다.

| env var | 대상 앱 | 기본값 (미설정 시) |
|---|---|---|
| `LORA_TDM_SERIAL_PATH` | `lora_tdm_app` `OpenSerial()` | `LORA_TDM_APP_LORA_SERIAL_PATH` (USB CP2102) |
| `MAVLINK_BRIDGE_SERIAL_PATH` | `mavlink_bridge_app` `OpenSerial()` | `MAVLINK_BRIDGE_APP_SERIAL_PATH` (`/dev/serial0`) |

**실행 환경 결정:** WSL 로컬 cFS 빌드 (Pi `cFS_clean` 소스 복사 → x86 빌드).
반복 실행이 빠르고 sudo/하드웨어 의존 없음. 최종 확인은 Pi 실환경에서 별도 수행.

**단계별 목표:**

| Phase | 목표 | 상태 |
|---|---|---|
| B-1 | C 코드 env var override 추가 (위 표 2곳) + 본 문서 기록 | 진행 중 |
| B-2 | WSL에 cFS_clean 소스 복사, x86 빌드 (커스텀 앱 4개 포함) | 미착수 |
| B-3 | pytest cFS harness fixture — PTY 생성 → env var 설정 → `core-cpu1` subprocess 기동/종료 | 미착수 |
| B-4 | 관측/주입 인프라 — CI_LAB UDP(1234) 커맨드 주입, TO_LAB/EVS 텔레메트리 수신 검증 헬퍼 | 미착수 |
| B-5 | `test_rec_serial.py` REC-001~004 실구현 (skip 제거) | 미착수 |
| B-6 | `test_lora_fc_downlink_e2e.py` → **lora_tdm_app 대상으로 재작성** (원 대상 lora_fc_downlink_app은 baseline 제거됨) + `test_uplink_e2e.py` 실구현 | 미착수 |

---

## 미구현/미검증 항목

| 항목 | 비고 |
|---|---|
| ~~`mavlink_bridge_app` unit test~~ | 해소됨 — `mavlink_bridge_app/unit-test/` 구성 완료, 테스트 바이너리 4종(app/cmds/utils/dispatch) build-ut 등록·PASS (2026-07-20 확인) |
| ~~`lora_tdm_app` `ReportHousekeeping` 단위테스트~~ | 해소됨 — `coveragetest_lora_tdm_app.c` `Test_ReportHousekeeping` 추가, 16 assertion PASS (2026-07-20) |
| ~~`lora_tdm_app` `ReportLinkStatus` 단위테스트~~ | 해소됨 — `coveragetest_lora_tdm_app.c` `Test_ReportLinkStatus` 추가, 8 assertion PASS (2026-07-20) |
| `lora_tdm_app` SEQ_FAIL 경로 (TDM-RX-004) | 실물 하드웨어 RT 검증만 미실행 — 로직은 구현/단위테스트 완료(`A3_unittest_cases.md` C.1/C.2, 2026-07-14) |
| `lora_tdm_app` `RunCycle` TDM 타이밍 검증 | serial 의존 → Pi 런타임 필요 |
| `lora_tdm_app` LoRa 하드웨어 연동 | Pi 실물 serial 필요 |
| `uplink_app` LoRa serial read C 경로 단위테스트 | `ServiceLoRa()`/`ParseLoRaFrame()` static — 통합테스트로 대체 |
| `uplink_app` CRC16 C 구현 | `UPLINK_APP_CRC16()` static — `test_uplink_lora_frame.py`로 검증 예정 |
| `lora_fc_downlink_app` SB timeout alive (LORA-FC-008) | main loop — static 함수, 단위테스트 불가 |
| `lora_fc_downlink_app` LoRa 송신/수신 단위테스트 | static 함수 — `test_lora_fc_downlink_packet.py`로 검증 예정 |
| `lora_fc_downlink_app` HB 파싱 단위테스트 | static 함수 — `test_hb_parse.py`로 검증 예정 |
| `lora_fc_downlink_app` 실제 LoRa 하드웨어 검증 | Pi에서 실물 연결 필요 |
| B그룹 통합테스트 실행 | cFS + PTY 환경 필요 — 구조는 있음, 현재 pytest.skip() |

---

## 앱별 × Runtime 테스트 매트릭스

각 앱이 어떤 runtime 레벨에서 무엇을 검증하는지 정의한다.

### 범례

| 레벨 | 설명 | 도구 | 의존성 |
|---|---|---|---|
| **Unit** | C 코드 공개 API 함수 단위 | cFS `unit-test/` (coveragetest) | 없음 |
| **PyUnit (A)** | Python 동등 구현 검증 | pytest `tests/test_*.py` (--cfs 불필요) | 없음 |
| **E2E (B)** | cFS 실행 + mock serial | pytest `tests/test_*_e2e.py` (--cfs 필요) | WSL x86 빌드 또는 Pi |
| **Runtime** | Pi 실물 + 하드웨어 | 수동/스크립트 | Pi + FC + LoRa 등 |

---

### **lora_tdm_app** — LoRa TDM (다운링크 TX + 업링크 RX)

| 테스트 레벨 | 검증 항목 | 파일 | TC 범위 | 상태 |
|---|---|---|---|---|
| **Unit** | `OpenSerial()` 재시도 로직 | `coveragetest_lora_tdm_app.c` | — | ✓ 있음 |
| | `CloseSerial()` 유발 경로 (`RunTx`/`RunRxWindow` write/read 실패 → `fd=-1`) | `coveragetest_lora_tdm_app.c` | — | ✓ 있음 (2026-07-13, [[lora_tdm_serial_reopen_gap]] P3) |
| | `ProcessRxLine()` HB/UP 파싱 | `coveragetest_lora_tdm_app_utils.c` | TDM-RX-001~008 | ✓ 있음 |
| | `BuildFcDownlinkLine()` FC 패킷 포맷 | `coveragetest_lora_tdm_app_utils.c` | TDM-DOWN-001~006 | ✓ 있음 |
| | `UpdateLinkState()` 링크 상태 전이 | `coveragetest_lora_tdm_app_utils.c` | TDM-LINK-001~005 | ✓ 있음 |
| **PyUnit (A)** | HB 프레임 파싱 | `test_hb_parse.py` | LORA-HB-001~010, REC-005 | ✓ 구현 |
| | UP 프레임 파싱 | `test_uplink_lora_frame.py` | LORA-UP-001~013 | ✓ 구현 |
| | FC/SH 다운링크 패킷 포맷 | `test_lora_fc_downlink_packet.py` | LORA-FRAME-001~008 | ✓ 구현 |
| **E2E (B)** | PTY mock serial → lora_tdm_app 제어 | `test_rec_serial.py` | REC-001~004 (serial open/disconnect/recover) | ⏸️ pytest.skip() |
| | cFS SB message → LoRa 실제 출력 | `test_lora_fc_downlink_e2e.py` (재작성) | LORA-FRAME, LORA-FC-006~007 | ⏸️ pytest.skip() |
| | UP 수신 → SB 라우팅 | `test_uplink_e2e.py` | LORA-UP-011~013, REC-008 | ✓ 구현됨 (`--cfs` 게이트, 2026-07-20 정정) |
| **Runtime** | LoRa 하드웨어 TDM 사이클 | 수동 PI 테스트 | TDM-RT-001~009 | 미실행 (LoRa 필요) |

---

### **mavlink_bridge_app** — FC MAVLink 수신/파싱/게시

| 테스트 레벨 | 검증 항목 | 파일 | TC 범위 | 상태 |
|---|---|---|---|---|
| **Unit** | `OpenSerial()`, `CloseSerial()` | `coveragetest_mavlink_bridge_app.c` | — | ✓ 있음 |
| | MAVLink 메시지 파싱 (ATTITUDE 등) | `coveragetest_mavlink_bridge_app_utils.c` | MAV-PARSE-001~010 | ✓ 있음 |
| | 캐시 업데이트 | `coveragetest_mavlink_bridge_app_cmds.c` | MAV-CACHE-001~005 | ✓ 있음 |
| | 스트림 요청(`RequestTelemetryStreams`, COMMAND_LONG 6종) (2026-07-20 추가) | `coveragetest_mavlink_bridge_app_utils.c` | — | ✓ 있음 (socketpair로 실제 write 캡처, TargetSystemId==0 시 미전송 케이스 포함) |
| | heartbeat timeout, link state | `coveragetest_mavlink_bridge_app_dispatch.c` | — | ✓ 있음 |
| | `SendMissionItemInt` GLOBAL_RELATIVE_ALT frame/lat-lon degE7 인코딩 (2026-07-13, [[mission_item_int_frame_gap]]) | `coveragetest_mavlink_bridge_app_utils.c` | — | ✓ 있음 (GLOBAL_POSITION_INT 주입 + socketpair로 실제 write 캡처, legacy 공식과 비교) |
| | FC 값 finite 검증 — NaN/Inf 수신 시 미게시 (2026-07-13, [[fc_value_validation_gap]] 설계안 A) | `coveragetest_mavlink_bridge_app_utils.c` | — | ✓ 있음 (ATTITUDE NaN, LOCAL_POSITION_NED +Inf 거부 + 정상값 통과 회귀 3건) |
| **PyUnit (A)** | MAVLink UART 메시지 디스크립션 | `test_mavlink_uart_bridge.py` | MAV-DESC-001~005 | ✓ 구현 |
| | parse_args 기본값/커스텀 | `test_mavlink_uart_bridge.py` | MAV-ARGS-001~005 | ✓ 구현 |
| **E2E (B)** | PTY mock FC serial → ATTITUDE/LOCAL 수신 | `test_mavlink_bridge_e2e.py` (미작성) | MAV-E2E-001~003 (serial open, msg rx, heartbeat timeout) | ❌ 미작성 |
| | FC serial 분리/재연결 동작 | `test_rec_serial.py` | REC-003~004 (FC serial recover) | ⏸️ pytest.skip() |
| **Runtime** | FC 실물 연결 (UART /dev/serial0) | Pi 수동 테스트 | RT-FC-001~003 | ✓ 확인됨 |

---

### **uplink_app** — 지상국 명령 수신/검증/라우팅

| 테스트 레벨 | 검증 항목 | 파일 | TC 범위 | 상태 |
|---|---|---|---|---|
| **Unit** | `ProcessUplink()` seq 거부 로직 | `coveragetest_uplink_app_utils.c` | UPLINK-SEQ-001~005 | ✓ 있음 |
| | route update 캐시 및 라우팅 | `coveragetest_uplink_app_utils.c` | UPLINK-ROUTE-001~010 | ✓ 있음 |
| | config/viewpoint payload 검증 | `coveragetest_uplink_app_utils.c` | UPLINK-CONFIG-001~010 | ✓ 있음 |
| **PyUnit (A)** | LoRa UP 프레임 파싱 | `test_uplink_lora_frame.py` | LORA-UP-003~013, CFS-CMD-001~008 | ✓ 구현 |
| | route update sender (Python ↔ C 동등성) | `test_uplink_route_update_sender.py` | UPLINK-ROUTE-PYEQUIV-001~005 | ✓ 구현 |
| | config sender payload/checksum | `test_uplink_config_sender.py` | UPLINK-CONFIG-PYEQUIV-001~012 | ✓ 구현 |
| **E2E (B)** | UDP → uplink_app → SB 경로 | `test_uplink_e2e.py` | LORA-UP-011~013 (seq increase/reject/regression) | ✓ 구현됨 (`--cfs` 게이트, 2026-07-20 정정 — 원 표기 `pytest.skip()`은 stale, 실제로는 조건부 실행) |
| | LoRa serial RX → SB 라우팅 | `test_uplink_e2e.py` | REC-008 (seq regression count) | ✓ 구현됨 (`--cfs` 게이트) |
| **Runtime** | 지상국 명령 → FC MISSION 업로드 | Pi + FC 수동 테스트 | RT-UPLINK-001~003 | 미실행 (LoRa 필요) |

---

### **cfs_core_app** — 헬스 판단 및 상태 종합

| 테스트 레벨 | 검증 항목 | 파일 | TC 범위 | 상태 |
|---|---|---|---|---|
| **Unit** | health state machine (NOMINAL/DEGRADED/FAILED) | `coveragetest_cfs_core_app_utils.c` | HEALTH-STATE-001~010 | ✓ 있음 |
| | HK 게시 및 상태 캐시 | `coveragetest_cfs_core_app.c` | — | ✓ 있음 |
| | FC 타임스탐프 검증 (미래값 거부) | `coveragetest_cfs_core_app_utils.c` | HEALTH-TIME-001~005 | ✓ 있음 |
| **PyUnit (A)** | 없음 (헬스 로직은 상태 머신 — Python 동등 구현 불필요) | — | — | N/A |
| **E2E (B)** | health state 전이 (FC timeout 시뮬레이션) | `test_cfs_core_health_e2e.py` (미작성) | HEALTH-E2E-001~005 | ❌ 미작성 |
| **Runtime** | FC 분리 → health 1→2 전이, 재연결 시 복구 | Pi + FC 수동 테스트 | RT-HEALTH-001~002 | ✓ 확인됨 |
| | uplink_app/lora_tdm_app HK timeout → 자동 재시작 (bridge와 동일 패턴 확장, 2026-07-13) | `tools/runtime_app_restart_test.sh <uplink_app\|lora_tdm_app>` — `CFE_ES_STOP_APP_CC`로 실제 정지시켜 HK 끊김 재현, journalctl에서 재시작 EID(15/16) + 재기동 확인 | RT-CORE-003, RT-CORE-004 (§"추가 런타임 시험 후보 — FC 장애/깨진 값" ③ 참조) | ⬜ 절차만 기록, 미실행 — 다른 런타임 시험들과 함께 일괄 실행 예정 |

---

**Notes:**
- ✓ = 구현됨 / ⏸️ = 구조 있음, 현재 pytest.skip() / ❌ = 미작성 / N/A = 해당 없음
- E2E(B) 구현 순서: 공통 harness (B-3, B-4) → REC-001~004 → 각 E2E 테스트
- Runtime 항목은 Pi 연결 환경에서만 가능

---

## 현재 실행 가능한 런타임 시험 (2026-06-17 기준)

**전제 조건**: Pi(`192.168.50.65`) + FC 연결 + `sudo ./core-cpu1` 실행 중

### ✅ 완료된 런타임 확인 항목

| 항목 | 확인 내용 | 결과 |
|---|---|---|
| RT-BOOT-001 | cFS `OPERATIONAL` 진입, 커스텀 앱 4개 Initialized | ✅ 확인 |
| RT-BOOT-002 | SCH_LAB SEND_HK ~1Hz 트리거 (CFS_CORE_APP HK 출력 확인) | ✅ 확인 |
| RT-FC-001 | FC 하트비트 수신 → `sys=1`로만 스트림 요청 (주변기기 sysid 무시) | ✅ 확인 |
| RT-FC-002 | FC 텔레메트리 수신 (ATTITUDE, LOCAL_POSITION_NED, GPS_RAW_INT) | ✅ 확인 |
| RT-FC-003 | `health 2->1` BRIDGE_TIMEOUT 해소, NOMINAL 복귀 | ✅ 확인 |

### 🔲 Pi + FC로 지금 바로 실행 가능 (LoRa 불필요)

| ID | 시험 항목 | 방법 | 기대 EVS |
|---|---|---|---|
| RT-HEALTH-001 | FC serial 분리 → BRIDGE_TIMEOUT | `/dev/serial0` 물리 분리 후 3초 대기 | `health 0->2 fault=1` (NOMINAL→RECOVERY; enum NOMINAL=0/DEGRADED=1/RECOVERY=2/FAILED=3) |
| RT-HEALTH-002 | FC serial 재연결 → NOMINAL 복구 | serial 재연결 | `health 2->1` → 10초 안정화 후 `health 1->0` |

### 🔲 추가 런타임 시험 후보 — FC 장애/깨진 값 (2026-07-09 도출)

mavlink_bridge_app의 FC 장애 처리와 cfs_core_app 보고 경로 검증.
관측 수단이 실제 코드에 존재하는 항목만 포함 (EVS/HK 필드 기준).

**검증 상태 범례** (2026-07-10 코드·설계 대조 결과):
`✅ 실물` = Pi+FC/LoRa 실물로 즉시 검증 · `🔧 CI_LAB` = 테스트 startup scr(CI_LAB) 명령 주입 필요 ·
`🔵 E2E(B)` = 바이트/타이밍 정밀 제어 필요, PTY/CI_LAB 하네스가 적합 · `⚠️ 구현갭` = 코드 구현 선행 후에만 검증 가능

**① FC 보드 장애:**

| ID | 시나리오 | 검증 내용 | 관측 수단 (판정 기준) | 검증 상태 |
|---|---|---|---|---|
| RT-FC-004 | FC 전원 차단 (보드 죽음) | stale 마킹 + core_app 보고 | cfs_core `health 0->1 fault=3(EKF_INVALID)` — Ekf/Local/Attitude 전부 stale 시 우선순위 체인이 EKF 먼저 보고 (`cfs_core_app_utils.c:307`). stale TLM은 SB 재게시 안 됨(`MarkOutputsStale`은 내부 플래그만) → `AttitudeStatus.TimedOut=1` 등 cfs_core HK로 관측 | ✅ 실물 |
| RT-FC-005 | FC 재부팅 (일시 중단 후 복귀) | stale 해소 + NOMINAL 복귀 | FC 복귀 후 10초 안정화 → `health 1->0`. ⚠️ `ReconnectAttemptCount`는 UART 무신호(EAGAIN) 경로에서 증가 안 함 — read 오류 errno일 때만 close/재연결 (`mavlink_bridge_app_utils.c:1862-1870`) | ✅ 실물 (Reconnect 카운트 판정 제외) |
| RT-FC-006 | FC 부팅 직전 백로그 burst | 재연결 시 `tcflush`로 밀린 데이터 폐기 | SB queue overflow EVS 없음 | ✅ 실물 |

**② 깨진 값 수신:**

| ID | 시나리오 | 검증 내용 | 관측 수단 (판정 기준) | 검증 상태 |
|---|---|---|---|---|
| RT-FC-007 | serial 노이즈 (baud 불일치/접촉불량 모사) | CRC 실패 프레임 SB 미게시 | HK `ParseErrorCount` 증가, ATTITUDE 값 불변 | 🔵 E2E(B) PTY |
| RT-FC-008 | 부분 프레임 (전송 중 절단) | 파서 리셋 후 다음 정상 프레임 파싱 재개 | 이후 ATTITUDE `Seq` 계속 증가 | 🔵 E2E(B) PTY |
| RT-FC-009 | EKF 상태 불량 (FC 살아있으나 값 신뢰불가) | core_app EKF_INVALID 판단 | `health 0->1 fault=3(EKF_INVALID)` | ✅ 실물 (FC EKF 불량 유발 조건 필요) |

**③ cfs_core_app 보고 경로 (연쇄 검증):**

| ID | 시나리오 | 검증 내용 | 관측 수단 (판정 기준) | 검증 상태 |
|---|---|---|---|---|
| RT-CORE-001 | FC 타임스탬프 이상 (미래값) | core_app 거부 | `future timestamp rejected` EVS | 🔵 E2E(B) (미래 ts 주입 필요) |
| RT-CORE-002 | 메시지 유실 (seq 건너뜀) | 갭 감지 카운트 | HK `SeqGapCount` 증가. `SEQ_GAP_EID`는 **DEBUG 타입**이라 EVS DEBUG enable 선행 필요 | 🔵 E2E(B) (seq 조작 + DEBUG enable) |
| RT-CORE-003 | uplink_app HK timeout → 자동 재시작 (2026-07-13 도출) | `CFE_ES_STOP_APP_CC`로 uplink_app 강제 정지 → HK 5s 끊김 → cfs_core_app이 `CFE_ES_RestartApp` 호출 | `tools/runtime_app_restart_test.sh uplink_app` 실행 → journalctl에서 `CFS_CORE_APP_UPLINK_RESTART_EID(15)` "uplink restart attempt=1" + cFE `CFE_ES_RESTART_APP_INF_EID(10)` + uplink_app STARTUP 재등장 확인 | 🔧 CI_LAB (STOP_APP 명령 주입, FC 불필요). 절차만 기록, **미실행** — build/ 재빌드 + `sudo systemctl restart cfs.service` 선행 필요(사용자 직접) |
| RT-CORE-004 | lora_tdm_app HK timeout → 자동 재시작 (2026-07-13 도출) | `CFE_ES_STOP_APP_CC`로 lora_tdm_app 강제 정지 → HK 5s 끊김 → cfs_core_app이 `CFE_ES_RestartApp` 호출 | `tools/runtime_app_restart_test.sh lora_tdm_app` 실행 → journalctl에서 `CFS_CORE_APP_LORA_RESTART_EID(16)` "lora restart attempt=1" + cFE `CFE_ES_RESTART_APP_INF_EID(10)` + lora_tdm_app STARTUP 재등장 확인 | 🔧 CI_LAB (STOP_APP 명령 주입, FC 불필요). 절차만 기록, **미실행** — RT-CORE-003과 동시에 실행(같은 재빌드/재기동 1회로 둘 다 검증 가능) |
| RT-CORE-005 | **상위 fault 지속 중 재시작 발동** (BL-38 회귀, 2026-07-23 도출) | 실내(GPS 없음, `fault=3 EKF_INVALID` 상시) 상태에서 uplink_app STOP → 상위 fault가 있어도 `CheckAppRestarts()`가 독립 발동 | journalctl에서 HK `FaultCode=3` 유지 **중에** `UPLINK_RESTART_EID(15)` + uplink 재기동 확인. 2026-07-22 실측 FAIL 시나리오의 정확한 재현 조건 | 🔧 CI_LAB. BL-38 수정 후 실행 — **RT-CORE-003을 실내에서 실행하면 이 케이스를 겸함** |
| RT-CORE-006 | **동시 다중 사망 → 사이클당 1건·우선순위 순차** (2026-07-23 도출) | uplink_app + lora_tdm_app 동시 STOP → 우선순위(uplink > lora)대로 사이클당 1건씩 순차 재시작 | journalctl에서 EID 15가 EID 16보다 **먼저**(같은 초 아님) 발생 + 양쪽 모두 재기동. 재시작 발행 간 최소 1사이클 간격 | 🔧 CI_LAB (STOP_APP 2연발) |
| RT-CORE-007 | **무한 재시도 (3회 초과 지속)** (2026-07-23 도출) | 앱을 STOP 상태로 유지(START 없이 방치 불가하므로: STOP 직후 재기동되는 앱을 반복 STOP)해 attempt 4 이상 관측 | journalctl에서 `restart attempt=4` 이상 발생(구 MAX_RESTARTS=3 초과) + HK 재시도 카운터 단조 증가 | 🔧 CI_LAB. 재현 난도 있음 — 4회 반복 STOP 스크립트로 대체 가능 |
| RT-CORE-008 | **ExceptionAction 기존 동작 확인** (2026-07-23 도출, 정정: 수정 아닌 기존 동작 검증) | 앱 크래시(예외) 시 cFE ES가 프로세서 리셋 없이 해당 앱만 즉시 재시작 — startup.scr 8번째 필드가 4개 앱 전부 이미 `0`(=앱만 재시작)이라 코드/설정 변경 없이 이미 성립해야 함 | 크래시 유발 수단 필요(kill -SEGV는 스레드 모델이라 불가 — 별도 검토). 관측: cFS 전체 재기동 **없이** 해당 앱 STARTUP 재등장 | ⬜ 주입 수단 미정 — 실행 방법 결정 후 절차 확정 |
| RT-CORE-009 | **FaultCode 보고 불변 회귀** (2026-07-23 도출) | 재시작 분리 후에도 else-if 체인의 FaultCode 우선순위 의미 불변 | EKF fault + uplink 사망 동시 상태에서 HK `FaultCode=3`(상위) 유지 + `UplinkStatus.TimedOut=1` 개별 플래그 병행 관측 (RT-CORE-005 실행 중 함께 판정) | 🔧 CI_LAB (RT-CORE-005와 동일 세션) |
| RT-CORE-010 | **cfs_core 자체 ExceptionAction 기존 동작 확인** (2026-07-23 도출, 정정: 이미 `0` 설정) | 감시자(cfs_core) 크래시 시 ES가 프로세서 리셋 없이 cfs_core만 재시작 + 재기동 후 감시·health 발행 **기능 복원** | cFS 전체 재기동 없이 cfs_core STARTUP 재등장 → SYSTEM_HEALTH 다운링크 재개 (health 발행 재개 = 메인 루프 동작 = CheckAppRestarts 동일 루프이므로 감시 복원의 충분한 증거). SaveState/LoadState 상태 복원 여부 병행 관측(BL-39 수정 전제) | ⬜ RT-CORE-008과 동일 — 크래시 주입 수단 미정 |
| RT-CORE-011 | **재시작→첫 HK 시간 실측 (이중 재시작 여부)** (2026-07-23 도출) | 재시작된 앱의 HK 복귀가 쿨다운 5초 이내인지 — 초과 시 살아나는 앱을 재차 재시작(attempt=2)하는 이중 재시작 발생 | RT-CORE-003 실행 로그에서 `restart attempt=1` 타임스탬프 → 해당 앱 첫 HK(타임아웃 해제) 시각 차이 측정. 5초 초과·attempt=2 관측 시 `*_RESTART_INTERVAL_MS` 상향 검토 | 🔧 CI_LAB — RT-CORE-003 로그 분석으로 겸함(별도 실행 불요) |

> **주:** RT-FC-007/008(깨진 바이트 주입)은 실물 재현이 어려워 **E2E(B) PTY 테스트가 더 적합**
> (PTY로 원하는 깨진 바이트를 정확히 주입 가능). 런타임에서는 ①③이 실물 검증 가치가 높다.

### 🔲 추가 런타임 시험 후보 — BL-41/42/43/45 영속화·time base·기본값 (2026-07-24 신설)

> 단위테스트(coveragetest)는 각 기능 구현 시 작성·green 확인됐으나(BACKLOG.md 참조),
> Pi 실기에서의 확인 절차가 이 문서에 등재돼 있지 않았던 항목들. 사전조건은 별도
> 표기 없는 한 Pi+FC 정상 연결 상태.

**④ CONFIG 영속화 (BL-41):**

| ID | 시나리오 | 검증 내용 | 관측 수단 (판정 기준) | 검증 상태 |
|---|---|---|---|---|
| RT-CONFIG-001 | CONFIG 파라미터 변경 → 재부팅 → 값 유지 | `tools/uplink_config_sender.py`로 3개 앱 중 1개(예: cfs_core `AttitudeTimeoutMs`) 변경 → `sudo systemctl restart cfs.service` → 재부팅 후 HK로 변경값 유지 확인 | HK 파라미터 필드가 기본값이 아닌 전송값과 일치. `/cf/cfs_core_app_state.bin` 등 상태파일 mtime이 전송 시점과 일치(SaveState 실제 호출 증거) | ✅ **PASS(2026-07-24)**. `attitude_timeout_ms=4321` 전송(DEGRADED라 `--force` 필요, `FORCED THROUGH health gate`) → `config activated param=0 value=4321` → 6회 연속 재부팅 내내 `restored health state=1 config(attitude=4321 ...)` 유지 확인 |
| RT-CONFIG-002 | 상태파일 손상 시 기본값 폴백 | 재부팅 전 `cf/*_state.bin`을 일부 바이트 덮어써 손상 후 재기동 | `STATE_CORRUPT_EID(19)`(cfs_core) 등 손상 EVS 발생 + HK가 기본값으로 복귀(크래시 아님) | ⬜ 미실행 |
| RT-CONFIG-003 | 3개 앱(mavlink_bridge/lora_tdm/cfs_core) 동시 영속화 | 3개 앱 각각 CONFIG 변경 후 동시 재부팅 | 3개 앱 전부 HK에서 개별 변경값 유지(상호 간섭 없음) | ⬜ 미실행 |

**⑤ route readback — FC를 진실원본으로 하는 RAM 캐시 (BL-41):**

| ID | 시나리오 | 검증 내용 | 관측 수단 (판정 기준) | 검증 상태 |
|---|---|---|---|---|
| RT-ROUTE-001 | FC 링크 CONNECTED 전이 시 자동 재조회 | mavlink_bridge 재시작(링크 재연결) → `FC_MISSION_READBACK_MID(0x1914)` 자동 발행 확인 | cfs_core HK `MissionRouteWaypointCount`가 FC에 이미 업로드된 실제 waypoint 수와 일치 | ✅ **PASS(2026-07-24)**. cFS 재기동 직후 `mission_wp=2`가 재부팅 전과 동일 `last_route_ts`로 즉시 재등장 — 파일 영속 아닌 FC readback으로 캐시 재구성 확인 |
| RT-ROUTE-002 | ROUTE_UPDATE 업로드 완료 후 캐시 갱신 | `tools/uplink_route_update_sender.py`로 REPLACE 업로드 → `MISSION_ACK` accepted 확인 후 캐시 자동 갱신 | cfs_core `MissionRoute` 캐시가 업로드한 waypoint와 일치(readback 경유 확인) | ⬜ 미실행 |
| RT-ROUTE-003 | `tools/query_fc_mission.py`로 수동 재조회(MISSION_QUERY_CC) | 수동 재조회 명령 전송 → 캐시 갱신 | `FC_MISSION_READBACK_MID` 발행 로그 + 캐시 갱신 확인 | ⬜ 미실행 |
| RT-ROUTE-004 | timeout 지수 백오프(1→2→4→5s) | FC 무응답 상태에서 재조회 시도 간격 측정 | journalctl 타임스탬프 간격이 1s→2s→4s→5s(상한) 패턴, 무한 재시도(포기 없음) | ⬜ 미실행 |

**⑥ BootCount 신뢰성 — time base 검증 (BL-42):**

| ID | 시나리오 | 검증 내용 | 관측 수단 (판정 기준) | 검증 상태 |
|---|---|---|---|---|
| RT-TIMEBASE-001 | FC 재부팅 시 time base 불연속 감지 | FC 전원 재투입(companion은 유지) → `Msg->TimestampMs`(FC time_boot_ms)가 0 부근으로 리셋되는 순간 관측 | `TIMEBASE_SHIFT_EID(20)` 발생 + HK `TimebaseShiftCount` 증가. 동시에 만료 판정(ArrivalMs 기준)은 **끊기지 않고** 정상 유지(false expiry 없음) | ⬜ 미실행 |
| RT-TIMEBASE-002 | 링크 두절 중 만료 정상 감지(회귀) | mavlink_bridge 프로세스 정지 등으로 FC 상태 갱신 중단 | 각 State(Attitude/Local/Gps/Ekf)가 `ArrivalMs` 기준으로 timeout 후 정상 stale 처리(구 TimestampMs 기준 버그였다면 FC 재부팅 전엔 만료가 늦게 감지될 수 있었음) | ⬜ 미실행 |

**⑦ 부팅 루프 감지 — 생존 마커 (BL-43):**

| ID | 시나리오 | 검증 내용 | 관측 수단 (판정 기준) | 검증 상태 |
|---|---|---|---|---|
| RT-BOOTLOOP-001 | 정상 부팅 — 생존 마커 정착 | 부팅 후 120초 이상 정상 유지 | uplink `StatusTlm.BootLoopSuspect=0`, `ShortBootStreak=0` 유지. 상태파일 `SurvivedMark=1` 기록 확인(재부팅 전 read) | ✅ **PASS(2026-07-24)**. 120초 경과 시 `UPLINK_APP: boot survival marked (uptime >= 120000ms)` EVS 확인 |
| RT-BOOTLOOP-002 | 반복 단명 재부팅(5회) → BootLoopSuspect | 120초 미만 생존 후 강제 재부팅을 5회 반복(`sudo systemctl restart cfs.service`를 100초 간격으로) | 5회차 부팅 후 uplink HK `BootLoopSuspect=1`, `ShortBootStreak=5`. 보고만 하고 자동 대응(강제 착륙 등) 없음 확인 | ✅ **PASS(2026-07-24)**. 12초 간격 6회 재부팅 → 6번째 부팅에서 정확히 `boot loop suspect - 5 consecutive short boots` 발생(threshold=5 정확) |
| RT-BOOTLOOP-003 | cfs_core 재시작 카운터·LastFaultCode 영속 | RESTART_BRIDGE/UPLINK/LORA 각 1회 실행 후 재부팅 | cfs_core HK `BridgeRestartCount`/`UplinkRestartCount`/`LoraRestartCount`가 재부팅 후에도 유지(0으로 리셋 안 됨), `LastFaultCode`도 마지막 값 유지 | ⬜ 미실행 |

**⑧ v2(DL2) 다운링크 컴파일타임 기본값 (BL-45):**

| ID | 시나리오 | 검증 내용 | 관측 수단 (판정 기준) | 검증 상태 |
|---|---|---|---|---|
| RT-DL2-DEFAULT-001 | 첫 부팅(CONFIG 저장 이력 없음) 시 v2로 송신 | `cf/lora_tdm_app_state.bin` 삭제 후 재부팅(첫 부팅 재현) | 지상 수신 프레임이 `0xD2`(DL2 magic)로 시작 — v1 ASCII(`F`/`S`) 아님 | ✅ **PASS(2026-07-24, 부수 확인)**. 별도 CONFIG 미전송 상태에서 `ACK2 seq mismatch` 이벤트 지속 관측 — ACK2는 v2 전용 명칭(v1은 `ACK,<seq>`)이라 기본 v2 송신 확인 |
| RT-DL2-DEFAULT-002 | 저장된 v1 설정이 있으면 재부팅 후에도 v1 유지 | CONFIG로 v1(`PARAM_DOWNLINK_PROTOCOL=0`) 전송 후 재부팅 | 지상 수신 프레임이 v1 텍스트(`F,...`)로 유지 — LoadState가 Init 기본값(v2)을 덮어씀 확인 | ⬜ 미실행 |

**⑨ FLIGHT_MODE base 명령 (BL-44, 2026-07-24 신규):**

| ID | 시나리오 | 검증 내용 | 관측 수단 (판정 기준) | 검증 상태 |
|---|---|---|---|---|
| RT-FLIGHTMODE-001 | HOVER — DEGRADED 헬스게이트 예외 허용 | health DEGRADED(fault=3, 실내 GPS 없음) 상태에서 HOVER(auth=3) 전송 | `UPLINK_APP: routed uplink class=8 ... target=4` → `MAVLINK_BRIDGE_APP: flight mode set mode=0` → `exec result ... generic=0`(OK), 차단 없음 | ✅ **PASS(2026-07-24)**. FC 보드 연결(프로펠러 미장착) 상태로 실기 확인 |
| RT-FLIGHTMODE-002 | WAYPOINT — DEGRADED 헬스게이트 정상 차단 | 동일 DEGRADED 상태에서 WAYPOINT(auth=3) 전송 | `UPLINK_APP: command blocked by health state=1 class=8` — mavlink_bridge로 미전달 | ✅ **PASS(2026-07-24)** |
| RT-FLIGHTMODE-003 | LAND — DEGRADED 헬스게이트 예외 허용 | 동일 DEGRADED 상태에서 LAND(auth=3) 전송 | `flight mode set mode=2` → `exec result generic=0`(OK), 차단 없음 | ✅ **PASS(2026-07-24)** |
| RT-FLIGHTMODE-004 | auth Level 3 미달 거부 | HOVER를 auth=2로 전송(Level 3 요구) | `UPLINK_APP: command blocked (insufficient auth) auth=2 required=3 class=8` | ✅ **PASS(2026-07-24)** |
| RT-FLIGHTMODE-005 | PX4 COMMAND_LONG/MISSION_SET_CURRENT wire 왕복(FC 응답 확인) | WAYPOINT를 NOMINAL(비-DEGRADED) 상태에서 전송해 실제 FC 모드 전환 확인 | FC가 실제로 AUTO/MISSION 모드로 전환됐는지 GCS/FC 텔레메트리로 확인 | ⬜ 미실행 — 실외 GPS로 NOMINAL 도달 필요(WAYPOINT는 DEGRADED에서 차단되므로 이번 세션은 게이트 검증까지만 수행) |

**⑩ PX4 미션 업로드 quirk 재검증 (BL-49, 2026-07-24):**

| ID | 시나리오 | 검증 내용 | 관측 수단 (판정 기준) | 검증 상태 |
|---|---|---|---|---|
| RT-PX4-MISSION-001 | ArduPilot 유래 quirk 3종의 PX4 유효성 | `tools/uplink_route_update_sender.py route-good-no-gps`(REPLACE 2-waypoint, DEGRADED 상태에서도 허용되는 클래스)로 실제 PX4에 업로드 | `MAVLINK_BRIDGE_APP: mission upload success wp_count=2` → readback 왕복(`FC mission readback applied wp_count=2`)까지 값 일치 확인 — `MISSION_CLEAR_ALL` 선행/`GLOBAL_RELATIVE_ALT` frame 변환/`sysid=255` 전부 PX4에서 유효 | ✅ **PASS(2026-07-24)** |

### 🔲 추가 런타임 시험 후보 — LoRa 링크/지상 명령 (2026-07-10 도출)

lora_tdm_app 장애 처리와 uplink_app 명령 검증/차단 경로.
기존 TDM-RT-001~009 (다운링크 주기, ACK→CONNECTED, no-ACK→DEGRADED/DISCONNECTED,
정상 UP 프레임 라우팅, CRC 오류 UFB, RX 윈도우, serial open 재시도)와 중복 없는 항목만 도출.
관측 수단이 실제 코드에 존재하는 항목만 포함 (EVS EID / HK 필드 기준).

**① lora_tdm_app — LoRa 모듈 장애/깨진 프레임 (TDM-RT-001~009 이후 갭):**

| ID | 시나리오 | 검증 내용 | 관측 수단 (판정 기준) | 검증 상태 |
|---|---|---|---|---|
| RT-LORA-001 | LoRa USB 모듈 런타임 분리 (동작 중 뽑기) | write/read 오류 감지 + 재오픈 시도 | `SERIAL_WRITE_ERR_EID(8)`/`SERIAL_READ_ERR_EID(9)` → `CloseSerial()`로 `fd=-1` → 다음 RunCycle 재오픈 → TxCount 재개. (구현 완료 2026-07-10, `lora_tdm_app.c` `RunTx`/`RunRxWindow`; [[lora_tdm_serial_reopen_gap]]) | Unit(실제 POSIX fd 조작, mock 아님): ✓ 있음 (2026-07-13, `coveragetest_lora_tdm_app.c` `Test_RunCycle_TxWriteFailClosesFd`/`Test_RunCycle_RxReadFailClosesFd` — closed-fd write→EBADF, write-only-fd read→EBADF 각각 `LoRaFd==-1` 확인, lora_tdm_app UT 9 PASS 회귀 없음). Runtime(물리 USB 분리): 미실측 |
| RT-LORA-002 | 깨진 ACK 수신 (형식 불일치) | ACK 파싱 실패 처리 | `ACK_PARSE_ERR_EID(10)` + HK `RxErrorCount` 증가, `RxAckCount` 불변 | 🔵 E2E(B) (깨진 ACK 바이트 주입) |
| RT-LORA-003 | UP 프레임 seq 재사용/역행 (재전송 공격 모사) | 시퀀스 검증 거부 | uplink `COMMAND_ERR_EID(2)` replay 거부 + 다음 다운링크 `UFB=SEQ_FAIL`(`LastCommandResult==REJECT_SEQUENCE`, §18.11.1). lora_tdm은 UP 프레임 자체 seq 검증을 하지 않고 uplink_app 판정 결과만 피드백 전달(`lora_tdm_app_dispatch.c:91-96`) — **`SEQ_FAIL_EID(12)`와는 무관**: EID 12는 ACK 응답의 `SeqEcho` 불일치 검출용 — 2026-07 구현 완료(`SeqEcho != LastSentSeq` 비교, `lora_tdm_app_utils.c:520-526`, 타이밍 버그 수정 commit `48c8d12`) | 🔵 E2E(B) (UP 프레임 seq 조작) |
| RT-LORA-004 | 링크 상태 전이 EVS 관측 (GS 정지→재개) | DEGRADED→DISCONNECTED→CONNECTED 전이 이벤트 | `LINK_DEGRADED_EID(14)` → `LINK_LOST_EID(13)` → `LINK_RESTORED_EID(15)`, HK `LinkState`/`NoAckCount` 일치 | ✅ 실물 (GS 정지/재개) |

**② uplink_app — 명령 검증/차단 (ProcessUplink 거부 파이프라인):**

| ID | 시나리오 | 검증 내용 | 관측 수단 (판정 기준) | 검증 상태 |
|---|---|---|---|---|
| RT-UPL-001 | 동일 seq 재전송 (replay) | 시퀀스 재사용 거부 | `COMMAND_ERR_EID(2)` "rejected replay seq=..." + HK `RejectedCount` 증가 | 🔵 E2E(B)/🔧 CI_LAB (GS 동일 seq 재전송 기능 시 ✅) |
| RT-UPL-002 | 부팅 직후 health 미수신 상태에서 명령 | fail-safe 차단 (health 수신 전 전면 거부) | `STATE_BLOCK_EID(6)` "command blocked (no health yet)" | 🔵 E2E(B) (수백 ms 타이밍 창, 수동 불가) |
| RT-UPL-003 | health DEGRADED 중 VIEWPOINT/CONFIG 명령 | 상태 기반 클래스 차단 (§18.10.1) | `STATE_BLOCK_EID(6)` "blocked by health state=..." + `RejectedCount` 증가 | 🔵 E2E(B)/🔧 CI_LAB (DEGRADED 유발 + 명령 주입) |
| RT-UPL-004 | health RECOVERY/FAILED 중 일반 명령 | RECOVERY+DIAGNOSTIC 클래스만 허용 | 일반 명령 `STATE_BLOCK_EID(6)`, RECOVERY 명령은 통과(`PUBLISH_EID(5)`) | 🔵 E2E(B)/🔧 CI_LAB |
| RT-UPL-005 | 권한 부족 명령 (auth_level < required) | 인가 차단, Level-3은 request_token 검증 | `AUTHZ_BLOCK_EID(7)` "insufficient auth auth=... required=..." | 🔵 E2E(B)/🔧 CI_LAB (auth 필드 조작) |
| RT-UPL-006 | 미지원 class 명령 | 라우팅 실패 처리 | HK `RoutingFailureCount` 증가 | 🔵 E2E(B)/🔧 CI_LAB |
| RT-UPL-007 | uplink_app 재시작 후 구 seq 재전송 | LastAcceptedSequence 영속(Magic+Checksum) 확인 | 재시작 후에도 구 seq `COMMAND_ERR_EID(2)` replay 거부 | 🔧 CI_LAB (STOP_APP/START_APP + seq 주입) |

**③ cfs_core_app 보고 경로 (연쇄 검증):**

| ID | 시나리오 | 검증 내용 | 관측 수단 (판정 기준) | 검증 상태 |
|---|---|---|---|---|
| RT-CORE-003 | uplink_app 정지 (ES `STOP_APP`, OS kill 불가—스레드) | core_app UPLINK_TIMEOUT 보고 | `health 0->1 fault=6(UPLINK_TIMEOUT)` → `START_APP` 후 `health 1->0` | 🔧 CI_LAB (startup_test.scr) |
| RT-CORE-004 | lora_tdm_app 정지 (ES `STOP_APP`) | core_app LORA_TIMEOUT 보고 | `health 0->1 fault=7(LORA_TIMEOUT)` → `START_APP` 후 `health 1->0` | 🔧 CI_LAB (startup_test.scr) |

> **주:** RT-LORA-002/003(깨진 ACK·seq 조작)과 RT-UPL-001~007(명령 페이로드 조작)은
> 프레임/명령을 바이트 단위로 제어해야 하므로 **E2E(B) PTY/CI_LAB 주입이 더 적합**.
> 런타임 실물 검증 가치가 높은 것은 RT-LORA-001(USB 분리), RT-LORA-004(링크 전이), ③ 연쇄 항목.

### 🔲 통합 런타임 시나리오 — 전체 앱 순차 세션 (2026-07-10 도출)

**목적**: 개별 검증된 단일 시나리오(RT-FC/RT-CORE/RT-LORA/RT-UPL)를 **하나의 연속 Pi 세션**에서
순서대로 실행 — 동시 장애(복합검증, 추후 별도 설계)가 아니라 **한 번에 한 결함만** 주입하고
매 단계 NOMINAL 복귀를 확인한 뒤 다음 단계로 진행. 물리 세팅(Pi+FC+LoRa 연결) 반복 비용을 줄이고,
앱 간 순차 전환이 서로 간섭하지 않는지(잔여 상태 누수 등) 확인하는 것이 목적.

**사전조건**:
- 4개 앱 기동, `health=NOMINAL(0)` 확인, LoRa 페어링 완료.
- **테스트용 startup script(`cpu1_cfe_es_startup_test.scr`, CI_LAB 포함) 사용** — cFS 앱은 core-cpu1 내
  스레드라 OS `kill`로 개별 정지 불가. 단계 3·4의 앱 정지/재시작은 CI_LAB(UDP 1234) 경유
  ES `STOP_APP`/`START_APP` 명령으로 주입.

| 단계 | 참조 ID | 주입 | 판정 기준 | 다음 단계 전 확인 |
|---|---|---|---|---|
| 1 | — | 세션 시작, 전체 기동 | `health 0(NOMINAL)`, TDM 다운링크 주기 수신 (TDM-RT-001) | NOMINAL 유지 |
| 2 | RT-FC-004→005 | FC 전원 차단 → 재연결 | `health 0->1 fault=3(EKF_INVALID)` → FC 복귀 후 stale 해소 → `health 1->0` | NOMINAL 복귀 확인 후 진행 |
| 3 | RT-CORE-003 | ES `STOP_APP UPLINK_APP` → 5초+ 대기 → `START_APP` | `health 0->1 fault=6(UPLINK_TIMEOUT)` → 재기동 후 `health 1->0` | NOMINAL 복귀 확인 |
| 4 | RT-CORE-004 | ES `STOP_APP LORA_TDM_APP` → 5초+ 대기 → `START_APP` | `health 0->1 fault=7(LORA_TIMEOUT)` → 재기동 후 `health 1->0` | NOMINAL 복귀 + TDM 다운링크 재개 확인 |
| 5 | RT-LORA-004 | 지상국 정지→재개 | `LINK_DEGRADED_EID(14)`→`LINK_LOST_EID(13)`→`LINK_RESTORED_EID(15)`, HK `LinkState` 일치. **링크 단절 중 `health=NOMINAL` 유지 확인** (LORA_TIMEOUT은 앱 HK 기준, 링크 상태 무관) | CONNECTED 복귀 |
| 6 | RT-UPL-001 | 정상 명령 1건 수락 → 동일 seq 재전송 | 수락 `PUBLISH_EID(5)` → 재전송은 `COMMAND_ERR_EID(2)` replay 거부 + `RejectedCount` 증가 | — |
| 7 | — | 세션 종료 | 전 구간(단계별 주입 구간 제외) `health=NOMINAL`, 4개 앱 HK 누적 카운터 기록 | — |

**설계 메모**:
- 단계 2~4는 cfs_core 우선순위 체인상 서로 다른 fault를 순차 유발 — 이전 fault 완전 해소(NOMINAL 복귀) 확인 후 다음 주입해야 원인 오귀속 방지.
- 단계 2 판정에서 `ReconnectAttemptCount` **제외**: FC 전원 차단 시 UART는 무신호(EAGAIN) 경로로
  빠져 serial close/재연결이 발생하지 않음 (`mavlink_bridge_app_utils.c:1862-1870`, close는 read
  오류 errno일 때만). 재연결 카운트는 USB serial 분리 유형에서만 유효.
- RT-UPL-002(no-health-yet 차단)는 **이 세션에서 제외**: uplink 재기동 후 health TLM 수신까지
  수백 ms라 수동 명령 주입으로 그 창을 맞출 수 없음 → E2E(B)에서 검증.
- RT-LORA-001(USB 런타임 분리)은 [[lora_tdm_serial_reopen_gap]] 구현 갭 해소 전까지 이 세션에서 **제외**.
- 단계 6 replay는 GS 툴이 동일 seq 재전송을 지원해야 함 (GS 측 기능 확인 선행).
- 세션 실패 시 어느 단계에서 끊겼는지로 회귀 원인 국소화 가능 (예: 단계 4에서만 실패 → lora_tdm 재기동 경로 문제).

### 🔲 LoRa 하드웨어 필요

| ID | 시험 항목 | 참조 |
|---|---|---|
| RT-TDM-001~009 | lora_tdm_app TDM 사이클, ACK, 링크 상태 전이 | 위 TDM-RT-001~009 |
| RT-DL-001 | FC → LoRa → GS PC 전 구간 텔레메트리 수신 | PC 수신 텔레메트리 시험 |
| RT-UL-001 | GS → LoRa → uplink_app → cFS SB 경로 | LoRa uplink 직접 수신 시험 |
| RT-DL2-SYSTIME-001 | v2(DL2) 다운링크에서 SysTime 블록 실제 도착 확인 — FC GPS 락 후 DL2 프레임 `flags bit0` 세팅 + 지상 CSV `sys_time_unix_usec` 컬럼에 값 채워짐 (2026-07-14, §16.4) | `notes/temp/gps_time_sync_164_implementation.md` — 단위테스트(TDM-CACHE-006, TDM-DL2-004~006)는 완료, 실기체 도착 확인만 남음 |
