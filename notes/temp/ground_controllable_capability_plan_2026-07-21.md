# 지상 제어 가능 기능 확장 계획 (2026-07-21)

## 배경

`command_dead_end_audit_2026-07-21.md`에서 발견한 3개 항목에 대한 방향 결정:

1. **Finding 1(EXECUTED 결과코드 부재)** — spec 정정이 아니라 **실제 구현
   대상**으로 확정. `mission_app_runtime_spec.md`가 "완료"라 쓴 대로
   실제로 "전달 성공 vs 실행 성공"을 구분할 수 있게 만든다.
2. **Finding 2(RESTART_BRIDGE 등 미연결)** — "재시작"만이 아니라, **각 앱이
   내부적으로 이미 갖고 있는 기능 중 지상에서 트리거 못 하는 것 전체를
   먼저 파악**하고, 그걸 기준으로 구현 목록을 만든다. RESTART_BRIDGE는
   그 목록의 항목 하나일 뿐.
3. **Finding 3(VIEWPOINT 캐시 전용)** — 위 목록 작업과 함께 다시 정리.

## 1. cfs_core_app 기능 인벤토리

| 기능 | 현재 트리거 | 지상 명령으로 가능? |
|---|---|---|
| 헬스 판정(NOMINAL/DEGRADED/RECOVERY/FAILED) | 자동(입력 staleness) | 아니오 — 판정 로직 자체는 트리거 대상이 아님(설계상 의도, 안전) |
| 헬스 임계값(timeout/publish_period) 변경 | CONFIG_CMD_MID | ✅ 이미 가능 |
| bridge 재시작(`CFE_ES_RestartApp` on mav_bridge_app) | 자동(bridge timeout)만 | ❌ RECOVERY_CMD_MID(RESTART_BRIDGE) 도달은 하나 미연결 |
| 복구 재시도 카운터 리셋 | RECOVERY_CMD_MID(RESET_COUNTER) | ✅ 이미 가능 |
| MODE 전이(NORMAL↔RECOVERY) | MODE_CMD_MID | ✅ 이미 가능 |
| VIEWPOINT 캐시 저장 | VIEWPOINT_CMD_MID | 부분적 — 캐시만, 실제 활용처(경로계획 등) 없음 |
| 헬스 상태 파일 영속화/복원 | 자동(재시작 시) | 대상 아님(재시작 트리거의 부수효과) |
| `PARSER_RESET` (명령은 존재) | 없음 | ❌ cfs_core_app 자체엔 파서가 없음 — mavlink_bridge_app의 파서를 가리키는 것으로 추정, 크로스앱 명령 채널 없음 |
| `SERIAL_RECONNECT` (명령은 존재) | 없음 | ❌ 마찬가지로 mavlink_bridge_app(또는 lora_tdm_app) 시리얼을 가리키는 듯, 크로스앱 채널 없음 |

## 2. mavlink_bridge_app 내부 기능 중 미노출

`mavlink_bridge_app_utils.c`에 이미 구현된 내부(static) 함수들이지만
자동 트리거만 있고 지상/타앱에서 호출할 방법이 없음:

| 함수 | 현재 트리거 | 비고 |
|---|---|---|
| `MAVLINK_BRIDGE_APP_ResetParser()` | 파싱 에러 발생 시 자동 | cfs_core_app의 `PARSER_RESET`이 가리키는 실제 대상으로 보임 |
| `MAVLINK_BRIDGE_APP_CloseSerial()`/`OpenSerial()` | 재연결 타이머(`ReconnectIntervalMs`) 자동 | cfs_core_app의 `SERIAL_RECONNECT`가 가리키는 실제 대상으로 보임 |

→ **cfs_core_app의 RECOVERY 명령 3종(RESTART_BRIDGE/PARSER_RESET/
SERIAL_RECONNECT)이 정확히 mavlink_bridge_app의 이 3개 자동복구
메커니즘을 지상에서 수동으로도 트리거하려는 의도였을 가능성이 높음** —
설계 의도와 배선 누락이 명확히 대응됨.

## 3. lora_tdm_app 내부 기능 중 미노출

| 함수 | 현재 트리거 | 비고 |
|---|---|---|
| `OpenSerial()`/`CloseSerial()` (LoRa 시리얼) | write 실패 시 자동 재오픈 | 자체 self-healing 있음 — 지상 수동 트리거 필요성은 낮음(참고용) |

## 4. uplink_app 기능 인벤토리

| 기능 | 현재 상태 |
|---|---|
| 명령 라우팅(6개 클래스 → 대상 앱) | ✅ 동작 |
| 권한 검증(Level 1/2/3) | ✅ 동작 |
| Fail-safe boot 차단 | ✅ 동작 |
| **실행 결과 확인(EXECUTED)** | ❌ ROUTED(전달)까지만, 대상 앱의 실제 처리 결과를 다시 받아오는 왕복 채널 없음 |

## 5. 체계적 전수조사 결과 (2026-07-21, 2차 — Explore 서브에이전트)

1~4절은 RECOVERY 명령 이름을 단서로 역추적한 반응적 조사였음. 이번엔
4개 앱의 `fsw/src/*.c` 전체에서 "실제 상태를 바꾸는 static 함수"를
기준으로 처음부터 훑음. 새로 확인된 사항만 추가 기록(1~4절과 중복되는
mavlink_bridge_app 파서/시리얼 건은 생략).

### 신규 발견 A — cfs_core_app의 앱 재시작이 bridge 외에도 있음

