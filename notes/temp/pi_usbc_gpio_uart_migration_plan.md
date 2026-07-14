# Pi USB-C 데이터모드 전환 + GPIO14/15 UART 해제 계획 (2026-07-14 도출, Pi 종료 상태에서 기록만)

## 문제

Pi(CM4)의 USB-C 포트를 데이터 입력(OTG)으로 전환하고, 기존에 UART(`/dev/serial0`)로
쓰던 GPIO14/GPIO15(TXD0/RXD0)를 해제하려는 계획. Pi가 방금 종료돼 실제 설정 파일
확인/적용은 다음 부팅 후 진행 필요 — 본 문서는 계획만 선기록.

## 현재 확인된 상태 (기존 notes 기준 + 사용자 확인)

- 캐리어보드: **Waveshare CM4-NANO-B** (사용자 확인, 2026-07-14)
- 목적: **FC 보드와의 통신용** (사용자 확인) — 즉 시나리오 (a): 기존 GPIO14/15 UART
  기반 FC 연결을 USB-C 데이터 연결로 옮기거나 보완하려는 것.
- `notes/uplink_lora_test_status.md` 확인: Pi는 CM4, `/dev/serial0 → ttyAMA0`
  (PL011, GPIO14/15 = TXD0/RXD0)를 `mavlink_bridge_app`의 FC 시리얼 연결로 사용 중.
  콘솔 리다이렉트는 이미 제거됨(로그인 콘솔과 경쟁 없음), `dtoverlay=disable-bt`도 적용됨.
- LoRa(CP2102)는 별도 USB-UART 어댑터로 GPIO14/15와 무관 — `lora_tdm_app`이 자동탐지.

## 웹 조사 결과 (2026-07-14, Waveshare CM4-NANO-B wiki)

- USB-C는 **전원 공급 또는 USB 데이터(host/slave) 겸용** 포트.
- **기본값은 OTG(slave/device) 모드** — `config.txt`의 `[cm4]` 섹션에 `otg_mode=1`이
  설정돼 있어, 기본적으로 eMMC 이미지 플래싱용 USB 슬레이브로 동작.
- **host 모드로 바꾸려면**: `otg_mode=1` 제거 + `dtoverlay=dwc2,dr_mode=host` 추가
  (이거 없으면 USB 인식 자체가 안 됨).
- USB2.0 인터페이스 자체가 기본 비활성 상태이므로 위 dtoverlay 설정이 곧 "활성화" 역할도 겸함.

## 핵심 발견 — GPIO14/15는 애초에 "해제"할 필요가 없음

CM4의 PL011 UART(GPIO14/15, `/dev/serial0`)와 USB2 OTG 컨트롤러(dwc2, USB-C로 배선)는
**완전히 독립된 하드웨어 블록**이다. USB-C를 host 데이터모드로 켠다고 GPIO14/15가
해제되거나 충돌하지 않는다 — 애초에 서로 다른 버스. 즉 "기존에 GPIO14/15를 입력으로
했을 텐데"라는 전제 자체가 이번 변경과 무관: GPIO14/15 UART 설정은 그대로 둬도 된다.

## FC 통신 방식 — 두 가지 선택지로 재정의됨

