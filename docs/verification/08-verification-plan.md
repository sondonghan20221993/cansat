# 08. Verification Plan

## 1. Purpose

이 문서는 요구사항이 올바르게 구현되었는지 검증하는 방법을 정의한다.

---

## 2. Verification Strategy

검증 흐름은 아래 순서를 따른다.

1. Unit testing
2. Module integration testing
3. System integration testing
4. Hardware testing
5. Performance validation

---

## 3. Unit Test Plan

### 3.1 Other Module Unit Test Summary
| Module | Test Scope | Pass Criteria |
|---|---|---|
| Reconstruction Module | Pipeline functions, remote execution, artifact handling, accumulated map manifest, inbox monitoring, session-state viewer | See TC-REC-01 through TC-REC-29 |
| Pose / Alignment Module | Transform math, calibration status, sensor fallback, per-chunk alignment metadata | See TC-ALIGN-01 through TC-ALIGN-12 |
| cFS Integration Layer | Message routing, timers, configuration, app lifecycle, event/log behavior | See TC-CFS-01 through TC-CFS-19 |
### 3.2 Reconstruction 검증 UI 테스트
| TC ID       | 테스트명                                 | 입력 조건                                                        | 기대 출력                                                              | 관련 요구사항                        |
|-------------|--------------------------------------------|------------------------------------------------------------------|------------------------------------------------------------------------|--------------------------------------|
| TC-REC-01   | 고정 좌표계 ENU 점군 시각화 검증          | 동일 이미지 세트, 동일 frame/transform 파라미터로 2회 실행      | 두 결과의 `transform.linear/translate`가 동일하고 점군 방향이 일치     | REC-OUT-12               |
| TC-REC-02   | 카메라 궤적-이미지 매핑 검증              | `camera_trajectory`가 포함된 결과를 UI에서 클릭                 | 선택 노드의 `source_path`가 해당 카메라 노드와 일치                   | REC-OUT-11, REC-OUT-13   |
| TC-REC-03   | 좌표 변환 파라미터 적용 검증              | `yaw/pitch/roll/translation`에 비영 값 설정                     | UI의 `linear matrix/trajectory/point cloud`가 설정값에 따라 변경됨     | REC-OUT-12               |
| TC-REC-04   | HTTP polling 규격 실행 검증               | `POST /jobs` 후 `GET /jobs/{job_id}` polling                    | `job_id`가 유지되고 최종 `status/result_ref/output_format` 반환        | REC-PROC-09, REC-PROC-12, REC-PROC-13A |
| TC-REC-05   | Artifact 자동 다운로드 검증               | 성공한 job 이후 `GET /jobs/{job_id}/artifact` 호출              | 클라이언트가 artifact를 로컬 경로에 저장하고 viewer 입력으로 사용 가능 | REC-PROC-11, REC-PROC-13B, REC-OUT-01 |

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
| TC-REC-13   | Accumulated map append | Append two reconstruction artifacts to the same map manifest without transforms | Manifest contains both chunks with job_id/image_set_id/artifact traceability and `alignment_status=UNALIGNED` | REC-OUT-14, REC-OUT-15, REC-OUT-17 |
| TC-REC-14   | Accumulated map render | Render a manifest containing multiple chunks without transform metadata | Viewer renders all chunks and distinguishes aligned vs unaligned chunks | REC-OUT-16 |
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

### 3.3 Pose / Frame Alignment Sensor Tests

