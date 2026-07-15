# uplink_app CONFIG_CHECKSUM UT 테스트 계획 (2026-07-15)

## 추가할 TC

### Test_UPLINK_APP_ForwardConfigCommand_ChecksumValid
- 목적: 올바른 checksum으로 config forward 성공 검증
- 입력: valid checksum, PayloadLength correct
- 예상: 명령 forward 성공, ConfigPendingState=IDLE

### Test_UPLINK_APP_ForwardConfigCommand_ChecksumInvalid
- 목적: 잘못된 checksum 거부
- 입력: checksum mismatch (1 byte 변조)
- 예상: false 반환, ErrCounter++, event COMMAND_ERR_EID

### Test_UPLINK_APP_ForwardConfigCommand_PayloadTooShort
- 목적: ConfigPayloadHdr_t 크기보다 작은 payload 거부
- 입력: PayloadLength < sizeof(UPLINK_APP_ConfigPayloadHdr_t)
- 예상: false 반환, ErrCounter++

### Test_UPLINK_APP_ForwardConfigCommand_InvalidValueLength
- 목적: ValueLength != sizeof(uint32) 거부
- 입력: Hdr->ValueLength != 4
- 예상: false 반환, ErrCounter++

### Test_UPLINK_APP_ForwardConfigCommand_PayloadOverflow
- 목적: Payload buffer overflow 방지
- 입력: (sizeof(Hdr) + ValueLength) > PayloadLength
- 예상: false 반환

---

## 예상 커버리지 변화

| 함수 | 기존 | 변경 후 | TC수 |
|------|------|--------|------|
| UPLINK_APP_ForwardConfigCommand | ~60% | 100% | 5 |
| UPLINK_APP_ConfigChecksum | N/A | 100% | 1 |

## 상태 (완료, 2026-07-15)
- [x] coveragetest_uplink_app_utils.c에 5개 TC 추가
- [x] 빌드 테스트 (~/cFS_clean native UT) — 에러 0건
- [x] UT 실행 — 5개 TC 전부 PASS
- [x] 회귀 확인 — uplink_app 전체 4개 러너 227 TOTAL PASS (cmds 91, dispatch 29, app 9, utils 98)

## 실행 중 발견/수정
- ChecksumValid 테스트에서 forward 성공 후 `ConfigPendingState`가
  `UPLINK_APP_CONFIG_PENDING`이 아니라 `UPLINK_APP_CONFIG_IDLE`로 리셋됨을 확인
  (uplink_app_utils.c:424 — SB 전송 성공 시 IDLE로 즉시 복귀).
  테스트 어서션을 IDLE 기대값으로 수정.
