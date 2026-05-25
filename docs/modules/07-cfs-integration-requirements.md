# 07. cFS Integration 요구사항

## 1. 목적

이 문서는 모듈을 cFS 환경에 통합하기 위한 요구사항을 정의한다.

## 2. cFS App 구조

- **CFS-APP-01**: 각 runtime module은 명시적인 초기화 경로를 가진 cFS-compatible app 또는 app-managed component로 통합되어야 한다.
- **CFS-APP-02**: integration layer는 main processing loop에 들어가기 전에 configuration, Software Bus subscription, timer, event service를 초기화해야 한다.
- **CFS-APP-03**: main loop는 subscribed message, timer event, module status update를 처리하되, 관련 없는 모듈 실행을 block해서는 안 된다.
- **CFS-APP-04**: shutdown 동작은 모듈 자원을 해제하고 최종 상태를 cFS event/log mechanism을 통해 기록해야 한다.
- **CFS-APP-05**: `telemetry_app`은 active transport monitoring과 `TELEMETRY_STATUS_MID` publish를 담당하는 cFS app으로 구현되어야 한다.
- **CFS-APP-06**: `telemetry_app`은 `ALIVE`, `DEGRADED`, `LOST` 중 하나의 link-state classification을 publish해야 한다.
- **CFS-APP-08**: `imu_app`과 `gps_app`은 `03-interface-specification.md`의 인터페이스 정의에 따라 timestamped sensor state message를 SB에 publish해야 한다.
- **CFS-APP-09**: `mavlink_bridge_app`은 FC MAVLink byte stream을 수신하고, 이를 파싱하여 지원되는 각 MAVLink 메시지 타입에 대한 typed cFS SB message를 publish하는 cFS app으로 구현되어야 한다.
- **CFS-APP-10**: `mavlink_bridge_app`은 raw MAVLink byte frame을 cFS Software Bus에 올려서는 안 된다. 모든 FC 데이터는 publish 전에 `03-interface-specification.md` Section 3.2AB에 정의된 typed SB message format으로 변환되어야 한다.

## 3. Software Bus 메시지 처리

baseline 필수 SB 입력 집합은 `IMU_STATE_MID (0x1901)`, `GPS_STATE_MID (0x1902)`, `TELEMETRY_STATUS_MID (0x1903)`로 구성된다. image/video payload와 관련 metadata는 baseline SB 경로를 직접 통과하지 않으며, reconstruction path는 지상국 image/video 경로를 통해 입력을 수신한다. `mavlink_bridge_app`은 `03-interface-specification.md` Section 3.2AB에 정의된 `FC_LOCAL_POS_MID (0x1905)`, `FC_ATTITUDE_MID (0x1906)`, `FC_GPS_RAW_MID (0x1907)`, `FC_ODOMETRY_MID (0x1908)`, `FC_EKF_STATUS_MID (0x1909)`, `MAVLINK_BRIDGE_STATUS_MID (0x190A)`를 baseline SB 집합에 추가한다. 모든 MID 값은 최종 할당 전까지 provisional이다 (`OI-CFS-01`).