| TC ID | 테스트명 | 입력 조건 | 기대 출력 | 관련 요구사항 |
|---|---|---|---|---|
| TC-ALIGN-01 | GPS/IMU 입력 계약 검증 | `GPS_Message`, `IMU_Message` 동시 입력 | 원본 측정값이 보존되고 `timestamp/source_frame` metadata가 유지됨 | ALIGN-PROC-01, ALIGN-PROC-02 |
| TC-ALIGN-02 | Reconstruction-to-World 정렬 metadata 검증 | reconstruction artifact와 센서 pose/transform 입력 | scale/rotation/translation을 포함한 `Reconstruction-to-World` transform 생성 | ALIGN-PROC-04, ALIGN-PROC-05 |
| TC-ALIGN-03 | 센서 결측 fallback 검증 | GPS 또는 IMU 중 하나 결측 | 결측 센서를 unavailable로 표시하고 가능한 alignment output은 유지 | ALIGN-ERR-04 |
| TC-ALIGN-04 | Per-chunk transform update | Existing accumulated map chunk receives improved sensor alignment | Chunk transform/alignment_status updates without changing other chunks | ALIGN-PROC-08, ALIGN-PROC-09, ALIGN-PROC-10 |
| TC-ALIGN-05 | Unaligned chunk policy | Reconstruction chunk has no valid Reconstruction-to-World transform | Chunk remains visible as UNALIGNED but is not treated as metric map contribution | ALIGN-ERR-05 |
| TC-ALIGN-06 | Partial alignment status | Chunk has partial alignment data such as orientation without reliable metric scale | Chunk status is PARTIAL_ALIGNMENT and transform metadata records missing/low-confidence components | ALIGN-OUT-05, ALIGN-OUT-06 |
| TC-ALIGN-07 | Transform math order | Known point and known scale/linear/translate are applied | Output equals `scale * (linear @ point) + translate` within tolerance | ALIGN-VER-01 |
| TC-ALIGN-08 | Known reference pose validation | Known source pose and target World / Map pose pair | Estimated transform reproduces the known reference pose within tolerance | ALIGN-VER-03 |
| TC-ALIGN-09 | Calibration validity reporting | Transform input includes valid and invalid calibration states | Output reports calibration validity for each transform used | ALIGN-PROC-03, ALIGN-ERR-02 |
| TC-ALIGN-10 | 다중 위치 기준 source selection | GPS와 camera pose 등 둘 이상의 위치 기준이 같은 timestamp window에서 사용 가능 | Output preserves all applicable measurements and records primary source selection | ALIGN-PROC-06 |
| TC-ALIGN-11 | Missing IMU tolerance | Reconstruction result is available while IMU data is missing | Reconstruction/alignment output is not automatically invalidated solely due to missing IMU | ALIGN-PROC-07 |
| TC-ALIGN-12 | Frame inconsistency handling | Source frame and transform metadata are inconsistent | Fused World-frame output is blocked or marked failed according to policy | ALIGN-ERR-03 |
### 3.4 cFS Integration Tests

