# 03. Interface Specification

## 1. 목적

모듈 간 인터페이스 계약을 정의한다.
각 모듈은 이 문서에 정의된 메시지 구조, 단위, 타임스탬프 기준,
Error_Code 체계를 준수해야 한다.

---

## 2. 메시지 형식

모든 모듈 간 메시지는 아래 공통 구조를 따른다.

| Field        | Type     | Required | 설명                                                         |
|--------------|----------|----------|--------------------------------------------------------------|
| `message_id` | uint32   | Yes      | 고유 메시지 식별자                                           |
| `timestamp`  | cFS_TIME | Yes      | 페이로드에 연관된 **기체 생성 시각** (vehicle-generated)     |
| `source`     | uint8    | Yes      | 생성 모듈 식별자                                             |
| `payload`    | (구조체) | Yes      | 메시지 본문                                                  |
| `status`     | uint8    | No       | 처리 상태 또는 오류 상태                                     |
| `seq`        | uint32   | No       | 메시지 생성 순서 번호. 동일 source 내에서 단조 증가해야 한다 |
| `frame_id`   | string   | No       | 좌표계 또는 세션 프레임 식별자                               |
| `job_id`     | string   | No       | Reconstruction 작업과 연관된 경우 해당 작업 식별자           |

상관 식별자 필드 규칙:

- `seq`는 하나의 source module 안에서 session 전체에 걸쳐 단조 증가해야 한다. 소비자는 `seq`를 이용해 누락이나 재정렬을 감지할 수 있다.
- `frame_id`, `job_id`는 각각 좌표계 frame과 reconstruction job에 연관된 이벤트를 설명하는 모든 메시지에 채워져야 한다.
- LoRa status 메시지와 image/video 메타데이터 메시지가 동일 차량 이벤트를 설명하는 경우, 지상국 correlation을 위해 동일한 `job_id` 또는 `seq` 값을 가져야 한다.
- control/health 경로에 대해서는 지상국 소비자가 차량에서 생성한 `timestamp` field를 권위 있는 event time으로 사용해야 한다. image/video 경로의 authoritative timestamp policy는 현재 baseline에서 확정되지 않았으며, 최종 정책이 정해지기 전까지는 문서화된 ground-side receive-time approximation을 사용할 수 있다.

### 2.1 통신 링크 역할 계약

cFS 경계에서 통신 역할 분리를 유지하기 위해, 각 메시지는 아래 논리적 link role 중 하나를 명시해야 한다.

| Link Role Token | 설명 | 허용되는 트래픽 종류 |
|---|---|---|
| `CONTROL_HEALTH_LINK` | LoRa control/health path | heartbeat, status, housekeeping, fault/event summaries, uplink commands |
| `PAYLOAD_LINK` | Image/video payload path | image frames, video streams, reconstruction artifact transfers |

역할 규칙:

- `CONTROL_HEALTH_LINK` traffic는 명시적으로 문서화된 fallback policy가 없는 한 `PAYLOAD_LINK`로 라우팅되어서는 안 된다.
- `PAYLOAD_LINK` traffic 역시 명시적으로 문서화된 fallback policy가 없는 한 `CONTROL_HEALTH_LINK`로 라우팅되어서는 안 된다.
- 모듈별 명명으로 매핑할 때 `CONTROL_HEALTH_LINK`는 `LORA`, `PAYLOAD_LINK`는 `IMG_VID`에 대응한다.

---

## 3. Input / Output Data Definitions

### 3.2A 입력 데이터(GPS / IMU / Pose Alignment)

| Name | Source | Format | Notes |
|---|---|---|---|
| `GPS_Message` | GPS receiver | latitude, longitude, altitude, timestamp | 원시 전역 위치 데이터. local ENU/NED 변환은 alignment 정책에서 처리한다. |
| `IMU_Message` | IMU sensor | attitude, angular_rate, acceleration, timestamp | Body-frame motion/orientation 입력. 축 기준은 하드웨어 calibration에 따라 달라진다. |
| `Camera_Pose_Message` | Camera or vision pipeline | pose, timestamp | 사용 가능한 경우 선택적으로 제공되는 camera pose 추정값. |
| `Frame_Transform_Message` | Calibration/config | source_frame, target_frame, transform, validity | alignment에 사용되는 정적 또는 동적 transform 메타데이터. |

### 3.2AA 입력 데이터(Flight Controller MAVLink)

MAVLink Bridge Module은 serial/UART 인터페이스를 통해 Flight Controller로부터 raw MAVLink 바이트 스트림을 수신하고, 이를 형식화된 cFS SB 메시지로 파싱한다. Raw MAVLink frame은 cFS Software Bus에 직접 올려서는 안 된다.

`mavlink_bridge_app`이 수용하는 baseline MAVLink 메시지 입력은 다음과 같다.

