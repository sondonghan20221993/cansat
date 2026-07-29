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
- FC SYSTEM_TIME 수신 및 시각 동기 설계 (§16)

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

> 코드 확인(2026-06-16): `fsw/src/*.c` 전체에 `LoRaFd`/`LoRaTxCount`/`ServiceLoRa` 등 LoRa 직접 접근 잔재 없음 — 위 서술과 일치. 미사용 잔존 정의 `MAVLINK_BRIDGE_APP_LORA_SERIAL_PATH`/`MAVLINK_BRIDGE_APP_LORA_BAUDRATE`는 `config/default_mavlink_bridge_app_platform_cfg.h`에서 제거함 (2026-06-16).

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
| 경로 캐시 저장 | `cfs_core_app` | MissionRoute 갱신 |
| FC 웨이포인트 업로드 | `mavlink_bridge_app` | MAVLink MISSION 업로드 프로토콜 수행 |

`ROUTE_UPDATE_MID` (0x190B)는 `cfs_core_app`과 `mavlink_bridge_app` 모두가 구독한다. 이 MID를 publish하는 생산자(`uplink_app`)는 payload 검증뿐 아니라 FC 업로드 가능성까지 고려해야 한다.

### 4.1 mavlink_bridge_app SB 구독 목록

`mavlink_bridge_app.c` 기준 실제 구독 MID (코드 권위):

| MID | 값 | 처리 |
| --- | --- | --- |
| `MAVLINK_BRIDGE_APP_CMD_MID` | `0x18A0` | Ground command (NOOP/RESET_COUNTERS/MISSION_QUERY_CC) |
| `MAVLINK_BRIDGE_APP_SEND_HK_MID` | `0x18A1` | HK publish 트리거 |
| `ROUTE_UPDATE_MID` | `0x190B` | FC 웨이포인트 업로드 시작 |
| `CONFIG_CMD_MID` | `0x190E` | 런타임 config 명령 수신 (`ProcessConfigCommand`) |
| `EXEC_RESULT_MID` | `0x1912` | (publish) CONFIG 처리 결과를 uplink_app에 회신 — `shared_msgs/exec_result_msg.h`, BL-08(2026-07-22) |

> **1-4 audit 해소**: `CONFIG_CMD_MID 0x190E` 구독은 `mavlink_bridge_app.c:142`에서 확인됨. 본 spec의 구 §2/§4에서 누락되어 있었으나 mission_app_runtime_spec.md §5.1.1의 Subscribe 목록에는 이미 포함되어 있음 (2026-06-17 추가).
>
> **EXEC_RESULT 회신 (2026-07-22, BL-08)**: `ProcessConfigCommand`의 모든 종료
> 지점(성공/BAD_VERSION/BAD_LENGTH/BAD_CHECKSUM/BAD_PARAM/BAD_VALUE)에서
> `EXEC_RESULT_MID`로 `(SourceSequence, GenericResult, DetailCode)`를 회신한다.
> **scope 불일치(다른 앱 대상)는 여전히 조용히 무시, EXEC_RESULT도 발행 안 함**
> — 실제 대상 앱의 정상 응답과 경합하지 않도록.

## 5. 트리거

유효한 `ROUTE_UPDATE_MID` 수신 시 경로 연산 타입에 따라 Pending buffer를 결정한 뒤 즉시 FC 업로드 시퀀스를 시작한다.

업로드 시작 조건:
- FC 링크가 CONNECTED 상태 (Heartbeat 수신 완료)

FC 링크가 연결되지 않은 상태에서 `ROUTE_UPDATE_MID`가 수신되면 업로드를 건너뛰고 EVS 오류 이벤트를 발생시킨다.

**⚠️ ARMED 차단 정책 전면 폐지(BL-56, 2026-07-25, 2026-07-27 문서 정정)**: 과거엔 "FC가 ARMED 상태(`IsArmed == 1`)이면 업로드를 차단하고 `MAVLINK_BRIDGE_APP_ARMED_WARN_EID`(EID 12) EVS 경고를 발생"시켰으나, `current=1` 재개-인덱스 메커니즘으로 인덱스 리셋 문제가 해결되며 이 제약 자체가 전면 폐지됨(`mavlink_bridge_app_utils.c:507-510` 주석 참조) — REPLACE 포함 4종 연산 전부 ARMED 상태에서도 허용된다. `MAVLINK_BRIDGE_APP_ARMED_WARN_EID`는 정의만 남은 죽은 코드. REPLACE의 "실수 파급 범위가 크다"는 위험은 기체측 가드 대신 **GUI 재확인 다이얼로그**(전송 전 항상 표시)로 완화하는 쪽으로 책임이 이동했다. ARMED 여부 자체는 여전히 FC Heartbeat의 `base_mode` bit7(0x80)로 판단 가능하나, 업로드 허용/차단 판단엔 더 이상 쓰이지 않는다.

업로드 진행 중에 새 `ROUTE_UPDATE_MID`가 수신되면 별도 cancel handshake 없이 `MissionUploadState`와 pending waypoint buffer를 새 경로로 덮어쓰고 `MISSION_CLEAR_ALL`부터 재시작한다.

### 5.1 경로 연산 타입 (RouteOpType)

`ROUTE_UPDATE_MID` payload의 `RouteType` 필드(= `UPLINK_APP_RouteOpType_t`)에 따라 Pending buffer 구성 방식이 결정된다.

**⚠️ BL-56(2026-07-25)에서 3종 → 4종으로 재정의됨(2026-07-27 문서 정정)**: 과거엔 REPLACE/APPEND(count)/DELETE(count, 마지막 N개 삭제) 3종이었으나, 실제 코드(`mavlink_bridge_app_utils.c:6-9`)는 아래처럼 **REPLACE/ADD/DELETE(index)/MODIFY(신규)** 4종이며 DELETE 의미론도 완전히 바뀌었다.

