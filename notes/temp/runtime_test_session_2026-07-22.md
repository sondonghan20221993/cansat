# Pi 런타임 테스트 세션 (2026-07-22 저녁)

환경: Pi 192.168.50.65 (cfs.service, PID 갱신됨), LoRa 동글 양측 연결,
Windows 지상국(fc_serial_ws_server.py, 127.0.0.1:8082) 가동, GPS 없음(실내,
`fault_code=3 EKF_INVALID` 상시 → health DEGRADED).
빌드: 이번 세션 전체 변경분(counter mgmt/RETX_IDX/P1-a/BL-17 등) + BL-15
Stage 4b 타이밍(100ms/50ms/50).

## 완료된 실기 검증

| # | 항목 | 결과 |
|---|---|---|
| 1 | counter management(class 7) 4개 scope 전부 | ✅ PASS — 라우팅+대상앱 실행(`ResetCounters`/`RESET` EVS), 전부 retx=0 |
| 2 | BL-14 RETX_IDX | ✅ 동작 — `routed uplink ... retx=0` 로그 확인 |
| 3 | v2 전환 미반영 의심 | ✅ **해결** — 원인=health gate(DEGRADED)가 CONFIG 차단. `force:true` → v2(DL2) 전환+ACK2 양방향 확인 |
| 4 | BL-08 EXEC_RESULT | ✅ 실기 동작 — `exec result seq=6 generic=0` |
| 5 | BL-15 Stage 4b(100ms) | ✅ PASS — 5분 soak 손실 0.00%(2991/2991), 9.97pkt/s, 업링크 정상 |
| 6 | P1-a PARSER_RESET E2E | ✅ PASS — 지상→uplink→cfs_core→bridge 실제 파서 리셋 |
| 7 | RT-CORE-003 (uplink_app 자동 재시작) | ❌ **FAIL → BL-38 결함 발견** (아래) |

## RT-CORE-003 FAIL 상세 (BL-38)

- 준비 과정 이슈 2건(테스트 인프라, 수정 완료):
  - `runtime_app_restart_test.sh`가 바이너리명(`uplink_app`)을 보냈으나
    cFE 등록명은 `UPLINK_APP` — `GetAppIDByName failed RC=0xC4000002`.
    스크립트에 `CFE_APP_NAME` 매핑 추가(로컬 수정, 커밋 예정)
  - Pi startup.scr에 CI_LAB 미탑재 → cmd_send(UDP 1234) 수신 불가.
    벤치용으로 추가(`~/cfe_es_startup.scr.bak` 백업, STOP_APP류는 지상
    LoRa 명령셋에 의도적으로 없어 CI_LAB 경유가 유일한 주입 경로)
- 본 결함: STOP_APP으로 uplink_app 정지(20:58:39 Stop Completed) 후
  cfs_core_app 재시작 시도 이벤트(EID 15) **전무**, `Msg Limit Err
  (0x1904→UPLINK_CMD)` 지속. 원인은 fault 체인 종속(BL-38/spec §11.1).
  복구는 cfs.service 수동 재시작으로 수행.
- 파생 관찰: uplink_app 사망 중엔 지상 RECOVERY(RESTART_BRIDGE) 명령도
  무반응(업링크 명령 경로 자체가 uplink_app 경유) — 사용자가 GUI에서
  체감. "텔레메트리는 계속 내려옴"은 정상(다운링크는 lora_tdm 담당).

## 남은 순서 (사용자 지시: 기록 후 순서대로 전부 시도)

- [ ] D-1 lora_tdm_app 재시작 실측 — BL-38과 동일 결함 예상(EKF 분기가
      상위) → FAIL 예상되나 실측 기록 목적으로 실행
- [ ] D-2: TDM-RT-001~009, RT-LORA-001/004, RT-DL2-SYSTIME-001
- [ ] D-3: 통합 순차 세션 7단계 (TEST_CASES.md 1008행)
- [ ] BL-22: `init_uart_clock=48000000` 원복 여부
- [ ] BL-35: ACK seq mismatch 실측(항상 4~6 lag 관측 중 — SF/BW/CR 또는
      타임스탬프 대조)
- [ ] (결함 수정 후) BL-38 A안 구현 → RT-CORE-003/004 재시험
