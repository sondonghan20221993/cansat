# 06. Pose / Frame Alignment Requirements

## 1. Purpose

Define requirements for aligning UWB, GPS, IMU, camera, reconstruction, and map coordinate frames.

The Pose / Frame Alignment module is responsible for converting sensor-specific
coordinate outputs into the system common World / Map frame. Image-based 3D
reconstruction outputs are treated as relative reconstruction-frame geometry
until aligned using available sensor pose information. When a sequence-based
SLAM backend already provides internally consistent session poses and map state,
the alignment module SHALL align that session as a whole to the World / Map
frame rather than prioritizing post hoc registration of independent 3D artifacts
from the same session.

## 2. Coordinate Frames

### 2.1 UWB Coordinate Frame

- Origin: TBD — see OI-ALIGN-01 and 04-uwb-requirements.md coordinate definition.
- Axis convention: TBD — see OI-ALIGN-01.
- Unit: cm at UWB Position_Result interface unless converted to the World / Map frame unit.

### 2.2 GPS Coordinate Frame

- Origin: WGS84 geodetic reference unless converted to a local tangent frame
- Axis convention: latitude / longitude / altitude at source; ENU or NED after local conversion
- Unit: degrees for latitude/longitude, meters for altitude before conversion; meters after conversion

### 2.3 IMU / Body Coordinate Frame

- Origin: IMU sensor origin or vehicle body reference point
- Axis convention: TBD by hardware mounting; SHALL be documented as body-frame axes
- Unit: radians or degrees for attitude; m/s^2 for acceleration; rad/s or deg/s for angular rate

### 2.4 Camera Coordinate Frame

- Origin: Camera optical center unless overridden by calibration.
- Axis convention: TBD — OpenCV camera frame is the current prototype assumption; see OI-ALIGN-04.
- Unit: backend-dependent relative units before metric alignment.

### 2.5 Reconstruction Coordinate Frame

- Origin: model-dependent reconstruction origin
- Axis convention: model/backend dependent; SHALL be reported as metadata when known
- Unit: relative scale unless metric scale is recovered from sensor alignment

### 2.6 World / Map Coordinate Frame

- Origin: TBD — mission/map origin to be finalized by OI-ALIGN-01.
- Axis convention: TBD — ENU is the current display-frame candidate; final ENU/NED policy is OI-ALIGN-01.
- Unit: TBD — expected metric unit is meters unless system-level cm policy is retained.

## 3. Alignment Method

Describe the coordinate alignment approach.

- Static transform
- Dynamic calibration
- Hybrid alignment

The module SHALL support the following transformation chain:

```text
UWB frame            -> World / Map frame
GPS local frame      -> World / Map frame
IMU / Body frame     -> World / Map frame
Camera frame         -> Body or World / Map frame
Reconstruction frame -> World / Map frame
```

For reconstruction output, the module SHALL estimate or apply:

- scale from relative reconstruction units to metric units when available
- rotation from reconstruction axes to World / Map axes
- translation from reconstruction origin to World / Map origin
- optional camera-to-body or camera-to-tag extrinsic offset

## 4. Offset and Calibration Parameters

| Parameter | Description | Source | Update Rule |
| --- | --- | --- | --- |
| UWB to World transform | Converts UWB coordinates into the system common frame | Anchor survey / calibration | Static unless anchors move |
| GPS to local transform | Converts WGS84 GPS into local ENU/NED frame | Mission origin / map config | Static per mission |
| IMU to Body transform | Corrects IMU mounting orientation | Hardware calibration | Static unless remounted |
| Camera to Body/Tag transform | Camera extrinsic offset from vehicle body or UWB Tag | Calibration | Static unless camera/tag moves |
| Reconstruction to World transform | Aligns relative reconstruction geometry to common frame | Sensor pose alignment | Updated per reconstruction job or image set |
| Scale factor | Converts reconstruction relative scale to metric scale | UWB/GPS/camera pose constraints | Updated per reconstruction job or image set |

## 5. Processing Requirements

