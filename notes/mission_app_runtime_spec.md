# Mission App Runtime Specification Draft

## 1. Purpose

This document defines the first-pass mission application runtime specification for
the cFS-based system. It is intended to guide implementation after the minimum
`telemetry_app` bring-up.

The main goals are:

- Define application responsibilities and Software Bus message contracts.
- Define which runtime values may be changed during mission operation.
- Define how hardware faults, app faults, restart, and reboot behavior are handled.
- Define persistent state that must survive abnormal shutdown and reboot.
- Define active/pending double-buffering rules for critical runtime changes.

This document is a draft. It captures the current intended architecture and must
be reconciled with implementation as each app is added.

## 2. Scope and Current State

The current repository contains a minimum runnable `telemetry_app` derived from
the cFS sample app pattern. It currently demonstrates:

- cFS app load and startup.
- Software Bus pipe creation and subscription.
- Housekeeping and command handling.
- Monitor input handling through `TELEMETRY_APP_MONITOR_MID`.
- Link state reporting through `TELEMETRY_STATUS_MID`.
- `ALIVE`, `DEGRADED`, `LOST`, and recovery state transitions.

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
- Continue operation using alternate data sources when allowed.

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

## 4. Application Responsibility Model

Each application shall own one clear responsibility and publish its state through
defined MID contracts. Apps shall not directly overwrite another app's owned
state.

| App | Responsibility | Publish MID | Subscribe MID |
| --- | --- | --- | --- |
| `imu_app` | Receive FC-provided IMU/inertial state and publish mission IMU state | `IMU_STATE_MID` | TBD |
| `gps_app` | Receive FC-provided GPS/GNSS state and publish mission navigation fix | `GPS_STATE_MID` | TBD |
| `telemetry_app` | Monitor communication or telemetry input health | `TELEMETRY_STATUS_MID` | `TELEMETRY_MONITOR_MID` or external monitor input |
| `img_app` | Capture image and publish image metadata | `IMAGE_META_MID` | TBD |
| `recon_app` | Run 3D reconstruction and publish reconstruction status/result references | `RECON_STATUS_MID`, `RECON_RESULT_REF_MID` | `IMAGE_META_MID`, `RECON_REQUEST_MID` |
| `align_app` | Align GPS, IMU, and reconstruction outputs | `ALIGN_STATE_MID` | `IMU_STATE_MID`, `GPS_STATE_MID`, `RECON_RESULT_REF_MID` |
| `map_app` | Manage accumulated map and map updates | `MAP_UPDATE_MID` | `ALIGN_STATE_MID`, `RECON_RESULT_REF_MID` |
| `cfs_core_app` | Monitor system health and recovery policy | `SYSTEM_HEALTH_MID` | App health/status MIDs |
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

The timestamp basis shall be defined before sensor fusion implementation. The
candidate bases are cFS mission elapsed time, GPS time, Unix time, or a monotonic
platform clock. `align_app` shall not fuse inputs whose time basis or time
validity is unknown.

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

`align_app` shall reject any individual fusion input when:

- `TimeValid=false` for that input.
- Timestamp source is unknown.
- Timestamp unit is unknown.
- Timestamp age exceeds the configured validity window.
- Sequence counter indicates stale, duplicate, or out-of-order data beyond the
  configured tolerance.
- Required inputs use incompatible time bases without a defined conversion rule.

Rejecting an input with `TimeValid=false` does not automatically invalidate the
fused output. The fused output may remain valid if the remaining inputs satisfy
the active mode and source-mask policy. If the remaining inputs are insufficient
for the current mode, `align_app` shall publish a degraded or invalid fused
state.

A monotonic platform clock may be used for relative timing if the mission only
requires local ordering and age checks. GPS time or another absolute time basis
shall be required only when absolute timing is needed by the mission policy.

The final time basis selection and validity rules shall be defined before
`align_app` implementation begins and are tracked in Section 16.

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

### 6.3 `TELEMETRY_STATUS_MID`

