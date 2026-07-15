# CONFIG_CMD_MID 체크섬 검증 — 5개 UT 케이스 미작성

## 배경

`config_checksum_validation_completed.md`에서 체크섬 기능을 설계·구현했는데,
단위테스트는 **5개 케이스가 아직 등록되지 않음**:

## 미작성 UT (from coveragetest_uplink_app_utils.c 체크리스트)

- [ ] `Test_UPLINK_APP_ForwardConfigCommand_ChecksumValid` 
- [ ] `Test_UPLINK_APP_ForwardConfigCommand_ChecksumInvalid`
- [ ] `Test_UPLINK_APP_ForwardConfigCommand_PayloadTooShort`
- [ ] `Test_UPLINK_APP_ForwardConfigCommand_InvalidValueLength`
- [ ] `Test_UPLINK_APP_ForwardConfigCommand_PayloadOverflow`

## 현황

- [x] 체크섬 계산 함수 구현 (`UPLINK_APP_ConfigChecksum()`)
- [x] 검증 로직 구현 (`UPLINK_APP_ForwardConfigCommand()` 내부)
- [ ] 5개 시나리오 UT 추가
- [ ] uplink_app 커버리지 91%→94% 상향
- [ ] 전체 UT 회귀 확인

## 참고

- `uplink_app/fsw/src/uplink_app_utils.c:410~430` (체크섬 검증 로직)
- `uplink_app/unit-test/coveragetest/coveragetest_uplink_app_utils.c` (UT 파일)