1. **USB-C를 FC 연결의 대체재로 사용** (GPIO UART 대신):
   FC가 USB로도 MAVLink를 낼 수 있다면(ArduPilot/PX4 보드 대부분 USB CDC-ACM 지원),
   Pi가 host 모드로 열리면 FC가 `/dev/ttyACM0` 등으로 잡힌다.
   `mavlink_bridge_app`은 이미 `MAVLINK_BRIDGE_SERIAL_PATH` env var override가
   구현돼 있음(`tests/TEST_CASES.md` B-1, `mavlink_bridge_app_utils.c:22`) —
   코드 변경 없이 `MAVLINK_BRIDGE_SERIAL_PATH=/dev/ttyACM0` 하나로 전환 테스트 가능.
   **부수 효과**: `integration_steps.md` 잔여 이슈("FC UART 링크 노이즈: crc fail
   msgid=24/30")가 GPIO UART 특유의 문제라면 USB 연결로 회피될 가능성 있음 — 검증 가치 있음.
2. **USB-C를 GPIO UART와 병행/보조로 사용** (예: 별도 디버그/명령 채널):
   이 경우 용도를 더 구체화해야 함 — 아직 목적 불명확.

**결정 (2026-07-14, 사용자 확인)**: **1번 채택 — 완전 대체.** 기존 GPIO14/15
UART FC 연결을 폐기하고 USB-C를 FC의 유일한 연결 경로로 쓴다. GPIO UART 관련
설정(`/dev/serial0` open 경로)은 코드에서 제거하지 않고 기본값만 바꾸는 방향으로
간다 — 물리 배선이 남아있어도 무해하고, 만약 USB 연결에 예상 못한 문제가 생기면
`MAVLINK_BRIDGE_SERIAL_PATH=/dev/serial0`로 즉시 롤백 가능해야 하므로.

**롤아웃 순서 결정**: `default_mavlink_bridge_app_platform_cfg.h`의 compile-time
기본값(`MAVLINK_BRIDGE_APP_SERIAL_PATH`)은 **지금 바꾸지 않는다.** 먼저 Pi 부팅 후
`MAVLINK_BRIDGE_SERIAL_PATH=/dev/ttyACM0`(실제 장치명은 `lsusb`/`dmesg`로 재확인)
env var override로 실측 검증 → 안정 확인되면 그때 compile-time 기본값을 변경한다.
(장치명이 `ttyACM0`이 아닐 수 있음 — USB 인식 순서/모델에 따라 달라질 수 있으므로
env var 단계에서 실제 이름부터 확정)

## 추가 확인 필요 — 전원 공급 경로 (2026-07-14, 중요/차단 이슈)

CM4-NANO-B는 **USB-C가 "전원 공급 또는 USB 데이터(host/slave)" 겸용**이다.
`dr_mode=host`로 바꾸면 그 포트는 전원을 "받는" 방향이 아니라 연결된 기기에
"공급하는" 방향으로 동작할 가능성이 높다 — 지금 Pi 전원을 USB-C로 공급받고 있다면
**설정 변경 후 재부팅 시 Pi 자체가 전원을 못 받아 부팅 실패할 위험**이 있음.

웹 조사에서 관련 안전 경고 확인: "5V 헤더와 USB-C로 동시에 외부전원 연결 금지 —
CM4/보드 영구 손상 가능" — 이는 **USB-C 외에 별도 5V 전원 헤더가 보드에 존재함**을
시사하나, CM4-NANO-B의 정확한 헤더 위치/핀번호는 검색으로 확정하지 못함(Waveshare
공식 위키/스키매틱 원문 확인 필요).

**전제 조건**: `otg_mode` 변경을 적용하기 **전에** 반드시:
1. Pi 부팅 후 현재 실제 전원 공급 경로가 USB-C인지 확인
2. USB-C가 전원 경로라면, CM4-NANO-B의 대체 5V 전원 헤더(핀 위치)를 보드 실물/스키매틱에서
   확인
3. 대체 전원 경로 확보(케이블/커넥터 준비) 후에만 `config.txt` 변경 적용
   (전원 경로가 이미 USB-C가 아니라 별도 헤더/PoE 등이라면 이 단계는 불필요 — 확인만 하면 됨)

**결정 (2026-07-14, 사용자 확인)**: 대체 전원 경로로 **40핀 GPIO 헤더의 5V 핀
(표준 라즈베리파이 배치 — 핀 2/4 = 5V, 인접 GND 핀 사용)**을 채택. 이 보드는
"라즈베리파이 HAT 호환 40핀 GPIO 헤더"로 표준 배치를 따름(웹 조사로 확인).

**필수 안전 수칙**:
- 외부 5V 공급은 **5V 2A 이상** 안정적인 전원이어야 함(CM4 권장 스펙) — 스펙 미달 시
  자동 종료/주파수 저하 발생 가능.
- **USB-C와 GPIO 5V를 절대 동시에 연결하지 않는다** — 두 전원 경로가 동시에 살아있으면
  역전류로 CM4/보드가 영구 손상될 수 있음(웹 조사에서 확인된 공식 경고). GPIO 5V로
  전환한 뒤에는 USB-C 케이블의 전원 관련 결선은 완전히 분리(데이터 전용 케이블 사용
  또는 VBUS 핀 미결선 케이블 확인)해야 함.
- 순서: **① GPIO 5V 전원 배선 준비 및 검증 → ② USB-C 전원 케이블 분리 →
  ③ GPIO 5V로 부팅 확인 → ④ 그 다음에 `otg_mode` 변경 + FC USB-C 데이터 연결.**
  전원 전환과 OTG 모드 전환을 동시에 하지 않는다(문제 발생 시 원인 분리 어려워짐).

## 다음 부팅 시 확인/적용 절차 (초안)

```bash
# 1. 현재 config.txt의 otg_mode 확인
grep -n "otg_mode\|dtoverlay" /boot/firmware/config.txt 2>/dev/null || \
grep -n "otg_mode\|dtoverlay" /boot/config.txt

# 2. host 모드 전환 (1번 선택지 채택 시)
#    [cm4] 섹션에서 otg_mode=1 제거, dtoverlay=dwc2,dr_mode=host 추가
#    편집 후 재부팅 필요

# 3. FC를 USB-C에 연결 후 인식 확인
lsusb
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
dmesg | tail -30

# 4. mavlink_bridge_app 연결 전환 테스트 (env var override, 코드 변경 없음)
MAVLINK_BRIDGE_SERIAL_PATH=/dev/ttyACM0 sudo -E ./core-cpu1
# (기존 GPIO14/15 FC 연결은 그대로 둔 채 병행 검증 가능 — 물리적으로 분리된 포트이므로
#  동시에 두 케이블 다 꽂아놓고 비교 테스트도 가능)
```

## 상태

- [x] 계획 문서 선기록 (Pi 종료 상태, 실제 확인/적용 전)
- [x] 캐리어보드 확인 — Waveshare CM4-NANO-B
- [x] 목적 확인 — FC 보드 통신용
- [x] USB-C host 모드 전환 방법 조사 완료 (otg_mode 제거 + dtoverlay=dwc2,dr_mode=host)
- [x] GPIO14/15와 USB-C가 독립 하드웨어임을 확인 — "GPIO14/15 해제" 불필요로 판명
- [x] 대체(1번) vs 병행(2번) 최종 확인 — **완전 대체로 결정**
- [x] 기본값(compile-time) 변경 시점 결정 — **지금 아님, env var 실측 검증 후**
- [x] **(선행 필수)** 대체 전원 경로 결정 — GPIO 40핀 헤더 5V(핀 2/4)+GND 채택
- [ ] GPIO 5V 외부 전원(5V 2A 이상) 배선 준비
- [ ] GPIO 5V로 전원 전환, USB-C 전원 결선 완전 분리 확인, 정상 부팅 검증
      (이 단계에서는 아직 `otg_mode` 변경 안 함 — 전원 전환만 먼저 단독 검증)
- [ ] `config.txt` 변경 적용 (전원 전환 검증 후: `otg_mode=1` 제거 +
      `dtoverlay=dwc2,dr_mode=host`)
- [ ] FC를 USB-C로 연결 후 인식 확인 (`lsusb`, `/dev/ttyACM*` 실제 장치명 확정)
- [ ] `MAVLINK_BRIDGE_SERIAL_PATH=<실제장치명>` env var로 `mavlink_bridge_app` 연결 검증
- [ ] 안정 확인되면 `default_mavlink_bridge_app_platform_cfg.h`의
      `MAVLINK_BRIDGE_APP_SERIAL_PATH` 기본값을 실제 장치명으로 변경(커밋)
- [ ] 기존 GPIO UART 대비 CRC/노이즈 개선 여부 비교 — `integration_steps.md`
      잔여 이슈("FC UART 링크 노이즈: crc fail msgid=24/30") 해소 여부 검증
- [ ] 안정화 후 GPIO14/15 물리 배선 제거(선택, 소프트웨어적으로는 무관하므로 필수 아님)