| TC ID | Test Name | Input Condition | Expected Output | Requirement |
|---|---|---|---|---|
| TC-CFS-01 | cFS app initialization | Startup with valid configuration | App initializes config, SB subscriptions, timers, and event services | CFS-APP-01, CFS-APP-02 |
| TC-CFS-02 | cFS non-blocking main loop | Timer, SB message, and reconstruction status events arrive together | Main loop processes events without blocking unrelated modules | CFS-APP-03, CFS-TMR-02 |
| TC-CFS-03 | cFS shutdown logging | Shutdown request during nominal operation | Resources are released and final status event is recorded | CFS-APP-04 |
| TC-CFS-04 | Software Bus routing | Reconstruction metadata and baseline SB inputs are published | SB messages preserve timestamp, source, status, and payload references | CFS-SB-01, CFS-SB-02, CFS-SB-03 |
| TC-CFS-05 | Configuration validation | Missing required endpoint or transform config | Affected module does not enter nominal operation and emits traceable event | CFS-CFG-01, CFS-CFG-02, CFS-CFG-03 |
| TC-CFS-06 | Runtime config update policy | Runtime update request for allowed and disallowed fields | Only runtime-changeable fields are accepted | CFS-CFG-04 |
| TC-CFS-08 | Baseline SB input publication | IMU, GPS, and telemetry inputs are generated | `0x1901` through `0x1903` are published by the declared owner apps with valid payload fields | CFS-SB-05, CFS-SB-06, CFS-SB-07 |
| TC-CFS-09 | Telemetry degraded transition | Link quality violates degraded threshold but not lost timeout | `TELEMETRY_STATUS_MID` reports `DEGRADED` and warning log is emitted | CFS-APP-06, CFS-TMR-06, CFS-LOG-05 |
| TC-CFS-10 | Telemetry lost and recovery | Valid link updates stop, then resume | `TELEMETRY_STATUS_MID` transitions to `LOST` and later to `ALIVE` or `DEGRADED` with recovery log | CFS-TMR-05, CFS-LOG-05, CFS-LOG-06 |
| TC-CFS-12 | Telemetry baseline heartbeat period | Telemetry monitor producer emits valid updates every 500 ms nominally | Link remains `ALIVE` and no degraded/lost transition is emitted under nominal reception | CFS-TMR-05, CFS-TMR-06 |
| TC-CFS-13 | Telemetry runtime config staging | Runtime request changes telemetry timing thresholds to valid values | New configuration is staged, validated, and applied only at the documented safe application point | CFS-CFG-10, CFS-CFG-11, CFS-CFG-13, CFS-CFG-14 |
| TC-CFS-14 | Invalid telemetry runtime config reject | Runtime request sets invalid telemetry timing values such as lost timeout <= degraded timeout | Active configuration remains unchanged and the rejection is reported through event and housekeeping telemetry | CFS-CFG-11, CFS-CFG-12, CFS-CFG-14 |
| TC-CFS-15 | MAVLink Bridge nominal parse and publish | FC sends `LOCAL_POSITION_NED`, `ATTITUDE`, and `GPS_RAW_INT` over serial | `FC_LOCAL_POS_MID (0x1905)`, `FC_ATTITUDE_MID (0x1906)`, and `FC_GPS_RAW_MID (0x1907)` are published on SB with correct field values and vehicle-generated `cFS_TIME` timestamp | CFS-APP-09, CFS-SB-13, CFS-SB-14, CFS-SB-15 |
| TC-CFS-16 | MAVLink Bridge raw frame not on SB | FC sends a valid MAVLink frame | No raw MAVLink byte frame appears on the cFS Software Bus; only typed SB messages are published | CFS-APP-10 |
| TC-CFS-17 | MAVLink Bridge optional message handling | FC sends `ODOMETRY` and `EKF_STATUS_REPORT` | `FC_ODOMETRY_MID (0x1908)` and `FC_EKF_STATUS_MID (0x1909)` are published when received; their absence does not trigger a parse error | CFS-SB-16 |
| TC-CFS-18 | MAVLink Bridge status transitions | MAVLink messages stop arriving, then resume | `MAVLINK_BRIDGE_STATUS_MID` transitions from `ALIVE` to `DEGRADED` to `LOST` and back to `ALIVE` at configured thresholds; transitions are logged at correct severity levels | CFS-SB-17, CFS-LOG-08, CFS-LOG-10 |
| TC-CFS-19 | MAVLink Bridge disabled isolation | `mavlink_bridge_app` is disabled in configuration | IMU, GPS, telemetry, reconstruction, and non-MAVLink alignment flows enter nominal operation without `FC_*_MID` messages | CFS-SB-18 |
| TC-CFS-23 | Communication-role separation rules | LoRa and image/video traffic are generated concurrently | Control/health traffic remains on `CONTROL_HEALTH_LINK` and payload traffic remains on `PAYLOAD_LINK` with no unintended cross-routing | CFS-LNK-01, CFS-LNK-03 |
| TC-CFS-24 | Timestamp origin rules | Vehicle-originated messages traverse bridge/relay path before ground reception | Ground-observed packets preserve vehicle-generated `timestamp` values without relay overwrite | CFS-SB-03, CFS-LNK-09 |
| TC-CFS-25 | Correlation identifier rules | A shared reconstruction job or source event produces LoRa status/event and image/video metadata messages | `job_id` and `seq` correlation identifiers are preserved and match across linked messages when applicable | CFS-LNK-10, CFS-LNK-11, CFS-LNK-12, CFS-LNK-13 |

---

