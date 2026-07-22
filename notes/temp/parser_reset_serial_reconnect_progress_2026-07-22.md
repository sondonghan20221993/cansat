# mavlink_bridge_app PARSER_RESET/SERIAL_RECONNECT 크로스앱 연결 — ✅ 완료 (2026-07-22)

## 배경

`ground_controllable_capability_plan_2026-07-21.md` P1-a. BL-09가 RESTART_BRIDGE/
UPLINK/LORA(앱 전체 재시작) 3종만 실제 연결하고, `PARSER_RESET`/
`SERIAL_RECONNECT`(앱은 유지한 채 mavlink_bridge_app 내부 함수만 재실행)
2종은 로그만 찍는 상태로 남겨뒀던 것을 마저 구현.

## 완료 — 받는 쪽 (mavlink_bridge_app)

- `config/default_mavlink_bridge_app_fcncode_values.h`: `PARSER_RESET_CC=3`,
  `SERIAL_RECONNECT_CC=4` 추가
- `config/default_mavlink_bridge_app_msgstruct.h`: `MAVLINK_BRIDGE_APP_ParserResetCmd_t`,
  `MAVLINK_BRIDGE_APP_SerialReconnectCmd_t` 추가(둘 다 `CommandHeader`만, payload 없음)
- `fsw/src/mavlink_bridge_app_utils.c`: `OpenSerial()` 정의 직후에
  `MAVLINK_BRIDGE_APP_ProcessParserResetCmd()`/`ProcessSerialReconnectCmd()`
  래퍼 추가 — 기존 static `ResetParser()`/`CloseSerial()`/`OpenSerial()`을
  그대로 호출, `CmdCounter++` + EVS 이벤트
- `fsw/src/mavlink_bridge_app_utils.h`: 위 2개 함수 프로토타입 추가
- `fsw/src/mavlink_bridge_app_dispatch.c`: `MAVLINK_BRIDGE_APP_CMD_MID`
  기존 switch에 두 case 추가
- `unit-test/stubs/mavlink_bridge_app_utils_stubs.c`: 두 함수 stub 추가
- `unit-test/coveragetest/coveragetest_mavlink_bridge_app_dispatch.c`:
  `Test_MAVLINK_BRIDGE_APP_TaskPipe_ParserReset`/`_SerialReconnect` 추가
- `unit-test/coveragetest/coveragetest_mavlink_bridge_app_utils.c`:
  `Test_ProcessParserResetCmd_IncrementsCmdCounter`/
  `Test_ProcessSerialReconnectCmd_ClosesFdAndIncrementsCmdCounter` 추가
  (Parser/SerialFd가 파일 static이라 직접 관측 불가 — `CmdCounter` 증가와
  크래시 없이 완주하는 것으로 검증, `OpenSerial()`은 테스트 환경에 실장치가
  없어 실패하는 게 정상이라 `SerialFd == -1` 유지 확인)

**아직 로컬 UT 재실행 안 함** — 위 stub/test 추가 후 빌드 검증 필요.

## 완료 — 보내는 쪽 (cfs_core_app)

**결정**: 신규 MID 신설 대신 `mavlink_bridge_app`의 기존 `CMD_MID`(`0x18A0`)를
FcnCode로 재사용(`PARSER_RESET_CC=3`/`SERIAL_RECONNECT_CC=4`) — 그 앱 자신이
이미 NOOP/RESET_COUNTERS/MISSION_QUERY를 같은 방식으로 구분하고 있어서
가장 일관됨. 이 저장소에 `CFE_MSG_SetFcnCode()` 사용 선례가 없었으나 표준
cFE API라 도입.

- `config/default_cfs_core_app_msgid_values.h`: `MAVLINK_BRIDGE_APP_CMD_MID_VALUE`(`0x18A0U`,
  mavlink_bridge_app 쪽 값과 반드시 동일 유지)와 두 FcnCode 재선언
