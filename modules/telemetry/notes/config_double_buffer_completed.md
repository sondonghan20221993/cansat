# 이중 버퍼 토글 패턴 (Double-Buffer Toggle)

## 설계 (spec 14)

```
write_buffer (pending_config)    read_buffer (active_config)
      ↓                                 ↓
 [새 설정 기록]               [현재 사용 중]
      ↓
 [전체 검증]
      ↓
 [토글] ← 짧은 원자 구간
      ↓
 [역할 교환]

write_buffer ← previous (이전 설정, 롤백용)
read_buffer = pending (새 설정 적용)
```

## 의도

**문제:** 부분 업데이트(tearing)로 인한 불일치
```
❌ 위험:
// Thread A: config 갱신 중
active_config.param1 = new_val1;
active_config.param2 = new_val2;  // 아직 진행 중

// Thread B: config 읽음 (중간에 읽음)
if (config.param1 == new_val1 && config.param2 == old_val2) {
  // 불일치 상태! (param1은 new, param2는 old)
}
```

**해결:** 버퍼 토글 (전체 원자 교체)
```
✓ 안전:
// Active: old_config (완전하고 검증됨)

// Pending: new_config (별도에서 작성 + 검증)

// Activation (원자 구간, 짧음):
temp = active;
active = pending;
pending = temp;  // 또는 previous

// 결과:
// Active: new_config (즉시 모두가 new 값 읽음)
// Pending/Previous: old_config (다음 업데이트 준비)
```

---

## 현재 구현 상태

### ✓ 설계는 있음 (spec 13, 14)
- `active_config`: 현재 검증된 구성
- `pending_config`: 검증 중인 새 구성
- `previous_config`: 롤백 대상

### ⚠️ 코드 구현은 미흡
```
cfs_core_app:
  ✓ ConfigPendingState (상태 머신)
  ✓ LastConfigResult (결과)
  ✗ active_config 버퍼 (미구현)
  ✗ pending_config 버퍼 (미구현)
  ✗ previous_config 버퍼 (미구현)
  ✗ 원자 토글 로직 (미구현)

mavlink_bridge_app:
  ✓ 상태 변수
  ✗ 이중 버퍼 토글 (미구현)

lora_tdm_app:
  ✓ 상태 변수
  ✗ 이중 버퍼 토글 (미구현)
```

---

## 이중 버퍼 구조 예

```c
// 이중 버퍼 선언
typedef struct {
  uint32 health_threshold_ms;
  uint16 publish_interval_ms;
  uint8  recovery_mode;
} CFS_CORE_APP_Config_t;

typedef struct {
  CFS_CORE_APP_Config_t active;     // 현재 사용 (읽기 안전)
  CFS_CORE_APP_Config_t pending;    // 다음 (쓰기 안전)
  CFS_CORE_APP_Config_t previous;   // 이전 (롤백용)
  uint32 generation;                // active 버전
} CFS_CORE_APP_ConfigBuffer_t;

// 토글 구현
void CFS_CORE_APP_ActivateConfig(void)
{
  // 검증 완료 시 호출 (활성화 경계 내)
  
  // 1. Previous ← Active (롤백용 백업)
  Data->ConfigBuf.previous = Data->ConfigBuf.active;
  
  // 2. Active ← Pending (토글)
  Data->ConfigBuf.active = Data->ConfigBuf.pending;
  
  // 3. 메타데이터 업데이트
  Data->ConfigBuf.generation++;
  Data->ConfigPendingState = CONFIG_IDLE;
  Data->LastConfigResult = CONFIG_SUCCESS;
}

// 롤백 구현
void CFS_CORE_APP_RollbackConfig(uint8 reason)
{
  // Active ← Previous (복구)
  Data->ConfigBuf.active = Data->ConfigBuf.previous;
  Data->LastRollbackReason = reason;
  Data->ConfigBuf.generation++;  // 또는 변경 안 함
}

// 읽기 (안전, 락 불필요)
void CFS_CORE_APP_ApplyActiveConfig(void)
{
  CFS_CORE_APP_Config_t cfg = Data->ConfigBuf.active;  // 복사
  
  // cfg는 완전한 상태 (절대 부분 업데이트 아님)
  gHealthThresholdMs = cfg.health_threshold_ms;
  gPublishIntervalMs = cfg.publish_interval_ms;
  // ...
}
```

---

