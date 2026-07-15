# 이중 버퍼 토글 구현 세부 (cfs_core_app)

## 구조체 정의

```c
// cfs_core_app.h:78-80
typedef struct {
  uint32 AttitudeTimeoutMs;
  uint32 LocalTimeoutMs;
  uint32 GpsTimeoutMs;
  uint32 EkfTimeoutMs;
  uint32 BridgeTimeoutMs;
  uint32 PublishPeriodMs;
} CFS_CORE_APP_ConfigParams_t;

typedef struct {
  // ... (다른 상태)
  CFS_CORE_APP_ConfigParams_t ActiveConfig;    // 78: 현재 사용 중
  CFS_CORE_APP_ConfigParams_t PendingConfig;   // 79: 검증 대기 중
  CFS_CORE_APP_ConfigParams_t PreviousConfig;  // 80: 이전 설정 (롤백용)
} CFS_CORE_APP_Data_t;
```

## 상태 머신

```
IDLE (초기상태)
  ↓ [CONFIG_CMD_MID 수신]
PENDING (검증 중)
  ├─ [검증 성공] → [토글] → IDLE (ConfigGeneration++)
  └─ [검증 실패] → REJECTED → IDLE
```

---

## 처리 흐름 (ProcessConfigCommand)

### 단계 1: 수신 직후 - 포맷 검증

```c
/* cfs_core_app_utils.c:523 */
void CFS_CORE_APP_ProcessConfigCommand(const CFS_CORE_APP_ConfigCmdTlm_t *Msg)
{
  const CFS_CORE_APP_ConfigPayloadHdr_t *Hdr;
  uint32 Value;
  CFS_CORE_APP_ConfigParams_t Candidate;  // 검증용 임시

  /* ── 1. 포맷 검증 (사전 검사) ── */
  if (Msg->PayloadLength < sizeof(ConfigPayloadHdr_t)) {
    ErrCounter++;
    ConfigPendingState = CONFIG_PENDING_REJECTED;
    LastConfigResult = CONFIG_RESULT_BAD_LENGTH;
    return;
  }

  Hdr = (ConfigPayloadHdr_t*)Msg->Payload;

  // Scope 확인 (자신의 scope인가?)
  if (Hdr->ConfigScope != CFS_CORE_APP_CONFIG_SCOPE) {  // 1 확인
    ErrCounter++;
    ConfigPendingState = CONFIG_PENDING_REJECTED;
    LastConfigResult = CONFIG_RESULT_BAD_SCOPE;
    return;
  }

  // Version 확인 (호환 가능한가?)
  if (Hdr->ConfigVersion != CFS_CORE_APP_CONFIG_VERSION) {
    ErrCounter++;
    ConfigPendingState = CONFIG_PENDING_REJECTED;
    LastConfigResult = CONFIG_RESULT_BAD_VERSION;
    return;
  }

  // Checksum 검증 (payload 무결성)
  ValueBytes = Msg->Payload + sizeof(*Hdr);
  Expected = CFS_CORE_APP_ConfigChecksum(Hdr, ValueBytes, Hdr->ValueLength);
  if (Hdr->Checksum != Expected) {
    ErrCounter++;
    ConfigPendingState = CONFIG_PENDING_REJECTED;
    LastConfigResult = CONFIG_RESULT_CHECKSUM_FAIL;
    event_send("config checksum mismatch");
    return;
  }
  
  // Value 디코딩
  if (Hdr->ValueLength != sizeof(uint32)) {
    ErrCounter++;
    return;
  }
  memcpy(&Value, ValueBytes, sizeof(uint32));
```

### 단계 2: Pending 버퍼 준비

```c
  /* ── 2. Pending 버퍼 준비 (쓰기 안전 영역) ── */
  
  // 2-1. Pending ← Active (기존값 복사)
  //      왜? 새 파라미터만 변경, 나머지는 유지
  PendingConfig = ActiveConfig;
  ConfigPendingState = CONFIG_PENDING_PENDING;
  
  // 2-2. 새 파라미터 적용
  switch (Hdr->ParameterId) {
    case PARAM_ATTITUDE_TIMEOUT_MS:
      PendingConfig.AttitudeTimeoutMs = Value;
      break;
    case PARAM_LOCAL_TIMEOUT_MS:
      PendingConfig.LocalTimeoutMs = Value;
      break;
    case PARAM_GPS_TIMEOUT_MS:
      PendingConfig.GpsTimeoutMs = Value;
      break;
    // ... 더 많은 파라미터들
    case PARAM_PUBLISH_PERIOD_MS:
      PendingConfig.PublishPeriodMs = Value;
      break;
    default:
      ErrCounter++;
      ConfigPendingState = CONFIG_PENDING_REJECTED;
      LastConfigResult = CONFIG_RESULT_BAD_PARAM;
      return;  // ← 여기서 검증 실패, Active 미변경
  }
```

