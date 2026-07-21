# FC-Pi UART4 baudrate 작업 요약 (2026-07-21)

## 시작 계기

FC 파라미터 송출 주기를 5Hz로 맞추려던 중, `mavlink status`로 실측하니
`tx rate mult=0.137` — PX4가 요청 레이트의 13.7%로 스로틀링 중임을 발견.
57600baud 대역폭 부족이 원인으로 확인.

## 진행 과정 (시행착오 포함)

1. **57600 → 921600 1차 시도**: baud 클럭 계산상 필요치(≥352,760bps) 충족.
   FC `SER_TEL2_BAUD`=921600 반영 후 `mavlink status`로 tx rate mult=1.000
   확인했으나, Pi 쪽 `mavlink_bridge_app` 로그에 `crc fail` 다수 발생.

2. **원인 오판 1 — UART 클럭 정밀도**: CM4 UART 클럭이 동적 스케일링으로
   흔들려서 baud 오차가 난다고 판단, `/boot/firmware/config.txt`에
   `init_uart_clock=48000000` 추가·재부팅. `vcgencmd measure_clock uart`로
   48,001,464Hz 고정 확인했으나 crc fail 지속 — 원인 아니었음.

3. **원인 오판 2 — 배선 신호 무결성**: 921600이 점퍼선 배선에 너무 빠르다고
   판단, 460800으로 하향(계산상 필요 대역폭 대비 30% 여유). 그래도 동일하게
   crc fail 지속 — 이것도 원인 아니었음.

4. **진짜 원인 발견 — 파서 STX 재진입 버그**: crc fail msgid(2, 24, 32, 33)가
   전부 요청 중인 고엔트로피 스트림(SYSTEM_TIME/GPS_RAW_INT/LOCAL_POSITION_NED/
   GLOBAL_POSITION_INT)과 일치함을 근거로 코드 직접 확인.
   `MAVLINK_BRIDGE_APP_ProcessReceivedByte()`가 파서 상태와 무관하게 모든
   수신 바이트를 STX(0xFD/0xFE)로 검사 → 페이로드/CRC 바이트가 우연히
   그 값이면 프레임 파싱 도중 재진입(리셋)되어 깨짐. baud를 올려
   스로틀링이 풀리면서 고엔트로피 데이터 비중이 늘어 증상이 급증한 것.
   상세: `notes/mavlink_stx_reentry_parser_bug_completed.md`.
   → STX 검사를 `WAIT_STX` 상태로 한정, 회귀 UT 추가, 로컬 161/161 PASS.

5. **배포 문제 추가 발견**: 파서 수정 후 재배포해도 crc fail이 그대로라
   재확인한 결과, 실제 서비스 바이너리(`cFS_clean/build/exe/cpu1/`)가
   **7/16일자 구버전**으로 고정돼 있었음 — `make mission-all`만 반복하고
   `mission-install DESTDIR=...` 단계를 빼먹어서, 오늘 했던 모든 코드
   수정(baud, 파서 수정)이 실제로는 한 번도 반영된 적 없었음. 정식
   설치(`mission-install`) 후 `.so`/`core-cpu1` md5sum이 빌드 트리와
   일치함을 확인하고 재시작하니 즉시 해결.

6. **최종 확정 — 921600으로 재상향**: 파서 버그와 배포 문제 해결 후
   460800→921600 재시도, `tx rate mult=1.000` 유지하면서 crc fail 없이
   ATTITUDE/LOCAL_POSITION_NED/SYSTEM_TIME 정상 디코딩 확인.

## 결론

- baud 자체(57600/460800/921600)와 UART 클럭 정밀도, 배선 신호 무결성은
  전부 **원인이 아니었음** — 그럼에도 921600 상향과 클럭 고정 조치는
  대역폭 여유 확보 차원에서 유지.
- 진짜 원인은 **① 파서 STX 재진입 버그, ② mission-install 배포 누락**
  두 가지였고 순서대로 해결.
- 최종 설정: FC `SER_TEL2_BAUD=921600`, Pi
  `MAVLINK_BRIDGE_APP_SERIAL_BAUDRATE=921600`, UART 클럭 고정 유지.

## 교훈 / 재발 방지

- **배포 시 반드시 `make mission-all` 다음에 `make mission-install
  DESTDIR=...`까지 실행**하고, `.so`/`core-cpu1` md5sum을 빌드 트리와
  대조해서 실제로 갱신됐는지 확인하는 습관 필요 — 겉보기엔 정상 빌드/재시작
  성공으로 보여도 구버전이 계속 돌 수 있음.
- 증상이 안 바뀐다고 반드시 마지막 조치가 무효였다는 뜻은 아님 — 여러 원인이
  겹쳐 있을 수 있으므로 로그에서 구체적 실패 패턴(이번엔 msgid별 crc fail)을
  근거로 코드 레벨 재확인하는 게 baud/하드웨어 튜닝 반복보다 빨랐음.

## 관련
- `notes/fc_telemetry_rate_1_2hz_duplicate_completed.md`
- `notes/mavlink_stx_reentry_parser_bug_completed.md`
- `mavlink_bridge_app/config/default_mavlink_bridge_app_platform_cfg.h`
- `mavlink_bridge_app/fsw/src/mavlink_bridge_app_utils.c`
