# LoRa Uplink 실시간 테스트 — 현황/문제/해결 (2026-06-15 ~ 2026-07-10 완료)

> uplink 명령(지상국 → 드론) LoRa 경로 실시간 검증 기록. §1-9 전 항목 결론 확정
> (§8.4 EKF 게이트는 옵션 A 채택으로 최종 결정, 실외 재검증만 잔여) — temp에서 승격.

## 1. 목표
지상국(OpenMCT) CLI에서 보낸 CONFIG/RECOVERY 명령이 LoRa를 통해
드론 `uplink_app`까지 도달·처리되는지 실시간 확인.

## 2. 경로 (설계)
```
OpenMCT Uplink CLI
  → fc_serial_ws_server.py (지상, HTTP/api/uplink → LoRa serial TX)
    → LoRa RF
      → Pi CP2102 (lora_fc_downlink_app 단독 소유)
        → TDM RX 윈도우에서 "UP,..." 수신 → UPLINK_RAW_MID(0x1909) SB publish
          → uplink_app 구독 → ParseLoRaFrame → ProcessUplink → 대상앱 라우팅
```
반이중 TDM: Pi RX 윈도우는 downlink TX 직후 **300ms만** 열림.

## 3. 검증된 것 (OK)
- 지상 브리지 슬롯 정렬 + 자동 재전송(`_UPLINK_RETX=4`): 콘솔에
  `[UP] CONFIG ... queued` → `[UP->slot] (1/4)~(4/4)` 정상 출력. 각 downlink 직후 송신.
- RECOVERY 경로(이전 세션, 수동 반복 전송): `uplink_app`까지 도달 →
  `UPLINK_APP: command blocked by health state=3 class=4` 확인.
  → Ground→LoRa→lora_fc_downlink_app→LORA_RAW_MID→uplink_app 경로 자체는 동작.
- CONFIG 코드 체인: uplink `ForwardConfigCommand` → `CONFIG_CMD_MID(0x190E)` publish
  → mavlink_bridge `ProcessConfigCommand` 적용 + `StreamRequestPending` + EVS
  `config activated ...`. (코드상 정상, 도달+health=0이면 적용됨)

## 4. 만난 문제와 해결

### 문제 A — 단발 uplink 미도달 (해결)
- 증상: 1회 전송은 무응답, 여러 번 빠르게 보내면 적중.
- 원인: 단발은 Pi의 300ms RX 윈도우 한 슬롯만 노려 타이밍 지터/RF 손실로 자주 빗나감.
- 해결: 지상 `fc_serial_ws_server.py`가 동일 프레임을 **연속 4개 downlink 슬롯에 자동 재전송**.
  `uplink_app`의 `IsSequenceAccepted`가 중복(seq)을 무시 → 1발만 적용, 나머지 replay 거부(무해).

### 문제 B — uplink 전혀 미수신 (RX 윈도우 즉시 종료 버그) (해결, 검증 대기)
- 증상: 지상은 4발 전송 확인되는데 Pi `uplink_app` 로그에 아무것도 안 뜸. 다운링크는 정상(비대칭).
- 원인: LoRa 포트가 `VMIN=0/VTIME=0`이라 데이터 없으면 `read()`가 즉시 0 반환.
  RX 윈도우 코드가 빈 read에 `break` → 윈도우 열리자마자(uplink 도착 전, RF 왕복 지연) 종료
  → uplink/HB를 영영 못 읽음.
- 해결(커밋 `cac209f`): 빈 read 시 `break` 대신 `usleep(2ms)` 후 **deadline까지 폴링 유지**.
  `lora_fc_downlink_app_utils.c` ServiceLoRa RX 윈도우.
- 상태: Pi 재빌드 후 재검증 필요.

### 문제 C — CONFIG "적용" 차단 (정책)
- 증상: 도달해도 CONFIG 미적용.
- 원인: `uplink_app_cmds.c` health-block 정책 — **FAILED(3)는 전 명령 차단**,
  DEGRADED(1)/RECOVERY(2)도 CONFIG 차단. CONFIG는 **NOMINAL(0)에서만** 허용.
- 실측 health 흐름: `0→2(0.75s)→3(51s)`, fault=1 = **BRIDGE_TIMEOUT 분기**
  (FC 텔레메트리 전체 끊김: ATTITUDE/LOCAL 4회뿐, GPS/EKF 0회 → bridge stale).
