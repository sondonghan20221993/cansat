# lora_tdm_app

LoRa 무선 링크로 지상국과 통신하는 TDM(Time-Division Multiplexing) 다운링크/업링크 앱이다. FC 상태·시스템 헬스를 주기적으로 다운링크하고, 지상국이 보낸 uplink 원문 프레임을 수신해 `uplink_app`으로 전달한다. (구 `lora_fc_downlink_app`을 대체, 2026-06-16~)

## MID 인터페이스

| 방향 | 심볼 | 값 | 설명 |
| --- | --- | --- | --- |
| CMD 수신 | `LORA_TDM_APP_CMD_MID_VALUE` | `0x18E0` | NOOP/RESET/SET_DOWNLINK_PROTOCOL 등 |
| CMD 수신 | `LORA_TDM_APP_SEND_HK_MID_VALUE` | `0x18E1` | HK 요청 |
| SB 수신 | `DIAGNOSTIC_CMD_MID` | `0x1910` | 진단 명령(LINK_STATUS/RX_STATS/TX_STATS) |
| SB 수신 | `CONFIG_CMD_MID` | `0x190E` | 런타임 config (scope=3, downlink_protocol 등) |
| SB 수신 | `UPLINK_STATUS_MID` | `0x190A` | uplink_app 처리 결과 → UFB(Uplink Feedback Byte) 판정 |
| SB 수신 | `EXEC_RESULT_MID` | `0x1912` | 대상앱 실행결과 회신 (BL-08, 공용) |
| SB 수신 | `FC_*_STATE_MID` 4종 + `FC_SYS_TIME_MID` | `0x1905~0x1909` | mavlink_bridge_app이 게시하는 FC 상태 — 다운링크 페이로드 구성용 |
| SB 수신 | `SYSTEM_HEALTH_MID` | `0x1904` | cfs_core_app 헬스 상태 — 다운링크 페이로드 구성용 |
| SB 발행(uplink 전달) | `LORA_TDM_APP_UPLINK_APP_CMD_MID_VALUE` | `0x18D0` | 수신한 uplink 원문 프레임을 uplink_app으로 포워딩 |
| 게시 | `LORA_TDM_APP_HK_TLM_MID_VALUE` | `0x08E0` | HK 텔레메트리 |
| 게시 | `LORA_TDM_APP_LINK_STATUS_MID_VALUE` | `0x1911` | 링크 상태(CONNECTED/DEGRADED/DISCONNECTED) |

## 구현 기능

### TDM 다운링크 (기체 → 지상)
- 고정 주기(`LORA_TDM_APP_CYCLE_PERIOD_MS`)로 FC 상태/시스템 헬스를 LoRa로 송신
- 두 가지 프로토콜을 CONFIG 런타임 전환으로 지원:
  - **v1(ASCII 텍스트)**: 기본값, `RunTx()`가 라인 단위로 조립
  - **v2(DL2 바이너리)**: `UseV2Downlink` 플래그로 활성화, `LORA_TDM_APP_BuildDl2Frame()`으로 조립 — seq/boot_count/SysTime 확장 블록 포함(BL-03)
- 다운링크 프레임에 `PendingUplinkFeedback`(UFB, 1바이트)을 실어 최근 uplink 명령 처리 결과를 지상에 회신 — 12종 코드(OK/CRC_FAIL/SEQ_FAIL/STATE_BLOCKED + 8종 거부 사유, BL-11)

### TDM 업링크 수신 (지상 → 기체)
- RX 창(`LORA_TDM_APP_RX_WINDOW_MS`) 동안 시리얼에서 수신, CRC 검증
- CRC 통과 시 원문을 `uplink_app`으로 SB 포워딩(`LORA_TDM_APP_UPLINK_PROCESS_UPLINK_CC`)
- CRC 실패 시 `PendingUplinkFeedback = UFB_CRC_FAIL`로 즉시 지상에 알림

### 링크 상태 관리
- `NoAckCount` 임계값(`LORA_TDM_APP_LINK_LOSS_THRESHOLD`) 기반 CONNECTED → DEGRADED → DISCONNECTED 전이
- 첫 관측은 전이 이벤트 대상 제외(BL-04, 오탐 방지)

### 진단/설정 명령
- `DIAGNOSTIC_CMD_MID`: `LINK_STATUS`/`RX_STATS`/`TX_STATS` 요약을 EVS로 출력
- `CONFIG_CMD_MID`(scope=3): `downlink_protocol` 파라미터로 v1/v2 런타임 전환

## 설정 파일

| 파일 | 주요 값 |
| --- | --- |
| `config/default_lora_tdm_app_mission_cfg.h` | TDM 타이밍(`CYCLE_PERIOD_MS`/`RX_WINDOW_MS`/`LINK_LOSS_THRESHOLD`), UFB 코드 12종, 시리얼 경로/baud, CONFIG scope/version |
| `config/default_lora_tdm_app_topicid_values.h` | 자체 MID + 구독하는 외부 MID 값 전체 |
| `fsw/inc/lora_tdm_app_fcncodes.h` | CMD_MID 함수코드 정의(실제 dispatch가 참조하는 위치) |

> **주의**: TDM 타이밍 상수는 2026-07-22 기준 BL-15 실측용 임시값(150ms/70ms/33)일 수 있음 — 실측 완료 후 최종값 확정 전까지 `notes/temp/bl15_stage4_5hz_cap_progress_2026-07-22.md` 참조.

## 동작 명세 참조

- 전체 시스템 MID 계약: `notes/mission_app_runtime_spec.md` §5.1.1
- UFB 코드표/판정 지속 정책/신호 흐름 상세: `notes/lora_tdm_app_behavior_spec.md` §9~10
- v2(DL2) 바이너리 프레임 포맷 전체: `notes/lora_protocol_v2_spec.md`
- 5Hz(200ms) 이하 실측 근거: `notes/lora_stage_measurement_runbook.md`
