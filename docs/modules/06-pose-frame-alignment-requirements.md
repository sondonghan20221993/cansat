# 06. Pose / Frame Alignment 요구사항

## 1. 목적

이 문서는 GPS, IMU, camera, reconstruction, map 좌표계를 정렬하기 위한 요구사항을 정의한다.

Pose / Frame Alignment module은 센서별 좌표 출력을 시스템 공통 World / Map frame으로 변환하는 책임을 가진다. 이미지 기반 3D reconstruction 출력은 사용 가능한 센서 pose 정보로 정렬되기 전까지 상대 reconstruction frame geometry로 취급한다. sequence 기반 SLAM backend가 이미 내부적으로 일관된 session pose와 map state를 제공하는 경우, alignment module은 동일 session에서 생성된 독립 3D artifact를 사후 정합하는 것보다 해당 session 전체를 World / Map frame으로 정렬해야 한다.

## 2. 좌표계

### 2.1 GPS 좌표계

- 원점: local tangent frame으로 변환되기 전까지 WGS84 geodetic reference
- 축 규칙: source에서는 latitude / longitude / altitude, local 변환 후에는 ENU 또는 NED
- 단위: 변환 전 latitude/longitude는 degree, altitude는 meter, 변환 후에는 meter

### 2.2 IMU / Body 좌표계

- 원점: IMU sensor 원점 또는 vehicle body 기준점
- 축 규칙: 하드웨어 장착 방향에 따라 TBD, body-frame axis로 문서화해야 함
- 단위: 자세는 radian 또는 degree, 가속도는 m/s^2, 각속도는 rad/s 또는 deg/s

### 2.3 Camera 좌표계

- 원점: calibration으로 재정의하지 않는 한 camera optical center
- 축 규칙: TBD — 현재 prototype 가정은 OpenCV camera frame이며 `OI-ALIGN-04` 참조
- 단위: metric alignment 이전까지는 backend 의존 상대 단위

### 2.4 Reconstruction 좌표계

- 원점: model 의존 reconstruction origin
- 축 규칙: model/backend 의존, 알려진 경우 metadata로 보고해야 함
- 단위: 센서 alignment를 통해 metric scale을 복구하기 전까지는 relative scale

### 2.5 World / Map 좌표계

- 원점: TBD — 임무/map origin은 `OI-ALIGN-01`에서 최종 확정
- 축 규칙: TBD — 현재 display-frame 후보는 ENU이며 최종 ENU/NED 정책은 `OI-ALIGN-01`에서 결정
- 단위: TBD — 시스템 전반의 cm 정책을 유지하지 않는 한 meter 사용 예상

## 3. 정렬 방법

좌표계 정렬 방식은 다음 접근을 지원해야 한다.

- static transform
- dynamic calibration
- hybrid alignment

모듈은 다음 transform chain을 지원해야 한다.

```text
GPS local frame      -> World / Map frame
IMU / Body frame     -> World / Map frame
Camera frame         -> Body or World / Map frame
Reconstruction frame -> World / Map frame
```

Reconstruction 출력에 대해서는 다음 항목을 추정하거나 적용해야 한다.

- 가능한 경우 relative reconstruction unit을 metric unit으로 변환하기 위한 scale
- reconstruction axis를 World / Map axis로 맞추기 위한 rotation
- reconstruction origin을 World / Map origin으로 맞추기 위한 translation
- 선택적인 camera-to-body 또는 camera-to-tag extrinsic offset

## 4. Offset 및 Calibration 파라미터

| 파라미터 | 설명 | 출처 | 갱신 규칙 |
| --- | --- | --- | --- |
| GPS to local transform | WGS84 GPS를 local ENU/NED frame으로 변환 | Mission origin / map config | 임무 단위로 고정 |
| IMU to Body transform | IMU 장착 방향 보정 | Hardware calibration | 재장착 전까지 고정 |
| Camera to Body transform | Vehicle body 기준 camera extrinsic offset | Calibration | Camera 이동 전까지 고정 |
| Reconstruction to World transform | Relative reconstruction geometry를 공통 frame에 정렬 | Sensor pose alignment | Reconstruction job 또는 image set별 갱신 |
| Scale factor | Reconstruction relative scale을 metric scale로 변환 | GPS/camera pose constraint | Reconstruction job 또는 image set별 갱신 |

## 5. 처리 요구사항

- **ALIGN-PROC-01**: 필요한 transform 데이터가 준비된 경우, 모듈은 GPS, IMU, camera, reconstruction 출력을 시스템 World / Map frame으로 변환해야 한다.
- **ALIGN-PROC-02**: 모듈은 source 데이터 간 timestamp 정렬을 유지해야 하며, interpolation, nearest-neighbor, stale sensor data 중 어떤 방식을 사용했는지 보고해야 한다.
- **ALIGN-PROC-03**: 모듈은 fused 또는 aligned output에 사용된 각 transform의 calibration validity를 보고해야 한다.
- **ALIGN-PROC-04**: Reconstruction-to-World transform이 준비되기 전까지 reconstruction 출력은 relative geometry로 취급해야 한다.
- **ALIGN-PROC-05**: 모듈은 raw reconstruction artifact를 수정해서는 안 되며, aligned metadata 또는 aligned derivative artifact/reference를 출력해야 한다.
- **ALIGN-PROC-06**: GPS와 camera pose 등 둘 이상의 위치 기준 source가 동시에 사용 가능한 경우, 모듈은 사용된 측정값을 보존하고 각 aligned output마다 어떤 source가 primary position reference로 사용되었는지 기록해야 한다.
- **ALIGN-PROC-07**: IMU 자세는 camera pose 또는 reconstruction alignment를 위한 보조 orientation constraint로 사용할 수 있어야 하지만, IMU 데이터가 없다고 reconstruction output을 자동으로 무효화해서는 안 된다.
- **ALIGN-PROC-08**: accumulated map에 대해서는 reconstruction chunk별 별도 Reconstruction-to-World transform, 또는 내부적으로 일관된 SLAM session별 단일 session-to-World transform을 계산하거나 적용해야 한다.
- **ALIGN-PROC-09**: 모듈은 per-chunk alignment status를 보존해야 하며, accumulated map manifest 안에 aligned, partially aligned, unaligned chunk가 공존할 수 있어야 한다.
- **ALIGN-PROC-10**: 더 나은 GPS/IMU/camera pose 정보가 확보된 뒤에는, accumulated map manifest 또는 `03-interface-specification.md` Section 3.5A에 정의된 `update_session_transform` operation을 통해 chunk 또는 session transform metadata를 초기 삽입 이후에도 갱신해야 한다.
- **ALIGN-PROC-11**: 모듈은 raw reconstruction chunk를 accumulated map manifest에 생성하거나 append해서는 안 된다. 해당 동작은 reconstruction ground-side path가 담당한다.