- **GPS 결합 문제 (결정됨)**: 설령 링크가 살아나도 GPS fix 없으면(실내) `GpsUnavailable→DEGRADED`로
  CONFIG가 영영 차단됨. cFS health는 통신-계층 상태여야 하고 GPS는 센서/비행 조건이므로
  **GPS를 health 게이트에서 분리(A안)** 하기로 결정.
  - md 갱신 완료: `cfs_core_app_behavior_spec.md §12.5/§13.2/테스트`,
    `mission_app_runtime_spec.md §15 GPS 정책/§5.1.1/테스트`.
  - **코드 변경 완료**: `cfs_core_app_utils.c` `else if (GpsUnavailable)` 헬스 분기 제거
    (GpsUnavailable 계산은 `GpsStatus.TimedOut` 보고 필드용으로 유지). 빌드 OK.
  - **단위 테스트 갱신 완료**(coveragetest_cfs_core_app_utils.c): GpsStale/GPS_Timeout →
    NOMINAL+보고 검증, SaveState_OnTransition·StabilityTimerReset 재-fault 트리거를
    GPS→EKF stale로 교체. 전체 193/193 PASS.
- 잔여: GPS 분리 후에도 **EKF/local/attitude/bridge** 가 fresh해야 NOMINAL.
  현재 FC 링크 자체가 불안정(UART CRC fail) → 별도 해결 필요. EKF 헬스 반영 여부는 추후 검토.

## 5. 부가 관찰
- FC UART 링크 노이즈: `crc fail msgid=24/30`, `Parse/data error code=4`, stream request 대상
  sys가 1/31/58/90/245로 흔들림 → FC 시리얼 잡음. 깨진 프레임만 폐기되나 telemetry 간헐 + health 저하 원인.

## 6. 런타임 검증 결과 (2026-06-15, b3d93dd)
- **실행 전제**: Pi에서 `sudo ./core-cpu1` 필수. 비-root 실행 시 `OS_API_Init() failure`
  (이전 sudo 실행이 남긴 `/dev/shm/osal:RAM` root 소유). 앞으로 **항상 sudo**로 고정.
- **GPS 분리 PASS**: `fault=5`(GPS_STALE) 전이 **부재**. health `0→2→3` 전부
  **`fault=1`(BRIDGE_TIMEOUT)** 단일 원인 → GPS는 health 게이트에서 완전 분리됨(실증).
- **잔여 차단 원인 = `fault=1` BRIDGE_TIMEOUT** (FC 텔레메트리 전체 stale). GPS 무관,
  별도 작업(§5: FC UART `crc fail` / stream request 흔들림 해소 → bridge fresh).
- CONFIG 적용(`config activated`)은 NOMINAL 필요 → FC 링크 복구 후 재검증.

## 7. 다음 단계
1. [별도] FC UART 링크 안정화 → bridge fresh → health NOMINAL 도달 (§5).
2. NOMINAL 후 CONFIG 1회 → `config activated` 적용 확인(문제 C 최종 마감).
3. 확정 시 본 노트를 `notes/integration_steps.md`로 승격, temp 정리.
   integration_steps §11/§12 빌드·실행 절차 stale → 실제(cmake + sudo 실행)로 갱신.

## 7. 관련 위치
- 드론: `lora_fc_downlink_app_utils.c`(RX 윈도우/ForwardUplinkFrame),
  `uplink_app_cmds.c`(health-block/ProcessUplink), `uplink_app_dispatch.c`(LORA_RAW_MID),
  MID `UPLINK_RAW=0x1909` / `CONFIG_CMD=0x190E`.
- 지상: openMCT `fc_serial_ws_server.py`(슬롯정렬+재전송), `openmct_bridge_notes.md`.

## 8. BRIDGE_TIMEOUT 원인 재조사 (2026-07-09)

> §4 문제 C의 잔여 원인(BRIDGE_TIMEOUT, §5 FC UART 노이즈)을 Pi(CM4, 192.168.50.65) 실측으로 재검증.