| MAVLink Message | MAVLink ID | Notes |
|---|---|---|
| `LOCAL_POSITION_NED` | #32 | NED frame 기준 FC 추정 local 위치 및 속도 |
| `ATTITUDE` | #30 | FC 추정 roll, pitch, yaw 및 각속도 |
| `GPS_RAW_INT` | #24 | FC GPS 수신기에서 제공하는 원시 GPS fix 데이터 |
| `ODOMETRY` | #331 | FC EKF에서 제공 가능한 경우의 전체 6-DOF odometry |
| `EKF_STATUS_REPORT` | #193 | FC EKF health flag 및 분산 추정값 |

입력 source 규칙:

- `mavlink_bridge_app`은 설정된 serial device path에서 MAVLink 바이트 스트림을 수신해야 한다.
- `mavlink_bridge_app`은 추가 메시지가 configuration 또는 향후 개정으로 명시적으로 추가되지 않는 한, 위에 나열한 baseline MAVLink 메시지 집합만 파싱해야 한다.
- 인식되지 않거나 지원되지 않는 MAVLink message ID는 조용히 폐기해야 하며, 이후 메시지 파싱 실패의 원인이 되어서는 안 된다.
- `mavlink_bridge_app`은 raw MAVLink 바이트 frame을 cFS Software Bus로 그대로 전달해서는 안 된다.

### 3.2AB 출력 데이터(MAVLink Bridge SB 메시지)

`mavlink_bridge_app`은 파싱된 MAVLink 입력으로부터 아래 cFS SB 메시지를 publish해야 한다. 모든 publish 메시지는 publish 시점에 부여된 vehicle-generated `cFS_TIME` timestamp를 포함해야 한다.

| MID | Message Name | Source MAVLink | Fields | Notes |
|---|---|---|---|---|
| `0x1905` | `FC_LOCAL_POS_MID` | `LOCAL_POSITION_NED` | `x`, `y`, `z` (m, NED), `vx`, `vy`, `vz` (m/s), `timestamp` | FC가 추정한 local 위치 및 속도 |
| `0x1906` | `FC_ATTITUDE_MID` | `ATTITUDE` | `roll`, `pitch`, `yaw` (rad), `rollspeed`, `pitchspeed`, `yawspeed` (rad/s), `timestamp` | FC가 추정한 자세 및 각속도 |
| `0x1907` | `FC_GPS_RAW_MID` | `GPS_RAW_INT` | `lat`, `lon` (degE7), `alt` (mm), `fix_type`, `satellites_visible`, `timestamp` | FC에서 전달된 원시 GPS fix |
| `0x1908` | `FC_ODOMETRY_MID` | `ODOMETRY` | `x`, `y`, `z` (m), `q[4]` (quaternion), `vx`, `vy`, `vz` (m/s), `timestamp` | 사용 가능한 경우의 전체 6-DOF odometry |
| `0x1909` | `FC_EKF_STATUS_MID` | `EKF_STATUS_REPORT` | `flags`, `velocity_variance`, `pos_horiz_variance`, `pos_vert_variance`, `timestamp` | FC EKF health 및 분산 정보 |
| `0x190A` | `MAVLINK_BRIDGE_STATUS_MID` | (bridge internal) | `link_state`, `parse_error_count`, `last_msg_age_ms`, `timestamp` | Bridge health 및 파싱 진단 정보 |

출력 field 규칙:

- 이 절의 모든 MID 값은 잠정값이며 최종 할당 대상이다(see `OI-CFS-01`).
- `FC_LOCAL_POS_MID`, `FC_ATTITUDE_MID`, `FC_GPS_RAW_MID`는 baseline 필수 출력이다. `FC_ODOMETRY_MID`와 `FC_EKF_STATUS_MID`는 해당 MAVLink 메시지가 수신된 경우에만 publish한다.
- `MAVLINK_BRIDGE_STATUS_MID`는 incoming MAVLink traffic과 독립된 설정 가능한 주기로 publish해야 한다.
- `MAVLINK_BRIDGE_STATUS_MID.link_state`는 Section 3.6.2의 link health 분류와 일치하는 `ALIVE`, `DEGRADED`, `LOST` 값을 사용해야 한다.
- Pose / Alignment Module은 `FC_LOCAL_POS_MID`, `FC_ATTITUDE_MID`, `FC_ODOMETRY_MID`, `FC_EKF_STATUS_MID`를 선택적인 FC 유래 alignment 입력으로 subscribe할 수 있다.

### 3.2B Baseline cFS SB 입력 집합

현재 cFS 배치에서 요구되는 baseline SB 입력 집합은 다음과 같다.

| MID | Message Name | Publisher | Purpose |
|---|---|---|---|
| `0x1901` | `IMU_STATE_MID` | `imu_app` | alignment용 baseline IMU 입력 |
| `0x1902` | `GPS_STATE_MID` | `gps_app` | alignment용 baseline GPS 입력 |
| `0x1903` | `TELEMETRY_STATUS_MID` | `telemetry_app` | baseline 통신 링크 health 입력 |
| `0x1905` | `FC_LOCAL_POS_MID` | `mavlink_bridge_app` | FC가 추정한 local 위치 및 속도(NED) |
| `0x1906` | `FC_ATTITUDE_MID` | `mavlink_bridge_app` | FC가 추정한 자세 및 각속도 |
| `0x1907` | `FC_GPS_RAW_MID` | `mavlink_bridge_app` | FC에서 전달되는 원시 GPS fix 데이터 |
| `0x1908` | `FC_ODOMETRY_MID` | `mavlink_bridge_app` | 사용 가능한 경우 FC EKF의 전체 6-DOF odometry |
| `0x1909` | `FC_EKF_STATUS_MID` | `mavlink_bridge_app` | FC EKF health flag 및 분산 추정값 |
| `0x190A` | `MAVLINK_BRIDGE_STATUS_MID` | `mavlink_bridge_app` | MAVLink bridge 링크 health 및 파싱 진단 정보 |

