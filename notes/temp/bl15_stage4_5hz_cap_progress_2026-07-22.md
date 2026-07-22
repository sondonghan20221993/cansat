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

## Stage 4b (100ms) — ✅ 완료, PASS (2026-07-22 20:40~20:46)

**설정**: `CYCLE_PERIOD_MS=100`, `RX_WINDOW_MS=50`, `LINK_LOSS_THRESHOLD=50`

**실측 결과 (5분 soak, `telemetry_20260722_203627.csv` 재시작 이후 구간)**:
- 손실 **0.00%** (expected=2991, received=2991, gap 이벤트 0회)
- 실효 수신율 **9.97 pkt/s** (FC+SH 합산, 이론 10/s)
- 업링크도 정상: counter management(class 7) 전송 → `retx=0`(첫 슬롯 적중)
  → `LORA_TDM_APP: ResetCounters command` 실행 확인

**판정: PASS** — 100ms(10Hz)까지 실측 근거로 확정 가능. v1 ASCII 기준.

## 조사 필요 — ✅ 해결됨 (2026-07-22)

지상 CONFIG 명령으로 `lora_tdm.downlink_protocol=1`을 보내 v2(DL2) 전환을
승인(`[OK] CONFIG accepted seq=1`)했는데도, 이후 Pi 로그(`/tmp/cfs_run.log`)에
v1 ASCII "ACK,<seq>" 형식의 "ACK seq mismatch" 이벤트가 계속 찍힘.

**원인 확정 (2026-07-22 실기 재현)**: v2 전환 CONFIG가 **health gate에
차단되고 있었음**. GPS 없는 실내 환경 → EKF invalid → `CfsHealthState=1
(DEGRADED)` → §18.10.1 정책이 CONFIG/VIEWPOINT 차단. Pi 로그 증거:
`UPLINK_APP: command blocked by health state=1 class=1 seq=5`.
당시 지상의 `[OK] CONFIG accepted`+UFB=0은 "프레임 정상 수신"일 뿐
"적용됨"이 아니었음(UFB=0의 구조적 한계 — lora_tdm_app_behavior_spec.md
§10 그대로).

**해결 확인**: `force:true`(FORCE_FLAG, §18.10.2)로 재전송 →
`FORCED THROUGH health gate` → `LORA_TDM_APP: downlink protocol set to
v2(DL2)` 실제 전환 + 지상이 DL2 수신 후 **ACK2**로 응답(Pi 로그에
"ACK2 seq mismatch" — v2 ACK 채널 동작 증거) — v2 양방향 정상.
부수 확인: BL-08 EXEC_RESULT 실기 동작(`exec result seq=6 generic=0`).

참고: "ACK seq mismatch"(v1)/"ACK2 seq mismatch"(v2)는 반이중 지연으로
원래 항상 나는 로그(BL-35 기존 이슈) — 프로토콜 미전환 증거가 아니었음.

## 상태

- [x] Pi 재배포 (이번 세션 누적 변경사항 전체 + counter mgmt/RETX_IDX 실기 검증)
- [x] Stage 4a (150ms) 실측 — PASS
- [x] v2 전환 미반영 의심 건 — 해결(health gate 차단이 원인, FORCE로 전환 확인)
- [x] Stage 4b (100ms) 실측 — PASS (0% 손실, 9.97pkt/s)
- [ ] `mission_cfg.h` 최종값 확정(100ms 확정 커밋 vs 더 낮은 주기 추가 실측)
      — 사용자 결정 대기. 확정 시 `lora_stage_measurement_runbook.md`/
      `lora_downlink_5hz_cap_2026-07-21.md`에 결과 반영
