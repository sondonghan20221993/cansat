# A-3 단위테스트 케이스 (Phase 1)

> 빌드 환경: cFS build-ut (CMake + CMOCKA)  
> 상태: **A-3.1~3.3 구현·실행 완료 (2026-07-14)** — `~/verify-build` 로컬
> build-ut 환경으로 실행. 상세: `notes/temp/a3_unittest_gap_implementation.md`.
> 권한검증(A.1~A.4)·fail-safe(B.1/B.2)는 `uplink_app_cmds_ut_auth_gate_failures.md`
> (커밋 `740521d`)에서 이미 별도 구현·검증 완료.

---

## A-3.1 RECOVERY 명령 테스트

### 테스트 파일
- `uplink_app/unit-test/coveragetest/coveragetest_uplink_app_utils.c` (또는 신규 파일)
- Target: `uplink_app_utils.c::UPLINK_APP_ForwardRecoveryCommand`
- Target: `cfs_core_app_utils.c::CFS_CORE_APP_ProcessRecoveryCommand`

### 테스트 케이스

#### 1.1 정상 케이스: RESET_COUNTER 액션
```
입력:
  - RecoveryCmdTlm_t.RecoveryAction = RESET_COUNTER (0)
  - RecoveryCmdTlm_t.TargetComponent = BRIDGE (1)
  - RecoveryCmdTlm_t.RequestToken = 0x12345678

기대:
  - RECOVERY_CMD_MID 발행 ✓
  - RecoveryAction, TargetComponent, RequestToken이 그대로 전달 ✓
  - AcceptedCount 증가 ✓
```

#### 1.2 정상 케이스: RESTART_BRIDGE 액션
```
입력:
  - RecoveryAction = RESTART_BRIDGE (1)
  - TargetComponent = LORA_TDM (2)
  - RequestToken = 0xAABBCCDD

기대:
  - RECOVERY_CMD_MID 발행 ✓
  - cfs_core_app이 RESTART_BRIDGE 로직 실행
```

#### 1.3 정상 케이스: PARSER_RESET 액션
```
입력:
  - RecoveryAction = PARSER_RESET (2)
  - RequestToken = 0

기대:
  - 대상 앱으로 라우팅 ✓
```

#### 1.4 정상 케이스: SERIAL_RECONNECT 액션
```
입력:
  - RecoveryAction = SERIAL_RECONNECT (3)

기대:
  - 라우팅 성공 ✓
```

#### 1.5 오류 케이스: 미정의 액션
```
입력:
  - RecoveryAction = 0xFF (invalid)

기대:
  - RejectedCount 증가 ✓
  - EVS 오류 이벤트 ✓
```

#### 1.6 오류 케이스: 권한 부족 (Level < 3)
```
입력:
  - Flags[7:6] = 1 (Level 2)
  - CommandClass = RECOVERY (Level 3 필요)

기대:
  - AUTHZ_BLOCK_EID EVS 발생 ✓
  - RejectedCount 증가 ✓
```

#### 1.7 오류 케이스: Level 3 + RequestToken = 0
```
입력:
  - Flags[7:6] = 2 (Level 3)
  - RequestToken = 0

기대:
  - AUTHZ_BLOCK_EID EVS 발생 ✓
  - 거부 ✓
```

---

## A-3.2 MODE 명령 테스트

### 테스트 파일
- `cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app_utils.c`
- Target: `cfs_core_app_utils.c::CFS_CORE_APP_ProcessModeCommand`

### 테스트 케이스

#### 2.1 정상 케이스: NORMAL → RECOVERY
```
입력:
  - ModeAction = ENTER_RECOVERY (0)
  - RequestedState = RECOVERY (1)
  - 현재 상태 = NOMINAL (0)

기대:
  - 상태 전이 허용 ✓
  - MODE_CMD_MID 발행 ✓
  - LastModeRequestToken 저장 ✓
```

#### 2.2 정상 케이스: RECOVERY → NORMAL
```
입력:
  - ModeAction = EXIT_RECOVERY (1)
  - RequestedState = NOMINAL (0)
  - 현재 상태 = RECOVERY (2)

기대:
  - 상태 전이 허용 ✓
```

