# 08. Verification Plan

## 1. Purpose

?�구?�항???�바르게 구현?�었?��? 검증하??방법???�의?�다.

---

## 2. Verification Strategy

검�??�름?� ?�래 ?�서�??�른??

1. Unit testing
2. Module integration testing
3. System integration testing
4. Hardware testing
5. Performance validation

---

## 3. Unit Test Plan

### 3.1 UWB 모듈 ?�위 ?�스??
| TC ID       | ?�스??�?                       | ?�력 조건                                                        | 기�? 출력                                                              | ?�???�구?�항                        |
|-------------|----------------------------------|------------------------------------------------------------------|------------------------------------------------------------------------|--------------------------------------|
| TC-UWB-01   | ?�상 4-Anchor trilateration      | 4�?Anchor ?�효 거리�? ?�려�?Tag ?�치                          | valid=True, x/y/z ?�차 ???�용�? error_code=NONE                      | UWB-PROC-09, UWB-OUT-01              |
| TC-UWB-02   | 30 ms ?��????�상 ?�신          | Timer 만료 ??3�??�신 ??30 ms ??4번째 ?�신                    | valid=True, trilateration ?�행                                         | UWB-PROC-02, UWB-PROC-09             |
| TC-UWB-03   | 30 ms ?��??�에???�락           | Timer 만료 ??3�??�신 ??30 ms ?�에??1�??�락                  | Invalid_Position, error_code=MISSING_DISTANCE                          | UWB-PROC-03                          |
| TC-UWB-04   | Timer 만료 ???�체 ?�락          | 4�?모두 미수??                                                 | Invalid_Position, error_code=MISSING_DISTANCE                          | UWB-PROC-02, UWB-PROC-03             |
| TC-UWB-05   | Non-positive 거리�?거�?         | 거리�???0??Anchor ?�함                                        | ?�당 Anchor 결측 처리, error_code=NON_POSITIVE_RANGE                  | UWB-IN-04                            |
| TC-UWB-06   | Duplicate_Distance 처리          | ?�일 Anchor_ID�??�일 ?�이????2???�신                        | 최신�?1개만 Distance_Set???�함                                       | UWB-PROC-05                          |
| TC-UWB-07   | Residual 20 cm 초과 ??valid ?��? | trilateration ?�공, residual > 20 cm                            | valid=True, 경고 로그 기록                                             | UWB-PROC-17                          |
| TC-UWB-08   | Residual 20 cm ?�하 ??경고 ?�음  | trilateration ?�공, residual ??20 cm                            | valid=True, 경고 로그 ?�음                                             | UWB-PROC-17                          |
| TC-UWB-09   | Trilateration ?�치 ?�패          | ?�치?�으�??��? ?�는 거리 조합 (NaN/inf, ?�수 루트 ??          | Invalid_Position, error_code=NUMERIC_FAILURE                           | UWB-PROC-14                          |
| TC-UWB-10   | 기하?�적 무효 배치               | Anchor 1,2,3???�일???�는 거의 ?�직??                         | Invalid_Position, error_code=GEOMETRY_INVALID                          | UWB-PROC-13                          |
| TC-UWB-11   | Z ??2�?�??�수 ?�택            | Z ?��? +z, -z ??�??�출?�는 Anchor 배치                        | Z >= 0?????�택, valid=True                                           | UWB-PROC-11                          |
| TC-UWB-12   | Z ??2�?모두 ?�수               | ??Z ?��? 모두 ?�수??Anchor 배치                               | Invalid_Position, error_code=Z_INVALID                                 | UWB-PROC-12                          |
| TC-UWB-13   | Invalid 출력 ?�드 검�?          | ?�의 Invalid 조건                                               | x=NaN, y=NaN, z=NaN, residual=NaN, valid=False, ?�락 distance=-1      | UWB-OUT-04, UWB-OUT-05, UWB-OUT-06, UWB-OUT-07 |
| TC-UWB-14   | Timestamp 검�?                  | 4�?거리�??�신 ?�각???�이                                     | timestamp = 마�?�??�신 ?�각(cFS_TIME)                                 | UWB-OUT-09                           |
| TC-UWB-15   | 출력 주기 검�?                  | ?�속 ?�이???�행                                                | �??�이??Position_Result ?�확??1??발행                              | UWB-OUT-10                           |
| TC-UWB-16   | Round-trip ?�확??검�?          | ?�려�??�치?�서 ??��???�론 거리�??�력                         | 추정 ?�치 ?�차 ???�용 ?�계�?                                          | UWB-PROC-10                          |

### 3.2 Other Module Unit Test Summary
| Module | Test Scope | Pass Criteria |
|---|---|---|
| Reconstruction Module | Pipeline functions, remote execution, artifact handling, accumulated map manifest | See TC-REC-01 through TC-REC-18 |
| Pose / Alignment Module | Transform math, calibration status, sensor fallback, per-chunk alignment metadata | See TC-ALIGN-01 through TC-ALIGN-13 |
| cFS Integration Layer | Message routing, timers, configuration, app lifecycle, event/log behavior | See TC-CFS-01 through TC-CFS-07 |
### 3.3 Reconstruction 검�?UI ?�스??
| TC ID       | ?�스??�?                                 | ?�력 조건                                                        | 기�? 출력                                                              | ?�???�구?�항                        |
|-------------|--------------------------------------------|------------------------------------------------------------------|------------------------------------------------------------------------|--------------------------------------|
| TC-REC-01   | 고정 좌표�?ENU) ?�군 ?�각???��???검�?  | ?�일 ?��?지 ?�트, ?�일 frame/transform ?�라미터�?2???�행      | 2??결과??transform.linear/translate ?�일, ?�군 �?방향 ?��?          | REC-OUT-12               |
| TC-REC-02   | 카메??궤적-?��?지 매핑 검�?              | camera_trajectory ?�함 결과�?UI?�서 ?�릭                        | ?�택 ?�드??image_id/source_path가 ?�당 카메???�드?� ?�치             | REC-OUT-11, REC-OUT-13   |
| TC-REC-03   | 좌표 변???�라미터 ?�용 검�?              | yaw/pitch/roll/translation??비영(0) 값으�??�정                 | UI ?�기 linear matrix/trajectory/point cloud가 ?�일 ?�라미터�?변?�됨  | REC-OUT-12               |
| TC-REC-04   | HTTP polling ?�격 ?�행 검�?               | `POST /jobs` ??`GET /jobs/{job_id}` polling                     | job_id가 ?��??�고 최종 status/result_ref/output_format??반환??       | REC-PROC-09, REC-PROC-12, REC-PROC-13A |
| TC-REC-05   | Artifact ?�동 ?�운로드 검�?               | ?�공??job???�??`GET /jobs/{job_id}/artifact` ?�출             | ?�라?�언?��? artifact�?로컬 경로???�?�하�?viewer ?�력?�로 ?�용 가??| REC-PROC-11, REC-PROC-13B, REC-OUT-01 |

