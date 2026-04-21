# 03. Interface Specification

## 1. Purpose

모듈 간 인터페이스 계약을 정의한다.
각 모듈은 이 문서에 정의된 메시지 구조, 단위, 타임스탬프 기준,
Error_Code 체계를 준수 SHALL.

---

## 2. Message Format

모든 모듈 간 메시지는 아래 공통 구조를 따른다.

| Field      | Type    | Required | Description                  |
|------------|---------|----------|------------------------------|
| message_id | uint32  | Yes      | 고유 메시지 식별자            |
| timestamp  | cFS_TIME| Yes      | 페이로드에 연관된 시각        |
| source     | uint8   | Yes      | 생성 모듈 식별자              |
| payload    | (구조체)| Yes      | 메시지 본문                   |
| status     | uint8   | No       | 처리 상태 또는 오류 상태      |

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
| `images[].timestamp` | string/number | Yes | Acquisition timestamp |
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
| Angle    | TBD      | (01-system-requirements.md에서 확정)           |
| Time     | cFS_TIME | 시스템 공통 타임스탬프 기준                    |

---

## 6. Timestamp Standard

- **Timestamp source**: cFS_TIME (cFS 시스템 클럭)
- **Reference clock**: TBD (01-system-requirements.md에서 확정)
- **Time zone handling**: N/A (절대 시각 기준)
- **Synchronization tolerance**: TBD

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
