# 추후 작업 의제 — 2026-07-05 갱신

> Phase 2.1 request_token 규약 확정 완료 (2026-07-05).
> Mode/Diagnostic 명령 실제 구현 계속 진행.

## 우선순위 (재설계 필요성 역순)

### Phase 1: A-3 완료 — ✅ **완료 2026-07-05**
**구조 영향**: ❌ 없음

#### 1.1 ~ 1.3: 명령 구현 ✅ 완료
- [x] **A-3.1 RECOVERY 명령 실제 구현** — ✅ 완료 (2026-07-05)
  - uplink_app_utils.c ForwardRecoveryCommand (payload 파싱)
  - cfs_core_app_utils.c ProcessRecoveryCommand (action 분기)
  - RecoveryCmdTlm_t 확장 (RecoveryAction/TargetComponent/ReasonCode/RequestToken)
  - CFS_CORE_APP_RecoveryAction_t enum (4개 action)

- [x] **A-3.2 MODE 명령 실제 구현** — ✅ 완료 (2026-07-05)
  - cfs_core_app_utils.c ProcessModeCommand (state transition logic)
  - Mode state machine (CFS_CORE_APP_ModeState_t enum: NORMAL/RECOVERY)
  - Allowed transitions: NORMAL↔RECOVERY via ENTER/EXIT actions
  - ModeCmdTlm_t 확장 (ModeAction/RequestedState/RequestToken)

- [x] **A-3.3 DIAGNOSTIC 명령 실제 구현** — ✅ 완료 (2026-07-05)
  - lora_tdm_app_utils.c ProcessDiagnosticCommand (payload 해석)
  - DiagnosticCmdTlm_t 확장 (DiagAction/DiagTarget/RequestToken)
  - LORA_TDM_APP_DiagnosticCmdTlm_t 정의
  - LORA_TDM_APP_DiagAction_t enum (3개: LINK_STATUS/RX_STATS/TX_STATS)

#### 1.4: 단위테스트 갱신 (선택사항, 미진행)
- [ ] **A-3 단위테스트 갱신** (선택사항, cFS build-ut 환경 필요)
  - RECOVERY action별 테스트 (4개): RESET_COUNTER, RESTART_BRIDGE, PARSER_RESET, SERIAL_RECONNECT
  - MODE state transition 테스트 (4개: ENTER_RECOVERY, EXIT_RECOVERY, INVALID_TRANSITION, ...)
  - DIAGNOSTIC action 테스트 (3개): LINK_STATUS, RX_STATS, TX_STATS
  - 추정 시간: 2-3시간 (cFS build-ut 환경 필요)
  - **결정**: 로컬 빌드 환경 없으므로 보류. 배포 후 통합테스트로 대체 가능.

---

### Phase 2: 구조 안정화 (부수 작업)

#### 2.1 request_token 규약 확정 — ✅ **완료 2026-07-05**
**구조 영향**: ✅ 재설계 방지됨

**결정**: **기본안** (지상국 채번 / 앱은 불투명 저장 / downlink echo / replay와 양립)

**완료 항목**:
- [x] 기본안으로 결정 (선택지 2, 3은 Phase 3.2/인증 계층에서 처리)
- [x] spec §18.4.7 "Request Token 계약" 작성 완료
  - 역할: 지상국이 생성한 불투명 요청-응답 상관 식별자
  - Replay 방어와의 관계: 독립적 (command sequence가 담당)
  - 인증과의 관계: 무관 (Phase 3.2에서 프레임 헤더 Flags로 처리)
  - 구현 요구사항: downlink 텔레메트리에 RequestToken 필드 echo
  - 파일: `/mnt/d/cfs-telemetry-app/notes/mission_app_runtime_spec.md` §18.4.7 추가

---

### Phase 3: 보안·신뢰성 강화 (독립적 또는 병렬 진행 가능)

#### 3.1 fail-open 부팅 구간 제거 (우선: 중)
**구조 영향**: ❌ 없음 — uplink_app_cmds.c 로직만

**현황**: ~~boot 직후 SYSTEM_HEALTH_MID 수신 전 health-block 미작동 (fail-open)~~
→ ✅ **완료** (하단 상태표 3.1 참조 — 이 절의 서술은 착수 전 기준, stale 정정 2026-07-23)