### 단계 3: 검증 (교차 일관성)

```c
  /* ── 3. PendingConfig 전체 검증 ── */
  // 임시 복사본에서 검증 (원본 변경 전)
  Candidate = PendingConfig;
  
  if (Candidate.AttitudeTimeoutMs < PARAM_MIN_MS ||
      Candidate.LocalTimeoutMs < PARAM_MIN_MS ||
      Candidate.GpsTimeoutMs < PARAM_MIN_MS ||
      Candidate.EkfTimeoutMs < PARAM_MIN_MS ||
      Candidate.BridgeTimeoutMs < PARAM_MIN_MS ||
      Candidate.PublishPeriodMs < PARAM_MIN_MS) {
    
    ErrCounter++;
    ConfigPendingState = CONFIG_PENDING_REJECTED;
    LastConfigResult = CONFIG_RESULT_BAD_VALUE;  // 범위 오류
    return;  // ← 검증 실패, Active 미변경
  }
  
  // 추가 교차 검증 가능
  if (Candidate.AttitudeTimeoutMs > Candidate.BridgeTimeoutMs) {
    // 상호 의존성 검증
    ConfigPendingState = CONFIG_PENDING_REJECTED;
    LastConfigResult = CONFIG_RESULT_INCONSISTENT;
    return;  // ← 검증 실패, Active 미변경
  }
```

### 단계 4: 토글 (원자 활성화)

```c
  /* ── 4. 토글 (원자 구간, 매우 짧음) ── */
  //
  // 이 단계는 인터럽트 disabled 또는 spinlock 내에서 수행해야 함
  // (보통 cFS EVS_SendEvent 이전에 위치, 실제로는 더 짧아야 함)
  
  // Step 4-1: 현재 Active를 Previous로 백업 (롤백용)
  PreviousConfig = ActiveConfig;
  // 상태: Previous = old, Active = old, Pending = new (검증됨)
  
  // Step 4-2: Pending을 Active로 승격 (토글)
  ActiveConfig = PendingConfig;
  // 상태: Previous = old, Active = new, Pending = new (복사)
  
  // Step 4-3: 메타데이터 업데이트
  ConfigGeneration++;  // 버전 증가 (몇 번 변경됐는가)
  ConfigPendingState = CONFIG_PENDING_IDLE;  // 상태 정상화
  LastConfigResult = CONFIG_RESULT_OK;  // 결과 기록
  CmdCounter++;  // 명령 카운터
  
  CFE_EVS_SendEvent(INFO_EID, "config activated generation=%lu", ConfigGeneration);
}
```

---

## 시간 흐름 다이어그램

```
T0: CONFIG_CMD_MID 수신
    ┌──────────────────────────────────────────────────┐
    │ Active:   {100, 200, 300, 400, 500, 1000}       │
    │ Pending:  (불명)                                 │
    │ Previous: (불명)                                 │
    │ State:    IDLE                                    │
    └──────────────────────────────────────────────────┘

T0 → T1: 포맷/범위 검증
    ┌──────────────────────────────────────────────────┐
    │ Active:   {100, 200, 300, 400, 500, 1000}       │
    │ Pending:  (검증 중)                               │
    │ Previous: (불명)                                 │
    │ State:    PENDING                                 │
    └──────────────────────────────────────────────────┘

T1 → T2: Pending ← Active (복사) + 신규 파라미터 적용
    ┌──────────────────────────────────────────────────┐
    │ Active:   {100, 200, 300, 400, 500, 1000}       │
    │ Pending:  {100, 200, 150, 400, 500, 1000}  ← 변경됨
    │ Previous: (불명)                                 │
    │ State:    PENDING                                 │
    └──────────────────────────────────────────────────┘

T2 → T3: 검증 (범위, 교차 일관성)
    ┌──────────────────────────────────────────────────┐
    │ Active:   {100, 200, 300, 400, 500, 1000}       │
    │ Pending:  {100, 200, 150, 400, 500, 1000} ✓ OK  │
    │ Previous: (불명)                                 │
    │ State:    PENDING                                 │
    └──────────────────────────────────────────────────┘

T3 → T4: 토글 (원자 구간 <<<<<<< 여기!)
    ┌──────────────────────────────────────────────────┐
    │ Step 4-1: Previous ← Active                      │
    │ Previous: {100, 200, 300, 400, 500, 1000}       │
    │                                                   │
    │ Step 4-2: Active ← Pending                       │
    │ Active:   {100, 200, 150, 400, 500, 1000}       │
    │                                                   │
    │ Step 4-3: Metadata update                        │
    │ ConfigGeneration++  (→ 2)                        │
    │ State: IDLE                                       │
    └──────────────────────────────────────────────────┘

T4 이후: 정상 동작 (새 파라미터 적용됨)
    ┌──────────────────────────────────────────────────┐
    │ Active:   {100, 200, 150, 400, 500, 1000} ← 사용중
    │ Pending:  {100, 200, 150, 400, 500, 1000}       │
    │ Previous: {100, 200, 300, 400, 500, 1000} ← 롤백용
    │ State:    IDLE                                    │
    │ Gen:      2 (변경됨)                              │
    └──────────────────────────────────────────────────┘
```

