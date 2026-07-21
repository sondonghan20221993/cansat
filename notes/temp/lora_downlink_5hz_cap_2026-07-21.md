# LoRa 다운링크 5Hz 고정 — 추후 검토 항목 (2026-07-21)

## 배경

오늘 FC-Pi UART4 baud/파서 버그를 고쳐서 mavlink_bridge_app 캐시 갱신은
빨라졌지만, **Pi→지상국(GS) LoRa 다운링크는 별개로 5Hz에 고정**돼 있음을 확인.

```c
// lora_tdm_app/config/default_lora_tdm_app_mission_cfg.h:7
#define LORA_TDM_APP_CYCLE_PERIOD_MS   200   /* = 5Hz */
```

`lora_tdm_app.c:525`에서 메인 루프가 이 값으로 `OS_TaskDelay()` — 최종적으로
지상국 CSV/openMCT에 찍히는 갱신률은 이 이상 빨라질 수 없음.

## 정정 (2026-07-21, 같은 날 재확인)

이전 버전에서 "RF 실측 없음"을 근거로 보류 결정했으나 **오류였음** —
`notes/lora_stage_measurement_runbook.md`(2026-07-13~14) 확인 결과 5Hz(200ms)는
이미 실측 검증 완료된 값:

- Stage 2: 1000ms→500ms→400ms 단계적 실측, 손실률 전부 0%
- Stage 3 (2026-07-14): v2 DL2 프레임으로 200ms(5Hz) 5분 soak —
  관측 **5.0Hz**, 손실률 **0.0%**, LinkState **100% CONNECTED** → PASS

즉 현재 5Hz는 추정치가 아니라 실측 근거가 있는 값. 미검증 영역은
**"5Hz보다 더 빠르게(200ms 미만)"** 뿐임.

## 결정 (2026-07-21, 정정판)

**현재는 CONFIG 파라미터화 보류.** 이유:
- 지금까지 5Hz로 운용 중 부족하다고 판단된 구체 사례 없음 (오늘 영상-텔레메트리
  동기화 검증에서도 주요 회전 이벤트가 0.1~0.5초 오차로 정합됨)
- 200ms 미만 구간은 실측된 적 없음 — uplink로 값을 노출하면 운용자가
  검증 안 된 영역(예: 100ms)으로 올려 패킷 손실/충돌 유발 위험
- 파라미터를 추가하려면 200ms 미만 구간의 실측(Stage 2 방식과 동일한
  단계적 soak)이 먼저 있어야 min/max bound를 정할 수 있음

→ 200ms 미만 실측이 먼저 이뤄지고, 실제 운용 요구(5Hz로는 부족하다는 근거)가
생기면 그때 `LORA_TDM_APP_PARAM_DOWNLINK_PROTOCOL`과 같은 방식으로 CONFIG
파라미터화 검토 (cfs_core_app의 `PublishPeriodMs`처럼 죽은 설정을 하나 더
만드는 방향은 피함).

## 검토 필요 사항 (미착수)

- [ ] 5Hz보다 빠른 값(예: 100ms/10Hz)이 실제로 필요한지 운용 요구 확인
- [ ] 필요 시 200ms 미만 구간 단계적 실측 (runbook Stage 2 방식 재사용:
      150ms→100ms, 각 5분 soak, 손실률/RX p95/LinkState 확인)
- [ ] `LORA_TDM_APP_RX_WINDOW_MS=100`(RX 윈도우)이 더 짧은 사이클에서도
      안전한지 — 사이클 자체가 100ms면 RX 윈도우와 정확히 같아져 마진 없음,
      윈도우도 함께 줄여야 할 가능성 큼

## 관련
- `lora_tdm_app/config/default_lora_tdm_app_mission_cfg.h`
- `lora_tdm_app/fsw/src/lora_tdm_app.c:525`
- `notes/lora_stage_measurement_runbook.md` (5Hz까지 실측 완료 기록)
- `notes/fc_telemetry_rate_1_2hz_duplicate_completed.md`
