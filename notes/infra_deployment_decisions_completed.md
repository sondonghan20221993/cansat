# 인프라/배포 결정 이력 모음

## Pi USB-C vs GPIO UART 경로 결정


> **최종 결론 (2026-07-14, 종결)**: **USB 전환 계획 자체를 철회.** 보드(CM4-NANO-B)에
> GPIO14/15(UART, `/dev/serial0`)로 배선된 **전용 커넥터가 있음을 발견**, FC를 그
> 커넥터로 연결해 해결 — 기존 UART 경로(`MAVLINK_BRIDGE_APP_SERIAL_PATH=/dev/serial0`)
> 그대로 사용하므로 코드/설정 변경 불필요. 아래 USB-C/USB-A 조사 내용은 향후 참고용
> 기록으로 남긴다(핵심: 이 보드 USB-C는 CC 풀업 부재로 C-to-C host 불가, USB-A 포트
> +A-to-C는 동작 확인됨 — USB 경로가 다시 필요해지면 USB-A 포트 사용).
> 부수 성과로 남는 것: ① Pi 전원이 USB-C → GPIO 5V(핀2/4)로 이전됨(유지),
> ② `config.txt`의 `otg_mode=1` → `dtoverlay=dwc2,dr_mode=host` 변경도 유지
> (USB-A 포트 host 동작에 필요, 부작용 없음).

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

## 실측 확인 (2026-07-14, Pi 재부팅 없이 SSH로 확인 — 아직 변경 없음)

- `pinout` 실측: `Pi CM4 rev 1.1`, J8 40핀 헤더 — **pin 2/4 = 5V, pin 6 = GND,
  pin 8 = GPIO14, pin 10 = GPIO15** (계획에서 가정한 위치와 일치, 확인 완료).
- `core-cpu1`은 이전 세션에서 기동해둔 프로세스가 계속 실행 중(20:04부터).

### 정정 (2026-07-14) — config.txt 섹션 구조 오판 바로잡음

앞서 "`dtoverlay=dwc2,dr_mode=host`가 이미 있어 `otg_mode=1`만 지우면 된다"고
기록했으나 **틀렸음**. `/boot/firmware/config.txt` 전체 구조를 다시 보니:

```
[cm4]
# Enable host mode on the 2711 built-in XHCI USB controller.
# This line should be removed if the legacy DWC2 controller is required
# (e.g. for USB device mode) or if USB support is not required.
otg_mode=1

[cm5]
dtoverlay=dwc2,dr_mode=host

[all]
enable_uart=1
dtoverlay=disable-bt
```

`dtoverlay=dwc2,dr_mode=host`는 **`[cm5]` 섹션**에 있다. 이 Pi는 CM4이므로
`[cm5]` 블록은 **부팅 시 적용되지 않는다** — 죽은 설정이었음. `[cm4]`에는
`otg_mode=1`만 있는데, 주석을 보면 이건 **2711 내장 XHCI 컨트롤러(USB-A용
host 모드)를 켜는 것** — 이는 Raspberry Pi OS 기본 템플릿 문구이지 Waveshare가
넣은 게 아니며, CM4-NANO-B의 USB-C(legacy DWC2 경로로 추정)와는 별개일 가능성이
높다.

