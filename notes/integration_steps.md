# Raspberry Pi 재통합 및 재설정 절차

이 문서는 Raspberry Pi가 초기화된 뒤 `cFS`와 baseline telemetry app set을 다시 올릴 때
필요한 절차와, 실제로 확인된 문제 및 해결 기준을 정리한다.

현재 baseline app set은 다음을 기준으로 한다.

- `mavlink_bridge_app`
- `cfs_core_app`
- `downlink_app` (`lora_fc_downlink_app` 기반)
- `uplink_app`

`telemetry_app`과 `img_app`은 현재 baseline app set에 포함하지 않는다.

## 목표

- 단일 Raspberry Pi 환경에서 `native_std / cpu1`로 `cFS`를 안정적으로 실행한다.
- MAVLink를 `FC -> Raspberry Pi -> mavlink_bridge_app -> lora_fc_downlink_app` 경로로 연결한다.
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

### 4. `mavlink_bridge_app` 동적 로드 실패

`mavlink_bridge_app.so`는 존재했지만, 긴 파일명과 앱 이름으로 인해 OSAL loader가
`-104 (OS_FS_ERR_NAME_TOO_LONG)`를 반환했다.

해결 기준:

- CMake target과 startup entry를 짧은 이름 `mav_bridge_app`로 통일한다.
- entry point는 `MAV_BRIDGE_APP_Main`, 앱 이름은 `MAV_BRIDGE_APP`를 사용한다.

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
apps/lora_fc_downlink_app
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

- `mav_bridge_app`, `cfs_core_app`, `uplink_app`, `lora_fc_downlink_app`는 수동 entry로 추가한다.
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

실행은 반드시 `cpu1` 디렉터리 안에서 한다.

```bash
cd ~/Desktop/cFS_clean/build-native_std/exe/cpu1
./core-cpu1
```

기대 결과:

- `cFS`가 `OPERATIONAL` 상태에 도달한다.
- `TO_LAB`가 7 messages 수준으로 subscription을 완료한다.
- `MAV_BRIDGE_APP`가 초기화되고 `/dev/serial0`를 `57600`으로 open한다.
- `MAV_BRIDGE_APP`가 companion heartbeat와 stream request를 자동 수행한다.
- `SBNSubPipe overflow`가 발생하지 않는다.

## 현재 baseline bring-up 정상 판정 기준

다음 조건을 모두 만족하면 baseline bring-up 성공으로 본다.

- `native_std / cpu1`가 `OPERATIONAL` 상태에 도달한다.
- `CFS_CORE_APP`, `UPLINK_APP`, `LORA_FC_DOWNLINK_APP`, `MAV_BRIDGE_APP` 초기화 로그가 출력된다.
- `MAV_BRIDGE_APP: opened serial path /dev/serial0 at 57600 baud` 로그가 출력된다.
- `MAVLINK_BRIDGE_APP: requested telemetry streams` 또는 `COMMAND_ACK cmd=511 result=0` 로그가 출력된다.
- `Pipe Overflow, MsgId 0x80e, pipe SBNSubPipe, sender TO_LAB`가 재발하지 않는다.

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
