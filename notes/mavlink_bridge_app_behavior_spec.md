# mavlink_bridge_app 동작 명세 — FC 경로 업로드

## 1. 목적

이 문서는 `mavlink_bridge_app`의 FC 경로 업로드 동작을 정의한다.
코드와 정합된 명세서로, 리뷰, 통합, 테스트 수행에 활용한다.

코드와 이 문서가 서로 다르면, 코드를 조사의 원본으로 취급해야 한다.

## 2. 범위

이 명세는 다음을 다룬다.

- `ROUTE_UPDATE_MID` 수신 후 FC로 웨이포인트를 전달하는 FC MISSION 업로드 동작
- MAVLink MISSION 업로드 핸드셰이크 프로토콜
- MISSION_ITEM_INT 필드 매핑 (좌표계 및 z 부호 포함)
- 타이밍, 재시도, 실패 및 성공 처리
- MISSION_ACK ACCEPTED 의미 범위
- HK 추가 필드
- FC MISSION 조회 명령 (MISSION_QUERY_CC)
- 알려진 FC 호환성 제약 (MAV_FRAME_LOCAL_NED)

이 명세는 다음을 다루지 않는다.

- MAVLink 수신 파싱 (ATTITUDE, GPS, EKF 등)
- LoRa 텔레메트리 송신 (→ `lora_fc_downlink_app` 담당, 아래 §2.1 참조)
- `cfs_core_app` 경로 캐시 갱신 동작 (→ `cfs_core_app_behavior_spec.md` §7.3, §8.3, §16)

### 2.1 LoRa 텔레메트리 송신 이관

`mavlink_bridge_app`은 과거에 `ServiceLoRa()` 함수를 통해 LoRa serial write를 직접 수행했으나, 이 기능은 `lora_tdm_app`으로 이관되었다.

현재 `mavlink_bridge_app`의 LoRa 관련 책임:
- LoRa serial 접근 없음
- `LoRaFd`, `LoRaTxCount` 필드 없음
- FC 상태(ATTITUDE, EKF_LOCAL 등)를 SB publish하면 `lora_tdm_app`이 구독하여 TDM downlink로 전송

> 코드 확인(2026-06-16): `fsw/src/*.c` 전체에 `LoRaFd`/`LoRaTxCount`/`ServiceLoRa` 등 LoRa 직접 접근 잔재 없음 — 위 서술과 일치. 단, `config/default_mavlink_bridge_app_platform_cfg.h`에 미사용 잔존 정의 `MAVLINK_BRIDGE_APP_LORA_SERIAL_PATH`/`MAVLINK_BRIDGE_APP_LORA_BAUDRATE`가 남아 있음(런타임 동작에는 영향 없는 dead config — 향후 정리 대상).

## 3. 참조

- Source: `mavlink_bridge_app/fsw/src/mavlink_bridge_app_utils.c` — `StartMissionUpload`, `SendMissionCount`, `SendMissionItemInt`, `HandleMissionAck`
- Source: `mavlink_bridge_app/fsw/src/mavlink_bridge_app.c` — RouteUpdate dispatch
- Source: `mavlink_bridge_app/fsw/src/mavlink_bridge_app.h` — `MissionPendingX/Y/Z`, `MissionUploadState`
- 전체 MID 계약: `notes/mission_app_runtime_spec.md` §5.1.1
- 앱 간 책임 분리: `notes/cfs_core_app_behavior_spec.md` §22

## 4. 책임 분리

| 단계 | 담당 앱 | 동작 |
| --- | --- | --- |
| 경로 명령 수신 및 검증 | `uplink_app` | `ROUTE_UPDATE_MID` 게시 |
| 경로 캐시 저장 | `cfs_core_app` | MissionRoute / LandingRoute 갱신 |
| FC 웨이포인트 업로드 | `mavlink_bridge_app` | MAVLink MISSION 업로드 프로토콜 수행 |

`ROUTE_UPDATE_MID` (0x190B)는 `cfs_core_app`과 `mavlink_bridge_app` 모두가 구독한다. 이 MID를 publish하는 생산자(`uplink_app`)는 payload 검증뿐 아니라 FC 업로드 가능성까지 고려해야 한다.

## 5. 트리거

유효한 `ROUTE_UPDATE_MID` 수신 시 즉시 FC 업로드 시퀀스를 시작한다.

