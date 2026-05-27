# cfs_core_app Behavior Specification

## 1. Purpose

This document defines the current implemented behavior of `cfs_core_app` in this repository.
It is a code-aligned specification for review, integration, and test execution.

This document does not define target behavior that is not present in code.
If the code and this document disagree, the code must be treated as the source to investigate.

## 2. Scope

This specification covers:

- `cfs_core_app` message subscriptions
- Internal cached state updated by each subscribed message
- `SYSTEM_HEALTH_MID` output fields and publish timing
- Health-state classification logic
- Timeout handling for bridge, GPS, EKF, local-state, and attitude-state inputs
- Behavior after startup and after input loss
- Existing unit-test coverage and recommended runtime verification

This specification does not cover:

- `mavlink_bridge_app` internal parsing rules
- `downlink_app` packet formatting
- Any active restart of other apps or devices
- Any fault-handling behavior not present in `cfs_core_app`

## 3. References

- Source: [cfs_core_app.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app.c)
- Source: [cfs_core_app_dispatch.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app_dispatch.c)
- Source: [cfs_core_app_utils.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app_utils.c)
- Source: [cfs_core_app.h](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app.h)
- Source: [cfs_core_app_utils.h](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app_utils.h)
- Config: [default_cfs_core_app_internal_cfg_values.h](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/config/default_cfs_core_app_internal_cfg_values.h)
- Config: [default_cfs_core_app_topicid_values.h](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/config/default_cfs_core_app_topicid_values.h)
- Config: [default_cfs_core_app_msgid_values.h](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/config/default_cfs_core_app_msgid_values.h)
- Config: [default_cfs_core_app_msgdefs.h](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/config/default_cfs_core_app_msgdefs.h)
- Tests: [coveragetest_cfs_core_app_utils.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/unit-test/coveragetest/coveragetest_cfs_core_app_utils.c)

## 4. Responsibilities

`cfs_core_app` has the following implemented responsibilities:

- Subscribe to bridge housekeeping, FC state messages, route updates, and app command/HK messages.
- Cache the latest received bridge, attitude, local, GPS, EKF, mission-route, and landing-route data.
- Recompute system health whenever a subscribed state message is received.
- Recompute system health periodically when no input arrives.
- Publish `SYSTEM_HEALTH_MID`.
- Publish housekeeping telemetry on HK request.

`cfs_core_app` does not currently:

- Restart another app
- Reopen a serial device
- Reset an external component
- Publish a separate recovery command
- Persist health state across restart

## 5. Definitions

The following terms are used consistently in this document.

- `state cache`: The latest in-memory copy of a subscribed FC state message.
- `bridge cache`: The latest in-memory copy of subscribed bridge housekeeping summary fields.
- `received`: Internal boolean indicating that at least one message of that type has been processed since app initialization.
- `expired`: Internal condition where `NowMs - TimestampMs > TimeoutMs`, or where no message has ever been received for that cache.
- `unavailable`: Input condition used by health logic to mark an input as unsuitable for nominal operation.
- `force publish`: A direct health publish request triggered after processing a subscribed state message.
- `periodic publish`: A health publish attempt triggered from the app main-loop timeout path.

## 6. Interfaces

### 6.1 Subscribed messages

`cfs_core_app` subscribes to the following message IDs during initialization.

| Symbol | Value | Purpose |
| --- | --- | --- |
| `CFS_CORE_APP_CMD_MID_VALUE` | `0x18C0` | Command input |
| `CFS_CORE_APP_SEND_HK_MID_VALUE` | `0x18C1` | HK request |
| `CFS_CORE_APP_BRIDGE_HK_MID_VALUE` | `0x08A0` | Bridge HK mirror input |
| `CFS_CORE_APP_FC_EKF_LOCAL_STATE_MID_VALUE` | `0x1905` | FC local-state input |
| `CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE` | `0x1906` | FC attitude-state input |
| `CFS_CORE_APP_FC_GPS_RAW_STATE_MID_VALUE` | `0x1907` | FC GPS-state input |
| `CFS_CORE_APP_FC_EKF_STATUS_MID_VALUE` | `0x1908` | FC EKF-status input |
| `ROUTE_UPDATE_MID` | `0x190B` | Route update input |

### 6.2 Published messages

