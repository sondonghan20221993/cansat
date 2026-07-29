# cfs_core_app — 명령 실행 구현 현황 (2026-07-22 갱신, BL-07)

> **이 문서는 낡았었음**: MODE 상태 전이와 RECOVERY 필드 검증/구분 처리는
> 이후 세션(BL-09, 2026-07-21 / mode 전이는 그 이전)에 실제로 구현됨.
> 아래는 2026-07-22 기준 재확인 결과로 갱신.

## 배경

`cfs_core_app_behavior_spec.md` §20 "알려진 미구현 항목" 참조.

## 명령별 구현 현황

### 1. VIEWPOINT_CMD (0x190D)
- **구현**: 페이로드(type/frame/X/Y/Z/Yaw/Pitch/HoldTime) → `ViewpointCmd` 캐시 저장, `VIEWPOINT_EID` 발생
- **미구현(방향 확정)**: 실제 viewpoint 위치 설정/전환 로직은 짐벌 미탑재로 범위 제외 확정(BL-10)
- **BL-82 완료(2026-07-29)**: `ProcessViewpointCommand()` 끝에 `CFS_CORE_APP_PublishExecResult(SourceSequence, 3U, false, ViewpointType)` 추가 — `CommandClass=3`(`UPLINK_APP_CLASS_VIEWPOINT`), 명시적 FAILED 회신으로 uplink_app의 무한 ROUTED 대기 해소

### 2. RECOVERY_CMD (0x190C) — ✅ 구현 완료 (BL-09, 2026-07-21)
- `cfs_core_app_utils.c:772` `CFS_CORE_APP_ProcessRecoveryCommand()`가 `RecoveryAction`
  switch로 실제 구분 처리: `RESTART_BRIDGE`/`RESTART_UPLINK`/`RESTART_LORA` 3종은
  `CFE_ES_RestartApp()` 실제 호출로 연결(AppId 조회 실패 시 실패 처리).
  `PARSER_RESET`/`SERIAL_RECONNECT`는 여전히 로그만(mavlink_bridge_app 프로세스
  내부 함수라 크로스앱 CMD_MID 신설 필요 — 별도 미착수 항목).
- BL-08(2026-07-22)로 처리 결과가 `EXEC_RESULT_MID`를 통해 uplink_app에도 회신됨.

### 3. MODE_CMD (0x190F) — ✅ 구현 완료
- `cfs_core_app_utils.c:883` `CFS_CORE_APP_ProcessModeCommand()`가 실제 상태 전이
  로직 수행: `ENTER→RECOVERY`(현재 NORMAL일 때만), `EXIT→NORMAL`(현재 RECOVERY일
  때만) 허용, 그 외 조합은 `TransitionAllowed=false`로 거부 이벤트(`MODE_CMD_EID`,
  ERROR) 발생. 캐시(`LastModeRequestToken`)뿐 아니라 `CurrentModeState`가 실제로
  갱신됨.
- **BL-81 완료(2026-07-29)**: `ProcessModeCommand()` 끝에
  `CFS_CORE_APP_PublishExecResult(SourceSequence, 5U, TransitionAllowed, RequestedState)`
  추가(`CommandClass=5`=`UPLINK_APP_CLASS_MODE`), 거부 분기에 `ErrCounter++` 추가.
  단, `CurrentModeState`가 다른 로직에서 실제로 읽혀 게이팅에 반영되는지는 이번
  스코프 밖 — EXEC_RESULT 회신으로 uplink_app의 dead-end만 해소.

### 4. 타임스탐프 유효성 (§7)
- **구현**: 미래 타임스탐프 거부(`Msg->TimestampMs > NowMs + 5000ms`)
- **구현 완료(BL-42, 2026-07-24)**: 만료 판정을 Pi 도착시각(`ArrivalMs`) 기준으로 전환 + FC 재부팅(TimestampMs 역행) 감지(`TIMEBASE_SHIFT_EID`/`TimebaseShiftCount`). cfs_core spec §7 참조

## 상태

- [x] 명령 페이로드 파싱 (cached)
- [ ] VIEWPOINT 실행 로직 (짐벌 미탑재로 범위 제외 확정, BL-10)
- [x] VIEWPOINT EXEC_RESULT FAILED 회신 (BL-82 완료 2026-07-29)
- [x] RECOVERY 필드 검증 및 구분 처리 (BL-09 완료)
- [x] MODE 상태 전이 검증 (구현 완료 확인)
- [x] MODE EXEC_RESULT 회신 (BL-81 완료 2026-07-29)
- [x] 타임스탐프 time base 검증 (**BL-42 완료** 2026-07-24 — 도착시각 만료 + FC 재부팅 감지)

## 참고

- `cfs_core_app/fsw/src/cfs_core_app_utils.c` (명령 처리 로직 — RECOVERY:772, MODE:883)
- `cfs_core_app/fsw/src/cfs_core_app_cmds.c` (명령 핸들러)
- `cfs_core_app/fsw/src/cfs_core_app.h` (상태 구조체)