Minimum fields:

- Link state.
- Active transport ID.
- Last valid update age.
- Degraded transition count.
- Lost transition count.
- Recovery count.
- Fault code.

The meaning of `TELEMETRY_STATUS_MID` shall be kept narrow: it represents
telemetry or communication health, not the full system health state.

### 6.4 `ALIGN_STATE_MID`

Minimum fields:

- Timestamp and time validity.
- Sequence counter.
- Valid flag and health state.
- Fused pose or position.
- Attitude, if produced by alignment.
- Source mask showing whether IMU, GPS, and reconstruction were used.
- Quality or covariance.
- Fault code.

### 6.5 `MAP_UPDATE_MID`

Minimum fields:

- Timestamp.
- Map version.
- Checkpoint or update reference.
- Valid flag and health state.
- Update result code.
- Fault code.

### 6.6 `RECON_RESULT_REF_MID`

`RECON_RESULT_REF_MID` shall carry a reference to a reconstruction result, not
raw reconstruction data. Large reconstruction outputs shall not be transported
directly through Software Bus messages.

The payload shall identify where the result can be found and whether it is safe
for consumers to use.

Minimum fields:

- Timestamp and time validity.
- Sequence counter.
- Valid flag and health state.
- Result ID.
- Result type.
- Result storage backend.
- Result reference, such as file path, checkpoint ID, shared-memory key, or
  ring-buffer index.
- Result size.
- Result version.
- Producer job ID.
- Fault code.

The final large-result transfer mechanism shall be defined before implementing
`recon_app` and `map_app` and is tracked in Section 16.

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
| 5 | Failover | Use backup sensor or alternate source, if available |
| 6 | Degraded cFS operation | Continue cFS telemetry reporting with reduced or invalid data sources |
| 7 | Minimum reporting | Keep only required cFS telemetry/fault reporting active when recovery cannot restore normal operation |

Example hardware responses:

- `imu_app`: detect missing FC-provided IMU stream, saturated values, or sample
  rate drop; publish degraded/lost and request communication-path recovery after
  retry threshold.
- `gps_app`: detect no fix, timestamp jump, low satellite count, or missing
  FC-provided GPS stream; publish no-fix/degraded and allow `align_app` to
  operate without GPS.
- `img_app`: detect frame timeout or corrupt image; publish invalid image
  metadata and prevent `recon_app` from using the frame.
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
| `imu_app` | Missing FC-provided IMU stream, stale sample, invalid sample rate | Reinitialize local parser or re-request IMU stream | 10 s | 3 | Mark IMU `FAILED`; continue without direct FC reset or sensor power-cycle |
| `gps_app` | Missing FC-provided GPS stream, no fix, stale GPS state | Reinitialize local parser or re-request GPS stream | 30 s | 5 | Mark GPS `FAILED`; continue navigation without GPS if allowed |
| `img_app` | Frame timeout, corrupt frame, capture failure | Restart capture pipeline | 10 s | 3 | Mark camera `FAILED`; block image-dependent reconstruction |
| `recon_app` | Reconstruction job failure, timeout, invalid output | Abort current job and restart next requested job | Per job boundary | 2 | Mark reconstruction `FAILED`; preserve last valid result reference |
| `telemetry_app` | Monitor input timeout, link lost | Restart Pi-side transport or monitor input path | 5 s | 3 | Mark link `LOST`; request recovery authority evaluation |
| `align_app` | Required input unavailable or time basis invalid | Reject current fusion frame | Next alignment frame | N/A | Publish invalid/degraded fused state |
| `map_app` | Map update commit failure or checkpoint failure | Retry commit from last valid pending update | 30 s | 3 | Roll back to last valid checkpoint; if rollback target is also invalid or corrupt, mark map `FAILED` and continue with last known valid map or enter degraded mode |
| `cfs_core_app` | Repeated app failure or system fault | Restart failed app if allowed | 60 s | 3 per app | Enter `CFS_RECOVERY` or minimum reporting |
| Raspberry Pi/cFS host | System-level unrecoverable cFS host fault | Pi/cFS process or host reset | N/A | 1 per 1800 s recovery window | Minimum-reporting startup if fault repeats |