## 토글 타이밍 (활성화 경계)

```
┌──────────────────────┐
│ Normal Operation     │
│ Active 사용 중       │
└──────────────────────┘
         ↓ [CONFIG_CMD_MID 수신]
┌──────────────────────┐
│ Pending 버퍼 기록    │
│ 전체 검증            │
│ 결과: PENDING 상태   │
└──────────────────────┘
         ↓ [다음 경계 도달]
┌──────────────────────┐
│ ← 활성화 경계        │
│ Atomic Exchange      │
│ Previous ← Active    │
│ Active ← Pending     │
│ Generation++         │
│ State ← IDLE         │
└──────────────────────┘
         ↓
┌──────────────────────┐
│ Normal Operation     │
│ Active (new) 사용    │
└──────────────────────┘
```

---

## 앱별 활성화 경계

| 앱 | 경계 | 빈도 | 구현 |
|-----|------|------|------|
| lora_tdm_app | TDM 슬롯 경계 | ~100ms | ⏳ 미구현 |
| mavlink_bridge_app | FC 스트림 처리 경계 | ~10-50ms | ⏳ 미구현 |
| cfs_core_app | 운영자 승인 또는 시스템 안전 경계 | 수동 또는 ~1s | ⏳ 미구현 |

---

## 향후 구현 우선순위

1. **High**: mavlink_bridge_app (FC 연결 파라미터 변경)
2. **Medium**: lora_tdm_app (TDM 슬롯 재구성)
3. **Low**: cfs_core_app (수동 활성화, 안전성 우선)

---

## 참고: 현재 간단한 구현

현재는 토글 대신 **상태 머신** 사용:
```
IDLE → [CONFIG_CMD 수신] → PENDING → [검증] → IDLE or REJECTED
```

이는 Config가 작거나 자주 변경되지 않는 경우 충분하지만,
**빈번한 업데이트 또는 복잡한 파라미터 세트** → 토글 필요

---

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

---

# 앱별 이중 버퍼 토글 처리 현황

## 요약

| 앱 | 토글 구현 | 파일 | 상태 |
|-----|---------|------|------|
| **cfs_core_app** | ✅ 완전 구현 | `cfs_core_app_utils.c:523` | **운영 중** |
| **mavlink_bridge_app** | ❓ 미확인 | `mavlink_bridge_app_utils.c:~1777` | ? |
| **lora_tdm_app** | ❓ 미확인 | ? | ? |
| **uplink_app** | ❌ 미구현 | N/A | (자신의 config 없음) |

---

## 1. cfs_core_app ✅ 완전 구현

### 구조
```c
// cfs_core_app.h:78-80
CFS_CORE_APP_ConfigParams_t ActiveConfig;
CFS_CORE_APP_ConfigParams_t PendingConfig;
CFS_CORE_APP_ConfigParams_t PreviousConfig;
```

### 토글 로직 (cfs_core_app_utils.c:523)
```c
void CFS_CORE_APP_ProcessConfigCommand(...)
{
  // 1. 포맷 검증
  // 2. Pending ← Active (복사)
  //    + 신규 파라미터 적용
  // 3. 범위/교차 검증
  // 4. 토글:
  PreviousConfig = ActiveConfig;  // 백업
  ActiveConfig = PendingConfig;   // 승격
  ConfigGeneration++;
}
```

### 활성화 경계
```
운영자 승인 또는 안전한 시스템 경계
(즉시 활성화는 아님, 선택적 지연)
```

### 상태 변수
```
ConfigPendingState: IDLE / PENDING / REJECTED
LastConfigResult: OK / CHECKSUM_FAIL / BAD_VALUE / ...
ConfigGeneration: 활성화 횟수 (롤백 감지 가능)
LastRollbackReason: 0=없음 / 1~N=원인
```

---

## 2. mavlink_bridge_app ❓ 미확인

### 구조 (예상)
```c
// default_mavlink_bridge_app_msgstruct.h?
MAVLINK_BRIDGE_APP_ConfigParams_t ActiveConfig;
MAVLINK_BRIDGE_APP_ConfigParams_t PendingConfig;
MAVLINK_BRIDGE_APP_ConfigParams_t PreviousConfig;
```

### 파라미터 (추측)
- FC 스트림 요청 파라미터
- 재시도 한도
- 타임아웃 임계값

