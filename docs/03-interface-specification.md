# 03. Interface Specification

## 1. Purpose

모듈 간 인터페이스 계약을 정의한다.
각 모듈은 이 문서에 정의된 메시지 구조, 단위, 타임스탬프 기준,
Error_Code 체계를 준수 SHALL.

---

## 2. Message Format

모든 모듈 간 메시지는 아래 공통 구조를 따른다.

| Field        | Type     | Required | Description                                                  |
|--------------|----------|----------|--------------------------------------------------------------|
| `message_id` | uint32   | Yes      | 고유 메시지 식별자                                           |
| `timestamp`  | cFS_TIME | Yes      | 페이로드에 연관된 **기체 생성 시각** (vehicle-generated)     |
| `source`     | uint8    | Yes      | 생성 모듈 식별자                                             |
| `payload`    | (구조체) | Yes      | 메시지 본문                                                  |
| `status`     | uint8    | No       | 처리 상태 또는 오류 상태                                     |
| `seq`        | uint32   | No       | 메시지 생성 순서 번호. 동일 source 내에서 단조 증가 SHALL    |
| `image_id`   | string   | No       | 이미지 이벤트와 연관된 경우 해당 이미지 식별자              |
| `frame_id`   | string   | No       | 좌표계 또는 세션 프레임 식별자                               |
| `job_id`     | string   | No       | Reconstruction 작업과 연관된 경우 해당 작업 식별자           |

Message-format rules:

Correlation field rules:

- `seq` SHALL be monotonically increasing within a single source module across a session. Consumers MAY use `seq` to detect gaps or reordering.
- `image_id`, `frame_id`, and `job_id` SHALL be populated in any message that describes an event associated with an image capture, coordinate frame, or reconstruction job respectively.
- When a LoRa status message and an image/video metadata message describe the same vehicle event, they SHALL carry the same `image_id`, `job_id`, or `seq` value to allow ground-side correlation.
- Ground-side consumers SHALL use the vehicle-generated `timestamp` field — not the ground reception time — as the authoritative event time for cross-link correlation.

### 3.2D Communication Link Role Contract

To preserve communication-role separation on the cFS boundary, each message SHALL
declare one of the following logical link roles:

| Link Role Token | Description | Allowed Traffic Class |
|---|---|---|
| `CONTROL_HEALTH_LINK` | LoRa control/health path | heartbeat, status, housekeeping, fault/event summaries, uplink commands |
| `PAYLOAD_LINK` | Image/video payload path | image frames, video streams, reconstruction artifact transfers |

Role rules:

- `CONTROL_HEALTH_LINK` traffic SHALL NOT be routed on `PAYLOAD_LINK` except under an explicitly documented fallback policy.
- `PAYLOAD_LINK` traffic SHALL NOT be routed on `CONTROL_HEALTH_LINK` except under an explicitly documented fallback policy.
- When bridged into module-specific naming, `CONTROL_HEALTH_LINK` maps to `LORA` and `PAYLOAD_LINK` maps to `IMG_VID`.

---

## 3. Input / Output Data Definitions

### 3.1 Input Data (UWB 모듈 기준)

| Name             | Source        | Format        | Notes                              |
|------------------|---------------|---------------|------------------------------------|
| Distance_Message | UWB Anchor×4  | float64 + cFS_TIME | Anchor_ID별 거리(cm) 및 수신 시각 |

### 3.2 Output Data (UWB 모듈 기준)

**Position_Result** 메시지 필드 정의:

| 필드명         | 타입        | 필수 | 설명                                                    |
|----------------|-------------|------|---------------------------------------------------------|
| `timestamp`    | cFS_TIME    | Yes  | Distance_Set 내 마지막 거리값 수신 시각                 |
| `position.x`   | float64     | Yes  | Tag 추정 위치 X (cm), Invalid 시 NaN                   |
| `position.y`   | float64     | Yes  | Tag 추정 위치 Y (cm), Invalid 시 NaN                   |
| `position.z`   | float64     | Yes  | Tag 추정 위치 Z (cm), Invalid 시 NaN                   |
| `anchor_count` | uint8       | Yes  | 해당 사이클 유효 Anchor 수                              |
| `distances[4]` | float64[4]  | Yes  | Anchor별 입력 거리 (cm), 누락 시 -1                    |
| `residual`     | float64     | Yes  | 절대 오차 평균 (cm), 계산 불가 시 NaN                  |
| `valid`        | bool        | Yes  | True = valid position, False = invalid position         |
| `error_code`   | uint8       | Yes  | 아래 Error_Code 열거형 참조. 정상 시 NONE(0x00)        |

