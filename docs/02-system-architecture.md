# 02. System Architecture

## 1. Overview

Describe the overall architecture and design intent.

## 2. Module Composition

List the major modules and their relationships.

| Module | Responsibility | Depends On |
| --- | --- | --- |
| UWB Module | Position estimation from anchor/tag data | Sensor input |
| GPS Interface | Global position acquisition and local frame conversion support | GPS receiver |
| IMU Interface | Attitude, angular rate, and acceleration acquisition | IMU sensor |
| Reconstruction Module | 3D reconstruction from image/sensor input | Camera and sensor input |
| Pose / Frame Alignment Module | Coordinate frame alignment and calibration | UWB, GPS, IMU, camera, and reconstruction outputs |
| cFS Integration Layer | Runtime integration, messaging, configuration, logging | All functional modules |

## 3. Module Responsibilities

### 3.1 UWB Module

- Estimate distance and position
- Validate ranging data
- Provide structured outputs

### 3.2 Reconstruction Module

- Process input images and sensor data
- Generate 3D reconstruction outputs
- Apply quality checks

### 3.3 GPS Interface

- Receive GPS position and timestamp data
- Preserve raw WGS84 measurements
- Provide local-frame conversion inputs to the alignment module

### 3.4 IMU Interface

- Receive vehicle attitude, angular rate, and acceleration data
- Preserve IMU/body-frame metadata
- Provide orientation constraints to the alignment module

### 3.5 Pose / Frame Alignment Module

- Manage frame transforms
- Apply offsets and calibration
- Produce unified coordinate outputs
- Align UWB, GPS, IMU, camera, and reconstruction frames into the World / Map frame

### 3.6 cFS Integration Layer

- Manage application lifecycle
- Publish and subscribe messages
- Handle configuration, events, and timers

## 4. Data Flow

Describe how information moves between modules.

1. Raw inputs are collected by source interfaces.
2. UWB, GPS, IMU, and reconstruction processing run on their respective inputs.
3. Alignment logic transforms source outputs into a common World / Map frame.
4. cFS integration distributes outputs to downstream consumers.

## 5. Connectivity Between Modules

| Source Module | Target Module | Interface Type | Notes |
| --- | --- | --- | --- |
| UWB Module | Pose / Frame Alignment Module | Data message | Position and ranging result |
| GPS Interface | Pose / Frame Alignment Module | Data message | GPS position and timestamp |
| IMU Interface | Pose / Frame Alignment Module | Data message | Attitude, angular rate, acceleration |
| Reconstruction Module | Pose / Frame Alignment Module | Data message | 3D result and metadata |
| Pose / Frame Alignment Module | cFS Integration Layer | Data message | Unified output |
| cFS Integration Layer | All Modules | Control/config interface | Runtime control |

## 6. Architecture Constraints

- Use consistent interface definitions across modules.
- Preserve traceability from requirements to verification.
- Isolate module responsibilities where possible.

## 7. Open Items

- TBD
- TBD