### 8.1 기각된 가설
- **GPS 미보정**: §12.5/§12.7에서 GPS는 이미 health 게이트 분리 완료, §6 실측에서 `fault=5(GPS_STALE)` 전이 0건 확인됨 → BRIDGE_TIMEOUT과 무관 (재확인, 기각).
- **stream request 대상 sysid 떨림(1/31/58/90/245)이 손상 프레임 탓**: 논리적으로 성립 안 함(CRC 깨진 프레임은 파싱 전 폐기되어 sysid로 채택 불가). 실제 원인은 **주변기기 heartbeat 오인식**이며 커밋 `7888a35`(2026-06-17, "FC sysid lock-in — autopilot 타입으로 식별, 주변기기 하트비트 무시")로 이미 해결됨. §5 관찰은 이 수정 이전 시점.
- **CM4 mini-UART 클럭 드리프트**: 실측 결과 Pi는 CM4이며 `/dev/serial0 → ttyAMA0`(PL011, 코어클럭 무관 안정 UART) 사용 중. 블루투스도 `dtoverlay=disable-bt`로 이미 비활성화됨. 이 가설 자체가 대상 하드웨어에 해당 없음(기각).

### 8.2 확인된 살아있는 원인 — 커널 콘솔이 FC UART 공유
- 실측(`/proc/cmdline`, dmesg): `console=ttyAMA0,115200` 활성. `printk: legacy console [ttyAMA0] enabled` 부팅 로그로 확정.
- `serial-getty@ttyAMA0.service`는 masked/inactive — 로그인 프롬프트 경쟁은 이미 없음(부분 완화 기 적용).
- 그러나 **커널 printk 콘솔 리다이렉트 자체는 여전히 ttyAMA0로 활성** → 커널 이벤트(usb/driver/thermal 등) 발생 시 115200 baud로 텍스트가 mavlink_bridge(57600 baud) 프레임 사이에 삽입 → CRC 불일치 → FC가 프레임 폐기 → stream request 미도달 가능.
- **범위 한정**: 이 메커니즘은 Pi→FC 송신 방향만 설명. §5의 Pi **수신** 측 `crc fail msgid=24/30`은 별도 원인(배선/노이즈 등) 가능성 있음 — 커널 콘솔 제거로 전부 해결된다는 보장 없음.
- 현재 cFS 구동 중(`core-cpu1`, PID 719) 실제 CRC fail 발생 여부는 미확인 — mavlink_bridge_app이 자체 로그 파일을 쓰지 않고 CFE EVS로만 이벤트 발행하므로, 지상국(OpenMCT) 연결 없이는 SSH만으로 실시간 확인 불가.

### 8.3 조치 계획
1. [x] `/boot/firmware/cmdline.txt` 백업 → `cmdline.txt.bak_20260709` (2026-07-09 21:08).
2. [x] `console=serial0,115200 ` 토큰 제거, `console=tty1`은 유지. (SSH `ssh -t` 대화형 sudo로 실행)
3. [x] cFS(`core-cpu1` PID 719) 정상 종료 후 Pi 재부팅 완료 (2026-07-09 21:09:38 부팅).
   - 검증: `/proc/cmdline` = `console=tty1 ...` (ttyAMA0 토큰 제거 확인), dmesg `[tty1] enabled`만 등록, ttyAMA0는 순수 UART 드라이버로만 등록(콘솔 아님).
   - `/dev/serial0 → ttyAMA0` 유지, mavlink_bridge_app 접근 경로 이상 없음.
4. [x] cFS 자동 재기동 확인 — `cfs.service`(systemd)로 부팅 시 자동 시작되게 등록되어 있음. 재부팅 직후 `core-cpu1`(PID 723) 정상 기동.
5. [x] **지상국 실측 검증 완료** (2026-07-09, `fc_serial_ws_server.py` 라이브 캡처, seq 213~401, 약 2.5분 연속):
   - 전 프레임 `[OK]`, `packet_loss: 0.0`, heartbeat 끊김 없이 순차 증가 → **CRC fail 재발 없음, 링크 안정**.
   - `fault_code`가 시종 `3`(EKF_INVALID)만 관측됨 — **`1`(BRIDGE_TIMEOUT)이 한 번도 안 나타남** → 커널 콘솔 오염 문제 해결 실증됨.
   - `health_state: 1`(DEGRADED, `CFS_CORE_APP_HEALTH_DEGRADED`) 유지 — NOMINAL 미도달.