- `fsw/src/cfs_core_app.h`: `CFS_CORE_APP_BridgeCtrlCmd_t`(CommandHeader만) +
  `Data.BridgeCtrlCmd` 필드 추가
- `fsw/src/cfs_core_app.c`: `Init()`에서 `CFE_MSG_Init()`으로 해당 MID에 고정
- `fsw/src/cfs_core_app_utils.c`: `CFS_CORE_APP_SendBridgeCtrlCmd(FcnCode)` 헬퍼
  추가(`CFE_MSG_SetFcnCode()` + `CFE_SB_TransmitMsg()`) — `ProcessRecoveryCommand()`의
  `PARSER_RESET`/`SERIAL_RECONNECT` case에서 호출, 반환값을 `Ok`(EXEC_RESULT
  근거)로 사용
- 유닛테스트: 성공 경로 2건(ParserReset/SerialReconnect, `CFE_MSG_SetFcnCode`
  호출 확인 + EXEC_RESULT OK), 실패 경로 1건(SetFcnCode 실패 → EXEC_RESULT FAILED)
- 문서 갱신: `cfs_core_app_behavior_spec.md`, `ground_controllable_capability_plan_2026-07-21.md`,
  openMCT `uplinkCLI/plugin.js` help 텍스트("로그만" 문구 제거)
- 회귀 UT: cfs_core_app 4종(19/7/277/35), mavlink_bridge_app 4종(14/4/170/34) 전부 PASS

### (참고, 해소됨) 설계 이슈였던 부분

`cfs_core_app`이 mavlink_bridge_app의 `CMD_MID`로 보낼 메시지를 어떻게
구성할지 확정 안 됨:

- mavlink_bridge_app의 `CMD_MID`는 이미 자기 내부에서 5개 명령(NOOP/
  RESET_COUNTERS/MISSION_QUERY/PARSER_RESET/SERIAL_RECONNECT)을 **같은 MID +
  다른 FcnCode**로 구분하는 기존 패턴을 씀
- `cfs_core_app`이 이 MID로 보내려면 같은 방식(MID 고정, FcnCode만 다르게
  설정)을 써야 하는데, **이 저장소 전체에서 `CFE_MSG_SetFcnCode()`를 쓴
  선례가 없음**(grep 0건) — 대신 기존 컨벤션은 "명령 종류마다 별도 MID를
  `CFE_MSG_Init()`으로 한 번 고정"하는 방식(예: CONFIG_CMD_MID, RECOVERY_CMD_MID
  전부 각자 자기 MID 하나씩)
- 즉 두 갈래 선택지:
  1. **`CFE_MSG_SetFcnCode()` 사용** — mavlink_bridge_app의 기존 CMD_MID
     그대로 재사용, 전송 직전 FcnCode만 바꿔서 발행. 표준 cFE API이긴
     하나 이 저장소에 선례가 없어 처음 도입하는 패턴이 됨
  2. **새 MID 2개 신설** — `PARSER_RESET_CMD_MID`/`SERIAL_RECONNECT_CMD_MID`를
     따로 만들어 기존 컨벤션(명령마다 자기 MID)과 일관되게 유지, 대신
     mavlink_bridge_app이 구독할 MID가 2개 늘어남

이 저장소의 기존 관례상 2번이 더 일관되지만, mavlink_bridge_app 자체가
이미 자기 CMD_MID 안에서 FcnCode로 여러 명령을 구분하는 방식을 쓰고
있어서(NOOP/RESET_COUNTERS/MISSION_QUERY도 전부 그 안에 있음) 1번이
mavlink_bridge_app 관점에서는 더 자연스러움 — **사용자 결정 필요**.

## 상태

- [x] 받는 쪽(mavlink_bridge_app) 구현 + 로컬 UT 회귀
- [x] 보내는 쪽(cfs_core_app) 설계 결정 + 구현 + 유닛테스트
- [x] 문서(spec/plan/uplinkCLI help) 갱신
- [ ] Pi 배포(전원 꺼짐, 재개 대기 — 다른 미배포 변경사항과 함께 처리)
