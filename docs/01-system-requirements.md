# 01. System Requirements

## 1. Purpose

Describe the overall objective of the system.

- Why the system exists
- What mission or operational problem it solves
- Expected end users or operators

## 2. Scope

Define what is included and excluded.

- In-scope functions
- Out-of-scope functions
- Assumptions and constraints

## 3. System Components

List the top-level components.

| Component | Description | Inputs | Outputs |
| --- | --- | --- | --- |
| UWB Module | Estimates drone/tag position from UWB Anchor distance measurements | Anchor distance messages, Anchor coordinates, timer events | Position_Result, UWB logs/status |
| GPS Interface | Receives global position measurements when available | GPS receiver data | GPS position/time metadata |
| IMU Interface | Receives vehicle attitude, angular rate, and acceleration data | IMU sensor data | IMU/body-frame motion metadata |
| Reconstruction Module | Produces image-based 3D reconstruction outputs from drone image sets | Image set metadata, image references, optional auxiliary pose/localization | Reconstruction result reference, quality metadata, camera trajectory metadata |
| Pose / Alignment Module | Aligns UWB, GPS, IMU, camera, and reconstruction frames into the common World / Map frame | Source poses, transforms, calibration parameters, reconstruction metadata | Aligned pose/transform metadata, calibration status, source selection metadata |
| cFS Integration Layer | Provides runtime integration through cFS app lifecycle, Software Bus messages, timers, configuration, and events | Module messages, timer events, configuration tables, health/status events | Published SB messages, scheduled callbacks, event logs, diagnostic telemetry |

## 4. End-to-End Data Flow

Describe how data flows through the full system.

1. Sensor and source data are acquired.
2. UWB, GPS, IMU, camera, and image-source metadata are timestamped and packaged.
3. Positioning and reconstruction processing are executed.
4. Coordinate alignment is applied into the system World / Map frame.
5. Results are packaged and delivered through the integration layer.

## 5. Common Rules

Define system-wide conventions.

- **Naming rules**: Module-owned messages, artifacts, and manifest fields SHALL use stable snake_case field names and documented IDs.
- **Data ownership rules**: Each module SHALL own creation of its primary output. Other modules MAY update only the metadata fields explicitly exposed through an interface contract. Raw reconstruction artifacts SHALL NOT be modified by alignment or viewer modules.
- **Logging rules**: Logs SHALL include timestamp, source module, severity, status/error code when available, and relevant payload or artifact references.
- **Time synchronization rules**: cFS_TIME is the system reference timestamp unless a prototype interface explicitly documents a temporary serialization format.
- **Fault handling principles**: Missing or degraded sensor data SHALL be reported explicitly and SHALL NOT silently produce nominal fused outputs.
- **Version compatibility rules**: Interface changes SHALL preserve backward-compatible optional fields where possible and SHALL update 03-interface-specification.md before implementation.
- **Module optionality rules**: Sensor/source modules, including UWB, GPS, IMU, camera, and reconstruction, SHALL be independently enableable/disableable through configuration when the mission mode permits. Disabled modules SHALL produce explicit unavailable/degraded status rather than blocking unrelated modules.

## 6. System-Level Requirements

### 6.1 Functional Requirements

- The system shall provide a modular pipeline for collecting sensor/source data, producing reconstruction outputs, and aligning results into the common World / Map frame.
- The system shall support UWB, GPS, IMU, camera, and reconstruction data as independent sensor/source inputs.
- The system shall preserve source-specific measurements before converting them into a common World / Map coordinate frame.
- The system shall allow reconstruction outputs to remain in a relative reconstruction frame until alignment metadata is available.
- The system shall support degraded operation when the UWB module is disabled, unavailable, or physically removed, provided that downstream modules can operate with GPS, IMU, camera, reconstruction, or other configured sources.

### 6.2 Performance Requirements

- The UWB positioning path shall target 15 Hz nominal output as defined by the cFS Output_Cycle_Timer.
- Reconstruction runtime and throughput targets shall be measured per image set and finalized after prototype DUSt3R-family benchmarking.
- System-level maximum latency and accuracy thresholds remain open under OI-SYS-01.

### 6.3 Reliability Requirements

- The system shall isolate disabled or failed source modules so that unrelated enabled modules can continue operating when mission mode permits.
- The system shall expose degraded/unavailable status for missing UWB, GPS, IMU, camera, reconstruction, or alignment data rather than silently publishing nominal fused outputs.
- Availability target and recovery timing remain open under OI-SYS-01 and OI-SYS-02.

### 6.4 Operational Requirements

- The system shall support a ground-side cFS-managed execution environment with a remote GPU reconstruction server for DUSt3R-family processing.
- UWB, GPS, IMU, camera, reconstruction endpoint, output format, module enable flags, and alignment transform parameters shall be configurable at startup.
- Exact deployment split and hardware dependency list remain open under OI-SYS-02.

### 6.5 Safety and Security Requirements

- Remote reconstruction access shall be restricted to configured endpoints or tunnels during prototype operation.
- Large artifacts shall be referenced by path/URI and shall not be silently embedded into cFS Software Bus messages.
- Final access control, data protection, and fail-safe policy remain open under OI-SYS-02.

## 7. Open Items

- OI-SYS-01: System-level latency, update-rate, and accuracy targets need to be finalized.
- OI-SYS-02: Deployment environment, hardware dependencies, and security policy need to be finalized.