## 6. 출력 요구사항

- **ALIGN-OUT-01**: alignment가 가능한 경우, 모듈은 World / Map frame 기준의 통합 pose 또는 transform metadata를 출력해야 한다.
- **ALIGN-OUT-02**: 출력에는 source frame, target frame, transform matrix, scale, timestamp basis를 포함한 alignment metadata가 포함되어야 한다.
- **ALIGN-OUT-03**: GPS-to-local, IMU-to-Body, Camera-to-Body, Reconstruction-to-World transform의 calibration status를 보고해야 한다.
- **ALIGN-OUT-04**: 출력에는 GPS, IMU, camera pose, reconstruction constraint 중 무엇이 사용되었는지 나타내는 source selection metadata가 포함되어야 한다.
- **ALIGN-OUT-05**: 모든 출력은 `ALIGNED`, `PARTIAL_ALIGNMENT`, `UNALIGNED` 중 하나의 alignment status를 포함해야 한다.
- **ALIGN-OUT-06**: accumulated map 출력에서 `alignment_status`가 `ALIGNED` 또는 `PARTIAL_ALIGNMENT`인 경우 per-chunk transform metadata를 출력해야 하며, `UNALIGNED` chunk는 `transform = null`을 가져야 한다.

Alignment status 값은 다음과 같다.

| 상태 | 의미 |
| --- | --- |
| `ALIGNED` | 완전한 Reconstruction-to-World 또는 Session-to-World transform이 존재하며, 필요한 경우 scale, 선형 transform, translation, timestamp basis, 유효한 calibration status를 모두 포함한다. |
| `PARTIAL_ALIGNMENT` | 일부 alignment 정보는 있으나 metric map에 필요한 구성 요소 중 하나 이상이 불완전하거나 신뢰도가 낮다. 예: 신뢰할 수 있는 metric scale 없이 orientation만 있는 경우, 또는 stale/low-confidence timestamp basis를 가진 position만 있는 경우. |
| `UNALIGNED` | 유효한 Reconstruction-to-World transform이 없다. 진단용 시각화 목적으로만 표시할 수 있다. |

## 7. 오류 처리 요구사항

- **ALIGN-ERR-01**: 필요한 transform 데이터가 없으면 World-frame 결과를 조용히 내보내지 말고, `ALIGN-OUT-05`에 정의된 alignment status 중 하나를 생성해야 한다.
- **ALIGN-ERR-02**: Calibration mismatch는 추적 가능한 error/status code와 함께 보고해야 하며, 영향을 받은 transform 이름을 포함해야 한다.
- **ALIGN-ERR-03**: 명시적인 fallback policy가 구성되지 않은 한, frame inconsistency는 fused World-frame output의 publish를 막아야 한다.
- **ALIGN-ERR-04**: GPS와 IMU 데이터 누락은 서로 독립적으로 처리해야 하며, 하나의 센서 unavailable이 다른 모든 alignment output을 무효화해서는 안 된다.
- **ALIGN-ERR-05**: `ALIGN-PROC-04`와 일관되게, Reconstruction-to-World transform이 없거나 무효한 chunk는 unaligned visualization chunk로는 남아 있을 수 있지만 metric map contribution으로 취급해서는 안 된다.

## 8. 시험 요구사항

- **ALIGN-VER-01**: 검증 계획에는 transform calculation unit test가 포함되어야 한다.
- **ALIGN-VER-02**: 검증 계획에는 GPS + IMU + reconstruction alignment metadata에 대한 integration test가 포함되어야 한다.
- **ALIGN-VER-03**: 검증 계획에는 known reference pose와의 비교 검증이 포함되어야 한다.
- **ALIGN-VER-04**: 검증 계획에는 센서 누락 시 fallback 동작 시험이 포함되어야 한다.

## 9. 미정 항목

- `OI-ALIGN-01`: World / Map frame 규칙을 확정해야 한다. ENU/NED, 원점, 단위를 모두 포함한다.
- `OI-ALIGN-02`: GPS 변환 정책을 확정해야 한다. WGS84에서 local tangent frame으로의 변환 방식과 mission origin을 포함한다.
- `OI-ALIGN-03`: IMU body-frame axis 규칙과 장착 calibration을 확정해야 한다.
- `OI-ALIGN-04`: Camera-to-Body/Tag extrinsic calibration 절차를 확정해야 한다.
- `OI-ALIGN-05`: Reconstruction-to-World scale/rotation/translation 추정 방식을 확정해야 한다.
- `OI-ALIGN-06`: Per-chunk transform update interface는 accumulated map manifest 계약으로 정의되며, 상세 re-alignment trigger policy는 추후 확정한다.