Additional reconstruction verification cases:

| TC ID       | Test Name | Input Condition | Expected Output | Requirement |
|-------------|-----------|-----------------|-----------------|-------------|
| TC-REC-06   | Nominal reconstruction smoke test | Representative valid image set | Reconstruction finishes with success/degraded result and quality metadata | REC-VER-01 |
| TC-REC-07   | Image-only reconstruction path | Valid images without aux_pose | Reconstruction starts and returns a structured result | REC-VER-02 |
| TC-REC-08   | Auxiliary pose input path | Valid images with optional aux_pose/localization input | Auxiliary input is accepted without becoming mandatory | REC-VER-03 |
| TC-REC-09   | Input and remote failure handling | Corrupted images, insufficient image count, or remote failure | Failure/degraded status and error metadata are returned | REC-VER-04 |
| TC-REC-10   | Remote job return integration | Ground-side client submits job to remote server | Request/response identity and returned result are preserved | REC-VER-05 |
| TC-REC-11   | Reconstruction backend swap | Replace reconstruction backend with compatible implementation | Module boundary contract remains unchanged | REC-VER-06 |
| TC-REC-12   | Output format swap | Change configured output_format | Pipeline returns the requested format without module redesign | REC-VER-07 |
| TC-REC-13   | Accumulated map append | Append two reconstruction artifacts to the same map manifest without transforms while UWB is disabled | Manifest contains both chunks with job_id/image_set_id/artifact traceability and `alignment_status=UNALIGNED`; UWB unavailable status does not block append | REC-OUT-14, REC-OUT-15, REC-OUT-17 |
| TC-REC-14   | Accumulated map render | Render a manifest containing multiple chunks while UWB is disabled | Viewer renders all chunks and distinguishes aligned vs unaligned chunks; UWB unavailable status does not block rendering | REC-OUT-16 |
| TC-REC-15   | Raw artifact preservation | Update chunk transform after insertion | Raw artifact path is unchanged and artifact file hash before/after update is identical; only manifest transform metadata changes | REC-PROC-21 |
| TC-REC-16   | Manifest persistence | Append chunks, close/reload map manifest | Reloaded manifest preserves map_id, chunk list, artifact refs, status, and transform metadata | REC-PROC-22 |
| TC-REC-17   | Duplicate job append policy | Append a chunk with an existing job_id | Default behavior rejects duplicate and preserves original chunk entry | REC-PROC-23 |
| TC-REC-18   | Chunk invalidation | Invalidate an existing chunk | Manifest marks chunk invalidated without deleting or modifying raw artifact | REC-PROC-24 |
| TC-REC-19   | Inbox buffer accumulation | Drop images one by one into inbox directory | Each image is detected and added to buffer; no job is dispatched until buffer reaches chunk size | REC-VER-14 |
| TC-REC-20   | Inbox auto-dispatch at chunk size | Drop exactly chunk_size images into inbox | Reconstruction job is dispatched automatically; images are moved to processed directory | REC-VER-15 |
| TC-REC-21   | Inbox/processed separation | Drop images, wait for dispatch, inspect directories | Dispatched images exist only in processed/; inbox contains only undispatched images; no image appears in both | REC-VER-16 |
| TC-REC-22   | No re-read of processed images | Restart monitoring loop after dispatch | Images already in processed/ are not re-buffered or re-dispatched | REC-VER-16 |
| TC-REC-23   | Invalid image rejected to rejected/ | Drop an unreadable file into inbox | File is moved to rejected/ subdirectory; monitoring loop continues without stopping | REC-IN-16 |
| TC-REC-24   | Live viewer auto-update | Append a new chunk to manifest while viewer is open | Viewer displays updated chunk count and new point cloud data without page reload within configured poll interval | REC-VER-17 |
| TC-REC-25   | Live viewer status panel | Open viewer and append two chunks sequentially | Panel shows correct chunk count, rendered point count, and last-updated timestamp after each append | REC-VER-18 |
| TC-REC-26   | Session start and ordered frame append | Start a sequence session and append one ordered frame batch | Session enters `active`, frame count increases, and pose/map state becomes queryable through the session-state contract | REC-VER-10 |
| TC-REC-27   | Session transform update | Apply `update_session_transform` to an existing active or completed session | Session alignment metadata updates without redefining the session as independent artifact chunks | REC-VER-13 |
| TC-REC-28   | Session export to artifact path | Export a completed session and then archive the result through the artifact path | Export returns `artifact_ref`; optional archive/manifest insertion preserves traceability without altering the existing session-state contract | REC-VER-12 |
| TC-REC-29   | Session-state live viewer | Open live session viewer while session state is updated | Viewer shows session id, frame count, tracking state, `pose_stream_ref`/`map_state_ref` file-backed resources, and trajectory/map visualization without requiring export to manifest first | REC-VER-19 |