업로드 시작 조건:
- FC 링크가 CONNECTED 상태 (Heartbeat 수신 완료)
- FC가 ARMED 상태가 아님 (`IsArmed == 0`)

FC 링크가 연결되지 않은 상태에서 `ROUTE_UPDATE_MID`가 수신되면 업로드를 건너뛰고 EVS 오류 이벤트를 발생시킨다.

FC가 ARMED 상태이면 업로드를 수행하지 않고 `MAVLINK_BRIDGE_APP_ARMED_WARN_EID` (EID 12) EVS 경고를 발생시킨다. ARMED 여부는 FC Heartbeat의 `base_mode` bit7 (0x80)으로 판단한다.

업로드 진행 중에 새 `ROUTE_UPDATE_MID`가 수신되면 별도 cancel handshake 없이 `MissionUploadState`와 pending waypoint buffer를 새 경로로 덮어쓰고 `MISSION_CLEAR_ALL`부터 재시작한다.

## 6. MAVLink MISSION 업로드 프로토콜

MAVLink standard MISSION upload handshake를 따른다. FC 펌웨어/설정에 따라 두 가지 경로를 모두 지원한다.

### 6.0 공통 선행 단계: MISSION_CLEAR_ALL

모든 업로드 경로에서 `MISSION_COUNT` 전송 전에 반드시 `MISSION_CLEAR_ALL(45)`을 먼저 전송한다.

```
mavlink_bridge_app → FC : MISSION_CLEAR_ALL (target_system, target_component, mission_type=0)
FC → mavlink_bridge_app : MISSION_ACK       (result=ACCEPTED, 선택적)
mavlink_bridge_app → FC : MISSION_COUNT     (count=N, ...)
...
```

**이유**: 현재 테스트한 ArduPilot 환경에서 `MISSION_CLEAR_ALL` 없이 `MISSION_COUNT`만 전송했을 때 FC 응답이 확인되지 않았다 (`tools/mission_upload_diag.py`, 2026-05-28). 따라서 현재 구현은 호환성 확보를 위해 `MISSION_COUNT` 전에 반드시 `MISSION_CLEAR_ALL`을 전송한다.

`MISSION_CLEAR_ALL`에 대한 ACK는 명시적으로 대기하지 않는다. 구현은 `MISSION_CLEAR_ALL` 전송 후 `MAVLINK_BRIDGE_APP_MISSION_CLEAR_DELAY_MS` (300ms) 경과 시 `MISSION_COUNT`를 전송한다.

### 6.1 INT 경로 (권장, MAVLink2 / ArduPilot 4.x+)

```
mavlink_bridge_app → FC : MISSION_CLEAR_ALL
mavlink_bridge_app → FC : MISSION_COUNT      (count=N, mission_type=MAV_MISSION_TYPE_MISSION)
FC → mavlink_bridge_app : MISSION_REQUEST_INT (msg 51, seq=0)
mavlink_bridge_app → FC : MISSION_ITEM_INT    (msg 73, seq=0, int32 좌표)
...
FC → mavlink_bridge_app : MISSION_ACK         (result=MAV_MISSION_ACCEPTED)
```

### 6.2 Legacy 경로 (호환, MAVLink1 / 구형 펌웨어)

```
mavlink_bridge_app → FC : MISSION_CLEAR_ALL
mavlink_bridge_app → FC : MISSION_COUNT   (count=N, mission_type=MAV_MISSION_TYPE_MISSION)
FC → mavlink_bridge_app : MISSION_REQUEST  (msg 40, seq=0)
mavlink_bridge_app → FC : MISSION_ITEM     (msg 39, seq=0, float 좌표)
...
FC → mavlink_bridge_app : MISSION_ACK      (result=MAV_MISSION_ACCEPTED)
```

- `MISSION_REQUEST(40)` 수신 시 `MISSION_ITEM(39)`(float 좌표)으로 응답한다.
- INT 경로와 Legacy 경로는 동일한 `MissionUploadState` 상태머신을 공유한다.
- 두 경로 모두 timeout/재시도 동작은 §8과 동일하다.

각 단계에서 FC 응답을 기다리며, timeout 초과 시 재시도한다.

