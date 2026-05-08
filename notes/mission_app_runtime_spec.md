# Mission App Runtime Specification Draft

## 1. Purpose

This document defines the first-pass mission application runtime specification for
the cFS-based system. It is intended to guide implementation after the minimum
MAVLink bridge and telemetry downlink bring-up.

The main goals are:

- Define application responsibilities and Software Bus message contracts.
- Define which runtime values may be changed during mission operation.
- Define how hardware faults, app faults, restart, and reboot behavior are handled.
- Define persistent state that must survive abnormal shutdown and reboot.
- Define active/pending double-buffering rules for critical runtime changes.

This document is a draft. It captures the current intended architecture and must
be reconciled with implementation as each app is added.

## 2. Scope and Current State

The current repository contains a minimum runnable cFS application baseline
derived from the cFS sample app pattern. It currently demonstrates:

- cFS app load and startup.
- Software Bus pipe creation and subscription.
- Housekeeping and command handling.
- Basic message routing and state publication patterns needed by mission apps.
- Recovery-oriented state handling patterns such as `NOMINAL`, `DEGRADED`,
  `LOST`, and restart or recovery transitions.

The broader mission app set described below is not yet fully implemented in this
repository. This spec is the target contract for subsequent implementation.

## 3. Platform Boundary Model

The mission sensors and flight-control hardware are physically connected to the
MicoAir H743 V2 flight controller board. The Raspberry Pi acts as the cFS host
and communication bridge between the flight controller and the ground segment.

Baseline platform responsibility:

| Component | Responsibility |
| --- | --- |
| Flight controller board | Owns direct sensor interfaces, flight-control loop, actuator control, and flight-critical state |
| Flight controller firmware | Owns sensor drivers, vehicle stabilization, navigation estimates exposed by the FC, and FC-level health |
| Raspberry Pi | Hosts cFS apps, receives FC data, packages data into mission MIDs, forwards telemetry, and manages communication-path recovery |
| cFS apps | Validate received data, publish Software Bus state, monitor link/app health, report faults, and make non-flight-control recovery decisions |
| Ground segment | Receives telemetry, sends authorized commands, and performs operator-level monitoring |

cFS apps running on the Raspberry Pi shall not assume direct electrical control
over sensors connected to the flight controller. Sensor reset, sensor
power-cycle, flight-control reset, actuator control, arming/disarming, and flight
controller reboot are outside the baseline cFS recovery authority.

The term "sensor recovery" in this document means logical or communication-path
recovery unless the hardware design later exposes an explicitly approved,
software-controllable reset or power interface that does not affect flight
stability.

Baseline allowed cFS-side recovery actions:

- Reopen or reinitialize the Raspberry Pi communication endpoint.
- Resubscribe, restart, or re-request FC telemetry streams.
- Mark a data source invalid, degraded, lost, or failed.
- Publish fault and recovery status to telemetry.
- Continue degraded operation using only the remaining valid onboard data sources.

Baseline prohibited cFS-side recovery actions:

- Flight controller reboot.
- Flight-control firmware reset.
- Motor or actuator command.
- Arming or disarming command.
- Sensor power-cycle through the FC.
- Flight mode or flight-control parameter changes that may affect stability.

Any exception to this baseline shall require a separate mission safety policy and
explicit command authorization.

The hardware connection topology for `img_app` and any future mission payload
device shall be documented before enabling device-specific recovery. Until the
topology is defined, recovery for those apps shall be limited to data validity
handling, local parser or transport recovery, telemetry reporting, and degraded
operation.

UWB ranging hardware is not part of the current mission scope. `uwb_app` and
`UWB_POS_MID` are removed from the baseline app set. If UWB is added in a
future mission revision, a separate topology and recovery policy shall be defined
at that time.

## 4. Application Responsibility Model

Each application shall own one clear responsibility and publish its state through
defined MID contracts. Apps shall not directly overwrite another app's owned
state.

| App | Responsibility | Publish MID | Subscribe MID |
| --- | --- | --- | --- |
| `mavlink_bridge_app` | Receive FC-provided MAVLink telemetry, extract mission state fields, and publish mission state MIDs | `IMU_STATE_MID`, `GPS_STATE_MID`, `EKF_STATE_MID`, `BRIDGE_STATUS_MID` | Approved FC transport input |
| `img_app` | Capture image and publish image metadata | `IMAGE_META_MID` | TBD |
| `cfs_core_app` | Validate received mission state, manage health and recovery policy, and publish system health | `SYSTEM_HEALTH_MID` | `IMU_STATE_MID`, `GPS_STATE_MID`, `EKF_STATE_MID`, `BRIDGE_STATUS_MID`, app health/status MIDs |
| `downlink_app` | Collect approved mission state and telemetry from the Software Bus and transmit downlink packets to the ground segment | `DOWNLINK_STATUS_MID` | `IMU_STATE_MID`, `GPS_STATE_MID`, `EKF_STATE_MID`, `SYSTEM_HEALTH_MID`, approved telemetry MIDs |
| `uplink_app` | Receive ground commands, validate uplink packets, and route authorized runtime configuration or recovery commands to mission apps | `UPLINK_STATUS_MID` | `UPLINK_CMD_MID` or approved transport input |

