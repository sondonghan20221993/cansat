# FC ATTITUDE 실제 수신율(~1.2Hz)이 요청 레이트(5Hz)에 미달 — boot_ms 정체 원인 (2026-07-14 도출)

## 배경

Stage 3(5Hz/DL2) soak 통과 후 `analyze_downlink_csv.py`가 경고:
"⚠ FC boot_ms 정체: 최대 연속 7회 동일값 (FC UART 끊김/캐시값 재전송 의심)".
DL2 다운링크는 5Hz(0% 손실)로 정상인데 그 안의 FC boot_ms(자세 타임스탬프)가
같은 값을 7회씩 반복하는 현상을 조사.

## 측정

지상 CSV(`telemetry_20260714_220516.csv`) DL2 2816행 분석:
- run length 분포: 6~8회 반복이 대부분(624/684 고유 boot_ms가 6~8 run)
- 고유 boot_ms 증가폭: median 1450ms (≈0.7Hz), mean 1427ms
- 7회 반복 × 200ms 주기 = 1400ms ≈ 관측된 boot_ms 갱신 주기

Pi 측 EVS 로그(`ATTITUDE decoded seq=... boot_ms=... rx_ms=...`):
- seq 1증가당 boot_ms/rx_ms 모두 ~800~840ms 전진
- 즉 **FC ATTITUDE가 Pi에 ~1.2Hz로 도착** (요청 200000us=5Hz의 1/4 수준)

## 원인 규명

1. **FC가 `SET_MESSAGE_INTERVAL`에 COMMAND_ACK를 전혀 안 보냄** — Pi 로그
   `22:00~` 구간에 `COMMAND_ACK cmd=511` 이벤트 0건, `StreamRequestAckCount`
   증가 흔적 없음.
2. ACK 누락이 수신 손실 때문은 아님 — 같은 구간 crc fail 22건인데 전부
   저빈도 메시지(msgid 2=SYSTIME 11건, 193=EKF 5건, 24=GPS 3건, 32/33=POS 3건).
   **ATTITUDE(30)/COMMAND_ACK(77)에는 crc fail 0건.**
3. 지상측 요청 송신은 정상 — `request failed` 0건, 2초마다 재시도
   (`STREAM_REQUEST_RETRY_MS=2000`)로 계속 SET_MESSAGE_INTERVAL 전송 중.
4. COMMAND_LONG/SET_MESSAGE_INTERVAL 페이로드 레이아웃 검증 완료(정상):
   param1(off0)=msgid, param2(off4)=interval_us, command(off28)=511,
   target_sys(off30)=1, target_comp(off31)=1, confirmation(off32)=0.
   타깃 sys=1/comp=1은 PX4 autopilot 표준값과 일치.

**결론**: cFS/지상 코드 버그 아님. **FC(MicoAir743v2, PX4)가 우리
SET_MESSAGE_INTERVAL 요청을 ack도 반영도 하지 않고 자기 기본 스트림레이트
(~1.2Hz)로 ATTITUDE를 내보내는 것.** DL2 5Hz 다운링크는 이 ~1.2Hz 소스를
5Hz로 재전송(같은 값 반복)하므로 boot_ms 정체가 발생.

분석기 경고 문구("UART 끊김/캐시 재전송 의심")는 증상은 맞게 짚었으나 근본
원인(FC 스트림레이트 미달)과는 다름 — 캐시 재전송 자체는 설계상 정상 동작
(RunTx가 최신 캐시를 5Hz로 송신, 소스가 느리면 반복되는 게 맞음).

## Stage 3와의 관계

**Stage 3 통과 판정은 유효.** Stage 3 목표는 "LoRa 링크/TDM이 5Hz를
버티는가"였고 그건 증명됨(다운링크 5.0Hz, seq gap 손실 0%). FC 자세 소스가
5Hz를 못 채우는 건 별개 레이어(FC↔Pi UART/FC 펌웨어 설정) 문제.

## 결정 — 미정 (조사만, FC측 사안)

이 프로젝트 코드로 고칠 수 있는 게 아니라 PX4 파라미터/설정 영역.
후속 확인 후보(실기체/QGC 필요):
- A: PX4 파라미터로 ATTITUDE 스트림레이트/대역폭 예산 상향
  (`MAV_x_RATE`, 해당 링크 MODE 설정). 57600baud 대역 예산이 ATTITUDE를
  down-rate하고 있을 가능성.
- B: PX4가 SET_MESSAGE_INTERVAL을 왜 ack 안 하는지 — 펌웨어 버전/타깃 comp
  재확인. (QGC로 동일 명령 보내 ack 오는지 대조하면 우리 프레이밍 문제인지
  FC 문제인지 즉시 판별 가능)
- C: 5Hz 자세가 실제로 필요 없으면(현 미션 요구 미확정) 현행 유지 —
  다운링크 5Hz 자체는 다른 데이터(위치/health)에도 쓰이므로 무의미하지 않음.

## 상태

- [x] 증상 측정 (지상 CSV + Pi EVS 양쪽)
- [x] 원인 규명 — FC가 SET_MESSAGE_INTERVAL 미ack/미반영, ~1.2Hz 기본 스트림
- [x] cFS/지상 코드 결함 아님 확인 (페이로드/타깃/재시도/CRC 전부 정상)
- [ ] FC측 확인 (QGC 대조 — 우리 프레이밍 vs PX4 동작 판별)
- [ ] 5Hz 자세 필요 여부 미션 요구 확정
- [ ] (필요시) PX4 파라미터 조정 후 재측정