| 값 | 이름 | Pending buffer 구성 방식 |
| --- | --- | --- |
| `1` | `REPLACE` | `Msg->Waypoints[0..N-1]`을 그대로 복사. Active cache 무시. |
| `2` | `ADD` | Active cache를 먼저 복사한 뒤 `Msg->Waypoints`를 끝에 이어 붙임(중간 삽입 불가, 필요 시 MODIFY/DELETE 조합). 합계가 `MAX_WAYPOINTS`를 초과하면 MAX에서 절단하고 `MAVLINK_BRIDGE_APP_MISSION_UPLOAD_INF_EID` EVS 경고 발생. |
| `3` | `DELETE` | **(옛 "마지막 count개 삭제"가 아니라) index 기반 단일 삭제**로 변경됨. `Msg->WaypointCount` 필드를 index로 재해석(`IndexOrCount = Msg->WaypointCount`, `mavlink_bridge_app_utils.c:493`) — 해당 index 하나만 제거하고 뒷 인덱스를 한 칸씩 당김. `index >= ActiveCount`면 range 오류로 거부. `index == ActiveResumeIndex`(현재 진행 중인 목표점)면 REJECT_ROUTE로 거부, 그 외엔 뒷 인덱스가 당겨지는 만큼 `ActiveResumeIndex`도 보정된다. |
| `4` | `MODIFY` | (신규) 동일하게 index 기반 — 해당 슬롯 하나(좌표뿐 아니라 `CmdType`/`Param1~4` 전체)를 통째로 교체. `index == ActiveResumeIndex`(진행 중인 목표점)도 허용됨(장애물 회피 등 의도된 긴급 수정, 사용자 책임). |

Pending buffer가 결정되면 네 연산 모두 동일한 `MISSION_CLEAR_ALL → MISSION_COUNT → ...` 핸드셰이크를 수행한다.

Active cache(`ActiveWaypointX/Y/Z`, `ActiveWaypointCount`)는 `MISSION_ACK ACCEPTED` 수신 시에만 Pending buffer 내용으로 갱신된다(§9.2 참조). 연산 실패 시 Active cache는 변경되지 않는다.

## 6. MAVLink MISSION 업로드 프로토콜

MAVLink standard MISSION upload handshake를 따른다. FC 펌웨어/설정에 따라 두 가지 경로를 모두 지원한다.

> **FC 펌웨어 불일치 발견 (2026-07-14)**: 본 섹션의 `MISSION_CLEAR_ALL` 필수화(§6.0),
> `sysid=255` 요구(§13, `SYSID_MYGCS`), `MAV_FRAME_LOCAL_NED` 거부(§13) 등은 모두
> **ArduPilot 실측(2026-05-28 등)** 기반으로 기록된 내용이다. 그러나 현재 실제 연결된
> FC는 **PX4**로 확인됨(2026-07-14) — 위 ArduPilot 특정 동작들이 PX4에서도 동일하게
> 적용되는지 **미검증**. `mavlink_bridge_app_utils.c`의 FC 식별 로직 자체는 이미
> `autopilot` 필드로 ArduPilot(3)/PX4(12) 둘 다 인식하도록 설계돼 있음
> (`spec_code_audit.md` 부록 참조) — 즉 코드가 PX4를 지원하지 않는 게 아니라, 이
> 문서의 mission-upload 세부 거동 기록이 ArduPilot 전용 실측이라는 뜻. PX4 연결 시
> 재검증 필요(특히 §6.0 MISSION_CLEAR_ALL 필요 여부, §13 sysid/frame 요구사항).

### 6.0 공통 선행 단계: MISSION_CLEAR_ALL

모든 업로드 경로에서 `MISSION_COUNT` 전송 전에 반드시 `MISSION_CLEAR_ALL(45)`을 먼저 전송한다.

```
mavlink_bridge_app → FC : MISSION_CLEAR_ALL (target_system, target_component, mission_type=0)
FC → mavlink_bridge_app : MISSION_ACK       (result=ACCEPTED, 선택적)
mavlink_bridge_app → FC : MISSION_COUNT     (count=N, ...)
...
```

**이유**: 현재 테스트한 ArduPilot 환경에서 `MISSION_CLEAR_ALL` 없이 `MISSION_COUNT`만 전송했을 때 FC 응답이 확인되지 않았다 (`legacy/tools/mission_upload_diag.py`, 2026-05-28). 따라서 현재 구현은 호환성 확보를 위해 `MISSION_COUNT` 전에 반드시 `MISSION_CLEAR_ALL`을 전송한다.

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

> **2026-07-13 수정**: §13.1 실측(legacy MISSION_ITEM 경로에서 ArduPilot이
> `MAV_FRAME_LOCAL_NED`를 거부함이 확인됨)에 근거해, INT 경로도 legacy와 동일하게
> `MAV_FRAME_GLOBAL_RELATIVE_ALT` + lat/lon 변환으로 전환했다(`mission_item_int_frame_gap`
> 갭 해소). 아래 표는 이 수정을 반영한 현재 구현 기준이다 — 단 INT 경로 자체의
> 실물 FC 검증(FC가 실제로 INT를 요청하는지, `MISSION_ACK` accepted 여부)은 아직
> 안 됨(FC 점유 중).

| MAVLink 필드 | 값 |
| --- | --- |
| `target_system` | FC system ID (런타임에 Heartbeat에서 획득) |
| `target_component` | FC autopilot component ID |
| `seq` | 웨이포인트 인덱스 (0-based) |
| `frame` | `MAV_FRAME_GLOBAL_RELATIVE_ALT` (= 3) |
| `command` | `MAV_CMD_NAV_WAYPOINT` (= 16) |
| `current` | 0 (첫 번째 항목도 0, SET_CURRENT_ITEM으로 별도 지정) |
| `autocontinue` | 1 |
| `param1..4` | 0.0 (hold time, acceptance radius 등 미사용) |
| `x` (lat) | `Waypoints[i].LatE7`(int32, degE7) 그대로 기입(2026-07-27 정정) — **BL-56(2026-07-25)에서 local X/Y(m)→RefLat/RefLon 기반 변환 로직 자체가 삭제됨**(`utils.c:404-433` 주석). 기존 표는 "RefLat + local X 기반 delta"였으나 낡음, 항상 절대좌표 LatE7을 그대로 씀 |
| `y` (lon) | `Waypoints[i].LonE7`(int32, degE7) 그대로 기입 — 동일하게 로컬→전역 변환 없이 절대좌표 직결 |
| `z` | `Waypoints[i].Z` (float meters, relative altitude, 부호 반전 없음 — GLOBAL_RELATIVE_ALT는 이미 altitude-positive) |
| `mission_type` | `MAV_MISSION_TYPE_MISSION` (= 0) |

