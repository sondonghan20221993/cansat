# 통합테스트 검증 계획

> 대상: Phase 1~3 구현 통합 검증  
> 환경: 실 하드웨어 또는 SITL  
> 타이밍: 배포 전 최종 검증

---

## 1. 부팅 Fail-Safe 검증 (Phase 3.1)

### 시나리오 1.1: 부팅 직후 health 미수신 상태

**절차**:
1. cFS 부팅
2. `SYSTEM_HEALTH_MID` 수신 전에 uplink 명령 전송 (예: DIAGNOSTIC)
3. lora_tdm_app → uplink_app 수신 확인

**기대 결과**:
```
✓ uplink 명령 거부 (REJECT_STATE)
✓ EVS: "command blocked (no health yet)"
✓ UPLINK_STATUS_MID RejectedCount 증가
```

**확인 방법**:
- EVS 로그 또는 cFS telemetry 모니터링
- UPLINK_STATUS_MID 값 확인
- 타이밍: SYSTEM_HEALTH_MID 수신 전 0~1초 사이

---

### 시나리오 1.2: Health 수신 후 정상 동작

**절차**:
1. cFS 부팅 완료, SYSTEM_HEALTH_MID 수신 확인 (health 상태 = NOMINAL)
2. 동일한 명령 재전송

**기대 결과**:
```
✓ uplink 명령 수락 (ACCEPTED)
✓ 라우팅 진행
✓ AcceptedCount 증가
```

---

### 시나리오 1.3: RECOVERY/FAILED 상태 데드락 해제

**절차**:
1. cFS 상태를 FAILED 또는 RECOVERY로 강제 설정 (또는 bridge 타임아웃 발생 대기)
2. RECOVERY 명령 전송

**기대 결과**:
```
✓ RECOVERY 명령 수락 (이전: 거부 → 현재: 허용)
✓ 데드락 해제
✓ EVS: recovery 명령 처리 이벤트
```

---

## 2. 권한 검증 (Phase 3.2)

### 시나리오 2.1: 낮은 권한 → 높은 권한 필요 명령 = 거부

**절차**:
1. 명령 생성: Flags[7:6] = 1 (Level 2)
2. CommandClass = RECOVERY (Level 3 필요) 전송

**기대 결과**:
```
✓ 명령 거부
✓ EVS: "command blocked (insufficient auth) auth=1 required=3"
✓ UPLINK_STATUS_MID RejectedCount 증가
```

**확인 방법**:
- `tools/uplink_route_update_sender.py` Flags 수정하여 전송
- EVS 로그에서 AUTHZ_BLOCK_EID 확인

---

### 시나리오 2.2: 같은 권한 = 허용

**절차**:
1. Flags[7:6] = 2 (Level 3)
2. CommandClass = RECOVERY 전송

**기대 결과**:
```
✓ 명령 수락
✓ AcceptedCount 증가
```

---

### 시나리오 2.3: 높은 권한 → 낮은 권한 필요 명령 = 허용

**절차**:
1. Flags[7:6] = 2 (Level 3)
2. CommandClass = DIAGNOSTIC (Level 1) 전송

**기대 결과**:
```
✓ 명령 수락 (권한 역전: OK)
✓ 진단 명령 실행
```

---

### 시나리오 2.4: Level 3 명령 + RequestToken = 0 = 거부

**절차**:
1. Flags[7:6] = 2 (Level 3)
2. RECOVERY 명령 생성 시 RequestToken = 0 설정
3. 전송

**기대 결과**:
```
✓ 명령 거부
✓ EVS: "insufficient auth" 또는 "request_token 필수"
✓ RejectedCount 증가
```

---

### 시나리오 2.5: Level 3 명령 + RequestToken ≠ 0 = 허용

**절차**:
1. Flags[7:6] = 2 (Level 3)
2. RECOVERY 명령, RequestToken = 0x12345678
3. 전송

**기대 결과**:
```
✓ 명령 수락
✓ RecoveryCmdTlm_t에 RequestToken echo 확인
```

---

## 3. 시퀀스 거부 피드백 (Phase 3.3)

### 시나리오 3.1: 시퀀스 거부 → SEQ_FAIL 피드백

**절차**:
1. 첫 uplink 명령 전송: Sequence = 100
2. 두 번째 명령: Sequence = 50 (역행, 거부 예상) 전송
3. 다음 downlink 패킷의 UFB 확인

**기대 결과**:
```
✓ UPLINK_STATUS_MID: LastCommandResult = REJECT_SEQUENCE
✓ lora_tdm_app이 UPLINK_STATUS 구독
✓ PendingUplinkFeedback = UFB_SEQ_FAIL (2)
✓ 다음 downlink TDM 패킷에 "UFB=2" 포함
✓ 지상국에서 UFB=2 수신 → "시퀀스 거부" 해석
```

**확인 방법**:
- LoRa downlink 패킷 분석 (시리얼 로그 또는 패킷 스니핑)
- TDM 패킷 형식: `FC,<seq>,<ts>,...,<ufb>\n` 에서 `<ufb>` = 2 확인
- 지상국 로그 분석

---

### 시나리오 3.2: CRC 오류 → CRC_FAIL 피드백 (기존)

**절차**:
1. 의도적으로 손상된 패킷 전송 (CRC 변조)