### 3.2A Input Data (GPS / IMU / Pose Alignment)

| Name | Source | Format | Notes |
|---|---|---|---|
| `GPS_Message` | GPS receiver | latitude, longitude, altitude, timestamp | Raw global position. Conversion to local ENU/NED is handled by alignment policy. |
| `IMU_Message` | IMU sensor | attitude, angular_rate, acceleration, timestamp | Body-frame motion/orientation input. Axis convention is hardware-calibration dependent. |
| `Camera_Pose_Message` | Camera or vision pipeline | pose, timestamp, camera_id | Optional camera pose estimate when available. |
| `Frame_Transform_Message` | Calibration/config | source_frame, target_frame, transform, validity | Static or dynamic transform metadata used by alignment. |

### 3.2B Baseline cFS SB Input Set

The baseline required SB input set for the current cFS deployment is:

| MID | Message Name | Publisher | Purpose |
|---|---|---|---|
| `0x1901` | `IMU_STATE_MID` | `imu_app` | Baseline IMU input to alignment |
| `0x1902` | `GPS_STATE_MID` | `gps_app` | Baseline GPS input to alignment |
| `0x1903` | `TELEMETRY_STATUS_MID` | `telemetry_app` | Baseline communication-link health input |
| `0x1904` | `IMAGE_META_MID` | `img_app` | Baseline image metadata input to reconstruction |

`IMAGE_META_MID` is a metadata/reference message only and SHALL NOT carry raw image
binary payload on the baseline SB path.

`TELEMETRY_STATUS_MID.link_state` SHALL use the values `ALIVE`, `DEGRADED`, and `LOST`.

The detailed field structures for `IMU_Message` and `GPS_Message` are defined in
Section 3.2A.

### 3.2C Telemetry Monitor Input Contract

The baseline telemetry monitor input contract for `telemetry_app` is:

| Field | Type | Required | Description |
|---|---|---|---|
| `active_transport_id` | uint8 | Yes | Identifier of the transport path being assessed |
| `valid` | bool | Yes | `True` when the monitor update is valid for link-state assessment |
| `update_age_ms` | uint32 | Yes | Age in milliseconds of the most recent valid transport activity indication |

Telemetry monitor input rules:

- The monitor-input producer SHALL provide `active_transport_id`, `valid`, and `update_age_ms` for each accepted telemetry monitor update.
- `update_age_ms` SHALL represent the elapsed time since the most recent valid transport activity indication used for link-health assessment.
- `telemetry_app` SHALL treat `valid == False` monitor updates as invalid for nominal link-state refresh.
- `telemetry_app` SHALL use the configured active transport identifier together with `update_age_ms` to classify the link state as `ALIVE`, `DEGRADED`, or `LOST`.

### 3.3 Output Data (Reconstruction 검증 UI 연동)

Reconstruction 결과의 좌표 기반 검증 UI 연동 필드 정의:

| 필드명                  | 타입            | 필수 | 설명 |
|-------------------------|-----------------|------|------|
| `job_id`                | string          | Yes  | Reconstruction 요청/응답 상관관계를 위한 작업 식별자 |
| `image_set_id`          | string          | Yes  | 입력 이미지 세트 식별자 |
| `frame_id`              | string          | Yes  | 고정 좌표계 식별자 (예: `opencv`, `enu`) |
| `transform.linear[3][3]`| float64[3][3]   | Yes  | UI 표기 좌표로의 선형 변환 행렬 |
| `transform.translate`   | float64[3]      | Yes  | UI 표기 좌표로의 평행이동 벡터 |
| `point_cloud_ref`       | string or object| Yes  | 점군 데이터 참조 또는 직접 페이로드 |
| `camera_trajectory[]`   | list            | Yes  | 이미지별 카메라 위치 목록 |
| `camera_trajectory[].image_id` | string   | Yes  | 카메라 노드와 매핑되는 이미지 식별자 |
| `camera_trajectory[].source_path` | string | No   | 디버그/검증용 원본 이미지 경로 |
| `camera_trajectory[].position` | float64[3] | Yes | 고정 좌표계 기준 카메라 위치 |
| `uwb_overlay_points[]` | list            | No   | 검증 UI에 겹쳐 표시할 UWB 좌표 목록 |
| `uwb_overlay_points[].label` | string     | No   | UWB 점 식별 라벨 |
| `uwb_overlay_points[].position` | float64[3] | Yes | `frame_id`/`transform` 적용 전 원본 UWB 좌표(cm) |

