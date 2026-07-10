# lora_tdm serial 오류 시 재오픈 미구현 갭 (2026-07-10 도출)

## 문제

`lora_tdm_app`은 serial write/read 오류 시 **fd를 닫지 않아 재오픈이 트리거되지 않는다**.

- write 실패: `SERIAL_WRITE_ERR_EID(8)` EVS만 송출, fd 유지 (`lora_tdm_app.c:184`)
- 재오픈 조건: `LoRaFd < 0`일 때만 `OpenSerial()` 재시도 (`lora_tdm_app.c:256-259`)
- USB 모듈 런타임 분리 시 fd는 유효값을 유지 → **영구 write 실패 루프**, 자동 복구 불가

대비: mavlink_bridge_app은 read 실패 시 `CloseSerial()` 호출로 fd 정리 후
다음 사이클에 재연결 (`mavlink_bridge_app_utils.c:1873-1879`) — 동일 패턴 적용 가능.

## 필요 수정

write/read 오류 감지 시:

```c
close(LORA_TDM_APP_Data.LoRaFd);
LORA_TDM_APP_Data.LoRaFd = -1;   /* 다음 RunCycle에서 OpenSerial() 자동 재시도 */
```

- 연속 오류 판단(1회 오류 즉시 close vs N회 누적 후 close)은 구현 시 결정
- coveragetest: write 실패 → fd == -1 확인 케이스 추가

## 관련 항목

- `tests/TEST_CASES.md` RT-LORA-001 (LoRa USB 런타임 분리) — 이 갭 해소가 선행 조건
- TDM-RT-009 (serial open 실패 후 재시도)는 **초기 open 실패**만 커버, 런타임 분리는 미커버

## 상태

- [ ] 오류 시 close+fd=-1 구현
- [ ] coveragetest 추가
- [ ] RT-LORA-001 실물 검증