### 3.4 Pose / Frame Alignment Sensor Tests

| TC ID | ?�스??�?| ?�력 조건 | 기�? 출력 | ?�???�구?�항 |
|---|---|---|---|---|
| TC-ALIGN-01 | GPS/UWB/IMU ?�력 계약 검�?| GPS_Message, IMU_Message, UWB Position_Result ?�시 ?�력 | �??�본 측정값이 보존?�고 timestamp/source_frame 메�??�이?��? ?��???| ALIGN-PROC-01, ALIGN-PROC-02 |
| TC-ALIGN-02 | Reconstruction-to-World ?�렬 메�??�이??검�?| reconstruction artifact?� ?�서 pose/transform ?�력 | scale/rotation/translation???�함??Reconstruction-to-World transform ?�성 | ALIGN-PROC-04, ALIGN-PROC-05 |
| TC-ALIGN-03 | ?�서 결측 fallback 검�?| GPS ?�는 IMU ?��? 결측 | 결측 ?�서�?unavailable�??�시?�고 가?�한 alignment output?� ?��? | ALIGN-ERR-04 |
| TC-ALIGN-04 | Per-chunk transform update | Existing accumulated map chunk receives improved sensor alignment | Chunk transform/alignment_status updates without changing other chunks | ALIGN-PROC-08, ALIGN-PROC-09, ALIGN-PROC-10 |
| TC-ALIGN-05 | Unaligned chunk policy | Reconstruction chunk has no valid Reconstruction-to-World transform | Chunk remains visible as UNALIGNED but is not treated as metric map contribution | ALIGN-ERR-05 |
| TC-ALIGN-06 | Partial alignment status | Chunk has partial alignment data such as orientation without reliable metric scale | Chunk status is PARTIAL_ALIGNMENT and transform metadata records missing/low-confidence components | ALIGN-OUT-05, ALIGN-OUT-06 |
| TC-ALIGN-07 | Transform math order | Known point and known scale/linear/translate are applied | Output equals `scale * (linear @ point) + translate` within tolerance | ALIGN-VER-01 |
| TC-ALIGN-08 | Known reference pose validation | Known source pose and target World / Map pose pair | Estimated transform reproduces the known reference pose within tolerance | ALIGN-VER-03 |
| TC-ALIGN-09 | Calibration validity reporting | Transform input includes valid and invalid calibration states | Output reports calibration validity for each transform used | ALIGN-PROC-03, ALIGN-ERR-02 |
| TC-ALIGN-10 | GPS/UWB source selection | GPS and UWB are both available for the same timestamp window | Output preserves both measurements and records primary source selection | ALIGN-PROC-06 |
| TC-ALIGN-11 | Missing IMU tolerance | Reconstruction result is available while IMU data is missing | Reconstruction/alignment output is not automatically invalidated solely due to missing IMU | ALIGN-PROC-07 |
| TC-ALIGN-12 | Frame inconsistency handling | Source frame and transform metadata are inconsistent | Fused World-frame output is blocked or marked failed according to policy | ALIGN-ERR-03 |
| TC-ALIGN-13 | UWB disabled alignment fallback | UWB source is disabled/unavailable while GPS/IMU/reconstruction are available | Alignment output records UWB unavailable and continues with available sources | ALIGN-ERR-06 |