> **FC 호환성 주의**: 두 경로는 서로 다른 frame을 사용한다.
> - **INT 경로 (msg 73)**: `MAV_FRAME_LOCAL_NED` (= 1). x/y는 int32 (×10000, 0.1mm 단위), z는 float (부호 반전).
> - **Legacy 경로 (msg 39)**: `MAV_FRAME_GLOBAL_RELATIVE_ALT` (= 3). GPS 기준점(RefLatE7/RefLonE7)을 사용해 local x/y를 lat/lon으로 변환. §12.1 참조.
>
> FC가 INT 경로에서 `MISSION_ACK result = MAV_MISSION_UNSUPPORTED_FRAME`을 반환하면, INT 경로에도 global frame 변환 적용이 필요하다.

## 7. MISSION_ITEM_INT 필드 매핑

| MAVLink 필드 | 값 |
| --- | --- |
| `target_system` | FC system ID (런타임에 Heartbeat에서 획득) |
| `target_component` | FC autopilot component ID |
| `seq` | 웨이포인트 인덱스 (0-based) |
| `frame` | `MAV_FRAME_LOCAL_NED` (= 1) |
| `command` | `MAV_CMD_NAV_WAYPOINT` (= 16) |
| `current` | 0 (첫 번째 항목도 0, SET_CURRENT_ITEM으로 별도 지정) |
| `autocontinue` | 1 |
| `param1..4` | 0.0 (hold time, acceptance radius 등 미사용) |
| `x` | `(int32)(Waypoints[i].X * 10000)` — float meters → int32 (0.1mm 단위) |
| `y` | `(int32)(Waypoints[i].Y * 10000)` — 동일 |
| `z` | `-Waypoints[i].Z` (float meters). Route payload는 altitude-positive convention을 사용하며, LOCAL_NED z는 down-positive이므로 부호를 반전한다. |
| `mission_type` | `MAV_MISSION_TYPE_MISSION` (= 0) |

x/y는 `int32` (× 10000, 0.1mm 단위)이고, z는 `float` meters (부호 반전)임에 유의한다.

## 8. 타이밍 및 재시도

| 파라미터 | 값 |
| --- | --- |
| 단계별 응답 대기 timeout | 2000 ms |
| 최대 재시도 횟수 | 3 |
| 재시도 조건 | timeout. 예상치 못한 seq 응답은 즉시 재시도하지 않고 무시되며, 이후 timeout 경로에서 재시도된다. |
| 재시도 시 재시작 위치 | MISSION_COUNT부터 전체 재시작 |

> 정의 위치: `mavlink_bridge_app_utils.c:63-65` (`MAVLINK_BRIDGE_APP_MISSION_UPLOAD_TIMEOUT_MS=2000U`, `MAVLINK_BRIDGE_APP_MISSION_MAX_RETRIES=3U`, `MAVLINK_BRIDGE_APP_MISSION_CLEAR_DELAY_MS=300U`) — 코드 확인, 2026-06-16.

## 9. 실패 및 성공 처리

### 9.1 실패 처리

| 실패 조건 | 처리 |
| --- | --- |
| FC 응답 timeout | 재시도. 최대 재시도 초과 시 업로드 실패 기록 |
| `MISSION_ACK` result ≠ ACCEPTED | 즉시 실패 기록. 재시도 없음 |
| 업로드 중 새 `ROUTE_UPDATE_MID` 수신 | 별도 cancel handshake 없이 `MissionUploadState`와 pending waypoint buffer를 새 경로로 덮어쓰고 `MISSION_CLEAR_ALL`부터 재시작 |
| FC 링크 단절 | 업로드 중단. 링크 복구 후 자동 재시도 없음 (다음 경로 명령 대기) |

업로드 실패 시:
- EVS 오류 이벤트 발생 (`MAVLINK_BRIDGE_APP_MISSION_UPLOAD_ERR_EID`)
- HK에 실패 카운터 증가
- **기존 FC 미션 보존은 보장하지 않음**: 업로드 시작 시 `MISSION_CLEAR_ALL`이 이미 FC에 전송되었을 수 있으므로, timeout 또는 NAK 발생 시 FC 미션이 지워진 상태일 수 있다. 실제 FC 저장 상태는 `MISSION_QUERY_CC` 또는 GCS mission list 조회로 확인해야 한다.

### 9.2 성공 처리

업로드 성공(`MISSION_ACK` = ACCEPTED) 시:
- EVS 정보 이벤트 발생 (`MAVLINK_BRIDGE_APP_MISSION_UPLOAD_INF_EID`)
- HK에 성공 카운터 및 마지막 업로드 타임스탬프 갱신