**정정된 조치**: `[cm4]` 섹션에서 `otg_mode=1`을 지우고, `dtoverlay=dwc2,dr_mode=host`를
`[cm5]`가 아니라 **`[cm4]`(또는 `[all]`)로 옮겨** 실제로 이 하드웨어에 적용되게
한다. (Waveshare wiki 원문 지침 — "otg_mode=1 제거 + dtoverlay=dwc2,dr_mode=host
추가" — 을 정확히 CM4 섹션에 적용하는 것으로 재해석)

## 실측 완료 — GPIO 5V 전환 + otg_mode 변경 + FC 인식 (2026-07-14)

- GPIO 5V(핀2/4)+GND 외부전원 배선 → USB-C 전원 분리 → 정상 부팅 확인
  (`throttled=0x0`, 온도/전압 정상).
- `config.txt` `[cm4]`: `otg_mode=1` → `dtoverlay=dwc2,dr_mode=host`로 교체
  (백업: `config.txt.bak-20260714`) 후 재부팅 → `dwc2 fe980000.usb: DWC OTG
  Controller` 로그로 host 모드 활성 확인.
- FC 연결 실측 (2026-07-14 사용자 확인으로 최종 정정):
  - **Pi USB-C 포트 ←C-to-C→ FC: 실패** (`lsusb`/`dmesg` 완전 무반응, 수 분간)
  - **Pi USB-A 포트 ←A-to-C→ FC: 즉시 인식 성공**
- **원인 분석**: USB-A 포트는 CC 협상 없이 항상 host 역할이라 정상 동작.
  USB-C 포트의 C-to-C 실패는 **CM4-NANO-B USB-C 포트에 host용 CC 풀업(Rp)이
  없어, `dr_mode=host` 소프트웨어 설정과 무관하게 C-to-C 연결 시 상대(FC)가
  host 존재를 전기적으로 감지하지 못하는 보드 설계 제약**으로 추정 — 이
  포트는 원래 전원 입력 + device(eMMC 플래싱)용으로 설계됨. (케이블 충전전용
  가능성도 배제 못 하나, 보드 설계상 C-to-C는 원래 안 될 가능성이 높음.)
- 참고: CM4는 USB2 인터페이스가 1개뿐이라 USB-A/USB-C 포트가 같은 dwc2
  컨트롤러를 공유 — `dtoverlay=dwc2,dr_mode=host` 설정은 USB-A 포트 host
  동작에도 필요했던 것이므로 헛수고는 아니었음 (`otg_mode=1` 제거 상태에서
  이 overlay 없으면 USB 인식 자체가 안 됨, Waveshare wiki 지침과 일치).
- **운영 기준 확정: FC 연결은 Pi USB-A 포트 + A-to-C 케이블 사용.**
  Pi의 USB-C 포트는 전원/플래싱 전용으로 간주(현재 전원은 GPIO 5V로 옮겼으니
  실질적으로 플래싱 전용).
- **FC 인식 성공**: `usb 1-1: Product: MicoAir743v2, Manufacturer: MicoAir`
  (idVendor=1b8c, idProduct=0036) → `cdc_acm 1-1:1.0: ttyACM0` →
  **`/dev/ttyACM0`**로 확정.
- FC 펌웨어 재확인: 사용자 확인 결과 **PX4**(레포 기존 문서의 ArduPilot 실측
  기록과 불일치 — `mavlink_bridge_app_behavior_spec.md`에 별도 기록,
  커밋 `9ef3777`).

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
- [x] `config.txt`/pinout 실측 확인 — `dtoverlay=dwc2,dr_mode=host`는 이미 존재,
      `otg_mode=1`만 삭제하면 됨. J8 핀 2/4=5V, 6=GND 위치 확인 완료
- [x] GPIO 5V 외부 전원(5V 2A 이상) 배선 완료, USB-C 전원 분리, 정상 부팅 확인
- [x] `config.txt`에서 `otg_mode=1` → `dtoverlay=dwc2,dr_mode=host` 교체 적용,
      재부팅 후 dwc2 host 모드 활성 확인
- [x] FC USB 인식 확인 — `/dev/ttyACM0` (MicoAir743v2) 확정. 단 **USB-C 포트가
      아니라 USB-A 포트**(A-to-C 케이블)로 연결됨 — USB-C C-to-C는 보드 설계
      제약(추정)으로 실패, 운영 기준은 USB-A 포트로 확정
- [x] ~~`MAVLINK_BRIDGE_SERIAL_PATH=/dev/ttyACM0` env var 검증~~ — **불필요로 종결**:
      보드의 GPIO14/15 UART 전용 커넥터 발견, FC를 그리로 연결해 기존
      `/dev/serial0` 경로 그대로 사용 (USB 전환 철회)
- [x] ~~compile-time 기본값 변경~~ — 불필요 (기본값이 이미 `/dev/serial0`)
- [ ] `integration_steps.md` 잔여 이슈("FC UART 링크 노이즈: crc fail
      msgid=24/30") — UART 경로 유지하므로 여전히 유효한 관찰 대상.
      새 전용 커넥터 배선에서 재발하는지 실제 텔레메트리 수신으로 확인 필요

---

## sch_lab bridge timeout 재발 이슈


## 문제

`spec_code_audit.md:181`(2026-06-17 최초 발견·해결 기록)과 **동일 증상이 재발**.
Pi 재빌드/재기동(오늘 lora_tdm 200ms 변경 적용 세션) 후:

- `MAVLINK_BRIDGE_APP`은 FC로부터 ATTITUDE/LOCAL_POSITION_NED를 정상 디코드 중
  (로그로 확인, 계속 seq 증가)
- 그런데도 `cfs_core_app: health 0->2->3 fault=1(BRIDGE_TIMEOUT)`로 부팅 30초 만에
  고착 — 6월 17일에 고쳤던 것과 완전히 같은 패턴
- 부수 증상: `UPLINK_APP: command blocked by health state=3 class=1` — CONFIG류
  명령이 health FAILED 때문에 전부 거부됨 (이번에 `lora_tdm.downlink_protocol=1`
  CONFIG 명령 보낼 때 처음 발견)

## 왜 문제인가

- 6월 17일 조치(`mission_defs/tables/cpu1_sch_lab_table.c` 신규 추가 — 커스텀 앱
  4개 `SEND_HK` MID를 ~1Hz로 스케줄링)로 해결됐던 문제인데, **원인 소스 자체는
  이번 재확인 결과 존재하고 내용도 정상**(`MAVLINK_BRIDGE_APP_SEND_HK_MID_VALUE`
  0x18A1 포함 4건 전부 있음, `~/cfs-telemetry-app/mission_defs/tables/cpu1_sch_lab_table.c`).
  즉 **소스 회귀는 아님.**
- 그런데 오늘 로그에는 `SCH_LAB` 로드 이후 **`SCH_LAB Initialized` 등 후속 EVS
  이벤트가 전혀 없음** — 6월 17일 수정이 반영된 상태에서도 `SCH_LAB` 앱 자체가
  스케줄을 정상 구동하지 못하는 것으로 보임(테이블 파일 자체 로드 실패 가능성,
  또는 EVS 필터로 로그만 안 보이고 실제로는 동작 중일 가능성 — 미확정).
- CONFIG류 명령이 health FAILED에서 전부 막히므로, **`FORCE_FLAG`(벤치 전용
  health gate 우회) 없이는 사실상 어떤 CONFIG 명령도 Pi에 반영 불가능한 상태** —
  이번엔 force로 우회해서 `lora_tdm.downlink_protocol=1` 전환에 성공했으나,
  근본 원인 미해결 상태로 넘어감.

## 근본 원인 확정 (2026-07-14) — SCH_LAB 아님, HK mirror 구조체 레이아웃 불일치 회귀

**후보 A/B/C(SCH_LAB 미동작) 전부 오진.** 실측으로 SCH_LAB은 정상 동작 확인:
- `CFS_CORE_APP HK: mission_wp=...` 로그가 매 초 출력됨 — 이건
  `CFS_CORE_APP_ReportHousekeeping()`가 SEND_HK(`0x18C1`)로 트리거된 것.
  즉 SCH_LAB이 스케줄 테이블(4개 엔트리 전부 포함)을 로드·실행 중이라는
  직접 증거. `sch_lab_table.tbl`도 정상(2552B, 당일 재생성), 시스로그 에러 0건.

**진짜 원인 — 구조체 레이아웃 불일치:**
- 발행측 `MAVLINK_BRIDGE_APP_HkTlm_t`(msgstruct.h): `...ParseErrorCount(u32),
  NonFiniteValueCount(u32), LastRxTimestampMs(u32)...`
- 수신측 `CFS_CORE_APP_BridgeHkMirror_t`(cfs_core_app_utils.h): `...ParseErrorCount(u32),
  LastRxTimestampMs(u32)` — **`NonFiniteValueCount`가 빠져 있음.**
- 결과: mirror가 `LastRxTimestampMs`를 4바이트 앞에서 읽어 발행측의
  `NonFiniteValueCount`(정상 운용 시 0) 값을 타임스탬프로 오독.
  `LinkState`/`LastErrorCode`는 오프셋이 같아 정상.
- `cfs_core_app_utils.c:245`:
  `BridgeTimedOut = !Received || (NowMs - LastRxTimestampMs) > BridgeTimeoutMs`.
  `LastRxTimestampMs`가 0으로 읽히니 `NowMs - 0 = NowMs`(부팅 후 경과 ms)가
  항상 `BridgeTimeoutMs` 초과 → **BRIDGE_TIMEOUT 영구 참 → health FAILED 고착.**

**회귀 출처**: 커밋 `947b3cf`(2026-07-13, `fc_value_validation_gap` NaN/Inf 검증
수정)가 발행측 `HkTlm_t`에 `NonFiniteValueCount`를 삽입했으나, cfs_core의 mirror
구조체를 동기화하지 않음(`git log -S NonFiniteValueCount -- cfs_core_app_utils.h`
결과 없음 = mirror엔 한 번도 존재한 적 없음). 6월 17일 SCH_LAB 이슈와는 무관한
별개의 신규 버그였고, "재발"로 오인한 것.

## 결정

mirror 구조체(`CFS_CORE_APP_BridgeHkMirror_t`)에 `NonFiniteValueCount(u32)`를
발행측과 동일하게 `ParseErrorCount`와 `LastRxTimestampMs` 사이에 삽입해 레이아웃
정합. (더 근본적으로는 mirror-복사 패턴 자체가 이런 드리프트에 취약 — 공용 헤더
공유가 이상적이나 이번은 최소 수정으로 필드 추가만.)

## 상태

- [x] 재발 확인 (2026-07-14) — health FAILED 고착, CONFIG 차단
- [x] `FORCE_FLAG`로 즉시 우회, `lora_tdm.downlink_protocol` v2 전환 성공
- [x] 근본 원인 확정 — SCH_LAB 정상, mirror 구조체 `NonFiniteValueCount` 누락
      (커밋 `947b3cf` 회귀). A/B/C 후보 전부 기각.
- [x] SCH_LAB 정상 동작 실측 확인 (cfs_core HK 매 초 = SEND_HK 수신 증거)
- [x] mirror 구조체에 `NonFiniteValueCount` 추가 (레이아웃 정합) — 커밋 `3164020`
- [x] 단위테스트 회귀 확인 — cfs_core UT 245/245 PASS (coveragetest의 fake
      구조체도 실제 발행측 레이아웃 반영하도록 함께 수정)
- [x] Pi 재빌드/재기동 후 검증 (2026-07-14) — **BRIDGE_TIMEOUT(fault=1) 해소.**
      부팅 첫 ~1초(첫 BRIDGE_HK 도착 전)만 fault=1, 이후 재발 0건. health가
      `2->1 fault=3`(DEGRADED, EKF_INVALID)로 안정 — 벤치에서 GPS/EKF 미확보인
      실제 FC 상태를 정확히 반영(더 이상 살아있는 bridge를 죽었다고 오판 안 함).
      **부수 효과**: health가 이제 실제 FC 상태에 따라 움직이므로, FC가 NOMINAL이
      되면 CONFIG 명령도 FORCE_FLAG 없이 통과할 수 있게 됨(기존엔 영구 FAILED라
      CONFIG가 항상 차단됐음).
