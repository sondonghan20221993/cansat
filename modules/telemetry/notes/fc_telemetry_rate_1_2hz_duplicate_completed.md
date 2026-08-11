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

## 재조사 및 수정 (2026-07-21)

`mavlink status` (NSH 콘솔, USB-C 직결)로 실측:
```
instance #0 (ttyS3 @57600, TEL2/UART4, Pi 링크):
  tx: 2416.4 B/s
  tx rate mult: 0.137   ← 요청 레이트의 13.7%만 실전송
  tx rate max: 2880 B/s ← baud(57600)/10/2 로 PX4가 계산한 캡
```

`tx rate max = baud/20` 관계 확인. 역산 결과 mult=1.0 도달에 필요한
실제 요구 대역폭 ≈ 2416.4/0.137 ≈ **17,638 B/s** → 필요 baud ≥ 352,760bps.
57600/115200 둘 다 부족, 최소 460800 이상 필요.

**원인 확정**: PX4가 UART4/TELEM2(Onboard 모드, 우리 요청분 외 기본 스트림도
포함) 링크의 baud 대역폭 부족으로 전 스트림 레이트를 자동으로
0.137배까지 스로틀링 — 코드/설정 버그 아니라 **baud rate 설정 부족**이 근본 원인.

**수정**:
- FC: `SER_TEL2_BAUD` = 921600 (사용자 QGC에서 적용 완료)
- Pi: `mavlink_bridge_app/config/default_mavlink_bridge_app_platform_cfg.h`
  `MAVLINK_BRIDGE_APP_SERIAL_BAUDRATE` 57600 → 921600
- `mavlink_bridge_app_utils.c:MAVLINK_BRIDGE_APP_GetBaudConstant()`에
  B460800/B921600 케이스 추가 (기존엔 B230400까지만 매핑돼 있어 921600
  설정 시 초기화 실패했을 것 — 같이 수정)
- 검증: 4개 앱 UT 16/16 PASS, 회귀 없음

## 최종 확인 (2026-07-21, 계속)

baud 상향 배포 후에도 CRC fail로 인해 실질적 개선이 안 됐던 추가 문제 발견 및
해결 — 상세는 `notes/mavlink_stx_reentry_parser_bug_completed.md` 참조.

- 파서의 STX 재진입 버그(페이로드 바이트가 우연히 0xFD/0xFE면 프레임 파싱
  도중 리셋)가 baud 상향으로 고엔트로피 메시지 비중이 늘면서 크게 드러남 → 수정.
- `mission-install` 배포 누락으로 실제 서비스가 구버전 바이너리로 돌던 문제
  발견 및 수정.

두 문제 해결 후 baud 921600 확정, `mavlink status` tx rate mult=1.0 확인,
journalctl에 ATTITUDE/LOCAL_POSITION_NED/SYSTEM_TIME 등 정상 디코딩 로그
확인 완료. 원래 증상(1.2Hz 중복값)의 근본 원인이었던 대역폭 스로틀링 해소됨.

## 관련
- `mavlink_bridge_app/fsw/src/mavlink_bridge_app_utils.c`
  (`MAVLINK_BRIDGE_APP_RequestTelemetryStreams`)
- `lora_tdm_app/config/default_lora_tdm_app_mission_cfg.h`
  (`LORA_TDM_APP_CYCLE_PERIOD_MS`)
- `notes/integration_steps.md` FC UART 링크 노이즈 잔여 이슈