`StartMissionUpload` 함수 진입 시에도 동일 EID로 진단 로그를 발생시킨다:
```
MAVLINK_BRIDGE_APP: StartMissionUpload called wp=<N> link=<state>
```
`link` 값: 0=DISCONNECTED, 1=CONNECTED. 이 로그는 `ROUTE_UPDATE_MID`가 실제로 dispatch 함수까지 도달했는지 확인하는 진단용이다.

### 9.3 MISSION_ACK result 파싱

`MISSION_ACK` (msg 47) payload에서 result는 `Payload[2]` (type 필드)로 읽는다. payload 길이가 3 미만이면 result = 0 (ACCEPTED)로 처리한다.

MAVLink MISSION_ACK 필드 순서: `[0]` target_system, `[1]` target_component, `[2]` type (= result), `[3]` mission_type.

### 9.4 MISSION_ACK ACCEPTED 의미 범위

`MISSION_ACK ACCEPTED`는 FC가 업로드 절차를 수락했음을 의미한다. 다음은 포함하지 않는다.

| 항목 | 포함 여부 |
| --- | --- |
| FC가 mission item 수신을 수락함 | O |
| FC 미션 저장소에 실제 반영됐는지 | 별도 조회 필요 |
| 기체가 해당 경로를 실행함 | X |
| AUTO 모드 전환, ARM, 미션 시작 수행 | X |
| 현재 미션 인덱스 변경 | X |

업로드 후 실제 저장 여부는 `MISSION_QUERY_CC` 또는 GCS mission list 조회로 별도 확인해야 한다.

## 10. FC MISSION 조회 명세 (MISSION_QUERY_CC)

cFS CMD `MAVLINK_BRIDGE_APP_CMD_MID` (0x18A0), CC=2로 트리거된다.

**목적**: FC에 저장된 현재 미션 항목을 읽어 EVS 로그로 출력한다. 업로드 결과 확인용.

**다운로드 프로토콜:**

```
mavlink_bridge_app → FC : MISSION_REQUEST_LIST
FC → mavlink_bridge_app : MISSION_COUNT (count=N)
mavlink_bridge_app → FC : MISSION_REQUEST_INT (seq=0)
FC → mavlink_bridge_app : MISSION_ITEM_INT    (seq=0)
...
FC → mavlink_bridge_app : MISSION_ITEM_INT    (seq=N-1)
mavlink_bridge_app → FC : MISSION_ACK         (MAV_MISSION_ACCEPTED)
```

**출력**: 수신된 각 waypoint를 `MAVLINK_BRIDGE_APP_MISSION_DOWNLOAD_INF_EID` EVS 이벤트로 출력한다.
```
[wp N] x=<val> y=<val> z=<val> cmd=<cmd>
```

**타이밍**: 단계별 응답 대기 timeout 3000 ms. 재시도 없음.

**지상국 트리거**:
```bash
python3 tools/query_fc_mission.py <Pi_IP> 1234
```

## 11. FC HEARTBEAT 파싱

`HEARTBEAT (msg 0)` 수신 시 다음을 처리한다.

1. `TargetSystemId`, `TargetComponentId` 획득 (이후 모든 MAVLink 메시지에 사용)
2. `LastRxTimestampMs` 갱신
3. `LinkState` → CONNECTED 전환
4. `UpdateFromHeartbeat(Payload[6], Payload[7])` 호출

### 11.1 UpdateFromHeartbeat

`HEARTBEAT` payload 필드:

| Byte | 필드 | 값 |
| --- | --- | --- |
| `[6]` | `base_mode` | MAV_MODE_FLAG bitmask |
| `[7]` | `system_status` | MAV_STATE enum |

`UpdateFromHeartbeat(BaseMode, SystemStatus)` 동작:

| 필드 | 갱신 값 |
| --- | --- |
| `FcBaseMode` | `BaseMode` 그대로 저장 |
| `FcSystemStatus` | `SystemStatus` 그대로 저장 |
| `IsArmed` | `(BaseMode & 0x80) ? 1 : 0` — bit7 = `MAV_MODE_FLAG_SAFETY_ARMED` |

`IsArmed == 1`이면 `StartMissionUpload()`가 업로드를 차단하고 `MAVLINK_BRIDGE_APP_ARMED_WARN_EID (12)` EVS 경고를 발생시킨다.

## 12. HK 추가 필드 (미션 업로드 + FC 상태)