## 5. MID Contract Rules

Every mission MID shall have a documented owner, producer, consumer list,
payload layout, publication rate, validity rules, and fault behavior.

Each state or telemetry payload should include, unless explicitly exempted:

- `Timestamp`: Measurement or generation time.
- `TimeValid`: Whether the timestamp is trusted.
- `SequenceCounter`: Monotonic counter for stale, duplicate, and dropped data detection.
- `Valid`: Whether the payload may be used by consumers.
- `HealthState`: `NOMINAL`, `DEGRADED`, `LOST`, or `FAILED`.
- `FaultCode`: Last or current fault reason.
- `SourceId`: Sensor, transport, or app instance identifier.
- `AgeMs`: Age of last known good value, where applicable.

The timestamp basis shall be defined before state correlation and health-policy
implementation. The candidate bases are cFS mission elapsed time, GPS time, Unix
time, or a monotonic platform clock. `cfs_core_app` shall not combine or compare
inputs whose time basis or time validity is unknown.

### 5.1 MID Contract Table Template

Each mission MID shall be specified using the following contract fields before
the producing app is implemented.

| Field | Meaning |
| --- | --- |
| MID Name | Symbolic message ID |
| Owner App | App responsible for defining the payload |
| Producer | App or component that publishes the MID |
| Consumers | Apps that subscribe to the MID |
| Command MID | MID used to send commands to the owning app, if applicable |
| Publication Rate | Periodic rate or event-driven condition |
| Payload Layout | Struct definition or field list reference |
| Validity Rule | Required condition for `Valid=true` |
| Fault Behavior | Behavior when producer is degraded, lost, or failed |
| Time Basis | Timestamp source and unit |
| Sequence Rule | Counter increment and wrap behavior |

The complete per-MID contract table shall be populated before implementing each
app. Until finalized, the MID contract fields are listed as TBD in Section 3 and
tracked in Section 16.

### 5.2 Time Basis Validity Policy

A payload time basis shall be considered known only when the producer declares
the timestamp source, timestamp unit, and `TimeValid=true`.

`cfs_core_app` shall reject or downgrade any individual input when:

- `TimeValid=false` for that input.
- Timestamp source is unknown.
- Timestamp unit is unknown.
- Timestamp age exceeds the configured validity window.
- Sequence counter indicates stale, duplicate, or out-of-order data beyond the
  configured tolerance.
- Required inputs use incompatible time bases without a defined conversion rule.

Rejecting an input with `TimeValid=false` does not automatically invalidate the
overall system output. Published state may remain usable if the remaining inputs
satisfy the active mission policy. If the remaining inputs are insufficient for
the current mode, `cfs_core_app` shall publish degraded or invalid system state.

A monotonic platform clock may be used for relative timing if the mission only
requires local ordering and age checks. GPS time or another absolute time basis
shall be required only when absolute timing is needed by the mission policy.

The final time basis selection and validity rules shall be defined before
`cfs_core_app` validation policy is finalized and are tracked in Section 16.

## 6. Minimum Payload Candidates

### 6.1 `IMU_STATE_MID`

Minimum fields:

- Timestamp and time validity.
- Sequence counter.
- Valid flag and health state.
- Acceleration X/Y/Z.
- Gyro X/Y/Z.
- Optional attitude estimate: quaternion or roll/pitch/yaw.
- Sensor quality or covariance indicator.
- Fault code.

### 6.2 `GPS_STATE_MID`

Minimum fields:

- Timestamp and time validity.
- Sequence counter.
- Valid flag and health state.
- Latitude, longitude, altitude.
- Fix type.
- Satellite count.
- Accuracy or HDOP/VDOP.
- Fault code.

### 6.3 `EKF_STATE_MID`

Minimum fields:

- Timestamp and time validity.
- Sequence counter.
- Valid flag and health state.
- Local position X/Y/Z, where available from the FC.
- Local velocity X/Y/Z, where available from the FC.
- EKF status flags or equivalent FC estimator state.
- Fault code.

### 6.4 `BRIDGE_STATUS_MID`

Minimum fields:

- Timestamp and time validity.
- Sequence counter.
- Valid flag and health state.
- Active transport ID.
- Last valid update age.
- Parser error count.
- Stream timeout count.
- Recovery count.
- Fault code.