- `cfs_core_app.c:472-480` — bridge timeout 시 `CFE_ES_RestartApp(mav_bridge_app)`
- `cfs_core_app.c:551-560` — **uplink timeout 시 `CFE_ES_RestartApp(uplink_app)`** (자동 전용)
- `cfs_core_app.c:583-592` — **lora timeout 시 `CFE_ES_RestartApp(lora_tdm_app)`** (자동 전용)

→ 지금까지 "RESTART_BRIDGE"만 얘기했는데, 실제로는 **uplink_app/
lora_tdm_app도 동일하게 자동재시작 로직이 있고 지상에서 수동 트리거할
방법이 없음**. `RecoveryAction` enum에 `RESTART_UPLINK`/`RESTART_LORA`가
없음 — P1 작업 범위에 포함해야 함.

### 신규 발견 B — cfs_core_app 자체 상태 저장도 자동 전용

- `cfs_core_app_utils.c:704` `CFS_CORE_APP_SaveState()` — 헬스상태 파일
  영속화, 상태 전이 시에만 자동 호출. 지상에서 강제 저장할 필요성은
  낮음(참고용, P2 이하 우선순위로도 불필요할 가능성 큼).

### 신규 발견 C — mavlink_bridge_app의 stale 마킹도 자동 전용

- `mavlink_bridge_app_utils.c:857` `MarkOutputsStale()` — 자동 에러/타임아웃
  경로에서만 호출. 지상에서 강제로 "지금 다 stale 처리해" 할 이유가
  약함 — 참고용, 작업목록 제외 권장.

### 신규 발견 D — lora_tdm_app fcncode 정의 위치 불일치 (기능 갭 아님, 위생 문제)

- `LORA_TDM_APP_SET_DOWNLINK_PROTO_CC`(값 2)가 `fsw/inc/lora_tdm_app_fcncodes.h`에
  정의돼 있는데 `config/default_..._fcncode_values.h`엔 없음. 실제
  dispatch는 정상 동작(`lora_tdm_app_dispatch.c:54`)하므로 **기능 갭
  아님** — 정의 위치 중복/불일치라 정리 시 참고.

### 확인됨 — DEFINED-BUT-UNHANDLED 명령 enum

4개 앱 전체에서 **"정의는 됐는데 처리 코드가 아예 없는" 명령은 0건**.
전부 최소한 `default` 분기로는 걸림 — 이 종류의 갭은 없음.

### 확인됨 — uplink_app 자체엔 후보 갭 없음

`uplink_app`은 순수 포워더 구조(`Forward*` 함수들이 전부
`PROCESS_UPLINK_CC` 경로로만 호출됨)라 구조적으로 이 패턴이 발생할 수
없음 — 전수조사 결과도 0건 확인.

### 전수조사로 갱신된 P1 범위

기존 P1(mavlink_bridge_app 파서/시리얼 연결)에 **RESTART_UPLINK/
RESTART_LORA를 RecoveryAction enum에 추가하고 cfs_core_app에서
직접 `CFE_ES_RestartApp()` 연결**하는 작업을 포함해야 함 — 아래
구현목록에 반영.

## 구현 목록 (우선순위순)

### P0 — Finding 1: EXECUTED 상태 왕복 채널
- [ ] `uplink_app`에 새 결과코드 `UPLINK_APP_RESULT_EXECUTED` (가칭) 추가
- [ ] 대상 앱(cfs_core_app 등)이 명령 처리 완료 후 `uplink_app`에 실행결과
      회신하는 MID/메커니즘 설계 (현재 `UPLINK_STATUS_MID`는 uplink_app→
      lora_tdm_app 단방향, 대상앱→uplink_app 방향 채널이 없음 — 신규 설계 필요)
- [ ] `mission_app_runtime_spec.md` §18.4.6.4 스펙과 실제 구현 정합화

### P1 — Finding 2: cfs_core_app RECOVERY → 각 앱 크로스앱 연결
- [ ] `mavlink_bridge_app`에 새 CMD_MID 추가: `PARSER_RESET_CMD`,
      `SERIAL_RECONNECT_CMD` (또는 기존 RESET 함수코드 재사용 검토)
- [ ] `cfs_core_app_utils.c`의 `ProcessRecoveryCommand()`에서
      `PARSER_RESET`/`SERIAL_RECONNECT` 액션 시 위 MID를 실제로 publish
- [ ] `RESTART_BRIDGE`는 기존 `CFE_ES_RestartApp()` 경로 그대로 재사용,
      ProcessRecoveryCommand에서 직접 호출하도록 연결
- [ ] **(전수조사 신규)** `RecoveryAction` enum에 `RESTART_UPLINK`/
      `RESTART_LORA` 추가, `cfs_core_app.c:551-560`/`583-592`의
      기존 자동재시작 로직과 동일한 `CFE_ES_RestartApp()` 호출을
      지상 명령 경로에서도 사용하도록 연결
- [ ] 회귀 테스트: 5개 액션(RESTART_BRIDGE/UPLINK/LORA, PARSER_RESET,
      SERIAL_RECONNECT) 각각 실제 효과 발생 검증

### P2 — Finding 3: VIEWPOINT 캐시 활용처 확인
- [ ] VIEWPOINT_CMD_MID로 캐시된 값이 실제로 쓰이는 곳이 설계상
      있어야 하는지(예: 향후 경로계획 연동) 확인 — 없으면 spec에
      "저장만, 활용 미정" 명시로 충분

## 관련
- `notes/temp/command_dead_end_audit_2026-07-21.md`
- `notes/mission_app_runtime_spec.md` §18.4.6.4
- `notes/cfs_core_app_behavior_spec.md`
- `cfs_core_app/fsw/src/cfs_core_app_utils.c:740-829`
- `mavlink_bridge_app/fsw/src/mavlink_bridge_app_utils.c:149,237,865`