`TELEMETRY_STATUS_MID.link_state`는 `ALIVE`, `DEGRADED`, `LOST` 값을 사용해야 한다.

`MAVLINK_BRIDGE_STATUS_MID.link_state`는 Section 3.6.2와 일치하도록 `ALIVE`, `DEGRADED`, `LOST` 값을 사용해야 한다.

`FC_ODOMETRY_MID`와 `FC_EKF_STATUS_MID`는 해당 MAVLink 메시지를 FC에서 수신한 경우에만 publish되는 조건부 출력이다.

`IMU_Message`와 `GPS_Message`의 상세 field 구조는 Section 3.2A에 정의되어 있다.
MAVLink Bridge 출력의 상세 field 구조는 Section 3.2AB에 정의되어 있다.

### 3.2C Telemetry Monitor 입력 계약

`telemetry_app`의 baseline telemetry monitor 입력 계약은 다음과 같다.

| Field | Type | Required | 설명 |
|---|---|---|---|
| `active_transport_id` | uint8 | Yes | 평가 대상 transport 경로 식별자 |
| `valid` | bool | Yes | monitor update가 link-state 평가에 유효하면 `True` |
| `update_age_ms` | uint32 | Yes | 가장 최근의 유효한 transport activity indication으로부터 경과한 시간(ms) |

Telemetry monitor 입력 규칙:

- monitor-input producer는 승인된 각 telemetry monitor update마다 `active_transport_id`, `valid`, `update_age_ms`를 제공해야 한다.
- `update_age_ms`는 link-health 평가에 사용한 가장 최근의 유효한 transport activity indication 이후 경과 시간을 나타내야 한다.
- `telemetry_app`은 `valid == False`인 monitor update를 nominal link-state 갱신용으로 유효하지 않은 값으로 취급해야 한다.
- `telemetry_app`은 설정된 active transport identifier와 `update_age_ms`를 함께 사용하여 link state를 `ALIVE`, `DEGRADED`, `LOST`로 분류해야 한다.

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
| `camera_trajectory[].source_path` | string | Yes   | 디버그/검증용 원본 이미지 경로 또는 동등한 source reference |
| `camera_trajectory[].position` | float64[3] | Yes | 고정 좌표계 기준 카메라 위치 |
시각화 위치 규칙:

- Reconstruction viewer 위치는 transform이 명시적으로 scale을 변환하지 않는 한 artifact/backend 좌표 단위를 그대로 보존해야 한다.
- 여러 source를 함께 렌더링할 때 viewer는 transform 메타데이터를 표시해야 하며, alignment status가 `ALIGNED`를 가리키지 않는 한 metric consistency를 암시해서는 안 된다.
- 이 절의 `transform.translate`는 Section 3.5에 정의된 accumulated map manifest transform 객체의 `translate[3]`에 대응한다.

---

### 3.4 Reconstruction Remote Execution API (Prototype)

현재 prototype 원격 reconstruction transport는 HTTP polling이다.
이 절은 최종 transport가 확정되기 전까지 사용할 임시 계약을 정의한다.

#### 3.4.1 Submit Job

`POST /jobs`

요청 payload:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `job_id` | string | Yes | client가 생성한 reconstruction job 식별자 |
| `image_set_id` | string | Yes | 논리적 image set 식별자 |
| `images[]` | list | Yes | 순서가 보존된 이미지 descriptor 목록 |
| `images[].timestamp` | cFS_TIME | Yes | 현재는 backend 또는 bridge가 제공하는 image acquisition 또는 ingestion 기준 timestamp. 최종 authoritative policy는 미정 |
| `images[].source_path` | string | Yes | server가 읽을 수 있는 이미지 경로 또는 URI |
| `output_format` | string | Yes | 요청한 외부 출력 형식, 현재는 주로 `glb` |
| `aux_pose` | object/null | No | 선택적 camera pose 또는 localization 보조 정보 |
| `extra` | object | No | 하위 호환을 위한 확장 field |

응답 payload:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `job_id` | string | Yes | 수락된 job 식별자 |
| `status` | string | Yes | 초기 job 상태 |
| `poll_url` | string | Yes | job 상태를 polling하기 위한 URL 경로 |
| `artifact_url` | string | Yes | 완료된 artifact를 내려받기 위한 URL 경로 |

#### 3.4.2 Poll Job

`GET /jobs/{job_id}`

