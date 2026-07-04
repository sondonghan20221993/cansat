# Raspberry Pi 재통합 및 재설정 절차

이 문서는 Raspberry Pi가 초기화된 뒤 `cFS`와 baseline telemetry app set을 다시 올릴 때
필요한 절차와, 실제로 확인된 문제 및 해결 기준을 정리한다.

현재 baseline app set은 다음을 기준으로 한다.

- `mavlink_bridge_app`
- `cfs_core_app`
- `lora_tdm_app`
- `uplink_app`

`telemetry_app`과 `img_app`은 현재 baseline app set에 포함하지 않는다.

## 목표

- 단일 Raspberry Pi 환경에서 `native_std / cpu1`로 `cFS`를 안정적으로 실행한다.
- MAVLink를 `FC -> Raspberry Pi -> mavlink_bridge_app -> lora_tdm_app` 경로로 연결한다.
- Windows 측에서 최종 수신 여부를 검증한다.

## 확인된 문제 요약

### 1. `TO_LAB` startup subscription burst

기본 sample 설정에서는 `TO_LAB` startup subscription 수가 많아 `SBNSubPipe` overflow가 발생했다.

확인된 증상:

- `Pipe Overflow, MsgId 0x80e, pipe SBNSubPipe, sender TO_LAB`

해결 기준:

- `sample_defs/tables/to_lab_sub.c`의 startup subscription 수를 크게 줄인다.
- 실제 bring-up에서는 `TO_LAB` 구독 수를 7개 수준으로 줄였을 때 안정적으로 동작했다.

### 2. sample `SBN` 설정이 단일 노드 환경에 과함

기본 sample `SBN` 구성은 self-peer와 multi-peer, remap filter를 포함한다.
단일 Raspberry Pi baseline bring-up에는 이 구성이 과하며, 초기 부팅 시 불필요한 복잡도를 만든다.

확인된 증상:

- `SBN_F_Remap` symbol load 실패
- `SBN_UDP_Ops` module/load 관련 초기화 오류
- `TO_LAB` subscription burst와 결합된 `SBNSubPipe` overflow

해결 기준:

- baseline bring-up 단계에서는 `SBN`, `sbn_udp`, `sbn_f_remap`를 startup에서 제외한다.
- 추후 SBN 경로를 다시 검증할 경우에는 self-only 구성부터 다시 시작한다.

### 3. `DS` filter table의 long event 저장 항목

기본 sample `DS` filter table에 `CFE_EVS_LONG_EVENT_MSG_MID (0x808)`가 포함되어 있었다.

해결 기준:

- `apps/ds/fsw/tables/ds_filter_tbl.c`에서 해당 항목을 비활성화한다.

### 4. OSAL 이름 길이 한계 (앱 로드/실행 실패의 최다 원인)

cFS/OSAL은 식별자 길이에 **하드 제한**이 있고, 초과해도 빌드는 성공하지만
런타임에 조용히(또는 syslog에만) 실패한다. baseline bring-up에서 발생한
다수의 "앱이 로드는 되는데 init 로그가 없음" 증상이 모두 이 한계 때문이었다.

#### 적용되는 두 한계

| 상수 | 기본값 | 적용 대상 | 검사 방식 |
| --- | --- | --- | --- |
| `OS_MAX_API_NAME` | 20 | 앱/태스크 이름(startup 4번째 필드), **entry point 심볼명(3번째 필드)** | null 종료문자 포함 ≤20 → **실사용 ≤19자** |
| `OS_MAX_FILE_NAME` | 20 | `.so` 파일명(startup 2번째 필드 = CMake `add_cfe_app` 타겟명 + `.so`) | basename ≤20 → **실사용 ≤19자** |

> 검사는 `memchr(name, '\0', 20)` 방식이라 정확히 20자면 첫 20바이트 안에 null이
> 없어 실패한다. 따라서 **모든 이름은 19자 이하**로 잡는다.

#### startup.scr 필드별 점검 (`CFE_APP, <파일>, <엔트리심볼>, <앱이름>, ...`)