**기대 결과**:
```
✓ uplink_app: 길이/CRC 검증 실패
✓ PendingUplinkFeedback = UFB_CRC_FAIL (1)
✓ downlink: "UFB=1"
```

---

### 시나리오 3.3: 정상 명령 → OK 피드백

**절차**:
1. 유효한 순차 명령 전송 (Sequence 단조 증가)

**기대 결과**:
```
✓ UPLINK_STATUS_MID: LastCommandResult = SUCCESS (0)
✓ PendingUplinkFeedback = UFB_OK (0)
✓ downlink: "UFB=0"
```

---

## 4. A-3 명령 통합 동작 (Phase 1)

### 시나리오 4.1: RECOVERY 명령 실행

**절차**:
1. 지상국에서 `tools/uplink_route_update_sender.py` 변종으로 RECOVERY 명령 생성
2. 적절한 권한 + RequestToken 설정
3. 전송

**기대 결과**:
```
✓ uplink_app: ForwardRecoveryCommand() 실행
✓ RECOVERY_CMD_MID 발행
✓ cfs_core_app: ProcessRecoveryCommand() 처리
✓ EVS: recovery action 로그 (예: "bridge restart request")
```

---

### 시나리오 4.2: MODE 명령 상태 전이

**절차**:
1. MODE 명령: ENTER_RECOVERY
2. 현재 상태 = NOMINAL 확인
3. 전송

**기대 결과**:
```
✓ 상태 전이: NOMINAL → RECOVERY
✓ MODE_CMD_MID 발행
✓ LastModeRequestToken 저장
✓ EVS: "mode command accepted" 등
```

---

### 시나리오 4.3: DIAGNOSTIC 명령 실행

**절차**:
1. DIAGNOSTIC 명령: LINK_STATUS
2. 전송

**기대 결과**:
```
✓ 명령 실행 즉시 (pending buffer 없음)
✓ EVS: "DIAGNOSTIC_CMD_EID" + 링크 상태 요약
✓ DiagnosticCmdTlm_t 발행 (RequestToken echo)
```

---

## 5. 통합 회귀테스트 (Regression Test)

### 시나리오 5.1: 기존 ROUTE_UPDATE 동작 확인

**절차**:
1. 유효한 경로 업데이트 전송

**기대 결과**:
```
✓ 경로 검증 통과
✓ ROUTE_UPDATE_MID 발행
✓ 기존 동작 변화 없음
```

---

### 시나리오 5.2: 기존 RUNTIME_CONFIG 동작 확인

**절차**:
1. 유효한 구성 명령 전송

**기대 결과**:
```
✓ pending buffer에 저장
✓ 기존 동작 변화 없음
```

---

## 6. CI_LAB 보안 확인 (Phase 3.4)

### 시나리오 6.1: 배포 빌드에서 CI_LAB 비활성

**절차**:
1. 배포 빌드(cpu1_cfe_es_startup.scr) 사용
2. localhost:1234로 명령 전송 시도

**기대 결과**:
```
✓ CI_LAB 비활성 (앱 미로드)
✓ UDP 1234 경로 응답 없음
```

---

### 시나리오 6.2: 테스트 빌드에서 CI_LAB 활성

**절차**:
1. 테스트 빌드(cpu1_cfe_es_startup_test.scr) 사용
2. localhost:1234로 명령 전송

**기대 결과**:
```
✓ CI_LAB 로드됨
✓ UDP 1234 경로 정상 작동
✓ `tools/query_fc_mission.py` 실행 가능
```

---

## 7. 테스트 체크리스트

| # | 항목 | 시나리오 | 상태 |
|---|---|---|---|
| 1 | Fail-safe boot | 1.1~1.3 | ⏳ |
| 2 | 권한 검증 | 2.1~2.5 | ⏳ |
| 3 | SEQ_FAIL 피드백 | 3.1~3.3 | ⏳ |
| 4 | A-3 명령 | 4.1~4.3 | ⏳ |
| 5 | 회귀 테스트 | 5.1~5.2 | ⏳ |
| 6 | CI_LAB 보안 | 6.1~6.2 | ⏳ |

**총**: 19개 시나리오

---

## 8. 통합테스트 실행 환경

### 필수
- cFS 실행 환경 (SITL 또는 실 하드웨어)
- LoRa 시뮬레이터 또는 mock uplink (UDP/UART)
- EVS 로그 캡처
- telemetry 모니터링 (OpenMCT 또는 standalone viewer)

### 선택
- 패킷 스니퍼 (downlink UFB 확인용)
- 시리얼 로그 분석

---

## 9. 성공 기준

모든 19개 시나리오 **PASS** → 배포 승인 가능

| 기준 | 상태 |
|---|---|
| Fail-safe boot 작동 | ✓ |
| 권한 검증 체계 작동 | ✓ |
| SEQ_FAIL 피드백 전달 | ✓ |
| A-3 명령 정상 동작 | ✓ |
| 기존 기능 회귀 없음 | ✓ |
| 보안 정책 적용 | ✓ |

---

## 10. 현재 상태

| 항목 | 상태 |
|---|---|
| ✅ 테스트 계획 수립 | 19개 시나리오 설계 |
| ⏳ 테스트 환경 구성 | SITL/실 하드웨어 필요 |
| ⏳ 테스트 실행 | 배포 전 최종 단계 |
| ⏳ 결과 기록 | 각 시나리오별 PASS/FAIL |