### 3.5 cFS Integration Tests

| TC ID | Test Name | Input Condition | Expected Output | Requirement |
|---|---|---|---|---|
| TC-CFS-01 | cFS app initialization | Startup with valid configuration | App initializes config, SB subscriptions, timers, and event services | CFS-APP-01, CFS-APP-02 |
| TC-CFS-02 | cFS non-blocking main loop | Timer, SB message, and reconstruction status events arrive together | Main loop processes events without blocking unrelated modules | CFS-APP-03, CFS-TMR-04 |
| TC-CFS-03 | cFS shutdown logging | Shutdown request during nominal operation | Resources are released and final status event is recorded | CFS-APP-04 |
| TC-CFS-04 | Software Bus routing | UWB Position_Result and reconstruction metadata are published | SB messages preserve timestamp, source, status, and payload references | CFS-SB-01, CFS-SB-02, CFS-SB-03, CFS-SB-04 |
| TC-CFS-05 | Configuration validation | Missing required anchor or endpoint config | Affected module does not enter nominal operation and emits traceable event | CFS-CFG-01, CFS-CFG-02, CFS-CFG-03 |
| TC-CFS-06 | Runtime config update policy | Runtime update request for allowed and disallowed fields | Only runtime-changeable fields are accepted | CFS-CFG-04 |
| TC-CFS-07 | UWB module disabled config | UWB disabled and Anchor coordinates absent | UWB remains disabled/unavailable and non-UWB modules enter nominal operation | CFS-CFG-05 |
| TC-CFS-08 | Baseline SB input publication | IMU, GPS, telemetry, and image metadata inputs are generated | `0x1901` through `0x1904` are published by the declared owner apps with valid payload fields | CFS-SB-05, CFS-SB-06, CFS-SB-07, CFS-SB-08 |
| TC-CFS-09 | Telemetry degraded transition | Link quality violates degraded threshold but not lost timeout | `TELEMETRY_STATUS_MID` reports `DEGRADED` and warning log is emitted | CFS-APP-06, CFS-TMR-06, CFS-LOG-05 |
| TC-CFS-10 | Telemetry lost and recovery | Valid link updates stop, then resume | `TELEMETRY_STATUS_MID` transitions to `LOST` and later to `ALIVE` or `DEGRADED` with recovery log | CFS-TMR-05, CFS-LOG-05, CFS-LOG-06 |
| TC-CFS-11 | Image metadata payload-only rule | Image capture event occurs | `IMAGE_META_MID` contains metadata/reference only and no raw binary image payload | CFS-APP-07, CFS-SB-08 |
| TC-CFS-12 | Telemetry baseline heartbeat period | Telemetry monitor producer emits valid updates every 500 ms nominally | Link remains `ALIVE` and no degraded/lost transition is emitted under nominal reception | CFS-TMR-05, CFS-TMR-06 |
| TC-CFS-13 | Telemetry runtime config staging | Runtime request changes telemetry timing thresholds to valid values | New configuration is staged, validated, and applied only at the documented safe application point | CFS-CFG-10, CFS-CFG-11, CFS-CFG-13, CFS-CFG-14 |
| TC-CFS-14 | Invalid telemetry runtime config reject | Runtime request sets invalid telemetry timing values such as lost timeout <= degraded timeout | Active configuration remains unchanged and the rejection is reported through event and housekeeping telemetry | CFS-CFG-11, CFS-CFG-12, CFS-CFG-14 |
| TC-CFS-23 | Communication-role separation rules | LoRa and image/video traffic are generated concurrently | Control/health traffic remains on `CONTROL_HEALTH_LINK` and payload traffic remains on `PAYLOAD_LINK` with no unintended cross-routing | CFS-LNK-01, CFS-LNK-03, CFS-LNK-05 |
| TC-CFS-24 | Timestamp origin rules | Vehicle-originated messages traverse bridge/relay path before ground reception | Ground-observed packets preserve vehicle-generated `timestamp` values without relay overwrite | CFS-SB-03, CFS-LNK-04 |
| TC-CFS-25 | Correlation identifier rules | A shared capture event produces LoRa status/event and image metadata messages | `image_id`, `job_id`, and `seq` correlation identifiers are preserved and match across linked messages | CFS-LNK-04 |