| 필드 | 한계 | 초과 시 에러 |
| --- | --- | --- |
| 2번째 `.so` 파일명 | `OS_MAX_FILE_NAME` | `CFE_ES_LoadModule: Could not load file. EC = -104` (`OS_FS_ERR_NAME_TOO_LONG`) |
| 3번째 entry 심볼 | `OS_MAX_API_NAME` | `Could not find symbol:<19자로 잘린 이름>. EC = -1` |
| 4번째 앱 이름 | `OS_MAX_API_NAME` | `OS_TaskCreate ... EC = -13` (`OS_ERR_NAME_TOO_LONG`), **syslog에만** 출력 → stdout엔 안 보임 |

#### 실제 사례

- `mavlink_bridge_app`: `.so` 파일명/앱 이름 20자 초과 → `-104`.
  - 해결: CMake `add_cfe_app(mav_bridge_app)`, entry `MAV_BRIDGE_APP_Main`(19),
    앱 이름 `MAVLINK_BRIDGE_APP`(18).
- `lora_fc_downlink_app`: 세 필드 모두 초과로 단계적 실패(같은 증상 반복).
  - `LORA_FC_DOWNLINK_APP`(앱 이름, 20) → `-13` (TaskCreate 실패, syslog만)
  - `lora_fc_downlink_app.so`(파일명, 23) → `-104`
  - `LORA_FC_DOWNLINK_APP_Main`(엔트리, 25) → 19자로 잘려 symbol not found `-1`
  - 해결: 파일명/타겟 `lora_fc_dl_app`(`.so` 17), entry `LORA_FC_DL_Main`(15),
    앱 이름 `LORA_FC_DOWNLINK`(16). C 내부 함수(`..._Init` 등)는 OS_SymbolLookup을
    거치지 않으므로 길어도 무방하다 — **entry point 함수명만** 줄이면 된다.

#### 신규 앱 추가 시 체크리스트

- `.so` 파일명(= `add_cfe_app` 타겟명 + `.so`) ≤ 19자
- entry point 함수명 ≤ 19자
- startup 앱 이름 ≤ 19자
- 디렉토리명/`APPLIST` 항목은 길어도 무방(빌드 탐색용일 뿐, OSAL 거치지 않음)

### 4-1. `apps/` 복사본과 git 레포의 드리프트 (반드시 확인)

빌드는 `~/cFS_clean/apps/<app>`을 소스로 사용한다. custom app을 git 레포
`~/cfs-telemetry-app/<app>`에서 **복사**해 두면, 이후 `git pull`은 git 레포만
갱신하고 빌드가 쓰는 복사본은 옛 버전 그대로 남는다. 그 결과 소스 수정이
빌드에 전혀 반영되지 않으면서 동일 증상이 반복된다(디버깅 시간 최대 낭비 지점).

확인:

```bash
diff -q ~/cFS_clean/apps/<app>/CMakeLists.txt ~/cfs-telemetry-app/<app>/CMakeLists.txt
```

해결 기준:

- 복사본을 git 레포로 **심볼릭 링크**해 단일 소스로 만든다.

```bash
rm -rf ~/cFS_clean/apps/<app>
ln -s ~/cfs-telemetry-app/<app> ~/cFS_clean/apps/<app>
```

- 이후 `git pull` 한 번으로 빌드 소스까지 갱신된다.
- 단, `add_cfe_app` 타겟명을 바꾼 경우(파일명 변경) arch 빌드 캐시 재구성을 위해
  `cmake -DMISSION_DEFS=... cfe -B build`를 다시 실행해야 새 `.so`가 생성된다.

### 5. startup script 중복 등록

custom app 4개를 수동으로 startup script에 추가한 뒤, 자동 생성 루프가 다시 같은 앱을 추가해
duplicate app name 문제가 발생했다.

해결 기준:

- `sample_defs/generate_startup.cmake`의 자동 루프 목록에는 기본 app만 남긴다.
- custom app 4개는 수동 entry로만 등록한다.

### 6. build/install 과정에서 헷갈린 점

기본 `make prep`, `make install`은 모든 config를 대상으로 동작할 수 있어 `native_eds`가 같이 돌 수 있다.

해결 기준:

- `native_std`만 명시적으로 실행한다.
- 아래 3개 타깃만 사용한다.

```bash
make native_std.prep SIMULATION=native
make native_std.compile SIMULATION=native
make native_std.install SIMULATION=native
```