### 8.4 새 블로커 — EKF_INVALID(fault=3)로 DEGRADED 고착
- §13.3 조건(EKF 메시지 미수신 / timestamp>2000ms / `Valid==0` / `Stale!=0`) 중 하나 이상 충족.
- attitude(roll/pitch/yaw)·local(x/y/z/vx/vy/vz)은 정상 갱신 중 → bridge 자체는 살아있음, **EKF_STATUS_REPORT(MAVLink msg 193) 스트림만 문제**.
- **원인 확정(2026-07-09, 사용자 확인)**: 실내 + GPS 보정 미진행. FC EKF는 GPS 등 위치 보정 입력 없이는 정렬되지 않으므로, 이 환경에서 `EKF_INVALID`(미정렬)는 **버그가 아니라 정상적으로 예상되는 FC 내부 상태**. `mavlink_bridge_app`/코드 문제 아님 — (a) 가설(스트림 request 누락) 기각, (b) 가설(미정렬) 확정.
- **정책 갈림길**: GPS는 이미 §12.5/§12.7에서 health 게이트 분리(실내 테스트를 막지 않기 위한 결정)됐으나, **EKF는 아직 분리 안 됨** → 실내 테스트에서는 GPS 때와 동일한 이유로 CONFIG가 계속 막힘.
  - 옵션 A: 실외 GPS 보정 후 재검증 (EKF 자연 정렬, 코드 변경 없음)
  - 옵션 B: EKF도 GPS처럼 health 게이트에서 분리(보고 전용화) — 단, EKF는 비행 중 자세/위치 추정 신뢰도와 직결되어 GPS보다 안전 영향이 큼. 분리 여부는 별도 결정 필요(구두 확정 전까지 보류).
- **결정 (2026-07-10)**: 옵션 A 채택. 실외 비행시험에서 GPS fix 확보 시 EKF는 자연 정렬되므로 코드 변경 불필요.
  EKF는 비행 중 자세/위치 추정 신뢰도에 직결되어 GPS보다 안전 영향이 크므로, health 게이트 분리(옵션 B)는 보류.
  실내 `EKF_INVALID` 고착은 실외 시험에서 재발하지 않을 것으로 예상 — 실외 재검증 시 확인.
- 결론: 커널 콘솔 수정 검증은 완료. 남은 건 실내 테스트 한계(EKF 미정렬)이지 버그가 아님. 정책 결정 완료, 실외 실측 확인만 잔여.

> 참고: 아래 §8.3의 마지막 항목(SSH 원격 sudo)은 조치 계획의 부속 메모.

6. [x] SSH 원격 sudo는 비밀번호 필요(키인증과 별개) 확인 → `ssh -t`로 대화형 실행하면 동작함.

## 9. cFE 시간 에폭 표시 (1980 → 2026) (2026-07-09)

### 9.1 현상 / 원인
- OpenMCT/cFS 텔레메트리 시간이 1980년 기준으로 표시됨.
- 원인: cFE `CFE_TIME`의 기본 에폭 = 1980-01-01 (`DEFAULT_CFE_MISSION_TIME_EPOCH_YEAR 1980`, `cfe/modules/time/fsw/inc/cfe_time_interface_cfg.h:187`). 지상 SET_TIME 동기화가 없으면 "에폭 + 부팅 후 경과시간(MET)"으로 표시되어 연도가 1980으로 나옴.
- 지상국 `fc_serial_ws_server.py` 로그의 `"timestamp"`(예 1783599533545)는 PC OS 시계라 정상(2026) — cFS 내부 시간 필드만 1980.

### 9.2 판단
- 기능 무해: health/CONFIG/타임아웃 로직은 전부 `boot_ms`(MET) 기준 → 실제 날짜 무관. 사용자 목적은 **표시 숫자만 2026으로** (실제 벽시계 동기화 아님).
- 에폭만 2026으로 바꾸면 표시는 "2026-01-01 + MET"이 되어 여전히 실제 날짜와는 다름(정확한 실시각 원하면 SET_TIME 동기화 필요) — 사용자 확인 후 "표시만" 방식 선택.