- **ALIGN-PROC-01**: The module SHALL transform UWB, GPS, IMU, camera, and reconstruction outputs into the system World / Map frame when the required transform data is available.
- **ALIGN-PROC-02**: The module SHALL preserve timestamp alignment across source data and SHALL report when alignment uses interpolated, nearest-neighbor, or stale sensor data.
- **ALIGN-PROC-03**: The module SHALL report calibration validity for each transform used in a fused or aligned output.
- **ALIGN-PROC-04**: The module SHALL treat reconstruction output as relative geometry until a Reconstruction-to-World transform is available.
- **ALIGN-PROC-05**: The module SHALL NOT modify the raw reconstruction artifact; it SHALL output aligned metadata or an aligned derivative artifact/reference.
- **ALIGN-PROC-06**: When GPS and UWB are both available, the module SHALL preserve both source measurements and record which source was used as the primary position reference for each aligned output.
- **ALIGN-PROC-07**: IMU attitude SHALL be usable as an auxiliary orientation constraint for camera pose or reconstruction alignment, but missing IMU data SHALL NOT automatically invalidate reconstruction output.
- **ALIGN-PROC-08**: For accumulated maps, the module SHALL calculate or apply a separate Reconstruction-to-World transform for each reconstruction chunk, or a single session-to-World transform for each internally consistent SLAM session.
- **ALIGN-PROC-09**: The module SHALL preserve per-chunk alignment status so that aligned, partially aligned, and unaligned chunks can coexist in the accumulated map manifest.
- **ALIGN-PROC-10**: The module SHALL update chunk or session transform metadata through the accumulated map manifest or the `update_session_transform` operation defined in 03-interface-specification.md Section 3.5A after initial insertion when improved UWB/GPS/IMU/camera pose information becomes available.
- **ALIGN-PROC-11**: The module SHALL NOT create or append raw reconstruction chunks to the accumulated map manifest; those operations are owned by the reconstruction ground-side path.

## 6. Output Requirements

- **ALIGN-OUT-01**: The module SHALL output unified pose or transform metadata in the World / Map frame when alignment is available.
- **ALIGN-OUT-02**: The module SHALL include alignment metadata containing source frame, target frame, transform matrix, scale, and timestamp basis.
- **ALIGN-OUT-03**: The module SHALL report calibration status for UWB-to-World, GPS-to-local, IMU-to-Body, Camera-to-Body/Tag, and Reconstruction-to-World transforms.
- **ALIGN-OUT-04**: The module SHALL include source selection metadata indicating whether UWB, GPS, IMU, camera pose, or reconstruction constraints were used.
- **ALIGN-OUT-05**: The module SHALL include alignment status in each output using `ALIGNED`, `PARTIAL_ALIGNMENT`, or `UNALIGNED`.
- **ALIGN-OUT-06**: For accumulated map outputs, the module SHALL emit per-chunk transform metadata when `alignment_status` is `ALIGNED` or `PARTIAL_ALIGNMENT`; `UNALIGNED` chunks SHALL carry `transform = null`.

Alignment status values:

| Status | Meaning |
| --- | --- |
| `ALIGNED` | A complete Reconstruction-to-World or Session-to-World transform is available, including scale when required, linear transform, translation, timestamp basis, and valid calibration status. |
| `PARTIAL_ALIGNMENT` | Some alignment information is available but one or more required metric-map components remain incomplete or low-confidence. Example: orientation available without reliable metric scale, or position available with stale/low-confidence timestamp basis. |
| `UNALIGNED` | No valid Reconstruction-to-World transform is available. The chunk may be displayed diagnostically only. |

## 7. Error Handling Requirements

- **ALIGN-ERR-01**: Missing required transform data SHALL produce one of the alignment status values defined in ALIGN-OUT-05 rather than silently emitting a World-frame result.
- **ALIGN-ERR-02**: Calibration mismatch SHALL be reported with a traceable error/status code and SHALL include the affected transform name.
- **ALIGN-ERR-03**: Frame inconsistency SHALL prevent publication of a fused World-frame output unless an explicit fallback policy is configured.
- **ALIGN-ERR-04**: Missing GPS, UWB, or IMU data SHALL be handled independently so that one unavailable sensor does not invalidate all other alignment outputs.
- **ALIGN-ERR-05**: Consistent with ALIGN-PROC-04, a chunk with missing or invalid Reconstruction-to-World transform SHALL remain available as an unaligned visualization chunk but SHALL NOT be treated as a metric map contribution.
- **ALIGN-ERR-06**: Disabled or removed UWB SHALL be represented as an unavailable source and SHALL NOT prevent GPS/IMU/camera/reconstruction-only alignment outputs when those outputs are otherwise valid.

## 8. Test Requirements

- **ALIGN-VER-01**: The verification plan SHALL include unit tests for transform calculation.
- **ALIGN-VER-02**: The verification plan SHALL include integration tests for UWB + GPS + IMU + reconstruction alignment metadata.
- **ALIGN-VER-03**: The verification plan SHALL include validation against known reference poses.
- **ALIGN-VER-04**: The verification plan SHALL include tests for missing sensor fallback behavior.

## 9. Open Items

- OI-ALIGN-01: World / Map frame convention shall be finalized: ENU vs NED, origin, and units.
- OI-ALIGN-02: GPS conversion policy shall be finalized: WGS84 to local tangent frame method and mission origin.
- OI-ALIGN-03: IMU body-frame axis convention and mounting calibration shall be finalized.
- OI-ALIGN-04: Camera-to-Body/Tag extrinsic calibration procedure shall be finalized.
- OI-ALIGN-05: Reconstruction-to-World scale/rotation/translation estimation method shall be finalized.
- OI-ALIGN-06: Per-chunk transform update interface is defined by the accumulated map manifest contract. Detailed re-alignment trigger policy remains to be finalized.