### 7. companion link에는 telemetry stream이 자동으로 안 열릴 수 있음

FC USB 연결에서는 `ATTITUDE`, `GLOBAL_POSITION_INT`, `GPS_RAW_INT`, `EKF_STATUS_REPORT`가 보였지만,
같은 FC의 UART4 companion link에서는 기본적으로 `HEARTBEAT (0)`와 `TIMESYNC (111)`만 들어오는 경우가 있었다.

확인된 증상:

- Pi 로그에서 `unsupported msgid=0`
- Pi 로그에서 `unsupported msgid=111`
- `SERIAL4_PROTOCOL`, `SERIAL4_BAUD`, `SR4_*` 설정이 맞아도 `ATTITUDE (30)` 등이 자동으로 오지 않음

원인 정리:

- UART4 물리 연결과 baud, MAVLink parser 자체는 정상이었다.
- companion link에 필요한 telemetry stream이 자동으로 열리지 않았다.
- `MAV_CMD_SET_MESSAGE_INTERVAL`을 Pi에서 직접 보내자 `ATTITUDE (30)` 수신이 즉시 시작되었다.

해결 기준:

- `mavlink_bridge_app`가 serial open 이후 companion heartbeat를 주기적으로 송신한다.
- FC heartbeat를 수신해 target system/component를 식별한 뒤,
  `MAV_CMD_SET_MESSAGE_INTERVAL`로 필요한 메시지를 자동 요청한다.
- 최소 요청 대상:
  - `ATTITUDE (30)`
  - `LOCAL_POSITION_NED (32)`
  - `GLOBAL_POSITION_INT (33)`
  - `GPS_RAW_INT (24)`
  - `EKF_STATUS_REPORT (193)`

### 8. CP2102 LoRa 포트 충돌 (구 lora_fc_downlink_app ↔ uplink_app → lora_tdm_app으로 해결)

`lora_fc_downlink_app`과 `uplink_app`이 동일한 CP2102 USB-UART 포트를 각각 직접 open했다.
Linux는 같은 tty 동시 open을 막지 않아 `EBUSY` 없이 둘 다 열리지만, 수신 바이트를
서로 빼앗아 HB/UP 수신이 모두 불안정해진다.

확인된 증상:

- 두 앱 모두 `opened LoRa serial ...CP2102...` 로그 출력
- HB/UP 프레임 간헐적 유실

해결 기준 (transport/app 분리):

- CP2102는 `lora_fc_downlink_app`이 단독 소유(downlink TX + TDM RX 윈도우).
- `uplink_app`은 serial 직접 open 제거 → SB 구독으로 전환.
- `lora_fc_downlink_app`이 RX에서 읽은 "UP,..." 원문을 `UPLINK_RAW_MID`(0x1909)로 publish,
  `uplink_app`이 이를 구독해 `ParseLoRaFrame()`로 파싱(파싱·검증은 uplink 소유).
- 반이중 제약: RX 윈도우는 downlink TX 후 300ms만 열림(지상국은 슬롯 내 응답 필요).
- **지상국측 정합**: 지상 LoRa 브리지(openMCT `fc_serial_ws_server.py`)는 UP 프레임을 즉시
  쏘지 말고 큐에 적재 후, downlink 라인 수신 직후(= Pi RX 윈도우 열림) 그 슬롯에 송신해야 한다.
  SH가 FC 없이도 ~1Hz로 downlink되므로 슬롯은 항상 열린다. (상세: openMCT `openmct_bridge_notes.md`)

### 9. CommandPipe starvation (구 lora_fc_downlink_app 기록; lora_tdm_app 동일 구조)

`ServiceLoRa()`가 SB 메시지 핸들러(`ProcessInputMessage`) 안에서 TX 후 **300ms 블로킹
RX 윈도우**를 도는 동안 SB 수신 루프가 멈춰 FC 스트림이 CommandPipe에 누적된다.

확인된 위험:

- FC 스트림 ~45 msg/s × 300ms ≈ 14개 누적 → CommandPipe depth 10이면 오버플로(FC 드롭).
- `lora_tdm_app` 시절 pipe depth starvation의 구조적 이전형.

해결 기준:

