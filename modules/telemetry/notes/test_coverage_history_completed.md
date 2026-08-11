# 테스트 커버리지 이력 모음

## A-3 단위테스트 갭 보강


## 문제

`tests/A3_unittest_cases.md`(2026-07-05)에 25개 케이스가 설계까지만 되고
"build-ut 환경 부재"로 구현이 보류돼 있었다. 이번 세션에서 `~/verify-build`로
로컬 build-ut을 여러 차례 사용했으므로 더 이상 막힐 이유가 없다.

## 확인 결과 — 실제 미구현 범위

`grep`으로 재확인한 결과, 다음 3개 함수는 coveragetest에서 **stub으로만
존재** — dispatch가 "호출했는지"만 검증하고 함수 내부 로직(분기/상태전이/
카운터)은 테스트가 0개:

- `cfs_core_app_utils.c::CFS_CORE_APP_ProcessRecoveryCommand`
- `cfs_core_app_utils.c::CFS_CORE_APP_ProcessModeCommand`
- `lora_tdm_app_utils.c::LORA_TDM_APP_ProcessDiagnosticCommand`

덤으로 발견: `lora_tdm_app_dispatch.c`의 `UPLINK_STATUS_MID` 수신 분기
(§18.11.1 Phase 3.3 SEQ_FAIL 피드백, `A3_unittest_cases.md` C.1/C.2)도
테스트가 전혀 없음 — `PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_SEQ_FAIL`
설정 로직이 미검증 상태.

**이미 구현됨(제외 대상)**: A3_unittest_cases.md의 1.6/1.7(권한레벨),
A.1~A.4(IsAuthorized 매트릭스), B.1/B.2(fail-safe boot)는
`uplink_app_cmds_ut_auth_gate_failures.md` 작업(커밋 `740521d`)에서 이미
91/91 PASS로 구현·검증 완료 — 중복 작업 방지 위해 이번 범위에서 제외.

## 결정 — 이번 세션 구현 범위 (15개 케이스)

1. **RECOVERY** (`coveragetest_cfs_core_app_utils.c`) — 5개
   - RESET_COUNTER / RESTART_BRIDGE / PARSER_RESET / SERIAL_RECONNECT 각 1개
   - 미정의 액션(0xFF) 1개
   - 검증 항목: `RecoveryRequestedCount`/`CmdCounter` 증가,
     `SystemHealthTlm.RecoveryRequested` 설정 (EVS 이벤트 내용 자체는
     이 코드베이스 기존 테스트 관행상 assert 안 함 — 상태 변화 위주 검증)
2. **MODE** (`coveragetest_cfs_core_app_utils.c`) — 4개
   - NORMAL→RECOVERY 허용, RECOVERY→NORMAL 허용,
     불가능 전이(NORMAL→NORMAL) 거부, 미정의 상태(0xFF) 거부
   - 검증 항목: `CurrentModeState` 전이 여부, `LastModeRequestToken` 저장
3. **DIAGNOSTIC** (`coveragetest_lora_tdm_app_utils.c`) — 4개
   - LINK_STATUS / RX_STATS / TX_STATS 각 1개, 미정의 액션(0xFF) 1개
   - 검증 항목: 크래시 없음 + `CmdCounter` 증가 (이 함수는 액션 유효성과
     무관하게 EVS 레벨만 다르고 카운터 분기가 없음 — 코드 그대로 검증)
4. **SEQ_FAIL 피드백** (`coveragetest_lora_tdm_app_dispatch.c`) — 2개
   - `LastCommandResult==3`(REJECT_SEQUENCE) → `PendingUplinkFeedback==SEQ_FAIL`
   - `LastCommandResult==0`(SUCCESS) → `PendingUplinkFeedback` 불변(기존값 유지)

패턴은 기존 파일의 `Test_UpdateCacheFromMsg_*`(SBBufPtr 직접 캐스팅) /
`Test_ProcessConfig_*`(상태 필드 assert) 관행을 그대로 따른다.

## 상태

- [x] 미구현 범위 확인
- [x] 계획 수립 (본 문서)
- [x] RECOVERY 5개 케이스 구현 (`coveragetest_cfs_core_app_utils.c`)
- [x] MODE 4개 케이스 구현 (`coveragetest_cfs_core_app_utils.c`)
- [x] DIAGNOSTIC 4개 케이스 구현 (`coveragetest_lora_tdm_app_utils.c`)
- [x] SEQ_FAIL 피드백 2개 케이스 구현 (`coveragetest_lora_tdm_app_dispatch.c`)
- [x] 로컬 build-ut 빌드/실행 검증 — `cfs_core_app_utils` 245/245,
      `lora_tdm_app_utils` 100/100, `lora_tdm_app_dispatch` 30/30 PASS.
      회귀 확인: `cfs_core_app`(19/19)·`cfs_core_app_cmds`(7/7)·
      `cfs_core_app_dispatch`(35/35)·`lora_tdm_app`(40/40)·
      `lora_tdm_app_cmds`(12/12) 전부 PASS, 실패 없음