| 필드 | 형식 | 의미 |
| --- | --- | --- |
| `MissionUploadSuccessCount` | `uint32` | 누적 업로드 성공 횟수 |
| `MissionUploadFailCount` | `uint32` | 누적 업로드 실패 횟수 |
| `LastUploadTimestampMs` | `uint32` | 마지막 성공 업로드 시각 |
| `LastUploadWaypointCount` | `uint8` | 마지막 업로드한 웨이포인트 수 |
| `LastUploadResult` | `uint8` | 0=없음, 1=성공, 2=timeout, 3=NAK |
| `FcBaseMode` | `uint8` | 마지막 수신 HEARTBEAT base_mode |
| `FcSystemStatus` | `uint8` | 마지막 수신 HEARTBEAT system_status |
| `IsArmed` | `uint8` | 0=DISARMED, 1=ARMED (base_mode bit7 기준) |

## 13. 알려진 FC 호환성 제약

### 13.0 MAVLink System ID

브리지의 MAVLink system ID는 `255`(표준 GCS ID)를 사용해야 한다.

| 항목 | 값 |
|------|-----|
| `MAVLINK_BRIDGE_APP_SYSTEM_ID` | `255` |
| `MAVLINK_BRIDGE_APP_COMPONENT_ID` | `190` |

> 정의 위치: `mavlink_bridge_app_utils.c:40-41` (로컬 `#define`, config 헤더에는 없음 — 코드 확인, 2026-06-16).

**이유**: ArduPilot은 미션 업로드 등 명령을 `SYSID_MYGCS`(기본값 255)로 등록된 시스템에서만 수락한다. sysid=200 등 비표준 ID를 사용하면 FC가 MISSION_COUNT를 무시하고 응답하지 않는다. MAVProxy 기본값(`source_system=255`)이 정상 동작하고 우리 브리지(sysid=200)가 무응답이었던 사례로 확인됨.

### 13.1 MAV_FRAME_LOCAL_NED → GLOBAL_RELATIVE_ALT 변환

ArduPilot은 미션 아이템에서 `MAV_FRAME_LOCAL_NED` (= 1)을 거부한다 (`MISSION_ACK result=2 = MAV_MISSION_UNSUPPORTED_FRAME`). `MAV_FRAME_GLOBAL_RELATIVE_ALT` (= 3)를 사용해야 한다.

#### 기준점

FC의 최신 `GLOBAL_POSITION_INT (msg 33)` lat/lon을 기준점(RefLat, RefLon)으로 사용한다.

- 기준점은 `GLOBAL_POSITION_INT` 수신마다 갱신된다.

#### GPS 가용 여부에 따른 동작

| 시나리오 | 조건 | 동작 |
|---------|------|------|
| **GPS 있음** | `RefLatE7 != 0 \|\| RefLonE7 != 0` | 실제 lat/lon 기준으로 변환 |
| **GPS 없음** | `RefLatE7 == 0 && RefLonE7 == 0` | `(0, 0)` 기준으로 변환 + 경고 로그 출력 후 업로드 진행 |

GPS 없음 시나리오는 기능 검증(미션 저장 여부 확인) 목적으로만 사용한다. FC는 절대 좌표 없이도 미션 아이템을 저장한다.

EVS 경고: `"MAVLINK_BRIDGE_APP: no GPS ref - uploading with (0,0) origin"`

#### 변환 공식

```
ref_lat_rad = RefLatE7 / 1e7 × π/180

delta_lat_deg = X_m / 6371000 × (180/π)
delta_lon_deg = Y_m / (6371000 × cos(ref_lat_rad)) × (180/π)

wp_lat = RefLatE7/1e7 + delta_lat_deg   [degrees, float]
wp_lon = RefLonE7/1e7 + delta_lon_deg   [degrees, float]
wp_alt = Z_m                             [m, positive-up]
```

- `X_m`: route 웨이포인트 X (LOCAL_NED north 방향, meters)
- `Y_m`: route 웨이포인트 Y (LOCAL_NED east 방향, meters)
- `Z_m`: route 웨이포인트 Z (altitude-positive, meters)

#### MISSION_ITEM 필드 변경

| 필드 | 기존 | 변경 |
|------|------|------|
| frame | `MAV_FRAME_LOCAL_NED` (1) | `MAV_FRAME_GLOBAL_RELATIVE_ALT` (3) |
| x | int32 (X×10000) | float (lat degrees) |
| y | int32 (Y×10000) | float (lon degrees) |
| z | float (−Z_m) | float (Z_m) |

#### 적용 범위