- `DEFAULT_LORA_FC_DOWNLINK_APP_PLATFORM_PIPE_DEPTH`를 **32**로 상향(cFS 최대 50 미만).
- 근본 해결(블로킹 LoRa I/O를 SB 핸들러에서 분리)은 후속 과제로 남긴다.
- 참고: cFS 파이프 depth 상한은 50. 이를 초과하면(과거 lora_tdm_app의 200) 즉시 종료된다.

### 10. LoRa RX 윈도우 즉시 종료 버그

LoRa 포트가 `VMIN=0/VTIME=0` 설정이라 데이터가 없으면 `read()`가 즉시 0을 반환한다.
RX 윈도우 코드가 빈 read 시 즉시 `break`하면 윈도우가 열리자마자(UP 프레임이 RF로
도착하기 전) 닫혀 uplink를 영영 수신하지 못한다.

확인된 증상:

- 지상국 4발 송신 확인인데 Pi `uplink_app` 로그에 아무 수신 기록 없음 (비대칭: downlink는 정상).

해결 기준 (커밋 `cac209f`):

- 빈 read 시 즉시 `break` 대신 `usleep(2ms)` 후 deadline까지 폴링 유지.
- `lora_tdm_app_utils.c` `ServiceLoRa()` RX 윈도우.

## Raspberry Pi 재설정 절차

### 1. 네트워크와 SSH 확인

Raspberry Pi 초기화 직후 가장 먼저 다음을 확인한다.

```bash
ip addr show wlan0
ip route
systemctl is-active ssh
hostname -I
```

기대 결과:

- `wlan0`에 IP가 할당되어 있다.
- default route가 존재한다.
- `ssh` 서비스가 `active` 상태다.

#### 1.1 Pi 접속 정보 (실측, 2026-06-29)

| 항목 | 값 |
| --- | --- |
| IP | `<PI_IP>` (TTL=64, ping/22 OK) |
| hostname | `sdh2983` |
| user | `sdh2983` |
| SSH 포트 | 22 |
| 인증 | **비밀번호** (현재 외부 머신 공개키 미등록 → key 인증 불가) |

```bash
ssh sdh2983@<PI_IP>          # 비밀번호 입력 필요
```

#### 1.2 무인증(키) 접속 설정 — SSH 누락분

key 기반 무인증 접속을 하려면 **접속하는 쪽(WSL/지상 PC)** 에서 공개키를 Pi에 등록해야 한다.
(Pi 안에서 `ssh-copy-id`를 실행하면 `No identities found`로 실패 — 방향이 반대다.)

```bash
# 지상/WSL 머신에서 실행 (Pi 안 X)
ssh-copy-id -i ~/.ssh/id_ed25519.pub sdh2983@<PI_IP>
# 키쌍이 없으면 먼저: ssh-keygen -t ed25519
```

등록 확인:

```bash
ssh -o BatchMode=yes sdh2983@<PI_IP> 'echo OK'   # OK면 무인증 성공
```

> **누락 기록**: 본 문서 §1은 `ip/hostname/ssh active` 점검만 있고 **실제 Pi IP·user·키 등록 절차가 없어**
> 외부에서 자동(무인증) SSH가 불가했다. 위 §1.1/§1.2가 그 누락분이다.

> **상태(2026-06-29)**: WSL 공개키(`~/.ssh/id_ed25519.pub`)는 **아직 Pi에 미등록**
> (`Permission denied (publickey,password)` 확인). 위 `ssh-copy-id`를 **Pi가 켜진 상태에서**
> WSL 쪽에서 실행해야 한다. Pi가 꺼져 있으면(`No route to host`) 등록 자체가 불가.

#### 1.3 원격 종료/재가동

- 종료(원격): SSH + `sudo`. 비밀번호 인증 환경에서는 tty 필요.

```bash
ssh -t sdh2983@<PI_IP> 'sudo poweroff'        # SSH 비번 + sudo 비번
```

- 무인증 자동 종료까지 원하면 키 등록(§1.2) + poweroff sudo 비번 면제:

```bash
# Pi에서 1회
echo "sdh2983 ALL=(ALL) NOPASSWD: /sbin/poweroff" | sudo tee /etc/sudoers.d/poweroff
```