### 활성화 경계
```
다음 FC 스트림 처리 경계
(~10-50ms, 즉시 활성화)
```

### ❓ 구현 확인 필요
```bash
grep -n "ProcessConfigCommand\|ActiveConfig\|PendingConfig" \
  /home/sdh2983/cfs-telemetry-app/mavlink_bridge_app/fsw/src/*.c
```

---

## 3. lora_tdm_app ❓ 미확인

### 구조 (추측)
```c
// lora_tdm_app의 config?
LORA_TDM_APP_ConfigParams_t ActiveConfig;
LORA_TDM_APP_ConfigParams_t PendingConfig;
LORA_TDM_APP_ConfigParams_t PreviousConfig;
```

### 파라미터 (추측)
- TDM 슬롯 주기
- 전송 타임아웃
- 활성 MID 목록

### 활성화 경계
```
다음 TDM 슬롯 경계
(~슬롯 주기, 예: 100ms)
```

### ❓ 구현 확인 필요
```bash
grep -n "ProcessConfigCommand\|ActiveConfig\|PendingConfig" \
  /home/sdh2983/cfs-telemetry-app/lora_tdm_app/fsw/src/*.c
```

---

## 4. uplink_app ❌ 미구현

### 이유
- 자신의 config scope이 없음 (ProcessConfigCommand 없음)
- 다른 앱 대상만 forward (ForwardConfigCommand만 있음)

### 현황
```
uplink_app = 중개 역할만 수행
  ├─ CONFIG_CMD_MID 수신
  ├─ Checksum 검증
  └─ 대상 앱으로 forward

자신의 설정 변경 = 불가
```

---

## CONFIG 흐름 (어느 앱이 처리하는가?)

```
지상국
  ↓
LoRa 송신
  ↓
lora_tdm_app (수신)
  ├─ 프레임 CRC 검증
  ├─ lora_tdm_app 자신의 config 처리? ❓
  │  ├─ scope=3인 경우만
  │  └─ ProcessConfigCommand?
  └─ UPLINK_APP_CMD_MID forward (다른 앱 대상)
       ↓
  uplink_app (수신)
    ├─ Checksum 검증
    └─ CONFIG_CMD_MID forward (대상 앱으로)
         ↓
  ┌─────────────────────────────────────┐
  │ 대상 앱에서 토글 처리              │
  ├─────────────────────────────────────┤
  │ cfs_core_app                        │
  │  ✅ ProcessConfigCommand 있음       │
  │  ✅ 이중 버퍼 토글 구현됨           │
  │  ✅ 즉시 활성화 또는 지연           │
  ├─────────────────────────────────────┤
  │ mavlink_bridge_app                  │
  │  ❓ ProcessConfigCommand 있는가?    │
  │  ❓ 토글 구현되어 있는가?           │
  ├─────────────────────────────────────┤
  │ lora_tdm_app                        │
  │  ❓ 자신의 config도 처리하는가?     │
  └─────────────────────────────────────┘
```

---

## 다음 단계

```
□ mavlink_bridge_app ProcessConfigCommand 확인
  grep -A 50 "void.*ProcessConfigCommand" \
    mavlink_bridge_app/fsw/src/mavlink_bridge_app_utils.c

□ lora_tdm_app 자신의 config 처리 확인
  grep -n "ProcessConfigCommand\|LORA_TDM_APP_CONFIG_SCOPE\|ActiveConfig" \
    lora_tdm_app/fsw/src/*.c

□ lora_tdm_app에서 CONFIG_CMD_MID 수신 여부 확인
  grep -n "CONFIG_CMD_MID\|Subscribe" \
    lora_tdm_app/fsw/src/lora_tdm_app.c
```

---

## 요점

**핵심: 각 앱이 자신의 설정을 직접 처리함**

```
CONFIG_CMD_MID 수신 앱들:
├─ uplink_app: forward만 (자신의 config 없음)
├─ cfs_core_app: ✅ 토글 처리
├─ mavlink_bridge_app: ❓ 토글 처리 여부?
└─ lora_tdm_app: ❓ 토글 처리 여부?

결론:
uplink_app = 중개 역할
cfs_core_app = 최종 처리 (확인됨)
나머지 = 확인 필요
```

---

# 파라미터 변경 과정 (2026-07-15)

## 개요