## 4. Module Integration Test Plan

모듈 통합 테스트 범위는 다음과 같다.

- Reconstruction ??Alignment
- Alignment ??cFS Integration

---

## 5. System Integration Test Plan

전체 end-to-end 시나리오는 다음과 같다.

- Nominal scenario
- Fault scenario
- Degraded mode scenario

### 5.1 크로스 리포 링크 통합 테스트 (2026-07-13 추가)

리포 경계(기체 `cfs-telemetry-app` ↔ 지상 `openMCT`)에서만 드러나는 결함을 검증한다.
단위 테스트로 대체 불가 — 실링크(LR24-F 페어) 또는 시리얼 루프백 필요.
배경: `docs/04-repository-map.md` §5 (알려진 크로스 리포 갭).

| TC ID | Test Name | Input Condition | Expected Output | Requirement |
| --- | --- | --- | --- | --- |
| TC-LINK-01 | ACK 왕복 링크 확립 | 지상 브리지 기동 + 기체 downlink 수신 | 지상이 ACK(v1) / ACK2(v2) 회신, 기체 `LinkState = CONNECTED` 전이 | 03-interface §3.6.2 |
| TC-LINK-02 | v1/v2 혼합 스트림 공존 | v2 이행 2단계(기체 CONFIG로 포맷 전환) 중 혼합 수신 | 지상 파서가 양 포맷을 무손실 분기 (magic/ASCII) | lora_protocol_v2_spec §8 |
| TC-LINK-03 | v2 5Hz soak (1h) | TDM 200ms + DL2 46B, 1시간 연속 | 프레임 손실률 및 `NoAckCount` 추이가 링크 임계 미만 | lora_protocol_v2_spec §9 |
| TC-LINK-04 | 대형 업링크 분할 수신 | ROUTE_UPDATE(payload>100ms 에어타임) 송신 | 여러 RX창에 걸친 UP2가 조립되어 `UPLINK_APP_CMD_MID` 도달 | lora_protocol_v2_spec §7.1 |
| TC-LINK-05 | 시각 동기 종단 검증 | GPS fix + SYSTEM_TIME 체인 가동 + 카메라 NTP | 영상 OSD UTC ↔ 지상 CSV 로그 UTC 편차 ≤ 100ms | 03-interface §6.1 |
| TC-LINK-06 | 대역 간섭 회피 | LoRa(2.4GHz) + WFB-ng 영상 동시 송출, WFB 5.8GHz 설정 | LoRa 프레임 손실률이 영상 off 대비 유의미한 증가 없음 | 02-architecture §6 |

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
| REC-OUT-11       | 05-reconstruction-requirements.md | Unit Test      | TC-REC-02   |
| REC-OUT-12       | 05-reconstruction-requirements.md | Unit Test      | TC-REC-01, TC-REC-03 |
| REC-OUT-13       | 05-reconstruction-requirements.md | Unit Test      | TC-REC-02   |
| REC-IN-01        | 05-reconstruction-requirements.md | Integration Test | TC-REC-06 |
| REC-IN-02        | 05-reconstruction-requirements.md | Integration Test | TC-REC-10 |
| REC-IN-03        | 05-reconstruction-requirements.md | Integration Test | TC-REC-09 |
| REC-IN-04        | 05-reconstruction-requirements.md | Integration Test | TC-REC-09 |
| REC-IN-05        | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-08 |
| REC-IN-06        | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-08 |
| REC-IN-07        | 05-reconstruction-requirements.md | Unit Test | TC-REC-08 |
| REC-IN-08        | 05-reconstruction-requirements.md | Unit Test | TC-REC-07 |
| REC-IN-09        | 05-reconstruction-requirements.md | Integration Test | TC-REC-10 |
| REC-IN-10        | 05-reconstruction-requirements.md | Integration Test | TC-REC-04, TC-REC-10 |
| REC-IN-10A       | 05-reconstruction-requirements.md | Integration Test | TC-REC-26 |
| REC-PROC-01      | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-06, TC-REC-09 |
| REC-PROC-02      | 05-reconstruction-requirements.md | Integration Test | TC-REC-06 |
| REC-PROC-03      | 05-reconstruction-requirements.md | Unit Test | TC-REC-11 |
| REC-PROC-04      | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-06, TC-REC-26 |
| REC-PROC-05      | 05-reconstruction-requirements.md | Unit/Integration Test | TC-REC-06, TC-REC-26 |
| REC-PROC-06      | 05-reconstruction-requirements.md | Unit Test | TC-REC-11 |
| REC-PROC-07      | 05-reconstruction-requirements.md | Unit Test | TC-REC-08 |
| REC-PROC-08      | 05-reconstruction-requirements.md | Unit Test | TC-REC-07 |
| REC-PROC-08A     | 05-reconstruction-requirements.md | Integration Test | TC-REC-26, TC-REC-29 |
| REC-PROC-09      | 05-reconstruction-requirements.md | Integration Test | TC-REC-04, TC-REC-10 |
| REC-PROC-10      | 05-reconstruction-requirements.md | Hardware/Performance Test | TBD (GPU deployment validation) |
| REC-PROC-11      | 05-reconstruction-requirements.md | Integration Test | TC-REC-05, TC-REC-10 |
| REC-PROC-12      | 05-reconstruction-requirements.md | Integration Test | TC-REC-04, TC-REC-10 |
| REC-PROC-13      | 05-reconstruction-requirements.md | Integration Test | TC-REC-09 |
| REC-PROC-13A     | 05-reconstruction-requirements.md | Integration Test | TC-REC-04 |
| REC-PROC-13B     | 05-reconstruction-requirements.md | Integration Test | TC-REC-05 |
| REC-PROC-13C     | 05-reconstruction-requirements.md | Integration Test | TC-REC-26, TC-REC-29 |
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
| REC-PROC-14      | 05-reconstruction-requirements.md | Integration Test | TC-REC-06 |
| REC-PROC-15      | 05-reconstruction-requirements.md | Integration Test | TC-REC-06, TC-REC-09 |
| REC-PROC-16      | 05-reconstruction-requirements.md | Integration Test | TC-REC-05, TC-REC-13 |
| REC-PROC-16A     | 05-reconstruction-requirements.md | Integration Test | TC-REC-26, TC-REC-29 |
| REC-OUT-01       | 05-reconstruction-requirements.md | Integration Test | TC-REC-05, TC-REC-12 |
| REC-OUT-02       | 05-reconstruction-requirements.md | Integration Test | TC-REC-10 |
| REC-OUT-03       | 05-reconstruction-requirements.md | Integration Test | TC-REC-10 |
| REC-OUT-04       | 05-reconstruction-requirements.md | Unit Test | TC-REC-12 |
| REC-OUT-05       | 05-reconstruction-requirements.md | Integration Test | TC-REC-06 |
| REC-OUT-06       | 05-reconstruction-requirements.md | Integration Test | TC-REC-06 |
| REC-OUT-07       | 05-reconstruction-requirements.md | Integration Test | TC-REC-06, TC-REC-09 |
| REC-OUT-08       | 05-reconstruction-requirements.md | Integration Test | TC-REC-09 |
| REC-OUT-09       | 05-reconstruction-requirements.md | Integration Test | TC-REC-09 |
| REC-OUT-10       | 05-reconstruction-requirements.md | Integration Test | TC-REC-09 |
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
| REC-ERR-01       | 05-reconstruction-requirements.md | Integration Test | TC-REC-09 |
| REC-ERR-02       | 05-reconstruction-requirements.md | Integration Test | TC-REC-09, TC-REC-23 |
| REC-ERR-03       | 05-reconstruction-requirements.md | Integration Test | TC-REC-09 |
| REC-ERR-04       | 05-reconstruction-requirements.md | Integration Test | TC-REC-09 |
| REC-ERR-05       | 05-reconstruction-requirements.md | Integration Test | TC-REC-09 |
| REC-PERF-01      | 05-reconstruction-requirements.md | Integration Test | TC-REC-10 |
| REC-PERF-02      | 05-reconstruction-requirements.md | Hardware/Performance Test | TBD (GPU deployment validation) |
| REC-PERF-03      | 05-reconstruction-requirements.md | Integration Test | TC-REC-10 |
| REC-PERF-04      | 05-reconstruction-requirements.md | Performance Validation | TBD (runtime/throughput target finalization) |
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
| ALIGN-ERR-05     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-05 |
| ALIGN-PROC-03    | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-09 |
| ALIGN-PROC-06    | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-10 |
| ALIGN-PROC-07    | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-11 |
| ALIGN-ERR-01     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-05, TC-ALIGN-06 |
| ALIGN-ERR-02     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-09 |
| ALIGN-ERR-03     | 06-pose-frame-alignment-requirements.md | Integration Test | TC-ALIGN-12 |
| CFS-APP-01~04    | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-01, TC-CFS-02, TC-CFS-03 |
| CFS-APP-05~08    | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-08, TC-CFS-09, TC-CFS-10, TC-CFS-12, TC-CFS-13, TC-CFS-14 |
| CFS-APP-09~10    | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-15, TC-CFS-16 |
| CFS-SB-01~02     | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-04 |
| CFS-SB-03        | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-04, TC-CFS-24 |
| CFS-SB-05~11     | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-08 |
| CFS-SB-07A       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-08 |
| CFS-SB-13~15     | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-15 |
| CFS-SB-16        | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-17 |
| CFS-SB-17        | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-18 |
| CFS-SB-18        | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-19 |
| CFS-TMR-01~02    | 07-cfs-integration-requirements.md | Unit/Integration Test | TC-CFS-02 |
| CFS-TMR-05~07    | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-09, TC-CFS-10, TC-CFS-12 |
| CFS-TMR-08       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-10 |
| CFS-TMR-09       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-13 |
| CFS-TMR-10       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-10 |
| CFS-CFG-01~04    | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-05, TC-CFS-06 |
| CFS-CFG-06~08    | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-09 |
| CFS-CFG-09       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-08 |
| CFS-LOG-01, CFS-LOG-03~07 | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-09, TC-CFS-10, TC-CFS-14 |
| CFS-LOG-04       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-04 |
| CFS-LOG-07       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-04 |
| CFS-LOG-08~10    | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-18 |
| CFS-DEP-01~03    | 07-cfs-integration-requirements.md | Deployment Verification | TBD (hardware deployment test) |
| CFS-DEP-04       | 07-cfs-integration-requirements.md | Deployment Verification | TBD (hardware deployment test) |
| CFS-VER-06       | 07-cfs-integration-requirements.md | Integration Test | TC-CFS-08 |
| CFS-VER-07       | 07-cfs-integration-requirements.md | Integration Test | TC-CFS-09, TC-CFS-10 |
| CFS-VER-12       | 07-cfs-integration-requirements.md | Integration Test | TC-CFS-15 |
| CFS-VER-13       | 07-cfs-integration-requirements.md | Integration Test | TC-CFS-16 |
| CFS-VER-14       | 07-cfs-integration-requirements.md | Integration Test | TC-CFS-18 |
| CFS-VER-15       | 07-cfs-integration-requirements.md | Integration Test | TC-CFS-19 |
| CFS-LNK-01       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-23 |
| CFS-LNK-02       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-23 |
| CFS-LNK-03       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-23 |
| CFS-LNK-04       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-23 |
| CFS-LNK-06       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-23 |
| CFS-LNK-07       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-23 |
| CFS-LNK-09       | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-24 |
| CFS-LNK-10~13    | 07-cfs-integration-requirements.md | Module Integration Test | TC-CFS-25 |

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

- OI-VER-03: 각 모듈 단위 테스트의 TBD pass criteria를 확정해야 한다.
- OI-VER-04: Reconstruction UI 테스트용 기준 이미지 세트와 기준 transform 파라미터(golden dataset)를 확정해야 한다.