**개선**:
- Option A (즉시): boot → `CfsHealthReceived=0` (이미 그렇게 됨), 명시적으로 "첫 health까지 차단" 정책 추가
- Option B (향후): 기본 health 상태를 NOMINAL이 아닌 DEGRADED로 초기화

**추정 시간**: 30분

---

#### 3.2 권한 검증 추가 (우선: 상)
**구조 영향**: ⚠️ Flags 필드 또는 새 헤더 필드 (이미 Reserve 공간 있음)

**현황**: ~~spec §17.5의 권한 레벨 1~3이 미구현~~ → ✅ **완료** (§18.11로 구현,
2026-07-23 실기 검증까지 됨 — auth=0 차단→auth=2 통과 실측. 이 절의 서술은
착수 전 기준, stale 정정 2026-07-23). 구조 재검토(자기신고식 한계)는 BL-46.

**범위**:
- Flags 또는 보안 헤더에 권한 수준 인코딩 (or token 기반)
- uplink_app_cmds.c의 health-block 블록 바로 뒤에 권한 검증 추가
  ```c
  // health-block 이후
  if (!UPLINK_APP_IsAuthorized(Cmd->CommandClass, Cmd->Flags)) {
      // 거부
  }
  ```
- 권한별 클래스 매트릭스 (예: Level 1은 DIAGNOSTIC만, Level 3은 전부)
- spec 섹션 추가 (예: §18.11 "Authorization Policy")

**추정 시간**: 3-4시간 (구조 영향 작은 편)

---

#### 3.3 물리계층 신뢰성: SEQ_FAIL 피드백 (우선: 중)
**구조 영향**: ⚠️ UFB(Uplink Feedback Byte) enum 확장

**현황**: LoRa downlink의 피드백 바이트가 OK/CRC_FAIL만 전달
→ 지상국이 "시퀀스 거부" vs "패킷 유실" 구분 불가

**범위**:
- lora_tdm_app: UFB enum 에 `UFB_SEQ_FAIL(2)` 추가
- uplink_app: 시퀀스 거부(IsSequenceAccepted=false) 시 downlink UFB에 SEQ_FAIL 마크
- lora_tdm_app_utils.c ProcessUplinkCommand 수정
  - 시퀀스 거부 → UFB = SEQ_FAIL
  - 구현: 현재는 received 확인만, 거부 시 UFB 생성 필요
- spec 섹션 추가 (예: lora_tdm_app_behavior_spec.md §6.2 "UFB Codes")

**추정 시간**: 2-3시간

---

#### 3.4 테스트용 경로 보안화 (우선: 낮음, 정책)
**구조 영향**: ❌ 없음

**현황**: UDP:1234 CI_LAB 경로가 배포 startup에 병행. 테스트용이지만 무인증.

**선택지**:
1. 배포 startup.scr에서 CI_LAB 제거 (테스트 전용 빌드 분리)
2. CI_LAB도 권한 검증 대상 (3.2 완료 후)
3. localhost bind만 허용 (이미 그럼)

**추정 시간**: 0 (정책만, 코드 변경 최소)

---

## 완료 vs 남은 작업 요약

| Phase | 항목 | 상태 | 재설계? | 시간 | 차단 의존성 |
|---|---|---|---|---|---|
| **1.1** | RECOVERY 명령 | ✅ 완료 | ❌ | - | - |
| **1.2** | MODE 명령 | ✅ 완료 | ❌ | - | - |
| **1.3** | DIAGNOSTIC 명령 | ✅ 완료 | ❌ | - | - |
| **1.4** | A-3 테스트 | ⏸️ 보류 | ❌ | 2-3h | build-ut 환경 필요 |
| **2.1** | request_token 규약 | ✅ 완료 | ❌ 재설계 방지됨 | - | - |
| **3.1** | fail-open 제거 | ✅ 완료 | ❌ | - | - |
| **3.2** | 권한 검증 | ✅ 완료 | ⚠️ 소 | - | - |
| **3.3** | SEQ_FAIL 피드백 | ✅ 완료 | ⚠️ 중 | - | - |
| **3.4** | CI_LAB 정책 | ⏳ 미시작 | ❌ | 0 | 정책만 결정 |