The meaning of `BRIDGE_STATUS_MID` shall be kept narrow: it represents FC-to-Pi
bridge and communication health, not the full system health state.

### 6.5 `SYSTEM_HEALTH_MID`

Minimum fields:

- Timestamp and time validity.
- Sequence counter.
- Valid flag and health state.
- Active cFS state.
- Per-input summary: IMU, GPS, EKF, bridge, and downlink health.
- Recovery mode or escalation stage.
- Last recovery action.
- Fault code.

### 6.6 `DOWNLINK_STATUS_MID`

Minimum fields:

- Timestamp and time validity.
- Sequence counter.
- Valid flag and health state.
- Active transport ID.
- Last transmit time.
- Transmit count.
- Transmit error count.
- Last transmit fault code.
- Fault code.

### 6.7 `IMAGE_META_MID`

Minimum fields:

- Timestamp and time validity.
- Sequence counter.
- Valid flag and health state.
- Image ID.
- Capture result code.
- Storage or transfer reference.
- Fault code.

## 7. Runtime-Changeable Parameters

Runtime changes shall be divided into three mechanisms:

| Mechanism | Purpose | Examples |
| --- | --- | --- |
| Command buffer | Immediate or one-shot action | enable/disable, reset request, diagnostic capture |
| Table/config buffer | Validated runtime configuration | threshold, timeout, calibration, retry limit |
| Persistent storage | State/config that survives reboot | boot counters, selected config version, map checkpoint |

Apps shall not apply unvalidated runtime parameters directly from incoming
buffers. Each app shall validate range, version, checksum or CRC, and current
mode compatibility before activation.

Each app shall define a command MID for receiving ground or system commands.
Command routing shall be defined before command authorization is finalized.
Until the command MID per app is assigned, command routing is tracked as an open
item in Section 16.

Runtime-changeable examples:

- Sensor enable/disable.
- Sensor timeout thresholds.
- Quality gates.
- Recovery retry limits.
- Telemetry link timeout thresholds.
- Diagnostic capture settings.
- Calibration values, if mission-approved.

Runtime-restricted or boot-time-only examples:

- App task priority.
- Stack size.
- MID assignments.
- Pipe depth.
- Startup order.
- Critical app classification.

Changing a restricted value shall require app restart, Raspberry Pi/cFS host
reset, reboot, or mission rebuild according to the final operations policy.
Flight-controller-affecting changes are outside the baseline cFS authority.

## 8. App Period and Priority Policy

App period and app priority shall be specified separately.

App period may be controlled by:

- SCH wakeup messages.
- App-local timeout or polling loop.
- Event-driven input messages.

If an app is driven by SCH, period changes shall be managed by SCH table or
schedule policy, not by changing only the app-local state. If an app is driven by
an internal loop, period changes may be managed by validated app configuration.

App priority shall be treated as a restricted operational parameter. Runtime
priority changes are not part of the baseline policy unless explicitly approved
by a later mission safety review.

## 9. Fault Classification

Faults shall be classified before recovery action is selected.

| Fault Class | Meaning | Example |
| --- | --- | --- |
| `APP_FAULT` | App logic or runtime failure | pipe error, invalid command handling, memory error |
| `HW_FAULT` | Hardware device failure | IMU read timeout, GPS receiver no response |
| `DATA_FAULT` | Device responds but data is unusable | stale value, outlier, invalid fix |
| `LINK_FAULT` | Communication path failure | telemetry link timeout, packet loss |
| `SYSTEM_FAULT` | Shared platform failure | CPU, filesystem, scheduler, power issue |

Sensor apps shall not directly trigger Pi/cFS host reset, flight controller
reset, or sensor power-cycle for a single hardware fault. A sensor app shall
first publish invalid, degraded, lost, or failed state. Raspberry Pi/cFS-side
recovery decisions shall be made by a recovery authority such as `cfs_core_app`
or a dedicated health/safety app. Flight-controller-affecting recovery is
outside the baseline authority defined by this spec.

## 10. Hardware Fault Response

Hardware-related fault handling shall follow staged recovery:

| Stage | Condition | App Behavior |
| --- | --- | --- |
| 0 | Normal | Publish valid state |
| 1 | Data quality degraded | Publish valid or degraded with quality/fault detail |
| 2 | Timeout or stale data | Publish invalid or lost state and last-good age |
| 3 | Soft recovery | Reinitialize local parser, reopen Pi communication endpoint, or resubscribe |
| 4 | Hard recovery request | Request communication-path recovery through recovery authority |
| 5 | Degraded cFS operation | Continue cFS telemetry reporting with reduced or invalid data sources |
| 6 | Minimum reporting | Keep only required cFS telemetry/fault reporting active when recovery cannot restore normal operation |

