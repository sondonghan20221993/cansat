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
- Python pytest + UDP mock 기반
- 실제 serial 없이 검증 가능한 범위까지

---

## Unit Test 현황 요약

| 앱 | 테스트 수 | assertion 수 | 마지막 확인 |
|---|---|---|---|
| `cfs_core_app` | 21 | 67 | 2026-05-28 |
| `uplink_app` | 35 | 63+ | 2026-06-07 |
| `lora_fc_downlink_app` | 14 | 40+ | 2026-06-07 |
| `mavlink_bridge_app` | 25 | — | 2026-06-02 |

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
| `UPLINK_APP_ForwardViewpointCommand` | viewpoint 명령 SB publish 성공/실패/zero-payload 경로 |

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
| `LORA_FC_DOWNLINK_APP_ProcessInputMessage` | attitude(roll/pitch/yaw float 캐시), local(x/y/z/vx/vy/vz float 캐시), gps(LatE7/LonE7/AltMm/FixType), ekf, system health(HealthState/FaultCode) 입력별 캐시 갱신 확인 |
| `LORA_FC_DOWNLINK_APP_ProcessInputMessage_InvalidInputs` | attitude/local Valid=0 → AttitudeValid/LocalValid=0 반영 (LORA-FC-005/006) |
| `LORA_FC_DOWNLINK_APP_ProcessInputMessage_GpsEdgeCases` | GPS Valid=0 → GpsValid=0; Valid=1+FixType=0 → GpsFixType=0 캐시 (LORA-FC-007) |

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
| `test_process_line_forwards_valid_frame_once` | 정상 프레임 → UDP 전송 1회, accept count 증가 |
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

### FC 경로 업로드 시험 (mavlink_bridge_app_behavior_spec.md §5–§9)

도구: `tools/uplink_route_update_sender.py` → cFS → FC 직렬

| ID | 시험 항목 | 검증 내용 | 기대 결과 |
|---|---|---|---|
| MAV-UP-001 | FC heartbeat 수신 전 route update | FC 링크 미연결 상태에서 업로드 시도 | EVS: `route update ignored - FC link not connected` |
| MAV-UP-002 | FC heartbeat 후 route update | `StartMissionUpload` 진단 로그 확인 | EVS: `StartMissionUpload called wp=N link=1`, HK `LastUploadResult==1` |
| MAV-UP-003 | FC가 `MISSION_REQUEST_INT` 미응답 | timeout 3회 재시도 후 실패 | EVS: `mission upload failed after 3 retries`, HK `LastUploadResult==2` |
| MAV-UP-004 | FC가 `MISSION_ACK result != ACCEPTED` | NAK 수신 시 즉시 실패, 재시도 없음 | EVS: upload fail, HK `LastUploadResult==3` |
| MAV-UP-005 | 업로드 후 `MISSION_QUERY_CC` 실행 | FC 저장 waypoint 조회 | EVS: `[wp 0] x=... y=... z=...` — 업로드 값과 일치 |
| MAV-UP-006 | `MAV_FRAME_LOCAL_NED` frame 거부 | FC가 `MAV_MISSION_UNSUPPORTED_FRAME` 반환 | EVS: upload fail, `LastUploadResult==3`, result 값 기록 확인 |
| MAV-UP-007 | z 좌표 부호 반전 확인 | route payload Z=5.0 업로드 후 MISSION_QUERY_CC | FC 저장 z=-5.0 (LOCAL_NED down) 확인 |

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

### UDP-* (UDP 전송)

| TC ID | 항목 | 분류 | 파일 |
|---|---|---|---|
| UDP-001~005 | UDP 전송 경로, host/port 변경 | 통합 | `test_lora_uplink_bridge.py` (기존 확장) |

---

### MAV-* (MAVLink 수신/캐시)

| TC ID | 항목 | 분류 | 비고 |
|---|---|---|---|
| MAV-001~009 | HEARTBEAT/ATTITUDE/GPS/EKF 캐시, serial 오류 | 런타임 | `mavlink_bridge_app` unit-test 미구성, 하드웨어 필요 |

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
| `test_lora_uplink_bridge.py` | LORA-UP-014, UDP 일부 | ✓ 구현 |
| `test_uplink_route_update_sender.py` | route update sender 전송 경로 | ✓ 구현 |
| `test_uplink_lora_frame.py` | LORA-UP-003~011, CFS-CMD-001~008, REC-006~008 | ✓ 구현 |
| `test_lora_fc_downlink_packet.py` | LORA-FRAME-001~008 | ✓ 구현 |
| `test_hb_parse.py` | LORA-HB-001~010, REC-005 | ✓ 구현 |
| `test_uplink_config_sender.py` | CONFIG payload/checksum/LoRa 프레임 빌드 (12개 테스트) | ✓ 구현 |
| `test_mission_upload_diag.py` | MAVLink X.25 CRC, V2 프레임 빌더, ITEM_INT z부호반전, Parser 라운드트립 (25개) | ✓ 구현 |
| `test_tools_packet_builders.py` | CFS 커맨드 XOR 체크섬, CCSDS 주헤더 구조 (9개) | ✓ 구현 |
| `test_mavlink_uart_bridge.py` | describe_message 포맷, parse_args 기본값/커스텀 (10개) | ✓ 구현 |

### 그룹 B — cFS 실행 + mock (cFS 필요, PTY/UDP)

cFS 프로세스가 실행 중인 상태에서 UDP 또는 PTY mock serial로 입력을 넣고
EVS 로그/HK/serial 출력으로 결과를 검증한다.

| 파일 | 검증 TC | 방법 | 상태 |
|---|---|---|---|
| `test_uplink_e2e.py` | LORA-UP seq regression C 경로, REC-008 C 검증 | UDP → cFS → EVS 로그 확인 | 미구현 |
| `test_lora_fc_downlink_e2e.py` | LORA-FRAME C 실제 출력, LORA-FC-006~007 | SB mock → cFS → PTY serial 캡처 | 미구현 |
| `test_rec_serial.py` | REC-001~004 장애/복구 | PTY close/reopen 시뮬레이션 | 미구현 |

**B 그룹 실행 조건:**
- cFS 빌드 완료 (`build/` 또는 `cFS_clean/build/`)
- `pytest --cfs` 또는 별도 마커로 A와 분리 실행
- Pi 환경 또는 Linux native 실행 환경 필요

---

## 미구현/미검증 항목

| 항목 | 비고 |
|---|---|
| `uplink_app` LoRa serial read C 경로 단위테스트 | `ServiceLoRa()`/`ParseLoRaFrame()` static — 통합테스트로 대체 |
| `uplink_app` CRC16 C 구현 | `UPLINK_APP_CRC16()` static — `test_uplink_lora_frame.py`로 검증 예정 |
| `lora_fc_downlink_app` SB timeout alive (LORA-FC-008) | main loop — static 함수, 단위테스트 불가 |
| `lora_fc_downlink_app` LoRa 송신/수신 단위테스트 | static 함수 — `test_lora_fc_downlink_packet.py`로 검증 예정 |
| `lora_fc_downlink_app` HB 파싱 단위테스트 | static 함수 — `test_hb_parse.py`로 검증 예정 |
| `lora_fc_downlink_app` 실제 LoRa 하드웨어 검증 | Pi에서 실물 연결 필요 |
| B그룹 통합테스트 실행 | cFS + PTY 환경 필요 — 구조는 있음, 현재 pytest.skip() |