### 9.3 배선 확인
- 빌드는 mission override를 안 씀: `build/inc/cfe_time_interface_cfg_values.h` → `default_cfe_time_interface_cfg_values.h`(`CFGVAL(x) = DEFAULT_..##x`) → 값은 라이브러리 기본 `DEFAULT_CFE_MISSION_TIME_EPOCH_YEAR`가 그대로 유효.
- `MISSION_DEFS = /home/sdh2983/cfs-telemetry-app/mission_defs`엔 시간 설정 없음. `cFS_clean/sample_defs/example_mission_cfg.h`의 EPOCH_YEAR는 include 경로에 없어 무효.
- 결론: 실제 값 변경 지점은 `cfe_time_interface_cfg.h:187` 한 곳뿐.
- **주의**: 이 파일은 vendored cFS 라이브러리(`cFS_clean`) 소속 — 앱 git(`cfs-telemetry-app`)에 안 남고 cFS 재-clone 시 소실됨.

### 9.4 조치 상태
- [x] WSL 로컬 사본(`/home/sdh2983/cFS_clean`, x86_64) EPOCH_YEAR 1980→2026 수정. **단 이 사본은 Pi와 무관**(로컬 dev/test 빌드용).
- [x] **Pi(ARM, aarch64) 적용 완료** (2026-07-09 21:40):
  - ① `sudo systemctl stop cfs.service` (자동재시작 방지) →
  - ② Pi `cfe_time_interface_cfg.h:187` EPOCH_YEAR 1980→2026 `sed` 수정 (확인: `187:#define DEFAULT_CFE_MISSION_TIME_EPOCH_YEAR 2026`) →
  - ③ Pi에서 `make && make install` 재빌드 성공(100% Built). 바이너리 `core-cpu1` 갱신 시각 21:39, 빌드 스탬프 `202607092139` 확인 →
  - ④ `sudo systemctl start cfs.service` → PID 4146, service `active` 정상 기동.
- [x] 최종 확인: OpenMCT/텔레메트리 시간 표시 2026 기준으로 정상 작동 (2026-07-09, 사용자 육안 확인).

### 9.5 영구화 완료 — mission_defs override로 이전 (2026-07-09)
- **방식**: 라이브러리 원본을 안 건드리고 앱 소유 `mission_defs/config/cfe_time_mission_cfg.h`로 override.
  - cFS 빌드(`generate_configfile_set`→`generate_config_includefile`→`cfe_locate_implementation_file`, global_functions.cmake)는 `TIME_MISSION_CONFIG_FILE_LIST`의 `cfe_time_mission_cfg.h`에 대해 **MISSION_DEFS/config/ 를 먼저 검색**, 있으면 라이브러리 `default_cfe_time_mission_cfg.h`를 대체.
  - override 파일 내용: `#include "cfe_time_interface_cfg.h"`(기본값 로드) 후 `#undef CFE_MISSION_TIME_EPOCH_YEAR` / `#define ... 2026`. 단일 심볼만 재정의(다른 시간 설정 전부 라이브러리 기본 유지).
- **검증(WSL)**:
  - 생성 wrapper `build/inc/cfe_time_mission_cfg.h`가 라이브러리 default 대신 `cfs-telemetry-app/mission_defs/config/cfe_time_mission_cfg.h`를 include하도록 바뀜 확인.
  - 전처리 실측: `CFE_MISSION_TIME_EPOCH_YEAR` → **2026** (라이브러리는 1980 원본 그대로). 전체 빌드 에러 없음.
- **적용(Pi)**:
  - 라이브러리 `cfe_time_interface_cfg.h:187` 2026→**1980 원복**(단일 소스는 override로 일원화).
  - override 파일 Pi `mission_defs/config/`에 복사, `make prep`로 wrapper 재생성(override 가리킴 확인), `make && make install` 재빌드(바이너리 21:55 갱신), `sudo systemctl restart cfs.service`(PID 6433, active).
- **효과**: 앱 git에 남아 재현 가능, WSL/Pi 어디서 빌드해도 자동 적용, cFS 재-clone에도 안전.
- **주의(잔여)**: cFS 단위테스트 `time_UT.c:739`가 `EPOCH_YEAR==1980`을 가정 → 유닛테스트 활성 빌드 시 실패 가능(비행 빌드는 무관). 라이브러리 원본은 1980이라 유닛테스트는 default 경로에선 통과.
- **커밋 대상**: `mission_defs/config/cfe_time_mission_cfg.h` (신규). WSL 저장소에 커밋 예정.