x/y는 `int32` (degE7, MISSION_ITEM_INT 표준 인코딩), z는 `float` meters(relative altitude)임에 유의한다.
`MISSION_ITEM`(legacy, float degree)과 인코딩 폭만 다를 뿐 좌표 변환 공식 자체는 동일하다.

## 8. 타이밍 및 재시도

| 파라미터 | 값 |
| --- | --- |
| 단계별 응답 대기 timeout | 2000 ms |
| 최대 재시도 횟수 | 3 |
| 재시도 조건 | timeout. 예상치 못한 seq 응답은 즉시 재시도하지 않고 무시되며, 이후 timeout 경로에서 재시도된다. |
| 재시도 시 재시작 위치 | MISSION_COUNT부터 전체 재시작 |

> 정의 위치: `mavlink_bridge_app_utils.c:63-65` (`MAVLINK_BRIDGE_APP_MISSION_UPLOAD_TIMEOUT_MS=2000U`, `MAVLINK_BRIDGE_APP_MISSION_MAX_RETRIES=3U`, `MAVLINK_BRIDGE_APP_MISSION_CLEAR_DELAY_MS=300U`) — 코드 확인, 2026-06-16.

### 8.1 링크·스트림 타이밍 및 CONFIG 한도 상수 (internal_cfg)

`default_mavlink_bridge_app_internal_cfg_values.h` 정의 (2026-07-20 재감사 시 문서화):

| 상수 | 값 | 의미 |
| --- | --- | --- |
| `RECONNECT_INTERVAL_MS` | 1000 | serial 재연결 시도 간격 |
| `STALE_TIMEOUT_MS` | 1000 | FC 수신 stale 판정 |
| `HEARTBEAT_INTERVAL_MS` | 1000 | 자체 HEARTBEAT 송신 주기 |
| `STREAM_REQUEST_RETRY_MS` | 2000 | 스트림 요청 재시도 간격 |
| `TARGET_DISCOVERY_TIMEOUT_MS` | 10000 | FC sysid lock-in 대기 한도 |
| `SYS_TIME_INTERVAL_US` | 1000000 | SYS_TIME 스트림 요청 간격 (1 Hz, §16) |
| `CONFIG_VERSION` / `CONFIG_SCOPE` | 1 / 2 | CONFIG 명령 수락 조건 |
| `PARAM_INTERVAL_MIN/MAX_US` | 10000 / 10000000 | CONFIG 스트림 간격 파라미터 허용 범위 |
| `PARAM_MS_MIN/MAX` | 100 / 60000 | CONFIG ms 파라미터 허용 범위 |

**BL-19(2026-07-22, 신규 기재)**: `SERIAL_BAUDRATE`(int→`speed_t` 변환)는 원래
이 앱 내부 static `GetBaudConstant()`였으나, `lora_tdm_app`도 동일 변환이
필요해져 `shared_msgs/serial_baud.h`(`SERIAL_BAUD_GetConstant()`)로 이관·공유—
지원 baud 목록(9600/19200/38400/57600/115200/230400/460800/921600)과 동작은
불변, 위치만 이동.

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
- **Active waypoint cache 갱신**: `MissionPendingX/Y/Z[0..WpCount-1]` 내용을 `ActiveWaypointX/Y/Z`에 복사하고 `ActiveWaypointCount = MissionUploadWpCount`로 설정. 이후 APPEND/DELETE 연산의 기준 상태가 된다.

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

## 10. FC MISSION 재조회(readback) 명세 (BL-41 route, 2026-07-23 재정의)

**목적**: FC에 저장된 현재 미션을 읽어 **`FC_MISSION_READBACK_MID(0x1914)`로
게시**한다 — cfs_core_app이 구독해 `MissionRoute` 캐시를 채운다(FC가 유일
진실원본, Pi 캐시는 RAM 전용 미러 — `bl41_route_buffer_design_2026-07-23.md`).
EVS 로그 출력은 진단용으로 유지.

**트리거 3종** (모두 동일 다운로드 상태머신 공유):

| # | 트리거 | 조건 |
| --- | --- | --- |
| 1 | FC 링크 **CONNECTED 전이** (엣지) | 이전 ≠ CONNECTED && 신규 == CONNECTED && upload/download 둘 다 IDLE. 전이 시 백오프 리셋. DISCONNECTED 전이는 대기 중 재시도 취소 |
| 2 | 미션 **업로드 완료** (MISSION_ACK ACCEPTED 수신, upload ACTIVE→IDLE 직후) | download IDLE일 때 — FC가 실제 받아들인 값으로 캐시 확정(검증 겸함) |
| 3 | 지상 명령 `MISSION_QUERY_CC` (CMD_MID 0x18A0, CC=2) | 기존과 동일 (링크 CONNECTED 필요) |

**다운로드 프로토콜** (기존과 동일):

```
mavlink_bridge_app → FC : MISSION_REQUEST_LIST
FC → mavlink_bridge_app : MISSION_COUNT (count=N)
mavlink_bridge_app → FC : MISSION_REQUEST_INT (seq=0)
FC → mavlink_bridge_app : MISSION_ITEM_INT    (seq=0)
...
FC → mavlink_bridge_app : MISSION_ITEM_INT    (seq=N-1)
mavlink_bridge_app → FC : MISSION_ACK         (MAV_MISSION_ACCEPTED)
```

**항목 처리**: 각 `MISSION_ITEM_INT`의 lat/lon degE7·alt를 업로드 변환
(§13.1)의 정확한 역함수로 로컬 미터로 되돌려 버퍼링:

```
X = (WpLatDeg - RefLatDeg) * DEG_TO_RAD * EARTH_RADIUS_M
Y = (WpLonDeg - RefLonDeg) * DEG_TO_RAD * EARTH_RADIUS_M * cos(RefLatRad)
Z = Alt (무변환)
```