- **CFS-SB-01**: direct SB transport에 payload가 너무 큰 경우, integration layer는 reconstruction request/result metadata를 cFS Software Bus 또는 문서화된 bridge를 통해 라우팅해야 한다.
- **CFS-SB-02**: integration layer는 모듈 경계를 넘는 동안 message timestamp, source module identifier, status, payload reference field를 보존해야 한다.
- **CFS-SB-03**: message ID, source ID, payload schema ownership은 `03-interface-specification.md`를 따라야 한다.
- **CFS-SB-05**: `imu_app`은 SB에 `IMU_STATE_MID (0x1901)`를 publish해야 한다.
- **CFS-SB-06**: `gps_app`은 SB에 `GPS_STATE_MID (0x1902)`를 publish해야 한다.
- **CFS-SB-07**: `telemetry_app`은 SB에 `TELEMETRY_STATUS_MID (0x1903)`를 publish해야 한다.
- **CFS-SB-07A**: telemetry monitor-input producer는 link-state 평가를 위해 `active_transport_id`, `valid`, `update_age_ms`를 `telemetry_app`에 제공해야 한다.
- **CFS-SB-09**: Pose / Frame Alignment Module은 `IMU_STATE_MID (0x1901)`와 `GPS_STATE_MID (0x1902)`를 subscribe해야 한다.
- **CFS-SB-10**: Reconstruction Module은 지상국 image/video 경로를 통해 전달되는 image set 입력을 consume해야 한다.
- **CFS-SB-11**: cFS Integration Layer는 runtime health 및 communication-state 처리를 위해 `TELEMETRY_STATUS_MID (0x1903)`를 subscribe해야 한다.
- **CFS-SB-13**: `mavlink_bridge_app`은 수신된 각 `LOCAL_POSITION_NED` MAVLink 메시지에 대해 SB에 `FC_LOCAL_POS_MID (0x1905)`를 publish해야 한다.
- **CFS-SB-14**: `mavlink_bridge_app`은 수신된 각 `ATTITUDE` MAVLink 메시지에 대해 SB에 `FC_ATTITUDE_MID (0x1906)`를 publish해야 한다.
- **CFS-SB-15**: `mavlink_bridge_app`은 수신된 각 `GPS_RAW_INT` MAVLink 메시지에 대해 SB에 `FC_GPS_RAW_MID (0x1907)`를 publish해야 한다.
- **CFS-SB-16**: `mavlink_bridge_app`은 해당 MAVLink 메시지를 수신한 경우 `FC_ODOMETRY_MID (0x1908)`와 `FC_EKF_STATUS_MID (0x1909)`를 publish해야 한다. 이 메시지가 없다고 해서 parse error로 취급해서는 안 된다.
- **CFS-SB-17**: `mavlink_bridge_app`은 구성 가능한 주기로 `MAVLINK_BRIDGE_STATUS_MID (0x190A)`를 publish해야 한다. 이 메시지는 가장 최근에 수신한 MAVLink 메시지의 age를 기준으로 `ALIVE`, `DEGRADED`, `LOST` 중 하나의 `link_state`를 보고해야 한다.
- **CFS-SB-18**: `mavlink_bridge_app`이 비활성화된 경우에도 `FC_LOCAL_POS_MID`, `FC_ATTITUDE_MID`, `FC_GPS_RAW_MID`, `FC_ODOMETRY_MID`, `FC_EKF_STATUS_MID`, `MAVLINK_BRIDGE_STATUS_MID`가 없다고 해서 IMU, GPS, telemetry, reconstruction, 비-MAVLink alignment 흐름의 nominal operation을 막아서는 안 된다.

## 3A. 통신 링크 분리 요구사항

시스템은 서로 다른 두 개의 통신 링크 역할을 가진다. 각 링크는 cFS integration layer 내에서 독립적으로 관리, 감시, 보고되어야 한다.

### 링크 역할 할당

- **CFS-LNK-01**: LoRa telemetry link (`link_role = LORA`)는 heartbeat, HK, status, fault/event report, uplink command traffic만 전달해야 한다.
- **CFS-LNK-01A**: uplink command traffic은 기본적으로 cFS mission-layer command로 해석되어야 하며, FC 직접 제어 명령으로 처리되어서는 안 된다. 단, 기존 임무 경로 뒤에 추가 경로 segment를 반영하는 경로 수정 명령은 예외적으로 허용될 수 있다. 이 경우에도 해당 명령은 검증된 경로 정보 전달로만 처리되어야 하며, 비행 모드 변경, 모터/액추에이터 제어, FC-level mission upload, FC-level parameter 변경을 직접 수행해서는 안 된다.
- **CFS-LNK-02**: image/video link (`link_role = IMG_VID`)는 image frame, video stream, large payload transfer, reconstruction artifact traffic만 전달해야 한다.
- **CFS-LNK-03**: LoRa link에 배정된 traffic class를 image/video link로 라우팅해서는 안 되며, 그 반대도 마찬가지다. 단, 명시적으로 문서화되고 승인된 fallback policy가 있는 경우는 예외다.

### 독립적인 Health 상태 관리

- **CFS-LNK-04**: LoRa link health state는 `telemetry_app`이 전담 관리하며 `link_role = LORA`를 가진 `TELEMETRY_STATUS_MID (0x1903)`를 통해 publish해야 한다.
- **CFS-LNK-06**: LoRa link의 health state를 image/video link의 상태로 추론하거나 override해서는 안 되며, 반대도 마찬가지다.
- **CFS-LNK-07**: 두 링크의 health state는 모두 `03-interface-specification.md` Section 3.6.2에 정의된 `ALIVE`, `DEGRADED`, `LOST` 분류를 독립적으로 사용해야 한다.