```
지상국
  ↓
CONFIG_CMD_MID (checksum 포함)
  ↓
lora_tdm_app (프레임 CRC 검증) ✓
  ↓
uplink_app (payload checksum 검증) ✓
  ↓
대상 앱(cfs_core_app/mavlink_bridge_app)
  ↓
pending_config (검증/적용)
  ↓
activation (active_config로 교체)
```

## 상세 흐름

### 1단계: 지상국 → LoRa

```
fc_serial_ws_server.py
  1. ConfigScope=1 (cfs_core_app 대상)
  2. ParameterId=0x1000 (health threshold)
  3. Value=threshold_value (uint32)
  4. ConfigChecksum 계산 = scope+version+param_id+value_type+value_len+value
  5. LoRa로 송신
```

**무결성 검증:** LoRa 링크 CRC-16/CCITT-FALSE

---

### 2단계: LoRa → lora_tdm_app → uplink_app

**lora_tdm_app (lora_tdm_app_utils.c:658)**
```c
LORA_TDM_APP_ProcessConfigCommand(Msg)
  if (Hdr->ConfigScope != LORA_TDM_APP_CONFIG_SCOPE) return; // 다른 앱 → 무시
  if (Hdr->Checksum != computed_checksum) { error; return; } // ✓ 검증
  // lora_tdm_app 자신의 파라미터 적용 (downlink protocol 등)
  memcpy(&Value, ValueBytes, 4);
  switch (Hdr->ParameterId) {
    case PARAM_DOWNLINK_PROTOCOL: Data->UseV2Downlink = (Value != 0); break;
  }
```

**uplink_app (uplink_app_utils.c:350)**
```c
UPLINK_APP_ForwardConfigCommand(Cmd)
  if (PayloadLength < sizeof(ConfigPayloadHdr_t)) return false; // 검증
  Hdr = (ConfigPayloadHdr_t*)Cmd->Payload;
  ValueBytes = Cmd->Payload + sizeof(*Hdr);
  if (Hdr->Checksum != ConfigChecksum(Hdr, ValueBytes, ...)) {
    error; return false; // ✓ 검증
  }
  // 다른 앱 대상이면 forward
  ConfigCmdTlm.PayloadLength = Cmd->PayloadLength;
  memcpy(ConfigCmdTlm.Payload, Cmd->Payload, Cmd->PayloadLength);
  CFE_SB_TransmitMsg(CONFIG_CMD_MID); // → cfs_core_app / mavlink_bridge_app
```

**무결성 검증:** CONFIG payload checksum

---

### 3단계: uplink_app → 대상 앱 (CONFIG_CMD_MID)

**대상 앱(cfs_core_app/mavlink_bridge_app) 수신**
```
CONFIG_CMD_MID
  ├─ Seq, TimestampMs, SourceSequence
  ├─ PayloadLength
  └─ Payload[
      ├─ ConfigScope (1B)
      ├─ ConfigVersion (1B)
      ├─ ParameterId (2B, LE)
      ├─ ValueType (1B)
      ├─ ValueLength (1B)
      ├─ Checksum (2B)
      ├─ ValueBytes (N B)
    ]
```

**cfs_core_app ProcessConfigCommand (cfs_core_app_utils.c:523)**
```c
void CFS_CORE_APP_ProcessConfigCommand(Msg)
  Hdr = (ConfigPayloadHdr_t*)Msg->Payload;
  
  // 1. Scope 확인
  if (Hdr->ConfigScope != CFS_CORE_APP_CONFIG_SCOPE) return; // 다른 앱 → 무시
  
  // 2. Checksum 검증
  if (Hdr->Checksum != computed_checksum) {
    error_event; return; // ✗ 거부
  }
  
  // 3. Pending 버퍼에 기록
  CFS_CORE_APP_Data.PendingConfig[Msg->Seq % PENDING_BUF_SIZE] = *Hdr;
  CFS_CORE_APP_Data.ConfigPendingState = CONFIG_PENDING;
  
  // 4. 검증
  if (!CFS_CORE_APP_ValidateConfig(&CFS_CORE_APP_Data.PendingConfig[...])) {
    ConfigPendingState = CONFIG_REJECTED;
    return;
  }
```

---

### 4단계: Activation (활성화 경계)

**활성화 조건**

| 앱 | 활성화 경계 | 시점 |
|-----|-----------|------|
| mavlink_bridge_app | 다음 FC 스트림 처리 경계 | 즉시 (~ms) |
| lora_tdm_app | 다음 TDM 슬롯 경계 | ~슬롯 주기 |
| cfs_core_app | 운영자 승인 또는 안전 경계 | 수동/자동 |