Ref(GLOBAL_POSITION_INT 기준점)가 (0,0)이면 업로드와 동일하게 그대로 진행
(왕복 정합 유지). 진단 EVS `[wp N] lat/lon/alt/cmd` 출력은 유지.

**완료 게시**: 다운로드 완료 시 `ROUTE_UPDATE_TLM_t` 레이아웃 그대로
`FC_MISSION_READBACK_MID(0x1914)`에 실어 게시. `SourceSequence=0`,
`RouteType=1`(MISSION), `RouteVersion=0`, `Seq`=자체 증가 카운터,
`WaypointCount=min(N,ROUTE_MAX_WAYPOINTS)`(37, 2026-07-28 확장 — 초과분 클램프 + WARN). 신규
`MISSION_READBACK_EID(16)` INFO 발생.

**재시도(백오프)**: 전체 시퀀스가 timeout(3s)으로 실패하면 링크가
CONNECTED인 동안 **무한 재시도, 지수 백오프 1s→2s→4s→5s(상한 고정)**.
재시도 발화는 `ServiceSerial()` 주기 체크(upload/download IDLE 조건).
링크 DISCONNECTED 전이 시 대기 중 재시도 즉시 취소 — 이후 재연결
(트리거 1)이 백오프를 리셋하고 재개. 근거: readback 실패 = 임무 수행
불가이므로 포기 금지, 단 요청 폭주 방지(BL-38 "무한 재시도" 패턴).

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
| `NonFiniteValueCount` | `uint32` | NaN/Inf 값 거부 누적 횟수 (§12.1) |

### 12.1 FC 값 finite 검증 (NaN/Inf 입구 차단)

CRC는 전송 오류만 잡고 값 자체의 NaN/Inf(EKF 발산, 센서 고장 등)는 통과시키므로,
FC 상태 메시지 파싱 직후 `isfinite()` 검사로 걸러 **SB 게시 자체를 차단**한다
(`MAVLINK_BRIDGE_APP_ValuesFinite6`/`RecordNonFiniteError`, `mavlink_bridge_app_utils.c:283-295`).
거부 시 `NONFINITE_VALUE_ERR_EID (13)` ERROR 이벤트 발생, `NonFiniteValueCount` 증가,
`LastErrorCode = INVALID_VALUE`. 이 카운터는 HK로 노출되며 `cfs_core_app`의
`BRIDGE_HK_TLM_t` 미러(shared_msgs/bridge_hk_msg.h)에도 포함된다.

> 참고 (2026-07-20 재감사): FC 상태 payload 구조체 4종(`EkfLocalTlm_t` 등)의 실정의는
> `shared_msgs/fc_state_msg.h`(`FC_*_TLM_t`)로 병합되었고, 앱측 이름은 typedef로 유지된다.

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

**⚠️ 이 섹션의 "기준점/변환 공식" 부분은 BL-56(2026-07-25)으로 대부분 폐기된 역사적 기록이다(2026-07-27 정정)**: `ROUTE_WAYPOINT_t`가 local X/Y(m) 필드를 아예 없애고 `LatE7`/`LonE7`(절대좌표)만 갖는 구조로 바뀌면서, 아래의 "FC 최신 GLOBAL_POSITION_INT를 기준점(RefLat/RefLon)으로 삼아 로컬 X/Y를 위경도로 환산"하는 변환 로직 자체가 `mavlink_bridge_app_utils.c`에서 삭제됐다(§7 표 참조). GPS 없을 때 `(0,0)` 기준으로 변환하던 것도 더 이상 해당 없음 — 좌표는 지상국이 이미 절대좌표로 만들어 보내고 기체는 그대로 씀. `frame = MAV_FRAME_GLOBAL_RELATIVE_ALT` 자체는 지금도 맞지만, 아래 "기준점"·"변환 공식" 하위 절은 **더 이상 코드와 일치하지 않는 이전 설계**로만 참조할 것.

ArduPilot은 미션 아이템에서 `MAV_FRAME_LOCAL_NED` (= 1)을 거부한다 (`MISSION_ACK result=2 = MAV_MISSION_UNSUPPORTED_FRAME`). `MAV_FRAME_GLOBAL_RELATIVE_ALT` (= 3)를 사용해야 한다(이 문장 자체는 여전히 유효).

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

`MISSION_ITEM (msg 39)` 경로. `MISSION_ITEM_INT (msg 73)` 경로도 2026-07-13 동일하게
GLOBAL_RELATIVE_ALT로 전환됨 — 아래 13.1.1 참조.

### 13.1.1 경로별 frame 및 좌표 인코딩 현황 (2026-07-13 갱신)

| 경로 | MAVLink 메시지 | frame | x/y 인코딩 | z 인코딩 |
| --- | --- | --- | --- | --- |
| **INT 경로** | MISSION_ITEM_INT (73) | `MAV_FRAME_GLOBAL_RELATIVE_ALT` (3) | lat/lon → int32 degE7 (§12.1 공식과 동일 변환, 인코딩 폭만 다름) | float meters, altitude-positive |
| **Legacy 경로** | MISSION_ITEM (39) | `MAV_FRAME_GLOBAL_RELATIVE_ALT` (3) | GPS 기준점 기반 lat/lon 변환 (§12.1 공식) | float meters, altitude-positive |

두 경로 모두 GLOBAL_RELATIVE_ALT로 통일됨 (`mission_item_int_frame_gap` 해소).
INT 경로 자체의 실물 FC 검증(FC의 `MISSION_ACK result` 확인)은 아직 잔여 —
FC가 다른 작업에 점유돼 있어 코드 수정만 선반영하고 실측은 보류.

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

**위치**: `legacy/tools/mission_upload_diag.py`

cFS 없이 Pi에서 직접 MAVLink mission upload 시퀀스를 실행하고 FC 응답을 단계별로 출력하는 진단 도구.

**목적**: cFS 브리지와 MAVProxy 간 동작 차이를 비교하여 FC가 응답하지 않는 원인 규명.

**실행**:
```bash
# cFS 종료 후
python3 legacy/tools/mission_upload_diag.py --port /dev/serial0 --baud 57600
```