---

## 완료 상태 (2026-07-05 Phase 3.2 완료)

### Phase 3.1 fail-open 제거 — ✅ 완료
- [x] 부팅 fail-safe 정책: health 미수신 상태에 모든 명령 차단
- [x] RECOVERY(2)/FAILED(3) 데드락 해제: RECOVERY/DIAGNOSTIC 허용
- [x] 코드 수정: `uplink_app_cmds.c` (라인 94 조건 반전 + fail-safe 블록)
- [x] spec 업데이트: §18.10.1 건강 상태 매트릭스

### Phase 3.2 권한 검증 추가 — ✅ 완료
- [x] **매트릭스 설계**: 명령 클래스 8개 × 권한 레벨 3단계 (§18.11.1)
- [x] **Flags 비트 정의**: Bits[7:6] = AUTH_LEVEL (이미 구조에 있음)
- [x] **코드 구현**:
  - `UPLINK_APP_GetClassRequiredLevel()`: 명령 클래스 → 요구 권한 레벨 매핑
  - `UPLINK_APP_IsAuthorized()`: 요청 권한 ≥ 요구 권한 검증 + Level 3 token!=0 검증
  - ProcessUplinkCommand 통합: health-block → [NEW: 권한 검증] → class 라우팅
- [x] **EVS 이벤트**: UPLINK_APP_AUTHZ_BLOCK_EID (ID=7) 추가
- [x] **spec 추가**: §18.11.1 "권한 검증 정책" (매트릭스, 규칙, 테스트 케이스)

**변경 파일**:
- `uplink_app_cmds.c`: 권한 검증 함수 + ProcessUplinkCommand 통합
- `uplink_app_eventids.h`: AUTHZ_BLOCK_EID 정의
- `mission_app_runtime_spec.md`: §18.4.3.1 Flags 정의, §18.11.1 권한 검증 정책

**단위테스트 미실행**: build-ut 환경 부재 → 배포 후 통합테스트 포함

---

## 완료 요약 (Phase 3 99% 완료)

### Phase 3.1 fail-open 제거 — ✅ 완료
- 부팅 fail-safe: health 미수신 시 모든 명령 차단
- RECOVERY/FAILED 데드락 해제
- Code: `uplink_app_cmds.c`, Spec: §18.10.1

### Phase 3.2 권한 검증 추가 — ✅ 완료  
- 명령 클래스별 권한 레벨 매트릭스 (3 levels × 8 classes)
- Flags[7:6] AUTH_LEVEL 인코딩 (이미 구조 존재)
- 권한 검증 함수 + 통합 (health-block → [권한검증] → routing)
- Code: `uplink_app_cmds.c` (IsAuthorized, GetClassRequiredLevel), Spec: §18.11.1

### Phase 3.3 SEQ_FAIL 피드백 — ✅ 완료
- UFB (Uplink Feedback Byte) 코드: OK(0) / CRC_FAIL(1) / SEQ_FAIL(2) ✅ 정의됨
- **구현**: lora_tdm_app이 UPLINK_STATUS_MID 구독 → LastCommandResult == REJECT_SEQUENCE 감지 → PendingUplinkFeedback = UFB_SEQ_FAIL 설정
- **신호 흐름**: uplink_app (시퀀스 거부) → UPLINK_STATUS_MID 발행 → lora_tdm_app (SEQ_FAIL 설정) → downlink TDM 패킷에 포함
- Code: 
  - `lora_tdm_app.c` (UPLINK_STATUS_MID 구독 추가)
  - `lora_tdm_app_dispatch.c` (UPLINK_STATUS 메시지 처리 추가)
  - `lora_tdm_app_topicid_values.h` (UPLINK_STATUS_MID_VALUE 정의 추가)
- Spec: `lora_tdm_app_behavior_spec.md` §9.2 "UFB Codes" 신설

---

## 남은 작업 (Phase 3.4만)

### Phase 3.4 CI_LAB 정책 결정 및 구현 — ✅ 완료
**우선도**: 낮음, **결정**: 선택지 1 (배포 보안)

**정책**: 배포 startup.scr에서 CI_LAB 제거 + 테스트 빌드 분리

