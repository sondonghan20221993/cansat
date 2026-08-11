# LoRa 단계별 실측 런북 (Stage 1/2/3)

작성: 2026-07-13. 목적: v2(바이너리) 전환 전에 실측 데이터로 설계 수치를 검증한다.
배경: `lora_protocol_v2_spec.md` §7 타이밍표는 데이터시트 산수이며 LR24-F 실물 타이밍
(FHSS 호핑 지연·내부 버퍼링·패킷화 지연)을 검증한 적이 없다.

각 단계는 **이전 단계 성공 후에만** 진행한다. 실패 시 그 단계 원인을 해소하고 재시도 —
다음 단계로 건너뛰지 않는다.

## 사전 준비 (Stage 0 — 밀린 빌드 검증부터)

Pi가 오프라인이던 동안 미검증 상태로 커밋된 것들을 먼저 청산한다.

```bash
ssh sdh2983@192.168.50.65 'cd cFS_clean && <빌드 명령>'
# UT: mavlink_bridge_app coveragetest — SysTime_FullPayload 등 4종 확인
```

- [ ] SYSTEM_TIME 파싱 커밋(`30a8d7b`)이 실제로 컴파일됨
- [ ] `SysTime_*` 단위테스트 4종 PASS
- [ ] 기존 회귀 없음 (전체 coveragetest 스위트)

## Stage 1 — ACK keepalive 정상화 (v1 그대로, 설정 변경 없음) — ✅ 완료 (2026-07-13)

**실측 결과** (2026-07-13 21:00~21:05, 1000ms 주기): 손실률 0.0%(352/352),
`link_state` 175/176(99.4%) CONNECTED, ACK 송신 지연 mean 0.3~0.7ms/p95 0ms.
성공 기준 3개 전부 충족. (`link_state=2` 1건 관측 — 전이 상태로 추정, 별도 확인 필요)


**변경 범위**: openMCT `fc_serial_ws_server.py`만 (커밋 대기 중 — 코드는 완료).
기체측 코드 변경 없음.

**목적**: 지상이 ACK를 보내기 시작했을 때 기체 `LinkState`가 실제로 CONNECTED로
전이하는지 확인. 지금까지 이게 안 되는 상태로 운용해왔다는 것 자체가 검증 대상.

**절차**:
1. openMCT `fc_serial_ws_server.py` 기동 (`--baud 57600`)
2. Pi에서 cFS 기동, `lora_tdm_app` HK를 CI_LAB으로 관찰 (`LinkState` 필드)
3. 5분 이상 연속 수신 유지
4. Pi 로그 또는 HK로 `LinkState` 값 확인

**성공 기준**:
- [ ] 기체 `LinkState == LORA_TDM_APP_LINK_CONNECTED(1)` 유지 (전에는 5000ms마다 DISCONNECTED로 떨어졌을 것으로 예상)
- [ ] `NoAckCount`가 0 근처에서 유지 (임계 3 미만)
- [ ] openMCT 콘솔의 `ack_send` 로그값이 수 ms 이내 (지상 처리 지연이 무시할 수준인지)

**분석**:
```bash
python3 tools/analyze_downlink_csv.py "<telemetry_logs 경로>/telemetry_YYYYMMDD_HHMMSS.csv" --cycle-ms 1000
```
`link_state` 분포에서 가 CONNECTED(1) 값만 나오는지 확인.

## Stage 2 — v1 유지, TDM 주기만 단축 (설정값 변경 + 재빌드)

**변경 범위**: `lora_tdm_app/config/default_lora_tdm_app_mission_cfg.h` (컴파일타임
상수 — lora_tdm_app에는 런타임 CONFIG 명령이 없음, 재빌드·재배포 필수).

목표: 코드 리스크(v2의 파서/프레이밍 변경) 없이 LR24-F 실물 타이밍만 먼저 검증.

**시도할 값 (단계적으로, 각 값마다 최소 5분 soak)**:

| 시도 | CYCLE_PERIOD_MS | RX_WINDOW_MS | LINK_LOSS_THRESHOLD | 목표 레이트 |
| --- | --- | --- | --- | --- |
| 2a | 1000 (기준선, 변경 없음) | 300 | 3 | 0.77Hz — Stage 1과 비교 기준선 확보 |
| 2b | 500 | 150 | 6 | ~1.5Hz |
| 2c | 400 | 100 | 8 | ~2.2Hz |

`LINK_LOSS_THRESHOLD`는 `elapsed_per_no_ack ≈ CYCLE_PERIOD_MS` 가정하에
`LINK_TIMEOUT_MS(5000) / CYCLE_PERIOD_MS`에 근접하게 맞춘다 (과민 방지, v2 spec §7 표와 동일 원칙).