**단계별 동작**:
1. FC heartbeat 수신 → `target_sysid`, `target_compid` 획득
2. `MISSION_CLEAR_ALL(45)` 전송 → `MISSION_ACK` 대기
3. `MISSION_COUNT(N)(44)` 전송 → `MISSION_REQUEST_INT(51)` 또는 `MISSION_REQUEST(40)` 대기
4. 요청된 seq에 맞는 `MISSION_ITEM_INT(73)` 또는 `MISSION_ITEM(39)` 응답
5. 최종 `MISSION_ACK` 수신 및 result 출력

**출력 형식**: 각 단계에서 TX/RX 프레임의 msgid, payload hex 전부 출력 → cFS 브리지와 직접 비교 가능.

**sysid/compid**: `255/190` (브리지와 동일)

**`_Parser` STX 리싱크 설계 결정 (2026-07-14)**: `_Parser.feed()`는 프레임
파싱 도중(LEN~CRC2)이라도 바이트 값이 `0xFD`/`0xFE`(STX_V1/V2)와 우연히 같으면
리싱크했었다 — MAVLink는 페이로드를 이스케이프하지 않으므로, 부동소수점 좌표
인코딩 시 우연히 이 값이 섞이면(예: `z=-3.0`) 정상 프레임을 파싱 도중 놓쳐
"FC 무응답"으로 오진단하는 실제 버그로 이어졌다(`test_roundtrip_mission_item_int`로
실증). 상태 가드를 추가해 **`self.state == 'STX'`(유휴 상태)일 때만 리싱크**하도록
변경 — 실 MAVLink 파서(pymavlink 등) 계약과 일치하며, 파싱 도중 우연한 STX 값은
정상 LEN/SEQ/PAYLOAD/CRC 데이터로 소비한다. 동기화가 실제로 깨져도 CRC 불일치로
걸러지고 다음 유휴 상태에서 재동기화되므로 "빠른 복구" 이점 상실은 이 CLI 디버그
툴 맥락에서 무시 가능(최대 지연 ~44ms, 5초 타임아웃 단위 동작).
(근거: `notes/temp/mission_upload_diag_test_findings.md`, 커밋 `8677489`)

---

## 15. 미구현 사항

**⚠️ 2026-07-27 정정**: 아래 "INT 경로 GLOBAL_RELATIVE_ALT 변환 미구현" 항목은
같은 문서 §7/§13.1.1과 자기모순이었음(spec_code_review_2026-07-27 §4-5) —
코드 확인 결과(`utils.c:426`, `SendMissionItemInt`) §7/§13.1.1이 맞고 이
항목이 낡은 잔재 문구라 제거함. BL-56(2026-07-25) 이후로는 애초에 INT/legacy
양쪽 경로 모두 §13.1/§13.1.1의 local-NED 변환 로직 자체가 삭제되고 절대좌표
(LatE7/LonE7)를 직결하므로, "GLOBAL_RELATIVE_ALT 변환"이라는 프레이밍 자체가
구식이다 — 상세는 §7 표 및 §13.1 상단 정정 참조.

- FC 현재 미션 항목 변경 (`MAV_CMD_DO_SET_MISSION_CURRENT`)
- Landing route 업로드 — **BL-56(2026-07-25) 확인**: 별도 landing route 캐시 자체가 애초에 존재한 적 없음(`utils.c:715` 주석 — "RouteType=1 MISSION 고정, FC에 landing 세그먼트 개념 없음"). 이 항목은 없는 개념에 대한 미구현이라 실질 의미 없음.
- 업로드 완료 후 자동 미션 시작

다음 항목은 구현 완료되었다:

- **⚠️ mid-flight 경로 변경 안전 검사(ARMED 차단)는 2026-07-27 기준 더 이상 사실이 아님** — BL-56(2026-07-25)에서 전면 폐지됨(§5 상단 정정 참조). `MAVLINK_BRIDGE_APP_ARMED_WARN_EID`는 정의만 남은 죽은 코드.
- Global mission frame (`MAV_FRAME_GLOBAL_RELATIVE_ALT`) 자체는 legacy(msg 39)/INT(msg 73) 양쪽 경로 모두 적용돼 있으나, **좌표 변환 방식이 BL-56으로 바뀜** — RefLat/RefLon 기반 local→global 변환은 삭제되고 절대좌표(LatE7/LonE7)를 그대로 사용(§7/§13.1 상단 정정 참조).

## 16. FC SYSTEM_TIME 수신 및 시각 동기 설계

### 16.1 목적

FPV 영상(OSD 번인 타임스탬프)과 텔레메트리 로그를 GPS 절대시각(UTC) 기준으로 대조하기 위해, FC의 `SYSTEM_TIME (msg 2)`을 수신하여 GPS 기반 UNIX epoch 시각을 확보한다. 최종 목표는 Pi(CM) 시스템 시계를 이 시각에 동기시켜 전 구간(영상·SB 로그·LoRa 다운링크 로그)이 동일한 UTC 시간축을 공유하는 것이다.

### 16.2 수신 파싱 (구현 완료, 2026-07-12, commit 30a8d7b)

**Stream 요청**: `RequestTelemetryStreams()`에서 `MAV_CMD_SET_MESSAGE_INTERVAL`로 `SYS_TIME`을 1 Hz(`MAVLINK_BRIDGE_APP_SYS_TIME_INTERVAL_US = 1000000`) 요청한다. 다른 스트림과 달리 런타임 CONFIG 파라미터가 아닌 컴파일 타임 상수를 사용한다 (CONFIG 이중버퍼 미적용 — `ConfigParams_t` wire 구조 변경 회피).

**payload 디코드** (`HandleSysTime`, `mavlink_bridge_app_utils.c`):

| Byte | 필드 | 형식 |
| --- | --- | --- |
| `[0..7]` | `time_unix_usec` | `uint64` LE — GPS 기반 UNIX epoch (µs) |
| `[8..11]` | `time_boot_ms` | `uint32` LE — FC 부팅 후 경과 (미사용) |

**파싱 규칙**:

1. MAVLink v2 zero-trimming 대응: 후행 0바이트가 잘려 12바이트 미만으로 수신될 수 있으므로(예: `time_boot_ms` 상위 바이트 0) 12바이트 버퍼에 zero-extend 후 디코드한다. 12바이트 **초과**만 길이 오류(`RecordLengthError`) 처리.
2. `time_unix_usec == 0`은 FC가 아직 GPS(또는 GCS 주입) 시각을 확보하지 못한 상태 → **무시**하고 필드를 갱신하지 않는다.
3. CRC 불일치 → `RecordParseError` (CRC_EXTRA = 137).

**내부 상태 필드** (`MAVLINK_BRIDGE_APP_Data_t`):

| 필드 | 형식 | 의미 |
| --- | --- | --- |
| `LastSysTimeUnixUsec` | `uint64` | 마지막 유효 수신한 GPS UNIX epoch (µs) |
| `LastSysTimeRxMs` | `uint32` | 해당 수신 시각 (bridge 로컬 ms) |

**단위테스트** (`coveragetest_mavlink_bridge_app_utils.c`, non-blocking pipe로 `ServiceSerial()` 경유 프레임 주입):

| 테스트 | 검증 |
| --- | --- |
| `SysTime_FullPayload` | 12바이트 정상 프레임 → 필드 갱신 |
| `SysTime_TrimmedPayload` | 8바이트 트림 프레임 → zero-extend 디코드 |
| `SysTime_ZeroClockIgnored` | `unix_usec=0` → 무시 |
| `SysTime_CrcFail` | CRC 불일치 → `ParseErrorCount` 증가, 필드 미갱신 |

> Pi 실빌드/UT 실행 검증은 미완 (작성 시점 Pi 오프라인).

### 16.3 SB 발행 — SysTimeTlm (구현 완료, 2026-07-13)

다른 FC 상태 MID(`FC_ATTITUDE_STATE_MID` 등, `default_mavlink_bridge_app_msgstruct.h`의 `EkfLocalTlm_t`/`AttitudeTlm_t`/`GpsRawTlm_t`/`EkfStatusTlm_t`와 동일한 필드 관례)와 동일한 패턴으로 발행한다.

**MID** (`default_mavlink_bridge_app_interface_cfg_values.h`에 추가, 기존 `FC_EKF_LOCAL_STATE_MID_VALUE 0x1905` ~ `FC_EKF_STATUS_MID_VALUE 0x1908` 다음 미사용 값):

```c
#define FC_SYS_TIME_MID_VALUE 0x1909
```

**payload 구조체** (`default_mavlink_bridge_app_msgstruct.h`에 추가, 기존 4개 FC Tlm 공통 헤더 필드 `TimestampMs`/`Seq`/`Valid`/`Stale`/`ErrorCode`/`Reserved` 그대로 유지):

```c
typedef struct
{
    CFE_MSG_TelemetryHeader_t TelemetryHeader;
    uint32                    TimestampMs;   /* = LastSysTimeRxMs (bridge 로컬 수신 시각, ms) */
    uint32                    Seq;
    uint8                     Valid;
    uint8                     Stale;
    uint8                     ErrorCode;
    uint8                     Reserved;
    uint64                    TimeUnixUsec;  /* = LastSysTimeUnixUsec (GPS 기반 UNIX epoch, us) */
} MAVLINK_BRIDGE_APP_SysTimeTlm_t;
```

| 항목 | 값 |
| --- | --- |
| 발행 조건 | `HandleSysTime()`에서 유효 SYSTEM_TIME 수신 시 (`unix_usec != 0`, CRC OK) — 즉 `LastSysTimeUnixUsec` 갱신과 같은 지점에서 발행 |
| 초기화 | `MAVLINK_BRIDGE_APP_Init()`에서 다른 FC Tlm과 동일하게 `CFE_MSG_Init()` 1회 |
| 발행 함수 | `CFE_SB_TransmitMsg(CFE_MSG_PTR(MAVLINK_BRIDGE_APP_Data.SysTimeTlm.TelemetryHeader), true)` — 기존 4개 FC Tlm 발행 호출과 동일 패턴 |
| 구독자(예상) | cFS 내부 시각 규율 소비자 (§16.4 — `CFE_TIME_ExternalGPS` 호출), 지상 진단용 다운링크(선택) |

**단위테스트 검증**: 기존 `SysTime_*` 4개 테스트(§16.2) + `mavlink_bridge_app` Init 테스트가 새 `CFE_MSG_Init()` 호출 경로를 포함해 회귀 없이 통과. 로컬 `~/verify-build/cFS_verify` 재빌드 결과: `coverage-mav_bridge_app-mavlink_bridge_app_utils-testrunner` 105/105 PASS, `coverage-mav_bridge_app-mavlink_bridge_app-testrunner` 14/14 PASS.

`mission_app_runtime_spec.md` §5.1.1 MID 계약 테이블에 `FC_SYS_TIME_MID (0x1909)` 행 추가 완료 (2026-07-20 재감사에서 반영).

**남은 유의사항**: §16.2에서 서술한 mavlink_bridge STX 이스케이프 결함(P1, 04-repository-map.md §5)이 아직 해결되지 않은 상태 — SysTimeTlm 발행은 살아있지만 페이로드 내 `0xFD`/`0xFE` 우연 출현 시 SYSTEM_TIME 프레임 자체가 유실될 수 있다. 또한 Pi 실기 연결 검증(실제 FC로부터 SYSTEM_TIME 수신 → SB 발행 확인)은 아직 미완 — 지금까지는 로컬 UT 검증만 완료된 상태.

### 16.4 시각 반영 — 두 시계 도메인 (설계 정정 2026-07-13, 방향 재정정 2026-07-14)

