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

## 결정 (2026-07-21)

**현재는 CONFIG 파라미터화 보류.** 이유:
- 지금까지 5Hz로 운용 중 부족하다고 판단된 구체 사례 없음 (오늘 영상-텔레메트리
  동기화 검증에서도 주요 회전 이벤트가 0.1~0.5초 오차로 정합됨)
- RF 실측(air-time) 없이 uplink로 값을 노출하면 운용자가 감당 안 되는 값으로
  올려 패킷 손실/충돌 유발 위험
- 파라미터를 추가하려면 SF/BW/CR 기준 실측 air-time 기반 min/max bound
  검증 로직이 선행되어야 하는데 아직 그 데이터가 없음

→ RF 실측이 먼저 이뤄지고, 실제 운용 요구가 생기면 그때
`LORA_TDM_APP_PARAM_DOWNLINK_PROTOCOL`과 같은 방식으로 CONFIG 파라미터화
검토 (cfs_core_app의 `PublishPeriodMs`처럼 죽은 설정을 하나 더 만드는 방향은 피함).

## 검토 필요 사항 (미착수)

- [ ] 이 값을 올릴 필요/여지가 있는지 판단 — 5Hz가 임무 요구사항 대비
      충분한지, 아니면 더 필요한지 확인
- [ ] 올린다면 LoRa 실제 전파시간(air time)이 감당 가능한지 확인 필요.
      `LORA_TDM_APP_RX_WINDOW_MS=100`(RX 윈도우) 및 이전 조사했던 SF/BW/CR
      설정과 연동 — 무작정 사이클만 줄이면 실측 없이는 의미 없을 수 있음
      (참고: `notes/fc_telemetry_rate_1_2hz_duplicate_completed.md`)
- [ ] RF 실측(Pi UART 타임스탬프 vs GS 수신 타임스탬프 대조)이 선행되어야
      정확한 상한선 판단 가능

## 관련
- `lora_tdm_app/config/default_lora_tdm_app_mission_cfg.h`
- `lora_tdm_app/fsw/src/lora_tdm_app.c:525`
- `notes/fc_telemetry_rate_1_2hz_duplicate_completed.md`