### cFS 경계에서의 Timestamp 및 Correlation

- **CFS-LNK-09**: control/health 경로의 cFS Software Bus 메시지는 차량 측에서 생성 시점에 부여된 `cFS_TIME` timestamp를 가져야 한다. relay 또는 bridge component가 차량 생성 `timestamp` field를 덮어써서는 안 된다. image/video 경로의 authoritative timestamp policy는 현재 baseline에서 미정이며, 추후 통합 시험 결과에 따라 별도 확정한다.
- **CFS-LNK-10**: image/video path의 per-image correlation key 정책은 현재 baseline에서 미정으로 둔다. image capture를 기체 측에서 직접 제어하지 않으므로 image-level identifier를 필수 cFS contract로 고정해서는 안 된다.
- **CFS-LNK-11**: 동일 reconstruction job 또는 동일 source event를 참조하는 모든 LoRa status 또는 event message는 사용 가능한 경우 reconstruction 요청 또는 image/video metadata와 동일한 `job_id` 또는 `seq`를 가져야 한다.
- **CFS-LNK-12**: reconstruction 관련 메시지는 job submission 시 할당된 `job_id`를 가져야 한다. `job_id`는 reconstruction request, result, 관련 LoRa event message 전 구간에서 변경 없이 보존되어야 한다.
- **CFS-LNK-13**: `seq` field는 originating module이 할당해야 하며, relay 또는 bridge component가 다시 할당해서는 안 된다.

## 4. Timer 요구사항

- **CFS-TMR-01**: timer는 cFS timer service 또는 동등한 cFS-managed scheduling mechanism을 사용해야 한다.
- **CFS-TMR-02**: timer callback은 작업을 queue에 넣거나 모듈 상태를 signal해야 하며, long-running reconstruction 또는 blocking I/O를 직접 수행해서는 안 된다.
- **CFS-TMR-05**: baseline deployment에서 telemetry monitor-input producer는 nominal heartbeat period를 `500 ms`로 가정해야 한다.
- **CFS-TMR-06**: baseline deployment에서 telemetry monitor-input producer는 최소 `100 ms`마다 telemetry monitor freshness를 평가하고 publish해야 한다.
- **CFS-TMR-07**: `telemetry_app`은 유효한 monitor update가 `1000 ms` 동안 없으면 `DEGRADED`로 전이해야 한다.
- **CFS-TMR-08**: `telemetry_app`은 유효한 monitor update가 `3000 ms` 동안 없으면 `LOST`로 전이해야 한다.
- **CFS-TMR-09**: `telemetry_app`은 nominal heartbeat period와 구분되는 degraded/lost threshold configuration을 지원해야 한다.
- **CFS-TMR-10**: baseline deployment에서 `telemetry_app`은 유효한 telemetry monitor update 1회 후 `DEGRADED` 또는 `LOST`에서 `ALIVE`로 복귀해야 한다.

## 5. Configuration 관리

- **CFS-CFG-01**: startup configuration에는 module enable flag, reconstruction endpoint setting, output format, alignment transform parameter가 포함되어야 한다.
- **CFS-CFG-02**: module output publish 전에 초기화 과정에서 configuration validation을 수행해야 한다.
- **CFS-CFG-03**: 필수 configuration이 잘못된 경우 해당 모듈은 nominal operation에 들어가서는 안 되며, 추적 가능한 event를 발생시켜야 한다.
- **CFS-CFG-04**: runtime parameter update는 각 모듈 명세에서 runtime-changeable로 명시된 field에 대해서만 허용되어야 한다.
- **CFS-CFG-06**: `telemetry_app`은 nominal, degraded, lost-link threshold configuration을 지원해야 한다.
- **CFS-CFG-07**: `telemetry_app`은 link-state 평가에 사용할 active transport identifier configuration을 지원해야 한다.
- **CFS-CFG-09**: telemetry link-state 평가를 위한 baseline active transport identifier는 `1`이어야 한다.
- **CFS-CFG-10**: telemetry timing parameter에 대한 runtime update는 먼저 pending configuration buffer에 기록되어야 한다.
- **CFS-CFG-11**: pending telemetry configuration은 activation 전에 검증되어야 한다.
- **CFS-CFG-12**: 잘못된 pending telemetry configuration 값이 active configuration을 덮어써서는 안 된다.
- **CFS-CFG-13**: active telemetry configuration은 문서화된 safe application point에서만 교체되어야 한다.
- **CFS-CFG-14**: `telemetry_app` HK는 현재 active telemetry timing parameter와 configuration update status를 노출해야 한다.

