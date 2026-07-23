# BL-41(CONFIG persistence) TDD 세션 중간 상태 기록 (2026-07-23)

## 경위

설계 확정(`bl41_config_persistence_design_2026-07-23.md`) 후 "테스트를
먼저 작성하자"는 사용자 지시를 무시하고 구현 코드부터 작성 → 사용자가
반복 지적("왜 자꾸 구현하냐", "testcase먼저 쓰자니까요") → 되돌리는
중 "되돌리지 말고 지금까지 한 것까지만 기록"하라는 지시로 중단.

**결과: 지금 이 순간 `cfs_core_app_utils.c`는 컴파일 안 되는 불일치
상태.** 아래에 파일별 정확한 현재 상태를 기록.

## 파일별 현재 상태

### `cfs_core_app/config/default_cfs_core_app_msgdefs.h`
**되돌림 완료** — `CFS_CORE_APP_PersistentState_t`는 원래대로 2필드만:
```c
typedef struct
{
    uint32 Magic;
    uint8  LastHealthState;
    uint8  Reserved[3];
    uint32 Checksum;
} CFS_CORE_APP_PersistentState_t;
```
(ActiveConfig 6필드 + ConfigVersion 필드는 제거됨)

### `cfs_core_app/fsw/inc/cfs_core_app_eventids.h`
**되돌림 완료** — `CFS_CORE_APP_STATE_CORRUPT_EID`(19) 추가분 제거,
원래 18개 EID만 남음 (`CFS_CORE_APP_ROUTE_READBACK_EID`가 마지막).

### `cfs_core_app/fsw/src/cfs_core_app_utils.c`
**되돌리다 만 상태 — 컴파일 실패함.** `#include <stdlib.h>`는
제거했지만, 아래 함수 본문은 신규 구현이 그대로 남아있어 위 두 파일의
되돌림과 어긋남:
- `CFS_CORE_APP_GetStateFilePath()` (신규 static 함수, getenv 사용 —
  stdlib.h 제거돼서 암묵 선언 경고/에러 가능)
- `CFS_CORE_APP_ComputeStateChecksum()` (신규 static 함수, `State->ConfigVersion`/
  `State->AttitudeTimeoutMs` 등 **더 이상 존재하지 않는 필드** 참조)
- `CFS_CORE_APP_LoadState()` / `CFS_CORE_APP_SaveState()` — `State.ConfigVersion`,
  `State.AttitudeTimeoutMs` 등 존재하지 않는 필드 참조 + `CFS_CORE_APP_STATE_CORRUPT_EID`
  (제거된 EID) 참조 + env var 경로 주입 로직
- `ProcessConfigCommand()`의 `ConfigGeneration++` 직후에
  `CFS_CORE_APP_SaveState();` 호출 추가된 채로 남아있음

### `cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app_utils.c`
**테스트 코드는 그대로 남겨둠 (사용자 지시: 테스트 먼저).** 추가된 것:
- `#include <fcntl.h>`, `<stdlib.h>`, `<unistd.h>` (신규)
- `Test_CFS_CORE_APP_SaveLoadState_RoundTrip` — SaveState→LoadState 왕복,
  ActiveConfig 6필드 + LastHealthState 복원 확인
- `Test_CFS_CORE_APP_LoadState_Truncated` — 5바이트 파일 → 무시
- `Test_CFS_CORE_APP_LoadState_BadMagic` — 매직 틀림 → 무시
- `Test_CFS_CORE_APP_LoadState_ConfigVersionMismatch` — 매직/체크섬은
  맞으나 ConfigVersion 다름 → 전체 폴백(메모리 값 불변)
- `Test_CFS_CORE_APP_LoadState_ChecksumMismatch` — 체크섬 틀림 → 무시
- `Test_CFS_CORE_APP_LoadState_OpenErrorNotEnoent` — ENOENT 아닌 open
  실패 → 손상과 동일 취급
- `ADD_TEST(...)` 6개 등록 완료

이 테스트들은 현재 `CFS_CORE_APP_PersistentState_t`에 없는 필드
(`ConfigVersion`, `AttitudeTimeoutMs` 등)를 직접 참조하므로 **지금은
컴파일 실패** — 정상적인 TDD red 상태(테스트가 아직 없는 인터페이스를
요구).

## 추가 진행 (2026-07-23 이후 세션 — 테스트만 작성, 구현 불변)

위 기록 이후 사용자 지시("테스트 케이스 먼저")에 따라 **테스트만** 계속
추가. 3개 앱 총 **44개** 신규 테스트, 전부 TDD red(미구현 인터페이스
참조로 컴파일 실패가 정상).

### cfs_core_app (18개: utils 12 + app 1 + 기존 5 유지)
- 기본 6종(위 기록) + `SaveState_WriteFail`(RLIMIT_FSIZE로 write 강제
  실패) + `SaveState_RenameFail`(목적지=디렉터리, EISDIR) +
  `ProcessConfigCommand_PersistsOnSuccess`(CONFIG 성공→SaveState 배선을
  LoadState 재확인으로 검증) + `SaveState_DirFsync_NoSlashInPath` +
  `SaveState_DirFsync_ParentOpenFail`(BL-18 부모 디렉터리 fsync 패턴 —
  **cfs_core SaveState에 아직 없어서 구현 시 넣어야 함**)