| Symbol | Value | Purpose |
| --- | --- | --- |
| `CFS_CORE_APP_HK_TLM_MID` | `0x08C0` | Housekeeping telemetry |
| `SYSTEM_HEALTH_MID` | `0x1904` | System health telemetry |

## 7. Input Payloads

### 7.1 Generic FC state input

The following subscribed inputs are processed as `CFS_CORE_APP_GenericStateTlm_t`:

- attitude state
- local state
- GPS state
- EKF state

The payload fields consumed by `cfs_core_app` are:

| Field | Type | Used for |
| --- | --- | --- |
| `TimestampMs` | `uint32` | Expiration and last-valid-input selection |
| `Seq` | `uint32` | Cached for traceability only |
| `Valid` | `uint8` | Input availability decision |
| `Stale` | `uint8` | Input availability decision |
| `ErrorCode` | `uint8` | Cached for traceability only |

No sequence monotonicity check is implemented.
No timestamp source validation is implemented.
No payload length validation is performed for these telemetry inputs.

### 7.2 Bridge HK input

`CFS_CORE_APP_BridgeHkMirror_t` fields consumed by `cfs_core_app` are:

| Field | Type | Used for |
| --- | --- | --- |
| `LinkState` | `uint8` | Cached only |
| `LastErrorCode` | `uint8` | Cached only |
| `LastRxTimestampMs` | `uint32` | Bridge-timeout decision |

The bridge command/error counters and byte counters are not used in health classification.

### 7.3 Route update input

`ROUTE_UPDATE_MID` is processed as `CFS_CORE_APP_RouteUpdateTlm_t`.

The consumed fields are:

| Field | Type | Used for |
| --- | --- | --- |
| `TimestampMs` | `uint32` | Route cache timestamp |
| `SourceSequence` | `uint32` | Cached for traceability |
| `RouteType` | `uint8` | Mission-route or landing-route selection |
| `RouteVersion` | `uint8` | Cached |
| `WaypointCount` | `uint8` | Cached and reported in HK |
| `Waypoints[]` | array | Cached |

Only the following route types update a cache:

- `CFS_CORE_APP_ROUTE_SEGMENT_MISSION_EXTENSION`
- `CFS_CORE_APP_ROUTE_SEGMENT_LANDING`

Any other route type produces no route-cache update.
Route updates do not affect health-state classification.

## 8. Internal State

### 8.1 Cached FC states

Each FC state cache stores:

- `TimestampMs`
- `Seq`
- `Valid`
- `Stale`
- `ErrorCode`
- `Received`

The app maintains one cache for each of:

- attitude state
- local state
- GPS state
- EKF state

### 8.2 Cached bridge state

The bridge cache stores:

- `LinkState`
- `LastErrorCode`
- `LastRxTimestampMs`
- `Received`

### 8.3 Cached route state

The app maintains:

- one mission-route cache
- one landing-route cache

Each route cache stores:

- `TimestampMs`
- `SourceSequence`
- `UpdateCount`
- `RouteType`
- `RouteVersion`
- `WaypointCount`
- `Valid`
- `Waypoints[]`

## 9. Timing Configuration

The following timing constants are implemented.

| Constant | Value | Meaning |
| --- | --- | --- |
| `CFS_CORE_APP_SB_POLL_TIMEOUT_MS` | `200` | Main-loop SB receive timeout |
| `CFS_CORE_APP_PROTOTYPE_PERIOD_MS` | `1000` | Minimum interval for periodic health publish |
| `CFS_CORE_APP_ATTITUDE_TIMEOUT_MS` | `2000` | Attitude-state expiration threshold |
| `CFS_CORE_APP_LOCAL_TIMEOUT_MS` | `2000` | Local-state expiration threshold |
| `CFS_CORE_APP_GPS_TIMEOUT_MS` | `3000` | GPS-state expiration threshold |
| `CFS_CORE_APP_EKF_TIMEOUT_MS` | `2000` | EKF-state expiration threshold |
| `CFS_CORE_APP_BRIDGE_TIMEOUT_MS` | `3000` | Bridge timeout threshold |

## 10. Health Output Contract

`SYSTEM_HEALTH_MID` publishes the following fields:

| Field | Meaning |
| --- | --- |
| `Seq` | Monotonic app-local publish counter incremented for every health publication |
| `TimestampMs` | Current cFE time in milliseconds at publish time |
| `LastValidInputTimestampMs` | Maximum timestamp among received attitude/local/GPS/EKF caches, or `NowMs` if none have been received |
| `HealthState` | `NOMINAL`, `DEGRADED`, or `RECOVERY` in current implementation |
| `FaultCode` | `NONE`, `BRIDGE_TIMEOUT`, `GPS_STALE`, `EKF_INVALID`, `LOCAL_TIMEOUT`, or `ATTITUDE_TIMEOUT` |
| `RecoveryRequested` | `1` only for bridge-timeout condition, otherwise `0` |

The current implementation zero-initializes the telemetry structure before each publish.
The current implementation reinitializes the message header before each publish.

## 11. Publish Conditions

### 11.1 Immediate health publish

`cfs_core_app` publishes `SYSTEM_HEALTH_MID` immediately after processing any of the following:

- bridge HK
- attitude state
- local state
- GPS state
- EKF state
- route update

### 11.2 Periodic health publish

If no SB message is received for `200 ms`, the app enters the timeout path.
On that path, the app attempts a health update.

The health update is published only when:

`NowMs - LastPublishTimeMs >= 1000`

### 11.3 Housekeeping publish

Housekeeping is published only when `CFS_CORE_APP_SEND_HK_MID_VALUE` is received.

## 12. Health Classification Rules

Health classification is evaluated in strict priority order.
Only the highest-priority matching condition determines the output.

### 12.1 Priority 1: Bridge timeout

Condition:

- bridge cache has never been received
- or `NowMs - BridgeState.LastRxTimestampMs > 3000`

Output:

- `HealthState = CFS_CORE_APP_HEALTH_RECOVERY`
- `FaultCode = CFS_CORE_APP_FAULT_BRIDGE_TIMEOUT`
- `RecoveryRequested = 1`

### 12.2 Priority 2: EKF invalid or stale

Condition:

- EKF state expired (`NowMs - EkfState.TimestampMs > 2000`, or never received)
- or EKF `Valid == 0`
- or EKF `Stale != 0`

Output:

- `HealthState = CFS_CORE_APP_HEALTH_DEGRADED`
- `FaultCode = CFS_CORE_APP_FAULT_EKF_INVALID`
- `RecoveryRequested = 0`

### 12.3 Priority 3: Local state timeout

Condition:

- local state expired (`NowMs - LocalState.TimestampMs > 2000`, or never received)

Output:

- `HealthState = CFS_CORE_APP_HEALTH_DEGRADED`
- `FaultCode = CFS_CORE_APP_FAULT_LOCAL_TIMEOUT`
- `RecoveryRequested = 0`

### 12.4 Priority 4: Attitude state timeout

Condition:

- attitude state expired (`NowMs - AttitudeState.TimestampMs > 2000`, or never received)

Output:

- `HealthState = CFS_CORE_APP_HEALTH_DEGRADED`
- `FaultCode = CFS_CORE_APP_FAULT_ATTITUDE_TIMEOUT`
- `RecoveryRequested = 0`

### 12.5 Priority 5: GPS unavailability

Condition:

- GPS state expired
- or GPS `Valid == 0`
- or GPS `Stale != 0`

Output:

- `HealthState = CFS_CORE_APP_HEALTH_DEGRADED`
- `FaultCode = CFS_CORE_APP_FAULT_GPS_STALE`
- `RecoveryRequested = 0`

### 12.6 Priority 6: Nominal

Condition:

- none of the previous conditions are true

Output:

- `HealthState = CFS_CORE_APP_HEALTH_NOMINAL`
- `FaultCode = CFS_CORE_APP_FAULT_NONE`
- `RecoveryRequested = 0`

### 12.7 Unused enum state

`CFS_CORE_APP_HEALTH_FAILED` is defined in message definitions but is not produced by current code.

## 13. Detailed Timeout and Fault Handling

### 13.1 Bridge timeout

`bridge timeout` is evaluated from bridge HK `LastRxTimestampMs`, not from the arrival time of any FC state message.

Effects:

- produces `RECOVERY`
- produces `FAULT_BRIDGE_TIMEOUT`
- sets `RecoveryRequested = 1`
- suppresses GPS and EKF fault reporting because bridge timeout has higher priority

### 13.2 GPS stale

`gps stale` covers all of the following:

- no GPS message has been received
- GPS timestamp age exceeds `3000 ms`
- GPS `Valid == 0`
- GPS `Stale != 0`

Effects:

- produces `DEGRADED`
- produces `FAULT_GPS_STALE`
- does not set `RecoveryRequested`

### 13.3 EKF invalid

`ekf invalid` covers all of the following:

- no EKF message has been received
- EKF timestamp age exceeds `2000 ms`
- EKF `Valid == 0`
- EKF `Stale != 0`

Effects:

- produces `DEGRADED`
- produces `FAULT_EKF_INVALID`
- does not set `RecoveryRequested`

### 13.4 Local timeout

`local timeout` is represented by a dedicated fault code `FAULT_LOCAL_TIMEOUT`.

`local timeout` covers:

- no local-state message has been received
- local-state timestamp age exceeds `2000 ms`

Effects:

- produces `DEGRADED`
- produces `FAULT_LOCAL_TIMEOUT`
- does not set `RecoveryRequested`

Note: local `Valid == 0` or `Stale != 0` is evaluated under Priority 2 (EKF-related) alongside EKF state validity.
Local timeout (expired timestamp) is a distinct condition evaluated at Priority 3 with its own fault code.

### 13.5 Attitude timeout

`attitude timeout` is represented by a dedicated fault code `FAULT_ATTITUDE_TIMEOUT`.

`attitude timeout` covers:

- no attitude-state message has been received
- attitude-state timestamp age exceeds `2000 ms`

Effects:

- produces `DEGRADED`
- produces `FAULT_ATTITUDE_TIMEOUT`
- does not set `RecoveryRequested`

Note: attitude `Valid == 0` or `Stale != 0` is evaluated under Priority 2 (EKF-related).
Attitude timeout (expired timestamp) is a distinct condition evaluated at Priority 4 with its own fault code.

## 14. Startup, Input Loss, and Recovery Behavior

### 14.1 Startup

At initialization, the entire app data structure is zeroed.
All caches begin with `Received = false`.

As a result:

- before the first bridge HK is processed, the bridge condition evaluates as timed out
- before the first health-relevant FC states are processed, those caches can also evaluate as expired

Because bridge timeout has highest priority, the app can publish `RECOVERY` during startup until valid bridge HK is received.

### 14.2 Input loss

If inputs stop arriving:

- the app main loop continues running
- every `200 ms` SB timeout triggers the periodic service path
- health is republished at most once per `1000 ms`
- expired caches eventually cause `DEGRADED` or `RECOVERY`

### 14.3 Input restoration

There is no separate recovery state machine.

A 10-second stabilization timer (`CFS_CORE_APP_NOMINAL_STABILITY_MS = 10000`) is required before transitioning to `NOMINAL` from a non-NOMINAL state.

When all fault conditions clear, the app enters a stabilization window:
- during the window, `HealthState = DEGRADED`, `FaultCode = NONE`
- after `10 s` of continuous fault-free operation, the app transitions to `NOMINAL`

If a fault recurs during the stabilization window, the timer resets.

If the app was already in `NOMINAL` before the non-NOMINAL episode, the timer is reset to `0` on re-entry into non-NOMINAL and restarts on the next fault-free cycle.

### 14.4 Active recovery actions

The only implemented recovery action is:

- set `RecoveryRequested = 1` in `SYSTEM_HEALTH_MID` on bridge timeout

No additional recovery side effect is implemented.

## 15. Housekeeping Behavior

On HK request, the app reports:

- command counter
- command error counter
- mission route waypoint count
- landing route waypoint count
- publish count
- last publish timestamp
- last route update timestamp
- total route update count

The app also emits an EVS informational event summarizing route-related HK fields.

## 16. Route Handling Rules

Mission and landing routes are cached independently.
Each accepted route update increments the selected route cache `UpdateCount`.

Route updates affect:

- route caches
- HK counters
- route update EVS event emission
- immediate health republish timing

Route updates do not affect:

- health-state classification
- fault-code selection
- `RecoveryRequested`

## 17. Command Handling

The app currently supports only:

- NOOP
- reset counters

Telemetry inputs are not validated with command-length checks.
Unknown command codes increment the command-error counter.

## 18. Existing Unit-Test Coverage

The current unit tests verify:

- housekeeping function executes
- command-length verification success and failure
- nominal health classification
- bridge-timeout health classification
- GPS stale health classification
- EKF invalid health classification
- local timeout classification as `FAULT_EKF_INVALID`
- attitude timeout classification as `FAULT_EKF_INVALID`
- mission-route cache update
- landing-route cache update
- bridge HK cache update
- service prototype execution path
- initialization success
- initialization failure on subscribe error
- NOOP command
- reset-counters command

## 19. Recommended Verification Procedure

### 19.1 Static verification

Review the following source locations before runtime test:

- initialization and subscriptions in [cfs_core_app.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app.c:52)
- message routing in [cfs_core_app_dispatch.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app_dispatch.c:21)
- cache updates in [cfs_core_app_utils.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app_utils.c:91)
- health classification in [cfs_core_app_utils.c](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/cfs_core_app/fsw/src/cfs_core_app_utils.c:154)

### 19.2 Runtime verification matrix

| ID | Scenario | Stimulus | Expected output |
| --- | --- | --- | --- |
| CORE-RUN-001 | Nominal | bridge HK and all FC states arrive fresh with `Valid=1`, `Stale=0` | `SYSTEM_HEALTH_MID` reports `NOMINAL`, `FAULT_NONE`, `RecoveryRequested=0` |
| CORE-RUN-002 | Bridge timeout | stop bridge HK updates for more than `3000 ms` | `SYSTEM_HEALTH_MID` reports `RECOVERY`, `FAULT_BRIDGE_TIMEOUT`, `RecoveryRequested=1` |
| CORE-RUN-003 | GPS stale flag | deliver fresh bridge, attitude, local, EKF; set GPS `Stale=1` | `SYSTEM_HEALTH_MID` reports `DEGRADED`, `FAULT_GPS_STALE`, `RecoveryRequested=0` |
| CORE-RUN-004 | GPS timeout | stop GPS updates for more than `3000 ms` while bridge and EKF-related inputs stay fresh | `SYSTEM_HEALTH_MID` reports `DEGRADED`, `FAULT_GPS_STALE` |
| CORE-RUN-005 | EKF invalid flag | set EKF `Valid=0` with other inputs fresh | `SYSTEM_HEALTH_MID` reports `DEGRADED`, `FAULT_EKF_INVALID` |
| CORE-RUN-006 | Local timeout | stop local-state updates for more than `2000 ms` while bridge, attitude, GPS, EKF stay fresh | `SYSTEM_HEALTH_MID` reports `DEGRADED`, `FAULT_LOCAL_TIMEOUT` |
| CORE-RUN-007 | Attitude timeout | stop attitude-state updates for more than `2000 ms` while bridge, local, GPS, EKF stay fresh | `SYSTEM_HEALTH_MID` reports `DEGRADED`, `FAULT_ATTITUDE_TIMEOUT` |
| CORE-RUN-008 | Priority check | force simultaneous bridge timeout and GPS stale | `SYSTEM_HEALTH_MID` reports `RECOVERY`, `FAULT_BRIDGE_TIMEOUT` |
| CORE-RUN-009 | Recovery to nominal | after CORE-RUN-002 or CORE-RUN-003, resume fresh valid inputs | next health evaluation returns to `NOMINAL` |
| CORE-RUN-010 | Startup warm-up | start app before first bridge HK | first health outputs may report `RECOVERY` until bridge HK arrives |

### 19.3 Log and telemetry observation points

Observe:

- `SYSTEM_HEALTH_MID` fields in the consumer path or telemetry display
- HK fields in `CFS_CORE_APP_HK_TLM_MID`
- EVS event `CFS_CORE_APP Initialized`
- EVS route update log
- EVS housekeeping log

Do not rely on EVS alone for health-state transitions.
Current health-state transitions are published as telemetry and are not emitted as dedicated EVS transition events.

## 20. Known Gaps

The following behaviors are not implemented and must not be assumed during test or operations:

- `FAILED` health output state
- sequence-gap or duplicate detection
- timestamp-base validation
- active restart of bridge or peer apps
- persistence of last health state across app restart

The following were previously listed as gaps and are now implemented:

- dedicated fault code for local timeout → `FAULT_LOCAL_TIMEOUT = 4` (implemented in A2)
- dedicated fault code for attitude timeout → `FAULT_ATTITUDE_TIMEOUT = 5` (implemented in A2)
- debounce or dwell-time logic during recovery → 10 s `NOMINAL_STABILITY_MS` stabilization timer (implemented in A4)