---

## 핵심 특성

### 1️⃣ 쓰기 안전 (Write-Safe)

**Pending 버퍼는 별도 영역**
```
Active의 리더
  ↓ [읽음]
  Active (완전한 상태, 부분 업데이트 없음)

Config 작성자
  ↓ [쓰기 중]
  Pending (독립적, 리더와 충돌 없음)
```

### 2️⃣ 원자 토글 (Atomic Toggle)

**토글 시점이 명확**
```
토글 전: 리더들은 old config 읽음 (모두 일관됨)
  ↓ [짧은 원자 구간]
토글 후: 리더들은 new config 읽음 (모두 일관됨)

→ 중간 상태(partial update)는 절대 없음
```

### 3️⃣ 롤백 지원

**Previous로 빠르게 복구**
```c
void CFS_CORE_APP_RollbackConfig(uint8 reason) {
  ActiveConfig = PreviousConfig;  // 이전 상태로 즉시 복구
  LastRollbackReason = reason;
  ConfigGeneration++;  // 롤백도 버전 변경으로 기록
}
```

---

## 검증 실패 시나리오

```
시나리오 1: 포맷 오류 (Bad Length)
  ┌─────────────────────────┐
  │ Checksum 검증 실패       │
  │ → ConfigPendingState=REJECTED
  │ → Active 무변경
  └─────────────────────────┘

시나리오 2: 범위 오류 (Bad Value)
  ┌─────────────────────────┐
  │ Pending 생성 후          │
  │ Range check 실패         │
  │ → ConfigPendingState=REJECTED
  │ → Active 무변경
  │ → PendingConfig 폐기
  └─────────────────────────┘

시나리오 3: 검증 완료 후 토글 실패
  ┌─────────────────────────┐
  │ 토글 중 오류?            │
  │ (매우 드문 경우)         │
  │ → 토글 자체는 fail-safe  │
  │ → Previous 백업 있음     │
  │ → 수동 rollback 가능     │
  └─────────────────────────┘
```

---

## 메모리 레이아웃

```
CFS_CORE_APP_Data_t
├─ ActiveConfig (offset 78)
│  ├─ AttitudeTimeoutMs   (4B)
│  ├─ LocalTimeoutMs      (4B)
│  ├─ GpsTimeoutMs        (4B)
│  ├─ EkfTimeoutMs        (4B)
│  ├─ BridgeTimeoutMs     (4B)
│  └─ PublishPeriodMs     (4B)
│  ↑ = 24B 완전 단위
│
├─ PendingConfig (offset 78+24)
│  └─ (동일 레이아웃, 24B)
│  ↑ = 쓰기 전용
│
└─ PreviousConfig (offset 78+48)
   └─ (동일 레이아웃, 24B)
   ↑ = 읽기 전용 (롤백용)
```

**복사 비용: 매 토글마다 24B × 3 = 72B 복사 (~μs)**

---

## HK Telemetry (모니터링)

```c
struct {
  uint8  ConfigPendingState;    // IDLE=0, PENDING=1, REJECTED=2
  uint8  LastConfigResult;      // OK=0, CHECKSUM_FAIL=1, BAD_VALUE=2, ...
  uint8  LastRollbackReason;    // 0=없음, 1~N=원인
  uint32 ConfigGeneration;      // 활성화 횟수 (증가값)
} → 지상국 모니터링 가능
```

**지상국에서 확인 가능:**
- "Config generation 5 → 6 변경" = 1회 적용됨
- "LastConfigResult=OK" = 마지막 적용 성공
- "ConfigPendingState=IDLE" = 정상 대기 중