응답 payload는 `ReconstructionResponse`를 따라야 한다:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `job_id` | string | Yes | job 식별자 |
| `status` | string | Yes | `pending`, `success`, `degraded`, `failed`, `timeout` 중 하나 |
| `result_ref` | string/null | No | server 측 artifact 참조값 |
| `output_format` | string/null | No | artifact format 토큰 |
| `quality_meta` | object | Yes | reconstruction 품질 메타데이터 |
| `error_code` | string/null | No | failure/degraded 사유 코드 |
| `processing_duration_s` | float/null | No | 원격 처리 시간 |
| `completed_at` | string/null | No | 완료 시각 |
| `extra` | object | No | 하위 호환을 위한 확장 field |

#### 3.4.3 Download Artifact

`GET /jobs/{job_id}/artifact`

server는 사용 가능한 경우 완료된 reconstruction artifact를 binary 데이터로 반환해야 한다.
client는 해당 artifact를 로컬에 저장하고, 저장된 local artifact 경로를 fixed-frame visualization 또는 downstream integration 경로로 전달해야 한다.

---

### 3.5 Accumulated Map Manifest (Prototype)

accumulated map manifest는 여러 reconstruction artifact가 map chunk로
추적되는 방식을 설명한다. 이는 지상국 측 데이터 계약이며,
raw reconstruction artifact의 수정을 요구해서는 안 된다.

| Field | Type | Required | 설명 |
|---|---|---|---|
| `map_id` | string | Yes | 영속적으로 유지되는 accumulated map 식별자 |
| `created_at` | cFS_TIME | Yes | manifest 생성 시각 |
| `updated_at` | cFS_TIME | Yes | 마지막 manifest 갱신 시각 |
| `display_frame_id` | string | Yes | viewer/display frame 식별자, 예: `enu`, `world`. 단일 artifact viewer 출력에서는 Section 3.3의 `frame_id`에 대응한다. |
| `chunks[]` | list | Yes | append 순서대로 정렬된 reconstruction chunk 목록. 단, renderer가 다른 문서화된 기준으로 명시적으로 정렬하는 경우는 예외다. |
| `chunks[].chunk_id` | string | Yes | 고유 chunk 식별자 |
| `chunks[].job_id` | string | Yes | 원본 reconstruction job 식별자 |
| `chunks[].image_set_id` | string | Yes | 원본 image set 식별자 |
| `chunks[].artifact_ref` | string | Yes | 성공적으로 내려받은 뒤 지상국에 저장된 local artifact 경로 |
| `chunks[].output_format` | string | Yes | artifact format 토큰, 예: `ply`, `glb` |
| `chunks[].alignment_status` | string | Yes | `ALIGNED`, `PARTIAL_ALIGNMENT`, or `UNALIGNED` |
| `chunks[].source_frame_id` | string | Yes | 원본 artifact/reconstruction frame 식별자. 더 구체적인 backend frame이 없으면 단일 artifact viewer 출력에서는 Section 3.3의 `frame_id`에서 유도한다. |
| `chunks[].target_frame_id` | string | No | alignment가 가능한 경우의 target frame |
| `chunks[].transform` | object/null | Conditional | `alignment_status`가 `ALIGNED` 또는 `PARTIAL_ALIGNMENT`일 때 필수이며, `UNALIGNED`일 때는 `null`이어야 한다. |
| `chunks[].quality_meta` | object | No | reconstruction 품질 메타데이터 |
| `chunks[].source_images[]` | list | No | 해당 chunk에 사용된 이미지 식별자 또는 경로 |

Transform 객체:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `scale` | float64 | Yes | 균일 scale 계수 |
| `linear[3][3]` | float64[3][3] | Yes | 회전 또는 선형 transform |
| `translate[3]` | float64[3] | Yes | 평행이동 벡터 |
| `timestamp_basis` | string | No | alignment에 사용한 sensor timestamp 기준 |
| `calibration_status` | string | No | calibration 유효 상태 |

Transform 적용 순서:

```text
p_target = scale * (linear @ p_source) + translate
```

`linear`는 column-vector 규약으로 적용한다. row-vector 수학을 사용하는 구현은
동등한 결과를 만들어야 한다.

Map 갱신 작업:

| Operation | Required Inputs | Expected Behavior |
|---|---|---|
| `append_chunk` | map_id, artifact_ref, job_id, image_set_id, output_format | raw artifact를 수정하지 않고 새 chunk를 추가한다 |
| `update_chunk_transform` | map_id, chunk_id, transform, alignment_status | 기존 chunk의 alignment 메타데이터를 갱신한다 |
| `invalidate_chunk` | map_id, chunk_id, reason | raw artifact를 삭제하지 않고 기존 chunk를 무효 상태로 표시한다 |
| `render_map` | map_id, display_frame_id | 정렬된 chunk와 정렬되지 않은 chunk를 구분하여 사용 가능한 모든 chunk를 렌더링한다 |

`append_chunk` 요청:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `map_id` | string | Yes | 대상 accumulated map 식별자 |
| `job_id` | string | Yes | 원본 reconstruction job 식별자 |
| `image_set_id` | string | Yes | 원본 image set 식별자 |
| `artifact_ref` | string | Yes | 지상국 측 local artifact 경로 |
| `output_format` | string | Yes | artifact format 토큰 |
| `source_frame_id` | string | Yes | 원본 reconstruction frame 식별자 |
| `quality_meta` | object | No | reconstruction 품질 메타데이터 |
| `source_images[]` | list | No | 원본 이미지 식별자 또는 경로 |