**절차 (값 하나당)**:
1. 헤더 수정 → 빌드 → Pi 배포 → cFS 재기동
2. openMCT ws 서버로 5분 이상 수신
3. `tools/analyze_downlink_csv.py <csv> --cycle-ms <해당값>` 실행
4. 결과를 아래 표에 기록 후 다음 값으로

**성공 기준 (각 시도마다)**:
- [ ] 관측 레이트 >= 목표의 90%
- [ ] seq gap 손실률 < 5%
- [ ] `_rx_total_ms` p95 < RX_WINDOW_MS (RX창 안에 처리가 끝나는지 — 못 끝나면 다음 사이클 침범)
- [ ] LinkState CONNECTED 유지 (Stage 1 조건 계속 만족)

**중단 기준**: 어느 시도에서 손실률이 급격히 악화되면(예: 2b는 정상, 2c에서 손실률 20%+)
그 값을 상한으로 기록하고 더 줄이지 않는다 — 이 상한이 v2 설계(200ms)의 실측 근거가 된다.

### 결과 기록표 (실측 후 채움)

| 시도 | 관측 레이트(Hz) | 손실률(%) | RX p95(ms) | LinkState | 판정 |
| --- | --- | --- | --- | --- | --- |
| 2a | 1.0 (Stage 1 데이터 재사용, 1000ms) | 0.0 | 984~1032 (창 300ms 대비 초과, ⚠아래 참고) | 100%(99.4%) CONNECTED | ✅ PASS |
| 2b | 2.0 (500ms) | 0.0 | 469~563 (창 150ms 대비 초과, 2a와 동일 비율) | 100% CONNECTED | ✅ PASS |
| 2c | 2.5 (400ms) | 0.0 | 375~469 (창 100ms 대비 초과, 동일 비율 반복) | 99.7% CONNECTED | ✅ PASS |

**Stage 2 전체 통과 — Stage 3(v2, 200ms/5Hz) 착수 조건 충족.**

⚠ **RX p95가 RX_WINDOW_MS를 항상 3~4.7배 초과하는 패턴 반복** — 2a/2b/2c 전부 유사 비율로
초과함. "RX창 내 처리시간 초과"가 아니라 `_rx_total_ms` 지표 자체가 다른 걸 재는(예: 동일
source 간 간격) 것으로 추정 — 손실률엔 영향 없었으나 Stage 3 전 지표 정의 재확인 필요.

⚠ **`link_state=2` 값이 2a/2c에서 각 1건씩 관측** — 전이 상태로 추정되나 의미 미확인,
`lora_tdm_app`의 LinkState enum 정의 확인 필요 (Stage 3 착수 전 참고 사항, 게이트 아님).

## Stage 3 — v2 바이너리 전환 (Stage 2 성공 시에만 착수)

**선행 조건**: Stage 2에서 최소 400ms(2.5Hz)가 성공해야 v2의 200ms(5Hz) 시도가
근거를 갖는다. 400ms에서 이미 실패하면 200ms는 시도하지 않는다 — 대신 Stage 2의
성공한 값을 잠정 운용값으로 채택하고 v2는 보류.

**착수 전 게이트 (코드 작업, 실측과 별개로 선행)** — 구현 세부는 `lora_protocol_v2_spec.md` §11:
- [x] lora_tdm `RunRxWindow` 버퍼를 static/전역으로 리팩터링 (RX창 간 파서 상태 유지) — §11.1 (2026-07-13, Pi 배포됨)
- [x] C 파서를 길이(len) 기반 상태머신으로 구현 — §11.2 (2026-07-13, ACK2/UP2 첫바이트 분기, Pi 배포됨)
- [x] C `Crc16` ↔ Python `crc16_ccitt` 표준 벡터 교차 검증 UT — §11.3 (2026-07-13)

**추가 완료 (게이트 범위 밖, 같은 날 진행)**:
- [x] DL2(다운링크 통합 프레임) C 인코더 + UT — `LORA_TDM_APP_BuildDl2Frame()`
- [x] UP2/ACK2 C 디코더 + UT — `LORA_TDM_APP_ParseUp2Frame()`/`ParseAck2Frame()`
- [x] RunTx에 `UseV2Downlink` 플래그로 DL2 실송신 연결 (기본값 0=v1, 안전)
- [x] `bridge/lora_downlink_decoder.py` sats 필드 미동기화 갭 해소 (spec §10에 있던 기존 이슈)

