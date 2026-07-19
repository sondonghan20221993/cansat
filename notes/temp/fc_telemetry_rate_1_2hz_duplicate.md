# FC 텔레메트리 실제 갱신률 1.2Hz — 요청은 5Hz, 중복 전송 발생 (2026-07-16)

## 증상
텔레메트리 CSV에서 FC 행 값이 2~4회 연속 동일 — TDM 5Hz 슬롯마다 전송은 되지만
같은 값이 반복.

## 원인 분석
- `LORA_TDM_APP_CYCLE_PERIOD_MS=200`(5Hz) — TDM 슬롯 정직하게 5Hz로 동작 확인.
- `MAVLINK_BRIDGE_APP_ATTITUDE_INTERVAL_US=200000`(200ms=5Hz)로 FC에 이미
  5Hz 요청 중(`MAVLINK_BRIDGE_APP_RequestTelemetryStreams` →
  `SET_MESSAGE_INTERVAL`).
- 실측 CSV: FC 값 자체가 ~800ms(1.2Hz)마다만 바뀜 → 요청은 5Hz인데 FC 응답이
  못 따라옴.

## 판단
코드/설정 버그 아님. mavlink_bridge_app→lora_tdm_app 파이프라인은 요청/전송
모두 5Hz로 정상 동작. 병목은 **FC측**(ArduPilot 시뮬/실기)이 요청 레이트를
못 따라가는 것 — 원인 후보:
- 실내 테스트 환경(EKF 미정렬) 부하
- FC-Pi UART 노이즈/부하 (`integration_steps.md` 잔여 이슈 "FC UART 링크
  노이즈" 항목과 연관 가능)
- FC 자체가 SET_MESSAGE_INTERVAL을 무시/제한하는 설정

## 영향
- 대역폭 낭비(같은 값 반복 전송) 외 기능적 문제는 없음 — health/CONFIG 로직은
  MET(`boot_ms`) 기준이라 무관.
- `correlate_video_telemetry.py` 매칭에는 영향 없음(중복이어도 시각 매칭 자체는
  유효).

## 다음 단계 (미착수)
실외 실측 또는 FC 로그로 SET_MESSAGE_INTERVAL 실제 반영 여부 확인 필요.

## 관련
- `mavlink_bridge_app/fsw/src/mavlink_bridge_app_utils.c`
  (`MAVLINK_BRIDGE_APP_RequestTelemetryStreams`)
- `lora_tdm_app/config/default_lora_tdm_app_mission_cfg.h`
  (`LORA_TDM_APP_CYCLE_PERIOD_MS`)
- `notes/integration_steps.md` FC UART 링크 노이즈 잔여 이슈