`append_chunk` 응답:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `map_id` | string | Yes | 대상 map 식별자 |
| `chunk_id` | string | Yes | 생성된 chunk 식별자 |
| `status` | string | Yes | `appended`, `duplicate_rejected`, `failed` 중 하나 |
| `error_code` | string/null | No | 실패 또는 중복 사유 |

`update_chunk_transform` 요청:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `map_id` | string | Yes | 대상 accumulated map 식별자 |
| `chunk_id` | string | Yes | 기존 chunk 식별자 |
| `alignment_status` | string | Yes | `ALIGNED`, `PARTIAL_ALIGNMENT`, or `UNALIGNED` |
| `target_frame_id` | string | Conditional | transform가 `null`이 아닐 때 필수 |
| `transform` | object/null | Conditional | `ALIGNED` 또는 `PARTIAL_ALIGNMENT`일 때 필수이며, `UNALIGNED`일 때는 `null` |
| `updated_by` | string | No | producer module, 예: `pose_frame_alignment` |

`update_chunk_transform` 응답:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `map_id` | string | Yes | 대상 map 식별자 |
| `chunk_id` | string | Yes | 갱신된 chunk 식별자 |
| `status` | string | Yes | `updated`, `not_found`, `failed` 중 하나 |
| `error_code` | string/null | No | 실패 사유 |

### 3.5A Sequence / Session Map State (Prototype Direction)

For sequence-based SLAM backends, the preferred primary output is an evolving
session map state rather than a set of independently aligned artifact chunks.
This section defines the additional contract that may coexist with Section 3.5.
아래에서 별도로 지정하지 않는 한, 이 절의 `ordered frames[]`는
Section 3.4 `images[]`와 동일한 image descriptor 구조를 재사용해야 한다.
즉 `timestamp`, `source_path`, 그리고 `extra`의 선택적 확장 필드를 그대로 사용한다.
`image_sequence_id`는 지상국 경로가 제공하는 논리적 ordered-frame 집합 식별자이다.
`session_config`는 backend 선택, 출력/내보내기 정책, 그리고 구현에서 정의한
sequence-mode runtime configuration을 담는 객체이다.

| Field | Type | Required | 설명 |
|---|---|---|---|
| `session_id` | string | Yes | Long-lived sequence processing session identifier |
| `status` | string | Yes | `active`, `completed`, `failed`, `exported` 중 하나 |
| `frame_count` | uint32 | Yes | Number of accepted frames in the session |
| `keyframe_count` | uint32 | No | Number of keyframes retained by the backend when that concept is available |
| `rendered_point_count` | uint32 | No | Number of points currently exposed for visualization or diagnostic export when available |
| `pose_stream_ref` | string or object | Yes | 세션 camera trajectory 또는 pose stream에 대한 참조. 현재 prototype에서는 backend-native `.txt` trajectory file을 권위 있는 pose-stream artifact로 취급하며, 정규화된 pose list는 참고용으로만 사용한다. |
| `map_state_ref` | string or object | Yes | 현재 dense/sparse map state에 대한 참조. 현재 prototype에서는 backend-native `.ply` snapshot file을 권위 있는 map-state artifact로 취급한다. |
| `current_frame_ref` | string/null | No | Most recent accepted frame path or URI when available |
| `exported_artifact_ref` | string/null | No | 오프라인 확인을 위한 선택적 exported artifact |
| `alignment_status` | string | Yes | session map의 World/Map alignment 상태. 상태값은 06-pose-frame-alignment-requirements.md의 ALIGN-OUT-05 정의를 따라야 한다. |
| `world_transform` | object/null | Conditional | Session-to-World transform when available |
| `tracking_state` | string | No | `initializing`, `tracking`, `relocalizing`, `completed` 또는 구현 정의 동등값과 같은 session runtime tracking 상태 |
| `last_updated` | cFS_TIME | No | 최신 session-state 갱신 시각 |

Session lifecycle rules:

- `active` 상태의 session은 추가 `append_frames` 요청을 수락할 수 있다.
- `completed`, `failed`, `exported` 상태의 session은 향후 개정에서 명시적인 reopen policy가 정의되지 않는 한 추가 `append_frames` 요청을 거부해야 한다.
- finalize 의미의 `end_session`은 session을 `completed` 상태로 전이시켜야 한다.
- discard 의미의 `end_session`은 session을 `failed` 또는 구현이 정의한 terminal discard 상태로 전이시키고, 이후 frame append를 받을 수 없도록 해야 한다.

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