- 재가동은 **물리 전원 재인가** 필요(원격 기동 불가). 이후 cFS 실행은 §12 / PC검증 §1.
- 종료 후 `ping`/포트22가 `No route to host`면 정상 종료된 것.

### 2. `cFS` 작업공간 준비

작업 경로는 `~/Desktop/cFS_clean`를 기준으로 한다.

```bash
cd ~/Desktop
git clone --recurse-submodules https://github.com/nasa/cFS.git cFS_clean
cd ~/Desktop/cFS_clean
git submodule update --init --recursive
```

### 3. baseline app 복사

다음 app 디렉터리를 `cFS` 작업공간에 복사한다.

```text
apps/mavlink_bridge_app
apps/cfs_core_app
apps/uplink_app
apps/lora_tdm_app
```

### 4. mission app 등록

다음 파일을 갱신한다.

- `sample_defs/targets.cmake`
- `sample_defs/generate_startup.cmake`

`cpu1` app list에 baseline app set을 추가한다.

### 5. `mavlink_bridge_app` target 이름 정리

다음 파일을 갱신한다.

- `apps/mavlink_bridge_app/CMakeLists.txt`

`add_cfe_app()` target 이름과 include target 이름을 `mav_bridge_app`로 통일한다.

### 6. startup script 생성 규칙 정리

다음 파일을 갱신한다.

- `sample_defs/generate_startup.cmake`

기준:

- `mav_bridge_app`, `cfs_core_app`, `uplink_app`, `lora_tdm_app`는 수동 entry로 추가한다.
- 자동 루프 목록에는 `lc`, `cf`, `ds`, `fm`, `hk`, `hs`, `mm`, `sc`, `md`, `cs`만 둔다.
- `sbn` 계열은 baseline bring-up에서는 제외한다.

### 8. `TO_LAB` subscription table 축소

다음 파일을 갱신한다.

- `sample_defs/tables/to_lab_sub.c`

baseline bring-up 기준으로 아래 MID만 남긴다.

- `TO_LAB_HK_TLM_MID`
- `TO_LAB_DATA_TYPES_MID`
- `CI_LAB_HK_TLM_MID`
- `CFE_ES_HK_TLM_MID`
- `CFE_EVS_HK_TLM_MID`
- `CFE_SB_HK_TLM_MID`
- `CFE_TBL_HK_TLM_MID`

### 9. `DS` long event filter 비활성화

다음 파일을 갱신한다.

- `apps/ds/fsw/tables/ds_filter_tbl.c`

`CFE_EVS_LONG_EVENT_MSG_MID` 항목을 reserved/unused로 바꾼다.

### 10. baseline bring-up에서는 `SBN` 제외

다음 파일을 갱신한다.

- `sample_defs/targets.cmake`

기준:

- `MISSION_GLOBAL_APPLIST`에 `sbn`, `sbn_udp`, `sbn_f_remap`를 추가하지 않는다.

### 11. 빌드와 설치

다음 명령만 사용한다.

```bash
make native_std.prep SIMULATION=native
make native_std.compile SIMULATION=native
make native_std.install SIMULATION=native
```

설치 결과는 기본적으로 `build-native_std/exe/cpu1` 아래에 생성된다.

### 12. 런타임 실행

실행은 반드시 `cpu1` 디렉터리 안에서 **`sudo`로** 한다.

```bash
cd ~/Desktop/cFS_clean/build-native_std/exe/cpu1
sudo ./core-cpu1
```

> 비-root 실행 시 `OS_API_Init() failure`로 즉시 종료된다. 이전 sudo 실행이
> `/dev/shm/osal:RAM`을 root 소유로 남기기 때문이며, 재부팅 없이도 발생한다.
> **항상 sudo로 고정한다.**

기대 결과:

- `cFS`가 `OPERATIONAL` 상태에 도달한다.
- `TO_LAB`가 7 messages 수준으로 subscription을 완료한다.
- `MAV_BRIDGE_APP`가 초기화되고 `/dev/serial0`를 `57600`으로 open한다.
- `MAV_BRIDGE_APP`가 companion heartbeat와 stream request를 자동 수행한다.
- `SBNSubPipe overflow`가 발생하지 않는다.

## 현재 baseline bring-up 정상 판정 기준

다음 조건을 모두 만족하면 baseline bring-up 성공으로 본다.