Example hardware responses:

- `mavlink_bridge_app`: detect FC transport timeout, parser error burst, or
  invalid MAVLink frame sequence; publish degraded or lost `BRIDGE_STATUS_MID`
  and request communication-path recovery after retry threshold.
- `img_app`: detect frame timeout or corrupt image; publish invalid image
  metadata and block image-dependent mission processing.

## 11. Recovery Limit and Escalation Policy

cFS does not define fixed retry, restart, or reboot timing values. Recovery
limits shall be mission-defined based on hardware characteristics, fault
criticality, power budget, operational risk, and operator recovery policy.

Each recovery action shall define:

- Trigger condition.
- Retry interval.
- Maximum retry count.
- Escalation target.
- Persistent counter update rule.
- Counter reset condition.
- Required mode or authorization condition.

Recovery actions shall be staged. Apps shall not immediately request processor
reset or reboot for recoverable local faults. A local app shall first report its
fault state through telemetry and housekeeping. System-level recovery decisions
shall be made by the recovery authority.

Default recovery flow:

1. General fault: perform local retry.
2. Repeated fault: request communication-path recovery through the recovery authority.
3. Continued failure: enter `CFS_DEGRADED`, `CFS_RECOVERY`, or minimum reporting.
4. System hang: allow watchdog reset.
5. Repeated reboot: force `CFS_RECOVERY` or minimum-reporting startup.

### 11.1 Recommended First-Pass Recovery Limits

The following values are first-pass mission defaults. They are not cFS-defined
standards and shall be revised after hardware testing.

| Target | Fault Condition | First Recovery | Retry Interval | Max Retry Count | Escalation |
| --- | --- | --- | --- | --- | --- |
| `img_app` | Frame timeout, corrupt frame, capture failure | Restart capture pipeline | 10 s | 3 | Mark camera `FAILED`; block image-dependent mission processing |
| `mavlink_bridge_app` | FC transport timeout, parser error burst, invalid MAVLink frame sequence | Reinitialize local parser or re-request FC stream | 5 s | 3 | Mark bridge `FAILED`; request recovery authority evaluation |
| `downlink_app` | Downlink transport timeout, transmit failure burst, packet formatting fault | Reopen transport endpoint and retry transmit path | 5 s | 3 | Mark downlink `FAILED`; continue local cFS operation without ground delivery if allowed |
| `cfs_core_app` | Repeated app failure or system fault | Restart failed app if allowed | 60 s | 3 per app | Enter `CFS_RECOVERY` or minimum reporting |
| Raspberry Pi/cFS host | System-level unrecoverable cFS host fault | Pi/cFS process or host reset | N/A | 1 per 1800 s recovery window | Minimum-reporting startup if fault repeats |

The retry interval is counted from the completion or failure detection time of
the previous recovery attempt.

A retry count shall be reset only after the target has remained in a valid or
nominal state for the configured stable period.

Recommended first-pass stable periods:

| Target | Stable Period Before Retry Counter Reset |
| --- | --- |
| Sensor app | 300 s |
| Telemetry link | 300 s |
| System-level recovery | 1800 s |

### 11.2 Escalation Rule

Recovery escalation shall follow the lowest-risk action first.

Default escalation order:

1. Report fault through event and housekeeping telemetry.
2. Mark current output invalid, degraded, lost, or failed.
3. Retry local operation.
4. Reinitialize local driver, pipe, file handle, or transport.
5. Request Pi-side communication-path recovery through the recovery authority.
6. Continue in `CFS_DEGRADED`.
7. Enter `CFS_RECOVERY`.
8. Enter minimum-reporting operation.
9. Request Raspberry Pi/cFS host reset only for system-level or unrecoverable repeated faults.

Apps shall not skip directly from local fault detection to Pi/cFS host reset
unless explicitly allowed by mission safety policy. Apps shall not request flight
controller reboot or flight-control reset under the baseline policy.

#### 11.2.1 Escalation Decision Rules

| Condition | Required Escalation |
| --- | --- |
| Single invalid sample | Reject sample; keep app running |
| Consecutive invalid samples below retry threshold | Publish degraded state |
| Timeout exceeds retry threshold | Perform local soft recovery |
| Local soft recovery limit exceeded | Request communication-path recovery through recovery authority |
| Hard recovery limit exceeded | Mark target failed and enter `CFS_DEGRADED` |
| Required cFS function unavailable | Request `CFS_RECOVERY` or minimum-reporting operation |
| Repeated app crash within recovery window | Stop restarting app and request recovery authority action |
| Repeated Pi/cFS host reset within reboot loop window | Start in minimum-reporting operation and block unsafe actions |

### 11.3 Watchdog Reset Policy