---

## 4. Module Integration Test Plan

모듈 �??�동 ?�스????��:

- UWB ??Alignment
- Reconstruction ??Alignment
- Alignment ??cFS Integration

---

## 5. System Integration Test Plan

?�체 end-to-end ?�나리오:

- Nominal scenario
- Fault scenario
- Degraded mode scenario

---

## 6. Hardware Test Plan

- Sensor connectivity test
- Timing synchronization test
- Real environment operational test

---

## 7. Performance Validation Criteria

| Metric     | Target | Verification Method |
|------------|--------|---------------------|
| Latency    | TBD    | TBD                 |
| Throughput | TBD    | TBD                 |
| Accuracy   | TBD    | TBD                 |
| Stability  | TBD    | TBD                 |

---

## 8. Traceability

The main table maps implementation requirements to test cases. Verification
requirement identifiers such as REC-VER-* are listed in a separate table below
to avoid mixing implementation requirements and verification requirements.

| Requirement ID   | Document                    | Verification Method | TC ID       |
|------------------|-----------------------------|---------------------|-------------|
| UWB-IN-01        | 04-uwb-requirements.md      | Unit Test           | TC-UWB-01   |
| UWB-IN-02        | 04-uwb-requirements.md      | Unit Test           | TC-UWB-06   |
| UWB-IN-03        | 04-uwb-requirements.md      | Unit Test           | TC-UWB-01   |
| UWB-IN-04        | 04-uwb-requirements.md      | Unit Test           | TC-UWB-05   |
| UWB-IN-05        | 04-uwb-requirements.md      | Unit Test           | TC-UWB-01, TC-UWB-13 |
| UWB-PROC-01      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-02, TC-UWB-15 |
| UWB-PROC-02      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-02, TC-UWB-04 |
| UWB-PROC-03      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-03, TC-UWB-04 |
| UWB-PROC-04      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-15   |
| UWB-PROC-05      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-06   |
| UWB-PROC-06      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-01, TC-UWB-02 |
| UWB-PROC-07      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-01, TC-UWB-06 |
| UWB-PROC-08      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-03, TC-UWB-13 |
| UWB-PROC-09      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-01   |
| UWB-PROC-10      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-16   |
| UWB-PROC-11      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-11   |
| UWB-PROC-12      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-12   |
| UWB-PROC-13      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-10   |
| UWB-PROC-14      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-09   |
| UWB-PROC-15      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-07, TC-UWB-08 |
| UWB-PROC-16      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-07, TC-UWB-08 |
| UWB-PROC-17      | 04-uwb-requirements.md      | Unit Test           | TC-UWB-07, TC-UWB-08 |
| UWB-OUT-01       | 04-uwb-requirements.md      | Unit Test           | TC-UWB-01, TC-UWB-11 |
| UWB-OUT-02       | 04-uwb-requirements.md      | Unit Test           | TC-UWB-01, TC-UWB-16 |
| UWB-OUT-03       | 04-uwb-requirements.md      | Unit Test           | TC-UWB-07, TC-UWB-08 |
| UWB-OUT-04~07    | 04-uwb-requirements.md      | Unit Test           | TC-UWB-13   |
| UWB-OUT-08       | 04-uwb-requirements.md      | Unit Test           | TC-UWB-03, TC-UWB-05, TC-UWB-09, TC-UWB-10, TC-UWB-12 |
| UWB-OUT-09       | 04-uwb-requirements.md      | Unit Test           | TC-UWB-14   |
| UWB-OUT-10       | 04-uwb-requirements.md      | Unit Test           | TC-UWB-15   |
| UWB-PERF-01      | 04-uwb-requirements.md      | Performance Test    | TBD         |
| UWB-PERF-02      | 04-uwb-requirements.md      | Performance Test    | TBD         |
| REC-OUT-11       | 05-reconstruction-requirements.md | Unit Test      | TC-REC-02   |
| REC-OUT-12       | 05-reconstruction-requirements.md | Unit Test      | TC-REC-01, TC-REC-03 |
| REC-OUT-13       | 05-reconstruction-requirements.md | Unit Test      | TC-REC-02   |
| REC-PROC-13A     | 05-reconstruction-requirements.md | Integration Test | TC-REC-04 |
| REC-PROC-13B     | 05-reconstruction-requirements.md | Integration Test | TC-REC-05 |
| REC-PROC-17      | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-13 |
| REC-PROC-18      | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-13 |
| REC-PROC-19      | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-14, TC-ALIGN-05 |
| REC-PROC-20      | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-14, TC-ALIGN-05 |
| REC-PROC-21      | 05-reconstruction-requirements.md | Unit Test | TC-REC-15, TC-ALIGN-04 |
| REC-PROC-22      | 05-reconstruction-requirements.md | Unit Test | TC-REC-16 |
| REC-PROC-23      | 05-reconstruction-requirements.md | Unit Test | TC-REC-17 |
| REC-PROC-24      | 05-reconstruction-requirements.md | Unit Test | TC-REC-18 |
| REC-PROC-24A     | 05-reconstruction-requirements.md | Integration Test | TC-REC-26, TC-REC-27 |
| REC-IN-11        | 05-reconstruction-requirements.md | Integration Test | TC-REC-19, TC-REC-20 |
| REC-IN-12        | 05-reconstruction-requirements.md | Integration Test | TC-REC-19 |
| REC-IN-13        | 05-reconstruction-requirements.md | Integration Test | TC-REC-20 |
| REC-IN-14        | 05-reconstruction-requirements.md | Integration Test | TC-REC-21 |
| REC-IN-15        | 05-reconstruction-requirements.md | Integration Test | TC-REC-19, TC-REC-20 |
| REC-IN-16        | 05-reconstruction-requirements.md | Unit Test | TC-REC-23 |
| REC-PROC-25      | 05-reconstruction-requirements.md | Integration Test | TC-REC-21 |
| REC-PROC-26      | 05-reconstruction-requirements.md | Integration Test | TC-REC-21 |
| REC-PROC-27      | 05-reconstruction-requirements.md | Integration Test | TC-REC-22 |
| REC-OUT-19       | 05-reconstruction-requirements.md | Integration Test | TC-REC-24 |
| REC-OUT-20       | 05-reconstruction-requirements.md | Integration Test | TC-REC-24 |
| REC-OUT-21       | 05-reconstruction-requirements.md | Integration Test | TC-REC-25 |
| REC-OUT-22       | 05-reconstruction-requirements.md | Integration Test | TC-REC-24 |
| REC-OUT-14       | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-13 |
| REC-OUT-15       | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-13 |
| REC-OUT-16       | 05-reconstruction-requirements.md | Integration Test | TC-REC-14 |
| REC-OUT-17       | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-13 |
| REC-OUT-18       | 05-reconstruction-requirements.md | Integration Test | TC-REC-14, TC-ALIGN-05 |
| REC-OUT-04A      | 05-reconstruction-requirements.md | Integration Test | TC-REC-26 |
| REC-OUT-13A      | 05-reconstruction-requirements.md | Integration Test | TC-REC-26 |
| REC-PROC-13D     | 05-reconstruction-requirements.md | Integration Test | TC-REC-26, TC-REC-29 |
| REC-PROC-13E     | 05-reconstruction-requirements.md | Integration Test | TC-REC-26, TC-REC-29 |
| REC-OUT-13B      | 05-reconstruction-requirements.md | Integration Test | TC-REC-29 |
| REC-OUT-23       | 05-reconstruction-requirements.md | Integration Test | TC-REC-29 |
| REC-OUT-24       | 05-reconstruction-requirements.md | Integration Test | TC-REC-29 |
| ALIGN-PROC-01    | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-01 |
| ALIGN-PROC-02    | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-01 |
| ALIGN-PROC-04    | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-02 |
| ALIGN-PROC-05    | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-02 |
| ALIGN-OUT-01     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-02 |
| ALIGN-OUT-02     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-01, TC-ALIGN-02 |
| ALIGN-OUT-03     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-01 |
| ALIGN-OUT-04     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-01 |
| ALIGN-OUT-05     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-06 |
| ALIGN-PROC-08    | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-04 |
| ALIGN-PROC-09    | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-04, TC-ALIGN-05 |
| ALIGN-PROC-10    | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-04 |
| ALIGN-PROC-11    | 06-pose-frame-alignment-requirements.md | Unit/Integration Test | TC-REC-13, TC-ALIGN-04 |
| ALIGN-OUT-06     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-04 |
| ALIGN-ERR-04     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-03 |
| ALIGN-ERR-05     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-05 |
| ALIGN-ERR-06     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-13 |
| ALIGN-PROC-03    | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-09 |
| ALIGN-PROC-06    | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-10 |
| ALIGN-PROC-07    | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-11 |
| ALIGN-ERR-01     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-05, TC-ALIGN-06 |
| ALIGN-ERR-02     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-09 |
| ALIGN-ERR-03     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-12 |
| CFS-APP-01~04    | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-01, TC-CFS-02, TC-CFS-03 |
| CFS-APP-05~08    | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-08, TC-CFS-09, TC-CFS-10, TC-CFS-11 |
| CFS-SB-01~04     | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-04 |
| CFS-SB-05~12     | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-07, TC-CFS-08, TC-CFS-11 |
| CFS-TMR-01~04    | 07-cfs-integration-requirements.md | Unit/Integration Test | TC-UWB-02, TC-UWB-03, TC-UWB-15 |
| CFS-TMR-05~07    | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-09, TC-CFS-10, TC-CFS-11 |
| CFS-CFG-01~08    | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-05, TC-CFS-06, TC-CFS-07, TC-CFS-09 |
| CFS-LOG-01~07    | 07-cfs-integration-requirements.md | Module Integration Test | TC-UWB-07, TC-UWB-08, TC-CFS-09, TC-CFS-10, TC-CFS-11 |
| CFS-VER-06       | 07-cfs-integration-requirements.md | Integration Test | TC-CFS-08 |
| CFS-VER-07       | 07-cfs-integration-requirements.md | Integration Test | TC-CFS-09, TC-CFS-10 |

