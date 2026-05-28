# 테스트 케이스 정리

이 문서는 `cfs-telemetry-app` 저장소에 구현된 unit test와 런타임 시험 항목을 정리한다.

목적:
- 어떤 테스트가 이미 구현됐는지 한눈에 확인한다.
- 각 테스트가 무엇을 검증하는지 명시한다.
- 통합 테스트 또는 하드웨어 연동 시험 설계 시 기준 문서로 사용한다.

주의:
- 실제 요구사항과 시험 범위의 기준은 `notes/cfs_core_app_behavior_spec.md` 및 `notes/mission_app_runtime_spec.md`다.
- 하드웨어 미연결 시험은 내부 계약 검증까지만 포함하며, 실제 FC mission 반영 및 LoRa 물리 송신은 포함하지 않는다.

---

## Unit Test 현황 요약

| 앱 | 테스트 수 | assertion 수 | 마지막 확인 |
|---|---|---|---|
| `cfs_core_app` | 21 | 67 | 2026-05-28 |
| `uplink_app` | 37 | 143 | 2026-05-28 |
| `lora_fc_downlink_app` | 12 | 44 | 2026-05-28 |
| `mavlink_bridge_app` | 없음 (unit-test 미구성) | — | — |

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
| `CFS_CORE_APP_UpdateHealth_GpsStale` | GPS stale → health `DEGRADED`, fault `GPS_STALE` |
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
| `CFS_CORE_APP_ProcessStateMessage_BridgeHk` | bridge HK 수신 → bridge state cache 갱신 |

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
| `UPLINK_APP_ProcessUplink_AllowedRecovery` | RECOVERY 상태에서 RECOVERY class → 허용 |
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

---

### `lora_fc_downlink_app`

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
| `LORA_FC_DOWNLINK_APP_ProcessInputMessage` | attitude/local/gps/ekf/system health 입력별 timestamp/valid/downlink count/packet type 갱신 |

---

### `mavlink_bridge_app`

unit-test 디렉터리 미구성. 아래 항목은 런타임/도구를 통해 검증한다.

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
| `test_process_line_forwards_valid_frame_once` | 정상 프레임 → UDP 전송 1회, accept count 증가 |
| `test_process_line_rejects_sequence_regression` | 동일 sequence 재입력 → replay 거부 |
| `test_process_line_allows_sequence_regression_when_disabled` | strict sequence 비활성화 시 동일 sequence 허용 |
| `test_process_line_rejects_non_frame_text` | 비프레임 텍스트 입력 → 전송 없음 |

### `test_uplink_route_update_sender.py`

| 테스트 이름 | 검증 내용 |
|---|---|
| `test_opens_serial_with_correct_path_and_baudrate` | 올바른 시리얼 경로/baud rate로 open |
| `test_writes_frame_as_ascii_with_newline` | 프레임이 ASCII+개행으로 기록 |
| `test_calls_flush_after_write` | write 후 flush 호출 확인 |
| `test_raises_runtime_error_when_pyserial_unavailable` | pyserial 없을 때 RuntimeError 발생 |

---

## 런타임 시험

### Route Update 업로드 시험

도구: `tools/uplink_route_update_sender.py`

| 시험 항목 | 검증 내용 | 기대 결과 |
|---|---|---|
| `route-good` | 정상 mission extension route 수신/검증/publish | `UPLINK_APP: routed`, `CFS_CORE_APP: route updated type=1` |
| `route-landing` | 정상 landing route | `CFS_CORE_APP: route updated type=2` |
| `route-bad-alt` | 고도 2m 미만 route → 거부 | `invalid route update payload`, core route update 없음 |
| `route-bad-distance` | waypoint 거리 제약 위반 → 거부 | invalid route 로그 |

### FC 경로 업로드 시험 (§22)

도구: `tools/uplink_route_update_sender.py` → cFS → FC 직렬

| 시험 항목 | 검증 내용 | 기대 결과 |
|---|---|---|
| FC 링크 연결 상태에서 route update | mavlink_bridge_app이 MISSION_COUNT → MISSION_ITEM_INT × N → MISSION_ACK 수신 | EVS: `mission upload success wp_count=N`, HK `LastUploadResult==1` |
| FC 링크 미연결 상태에서 route update | 업로드 무시 | EVS: `route update ignored - FC link not connected` |
| 업로드 도중 타임아웃 (3회 재시도) | 재시도 후 실패 | EVS: `mission upload failed after 3 retries`, HK `LastUploadResult==3` |

### FC 경로 재조회 시험 (MISSION_QUERY_CC)

도구: `tools/query_fc_mission.py`

```bash
python3 tools/query_fc_mission.py [cFS_host_ip]
```

| 시험 항목 | 검증 내용 | 기대 결과 |
|---|---|---|
| 전원 재투입 후 미션 재조회 | FC 플래시에 저장된 웨이포인트가 유지되는지 확인 | EVS: `[wp 0] x=... y=... z=...` — 업로드 값과 일치 |
| 빈 미션 조회 | count=0 처리 | EVS: `MISSION download complete: 0 waypoints (empty)` |
| FC 링크 미연결 상태 조회 | 오류 반환 | EVS: `MISSION_QUERY ignored - FC link not connected` |
| 조회 타임아웃 | FC 응답 없을 때 3초 후 실패 | EVS: `MISSION_QUERY timeout` |

### PC 수신 텔레메트리 시험

시험 목적: FC → mavlink_bridge_app → cFS SB → lora_fc_downlink_app → LoRa → PC 전 구간 검증

| 시험 항목 | 판정 기준 |
|---|---|
| 30초 이상 연속 수신 | 공백 없이 프레임 도착 |
| 자세 변화 반영 | FC 자세 변경 시 PC 수신 값 변화 |
| FAIL-PI-IN | Pi에서 bridge 입력 없음 → FC 또는 bridge 입력 단계 문제 |
| FAIL-DOWNLINK | Pi 수신 정상, PC 수신 없음 → publish~LoRa 구간 문제 |

---

## 미구현/미검증 항목

| 항목 | 비고 |
|---|---|
| `mavlink_bridge_app` unit test | unit-test 디렉터리 미구성 |
| `uplink_app` CRC 검사 C 경로 | Python 테스트에서만 검증됨 |
| `uplink_app` viewpoint payload 상세 검증 | 파서는 있으나 테스트 없음 |
| `lora_fc_downlink_app` 실제 LoRa 송신 검증 | 하드웨어 필요 |
| §21.2 Config 명령 end-to-end | 설계 미완 |