Visualization position rule:

- Reconstruction viewer positions SHALL preserve the artifact/backend coordinate unit unless a transform explicitly converts scale.
- UWB overlay positions SHALL remain cm before transform application.
- When multiple sources are rendered together, the viewer SHALL display transform metadata and SHALL NOT imply metric consistency unless an alignment status indicates `ALIGNED`.
- `transform.translate` in this section corresponds to `translate[3]` in the accumulated map manifest transform object defined in Section 3.5.

---

### 3.4 Reconstruction Remote Execution API (Prototype)

The current prototype transport for remote reconstruction is HTTP polling.
This section defines the temporary contract until the final transport is frozen.

#### 3.4.1 Submit Job

`POST /jobs`

Request payload:

| Field | Type | Required | Description |
|---|---|---|---|
| `job_id` | string | Yes | Client-generated reconstruction job identifier |
| `image_set_id` | string | Yes | Logical image set identifier |
| `images[]` | list | Yes | Ordered image descriptors |
| `images[].image_id` | string | Yes | Unique image identifier |
| `images[].timestamp` | cFS_TIME | Yes | Acquisition timestamp serialized by the wire layer |
| `images[].source_path` | string | Yes | Server-readable image path or URI |
| `output_format` | string | Yes | Requested external output format, currently usually `glb` |
| `aux_pose` | object/null | No | Optional camera pose or localization aid |
| `extra` | object | No | Forward-compatible extension fields |

Response payload:

| Field | Type | Required | Description |
|---|---|---|---|
| `job_id` | string | Yes | Accepted job identifier |
| `status` | string | Yes | Initial job status |
| `poll_url` | string | Yes | URL path for polling job status |
| `artifact_url` | string | Yes | URL path for downloading the completed artifact |

#### 3.4.2 Poll Job

`GET /jobs/{job_id}`

Response payload SHALL follow `ReconstructionResponse`:

| Field | Type | Required | Description |
|---|---|---|---|
| `job_id` | string | Yes | Job identifier |
| `status` | string | Yes | `pending`, `success`, `degraded`, `failed`, or `timeout` |
| `result_ref` | string/null | No | Server-side artifact reference |
| `output_format` | string/null | No | Artifact format token |
| `quality_meta` | object | Yes | Reconstruction quality metadata |
| `error_code` | string/null | No | Failure/degraded reason code |
| `processing_duration_s` | float/null | No | Remote processing duration |
| `completed_at` | string/null | No | Completion timestamp |
| `extra` | object | No | Forward-compatible extension fields |

#### 3.4.3 Download Artifact

`GET /jobs/{job_id}/artifact`

The server SHALL return the completed reconstruction artifact as binary data
when available. The client SHALL store the artifact locally and pass the local
artifact path to the fixed-frame visualization or downstream integration path.

---

### 3.5 Accumulated Map Manifest (Prototype)

The accumulated map manifest describes how multiple reconstruction artifacts are
tracked as map chunks. It is a ground-side data contract and SHALL NOT require
modification of raw reconstruction artifacts.

| Field | Type | Required | Description |
|---|---|---|---|
| `map_id` | string | Yes | Persistent accumulated map identifier |
| `created_at` | cFS_TIME | Yes | Manifest creation timestamp |
| `updated_at` | cFS_TIME | Yes | Last manifest update timestamp |
| `display_frame_id` | string | Yes | Viewer/display frame, e.g. `enu` or `world`. For single-artifact viewer output this corresponds to Section 3.3 `frame_id`. |
| `chunks[]` | list | Yes | Reconstruction chunks ordered by append sequence unless a renderer explicitly sorts by another documented key |
| `chunks[].chunk_id` | string | Yes | Unique chunk identifier |
| `chunks[].job_id` | string | Yes | Source reconstruction job identifier |
| `chunks[].image_set_id` | string | Yes | Source image set identifier |
| `chunks[].artifact_ref` | string | Yes | Ground-side local artifact path after successful download |
| `chunks[].output_format` | string | Yes | Artifact format token, e.g. `ply` or `glb` |
| `chunks[].alignment_status` | string | Yes | `ALIGNED`, `PARTIAL_ALIGNMENT`, or `UNALIGNED` |
| `chunks[].source_frame_id` | string | Yes | Original artifact/reconstruction frame. For single-artifact viewer output this is derived from Section 3.3 `frame_id` when no more specific backend frame is provided. |
| `chunks[].target_frame_id` | string | No | Target frame if alignment is available |
| `chunks[].transform` | object/null | Conditional | Required when `alignment_status` is `ALIGNED` or `PARTIAL_ALIGNMENT`; null when `alignment_status` is `UNALIGNED` |
| `chunks[].quality_meta` | object | No | Reconstruction quality metadata |
| `chunks[].source_images[]` | list | No | Image identifiers or paths used by the chunk |

