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
