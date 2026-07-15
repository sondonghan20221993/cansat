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