### 8.1 Verification Requirement Traceability

| Verification Requirement ID | Document | Verification Method | TC ID |
|-----------------------------|----------|---------------------|-------|
| REC-VER-01 | 05-reconstruction-requirements.md | Integration Test | TC-REC-06 |
| REC-VER-02 | 05-reconstruction-requirements.md | Unit Test | TC-REC-07 |
| REC-VER-03 | 05-reconstruction-requirements.md | Unit Test | TC-REC-08 |
| REC-VER-04 | 05-reconstruction-requirements.md | Unit Test | TC-REC-09 |
| REC-VER-05 | 05-reconstruction-requirements.md | Integration Test | TC-REC-10 |
| REC-VER-06 | 05-reconstruction-requirements.md | Unit Test | TC-REC-11 |
| REC-VER-07 | 05-reconstruction-requirements.md | Unit Test | TC-REC-12 |
| REC-VER-08 | 05-reconstruction-requirements.md | Unit Test | TC-REC-01, TC-REC-03 |
| REC-VER-09 | 05-reconstruction-requirements.md | Unit Test | TC-REC-02 |
| REC-VER-10 | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-13, TC-REC-26 |
| REC-VER-11 | 05-reconstruction-requirements.md | Integration Test | TC-REC-14 |
| REC-VER-12 | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-15, TC-REC-28 |
| REC-VER-13 | 05-reconstruction-requirements.md | Integration Test | TC-ALIGN-04, TC-ALIGN-05, TC-REC-27 |
| REC-VER-14 | 05-reconstruction-requirements.md | Integration Test | TC-REC-19 |
| REC-VER-15 | 05-reconstruction-requirements.md | Integration Test | TC-REC-20 |
| REC-VER-16 | 05-reconstruction-requirements.md | Integration Test | TC-REC-21, TC-REC-22 |
| REC-VER-17 | 05-reconstruction-requirements.md | Integration Test | TC-REC-24 |
| REC-VER-18 | 05-reconstruction-requirements.md | Integration Test | TC-REC-25 |
| REC-VER-19 | 05-reconstruction-requirements.md | Integration Test | TC-REC-29 |
| ALIGN-VER-01 | 06-pose-frame-alignment-requirements.md | Unit Test | TC-ALIGN-07 |
| ALIGN-VER-02 | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-01, TC-ALIGN-02 |
| ALIGN-VER-03 | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-08 |
| ALIGN-VER-04 | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-03 |

---

## 9. Open Items

- OI-VER-01: ?�용 ?�치 ?�차 ?�계�?TC-UWB-16 pass criteria) ?�정 ?�요
- OI-VER-02: GEOMETRY_INVALID ?�별 ?�계�?condition number 기�?) ?�정 ??  TC-UWB-10 pass criteria 갱신 ?�요
- OI-VER-03: �?모듈 ?�위 ?�스??pass criteria TBD ??�� ?�정 ?�요
- OI-VER-04: Reconstruction UI ?�스?�용 기�? ?��?지 ?�트 �?기�? 변???�라미터(골든 ?�이?? ?�정 ?�요