- `native_std / cpu1`가 `OPERATIONAL` 상태에 도달한다.
- `CFS_CORE_APP`, `UPLINK_APP`, `LORA_TDM_APP`(2026-06-16부터 `LORA_FC_DOWNLINK_APP` 대체), `MAV_BRIDGE_APP` 초기화 로그가 출력된다.
- `MAV_BRIDGE_APP: opened serial path /dev/serial0 at 57600 baud` 로그가 출력된다.
- `MAVLINK_BRIDGE_APP: requested telemetry streams` 또는 `COMMAND_ACK cmd=511 result=0` 로그가 출력된다.
- `Pipe Overflow, MsgId 0x80e, pipe SBNSubPipe, sender TO_LAB`가 재발하지 않는다.
- `CFS_CORE_APP: health`가 부팅 30초 후 `FAILED`(fault=1=BRIDGE_TIMEOUT)로 고착되지 않는다 —
  `mission_defs/tables/cpu1_sch_lab_table.c`(2026-06-17 추가)가 `mavlink_bridge_app`의
  `SEND_HK`를 스케줄링해야 `cfs_core_app`이 `BRIDGE_HK`를 받아 bridge 생존을 확인할 수 있다.
  이 override 없으면(기본 `sch_lab_table.c`는 빈 placeholder) bridge가 정상이어도 영원히
  타임아웃으로 오판한다.

## FC 연결 후 검증 절차

FC 연결 전에는 MAVLink payload decode 로그가 없는 것이 정상이다.

FC 연결 후에는 다음을 검증한다.

- `ATTITUDE (30)`
- `LOCAL_POSITION_NED (32)`
- `GLOBAL_POSITION_INT (33)`
- `GPS_RAW_INT (24)`
- `EKF_STATUS_REPORT (193)`

bridge log 확인 예:

```bash
cd ~/Desktop/cFS_clean/build-native_std/exe/cpu1
./core-cpu1 2>&1 | tee /tmp/core-cpu1.log
grep -n "unsupported msgid\|frame msgid\|ATTITUDE decoded\|LOCAL_POSITION_NED decoded\|GLOBAL_POSITION_INT mapped\|GPS_RAW_INT decoded\|EKF_STATUS_REPORT decoded" /tmp/core-cpu1.log
```

raw UART 확인 예:

```bash
python3 ~/Desktop/cfs-telemetry-app/bridge/mavlink_uart_bridge.py
```

판정 기준:

- 목표 msgid가 보이면 `mavlink_bridge_app -> cFS publish -> downlink` 검증 단계로 진행한다.
- `HEARTBEAT (0)`와 `TIMESYNC (111)`만 보이면 먼저 companion 쪽 `SET_MESSAGE_INTERVAL` 요청이 수행됐는지 확인한다.
- companion request가 정상인데도 계속 `0/111`만 보이면 FC 포트 매핑과 FC 상태를 다시 확인한다.

## PC 최종 수신 검증 절차

이 절차의 목적은 Raspberry Pi가 받은 FC 텔레메트리가 PC까지 실제로 전달되는지 확인하는 것이다.

### 1. Pi 로그 저장 실행

Pi에서 `cpu1` 로그를 파일로 저장하면서 실행한다.

```bash
cd ~/Desktop/cFS_clean/build-native_std/exe/cpu1
sudo ./core-cpu1 2>&1 | tee /tmp/core-cpu1.log
```

기대 결과:

- `cFS`가 `OPERATIONAL`까지 진입한다.
- `MAV_BRIDGE_APP` serial open 로그가 보인다.
- stream request 또는 `COMMAND_ACK cmd=511 result=0` 로그가 보인다.

### 2. Pi 입력 메시지 존재 확인

별도 터미널에서 bridge 입력 메시지가 실제로 들어오는지 확인한다.

```bash
grep -n "ATTITUDE decoded\|LOCAL_POSITION_NED decoded\|GLOBAL_POSITION_INT mapped\|GPS_RAW_INT decoded\|EKF_STATUS_REPORT decoded" /tmp/core-cpu1.log
```

기대 결과:

- 최소 `ATTITUDE`, `GPS_RAW_INT`, `GLOBAL_POSITION_INT`, `EKF_STATUS_REPORT` 중 하나 이상이 반복 확인된다.

