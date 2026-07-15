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