The watchdog shall be used only for failures where the system cannot reliably
continue execution or cannot rely on normal software recovery.

The watchdog shall not be used as the primary recovery method for ordinary
sensor faults, invalid data, missing GPS fix, temporary link loss, or single app
errors.

Watchdog reset may be allowed for:

- Main executive loop hang.
- Scheduler or timebase failure.
- Recovery authority hang.
- Critical app unresponsive beyond configured heartbeat timeout.
- Memory corruption indication.
- Deadlock or task starvation affecting system-level control.
- Repeated failure to service required health checks.

#### 11.3.1 Watchdog Heartbeat Rules

Each critical app shall publish or update a heartbeat at a defined interval.
The recovery authority shall monitor heartbeat age and health state.

Long-running apps shall separate heartbeat liveness from job completion when
their internal processing spans multiple cycles or I/O boundaries.

| Component | Heartbeat Timeout | First Action | Escalation |
| --- | --- | --- | --- |
| Critical app | 3 missed heartbeats | Restart app if restart is allowed | Enter `CFS_RECOVERY` if restart fails |
| Recovery authority | 2 missed health cycles | Watchdog reset | Minimum-reporting startup if repeated |
| Scheduler/time service | 2 missed cycles | Watchdog reset | Minimum-reporting startup |
| Non-critical app | 5 missed heartbeats | Mark failed or restart app | Continue degraded if allowed |

Missed heartbeat counts shall be converted to timeout durations using the
configured heartbeat period of each component. For example, a critical app with
a 5 s heartbeat period and a 3-missed-heartbeat threshold has an effective
timeout of 15 s.

The watchdog service interval shall be shorter than the watchdog timeout.
The watchdog timeout shall be long enough to avoid reset during expected peak
processing or file I/O boundaries.

Recommended first-pass values:

| Item | Value |
| --- | --- |
| Recovery authority health cycle | 1 s |
| Critical app heartbeat period | 1 s to 5 s |
| Watchdog service period | 1 s |
| Watchdog timeout | 10 s to 30 s |
| Reboot loop detection window | 1800 s |

These values are first-pass defaults and shall be revised after timing tests.

### 11.4 Reboot Loop Prevention Policy

The system shall prevent repeated automatic reboot from keeping the platform or
system in an unsafe or unusable state.

A reboot loop is detected when the number of Raspberry Pi/cFS host resets
exceeds the allowed limit within a configured time window. Flight controller
reboot is not part of this baseline policy.

Recommended first-pass rule:

| Condition | Action |
| --- | --- |
| 2 Pi/cFS host resets within 1800 s | Start in `CFS_RECOVERY` |
| 3 Pi/cFS host resets within 1800 s | Start in minimum-reporting operation |
| Minimum-reporting startup caused by reboot loop | Load minimum config and block nonessential app startup |
| Persistent state validation failure | Ignore corrupted record and load default safe config |
| Same app causes 3 restarts within 600 s | Stop restarting that app and mark it `FAILED` |
| Same hardware target fails 3 hard recoveries within 1800 s | Disable target and continue degraded if allowed |

#### 11.4.1 Persistent Counters Required for Reboot Loop Prevention

The following counters shall be stored persistently:

- Boot count.
- Pi/cFS host reset count.
- Watchdog reset count.
- Last reset reason.
- Last reset timestamp.
- Reboot loop window start timestamp.
- Per-app restart count.
- Per-app last fault code.
- Per-hardware-target recovery attempt count.
- Minimum-reporting entry reason.
- Last successful `CFS_NOMINAL` timestamp.

Each persistent counter record shall include version, size, CRC/checksum,
timestamp, and generation counter.

#### 11.4.2 Counter Reset Rules

Persistent recovery counters shall not be cleared immediately after reboot.
Counters may be cleared only after the system remains stable for the configured
stable period.

Recommended first-pass reset rules:

| Counter | Reset Condition |
| --- | --- |
| App restart count | App remains `CFS_NOMINAL` for 300 s |
| Hardware recovery count | Target remains valid for 300 s |
| Pi/cFS host reset window | System remains out of reset loop for 1800 s |
| Watchdog reset count | No watchdog reset for 1800 s |
| Minimum-reporting entry reason | Operator command or validated mission policy |

Manual counter clearing shall require explicit command authorization.

## 12. Persistent State and Reboot Recovery

Persistent state shall be limited to mission state, fault and recovery counters,
last known valid navigation or map references, and operator-modified
configuration. High-rate raw sensor samples shall not be written directly to
persistent storage except through bounded logs or explicit diagnostic capture.

Candidate persistent values:

| Category | Persistent Values |
| --- | --- |
| Boot/fault | Last reset reason, boot count, Pi/cFS host reset count, watchdog reset count, watchdog indication, reboot loop window start timestamp |
| App health | App state, restart count, last fault code |
| Hardware health | Sensor health state, last hardware fault code, recovery attempt count |
| Mission state | Mission phase, active cFS state, degraded/recovery/minimum-reporting entry |
| Navigation | Last valid GPS fix, last valid EKF state, last valid timestamp |
| Telemetry | Last link state, last good contact time, active transport ID |
| Config | Operator-modified config version and validated table version |
| Recovery | Pending recovery action, last recovery result |

Reboot handling:

| Event | Recovery Behavior |
| --- | --- |
| App restart | Restore app-local state only if valid and compatible |
| Pi/cFS host reset or soft boot | Restore persistent config, cFS state, last health, and checkpoints |
| Pi/cFS host hard boot or power cycle | Restore only validated persistent state and default safe config |
| Minimum-reporting startup | Load minimum config and expose fault logs; block unsafe recovery actions |

Every persistent record shall include enough integrity metadata for validation,
such as version, size, CRC/checksum, timestamp, and generation counter.

## 13. Active/Pending Configuration Model

Critical apps shall maintain separate active and pending configuration buffers.
Runtime configuration updates shall be written to the pending buffer first and
shall not affect active operation until validation and activation complete.

If validation fails, the app shall continue using the previous active
configuration and report the rejection through event and housekeeping telemetry.

Recommended config state:

- `active_config`: current validated configuration.
- `pending_config`: newly loaded or commanded configuration under validation.
- `previous_config`: optional rollback target for critical apps.
- `config_generation`: incremented on each successful activation.
- `active_config_version`.
- `pending_config_version`.
- `last_config_result`.
- `last_config_error`.

Activation boundaries shall be app-specific:

| App | Activation Boundary |
| --- | --- |
| `mavlink_bridge_app` | Next parser or FC stream processing boundary |
| `img_app` | Next capture cycle or safe capture boundary |
| `downlink_app` | Next transmit scheduling boundary |
| `cfs_core_app` | Operator-approved activation or safe system boundary |

## 14. Double-Buffered Runtime Data Model

Producer/consumer data shared inside an app or between app tasks shall use a
double-buffered or equivalent atomic handoff model when partial updates would be
unsafe.

Typical model:

- Writer fills `write_buffer`.
- Writer validates the complete sample or result.
- Writer swaps the active readable buffer at a short activation point.
- Reader uses `read_buffer` and never observes a partially written value.

This model is recommended for:

- Latest IMU sample.
- Latest GPS fix.
- Latest EKF state.
- Critical health/recovery policy snapshots.

## 15. cFS State and Command Safety

cFS state shall describe only the Raspberry Pi/cFS communication and
state-management layer. It shall not be treated as an FC flight mode or FC
failsafe state.

The baseline cFS states are:

- `CFS_NOMINAL`
- `CFS_DEGRADED`
- `CFS_RECOVERY`

`SHUTDOWN` is treated as an ordered procedure, not a normal operating state.
FC flight modes and failsafe states are owned by the flight controller firmware
and may be reported by cFS as telemetry only.

Each command shall define the modes in which it is allowed. Dangerous commands
such as Pi/cFS host reset, table activation, priority change, and recovery exit
shall require explicit authorization policy in a later command safety spec.
Flight controller reboot, flight-control reset, sensor power-cycle through the
FC, motor or actuator command, arming/disarming, and flight-control parameter
changes are prohibited by the baseline policy.

### 15.1 Per-State App Operation Policy

Each app shall define whether it is enabled, suspended, degraded, or blocked in
each cFS state. The table below defines the default cFS-layer policy.
Per-app deviations shall be documented when each app is implemented.

| cFS State | Default Policy |
| --- | --- |
| `CFS_NOMINAL` | FC data reception, validation, Software Bus publication, and ground telemetry are operating normally |
| `CFS_DEGRADED` | Some data, app, or communication path is stale, invalid, or unstable; cFS continues reporting available telemetry with degraded status |
| `CFS_RECOVERY` | cFS is reinitializing an app, parser, transport, or communication path; nonessential cFS processing may be limited during recovery |

The final per-app state table shall be defined before command authorization is
finalized and is tracked in Section 17.

## 16. Testing Requirements

Each app shall support or be testable through:

- Nominal message input.
- Invalid payload length handling.
- Stale, duplicate, and out-of-order sequence handling.
- Timeout and degraded input handling.
- Hardware fault injection, where practical.
- Runtime config load, reject, activate, and rollback.
- App restart and soft boot recovery.
- Persistent state CRC/checksum failure.

The existing baseline E2E sender only validates a subset of monitor and link
state behavior. Additional test tools are required for MAVLink bridge, IMU,
GPS, EKF, image, downlink, system health, and uplink message flows.