## 5A. 배치 환경별 Telemetry Link 규칙

- **CFS-DEP-01**: Linux 기반 deployment에서 serial-connected telemetry radio를 사용하는 경우, telemetry monitor-input producer는 가능하면 `/dev/serial/by-id/` 아래의 안정적인 device path를 사용해야 한다.
- **CFS-DEP-02**: `/dev/ttyUSB*`와 같이 enumeration에 의존하는 serial path는 deployment 환경이 안정적인 이름을 보장하지 않는 한 fallback 또는 debug 전용 path로 취급해야 한다.
- **CFS-DEP-03**: deployment configuration은 telemetry monitor-input producer에 사용되는 serial device path, baud rate, active transport identifier를 문서화해야 한다.
- **CFS-DEP-04**: Linux 기반 deployment에서 `mavlink_bridge_app`은 가능하면 FC MAVLink serial connection에 대해 `/dev/serial/by-id/` 아래의 안정적인 device path를 사용해야 한다. `/dev/ttyUSB*`와 같은 enumeration 의존 path는 fallback 또는 debug 전용 path로 취급해야 한다. deployment configuration은 FC serial device path와 baud rate를 문서화해야 한다.

## 6. Logging 및 Event 처리

- **CFS-LOG-01**: integration layer는 INFO, WARNING, ERROR, DIAGNOSTIC의 log/event level을 정의해야 한다.
- **CFS-LOG-03**: `Invalid_Position`, remote reconstruction failure, alignment failure, configuration validation failure는 owning module이 해당 상태를 degraded-but-usable로 명시하지 않는 한 ERROR로 기록해야 한다.
- **CFS-LOG-04**: diagnostic log는 사후 분석을 위해 timestamp, source module, status/error code, 관련 payload reference를 포함한 충분한 문맥을 보존해야 한다.
- **CFS-LOG-05**: `telemetry_app`은 `DEGRADED` 및 `LOST` 전이를 각각 `WARNING`, `ERROR` level로 기록해야 한다.
- **CFS-LOG-06**: `telemetry_app`은 `LOST`에서 `ALIVE` 또는 `DEGRADED`로 회복된 경우 `INFO` level로 기록해야 한다.
- **CFS-LOG-07**: 잘못되었거나 접근 불가능한 image artifact reference는 image/video ingestion path 또는 Reconstruction Module이 `ERROR`로 기록해야 한다.
- **CFS-LOG-08**: `mavlink_bridge_app`은 serial connection loss와 reconnection을 각각 `WARNING`, `INFO` level로 기록해야 한다.
- **CFS-LOG-09**: `mavlink_bridge_app`은 안정적인 event identifier와 함께 MAVLink parse error를 `WARNING` level로 기록해야 하며, 가능한 경우 raw message ID를 포함해야 한다.
- **CFS-LOG-10**: `mavlink_bridge_app`은 `DEGRADED`, `LOST` link state 전이를 각각 `WARNING`, `ERROR` level로 기록하고, `ALIVE`로의 회복은 `INFO` level로 기록해야 한다.

## 7. 모듈 통합 방식

| 모듈 | 통합 패턴 | Trigger | 출력 대상 |
| --- | --- | --- | --- |
| `imu_app` | SB publisher | IMU sample ready | `IMU_STATE_MID (0x1901)` |
| `gps_app` | SB publisher | GPS sample ready | `GPS_STATE_MID (0x1902)` |
| `telemetry_app` | SB publisher | LoRa link status update | `TELEMETRY_STATUS_MID (0x1903)` with `link_role = LORA` |
| `mavlink_bridge_app` | Serial reader + SB publisher | MAVLink byte received from FC | `FC_LOCAL_POS_MID (0x1905)`, `FC_ATTITUDE_MID (0x1906)`, `FC_GPS_RAW_MID (0x1907)`, `FC_ODOMETRY_MID (0x1908)`, `FC_EKF_STATUS_MID (0x1909)`, `MAVLINK_BRIDGE_STATUS_MID (0x190A)` |
| Reconstruction Module | Request/response bridge to remote execution path | Ground-side image-set ready event 또는 명시적 job request | reconstruction result metadata SB topic, artifact reference |
| Pose / Alignment Module | Message-driven transform processor | 새로운 source pose/result 또는 transform config update | aligned pose/transform metadata SB topic |

