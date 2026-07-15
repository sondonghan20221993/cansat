# CONFIG_CMD_MID checksum 검증 강화 (2026-07-15)

## 현황

ConfigChecksum 검증이 **lora_tdm_app에만 구현됨** (lora_tdm_app_utils.c:694-701)
- uplink_app: CONFIG 명령 forward만 함, 검증 없음
- mavlink_bridge_app: CONFIG 명령 수신, 검증 없음

**위험**: uplink_app/SB 메시지 처리 과정에서 payload 손상 시 미감지

## ConfigChecksum 정의 (lora_tdm_app_utils.c:640-656)

```c
uint16 ConfigChecksum(const LORA_TDM_APP_ConfigPayloadHdr_t *Hdr, 
                      const uint8 *ValueBytes, uint8 ValueLength)
{
    uint16 Sum = 0;
    Sum += Hdr->ConfigScope;         // 1B
    Sum += Hdr->ConfigVersion;       // 1B
    Sum += (Hdr->ParameterId & 0xFF);       // 2B (LE)
    Sum += ((Hdr->ParameterId >> 8) & 0xFF);
    Sum += Hdr->ValueType;           // 1B
    Sum += Hdr->ValueLength;         // 1B
    for (i = 0; i < ValueLength; i++)
        Sum += ValueBytes[i];        // N B
    return Sum;  // uint16 (wrapping OK)
}
```

## 해결 방안

### 1. spec 수정 (mission_app_runtime_spec.md)

CONFIG_CMD_MID 섹션에 checksum 검증 정책 추가:
```
§ CONFIG_CMD_MID Checksum 검증

각 앱(uplink_app, cfs_core_app, mavlink_bridge_app)은 CONFIG 명령 수신 시:
1. ConfigPayloadHdr 파싱
2. ConfigScope 검증 (자신의 scope 확인)
3. ConfigVersion 검증
4. **ConfigChecksum 검증**: additive sum (ConfigScope+ConfigVersion+ParameterId_LE+ValueType+ValueLength+ValueBytes[0..N])
5. ConfigScope 불일치 → 조용히 무시 (다른 앱 대상)
6. Checksum 불일치 → 에러 이벤트 발생, 거부
```

### 2. 코드 수정

#### 2.1 공용 함수 추가 (shared_msgs 또는 config_msg.h)

```c
// 추가될 함수 (config_msg.h 또는 config_validation.h)
uint16 CONFIG_ComputeChecksum(const CONFIG_PAYLOAD_HDR_t *Hdr, 
                              const uint8 *ValueBytes, uint8 ValueLength);
```

#### 2.2 uplink_app (uplink_app_cmds.c)

CONFIG 명령 처리 시 (UPLINK_APP_CLASS_CONFIG 경로):
```c
// 기존 forward 전:
if (!CONFIG_ValidateChecksum(CmdPayload)) {
    Result = UPLINK_APP_RESULT_REJECT_CONFIG_CHECKSUM;
    // reject 처리
}
```

#### 2.3 mavlink_bridge_app (mavlink_bridge_app.c 또는 dispatch)

CONFIG_CMD_MID 처리 시 (cfs_core_app과 동일 패턴):
```c
void MAVLINK_BRIDGE_APP_ProcessConfigCommand(...) {
    if (!CONFIG_ValidateChecksum(ConfigPayload)) {
        // error handling, EID 발생
        return;
    }
    // 기존 처리 계속
}
```

#### 2.4 cfs_core_app (동일 패턴, 이미 있을 수 있음)

존재 여부 확인 필요

### 3. UT 추가 (각 app별)

- [ ] ConfigChecksum 계산 UT
- [ ] ConfigChecksum 불일치 → 거부 UT
- [ ] ConfigScope 불일치 → 무시 UT
- [ ] ConfigVersion 불일치 → 에러 UT
- [ ] PayloadLength 초과 → 에러 UT

## tradeoff 검토

**성능**: ConfigChecksum은 additive sum (N byte loop) → ~마이크로초 (무시할 수준)
**대역폭**: 검증 로직만 추가, 메시지 크기 변화 없음
**복잡도**: 기존 lora_tdm_app 로직 재사용 (복사 또는 공용 함수)

**결론**: 비용 무시할 수준, 즉시 수행 권장

## 상태

- [ ] spec 수정 (CONFIG_CMD_MID 검증 정책 명시)
- [ ] 공용 checksum 함수 생성 또는 inline 정의
- [ ] uplink_app CONFIG 처리에 검증 추가
- [ ] mavlink_bridge_app CONFIG 처리에 검증 추가
- [ ] cfs_core_app 현황 확인 (이미 검증하는지)
- [ ] UT 추가 (4건 이상 per app)
- [ ] 빌드/회귀 검증