- Section 3.5는 독립적인 artifact chunk와 영속적 diagnostic artifact manifest에 대한 계약으로 유지된다.
- Section 3.5A는 연속적인 SLAM/session 출력에 대한 우선적인 기본 계약이다.
- `export_session_artifact`는 오프라인 확인, 보관, 혼합 모드 렌더링을 위해 나중에 Section 3.5 manifest에 `append_chunk`로 삽입될 artifact를 생성할 수 있다.
- 동일 session의 SLAM 갱신은 모든 증분 map update마다 반복적인 `append_chunk` 작업을 요구해서는 안 된다. 이러한 갱신은 명시적인 export 또는 snapshot 정책이 호출되기 전까지 session-state 경로 안에 남아 있어야 한다.

`start_session` request:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `image_sequence_id` | string | Conditional | Required when starting a session from a named sequence source |
| `session_config` | object | Conditional | Required when start behavior depends on runtime configuration |
| `session_config.backend_name` | string | No | Requested sequence backend, e.g. `sequence_slam_backend` |
| `session_config.output_policy` | string | No | `session_state_only`, `session_plus_export` 또는 구현 정의 동등값. `session_plus_export`는 session이 성공적으로 종료될 때 구현이 `export_session_artifact`와 동등한 의미로 diagnostic artifact를 자동으로 export해야 함을 뜻한다. |
| `session_config.extra` | object | No | Forward-compatible runtime configuration |

`start_session` response:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `session_id` | string | Yes | Created session identifier |
| `status` | string | Yes | `active` or `failed` |
| `error_code` | string/null | No | Failure reason when session start is rejected |

`append_frames` request:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `session_id` | string | Yes | Existing session identifier |
| `ordered_frames[]` | list | Yes | Ordered frame descriptors reusing the Section 3.4 image descriptor structure |

`append_frames` response:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `session_id` | string | Yes | Target session identifier |
| `status` | string | Yes | `accepted`, `session_not_found`, `session_closed`, or `failed` |
| `frame_count` | uint32 | No | Updated accepted frame count when available |
| `error_code` | string/null | No | Failure reason |

`get_session_state` response:

| Field | Type | Required | 설명 |
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
| `last_updated` | cFS_TIME | No | 최신 session-state 갱신 시각 |
| `error_code` | string/null | No | Failure reason |

`update_session_transform` request:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `session_id` | string | Yes | Existing session identifier |
| `alignment_status` | string | Yes | `ALIGNED`, `PARTIAL_ALIGNMENT`, or `UNALIGNED` |
| `world_transform` | object/null | Conditional | Required for `ALIGNED` or `PARTIAL_ALIGNMENT`; null for `UNALIGNED` |
| `updated_by` | string | No | Producer module, e.g. `pose_frame_alignment` |

`update_session_transform` response:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `session_id` | string | Yes | Updated session identifier |
| `status` | string | Yes | `updated`, `not_found`, or `failed` |
| `error_code` | string/null | No | Failure reason |

`export_session_artifact` request:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `session_id` | string | Yes | Existing session identifier |
| `output_format` | string | Yes | Requested export format token |

`export_session_artifact` response:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `session_id` | string | Yes | Session identifier |
| `status` | string | Yes | `exported`, `not_found`, or `failed` |
| `artifact_ref` | string/null | No | Exported artifact reference when successful |
| `output_format` | string/null | No | Export format token |
| `error_code` | string/null | No | Failure reason |

`end_session` request:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `session_id` | string | Yes | Existing session identifier |
| `mode` | string | Yes | `finalize` or `discard` |

`end_session` response:

| Field | Type | Required | 설명 |
|---|---|---|---|
| `session_id` | string | Yes | Session identifier |
| `status` | string | Yes | `completed`, `discarded`, `not_found`, or `failed` |
| `error_code` | string/null | No | Failure reason |

---

Prototype session-state resource policy:

- `pose_stream_ref`는 backend-native trajectory file path 또는 URI를 권위 있는 참조로 보존해야 한다.
- `pose_stream_ref`가 object로 표현되는 경우, 최소한 `backend`, `path`, 그리고 viewer 친화적인 pose summary를 포함해야 한다.
- `map_state_ref`는 backend-native map snapshot file path 또는 URI를 권위 있는 참조로 보존해야 한다.
- `map_state_ref`가 object로 표현되는 경우, 최소한 `backend`, `path`, `frame_count`, `point_count`를 포함해야 한다.
- 파생된 pose 배열이나 point summary는 live visualization을 위해 포함될 수 있지만, 향후 개정에서 다른 자원 계약을 명시적으로 확정하지 않는 한 file reference를 대체해서는 안 된다.

### 3.6 Communication Link Separation Contract

시스템은 서로 구분되는 두 개의 통신 링크 역할을 사용한다. 각 링크는 독립적인 health state를 가지며, 서로 다른 트래픽 종류를 운반한다.

#### 3.6.1 Link Role Definitions

| Link Role       | Identifier Token | Traffic Class                                                                 |
|-----------------|------------------|-------------------------------------------------------------------------------|
| LoRa Telemetry  | `LORA`           | Heartbeat, housekeeping (HK), status, fault/event reports, uplink commands    |
| Image / Video   | `IMG_VID`        | Image frames, video streams, large payload transfers, reconstruction artifacts |

#### 3.6.2 Link Health State

각 링크는 아래 상태값을 사용하여 독립적으로 health state를 보고해야 한다.