> **채택된 실제 경로 (2026-07-14)**: 아래 §16.4.1(`CFE_TIME_ExternalGPS`)은
> **보류**로 결론. 대신 GPS 시각을 **DL2 다운링크 프레임에 실어 지상으로
> 전달**하는 방식을 채택·구현 완료 — `lora_tdm_app`이 `FC_SYS_TIME_MID`를
> 구독해 DL2에 SysTime 확장 블록(§4.2, `lora_protocol_v2_spec.md`)으로
> 포함, 지상(openMCT)이 WS/CSV로 노출. cFE 코어 미변경, 4개 앱 전부에 영향
> 없음. 상세: `notes/temp/gps_time_sync_164_implementation.md`.
>
> **§16.4.1 보류 사유**: 조사 중 `CFE_TIME_ExternalGPS` 호출 시 STCF가
> 갱신되는 순간 `CFE_TIME_GetTime()`이 점프하는데, 4개 앱의 `GetTimeMs()`가
> **전부** 이 함수를 그대로 쓰고 있음이 드러남(당초 이 문서가 "PSP는
> MONOTONIC이라 안전"이라 가정했던 것과 다름 — 그건 PSP 틱 소스 얘기고,
> 앱이 실제로 읽는 `CFE_TIME_GetTime()`엔 STCF가 얹힘). GPS 락 잡히는
> 순간 모든 앱의 health/timeout 비교식이 오작동해 `cfs_core_app`이
> 앱을 오탐 재시작시킬 위험이 있어 보류. 재시도하려면 4개 앱
> `GetTimeMs()`를 먼저 진짜 monotonic 소스로 분리해야 함(후속 과제,
> 미착수).
>
> **§16.4.2(chrony) 방향은 일부 확정**: Pi에 별도 GPS 리시버는 불필요 —
> FC GPS(SysTimeTlm)로 이미 충분. 필요하면 SysTime을 chrony SOCK
> refclock으로 주입하는 작은 브릿지 유틸리티(옵션 b)로 진행. 단, DL2
> 다운링크 경로로 이미 지상에서 GPS UTC를 얻으므로(§4.2), 카메라 OSD
> 동기 목적이라면 chrony 없이 **지상 PC에서 DL2의 sys_time_unix_usec을
> 직접 활용**하는 방법도 가능 — 아직 미결정, 카메라 작업 착수 시 결정.

**핵심**: 이 시스템에는 GPS UTC로 규율해야 할 시계가 **두 개**이고, 서로 다른 밑바탕 소스를 쓴다. 이전 판(호스트 파이썬 데몬으로 OS 시계만 맞춤)은 cFS 로그용 시계와 카메라용 시계를 혼동한 오류였다.

| 시계 도메인 | 소비처 | 밑바탕 | 규율 방법 |
| --- | --- | --- | --- |
| **cFS 내부 시각** (`CFE_TIME`) | 모든 cFS 로그 타임스탬프 — SB 로그, LoRa 다운링크 로그(`CFE_TIME_GetTime()`) | MET(부팅 후 단조) + STCF 오프셋. 우리 PSP 타임베이스는 `CLOCK_MONOTONIC`(`cfe_psp_timebase_posix_clock.c:60`) — **NTP/`clock_settime`에 영향 안 받음** | §16.4.1 — `CFE_TIME_ExternalGPS()` (순수 C, cFS 안) |
| **리눅스 OS 시계** (`CLOCK_REALTIME`) | 카메라 OSD (NTP 경유) | OS realtime clock | §16.4.2 — chrony (OS 관리, cFS 밖). **카메라 때문에만 필요** |

두 도메인은 밑바탕이 다르므로 한쪽을 맞춰도 다른 쪽은 안 바뀐다(OS 시계를 맞춰도 cFS 로그는 MONOTONIC 기반이라 불변). 둘 다 같은 GPS UTC로 규율하면 sync tolerance(§6, ~100ms) 안에서 일치한다.

#### 16.4.1 cFS 내부 시각 규율 — `CFE_TIME_ExternalGPS` (권장, C in-cFS)

cFS가 제공하는 외부 시각 주입 API로 STCF를 GPS UTC에 맞춘다. 이러면 `CFE_TIME_GetTime()`을 쓰는 **모든 cFS 로그 타임스탬프가 자동으로 GPS UTC 축**이 된다. 호스트 데몬·chrony·root 불필요.

```c
void CFE_TIME_ExternalGPS(CFE_TIME_SysTime_t NewTime, int16 NewLeaps);   /* cfe_time.h:581 */
```

- **입력 변환**: §16.2에서 확보한 `TimeUnixUsec`(GPS UNIX epoch µs)를 `CFE_TIME_SysTime_t`(Seconds/Subseconds)로 변환. UNIX epoch → cFS epoch(기본 1980 TAI 등 `CFE_MISSION` epoch) 오프셋과 leap seconds(`NewLeaps`) 처리 주의.
- **호출 주체(설계 선택)**:
  - (권장) `mavlink_bridge_app`의 `HandleSysTime()`에서 직접 호출 — 값이 이미 그 자리에 있고, `CFE_TIME_ExternalGPS`는 OS 전역 부작용이 아니라 cFS 내부 API라 §4 앱 경계를 벗어나지 않음. CONFIG 파라미터로 on/off.
  - (대안) SysTimeTlm(§16.3)을 구독하는 별도 소비자(예: `cfs_core_app`)에서 호출 — 시각 권한을 한 곳에 모으고 싶을 때. 결합도 증가.
- **플랫폼 선행조건**: 이 CPU가 TIME 서버이고 시각 소스가 외부(`CFE_PLATFORM_TIME_CFG_SOURCE` 계열, `CFE_TIME_SourceSelect_EXTERNAL`)로 설정돼 있어야 `ExternalGPS`가 유효. 현 빌드 설정 확인 필요(미확인).
- **fallback**: `TimeUnixUsec == 0`(GPS 미확보) → 호출 안 함(§16.2 규칙). GPS 상실 시 cFS는 MET(monotonic)로 자유 구동, STCF는 마지막 값 유지.

> 이전 판의 기각 근거였던 "① `clock_settime` 전역 부작용 ② step으로 로그 점프"는 **OS 시계(`clock_settime`)에만** 해당한다. `CFE_TIME_ExternalGPS`는 cFS 내부 STCF 상관만 갱신하고 점프 처리는 CFE_TIME 상태머신이 담당하므로 두 근거 모두 비적용 → cFS 로그 규율은 C로 cFS 안에서 하는 것이 맞다.

#### 16.4.2 리눅스 OS 시계 — chrony (카메라 NTP 전용)

카메라(WiFiLink)는 이더넷 NTP로 **Pi의 OS realtime 시계**에 동기하므로(§6.1 체인), 이 시계는 별도로 규율해야 한다. cFS 로그는 이 시계에 의존하지 않으므로(위 표) **오직 카메라를 위해서만** 필요하다.

