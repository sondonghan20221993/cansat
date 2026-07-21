# 명령 핸들러 "dead-end" 전수 점검 (2026-07-21)

## 배경

openMCT GUI로 `cfs_core_app`의 CONFIG 파라미터 목록을 보다가 "왜 조작 가능한 게
타임아웃 값밖에 없지?" → "앱 재시작/상태 조회 등도 지상에서 제어 가능해야
하는 것 아닌가?"라는 질문에서 출발. `RECOVERY_CMD_MID`의 `RESTART_BRIDGE`
액션을 확인한 결과 **EVS 이벤트 로그만 찍고 실제로 아무 것도 안 함**을
발견 — 이 패턴(명령은 도달하는데 실행이 없는 "dead-end 핸들러")이 다른
곳에도 더 있는지 4개 앱 전체(`cfs_core_app`/`mavlink_bridge_app`/
`lora_tdm_app`/`uplink_app`) 명령 핸들러를 전수 조사.

## 조사 결과 (심각도순)

### Finding 1 — spec/코드 불일치 (가장 심각)

- 위치: `notes/mission_app_runtime_spec.md:1358-1379` (§18.4.6.4 recovery command)
- spec 주장: "1362행: `UPLINK_STATUS_MID`는 `전달 성공`과 `실행 성공`을
  구분할 수 있어야 한다", "1371행: 구현 상태 ... ~~스텁 수준~~ 완료."
  (스텁 아님, **완료**로 명시)
- 실제 코드: `uplink_app/fsw/src/uplink_app_cmds.c:362`는
  `LastCommandResult = UPLINK_APP_RESULT_ROUTED`만 세팅 — "전달(ROUTED)"
  결과 코드만 있고 "실행(EXECUTED)" 결과 코드 자체가 `uplink_app.h`/
  `uplink_app_cmds.c` 어디에도 없음. 수신측 `cfs_core_app_utils.c:757-776`
  (RESTART_BRIDGE/PARSER_RESET/SERIAL_RECONNECT)도 `CFE_EVS_SendEvent()`만
  호출 — 상태 변경 없음.
- 문제: spec이 "스텁 수준"에서 "완료"로 격상 표기하며 "전달 vs 실행 구분"을
  구현됐다고 명시했는데, 실제로는 그 구분 자체가 코드에 존재하지 않음.
  운영자가 spec만 보면 실행 여부까지 추적된다고 오해할 수 있음.

### Finding 2 — 원래 발견한 패턴 (확인됨, 단 spec은 정직하게 명시)

- 위치: `cfs_core_app/fsw/src/cfs_core_app_utils.c:757-776`
- 코드: `RESTART_BRIDGE`/`PARSER_RESET`/`SERIAL_RECONNECT` 전부
  `CFE_EVS_SendEvent(...)` 후 `break` — `CFE_ES_RestartApp()` 없음,
  파서/시리얼 상태 변경 없음.
- 대조: 진짜 동작하는 `CFE_ES_RestartApp()` 호출 체인은
  `cfs_core_app_utils.c:286,366,398`에 있으나, 이건 **자체 헬스모니터
  타임아웃 로직(`CFS_CORE_APP_UpdateHealth`)에서만** 자동으로 트리거되고,
  지상 명령(`ProcessRecoveryCommand`)과는 완전히 분리돼 있음.