Transform object:

| Field | Type | Required | Description |
|---|---|---|---|
| `scale` | float64 | Yes | Uniform scale factor |
| `linear[3][3]` | float64[3][3] | Yes | Rotation or linear transform |
| `translate[3]` | float64[3] | Yes | Translation vector |
| `timestamp_basis` | string | No | Sensor timestamp basis used for alignment |
| `calibration_status` | string | No | Calibration validity status |

Transform application order:

```text
p_target = scale * (linear @ p_source) + translate
```

`linear` is applied in column-vector convention. Implementations using row-vector
math SHALL produce equivalent results.

Map update operation:

| Operation | Required Inputs | Expected Behavior |
|---|---|---|
| `append_chunk` | map_id, artifact_ref, job_id, image_set_id, output_format | Add a new chunk without modifying the raw artifact |
| `update_chunk_transform` | map_id, chunk_id, transform, alignment_status | Update alignment metadata for an existing chunk |
| `invalidate_chunk` | map_id, chunk_id, reason | Mark an existing chunk as invalidated without deleting the raw artifact |
| `render_map` | map_id, display_frame_id | Render all available chunks, distinguishing aligned and unaligned chunks |

`append_chunk` request:

| Field | Type | Required | Description |
|---|---|---|---|
| `map_id` | string | Yes | Target accumulated map identifier |
| `job_id` | string | Yes | Source reconstruction job identifier |
| `image_set_id` | string | Yes | Source image set identifier |
| `artifact_ref` | string | Yes | Ground-side local artifact path |
| `output_format` | string | Yes | Artifact format token |
| `source_frame_id` | string | Yes | Source reconstruction frame identifier |
| `quality_meta` | object | No | Reconstruction quality metadata |
| `source_images[]` | list | No | Source image identifiers or paths |

`append_chunk` response:

| Field | Type | Required | Description |
|---|---|---|---|
| `map_id` | string | Yes | Target map identifier |
| `chunk_id` | string | Yes | Created chunk identifier |
| `status` | string | Yes | `appended`, `duplicate_rejected`, or `failed` |
| `error_code` | string/null | No | Failure or duplicate reason |

`update_chunk_transform` request:

| Field | Type | Required | Description |
|---|---|---|---|
| `map_id` | string | Yes | Target accumulated map identifier |
| `chunk_id` | string | Yes | Existing chunk identifier |
| `alignment_status` | string | Yes | `ALIGNED`, `PARTIAL_ALIGNMENT`, or `UNALIGNED` |
| `target_frame_id` | string | Conditional | Required when transform is not null |
| `transform` | object/null | Conditional | Required for `ALIGNED` or `PARTIAL_ALIGNMENT`; null for `UNALIGNED` |
| `updated_by` | string | No | Producer module, e.g. `pose_frame_alignment` |

`update_chunk_transform` response:

| Field | Type | Required | Description |
|---|---|---|---|
| `map_id` | string | Yes | Target map identifier |
| `chunk_id` | string | Yes | Updated chunk identifier |
| `status` | string | Yes | `updated`, `not_found`, or `failed` |
| `error_code` | string/null | No | Failure reason |

### 3.5A Sequence / Session Map State (Prototype Direction)

For sequence-based SLAM backends, the preferred primary output is an evolving
session map state rather than a set of independently aligned artifact chunks.
This section defines the additional contract that may coexist with Section 3.5.
Unless otherwise specified below, `ordered frames[]` in this section SHALL reuse
the same image descriptor structure as Section 3.4 `images[]`:
`image_id`, `timestamp`, `source_path`, and optional extension fields in `extra`.
`image_sequence_id` is a logical ordered-frame set identifier supplied by the
ground-side path. `session_config` is an object containing backend selection,
output/export policy, and any sequence-mode runtime configuration defined by
the implementation.