#### 2.3 오류 케이스: 불가능 전이 (NORMAL → NORMAL)
```
입력:
  - RequestedState = NOMINAL
  - 현재 상태 = NOMINAL

기대:
  - 전이 거부 ✓
  - EVS 이벤트 ✓
```

#### 2.4 오류 케이스: 미정의 상태
```
입력:
  - RequestedState = 0xFF

기대:
  - RejectedCount 증가 ✓
```

#### 2.5 오류 케이스: 권한 부족
```
입력:
  - Flags[7:6] = 1 (Level 2, MODE는 Level 3 필요)

기대:
  - AUTHZ_BLOCK_EID 발생 ✓
```

---

## A-3.3 DIAGNOSTIC 명령 테스트

### 테스트 파일
- `lora_tdm_app/unit-test/coveragetest/coveragetest_lora_tdm_app_utils.c`
- Target: `lora_tdm_app_utils.c::LORA_TDM_APP_ProcessDiagnosticCommand`

### 테스트 케이스

#### 3.1 정상 케이스: LINK_STATUS 액션
```
입력:
  - DiagAction = LINK_STATUS (0)
  - DiagTarget = LORA_TDM (0)

기대:
  - EVS 링크 상태 요약 출력 ✓
  - 명령 실행 ✓
```

#### 3.2 정상 케이스: RX_STATS 액션
```
입력:
  - DiagAction = RX_STATS (1)

기대:
  - RX 통계 로깅 ✓
```

#### 3.3 정상 케이스: TX_STATS 액션
```
입력:
  - DiagAction = TX_STATS (2)

기대:
  - TX 통계 로깅 ✓
```

#### 3.4 오류 케이스: 미정의 액션
```
입력:
  - DiagAction = 0xFF

기대:
  - RejectedCount 증가 ✓
```

---

## 권한 검증 통합 테스트 (Phase 3.2)

### 테스트 파일
- `uplink_app/unit-test/coveragetest/coveragetest_uplink_app_cmds.c`
- Target: `uplink_app_cmds.c::UPLINK_APP_IsAuthorized`

### 테스트 케이스

#### A.1 권한 검증: L2 명령 × L1 권한 = 거부
```
입력:
  - Flags[7:6] = 0 (Level 1)
  - CommandClass = runtime_config (Level 2)

기대:
  - AUTHZ_BLOCK_EID 발생 ✓
```

#### A.2 권한 검증: L3 명령 × L2 권한 = 거부
```
입력:
  - Flags[7:6] = 1 (Level 2)
  - CommandClass = recovery (Level 3)

기대:
  - AUTHZ_BLOCK_EID 발생 ✓
```

#### A.3 권한 검증: L1 명령 × L3 권한 = 허용
```
입력:
  - Flags[7:6] = 2 (Level 3)
  - CommandClass = diagnostic (Level 1)

기대:
  - 허용 ✓
```

#### A.4 권한 검증: L3 명령 + token=0 = 거부
```
입력:
  - Flags[7:6] = 2 (Level 3)
  - CommandClass = recovery (Level 3)
  - RequestToken = 0

기대:
  - AUTHZ_BLOCK_EID 발생 ✓
```

---

## Fail-Safe Boot 테스트 (Phase 3.1)

### 테스트 파일
- `uplink_app/unit-test/coveragetest/coveragetest_uplink_app_cmds.c`
- Target: `uplink_app_cmds.c::UPLINK_APP_ProcessUplinkCommand`

### 테스트 케이스

#### B.1 부팅 fail-safe: health 미수신 = 모든 명령 차단
```
입력:
  - CfsHealthReceived = false
  - CommandClass = diagnostic (Level 1, 가장 낮은 권한)

기대:
  - EVS "command blocked (no health yet)" 발생 ✓
  - RejectedCount 증가 ✓
```

#### B.2 health 수신 후 = 명령 허용
```
입력:
  - CfsHealthReceived = true
  - CfsHealthState = NOMINAL (0)
  - CommandClass = diagnostic

기대:
  - 라우팅 진행 ✓
```