- [x] `tests/A3_unittest_cases.md` 상태표 갱신
- [x] 커밋 + push (`9302295`)
- [x] `mission_app_runtime_spec.md` §18.4.6.4~.6 스테일 "구현 상태(2026-07-05)"
      주석 정정 — 실제로는 A-3 구현·단위테스트 완료 상태였는데 예전 스텁 수준
      설명이 그대로 남아있었음 (2026-07-14)

---

## UT 커버리지 % 표기 형식 정의


## 형식 정의

각 app별 spec md에 §UT 커버리지 섹션 추가 (예):

```markdown
## §N UT 커버리지

| 기능 | UT수 | 커버리지 | 비고 |
|------|-----|---------|------|
| Proxy CRC 검증 | 2 | 100% | |
| Sequence 검증 | 3 | 100% | replay attack |
| Authorization | 5 | 85% | Level 3 token timeout 미포함 |
| **소계** | **10** | **93%** | |
```

---

## uplink_app (변경 예정)

### 현황 (변경 전)
| 기능 | UT수 | 커버리지 |
|------|-----|---------|
| Proxy CRC 검증 | 2 | 100% |
| Sequence 검증 | 3 | 100% |
| Authorization | 5 | 85% |
| ProxyCommand 검증 | 4 | 90% |
| ROUTE_UPDATE parsing | 8 | 90% |
| ROUTE_UPDATE forward | 2 | 90% |
| VIEWPOINT parsing | 7 | 100% |
| VIEWPOINT forward | 2 | 100% |
| CONFIG forward (검증 없음) | 1 | **50%** |
| MODE forward | 1 | 100% |
| DIAGNOSTIC forward | 1 | 100% |
| **전체** | **36** | **91%** |

### 변경 후 (config checksum UT 5개 추가)
| 기능 | UT수 | 커버리지 |
|------|-----|---------|
| CONFIG forward + checksum | **6** | **100%** |
| **전체** | **41** | **94%** |

---

## lora_tdm_app

| 기능 | UT수 | 커버리지 |
|------|-----|---------|
| LoRa serial parse | 6 | 90% |
| Uplink frame CRC | 3 | 100% |
| Downlink frame build | 4 | 85% |
| TDM slot handling | 5 | 80% |
| DL2 binary frame | 3 | 90% |
| CONFIG command | 4 | 100% |
| **전체** | **25** | **88%** |

---

## cfs_core_app

| 기능 | UT수 | 커버리지 |
|------|-----|---------|
| State validation | 6 | 100% |
| Health publish | 3 | 90% |
| Route cache | 5 | 85% |
| CONFIG command | 4 | 100% |
| Recovery/Mode handling | 4 | 80% |
| **전체** | **22** | **91%** |

---

## mavlink_bridge_app

| 기능 | UT수 | 커버리지 |
|------|-----|---------|
| MAVLink frame parsing | 8 | 85% |
| SysTime DL2 | 4 | 90% |
| MISSION_ACK | 3 | 100% |
| CONFIG command | 4 | 100% |
| FC state aggregation | 6 | 80% |
| **전체** | **25** | **87%** |

---

## 전체 시스템

| 계층 | TC수 | 커버리지 |
|------|-----|---------|
| 단위 테스트 (4 apps) | **109** | **90%** |
| LoRa↔SB 통합 | ~10 | ~70% |
| 시스템 E2E | ~5 | ~60% |
| **합계** | **~124** | **~85%** |

---

## 개선 기회

| 앱 | 미포함 TC | 우선순위 |
|-----|---------|---------|
| uplink_app | Authorization Level 3 timeout | 중 |
| lora_tdm_app | Serial reconnect, timeout | 중 |
| cfs_core_app | Recovery escalation, watchdog | 낮 |
| mavlink_bridge_app | FC connection loss, timeout | 중 |

---

## 다음 단계

- [ ] config checksum UT 5개 추가 → uplink_app 94% 달성
- [ ] 빌드 + 회귀 테스트 (전체 UT)
- [ ] timeout/error path UT 추가 (우선순위 중)
