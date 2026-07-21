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

## 구현 목록 (우선순위순)

### P0 — Finding 1: EXECUTED 상태 왕복 채널
- [ ] `uplink_app`에 새 결과코드 `UPLINK_APP_RESULT_EXECUTED` (가칭) 추가
- [ ] 대상 앱(cfs_core_app 등)이 명령 처리 완료 후 `uplink_app`에 실행결과
      회신하는 MID/메커니즘 설계 (현재 `UPLINK_STATUS_MID`는 uplink_app→
      lora_tdm_app 단방향, 대상앱→uplink_app 방향 채널이 없음 — 신규 설계 필요)
- [ ] `mission_app_runtime_spec.md` §18.4.6.4 스펙과 실제 구현 정합화

### P1 — Finding 2: cfs_core_app RECOVERY → mavlink_bridge_app 크로스앱 연결
- [ ] `mavlink_bridge_app`에 새 CMD_MID 추가: `PARSER_RESET_CMD`,
      `SERIAL_RECONNECT_CMD` (또는 기존 RESET 함수코드 재사용 검토)
- [ ] `cfs_core_app_utils.c`의 `ProcessRecoveryCommand()`에서
      `PARSER_RESET`/`SERIAL_RECONNECT` 액션 시 위 MID를 실제로 publish
- [ ] `RESTART_BRIDGE`는 기존 `CFE_ES_RestartApp()` 경로 그대로 재사용,
      ProcessRecoveryCommand에서 직접 호출하도록 연결
- [ ] 회귀 테스트: 3개 액션 각각 실제 효과 발생 검증

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