**구현 완료** (2026-07-05):
- [x] **배포 빌드**: `cpu1_cfe_es_startup.scr` — CI_LAB 삭제 (라인 7 제거)
- [x] **테스트 빌드**: `cpu1_cfe_es_startup_test.scr` (신규) — CI_LAB 포함, 주석 추가
- [x] **문서화**: `mission_app_runtime_spec.md` §18.14 "배포 구성 및 테스트 빌드"

**효과**:
- 배포: 공격 표면 축소, UDP 1234 테스트 경로 비활성
- 테스트: 개발·CI/CD에서 로컬 명령 경로 유지 가능

---

## 최종 완료 요약 (2026-07-05)

| 항목 | 상태 | 핵심 파일 |
|---|---|---|
| **Phase 1** (A-3 명령) | ✅ 완료 | uplink_app_cmds.c, cfs_core_app_utils.c, lora_tdm_app_utils.c |
| **Phase 2** (request_token 규약) | ✅ 완료 | mission_app_runtime_spec.md §18.4.7 |
| **Phase 3.1** (fail-safe boot) | ✅ 완료 | uplink_app_cmds.c (health-block 반전) |
| **Phase 3.2** (권한 검증) | ✅ 완료 | uplink_app_cmds.c + mission_app_runtime_spec.md §18.11.1 |
| **Phase 3.3** (SEQ_FAIL 피드백) | ✅ 완료 | lora_tdm_app (UPLINK_STATUS 구독) + lora_tdm_app_behavior_spec.md §9.2 |
| **Phase 3.4** (CI_LAB 정책) | ✅ 완료 | 정책 결정: 배포에서 제거, 테스트 빌드 분리 |

**상태**: **Phase 3 전체 완료** ✅

---

## 배포 전 남은 것

- [x] ✅ Phase 3.4 구현: startup.scr CI_LAB 삭제 + startup_test.scr 작성 **완료**
- [x] ✅ A-3 단위테스트 케이스 설계 **완료** (→ `A3_unittest_cases.md`)
  - 25개 테스트 케이스 (RECOVERY/MODE/DIAGNOSTIC/권한/fail-safe/SEQ_FAIL)
- [x] ✅ A-3 코드 검증 **완료** (→ `A3_test_traceability.md`)
  - 설계-구현 추적성 검증: 100% 완벽 매칭 ✓
  - 권한 검증 함수 로직 정상 ✓
  - Fail-safe boot 로직 정상 ✓
  - SEQ_FAIL 처리 로직 정상 ✓
  - 실행 미진행: CMOCKA 라이브러리 부재 (배포 환경에서 통합테스트로 대체)
- [x] ✅ 통합테스트 검증 계획 수립 **완료** (→ `integration_test_plan.md`)
  - 19개 시나리오 설계
  - Fail-safe boot, 권한 검증, SEQ_FAIL, A-3 명령, 회귀, 보안
- [ ] 통합테스트 실행 (SITL 또는 실 하드웨어에서)
- [ ] 배포 빌드 및 검증

---

## 이 세션에서 진행한 작업

**시작**: 2026-07-05 Phase 2.1 request_token 규약 확정
**완료**: 2026-07-05 배포 전 테스트 계획 수립

**총 진행** (Phase 1 ~ 3.4 + 테스트 계획):
- 코드 변경: 3개 앱 (uplink_app, lora_tdm_app, cfs_core_app 참조용)
- Spec 추가: 5개 섹션 (request_token, fail-safe, 권한 검증, SEQ_FAIL, CI_LAB)
- 파일 수정/작성: 8개 파일 + 2개 테스트 계획 문서

**핵심 성과**:
1. Request token 규약 최종화 (재설계 리스크 제거)
2. 부팅 fail-safe + 데드락 해제
3. 명령 권한 검증 시스템 추가
4. 시퀀스 거부 피드백 구현
5. CI_LAB 보안 정책 결정 및 구현
6. **A-3 단위테스트 케이스 20+ 개 설계**
7. **통합테스트 시나리오 19개 설계**

**테스트 준비 완료**:
- 배포 전 단위테스트 계획 (build-ut 또는 통합 대체)
- 배포 전 통합테스트 계획 (SITL/실 하드웨어)