---

## SEQ_FAIL 피드백 테스트 (Phase 3.3)

### 테스트 파일
- `lora_tdm_app/unit-test/coveragetest/coveragetest_lora_tdm_app_dispatch.c`
- Target: `lora_tdm_app_dispatch.c::LORA_TDM_APP_ProcessCommandPacket`

### 테스트 케이스

#### C.1 시퀀스 거부 → SEQ_FAIL 설정
```
입력:
  - UPLINK_STATUS_MID 수신
  - LastCommandResult = 3 (REJECT_SEQUENCE)

기대:
  - PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_SEQ_FAIL (2) ✓
```

#### C.2 정상 수신 → OK 설정
```
입력:
  - UPLINK_STATUS_MID
  - LastCommandResult = 0 (SUCCESS)

기대:
  - PendingUplinkFeedback = LORA_TDM_APP_UPLINK_FB_OK (0) ✓
```

---

## 실행 방법

### 빌드 환경 (CMake + CMOCKA)

```bash
cd /path/to/cfs_base/build/pc-linux/mission_app

# 단위테스트 빌드
make mission_app-test

# 테스트 실행
mission_app-test
```

### 테스트 커버리지

기대 커버리지:
- `UPLINK_APP_IsAuthorized()`: 100%
- `UPLINK_APP_GetClassRequiredLevel()`: 100%
- Health-block 로직: 100%
- SEQ_FAIL 처리: 100%

---

## 현재 상태 (2026-07-14 갱신)

| 상태 | 항목 |
|---|---|
| ✅ 케이스 설계 | A-3.1~3.3, Phase 3.1~3.3 |
| ✅ 코드 작성·실행 | A-3.1(RECOVERY 5개)/A-3.2(MODE 4개)/A-3.3(DIAGNOSTIC 4개) +
| | SEQ_FAIL 피드백(C.1/C.2 2개) — `notes/temp/a3_unittest_gap_implementation.md` |
| ✅ 권한검증/fail-safe | A.1~A.4, B.1/B.2 — `uplink_app_cmds_ut_auth_gate_failures.md`(커밋 `740521d`)에서 기 구현 |
| ✅ 회귀 확인 | 관련 5개 스위트 전부 PASS, 실패 없음 (로컬 `~/verify-build`) |
| ⏳ Pi 배포 | 코드 변경 없음(테스트만 추가) — 배포 항목 아님 |

---

## BL-38 CheckAppRestarts() UT 케이스 (2026-07-23 설계, 구현 시 작성)

대상: `cfs_core_app_utils.c` 신설 `CheckAppRestarts()` + 순수화된 `PublishSystemHealth()`

| ID | 케이스 | 검증 |
|---|---|---|
| BL38-UT-1 | EKF fault 참 + UplinkTimedOut 참 | 재시작 발동됨(체인 비종속) — BL-38 핵심 회귀 |
| BL38-UT-2 | 3개 앱 동시 타임아웃 | 1사이클에 RestartApp 1회만, 대상=bridge(우선순위) |
| BL38-UT-3 | 스킵된 앱 다음 사이클 처리 | UT-2 후속 사이클(bridge 쿨다운 중)에서 uplink 발행 |
| BL38-UT-4 | 쿨다운 미경과 | NextRestartMs 도래 전 재발행 없음 |
| BL38-UT-5 | attempt 4회째 발행 | 구 MAX(3) 초과에도 계속 발행(무한 재시도) |
| BL38-UT-6 | HK 재시도 카운터 | 발행 시마다 단조 증가, 타 앱 fault로 리셋되지 않음(구 EKF 리셋 버그 회귀) |
| BL38-UT-7 | GetAppIDByName 실패 | 카운터 미증가 + 쿨다운은 진행(에러 무한루프 아님) — 동작 정의는 구현 시 확정 |
| BL38-UT-8 | FaultCode 보고 불변 | EKF+uplink 동시 fault 시 FaultCode=3, UplinkStatus.TimedOut=1 병행 |
| BL38-UT-9 | 전 앱 정상 | 재시작 미발동, 카운터 불변 |