## 21. System-Level Unimplemented Areas

The following items were identified during Raspberry Pi runtime verification and are not fully implemented as an operational end-to-end capability.

### 21.1 LoRa uplink transport path

**[2026-05-27 실물 검증 완료]**

`PC LoRa (COM7) → 무선 → Pi /dev/ttyUSB0 → lora_uplink_bridge.py → uplink_app UDP:1234 → cfs_core_app` 경로가 실물에서 end-to-end 확인되었다.

검증 시 발견 및 수정 사항:

- `lora_uplink_bridge.py`의 `MAX_PAYLOAD_LENGTH`가 `192`로 설정되어 있어 `uplink_app`이 기대하는 `212` bytes 대신 `208` bytes 패킷을 생성했다.
- `uplink_route_update_sender.py`의 `UPLINK_APP_MAX_PAYLOAD_LENGTH = 196`에 맞춰 `MAX_PAYLOAD_LENGTH`를 `196`으로 수정했다 (commit 7a63a98).

확인된 정상 동작:

- `lora_uplink_bridge.py --serial-path /dev/ttyUSB0 --allow-seq-regression` 실행 후 `serial open: /dev/ttyUSB0` 출력
- PC에서 `route-good` 전송 시 Pi에서 `forwarded uplink frame: class=2 seq=1 flags=0 payload_len=28` 출력
- cFS 로그에서 `UPLINK_APP: routed uplink class=2 seq=1 target=1 payload_len=28` 및 `CFS_CORE_APP: route updated type=1 version=1 count=2 src_seq=1` 확인

운용 시 주의사항:

- `--allow-seq-regression` 없이 운용 시 동일 sequence 번호 반복 전송은 거부된다. 실운용에서는 매 전송마다 sequence를 증가시켜야 한다.
- Pi의 LoRa 모듈은 `/dev/ttyUSB0` (CP2102 USB-UART), FC UART는 `/dev/ttyAMA0`으로 분리 확인되었다.

### 21.2 Runtime configuration application path

`UPLINK_APP_CLASS_CONFIG` is a recognized command class, but no confirmed implementation was identified that applies configuration payloads to mission-app runtime parameters such as publish period, timeout values, or equivalent output-rate controls.

Current state:

- config-class acceptance exists at the command-validation level
- no confirmed end-to-end implementation was identified that decodes a config payload and updates active settings in `cfs_core_app`, `telemetry_app`, or another mission app

Implication:

- route-update testing is currently supported
- output-period or timeout-change testing is not currently supported as an implemented operational feature

### 21.3 LoRa downlink robustness

LoRa downlink output from `mavlink_bridge_app` is not yet robust under current runtime conditions.

Observed runtime symptom:

- repeated `LoRa write failed errno=11, forcing reopen`

Relevant implementation behavior:

- the LoRa port is opened with `O_NONBLOCK`
- a single `write()` failure triggers immediate close and reopen
- transient backpressure is not distinguished from a persistent link fault

Implication:

- LoRa downlink path exists and attempts transmission
- LoRa downlink path is not yet stable enough to be treated as fully operational

### 21.4 Health-state observability on Raspberry Pi logs

`cfs_core_app` health-state transitions are published through `SYSTEM_HEALTH_MID`, but are not emitted as dedicated EVS transition logs.

Current state:

- Raspberry Pi runtime logs show input activity and startup events
- Raspberry Pi runtime logs do not directly show `NOMINAL`, `DEGRADED`, or `RECOVERY` transitions from `cfs_core_app`

Implication:

- health-state verification currently depends on telemetry consumers such as Open MCT or another subscriber to `SYSTEM_HEALTH_MID`
- direct operator confirmation from EVS logs alone is not currently sufficient

### 21.5 Fault-detail granularity

Some fault conditions are intentionally or unintentionally collapsed into shared outputs.

Current state:

- `local timeout`
- `attitude timeout`
- `ekf invalid`

all map to:

- `HealthState = DEGRADED`
- `FaultCode = EKF_INVALID`

Implication:

- current output is sufficient to detect degraded estimator-related health
- current output is not sufficient to distinguish the exact degraded source without additional telemetry correlation