## 8. 시험 요구사항

- **CFS-VER-01**: 검증 계획에는 app initialization 및 shutdown 동작 시험이 포함되어야 한다.
- **CFS-VER-02**: 검증 계획에는 Software Bus routing 및 payload reference preservation 시험이 포함되어야 한다.
- **CFS-VER-03**: 검증 계획에는 timer 동작과 non-blocking callback policy 시험이 포함되어야 한다.
- **CFS-VER-04**: 검증 계획에는 configuration validation과 runtime update policy 시험이 포함되어야 한다.
- **CFS-VER-05**: 검증 계획에는 event 및 logging 동작에 대한 failure-injection 시험이 포함되어야 한다.
- **CFS-VER-06**: 검증 계획에는 baseline SB input publish/subscribe 시험이 포함되어야 한다.
- **CFS-VER-07**: 검증 계획에는 `telemetry_app`의 degraded/lost/recovery 전이 시험이 포함되어야 한다.
- **CFS-VER-08**: 검증 계획에는 LoRa link health state가 image/video link 상태로부터 추론되거나 override되지 않음을 확인하는 시험이 포함되어야 한다 (`CFS-LNK-06`).
- **CFS-VER-09**: 검증 계획에는 동일 reconstruction job 또는 source event를 참조하는 reconstruction 요청 및 LoRa event message가 `job_id` 또는 `seq` 기준으로 일관성을 유지하는지 확인하는 시험이 포함되어야 한다 (`CFS-LNK-10`, `CFS-LNK-11`).
- **CFS-VER-10**: 검증 계획에는 차량 생성 `timestamp` field가 relay 또는 bridge component에 의해 덮어써지지 않는지 확인하는 시험이 포함되어야 한다 (`CFS-LNK-09`).
- **CFS-VER-11**: 검증 계획에는 `job_id`가 reconstruction request부터 result, 관련 LoRa event message까지 변경 없이 유지되는지 확인하는 시험이 포함되어야 한다 (`CFS-LNK-12`).
- **CFS-VER-12**: 검증 계획에는 `mavlink_bridge_app`이 각 baseline MAVLink message type을 올바른 cFS SB message로 파싱 및 publish하는지 확인하는 시험이 포함되어야 한다 (`CFS-SB-13`부터 `CFS-SB-16`).
- **CFS-VER-13**: 검증 계획에는 `mavlink_bridge_app`이 raw MAVLink byte frame을 SB에 올리지 않는지 확인하는 시험이 포함되어야 한다 (`CFS-APP-10`).
- **CFS-VER-14**: 검증 계획에는 `MAVLINK_BRIDGE_STATUS_MID`가 MAVLink message age에 따라 `ALIVE`, `DEGRADED`, `LOST` 상태로 전이하는지 확인하는 시험이 포함되어야 한다 (`CFS-SB-17`).
- **CFS-VER-15**: 검증 계획에는 `mavlink_bridge_app`을 비활성화해도 다른 센서 또는 alignment 모듈의 nominal operation이 방해받지 않는지 확인하는 시험이 포함되어야 한다 (`CFS-SB-18`).

## 9. 미정 항목

- `OI-CFS-01`: 정확한 cFS message ID와 source ID를 할당해야 한다.
- `OI-CFS-03`: 모듈별 runtime-changeable configuration field를 확정해야 한다.
- `OI-CFS-07`: `mavlink_bridge_app` 출력(`0x1905`–`0x190A`)의 최종 MID 할당을 확정해야 한다 (Section 3.2AB, `CFS-SB-13`부터 `CFS-SB-17`).
- `OI-CFS-08`: `mavlink_bridge_app`의 baseline MAVLink message set을 확정해야 한다. 특히 `ODOMETRY (#331)`와 `EKF_STATUS_REPORT (#193)`를 baseline deployment에서 필수로 볼지 선택 사항으로 둘지 결정해야 한다.
- `OI-CFS-09`: `mavlink_bridge_app`의 error recovery policy를 정의해야 한다. serial reconnect interval, 최대 retry 횟수, startup 시 FC unreachable 상태에서의 동작을 포함한다.