| State      | 의미                                                                    |
|------------|-------------------------------------------------------------------------|
| `ALIVE`    | Link is active and receiving valid updates within the configured window  |
| `DEGRADED` | Link has not received a valid update within the degraded threshold       |
| `LOST`     | Link has not received a valid update within the lost threshold           |

- LoRa link health state는 `telemetry_app`이 `TELEMETRY_STATUS_MID (0x1903)`와 `link_role = LORA`를 사용해 관리해야 한다.
- 한 링크의 health state를 다른 링크의 health state로부터 추론하거나 덮어써서는 안 된다.
- 각 링크의 degraded 및 lost threshold는 독립적으로 설정 가능해야 한다.

#### 3.6.3 Timestamp Consistency Rule

control/health 링크의 메시지는 vehicle-generated `cFS_TIME` timestamp를 authoritative event time으로 사용해야 한다. image/video 링크의 timestamp policy는 2026-07-13 확정되었다 (아래).

- 모든 downlink 및 uplink 메시지의 `timestamp` 필드는 차량 측 생성 시점에 cFS_TIME으로 설정되어야 한다.
- **image/video 링크의 authoritative event time은 기체 측 UTC 번인이다** (§6.1 동기 체인 기준): ① 카메라(OpenIPC majestic) OSD 타임스탬프 번인 — 카메라 시계가 NTP 동기된 후 유효, ② FC GPS 시각 OSD 요소(msposd) — GPS fix 즉시 유효. 구현: `cfs-telemetry-app/camera/`.
- ground-side reception time은 진단 목적으로 별도 `rx_timestamp` 필드에 기록할 수 있다. 카메라 시계 동기 체인(§6.1)이 가동되기 전까지의 과도기에는 이 값을 근사 event time으로 사용할 수 있다.
- 소비 모듈은 양 링크 모두 vehicle-generated UTC 기준으로 시간 순서와 상관관계를 판단해야 한다. 크로스 링크 대조(영상 프레임 ↔ LoRa 텔레메트리 로그)는 §6 Synchronization tolerance(~100ms급)를 전제한다.

#### 3.6.4 Cross-Link Correlation Fields

ground-side 소비자가 동일 vehicle event의 LoRa status data와 image/video data를 연계할 수 있도록, 아래 correlation field를 일관되게 사용해야 한다.

| Field      | Type   | Scope                                                                                  |
|------------|--------|----------------------------------------------------------------------------------------|
| `frame_id` | string | 좌표계 frame 또는 session frame 식별자. 동일한 spatial context를 설명하는 모든 메시지에서 일관되어야 한다. |
| `job_id`   | string | Reconstruction job 식별자. 동일한 job을 참조하는 reconstruction request, result, LoRa status message에서 동일해야 한다. |
| `seq`      | uint32 | source module 내부에서 단조 증가하는 sequence 번호. 양 링크 경로에서 누락이나 재정렬을 감지하는 데 사용할 수 있다. |

Correlation rules:

- LoRa status message와 image/video metadata message가 동일 vehicle event를 설명하는 경우, 해당 event class에서 사용 가능한 `job_id` 또는 `seq` 값이 일치해야 한다.
- `job_id`는 ground-side reconstruction client가 할당하며, 모든 reconstruction response 및 status message에서 동일하게 반환되어야 한다.
- `seq`는 원본 모듈이 할당해야 하며, relay나 bridge component가 다시 부여해서는 안 된다.

---

## 4. Coordinate System Rules

*(시스템 공통 좌표계 정의는 01-system-requirements.md를 따른다.)*

- Axis definitions: TBD (01-system-requirements.md에서 확정)
- Origin definition: TBD
- Rotation convention: TBD
- Handedness: TBD

---

## 5. Units

모든 모듈은 아래 단위 정의를 준수해야 한다.

| Quantity | Unit     | Notes                                          |
|----------|----------|------------------------------------------------|
| Residual | cm       | 절대 오차 평균값                               |
| GPS altitude | m    | GPS_Message.altitude_m 필드                    |
| Angular rate | rad/s | IMU_Message.angular_rate_xyz 필드 (하드웨어 캘리브레이션 의존) |
| Angle    | TBD      | (01-system-requirements.md에서 확정)           |
| Time     | cFS_TIME | 시스템 공통 타임스탬프 기준                    |

---

## 6. Timestamp Standard

- **Timestamp source**: cFS_TIME (cFS 시스템 클럭)
- **Reference clock**: **GPS UTC** (2026-07-13 확정 — §6.1 동기 체인 참조)
- **Time zone handling**: 전 구간 UTC 통일 (GPS 시각은 원래 UTC)
- **Synchronization tolerance**: 구간별 상이 — GPS→FC ~ms, FC→CM(Pi) ~수십 ms(MAVLink 지터), 영상 OSD 번인 ~100ms. 크로스 링크 상관관계 판단은 ~100ms급 허용 오차를 전제한다.

Timestamp origin rule:

- 링크별 timestamp 및 correlation rule은 Section 3.6.3과 Section 3.6.4를 따른다.