- OS 시계 설정은 시스템 전역·root 권한 작업이라 cFS 앱이 하지 않는다(원래 §16.4 근거 유효). chrony가 담당.
- 규율 소스: GPS 시각을 chrony에 넣는 방법은 두 가지 — (a) OS에 이미 GPS/gpsd가 있으면 chrony가 직접 그 refclock을 사용, (b) 없으면 cFS가 아는 GPS 시각을 chrony SOCK refclock으로 주입하는 작은 유틸리티. **어느 쪽인지 미결정** — 실기에서 Pi에 GPS 수신기 직결 여부에 달림.
- 카메라 NTP 서버 설정은 `camera/pi_chrony_camera.conf`(프로토타입). Pi OS 시계가 위 경로로 규율된 뒤에야 카메라 OSD 타임스탬프가 UTC로서 의미를 가진다.

> 정정 요지(2026-07-13): SysTimeTlm의 1차 소비자는 "호스트 데몬"이 아니라 **cFS 내부 `CFE_TIME_ExternalGPS` 호출**이다. chrony/OS-시계 경로는 카메라 전용으로 범위 축소. 양쪽 모두 코드 미착수 — 16.4.1의 epoch/leap 변환과 플랫폼 TIME 소스 설정 확인이 착수 선행조건.

### 16.5 정확도 한계

| 구간 | 예상 오차 |
| --- | --- |
| GPS → FC | ~ms |
| FC → bridge (UART 57600, 1 Hz 폴링) | ~수십 ms 지터 |
| bridge → cFS 내부 시각 (`CFE_TIME_ExternalGPS`) | STCF 상관 갱신 — 수신 지터 수준(~수십 ms) |
| bridge → OS 시계 (카메라용, chrony) | chrony slew 수렴 후 ~수십 ms |

영상 프레임 ↔ 텔레메트리 로그 대조 용도 기준 ~100 ms급이면 충분하다는 전제. PPS급 정밀도가 필요해지면 별도 하드웨어(GPS PPS 직결)로 이관한다.

## 17. MAVLink 파서 STX 이스케이프 결함 (P1, 수정 방안 확정 · ✅ 코드 적용 완료)

**⚠️ 2026-07-27 정정**: 부제의 "코드 미적용"은 오기였다 — 실제로는 이미 적용되어 있음(`ProcessReceivedByte`, `mavlink_bridge_app_utils.c:1875-1893`에서 STX 체크가 `MAVLINK_PARSE_WAIT_STX` 상태에서만 수행되도록 가드됨, §17.2 "채택 방안"과 일치). 관련 완료 노트 `mavlink_stx_reentry_parser_bug_completed.md`가 맞고 이 부제 표시가 틀렸었다. 아래 §17.1(결함 서술)은 **수정 전 상태에 대한 역사적 기록**으로 읽을 것 — 현재 코드에는 해당하지 않는다.

### 17.1 결함 (수정 전 — 역사적 기록)

`MAVLINK_BRIDGE_APP_ProcessReceivedByte()`(`mavlink_bridge_app_utils.c:1549`)는 함수 진입부에서 **모든 수신 바이트에 대해, 파서 상태와 무관하게** STX 검사를 수행한다:

```c
if (Byte == MAVLINK_STX_V1)  /* 0xFE */ { ResetParser(); ... return; }
else if (Byte == MAVLINK_STX_V2)  /* 0xFD */ { ResetParser(); ... return; }
switch (Parser.State) { ... }
```

MAVLink는 바이트 스터핑(escape)이 없으므로 페이로드 데이터에 `0xFD`/`0xFE`가 그대로 실려 온다. 파서가 `READING_PAYLOAD` 상태로 페이로드를 읽는 도중 해당 값을 만나면, 진행 중이던 프레임을 버리고 새 프레임 시작으로 오인하여 **프레임을 유실**한다. 페이로드 바이트 랜덤 분포 가정 시 28바이트 프레임 기준 ~20% 유실률. SYSTEM_TIME(§16) 포함 모든 수신 경로가 영향권.

### 17.2 채택 방안 — 길이 기반 소비 (방안 A)

프레임의 `LEN` 필드는 이미 `Parser.PayloadLen`으로 수신되어 있다. **STX는 `WAIT_STX` 상태에서만 프레임 시작 신호로 인식**하고, 그 외 상태(특히 `READING_PAYLOAD`)에서는 내용과 무관하게 위치 기반으로 정확히 `PayloadLen`바이트를 소비한다. 이는 표준 mavlink 파서(`mavlink_parse_char`)의 동작이며, LoRa v2 spec이 요구한 "길이 기반 상태머신" 방향과 동일하다.

- 손상/절단 프레임 재동기: `LEN`이 어긋나거나 프레임이 잘리면 CRC 검사에서 실패 → 파서 리셋 → `WAIT_STX`로 복귀 → 다음 STX부터 재동기. 즉 **재동기는 STX-mid-frame 리셋이 아니라 CRC 실패에 위임**한다.
- 트레이드오프: 와이어 글리치로 프레임이 절단되면 다음 1~2 프레임까지 유실될 수 있으나, 절단은 드물고(글리치 시에만) 페이로드 0xFD/0xFE 충돌은 상시 발생하므로 실익이 압도적으로 크다.

**기각한 방안 B(버퍼 재파싱)**: 표준 `mavlink_frame_char_buffer`처럼 수신 바이트를 버퍼링하고 CRC 실패 시 STX 다음 바이트부터 재파싱. 절단 복구도 빠르지만 구현 복잡도·버퍼 관리 비용 대비 실익(드문 절단의 빠른 복구)이 작아 채택하지 않음.

### 17.3 검증 계획 (코드 적용 시)

- 기존 `UT_FeedSerial`/`UT_BuildSysTimeFrame` 헬퍼로 **페이로드에 `0xFD`/`0xFE`를 포함한 프레임**을 주입해 정상 디코드되는지 회귀 테스트 추가.
- 프레임 절단 후 다음 정상 프레임이 CRC 재동기로 복구되는지 확인하는 테스트 추가.