| Field | Type | Required | Description |
|---|---|---|---|
| `session_id` | string | Yes | Long-lived sequence processing session identifier |
| `status` | string | Yes | `active`, `completed`, `failed`, or `exported` |
| `frame_count` | uint32 | Yes | Number of accepted frames in the session |
| `keyframe_count` | uint32 | No | Number of keyframes retained by the backend when that concept is available |
| `rendered_point_count` | uint32 | No | Number of points currently exposed for visualization or diagnostic export when available |
| `pose_stream_ref` | string or object | Yes | Reference to session camera trajectory or pose stream. For the current prototype, the backend-native `.txt` trajectory file SHALL be treated as the authoritative pose-stream artifact, and any normalized pose list is informational only. |
| `map_state_ref` | string or object | Yes | Reference to current dense/sparse map state. For the current prototype, the backend-native `.ply` snapshot file SHALL be treated as the authoritative map-state artifact. |
| `current_frame_ref` | string/null | No | Most recent accepted frame path or URI when available |
| `exported_artifact_ref` | string/null | No | Optional exported artifact for offline inspection |
| `alignment_status` | string | Yes | World/Map alignment status for the session map. Status values SHALL follow the definitions in 06-pose-frame-alignment-requirements.md ALIGN-OUT-05. |
| `world_transform` | object/null | Conditional | Session-to-World transform when available |
| `tracking_state` | string | No | Session runtime tracking state such as `initializing`, `tracking`, `relocalizing`, `completed`, or implementation-defined equivalent |
| `last_updated` | cFS_TIME | No | Timestamp of the latest session-state update |

Session lifecycle rules:

- `active` sessions MAY accept additional `append_frames` requests.
- `completed`, `failed`, and `exported` sessions SHALL reject additional `append_frames` requests unless an explicit reopen policy is defined by a future revision.
- `end_session` with finalize semantics SHALL transition the session to `completed`.
- `end_session` with discard semantics SHALL transition the session to `failed` or implementation-defined terminal discard state and SHALL make the session unavailable for further frame append.

Session operations:

| Operation | Required Inputs | Expected Behavior |
|---|---|---|
| `start_session` | image_sequence_id or session config | Create a long-lived SLAM/reconstruction session |
| `append_frames` | session_id, ordered frames[] | Add ordered frames to an existing session |
| `get_session_state` | session_id | Return latest pose/map state |
| `update_session_transform` | session_id, alignment_status, world_transform | Update session-to-World alignment metadata for an existing session |
| `export_session_artifact` | session_id, output_format | Export optional diagnostic artifact without redefining the session map as independent chunks |
| `end_session` | session_id | Finalize or discard the session |

Relationship to Section 3.5:

- Section 3.5 remains the contract for independent artifact chunks and persistent diagnostic artifact manifests.
- Section 3.5A is the preferred primary contract for continuous SLAM/session outputs.
- `export_session_artifact` MAY produce an artifact that is later inserted into the Section 3.5 manifest through `append_chunk` for offline inspection, archival, or mixed-mode rendering.
- Same-session SLAM updates SHALL NOT require repeated `append_chunk` operations for every incremental map update. Those updates SHALL remain within the session-state path until an explicit export or snapshot policy is invoked.

`start_session` request:

| Field | Type | Required | Description |
|---|---|---|---|
| `image_sequence_id` | string | Conditional | Required when starting a session from a named sequence source |
| `session_config` | object | Conditional | Required when start behavior depends on runtime configuration |
| `session_config.backend_name` | string | No | Requested sequence backend, e.g. MASt3R-SLAM-family |
| `session_config.output_policy` | string | No | `session_state_only`, `session_plus_export`, or implementation-defined equivalent. `session_plus_export` means the implementation SHALL export a diagnostic artifact automatically when the session is finalized successfully, with semantics equivalent to `export_session_artifact`. |
| `session_config.extra` | object | No | Forward-compatible runtime configuration |

`start_session` response:

| Field | Type | Required | Description |
|---|---|---|---|
| `session_id` | string | Yes | Created session identifier |
| `status` | string | Yes | `active` or `failed` |
| `error_code` | string/null | No | Failure reason when session start is rejected |

`append_frames` request:

| Field | Type | Required | Description |
|---|---|---|---|
| `session_id` | string | Yes | Existing session identifier |
| `ordered_frames[]` | list | Yes | Ordered frame descriptors reusing the Section 3.4 image descriptor structure |

`append_frames` response:

| Field | Type | Required | Description |
|---|---|---|---|
| `session_id` | string | Yes | Target session identifier |
| `status` | string | Yes | `accepted`, `session_not_found`, `session_closed`, or `failed` |
| `frame_count` | uint32 | No | Updated accepted frame count when available |
| `error_code` | string/null | No | Failure reason |

`get_session_state` response:

| Field | Type | Required | Description |
|---|---|---|---|
| `session_id` | string | Yes | Session identifier |
| `status` | string | Yes | Current session status |
| `frame_count` | uint32 | Yes | Number of accepted frames |
| `keyframe_count` | uint32 | No | Number of retained keyframes when available |
| `rendered_point_count` | uint32 | No | Current rendered/diagnostic point count when available |
| `pose_stream_ref` | string or object | Yes | Latest pose stream reference |
| `map_state_ref` | string or object | Yes | Latest map state reference |
| `current_frame_ref` | string/null | No | Most recent accepted frame path or URI when available |
| `alignment_status` | string | Yes | Alignment status using ALIGN-OUT-05 values |
| `world_transform` | object/null | Conditional | Session-to-World transform when available |
| `tracking_state` | string | No | Runtime tracking state exposed for monitoring and live visualization |
| `last_updated` | cFS_TIME | No | Timestamp of the latest session-state update |
| `error_code` | string/null | No | Failure reason |

`update_session_transform` request:

| Field | Type | Required | Description |
|---|---|---|---|
| `session_id` | string | Yes | Existing session identifier |
| `alignment_status` | string | Yes | `ALIGNED`, `PARTIAL_ALIGNMENT`, or `UNALIGNED` |
| `world_transform` | object/null | Conditional | Required for `ALIGNED` or `PARTIAL_ALIGNMENT`; null for `UNALIGNED` |
| `updated_by` | string | No | Producer module, e.g. `pose_frame_alignment` |

`update_session_transform` response:

| Field | Type | Required | Description |
|---|---|---|---|
| `session_id` | string | Yes | Updated session identifier |
| `status` | string | Yes | `updated`, `not_found`, or `failed` |
| `error_code` | string/null | No | Failure reason |

`export_session_artifact` request:

| Field | Type | Required | Description |
|---|---|---|---|
| `session_id` | string | Yes | Existing session identifier |
| `output_format` | string | Yes | Requested export format token |

`export_session_artifact` response:

| Field | Type | Required | Description |
|---|---|---|---|
| `session_id` | string | Yes | Session identifier |
| `status` | string | Yes | `exported`, `not_found`, or `failed` |
| `artifact_ref` | string/null | No | Exported artifact reference when successful |
| `output_format` | string/null | No | Export format token |
| `error_code` | string/null | No | Failure reason |

`end_session` request:

| Field | Type | Required | Description |
|---|---|---|---|
| `session_id` | string | Yes | Existing session identifier |
| `mode` | string | Yes | `finalize` or `discard` |

`end_session` response:

| Field | Type | Required | Description |
|---|---|---|---|
| `session_id` | string | Yes | Session identifier |
| `status` | string | Yes | `completed`, `discarded`, `not_found`, or `failed` |
| `error_code` | string/null | No | Failure reason |

---

Prototype session-state resource policy:

- `pose_stream_ref` SHOULD preserve the backend-native trajectory file path or URI as the authoritative reference.
- When `pose_stream_ref` is represented as an object, the object SHOULD include at minimum `backend`, `path`, and a viewer-friendly pose summary.
- `map_state_ref` SHOULD preserve the backend-native map snapshot file path or URI as the authoritative reference.
- When `map_state_ref` is represented as an object, the object SHOULD include at minimum `backend`, `path`, `frame_count`, and `point_count`.
- Derived pose arrays or point summaries MAY be included for live visualization, but they SHALL NOT replace the file reference unless a future revision explicitly freezes a different resource contract.

### 3.6 Communication Link Separation Contract

The system operates two distinct communication link roles. Each link has an independent health state and carries a distinct traffic class.

#### 3.6.1 Link Role Definitions

| Link Role       | Identifier Token | Traffic Class                                                                 |
|-----------------|------------------|-------------------------------------------------------------------------------|
| LoRa Telemetry  | `LORA`           | Heartbeat, housekeeping (HK), status, fault/event reports, uplink commands    |
| Image / Video   | `IMG_VID`        | Image frames, video streams, large payload transfers, reconstruction artifacts |