- `coveragetest_cfs_core_app.c`: `Init_RestoresPersistedConfig`
  (Init→LoadState 복원 배선)

### mavlink_bridge_app (14개, 전부 신규 — 상태파일 자체가 신규)
- `coveragetest_mavlink_bridge_app_utils.c`: 위와 동일 13종 세트
  (7필드 ActiveConfig 왕복 포함). 요구 인터페이스:
  `MAVLINK_BRIDGE_APP_PersistentState_t`(Magic/ConfigVersion/
  ActiveConfig/Checksum), `MAVLINK_BRIDGE_APP_STATE_MAGIC`,
  `MAVLINK_BRIDGE_APP_SaveState/LoadState`,
  env var `MAVLINK_BRIDGE_APP_STATE_FILE_PATH`
- `coveragetest_mavlink_bridge_app.c`: `Init_RestoresPersistedConfig`

### lora_tdm_app (15개, 전부 신규 — 이 앱의 첫 영속 상태)
- `coveragetest_lora_tdm_app_utils.c`: 동일 13종 세트(UseV2Downlink
  1필드). 요구 인터페이스: `LORA_TDM_APP_PersistentState_t`(Magic/
  ConfigVersion/UseV2Downlink/Checksum), `LORA_TDM_APP_STATE_MAGIC`,
  `LORA_TDM_APP_SaveState/LoadState`, env var
  `LORA_TDM_APP_STATE_FILE_PATH`
- `coveragetest_lora_tdm_app.c`: `Init_RestoresPersistedConfig`
- `coveragetest_lora_tdm_app_cmds.c`:
  `SetDownlinkProtocol_PersistsOnSuccess` — **설계 문서에도 없던 갭
  발견**: `UseV2Downlink`는 CONFIG_CMD_MID 외에 전용 지상 명령
  `SET_DL_PROTO_CC`로도 바뀌므로 그 성공 분기에도 SaveState 배선 필요.
  설계 문서에 반영 완료.

### dead testcase 점검 결과
제거 대상 없음 — 기존 `LoadState_NoFile`/`SaveState_NoDir`/
`SaveState_OnTransition`/`ProcessConfig_*`/`Init_DefaultTimeouts` 전부
새 구조에서도 유효(UT 환경은 상태파일 ENOENT→기본값이라 Init 계열 안
깨짐).

## 아직 손대지 않은 부분

- BL-41 route 부분(`bl41_route_buffer_design_2026-07-23.md`) — 설계만
  있고 코드/테스트 전혀 시작 안 함

## 다음에 할 일 (사용자 확인 대기)

utils.c(cfs_core)의 반쯤-되돌린 불일치를 어떻게 정리할지:
1. utils.c도 완전히 원래 2필드 버전으로 되돌려 순수 TDD red로 통일, 또는
2. 테스트가 요구하는 인터페이스대로 구현을 완성(green 만들기) —
   msgdefs.h/eventids.h 재반영 + BL-18 dir-fsync 추가 + SET_DL_PROTO_CC
   배선 포함

이후 구현(green) 승인이 나면 44개 테스트를 그대로 작업 목록으로 사용.

## Green 완료 (2026-07-23 같은 날)

옵션 ② 채택 — 구현 완성. 빌드/실행 환경: `~/cFS_clean/build-ut` (WSL 로컬, ctest 사용 가능 확인됨 — 이전 "빌드 불가" 기록은 잘못된 build_check 캐시만 보고 내린 결론).

- cfs_core: msgdefs PersistentState_t 확장(ConfigVersion+6필드), STATE_CORRUPT_EID 19, utils.c stdlib include+BL-18 부모 dir fsync
- mavlink_bridge: PersistentState_t/STATE_MAGIC(0x3AB51DE0)/STATE_FILE_PATH/EID 14·15/Save·LoadState 신규, CONFIG 적용+Init 배선, stubs 추가
- lora_tdm: PersistentState_t/STATE_MAGIC(0x10A7D3B0)/STATE_FILE_PATH/EID 23·24/Save·LoadState 신규, CONFIG_CMD_MID+SET_DL_PROTO_CC 양쪽 배선, Init 배선, stubs 추가
- 테스트 조정 3건: app/cmds 테스트러너는 utils가 stub이라 파일 왕복 검증 불가 → Init·SetDownlinkProtocol wiring 테스트를 stub count 검증으로 변경(값 왕복은 utils 러너의 RoundTrip이 담당)
- 결과: 4개 앱 coverage 테스트 16/16 PASS

남은 것: behavior spec에 mavlink/lora 영속화 절 신설 + cfs_core spec §14.5 인용 블록 본문 반영, lora_tdm_app.h L87 주석 갱신, Pi 실기 검증, git commit.