## 17. Open Items

- Final MID numeric assignments.
- Final timestamp basis and units.
- Payload type precision and endian policy.
- App startup order, priority, and stack size.
- SCH-driven versus app-local period control for each app.
- Persistent storage backend and write budget.
- Recovery authority implementation: `cfs_core_app`, health/safety app, or
  existing cFS app integration.
- Exact command authorization and table validation rules.
- Final `uplink_app` command routing contract, including authorized command
  classes, per-target command MID mapping, and `UPLINK_STATUS_MID` payload.
- Complete MID contract table per app, including owner, producer, consumers,
  command MID, publication rate, payload layout, validity rules, and fault
  behavior.
- Define command MID per app and command routing policy.
- Define time basis and time validity rules for mission inputs, including the
  minimum condition for a time basis to be considered known by `cfs_core_app`.
- Define per-state app operation policy, including which apps run, suspend, or
  publish invalid output in `CFS_NOMINAL`, `CFS_DEGRADED`, `CFS_RECOVERY`, and
  minimum-reporting startup. `SHUTDOWN` shall remain an ordered procedure.
- Define app-level correctness properties for sequence handling, timeout
  handling, recovery behavior, config activation, rollback, and reboot-loop
  prevention.
- Final numeric recovery constants for retry intervals, retry limits, heartbeat
  periods, watchdog timeout, and reboot loop windows, to be confirmed after
  hardware testing.
- Define hardware connection topology for `img_app` and future mission payload
  devices: Raspberry Pi direct, FC-bridged, or other. Update recovery policy
  according to the confirmed topology.

## 18. `uplink_app` Specification

### 18.1 Purpose and Scope

`uplink_app` receives commands from the ground segment or operator, validates
each uplink packet, and routes authorized runtime configuration changes and
recovery commands to the appropriate mission apps through the Software Bus.

`uplink_app` is the sole authorized entry point for ground-originated commands
into the cFS mission app layer. No other app shall accept raw ground commands
directly.

`uplink_app` shall not issue commands that affect flight-controller state,
including flight mode changes, arm/disarm, mission changes, motor or actuator
control, or any FC-level parameter change. These are prohibited by the baseline
platform boundary policy defined in Section 3.

### 18.2 Authorized Command Classes

`uplink_app` shall accept and route commands in the following classes only:

| Command Class | Examples | Notes |
| --- | --- | --- |
| Runtime configuration | Receive period change, message enable/disable, publish rate change, timeout threshold change, diagnostic/log level change | Routed through pending config buffer; validated before activation |
| Recovery command | Parser reset, serial reconnect, counter reset, app restart request | Routed to target app or recovery authority; subject to authorization policy |
| Mode command | Recovery mode entry, recovery mode exit | Allowed only within authorized cFS state and operator permission level |
| Diagnostic command | Diagnostic capture enable/disable, log level change | Immediate or one-shot; does not require pending buffer |
| Counter management | Persistent counter reset | Requires explicit command authorization per Section 11.4.2 |

Commands outside these classes shall be rejected and reported through
`UPLINK_STATUS_MID` with a fault code.

### 18.3 Prohibited Command Classes

`uplink_app` shall reject and report any command in the following classes:

- Flight controller reboot or reset.
- Flight-control firmware reset.
- Motor or actuator command.
- Arming or disarming command.
- Sensor power-cycle through the FC.
- Flight mode or flight-control parameter change.
- Any command that bypasses the pending configuration validation model.

### 18.4 Uplink Packet Validation

Every received uplink packet shall be validated before routing. Validation shall
check:

- Packet length and format.
- Command code and target app identifier.
- Packet version and compatibility.
- CRC or checksum integrity.
- Sequence number for duplicate and replay detection.
- Authorization level required for the command class.
- Current cFS state compatibility (some commands are blocked in `CFS_RECOVERY`
  or minimum-reporting startup).

A packet that fails any validation check shall be discarded. The rejection shall
be reported through `UPLINK_STATUS_MID` with the applicable fault code. The
active configuration and system state shall not be modified by a rejected packet.

### 18.5 Configuration Command Handling

Runtime configuration commands shall follow the active/pending model defined in
Section 13.

Processing steps:

1. Receive and validate uplink packet.
2. Write validated configuration payload to `pending_config` buffer of the
   target app, or to `uplink_app`-local pending buffer if the command targets
   `uplink_app` itself.
3. Validate range, version, checksum or CRC, and current mode compatibility.
4. If validation passes, activate by swapping pending to active at the next
   allowed activation boundary for the target app.
5. If validation fails, discard pending config, retain active config, and report
   rejection through `UPLINK_STATUS_MID`.