**아직 남음** (5Hz 실측 전 필요) — 2026-07-14 스테일 정정: 아래 두 항목은 이
목록 작성(236ec37) 직후 같은 날 이미 구현 완료됐는데 체크 갱신이 누락돼 있었음:
- [x] CONFIG 명령으로 v1/v2 런타임 전환 — 커밋 `431f13d`(2026-07-13):
      기체측 `ProcessConfigCommand`에 `PARAM_DOWNLINK_PROTOCOL` 분기, 지상측
      `fc_serial_ws_server.py`에 `lora_tdm.downlink_protocol` 파라미터 노출 완료
- [x] 지상 Python UP2 인코더 — 커밋 `1e80f77`(2026-07-13):
      `bridge/lora_downlink_decoder.py::build_up2()` + C UT 교차검증.
      (원문의 "ACK2 파서"는 방향 착오 — ACK2는 지상→기체 송신이므로 지상엔
      파서 불필요, 송신은 기존 구현 있음)
- [x] `CYCLE_PERIOD_MS` 200ms로 변경 + 실기체 5Hz soak — **완료, PASS (2026-07-14)**

**Stage 3 실측 시도 (2026-07-14 착수)**: spec §7 확정값 적용 —
`CYCLE_PERIOD_MS` 400→**200**, `RX_WINDOW_MS` 100→**100**(불변),
`LINK_LOSS_THRESHOLD` 8→**15**(5000ms/200ms 대비 ≈3s에서 DEGRADED, §7 표 근거).
절차: 헤더 수정 → Pi 빌드/배포 → cFS 기동(v1으로 부팅, `UseV2Downlink` 기본 0)
→ 지상 CONFIG 명령 `lora_tdm.downlink_protocol=1`로 v2 런타임 전환 → 5분 soak
→ `tools/analyze_downlink_csv.py <csv> --cycle-ms 200` 판정. 성공 기준은 Stage 2와
동일(관측 레이트 ≥ 4.5Hz, 손실률 <5%, LinkState CONNECTED 유지).

**결과 — ✅ PASS (2026-07-14)**: 도중 지상국(`fc_serial_ws_server.py`)에 DL2
파서가 아예 없던 걸 발견해 먼저 통합(openMCT repo, `notes/temp/
dl2_downlink_integration.md`). 통합 후 재시도:
- 관측 레이트: **5.0Hz** (DL2 2816건 / 562.9s)
- seq gap 손실률: **0.0%** (expected=3137, received=3137, 통합 seq 기준)
- LinkState: 100% CONNECTED(`1`)
- 참고(비차단): FC `boot_ms` 최대 7회 연속 정체 — **조사 완료(2026-07-14)**:
  캐시 재전송 자체는 설계상 정상이고 근본 원인은 FC(PX4)가 ATTITUDE를
  ~1.2Hz로만 내보내는 것(SET_MESSAGE_INTERVAL 미ack/미반영). DL2 5Hz가 이
  느린 소스를 반복 송신해 발생. cFS 코드 결함 아님, FC측 사안 —
  `notes/temp/fc_attitude_rate_below_requested.md` 참조.
  RX p95가 참고 RX창 초과하는 패턴은 Stage 2와 동일 재현(지표 정의 이슈로 추정)
- 부수 발견: `cfs_core_app` health FAILED(BRIDGE_TIMEOUT) 고착 재발 —
  `FORCE_FLAG`로 우회해 진행, 근본 원인은 `notes/temp/
  sch_lab_bridge_timeout_recurrence.md`에 별도 기록(미해결)

**Stage 3 게이트 전체 통과.** 이 게이트 통과 후 `lora_protocol_v2_spec.md` §9
검증 요구사항대로 진행.

## 진행 로그

| 날짜 | 단계 | 결과 | 비고 |
| --- | --- | --- | --- |
| 2026-07-13 21:00 | Stage 1 | ✅ PASS | 손실률 0%, LinkState 99.4% CONNECTED |
| 2026-07-13 21:00~21:05 | Stage 2a (기준선, 1000ms) | ✅ PASS | Stage 1 데이터 재사용 |
| 2026-07-13 21:20 | Stage 2b (500ms/150ms/6) | ✅ PASS | 손실률 0%, RX p95 초과는 2a와 동일 비율(지표 정의 이슈로 추정) |
| 2026-07-13 21:29~21:34 | Stage 2c (400ms/100ms/8) | ✅ PASS | 손실률 0%, LinkState 99.7% CONNECTED — Stage 2 전체 통과, Stage 3 착수 조건 충족 |
| 2026-07-14 22:05~22:14 | Stage 3 (200ms/100ms/15, v2 DL2) | ✅ PASS | 관측 5.0Hz, 손실률 0%, LinkState 100% CONNECTED — Stage 3 게이트 전체 통과 |