For frame-based consumers such as `align_app`, retry count is N/A because the
app rejects the current frame and evaluates the next frame independently. Each
frame is processed on its own merits; there is no per-frame retry loop.

The retry interval is counted from the completion or failure detection time of
the previous recovery attempt.

A retry count shall be reset only after the target has remained in a valid or
nominal state for the configured stable period.

Recommended first-pass stable periods:

| Target | Stable Period Before Retry Counter Reset |
| --- | --- |
| Sensor app | 300 s |
| Telemetry link | 300 s |
| Reconstruction job path | 1 successful job |
| Map checkpoint path | 1 successful checkpoint |
| System-level recovery | 1800 s |

### 11.2 Escalation Rule

Recovery escalation shall follow the lowest-risk action first.

Default escalation order:

1. Report fault through event and housekeeping telemetry.
2. Mark current output invalid, degraded, lost, or failed.
3. Retry local operation.
4. Reinitialize local driver, pipe, file handle, or transport.
5. Request Pi-side communication-path recovery through the recovery authority.
6. Fail over to backup device or alternate data source, if available.
7. Continue in `CFS_DEGRADED`.
8. Enter `CFS_RECOVERY`.
9. Enter minimum-reporting operation.
10. Request Raspberry Pi/cFS host reset only for system-level or unrecoverable repeated faults.

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

Long-running apps shall separate heartbeat liveness from job completion. For
example, `recon_app` may run a long reconstruction job, but it shall continue to
publish heartbeat state while the job is in progress.

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
processing, file I/O, or reconstruction job boundaries.

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
| Navigation | Last valid GPS fix, last valid pose, last valid timestamp |
| Mapping | Map checkpoint ID, map version, last committed update |
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
| `imu_app` | Next sample cycle or device reconfiguration boundary |
| `gps_app` | Next fix processing boundary |
| `telemetry_app` | Next monitor evaluation cycle |
| `recon_app` | After current reconstruction job completes |
| `align_app` | Next alignment frame boundary |
| `map_app` | Checkpoint or map commit boundary |
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
- Latest fused pose.
- Reconstruction result references.
- Map update references.
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

The existing `telemetry_app` E2E sender only validates monitor input and link
state behavior. Additional test tools are required for IMU, GPS, image,
reconstruction, alignment, map, system health, and uplink message flows.

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
- Mapping between current `telemetry_app` implementation and final
  `TELEMETRY_STATUS_MID` definition.
- Complete MID contract table per app, including owner, producer, consumers,
  command MID, publication rate, payload layout, validity rules, and fault
  behavior.
- Define command MID per app and command routing policy.
- Define time basis and time validity rules for fusion inputs, including the
  minimum condition for a time basis to be considered known by `align_app`.
- Define `RECON_RESULT_REF_MID` payload and large-result transfer policy,
  including the storage backend and reference mechanism.
- Define per-state app operation policy, including which apps run, suspend, or
  publish invalid output in `CFS_NOMINAL`, `CFS_DEGRADED`, `CFS_RECOVERY`, and
  minimum-reporting startup. `SHUTDOWN` shall remain an ordered procedure.
- Define map rollback failure behavior when the last valid checkpoint is missing,
  corrupt, or incompatible.
- Define app-level correctness properties for sequence handling, timeout
  handling, recovery behavior, config activation, rollback, and reboot-loop
  prevention.
- Final numeric recovery constants for retry intervals, retry limits, heartbeat
  periods, watchdog timeout, and reboot loop windows, to be confirmed after
  hardware testing.
- Define hardware connection topology for `img_app` and future mission payload
  devices: Raspberry Pi direct, FC-bridged, or other. Update recovery policy
  according to the confirmed topology.