주의:

- `LOCAL_POSITION_NED`는 현재 FC 상태에 따라 없을 수 있다.

### 3. PC 수신 로그 저장

PC에서는 최종 수신기 로그를 파일로 저장한다.

필수 조건:

- 로그에 수신 시각 또는 샘플 순서를 식별할 수 있어야 한다.
- 최소 30초 이상 연속 구간을 저장해야 한다.

기대 결과:

- 동일 시험 시간대에 PC에서 반복 수신 흔적이 남는다.

### 4. 정지 상태 연속 수신 확인

기체를 정지 상태로 두고 최소 30초 동안 다음을 확인한다.

- Pi 로그에서 목표 메시지 수신이 계속된다.
- PC 로그에서도 수신 공백 없이 프레임이 계속 도착한다.

실패 판정:

- Pi 입력은 있는데 PC 수신이 없으면 downlink 이후 구간 문제다.
- Pi 입력 자체가 없으면 FC 송신 조건 또는 Pi bridge 입력 단계 문제다.

### 5. 값 변화 반영 확인

가능하면 기체 자세를 소폭 바꾸거나 위치가 변하는 상황을 만들어 연속 샘플 변화를 본다.

기대 결과:

- `ATTITUDE` 관련 값 또는 위치 관련 값이 Pi와 PC 양쪽 로그에서 함께 변한다.

실패 판정:

- PC에서 수신은 되지만 값이 계속 고정되면 payload 매핑 또는 갱신 경로를 점검한다.

### 6. 최종 성공 기준

다음 조건을 모두 만족하면 PC 최종 수신 검증 성공으로 본다.

- Pi에서 목표 텔레메트리 입력이 반복 확인된다.
- 같은 시간대에 PC에서도 반복 수신이 확인된다.
- 최소 30초 동안 수신 공백이 없다.
- 자세 또는 위치 변화 시 PC 측 값도 함께 변한다.

## Uplink 통합 검증 절차

지상국(OpenMCT) → LoRa → Pi `uplink_app`까지 명령 도달을 검증하는 절차.

### 경로

```
OpenMCT CLI
  → fc_serial_ws_server.py (슬롯정렬 + 자동 재전송)
    → LoRa RF
      → Pi CP2102 (lora_tdm_app 단독 소유)
        → TDM RX 윈도우 → UPLINK_RAW_MID(0x1909) SB publish
          → uplink_app → ProcessUplink → 대상앱 라우팅
```

반이중 TDM: Pi RX 윈도우는 downlink TX 직후 300ms만 열림.

### 확인된 동작 (2026-06-15, b3d93dd)

1. **재전송 슬롯 정렬 (완료)**: 단발 전송은 300ms RX 윈도우 타이밍 지터로 종종 미도달.
   `fc_serial_ws_server.py`가 동일 프레임을 연속 4개 downlink 슬롯에 자동 재전송(`_UPLINK_RETX=4`).
   `uplink_app`의 `IsSequenceAccepted`가 중복 seq를 거부 → 1발만 적용, 나머지 replay 무해.

2. **RX 윈도우 폴링 (완료, cac209f)**: §10의 즉시 종료 버그 수정으로 UP 프레임 수신 가능.

3. **GPS 분리 런타임 검증 (완료)**: health 전이가 `fault=1(BRIDGE_TIMEOUT)` 단일 원인만 발생.
   `fault=5(GPS_STALE)` 전이 없음 → GPS가 health 게이트에서 완전 분리됨 실증.

4. **uplink_app health-block 정책 확인**: FAILED(3)는 전 명령 차단, DEGRADED/RECOVERY도
   CONFIG 차단. CONFIG는 **NOMINAL(0)에서만** 허용 → FC 링크 복구 후 NOMINAL 도달 필요.

### 잔여 이슈

- FC UART 링크 노이즈: `crc fail msgid=24/30`, stream request sysid 흔들림(1/31/58/90/245)
  → bridge 간헐 stale → health NOMINAL 도달 불가 원인. 별도 해결 필요.
- CONFIG 명령 최종 검증: NOMINAL 도달 후 CONFIG 1회 → `config activated` EVS 확인.