6. If activation causes a runtime fault, roll back to `previous_config` and
   report the rollback through `UPLINK_STATUS_MID`.

Activation boundaries per target app are defined in Section 13.

### 18.6 Recovery Command Handling

Recovery commands shall be routed to the target app or to `cfs_core_app` as the
recovery authority. `uplink_app` shall not execute recovery actions directly
unless the action is local to `uplink_app` itself (for example, parser reset or
serial reconnect of the uplink transport).

Recovery command routing:

| Recovery Command | Routing Target | Authorization Required |
| --- | --- | --- |
| Parser reset | Target app or bridge component | Operator command |
| Serial reconnect | Bridge component or transport layer | Operator command |
| Counter reset | `cfs_core_app` or target app | Explicit command authorization |
| App restart request | `cfs_core_app` | Operator command with mode check |
| Recovery mode entry | `cfs_core_app` | Operator command with mode check |
| Recovery mode exit | `cfs_core_app` | Operator command with mode check |

Recovery mode entry and exit commands shall be accepted only when the current
cFS state and operator authorization level permit the transition. `uplink_app`
shall check the current cFS state before forwarding a mode command and shall
reject the command if the transition is not allowed.

### 18.7 `UPLINK_STATUS_MID` Payload

`uplink_app` shall publish `UPLINK_STATUS_MID` at a defined housekeeping rate.

Minimum fields:

- Timestamp and time validity.
- Sequence counter.
- Valid flag and health state.
- Last received command code.
- Last received command sequence number.
- Last command result: accepted, rejected, routed, or failed.
- Last rejection fault code.
- Accepted command count.
- Rejected command count.
- Routing failure count.
- Active transport ID.
- Uplink link state: `NOMINAL`, `DEGRADED`, `LOST`, or `FAILED`.
- Last valid uplink receive time.
- Pending config state: idle, pending, validating, or rejected.
- Last config activation result.
- Last rollback reason, if applicable.

### 18.8 Uplink Transport Boundary

`uplink_app` is responsible for receiving uplink packets from the authorized
transport input. The transport may be LoRa serial, a ground-station radio link,
or another approved channel.

`uplink_app` transport responsibilities:

- Open and maintain the uplink transport endpoint.
- Parse and frame-validate incoming packets.
- Detect transport-level faults: timeout, disconnect, parse error.
- Attempt transport-level reconnection after fault.
- Report transport health through `UPLINK_STATUS_MID`.

`uplink_app` shall not classify final link state for the downlink telemetry path.
Downlink telemetry health remains the responsibility of `downlink_app` and
`cfs_core_app`.

### 18.9 Fault and Recovery Behavior

| Fault Condition | `uplink_app` Behavior |
| --- | --- |
| Invalid uplink packet | Discard packet; increment rejected count; report fault code |
| Duplicate or replayed packet | Discard packet; increment rejected count |
| Transport timeout or disconnect | Attempt reconnect; publish `UPLINK_STATUS_MID` with degraded or lost state |
| Routing failure to target app | Report routing failure; do not retry automatically |
| Config validation failure | Retain active config; report rejection |
| Config activation fault | Roll back to previous config; report rollback |
| Unauthorized command class | Reject and report; do not forward |

`uplink_app` shall not request Pi/cFS host reset for isolated uplink packet
errors or transport timeouts. Escalation to `cfs_core_app` shall follow the
staged recovery policy in Section 11.

Recovery limits for `uplink_app`:

| Fault Condition | First Recovery | Retry Interval | Max Retry Count | Escalation |
| --- | --- | --- | --- | --- |
| Transport timeout or disconnect | Reopen transport endpoint | 5 s | 3 | Mark uplink `LOST`; request recovery authority evaluation |
| Repeated routing failure | Report to `cfs_core_app` | Per fault event | 3 | Enter `CFS_DEGRADED` if routing path is unavailable |

### 18.10 Per-State Operation Policy

| cFS State | `uplink_app` Behavior |
| --- | --- |
| `CFS_NOMINAL` | Accept and route all authorized command classes |
| `CFS_DEGRADED` | Accept and route commands; reject commands that require nominal state |
| `CFS_RECOVERY` | Accept diagnostic and status commands only; block configuration and mode commands |
| Minimum-reporting startup | Accept counter reset and diagnostic commands only; block all other command classes |

### 18.11 Open Items for `uplink_app`

- Final `UPLINK_CMD_MID` numeric assignment.
- Final `UPLINK_STATUS_MID` numeric assignment and publication rate.
- Uplink transport type and physical interface definition.
- Per-command authorization level table.
- Sequence number window size and replay detection policy.
- Exact command routing table: command code to target app MID mapping.
- Per-state allowed command class table, finalized after command safety spec.
- Uplink packet format and version policy.
