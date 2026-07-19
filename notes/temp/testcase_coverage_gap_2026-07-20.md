# 추가 testcase 구현 항목 (2026-07-20 도출)

TEST_CASES.md 기준, 코드 공개 함수 전수 ↔ 테스트 함수 전수 대조로 선별.
"미구현/미검증 항목" 표(TEST_CASES.md 802행)에는 없던 신규 발견분.

## A. 지금 작성 가능 (순수 단위테스트, 하드웨어 불필요)

- [x] A-1. `LORA_TDM_APP_ReportHousekeeping` — HK payload 필드 반영 검증 (2026-07-20 완료)
      `coveragetest_lora_tdm_app.c`에 `Test_ReportHousekeeping` 추가 (16 assertion).
      **주의**: 최초 `coveragetest_lora_tdm_app_utils.c`에 작성했다가 전부 FAIL —
      cFE coveragetest 구조상 각 `coveragetest_<unit>.c`는 대응하는 실제 `<unit>.c`만
      링크하고 나머지는 stub으로 대체됨. `ReportHousekeeping`은 `lora_tdm_app.c`
      소속이라 `_utils.c` 빌드에서는 stub 호출이라 검증 불가 — `coveragetest_lora_tdm_app.c`로
      이동 후 16/16 PASS. 검증: `coverage-lora_tdm_app-lora_tdm_app-testrunner` 56/56 PASS,
      4앱 전체 UT 16/16 바이너리 PASS(회귀 없음).
      (부수 발견: `~/cFS_clean/apps/*`가 저장소와 별도 사본이라 빌드 전 rsync 동기화 필요 —
      지난 세션 shared_msgs 리팩터도 이 빌드 트리에는 미반영 상태였음, 이번에 함께 동기화)
- [x] A-2. `LORA_TDM_APP_ReportLinkStatus` — LinkStatus TLM(Seq/LinkState/카운터) 반영 검증 (2026-07-20 완료)
      동일 파일에 `Test_ReportLinkStatus` 추가 (8 assertion). 64/64 PASS, 4앱 UT 16/16 회귀 없음.
- [x] A-3. `MAVLINK_BRIDGE_APP_RequestTelemetryStreams` — 스트림 요청 경로 테스트 (2026-07-20 완료)
      `coveragetest_mavlink_bridge_app_utils.c`에 socketpair 기반 2건 추가:
      `Test_RequestTelemetryStreams_SendsSixStreamRequests` (TargetSystemId!=0 → COMMAND_LONG
      45B 프레임 6개 순서대로 전송 확인, 17 assertion), `Test_RequestTelemetryStreams_SkippedWhenNoTarget`
      (TargetSystemId==0 → 전송 없음, 3 assertion). 156/156 PASS, 4앱 UT 16/16 회귀 없음.
- [x] A-4. cfs_core_app 재시작 로직 변형 비대칭 (2026-07-20 완료)
      Lora 재시작에 `Test_CFS_CORE_APP_LoraRestart_GetAppIdFail`,
      `Test_CFS_CORE_APP_LoraRestart_ResetOnRecovery` 추가 — Bridge/Uplink와 동일 4변형 확보.
      249/249 PASS(해당 바이너리), 4앱 UT 16/16 회귀 없음.

## B. 경미 (선택)

- [x] B-1. `UPLINK_APP_Init` 실패주입 — `Subscribe2Error` 결손 (2026-07-20 재확인, 갭 아님)
      재확인 결과 `Test_UPLINK_APP_Init_SubscribeError`가 `CFE_SB_Subscribe` 2번째 호출
      실패를 이미 커버(`UT_SetDeferredRetcode(..., 2, ...)`) — 이름만 `Subscribe2Error`가
      아니라 `SubscribeError`로 지어져 grep에서 누락으로 오판됨. 1/2/3/4 전 호출 커버 확인,
      코드 변경 불필요.

## 참고 (테스트 코드는 있으나 문서 미반영 — 착수 불필요, 문서만 정정)

- `test_lora_downlink_decoder.py` 실제 23개 테스트 함수 존재.
  TEST_CASES.md 422행 표에는 1개(`test_systime_flag_set_but_block_missing_returns_none_not_crash`)만 기재.
  → `tests/TEST_CASES.md` "그룹 A" 표·해당 섹션에 나머지 22개(basic_roundtrip/systime_block/
  saturation_flag/byte_by_byte_feed/crc_corruption_resync/v1_v2_mixed_stream 등) 추가 필요.

## C. E2E(B) 하네스 구축 — 별도 규모 작업 (미착수)

- [ ] C-1. B-2~B-4: WSL x86 cFS 빌드 + pytest PTY fixture + CI_LAB 주입/EVS 관측 헬퍼
      TEST_CASES.md에 계획만 있고 미착수. 이게 되어야 아래가 풀림:
      - skip 중: `test_rec_serial.py`(REC-001~004), `test_uplink_e2e.py`, `test_lora_fc_downlink_e2e.py`(lora_tdm 대상 재작성 필요)
      - 미작성 파일: `test_mavlink_bridge_e2e.py`, `test_cfs_core_health_e2e.py`
      - E2E로만 판정 가능: RT-FC-007/008, RT-CORE-001/002, RT-LORA-002/003, RT-UPL-001~006

## D. Pi 하드웨어 런타임 시험 (사용자 직접, 코드 작업 아님)

- [ ] D-1. RT-CORE-003/004 재시작 실측 (`tools/runtime_app_restart_test.sh`, 재빌드+cfs.service 재기동 선행)
- [ ] D-2. TDM-RT-001~009, RT-LORA-001/004, RT-DL2-SYSTIME-001 (실물 LoRa 필요)
- [ ] D-3. 통합 순차 세션 7단계 (TEST_CASES.md 1008행)

## E. 문서 정정 (소규모, 코드 작업 아님)

- [x] E-1. TEST_CASES.md 862행 "스트림 요청 ✓" 과대표기 정정 — A-3 처리 시 완료
- [x] E-2. decoder 테스트 22건 누락 기재 (2026-07-20 완료) — 표를 1건→23건 전체로 갱신
- [x] E-3. 매트릭스의 e2e 파일 "⏸️ pytest.skip()" 표기 재확인·부분 정정 (2026-07-20)
      실제 코드 확인 결과: `test_rec_serial.py`, `test_lora_fc_downlink_e2e.py`는 진짜
      무조건 `pytest.skip()` 스텁 — 표기 정확, 유지. `test_uplink_e2e.py`는 실제로 `--cfs`
      fixture 게이트(cfs_host/cfs_port)로 조건부 실행되는 구현된 테스트 — "⏸️ pytest.skip()"은
      stale 표기였음. 매트릭스 871~873행, 907~908행 정정.

## 착수 우선순위

1. A-1, A-2 (lora_tdm HK/LinkStatus) — 가장 간단, 기존 `coveragetest_lora_tdm_app_utils.c` 패턴 재사용
2. A-3 (mavlink RequestTelemetryStreams) — 동시에 TEST_CASES.md 862행 표기 오류 정정
3. A-4 (lora 재시작 변형 보강) — cfs_core 기존 Bridge/Uplink 테스트 패턴 복제
4. B-1, E-1~E-3 문서 정정 — 여력 있을 때
5. C-1 (E2E 하네스) — 별도 세션 규모, 단계적 진행
6. D-1~D-3 (Pi 런타임) — Pi 앞에서 일괄 실행
