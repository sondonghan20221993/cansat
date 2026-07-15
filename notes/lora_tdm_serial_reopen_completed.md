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

- [x] 오류 시 close+fd=-1 구현 (2026-07-10) — `CloseSerial()` 헬퍼 추가,
      `RunTx` write 실패 및 `RunRxWindow` read 실제오류(EINTR/EAGAIN 제외) 경로에서 호출.
      read `Rc<=0` 분기를 `Rc==0`(정상 무데이터) / `Rc<0`(errno 판별) 로 분리.
      SERIAL_READ_ERR_EID(9)도 실오류 시 송출하도록 추가.
- [x] coveragetest 추가 (2026-07-13) — `RunTx`/`RunRxWindow`가 static이라 리팩터를 고려했으나,
      대신 실제 POSIX fd를 조작해 공개 진입점 `LORA_TDM_APP_RunCycle()`을 통해 간접 검증:
      `Test_RunCycle_TxWriteFailClosesFd`(닫힌 fd → write EBADF), `Test_RunCycle_RxReadFailClosesFd`
      (write-only fd → read EBADF, `/dev/null` 대상이라 write는 성공해 TX가 먼저 안 닫음).
      두 경로 모두 `LoRaFd==-1` 확인, PASS.
      단위 테스트 중 두 가지 숨은 전제 발견/처리:
      (1) 이 unit(`lora_tdm_app.c`)만 단독 빌드 시 `BuildFcDownlinkLine`/`BuildShDownlinkLine`
          (다른 파일 소속)은 stub 처리되어 기본 반환값 0 → `UT_SetDefaultReturnValue`로 양수
          길이 명시 필요.
      (2) `CFE_TIME_GetTime` 기본 stub은 호출마다 +1000ms 자동 증가 → RX_WINDOW_MS(<1000ms)
          데드라인이 read() 호출 전에 항상 지나버림 → `UT_SetDataBuffer`로 고정 시각 주입 필요.
      lora_tdm_app UT 전체 회귀 없음 (main 9, utils 49, cmds 8, dispatch 20 — 전부 PASS).
- [x] 빌드 검증 (2026-07-10, Pi `~/cFS_clean`, GCC 14.2.0 native) — UT 빌드 에러·경고 0,
      coverage 전체 78 PASS/0 FAIL (`lora_tdm_app` 3, `_utils` 47, `_cmds` 8, `_dispatch` 20).
      런타임 `build/` 재빌드·재시작(systemd `cfs.service`) 후 기동 회귀 없음 — lora_tdm 시리얼 오류 무.
- [ ] RT-LORA-001 실물 검증 (물리 USB 분리 → 재오픈 후 TxCount 재개 확인) — Pi 물리 접근 시 수동