#### 3.6.2 Link Health State

Each link SHALL independently report its health state using the following values:

| State      | Meaning                                                                 |
|------------|-------------------------------------------------------------------------|
| `ALIVE`    | Link is active and receiving valid updates within the configured window  |
| `DEGRADED` | Link has not received a valid update within the degraded threshold       |
| `LOST`     | Link has not received a valid update within the lost threshold           |

- The LoRa link health state SHALL be managed by `telemetry_app` using `TELEMETRY_STATUS_MID (0x1903)` with `link_role = LORA`.
- The image/video link health state SHALL be managed by a dedicated monitor (e.g., `img_link_app` or an equivalent component) and published as a separate SB message with `link_role = IMG_VID`.
- The health state of one link SHALL NOT be inferred from or overridden by the health state of the other link.
- Both link health states SHALL be independently configurable for degraded and lost thresholds.

#### 3.6.3 Timestamp Consistency Rule

All messages on both link roles SHALL carry a vehicle-generated `cFS_TIME` timestamp as the authoritative event time:

- The `timestamp` field in every downlink and uplink message SHALL be set at the point of creation on the vehicle using cFS_TIME.
- Image and video metadata messages SHALL use the same cFS_TIME basis as LoRa telemetry messages.
- Ground-side reception time MAY be recorded in a separate `rx_timestamp` field for diagnostic purposes but SHALL NOT replace `timestamp` for event correlation.
- Consumers SHALL NOT assume that messages with similar ground reception times describe simultaneous vehicle events. The vehicle-generated `timestamp` SHALL be used for temporal ordering and correlation.

#### 3.6.4 Cross-Link Correlation Fields

To allow ground-side consumers to associate LoRa status data with image/video data from the same vehicle event, the following correlation fields SHALL be used consistently:

| Field      | Type   | Scope                                                                                  |
|------------|--------|----------------------------------------------------------------------------------------|
| `image_id` | string | Unique identifier for a single image frame. SHALL be identical in `IMAGE_META_MID` and any LoRa status message referencing the same capture event. |
| `frame_id` | string | Coordinate frame or session frame identifier. SHALL be consistent across all messages describing the same spatial context. |
| `job_id`   | string | Reconstruction job identifier. SHALL be identical in the reconstruction request, result, and any LoRa status message referencing the same job. |
| `seq`      | uint32 | Monotonically increasing sequence number within a source module. MAY be used to detect gaps or reordering across both link paths. |

Correlation rules:

- When a LoRa status message and an image/video metadata message describe the same vehicle event, they SHALL carry matching `image_id`, `job_id`, or `seq` values.
- `image_id` SHALL be assigned by `img_app` at image capture time and SHALL be propagated unchanged through `IMAGE_META_MID`, reconstruction requests, and any associated LoRa event messages.
- `job_id` SHALL be assigned by the ground-side reconstruction client and SHALL be echoed back in all reconstruction response and status messages.
- `seq` SHALL be assigned by the originating module and SHALL NOT be reassigned by relay or bridge components.

---

## 4. Coordinate System Rules

*(시스템 공통 좌표계 정의는 01-system-requirements.md를 따른다.)*

- Axis definitions: TBD (01-system-requirements.md에서 확정)
- Origin definition: TBD
- Rotation convention: TBD
- Handedness: TBD

---

## 5. Units

모든 모듈은 아래 단위 정의를 준수 SHALL.

| Quantity | Unit     | Notes                                          |
|----------|----------|------------------------------------------------|
| Distance | cm       | UWB ranging 입력 및 Trilateration 입력         |
| Position | cm       | Position_Result x, y, z 필드                  |
| Residual | cm       | 절대 오차 평균값                               |
| GPS altitude | m    | GPS_Message.altitude_m 필드                    |
| Angular rate | rad/s | IMU_Message.angular_rate_xyz 필드 (하드웨어 캘리브레이션 의존) |
| Angle    | TBD      | (01-system-requirements.md에서 확정)           |
| Time     | cFS_TIME | 시스템 공통 타임스탬프 기준                    |

---

## 6. Timestamp Standard

- **Timestamp source**: cFS_TIME (cFS 시스템 클럭)
- **Reference clock**: TBD (01-system-requirements.md에서 확정)
- **Time zone handling**: N/A (절대 시각 기준)
- **Synchronization tolerance**: TBD

Timestamp origin rule:

- All downlink and uplink messages SHALL carry a **vehicle-generated** cFS_TIME timestamp set at the point of creation on the vehicle. This is the authoritative event time for all cross-link and cross-module correlation.
- Image and video metadata messages SHALL use the same cFS_TIME basis as LoRa telemetry messages. There SHALL be no separate time basis for the image/video link.
- Ground-side reception time MAY be recorded in a separate `rx_timestamp` field for diagnostic purposes but SHALL NOT replace the vehicle-generated `timestamp` field for event ordering or correlation.
- Consumers SHALL use the vehicle-generated `timestamp` — not ground reception time — when correlating LoRa status data with image/video data from the same event.

Position_Result의 timestamp는 Distance_Set 내 마지막 거리값 수신 시각
(cFS_TIME)을 사용한다. *(상세 규칙은 04-uwb-requirements.md UWB-OUT-09 참조)*

---

## 7. Error Propagation

### 7.1 Error_Code 열거형 (UWB 모듈)

| Code | 이름                | 발생 조건                                                        |
|------|---------------------|------------------------------------------------------------------|
| 0x00 | `NONE`              | 정상 (valid 출력)                                               |
| 0x01 | `MISSING_DISTANCE`  | 30 ms 대기 후에도 하나 이상의 Anchor 거리값 누락                |
| 0x02 | `NON_POSITIVE_RANGE`| 수신된 거리값이 0 이하                                          |
| 0x03 | `NUMERIC_FAILURE`   | 수치 계산 단계 실패 (NaN/inf, 음수 루트, 행렬 분해 실패 등)    |
| 0x04 | `GEOMETRY_INVALID`  | Anchor 배치가 동일점 또는 거의 일직선으로 XY 평면 정의 불가    |
| 0x05 | `Z_INVALID`         | 두 Z 해가 모두 음수                                             |

### 7.2 NaN / -1 사용 규칙

- float 필드에서 계산 불가 또는 결측 상태는 `NaN`으로 표현한다.
- `distances[i]` 필드에서 해당 Anchor 거리값이 누락된 경우 `-1`로 표현한다.
- 소비 모듈은 `valid == False`인 경우 `position` 필드를 사용하지 않아야 한다.

### 7.3 Error 전파 규칙

| Error Type          | Detection Point     | Propagation Method          | Recovery Expectation         |
|---------------------|---------------------|-----------------------------|------------------------------|
| MISSING_DISTANCE    | UWB 모듈            | Position_Result.error_code  | 다음 사이클에서 재시도       |
| NON_POSITIVE_RANGE  | UWB 모듈            | Position_Result.error_code  | 다음 사이클에서 재시도       |
| NUMERIC_FAILURE     | UWB 모듈            | Position_Result.error_code  | 다음 사이클에서 재시도       |
| GEOMETRY_INVALID    | UWB 모듈            | Position_Result.error_code  | Anchor 배치 점검 필요        |
| Z_INVALID           | UWB 모듈            | Position_Result.error_code  | 다음 사이클에서 재시도       |

---

## 8. Interface Compatibility Rules

- Backward compatibility policy: TBD
- Versioning policy: TBD
- Required validation checks: 소비 모듈은 `valid` 필드를 확인한 후
  `position` 필드를 사용 SHALL.

---

## 9. Open Items

- OI-IFC-01: 좌표계 정의(Section 4)를 01-system-requirements.md와 동기화 필요
- OI-IFC-02: Angle 단위 확정 필요
- OI-IFC-03: cFS_TIME 기준 클럭 및 동기화 허용 오차 확정 필요
- OI-IFC-04: message_id 및 source 필드의 열거형 값 정의 필요
- OI-IFC-05: Reconstruction fixed-frame visualization payload의 필수/선택 필드 동결 필요 (Section 3.3)
- OI-IFC-06: Accumulated map manifest schema and map update operation payloads need final freeze after prototype validation (Section 3.5)
- OI-IFC-07: Sequence / Session Map State contract (Section 3.5A) needs final freeze after MASt3R-SLAM prototype validation, including session lifecycle rules, operation request/response schemas, and the relationship between session export and Section 3.5 archive path.
- OI-IFC-08: Image/video link health monitor app name and MID assignment need to be finalized (Section 3.6.2).
- OI-IFC-09: `rx_timestamp` field format and optional inclusion policy need to be defined for ground-side diagnostic use (Section 3.6.3).
- OI-IFC-10: `seq` field rollover behavior and per-source reset policy need to be defined (Section 3.6.4).
