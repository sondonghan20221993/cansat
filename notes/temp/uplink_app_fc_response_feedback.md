# uplink_app FC 응답 피드백 추가 (2026-07-15 발견)

## 문제

openMCT에서 uplink 명령 송신 시:
- 현재: "수신만 되면 적용됨"으로 표시 (uplink_app 검증 결과만 반영)
- 실제: FC가 MISSION_ACK 응답으로 accept/reject 알려주고 있음 → 지상국이 못 받음

**발견 경로**: 
1. openMCT 명령 → uplink_app 검증 ✓
2. uplink_app → lora_tdm_app → FC (LoRa 송신) ✓
3. FC → mavlink_bridge_app (MISSION_ACK 응답) ✓ 수신 완료
4. mavlink_bridge_app HkTlm.LastUploadResult ✓ FC 응답 저장
5. **uplink_app StatusTlm → openMCT ✗ FC 응답 미포함 → 지상국이 못 봄**

## 근본 원인

### 현재 UPLINK_STATUS_MID 필드 (spec 18.7)

✓ 있음:
- LastCommandResult (uplink_app 검증: 수락/거부/라우팅실패)
- LastConfigResult (구성 활성화 결과)

✗ 없음:
- FC의 MISSION_ACK 응답 결과
- 비행체 업로드 상태

## 해결 방안

### 1. spec 수정 (mission_app_runtime_spec.md 18.7)

UPLINK_STATUS_MID에 추가 필드:

```
- FcMissionResult: FC MISSION_ACK 응답
  값: MISSION_ACCEPTED(0) | MISSION_UNSUPPORTED_FRAME(2) | MISSION_DENIED(3) | TIMEOUT(4)
- FcMissionUploadState: 현재 FC 업로드 상태
  값: IDLE(0) | ACTIVE(1)
- FcMissionUploadSuccessCount: 누적 성공 업로드 수 (FC 응답 기준)
```

### 2. 코드 수정

#### 2.1 uplink_app_utils.c

- mavlink_bridge_app의 HK 구독 추가 (BRIDGE_HK_MID)
- 수신 시 LastUploadResult 캐시
- UPLINK_APP_UpdateStatusTelemetry()에서 FC 응답 필드 채우기

#### 2.2 default_uplink_app_msgstruct.h

UPLINK_APP_StatusTlm_t에 필드 추가:
```c
uint8  FcMissionResult;           // MISSION_ACK result
uint8  FcMissionUploadState;      // IDLE/ACTIVE
uint32 FcMissionUploadSuccessCount; // 누적 성공
```

#### 2.3 UT (coveragetest_uplink_app_utils.c)

- mavlink_bridge HK 구독 모킹
- FcMissionResult 업데이트 시나리오 추가
- StatusTlm 필드 검증

### 3. openMCT 표시 (지상국, 별도 저장소)

- fc_serial_ws_server.py에서 UPLINK_STATUS_MID 파싱 시 FC 응답 필드 노출
- UI에서 "FC 응답" 상태 표시 (ACCEPTED/DENIED/TIMEOUT)

## 상태 (완료, 2026-07-15)

- [x] spec 18.7 업데이트 (필드 정의 추가)
- [x] uplink_app HK 구독 구현 (BRIDGE_HK_MID Subscribe #4 추가)
- [x] StatusTlm 필드 추가 (FcMissionResult/FcMissionUploadState/FcMissionUploadSuccessCount)
- [x] UT 추가 — Init_Subscribe4Error, TaskPipe_BridgeHk, UpdateStatusTelemetry FC 필드 검증
- [x] 빌드/회귀 검증 — uplink_app 4개 러너 전량 PASS(235 TOTAL), 타 앱 회귀 없음
- [ ] 지상국(openMCT) 파싱 업데이트 (별도 저장소, fc_serial_ws_server.py)

## 구현 요약

```
shared_msgs/bridge_hk_msg.h (BRIDGE_HK_TLM_t)
  ├─ LastUploadResult, MissionUploadSuccessCount 이미 존재 (재사용)

uplink_app/config/default_uplink_app_msgid_values.h
  └─ BRIDGE_HK_MID_VALUE (0x08A0) 추가

uplink_app/config/default_uplink_app_msgstruct.h
  ├─ #include "bridge_hk_msg.h"
  ├─ typedef BRIDGE_HK_TLM_t UPLINK_APP_BridgeHkMirror_t
  └─ UPLINK_APP_StatusTlm_t 끝에 FcMissionResult/FcMissionUploadState/
     FcMissionUploadSuccessCount 필드 append (mirror 레이아웃 컨벤션 준수)

uplink_app/fsw/src/uplink_app.c
  └─ Init()에 BRIDGE_HK_MID Subscribe 추가 (#4)

uplink_app/fsw/src/uplink_app_dispatch.c
  └─ TaskPipe()에 BRIDGE_HK_MID 처리 분기 추가
     (LastUploadResult/MissionUploadSuccessCount 캐시, UploadState=ACTIVE)

uplink_app/fsw/src/uplink_app_utils.c
  └─ UPLINK_APP_UpdateStatusTelemetry()에서 캐시값을 StatusTlm에 반영
```

## 다음 단계 (별도 저장소, openMCT/지상국)

fc_serial_ws_server.py에서 UPLINK_STATUS_MID 파싱 시 FcMissionResult 등
3개 필드 파싱 추가 필요. 이 저장소 범위 밖.

## 노트

- 기존 `LastCommandResult` 필드는 유지 (uplink 검증 결과)
- FC 응답은 별도 필드로 추가 (명확한 구분)
- mavlink_bridge_app HK는 이미 MISSION_ACK를 추적 중 — 재사용만 하면 됨
- 대역폭: StatusTlm에 4바이트 추가 (uint8×2 + padding + uint32)