- spec: `notes/cfs_core_app_behavior_spec.md:628` — "RESTART_BRIDGE/
  PARSER_RESET/SERIAL_RECONNECT는 수신 로그만(실행 로직 미구현 — 로그
  전용)" → **이 문서는 정직하게 한계를 기재함, 불일치 없음.**
  `RESET_COUNTER`(748-750)만 실제로 `RecoveryStartMs`/`BridgeRestartCount`를
  리셋 — `mission_app_runtime_spec.md:455`("지상국 RECOVERY 명령으로
  재시도 횟수 초기화 가능 ★ 구현됨")과 일치.
- 문제: 운영 리스크는 원래 신고된 버그와 동일 — 지상 운영자가
  RESTART_BRIDGE/PARSER_RESET/SERIAL_RECONNECT를 보내면 INFO 이벤트와
  `RecoveryRequestedCount` 증가만 보고 "재시작/리셋됐다"고 오인할 수 있음.
  `cfs_core_app_behavior_spec.md`는 이 한계를 공개했지만, Finding 1의
  `mission_app_runtime_spec.md`가 그 공개 내용과 모순되게 "완료"라 씀.

### Finding 3 — 문서화된 미구현 (낮은 심각도, 불일치 없음)

- 위치: `cfs_core_app/fsw/src/cfs_core_app_utils.c:662-666`
  (`CFS_CORE_APP_ProcessViewpointCommand`)
- 코드: 필드를 `ViewpointCmd` 구조체에 캐시, `Valid=true`, `VIEWPOINT_EID`
  INFO 이벤트만 — route planner 연동이나 실제 FC 명령 발행 없음.
- spec: `notes/cfs_core_app_behavior_spec.md:72,627,698,709` 전부 "실제
  실행은 미구현(캐시만)"로 명시 — **코드와 정확히 일치.**
- 문제: 숨겨진 결함은 아니지만(정직하게 문서화됨), `VIEWPOINT_CMD_MID`는
  `mission_app_runtime_spec.md:952-954` 기준 Level-2 명령인데, 이
  runtime_spec 문서는 그 캐시-전용 한계를 behavior_spec만큼 눈에 띄게
  언급하지 않음 — 두 spec 문서 간 강조 수준 불일치.

### 문제없음으로 확인된 부분

- `mavlink_bridge_app/fsw/src/mavlink_bridge_app_cmds.c`: NOOP/RESET
  핸들러만 존재, RECOVERY/MODE/DIAGNOSTIC 클래스 자체가 라우팅 대상이
  아님 — 이 패턴이 애초에 적용 불가.
- `lora_tdm_app/fsw/src/lora_tdm_app_utils.c:803-844`
  (`LORA_TDM_APP_ProcessDiagnosticCommand`): 각 case가 실제 내부
  카운터(`LinkState`, `NoAckCount`, `RxCmdCount`, `TxCount`)를 EVS로
  보고 — 상태변경형 명령이 아니라 조회(read-only) 명령이라 "dead-end"
  범주에 안 들어감.
- `CFS_CORE_APP_ProcessModeCommand`(`cfs_core_app_utils.c:786-829`):
  허용된 전이에서 실제로 `CurrentModeState`를 변경 — spec
  (`cfs_core_app_behavior_spec.md:629`)과 일치, 정상.

## 다음 액션 (미착수)

- [ ] Finding 1: `mission_app_runtime_spec.md` §18.4.6.4의 "완료" 표기를
      실제 구현 상태(ROUTED만 있고 EXECUTED 없음)에 맞게 정정할지,
      아니면 EXECUTED 결과 코드를 실제로 구현할지 결정 필요
      (spec 정정만 할지 vs 기능 자체를 완성할지 — 사용자 판단 필요)
- [ ] Finding 2: RESTART_BRIDGE/PARSER_RESET/SERIAL_RECONNECT 액션을
      실제로 구현할지(예: RESTART_BRIDGE → `CFE_ES_RestartApp()` 연결),
      아니면 "이 3개는 의도적으로 미구현"이라고 명시적으로 확정할지 결정
- [ ] Finding 3: 두 spec 문서(`mission_app_runtime_spec.md` vs
      `cfs_core_app_behavior_spec.md`) 간 VIEWPOINT 캐시-전용 한계
      언급 수준 통일

## 관련
- `notes/mission_app_runtime_spec.md` §18.4.6.4
- `notes/cfs_core_app_behavior_spec.md` §(RECOVERY/VIEWPOINT 섹션)
- `cfs_core_app/fsw/src/cfs_core_app_utils.c:740-829` (RECOVERY/MODE 핸들러)
- `uplink_app/fsw/src/uplink_app_cmds.c:362` (LastCommandResult=ROUTED만 존재)