### 6.1 Reference Clock 동기 체인 (2026-07-13 확정, 시계 도메인 정정)

GPS UTC로 규율할 시계는 **두 도메인**이며 밑바탕 소스가 다르다(한쪽을 맞춰도 다른 쪽은 불변).

```
GPS ─▶ FC ──SYSTEM_TIME(msg2,1Hz)──▶ CM(Pi) mavlink_bridge (파싱·SB 발행)
                                          │
        (A) cFS 내부 ────────────────────┤ CFE_TIME_ExternalGPS (C, cFS 안)
            = SB/LoRa 로그 타임스탬프      │   → cFS 로그가 GPS UTC 축
                                          │
        (B) 리눅스 OS 시계 ───────────────┘ chrony (cFS 밖, 카메라 전용)
            └─이더넷 NTP──▶ 카메라(WiFiLink) OSD 시계
```

| 구간 | 상태 | 상세 spec (단일 원본) |
| --- | --- | --- |
| FC SYSTEM_TIME 수신 파싱 | 구현 완료 | `cfs-telemetry-app/notes/mavlink_bridge_app_behavior_spec.md` §16.2 |
| SB 발행 (`FC_SYS_TIME_MID 0x1909`) | 구현 완료 (2026-07-13, commit `38c2f22`), Pi 실기 검증 미완 | 동 §16.3 |
| (A) cFS 내부 시각 규율 (`CFE_TIME_ExternalGPS`) | 미구현(예정) — C, cFS 안 | 동 §16.4.1 |
| (B) OS 시계 규율 (chrony, 카메라 전용) | 미구현(예정) — cFS 밖 | 동 §16.4.2 |
| 카메라 NTP 동기 | 프로토타입 | `cfs-telemetry-app/camera/pi_chrony_camera.conf` |

- cFS 로그(SB·LoRa)는 도메인 (A), 카메라 OSD는 도메인 (B)에 속한다. 둘 다 같은 GPS UTC로 규율돼야 크로스 링크 대조(§3.6.3)가 성립한다.
- GPS fix 미확보 구간(`time_unix_usec == 0`)에서는 양쪽 모두 동기를 수행하지 않으며, 각 시계는 마지막 동기 시점부터의 드리프트를 허용한다.

## 7. Error Propagation

### 7.1 일반 규칙

- float 필드에서 계산 불가 또는 결측 상태는 `NaN`으로 표현한다.
- 소비 모듈은 status 또는 validity flag가 무효를 나타내는 경우 해당 수치 필드를 사용하지 않아야 한다.

---

## 8. Interface Compatibility Rules

- Backward compatibility policy: TBD
- Versioning policy: TBD
- Required validation checks: 소비 모듈은 `valid` 필드를 확인한 후
  `position` 필드를 사용해야 한다.

---

## 9. Open Items

- OI-IFC-01: 좌표계 정의(Section 4)를 01-system-requirements.md와 동기화 필요
- OI-IFC-02: Angle 단위 확정 필요
- OI-IFC-03: cFS_TIME 기준 클럭 및 동기화 허용 오차 확정 필요
- OI-IFC-04: message_id 및 source 필드의 열거형 값 정의 필요
- OI-IFC-05: Reconstruction fixed-frame visualization payload의 필수/선택 필드 동결 필요 (Section 3.3)
- OI-IFC-06: Accumulated map manifest schema and map update operation payloads need final freeze after prototype validation (Section 3.5)
- OI-IFC-07: Sequence / Session Map State contract (Section 3.5A) needs final freeze after selected sequence-SLAM prototype validation, including session lifecycle rules, operation request/response schemas, and the relationship between session export and Section 3.5 archive path.
- OI-IFC-08: Image/video link health monitor app name and MID assignment need to be finalized (Section 3.6.2).
- OI-IFC-09: `rx_timestamp` field format and optional inclusion policy need to be defined for ground-side diagnostic use (Section 3.6.3).
- OI-IFC-10: `seq` field rollover behavior and per-source reset policy need to be defined (Section 3.6.4).
- OI-IFC-11: MAVLink Bridge SB message MIDs (0x1905–0x190A) are provisional and need final assignment (Section 3.2AB, OI-CFS-01).
- OI-IFC-12: `FC_ODOMETRY_MID` and `FC_EKF_STATUS_MID` conditional publication policy needs to be frozen — specifically whether absence of these messages triggers a degraded status or is silently tolerated (Section 3.2AB).
- OI-IFC-13: MAVLink Bridge serial device configuration contract (device path, baud rate, reconnect policy) needs to be finalized (Section 3.2AA).
- OI-IFC-14: 이미지 단위 correlation key 정책은 현재 baseline에서 미정이다. image capture를 기체 측에서 직접 제어하지 않으므로 per-image identifier를 필수 공통 필드로 고정하지 않는다.
- OI-IFC-15: image/video 경로의 authoritative timestamp policy는 현재 baseline에서 미정이다. `cFS_TIME` 기반 image-origin timestamp를 사용할지, 문서화된 ground-side receive-time approximation을 사용할지는 추후 통합 시험 결과를 반영하여 확정해야 한다.