`MISSION_ITEM (msg 39)` 경로에만 적용. `MISSION_ITEM_INT (msg 73)` 경로는 FC가 `MISSION_REQUEST_INT (51)`를 사용할 경우 별도 처리 필요 (현재 미구현).

### 13.1.1 경로별 frame 및 좌표 인코딩 현황

| 경로 | MAVLink 메시지 | frame | x/y 인코딩 | z 인코딩 |
| --- | --- | --- | --- | --- |
| **INT 경로** | MISSION_ITEM_INT (73) | `MAV_FRAME_LOCAL_NED` (1) | float meters → int32 (× 10000, 0.1mm) | float meters, 부호 반전 (down-positive) |
| **Legacy 경로** | MISSION_ITEM (39) | `MAV_FRAME_GLOBAL_RELATIVE_ALT` (3) | GPS 기준점 기반 lat/lon 변환 (§12.1 공식) | float meters, altitude-positive |

INT 경로에서 FC가 `MISSION_ACK result = MAV_MISSION_UNSUPPORTED_FRAME`을 반환하면 INT 경로에도 global frame 변환 적용이 필요하다 (현재 미구현).

## 14. 진단 로그

### 14.1 수신 프레임 로그

수신된 모든 MAVLink 프레임 완료 시점에 `MAVLINK_BRIDGE_APP_PARSE_EID` INFORMATION 이벤트를 발생시킨다.

```
MAVLINK_BRIDGE_APP: frame msgid=<id> len=<payload_len> rx_ms=<timestamp>
```

- **목적**: FC가 어떤 msgid로 응답하는지 진단. 특히 미션 업로드 시 `MISSION_REQUEST_INT (51)` 대신 legacy `MISSION_REQUEST (40)` 응답 여부 확인.
- **출력 시점**: `HandleFrameComplete` 진입 직후, CRC 검증 전.
- **주의**: 고빈도 프레임(ATTITUDE 등)에서도 출력되므로 진단 목적 외에는 제거 권장.

### 14.2 미션 업로드 단독 진단 스크립트

**위치**: `tools/mission_upload_diag.py`

cFS 없이 Pi에서 직접 MAVLink mission upload 시퀀스를 실행하고 FC 응답을 단계별로 출력하는 진단 도구.

**목적**: cFS 브리지와 MAVProxy 간 동작 차이를 비교하여 FC가 응답하지 않는 원인 규명.

**실행**:
```bash
# cFS 종료 후
python3 tools/mission_upload_diag.py --port /dev/serial0 --baud 57600
```

**단계별 동작**:
1. FC heartbeat 수신 → `target_sysid`, `target_compid` 획득
2. `MISSION_CLEAR_ALL(45)` 전송 → `MISSION_ACK` 대기
3. `MISSION_COUNT(N)(44)` 전송 → `MISSION_REQUEST_INT(51)` 또는 `MISSION_REQUEST(40)` 대기
4. 요청된 seq에 맞는 `MISSION_ITEM_INT(73)` 또는 `MISSION_ITEM(39)` 응답
5. 최종 `MISSION_ACK` 수신 및 result 출력

**출력 형식**: 각 단계에서 TX/RX 프레임의 msgid, payload hex 전부 출력 → cFS 브리지와 직접 비교 가능.

**sysid/compid**: `255/190` (브리지와 동일)

---

## 15. 미구현 사항

- FC 현재 미션 항목 변경 (`MAV_CMD_DO_SET_MISSION_CURRENT`)
- Landing route 업로드
- 업로드 완료 후 자동 미션 시작
- INT 경로 (msg 73)에서 `MAV_FRAME_GLOBAL_RELATIVE_ALT` 변환

다음 항목은 구현 완료되었다:

- mid-flight 경로 변경 안전 검사 → FC ARMED 상태 시 업로드 차단 구현 (`UpdateFromHeartbeat`, `IsArmed` 필드, `MAVLINK_BRIDGE_APP_ARMED_WARN_EID`)
- **Legacy 경로(msg 39)** 한정 Global mission frame (`MAV_FRAME_GLOBAL_RELATIVE_ALT`) 변환 및 업로드 (`SendMissionItem`, `mavlink_bridge_app_utils.c:368`). INT 경로(msg 73, `SendMissionItemInt`)는 위 미구현 항목대로 `MAV_FRAME_LOCAL_NED` 그대로 송신한다 — 두 항목은 서로 다른 경로를 가리키므로 모순이 아니다.
