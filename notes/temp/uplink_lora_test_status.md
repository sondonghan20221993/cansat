# LoRa Uplink 실시간 테스트 — 현황/문제/해결 (2026-06-15)

> 임시 작업 노트. uplink 명령(지상국 → 드론) LoRa 경로를 실시간 검증하는 과정에서
> 만난 문제와 해결 접근을 정리한다. 확정되면 `integration_steps.md`로 승격.

## 1. 목표
지상국(OpenMCT) CLI에서 보낸 CONFIG/RECOVERY 명령이 LoRa를 통해
드론 `uplink_app`까지 도달·처리되는지 실시간 확인.

## 2. 경로 (설계)
```
OpenMCT Uplink CLI
  → fc_serial_ws_server.py (지상, HTTP/api/uplink → LoRa serial TX)
    → LoRa RF
      → Pi CP2102 (lora_fc_downlink_app 단독 소유)
        → TDM RX 윈도우에서 "UP,..." 수신 → UPLINK_RAW_MID(0x1909) SB publish
          → uplink_app 구독 → ParseLoRaFrame → ProcessUplink → 대상앱 라우팅
```
반이중 TDM: Pi RX 윈도우는 downlink TX 직후 **300ms만** 열림.

## 3. 검증된 것 (OK)
- 지상 브리지 슬롯 정렬 + 자동 재전송(`_UPLINK_RETX=4`): 콘솔에
  `[UP] CONFIG ... queued` → `[UP->slot] (1/4)~(4/4)` 정상 출력. 각 downlink 직후 송신.
- RECOVERY 경로(이전 세션, 수동 반복 전송): `uplink_app`까지 도달 →
  `UPLINK_APP: command blocked by health state=3 class=4` 확인.
  → Ground→LoRa→lora_fc_downlink_app→LORA_RAW_MID→uplink_app 경로 자체는 동작.
- CONFIG 코드 체인: uplink `ForwardConfigCommand` → `CONFIG_CMD_MID(0x190E)` publish
  → mavlink_bridge `ProcessConfigCommand` 적용 + `StreamRequestPending` + EVS
  `config activated ...`. (코드상 정상, 도달+health=0이면 적용됨)

## 4. 만난 문제와 해결

### 문제 A — 단발 uplink 미도달 (해결)
- 증상: 1회 전송은 무응답, 여러 번 빠르게 보내면 적중.
- 원인: 단발은 Pi의 300ms RX 윈도우 한 슬롯만 노려 타이밍 지터/RF 손실로 자주 빗나감.
- 해결: 지상 `fc_serial_ws_server.py`가 동일 프레임을 **연속 4개 downlink 슬롯에 자동 재전송**.
  `uplink_app`의 `IsSequenceAccepted`가 중복(seq)을 무시 → 1발만 적용, 나머지 replay 거부(무해).

### 문제 B — uplink 전혀 미수신 (RX 윈도우 즉시 종료 버그) (해결, 검증 대기)
- 증상: 지상은 4발 전송 확인되는데 Pi `uplink_app` 로그에 아무것도 안 뜸. 다운링크는 정상(비대칭).
- 원인: LoRa 포트가 `VMIN=0/VTIME=0`이라 데이터 없으면 `read()`가 즉시 0 반환.
  RX 윈도우 코드가 빈 read에 `break` → 윈도우 열리자마자(uplink 도착 전, RF 왕복 지연) 종료
  → uplink/HB를 영영 못 읽음.
- 해결(커밋 `cac209f`): 빈 read 시 `break` 대신 `usleep(2ms)` 후 **deadline까지 폴링 유지**.
  `lora_fc_downlink_app_utils.c` ServiceLoRa RX 윈도우.
- 상태: Pi 재빌드 후 재검증 필요.

### 문제 C — CONFIG "적용" 차단 (정책)
- 증상: 도달해도 CONFIG 미적용.
- 원인: `uplink_app_cmds.c` health-block 정책 — **FAILED(3)는 전 명령 차단**,
  DEGRADED(1)/RECOVERY(2)도 CONFIG 차단. CONFIG는 **NOMINAL(0)에서만** 허용.
- 실측 health 흐름: `0→2(0.75s)→3(51s)`, fault=1 = **BRIDGE_TIMEOUT 분기**
  (FC 텔레메트리 전체 끊김: ATTITUDE/LOCAL 4회뿐, GPS/EKF 0회 → bridge stale).
- **GPS 결합 문제 (결정됨)**: 설령 링크가 살아나도 GPS fix 없으면(실내) `GpsUnavailable→DEGRADED`로
  CONFIG가 영영 차단됨. cFS health는 통신-계층 상태여야 하고 GPS는 센서/비행 조건이므로
  **GPS를 health 게이트에서 분리(A안)** 하기로 결정.
  - md 갱신 완료: `cfs_core_app_behavior_spec.md §12.5/§13.2/테스트`,
    `mission_app_runtime_spec.md §15 GPS 정책/§5.1.1/테스트`.
  - 코드 변경 예정: cfs_core `GpsUnavailable→DEGRADED` 분기 제거, `GpsValid`는 보고 유지.
- 잔여: GPS 분리 후에도 **EKF/local/attitude/bridge** 가 fresh해야 NOMINAL.
  현재 FC 링크 자체가 불안정(UART CRC fail) → 별도 해결 필요. EKF 헬스 반영 여부는 추후 검토.

## 5. 부가 관찰
- FC UART 링크 노이즈: `crc fail msgid=24/30`, `Parse/data error code=4`, stream request 대상
  sys가 1/31/58/90/245로 흔들림 → FC 시리얼 잡음. 깨진 프레임만 폐기되나 telemetry 간헐 + health 저하 원인.

## 6. 다음 단계
1. [대기] Pi 재빌드(`cac209f`) 후 CONFIG 1회 → `UPLINK_APP: command blocked ... class=1` 확인(문제 B 해결 검증).
2. health=3 원인(FC 입력 누락) 해소 → NOMINAL → CONFIG `config activated` 적용 확인(문제 C).
3. 확정 시 본 노트를 `notes/integration_steps.md` 해당 섹션으로 승격, temp 정리.

## 7. 관련 위치
- 드론: `lora_fc_downlink_app_utils.c`(RX 윈도우/ForwardUplinkFrame),
  `uplink_app_cmds.c`(health-block/ProcessUplink), `uplink_app_dispatch.c`(LORA_RAW_MID),
  MID `UPLINK_RAW=0x1909` / `CONFIG_CMD=0x190E`.
- 지상: openMCT `fc_serial_ws_server.py`(슬롯정렬+재전송), `openmct_bridge_notes.md`.