**활성화 코드 예 (pseudo)**
```c
void CFS_CORE_APP_ActivateConfig(void)
  if (ConfigPendingState != CONFIG_PENDING) return;
  
  // Active ← Pending
  previous_config = active_config;        // 롤백용 백업
  active_config = pending_config;         // 활성화
  ConfigGeneration++;                     // 버전 증가
  ConfigPendingState = CONFIG_IDLE;       // 상태 업데이트
  LastConfigResult = CONFIG_SUCCESS;
```

---

### 5단계: 시간 흐름

```
T0: 지상국 CONFIG 송신
  ↓
T1: lora_tdm_app 수신 (LoRa 수신)
  - 프레임 CRC 검증 ✓
  - lora_tdm_app 파라미터 적용 (scope 3인 경우만)
  ↓
T2: uplink_app 수신 (SB CONFIG_CMD_MID)
  - Checksum 검증 ✓
  - 대상 앱으로 forward
  ↓
T3: cfs_core_app 수신 (SB CONFIG_CMD_MID)
  - Checksum 검증 ✓
  - pending_config에 기록
  ↓
T4~T5: 다음 활성화 경계까지 대기
  (scope 1인 cfs_core_app은 즉시 또는 운영자 승인 대기)
  ↓
T6: Active ← Pending (파라미터 적용)
  - active_config로 교체
  - ConfigGeneration++
  - HK telemetry에 반영
```

---

## 무결성 보장 메커니즘

```
┌─────────────────────────────────────────┐
│ 레이어 1: LoRa 물리 계층                 │
│ 무결성: 프레임 CRC-16/CCITT-FALSE       │
│ lora_tdm_app에서 검증                   │
└─────────────────────────────────────────┘
          ↓
┌─────────────────────────────────────────┐
│ 레이어 2: SB 메시지 계층                │
│ 무결성: CONFIG payload checksum         │
│ uplink_app + 대상 앱에서 검증           │
└─────────────────────────────────────────┘
          ↓
┌─────────────────────────────────────────┐
│ 레이어 3: 파라미터 검증 + 활성화         │
│ 안전성: pending/active 이중 버퍼         │
│ 각 앱의 활성화 경계에서 수행             │
└─────────────────────────────────────────┘
```

---

## 에러 처리

| 상황 | 처리 |
|------|------|
| LoRa 프레임 CRC 실패 | lora_tdm_app 거부, PendingUplinkFeedback=CRC_FAIL |
| CONFIG checksum 불일치 | 수신 앱 거부, ErrCounter++, COMMAND_ERR_EID event |
| ConfigScope 불일치 | 조용히 무시 (다른 앱 대상) |
| ConfigVersion 불일치 | 검증 실패, ConfigPendingState=REJECTED |
| 검증 실패 | active_config 유지, previous_config는 변경 없음 |
| 활성화 중 오류 | 롤백 가능 (previous_config ← active_config) |

---

## 상태 변수 흐름

```
IDLE
  ↓ [CONFIG_CMD_MID 수신]
PENDING
  ├─ [검증 성공] → [활성화 경계] → IDLE (ConfigGeneration++)
  └─ [검증 실패] → REJECTED → IDLE
```

**HK Telemetry (상태 추적)**
```
ConfigPendingState: 현재 상태 (IDLE/PENDING/REJECTED)
LastConfigResult: 최근 결과 (SUCCESS/FAILURE)
ConfigGeneration: 활성화 횟수 (증가값 = 몇 번 변경됐는가)
LastRollbackReason: 롤백 사유 (0=없음, 1~N=원인 코드)
```

---

## 주의사항

⚠️ **uplink_app 자신의 파라미터 변경 불가**
- ProcessConfigCommand 미구현 (설계상 제한적)
- 프로토콜 변경 필요 시 → fw 업데이트만 가능

⚠️ **ConfigScope 충돌 방지**
```
1 = cfs_core_app
2 = mavlink_bridge_app
3 = lora_tdm_app
(0, 4~ 미정의)
```

⚠️ **활성화 경계 안전성**
- 각 앱의 활성화 경계는 **데이터 레이스 없이** 원자적 교체 필요
- cfE_CRITERION 또는 spinlock 사용 권장
