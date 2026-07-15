# uplink 명령 처리 경로 역할 정의 (2026-07-15)

## 역할 분리

### lora_tdm_app = 링크 수신/중계 (ProcessUpFrame)

**입력**: LoRa serial (텍스트 프레임)
```
UP,version,class,seq,flags,payload_hex,crc
예: UP,1,2,100,0,DEADBEEF,A1B2
```

**처리**:
1. 전체 프레임 CRC-16/CCITT-FALSE 검증 (line 438-446)
   - "UP,..." 문자열에 대한 CRC 확인
   - 목적: LoRa 링크 무결성
2. hex payload 디코딩
3. LORA_TDM_APP_UplinkFwdCmd_t 구성
4. **Proxy Checksum 계산** (line 489-498)
   - Version+CommandClass+PayloadLength+Flags+Sequence(LE)+Payload에 대한 CRC-16/CCITT-FALSE
   - 목적: SB 메시지 변환 후 payload 손상 감지용
5. CFE_SB_TransmitMsg(UPLINK_APP_CMD_MID)

**출력**: UPLINK_APP_CMD_MID (Checksum 필드 포함)

**역할**: **링크 물리계층 → SB 메시지 변환** (포맷만 검증, 명령 유효성은 검증 안 함)

---

### uplink_app = 명령 검증/라우팅 (UPLINK_APP_ProcessUplink)

**입력**: UPLINK_APP_ProcessUplinkCmd_t (lora_tdm_app이 forward한 SB 메시지)

**처리**:
1. **Sequence 검증** (line 93-104)
   - 새 sequence > 마지막 수락된 sequence
   - 목적: replay attack 방지
2. **Proxy CRC 재검증** (line 106 → ValidateProxyCommand → line 156)
   - Version+CommandClass+PayloadLength+Flags+Sequence+Payload 재계산
   - 목적: SB 메시지 변환/전송 중 payload 손상 감지
3. **Authorization 검증** (line ~200)
   - CommandClass별 필요 auth level 확인
   - Flags의 auth bits 검증
   - Level 3 명령(RECOVERY/MODE)은 RequestToken 필수
4. **Health state 검증** (line 148-176)
   - FAILED/RECOVERY state: RECOVERY/DIAGNOSTIC만 허용
   - DEGRADED state: VIEWPOINT/CONFIG 차단
   - FORCE 플래그로 override 가능 (이벤트 기록)
5. **Route target 결정** (line 119-132)
   - CommandClass → CONFIG: cfs_core_app, ROUTE: cfs_core_app, MODE: cfs_core_app, RECOVERY: cfs_core_app, DIAGNOSTIC: cfs_core_app
   - 목적: 다음 명령 처리 앱 결정
6. SB 메시지로 대상 앱에 forward

**출력**: CONFIG_CMD_MID / MODE_CMD_MID / RECOVERY_CMD_MID / ROUTE_UPDATE_MID 등

**역할**: **SB 메시지 → 명령 검증 → 대상별 forward** (명령 유효성 검증 및 라우팅)

---

## 구조

```
LoRa serial
    ↓
[lora_tdm_app: 링크 수신]
  - 프레임 CRC 검증
  - payload 디코딩
  - Proxy Checksum 계산
  - → UPLINK_APP_CMD_MID
    ↓
[uplink_app: 명령 검증]
  - Sequence 검증
  - Proxy CRC 재검증
  - Authorization 검증
  - Health state 검증
  - Route target 결정
  - → CONFIG_CMD_MID / MODE_CMD_MID / ... (대상 앱별)
    ↓
[cfs_core_app / mavlink_bridge_app / ...]
  - 최종 명령 처리
```

## 중복 제거 여부

**결론: 중복이 아님**

각 단계의 CRC/Checksum 검증은 서로 다른 목적:
- **lora_tdm_app CRC**: LoRa 링크 무결성 (serial I/O 손상 감지)
- **uplink_app Proxy CRC**: SB 메시지 변환/전송 무결성 (SB 버스 손상 감지)

분리 이유:
1. 물리계층(LoRa) ↔ SB 계층의 경계에서 손상 감지 필요
2. 각 계층의 독립적 오류 처리
3. uplink_app이 다른 uplink 소스(USB-C 등)로 확장될 때 재사용 가능

**실제 중복 제거 대상**: config payload checksum 검증만
- lora_tdm_app: ConfigChecksum 검증 O
- uplink_app: ConfigChecksum 검증 X (추가 필요)
- mavlink_bridge_app: ConfigChecksum 검증 X (추가 필요)
- cfs_core_app: ConfigChecksum 검증 ? (확인 필요)
