# BL-15 Stage 4 (150ms/100ms) 실측 진행 상황 (2026-07-22)

## 배경

`lora_downlink_5hz_cap_2026-07-21.md`/`lora_stage_measurement_runbook.md`에서
Stage 3(200ms/5Hz)까지 실측 완료됐고, BL-15 결정(2026-07-21 17:17)에 따라
200ms 미만 구간(Stage 4)의 단계적 실측을 진행 중.

## Pi 재배포 (BL-15와 별개로 선행됨)

Stage 4 시도 중 Pi의 `cFS_clean`이 이번 세션 변경사항(BL-08/17/19/11 등)을
전혀 반영 못 하고 있던 것 발견 — `shared_msgs` 자체가 없어 빌드 실패.
`shared_msgs`/`uplink_app`/`cfs_core_app`/`mavlink_bridge_app`/`lora_tdm_app`
전부 Pi로 rsync 후 재빌드·재설치 완료(2026-07-22 15:49).

**빌드/배포 절차 정리** (기존 `build_environment.md`에 없던 정보):
```bash
cd cFS_clean/build/native/default_cpu1
make                                          # 빌드
make DESTDIR=/home/sdh2983/cFS_clean/build install   # 설치(CMAKE_INSTALL_PREFIX=/exe라 DESTDIR 필수)
```
주의: 최상위 `cd cFS_clean && make`는 **다른 빌드 트리**(`build-native_std`)를
새로 cmake 초기화하므로 `build/` 트리를 직접 빌드하려면 위 방식으로 우회해야 함.

`core-cpu1`은 root 권한 필요(`sudo`, 비밀번호 필요) — 재시작 시 사용자 상호작용 필요.

## Stage 4a (150ms) — ✅ 완료, PASS

**설정**: `CYCLE_PERIOD_MS=150`, `RX_WINDOW_MS=70`, `LINK_LOSS_THRESHOLD=33`
(코드: `lora_tdm_app/config/default_lora_tdm_app_mission_cfg.h`, 아직 임시값
상태로 남아 있음 — Stage 4 전체 완료 전까지 원복하지 않음)

**절차**: 헤더 수정 → 로컬 UT 회귀(4종 PASS) → Pi rsync → 빌드/설치 →
`sudo pkill core-cpu1` → 재시작 → 지상국(`fc_serial_ws_server.py`, Windows PC,
`/mnt/c/Users/sdh97/Documents/GitHub/openMCT`) 별도 기동 → CONFIG 명령으로
`lora_tdm.downlink_protocol=1`(v2/DL2) 전환 → 5분 이상 soak.

**결과** (`telemetry_20260722_154720.csv`, `analyze_downlink_csv.py --cycle-ms 150`):
- 통합 seq gap 손실률: **0.0%** (6415/6415 expected/received)
- RX p95가 참고 RX창을 초과하는 패턴 재현(Stage 2/3와 동일, 지표 정의 이슈로
  기존에 이미 인지됨 — 비차단)
- `uplink_fb` 분포에서 `3`(STATE_BLOCKED)이 약 42% — 최초엔 이상 신호로
  보였으나 **사용자 확인: 벤치 테스트 환경에 GPS 미연결이라 헬스게이트가
  정상적으로 차단한 것** (150ms 자체가 유발한 문제 아님, 환경 요인)

**판정: PASS** — 150ms(≈6.67Hz)는 실측 근거로 확정 가능.

## Stage 4b (100ms) — 미착수

**제안 설정**: `CYCLE_PERIOD_MS=100`, `RX_WINDOW_MS=50`, `LINK_LOSS_THRESHOLD=50`
(사용자 확인 대기 중, 아직 반영 안 함)

## 조사 필요 — 미해결 (2026-07-22, 중단 시점)

지상 CONFIG 명령으로 `lora_tdm.downlink_protocol=1`을 보내 v2(DL2) 전환을
승인(`[OK] CONFIG accepted seq=1`)했는데도, 이후 Pi 로그(`/tmp/cfs_run.log`)에
v1 ASCII "ACK,<seq>" 형식의 "ACK seq mismatch" 이벤트가 계속 찍힘.

**확인된 사실**:
- `UseV2Downlink`는 `lora_tdm_app.c:262`(`RunTx`)의 다운링크 **송신** 포맷만
  결정 — RX 측 `ProcessRxLine`의 "ACK,<seq>" 파싱은 `UseV2Downlink`와 무관하게
  항상 동작(`lora_tdm_app_utils.c:565` 부근, 조건문에 `UseV2Downlink` 참조 없음)
- 즉 v1 ACK 텍스트가 계속 오는 것이, Pi가 여전히 v1으로 송신 중이라서인지
  아니면 Pi는 v2로 전환했는데 지상이 아직 ACK2(바이너리)로 응답 안 해서인지
  **미확인** — 사용자가 조사 중단시킴(그 다음 grep 명령 거부)

**다음 세션/재개 시 확인할 것**:
1. `lora_tdm_app_utils.c`에서 `SET_DOWNLINK_PROTO_CC`/`PARAM_DOWNLINK_PROTOCOL`
   CONFIG 처리 시 `UseV2Downlink ? "v2(DL2)" : "v1(text)"` 이벤트 로그가
   실제로 v2로 찍혔는지 Pi 로그(`/tmp/cfs_run.log`)에서 확인
   (`grep -i "v2(DL2)\|downlink_protocol" /tmp/cfs_run.log`)
2. 지상 `fc_serial_ws_server.py`의 `DownlinkStream`이 실제로 DL2 매직바이트를
   감지해 ACK2로 응답하는지, 아니면 여전히 v1 파서 경로로 ACK 텍스트를
   보내고 있는지 지상 로그/코드 확인
3. "ACK,<seq>" mismatch가 v1/v2 무관하게 원래도 나던 것인지(재시작 직후
   로그에서도 동일 패턴 관측됨 — v2 전환 명령 보내기 전부터 존재) 여부
   재확인 — 만약 그렇다면 이 채널 자체가 다운링크 프로토콜과 무관한
   별개 메커니즘일 가능성

## 상태

- [x] Pi 재배포 (이번 세션 누적 변경사항 전체)
- [x] Stage 4a (150ms) 실측 — PASS
- [ ] v2 전환 미반영 의심 건 조사 (위 참고)
- [ ] Stage 4b (100ms) 실측
- [ ] 실측 완료 후 `mission_cfg.h`를 최종값(또는 200ms 원복)으로 확정,
      `lora_stage_measurement_runbook.md`/`lora_downlink_5hz_cap_2026-07-21.md`에
      결과 반영
