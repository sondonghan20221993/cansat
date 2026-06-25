# 05. Reconstruction 요구사항

## 1. 목적

이 문서는 이미지 기반 3D reconstruction 모듈의 요구사항을 정의한다.

이 모듈은 선택된 image-based reconstruction 또는 sequence-based
SLAM 파이프라인을 주된 reconstruction 방식으로 사용해야 한다. 장기적으로는
연속 이미지 시퀀스를 사용할 수 있을 때 sequence-aware pose-and-map 파이프라인을
우선 구조로 삼는다. 모듈은 드론에서 수집한 image 입력을 받아 원격 GPU server에서
reconstruction 또는 SLAM 처리를 수행하고, downstream integration을 위해
reconstruction 출력, pose/map 상태, 품질 메타데이터를 반환해야 한다.

현재 시스템 기획 단계에서는 GLB가 가장 유력한 주 외부 출력 형식이다.
다만 reconstruction model과 출력 형식은 향후 개정에서 바뀔 수 있으므로,
모듈 경계 계약을 깨지 않고 교체할 수 있도록 구조화해야 한다.

좌표계, timestamp, 공통 인터페이스 규칙은 상위 시스템 문서와 인터페이스 명세(`03-interface-specification.md`)를 따라야 한다.

---

## 2. 기능 범위

### 2.1 포함 범위

- Image 입력 검증 및 메타데이터 일관성 확인
- 원격 job 제출을 위한 image 및 메타데이터 패키징
- reconstruction server로의 원격 job 제출
- 선택된 reconstruction / SLAM backend의 원격 GPU 처리
- 3차원 map state 생성 및 품질 평가
- 결과 패키징 및 지상국 시스템으로의 반환
- 검증 UI를 위한 지상국 fixed-frame visualization 메타데이터 생성
- 여러 reconstruction chunk를 누적 추가하거나 연속 session map을 갱신하기 위한 accumulated map 입출력 계약
- 지상국 image inbox 감시 및 buffer 기반 자동 job dispatch
- 처리된 image 파일 수명주기 관리(inbox → processed 분리)
- live-updating accumulated map viewer(server-push 또는 polling, 수동 새로고침 없음)

### 2.2 제외 범위

- cFS application lifecycle control(see `07-cfs-integration-requirements.md`)
- 시스템 수준 좌표계 정렬 정책(see `06-pose-frame-alignment-requirements.md`)

---

## 3. 입력 요구사항

### 3.1 필수 이미지 입력

- **REC-IN-01**: reconstruction 모듈은 드론에서 획득한 입력 이미지 집합을 수용해야 한다.
- **REC-IN-02**: 각 입력 이미지는 고유 식별자와 획득 timestamp를 가져야 한다.
- **REC-IN-03**: reconstruction 모듈은 job submission 이전에 손상되었거나 decode할 수 없는 이미지를 거부해야 한다.
- **REC-IN-04**: reconstruction 시작 전에 시스템이 정의한 최소 이미지 개수 이상이 확보되어야 한다. *(최소 개수는 `OI-REC-01` 참조)*

### 3.2 선택적 보조 입력

- **REC-IN-05**: reconstruction 모듈은 사용 가능한 경우 camera intrinsic parameter를 선택적 입력으로 수용해야 한다.
- **REC-IN-06**: reconstruction 모듈은 external camera pose, GPS/IMU 기반 위치 정보, 기타 localization 데이터를 job packaging 및 traceability를 위한 선택적 보조 metadata로만 수용해야 한다. 주 센서 융합 및 World / Map frame alignment는 Pose / Frame Alignment module이 담당해야 한다.
- **REC-IN-07**: 선택적 보조 입력은 reconstruction 시작의 필수 전제조건이 되어서는 안 된다.
- **REC-IN-08**: reconstruction 모듈은 어떤 보조 입력도 없이 이미지 입력만으로 reconstruction 결과를 생성할 수 있어야 한다.

### 3.3 지상국 입력 처리

- **REC-IN-09**: 지상국 컴퓨터는 reconstruction request를 제출하기 전에 입력 이미지와 관련 metadata를 수신해야 한다.
- **REC-IN-10**: 지상국 컴퓨터는 reconstruction 입력을 패키징하여 원격 reconstruction server로 전달해야 한다.
- **REC-IN-10A**: 연속 이미지 시퀀스를 사용할 수 있는 경우, 지상국 컴퓨터는 frame 순서를 보존해야 하며, sequence 기반 tracking 또는 SLAM backend가 시간 순서 입력을 사용할 수 있어야 한다.

### 3.4 Inbox 기반 자동 이미지 수집

- **REC-IN-11**: 지상국 시스템은 지정된 inbox directory를 지원해야 하며, 새 이미지 파일을 자동으로 감지하여 per-image 수동 실행 없이 reconstruction 대상으로 적재해야 한다.
- **REC-IN-12**: 지상국 시스템은 메모리 또는 디스크 기반 이미지 buffer를 유지해야 한다. inbox에서 감지된 이미지는 도착 순서대로 buffer에 추가되어야 한다.
- **REC-IN-13**: buffer가 구성된 chunk size에 도달하면, 지상국 시스템은 buffered image를 사용해 reconstruction job을 자동 dispatch하고 해당 이미지를 buffer에서 제거해야 한다.
- **REC-IN-14**: 지상국 시스템은 dispatch된 reconstruction job에 포함된 각 이미지 파일을 inbox directory에서 별도 processed directory로 이동해야 한다. 아직 dispatch되지 않은 이미지는 inbox 또는 buffer에 남아 있어야 하며, 이미 처리된 이미지와 섞여서는 안 된다.
- **REC-IN-15**: inbox monitoring loop는 지속적으로 실행되어야 하며, 새 이미지를 수집하기 위해 process restart가 필요해서는 안 된다.
- **REC-IN-16**: inbox의 이미지 파일을 읽을 수 없거나 validation에 실패하면, 지상국 시스템은 해당 파일을 rejected 하위 directory로 이동하고 monitoring loop를 멈추지 않은 채 오류를 기록해야 한다.

---

## 4. Reconstruction Pipeline 요구사항

### 4.1 입력 준비

- **REC-PROC-01**: reconstruction 모듈은 reconstruction 실행 전에 image 완전성과 메타데이터 일관성을 검증해야 한다.
- **REC-PROC-02**: reconstruction 모듈은 job 시작 시 입력 이미지 수, 이미지 해상도, 메타데이터 가용 여부를 기록해야 한다.
- **REC-PROC-03**: reconstruction 모듈은 선택된 reconstruction backend가 요구하는 전처리 단계를 지원해야 한다.

### 4.2 Reconstruction 처리

- **REC-PROC-04**: reconstruction 모듈은 선택된 image-based reconstruction 또는 sequence-based SLAM 방법을 주된 reconstruction pipeline으로 사용해야 한다.
- **REC-PROC-05**: reconstruction 모듈은 선택된 reconstruction 또는 SLAM backend를 사용하여 image 입력으로부터 장면 구조와 camera pose를 추정해야 한다.
- **REC-PROC-06**: reconstruction 모듈은 선택된 reconstruction model을 교체, 업그레이드, 재구성하더라도 모듈 경계 계약이 바뀌지 않도록 모듈화되어야 한다.
- **REC-PROC-07**: `REC-IN-06`에 정의된 선택적 auxiliary pose 또는 localization 입력이 제공되더라도, reconstruction 모듈은 이를 보조 메타데이터 패키징, traceability, backend hint 용도로만 사용해야 하며 필수 입력이나 주된 sensor fusion 입력으로 취급해서는 안 된다.
- **REC-PROC-08**: reconstruction 모듈은 auxiliary pose 입력이 없어도 image-only reconstruction을 계속 지원해야 한다.
- **REC-PROC-08A**: 연속 시퀀스 모드로 동작할 때 reconstruction 모듈은 session이 명시적으로 완료, export, 폐기될 때까지 session별 camera trajectory와 map state를 유지해야 한다.

### 4.3 원격 실행

- **REC-PROC-09**: 지상국 컴퓨터는 reconstruction job을 원격 GPU server로 제출해야 한다.
- **REC-PROC-10**: 원격 GPU server는 선택된 reconstruction / SLAM inference workload를 NVIDIA RTX A6000급 GPU 환경에서 실행해야 한다.
- **REC-PROC-11**: 원격 GPU server는 처리 후 reconstruction 출력과 실행 상태를 지상국 컴퓨터로 반환해야 한다.
- **REC-PROC-12**: reconstruction 모듈은 request와 response 사이에서 job identity를 보존하여 반환된 출력이 원래 image set과 매칭될 수 있도록 해야 한다.
- **REC-PROC-13**: 원격 실행이 실패, timeout, invalid output 반환으로 끝난 경우 reconstruction 모듈은 reconstruction failure 상태를 기록해야 한다.
- **REC-PROC-13A**: 최종 transport가 확정되기 전까지 prototype 원격 실행 경로는 `03-interface-specification.md` Section 3.4에 정의된 HTTP polling 계약을 지원해야 한다.
- **REC-PROC-13B**: 지상국 client는 원격 실행 성공 후 완료된 reconstruction artifact를 자동으로 내려받아야 하며, 내려받은 artifact를 fixed-frame visualization 또는 downstream integration 경로로 넘겨야 한다.
- **REC-PROC-13C**: sequence-based SLAM backend의 경우, 원격 실행 경로는 여러 개의 순차 frame이 하나의 진화하는 map state에 기여할 수 있도록 장수명 processing session 또는 동등한 session identifier를 지원해야 한다. Session state 전이와 operation response는 `03-interface-specification.md` Section 3.5A를 따라야 한다.
- **REC-PROC-13D**: 선택된 backend가 sequence-based SLAM backend인 경우, reconstruction 모듈은 backend runtime 출력을 즉시 독립적인 reconstruction chunk로 변환하도록 강제하지 말고 session-state 계약을 통해 노출해야 한다.
- **REC-PROC-13E**: reconstruction 모듈은 session-state 계약과 선택된 sequence backend 사이에 backend adapter 경계를 유지해야 한다. 이를 통해 외부 API를 바꾸지 않고도 backend별 runtime file, log, intermediate output을 공통 session-state field로 변환할 수 있어야 한다. 현재 prototype에서는 backend-native trajectory file을 권위 있는 `pose_stream_ref` 자원으로, backend-native map snapshot을 권위 있는 `map_state_ref` 자원으로 보존해야 한다.

### 4.4 결과 패키징

- **REC-PROC-14**: reconstruction 모듈은 reconstruction 출력과 함께 quality metadata 및 processing status를 묶어 제공해야 한다.
- **REC-PROC-15**: reconstruction 모듈은 successful, degraded, failed reconstruction outcome을 구분해야 한다. *(판정 기준: OI-REC-05 참조)*
- **REC-PROC-16**: reconstruction 모듈은 반환 결과를 정의된 인터페이스를 통해 downstream alignment 또는 integration 모듈이 사용할 수 있게 해야 한다.
- **REC-PROC-16A**: backend가 camera trajectory 또는 globally consistent map state를 직접 제공하는 경우, 모듈은 같은 session의 독립 artifact를 다시 downstream alignment하도록 요구하기보다 해당 출력을 주된 alignment source로 취급해야 한다.

### 4.5 Accumulated Map 처리

- **REC-PROC-17**: 지상국 reconstruction 경로는 영속적인 accumulated map manifest의 생성 및 append 작업을 담당해야 하며, 선택된 backend가 sequence/session 계약을 사용하는 경우 지상국 session map record의 생성과 갱신도 담당해야 한다.
- **REC-PROC-18**: 각 map chunk는 원본 reconstruction `job_id`, `image_set_id`, local artifact reference, output format, timestamp, quality metadata, frame/alignment metadata를 보존해야 한다.
- **REC-PROC-19**: 유효한 Reconstruction-to-World transform이 첨부되지 않은 한, accumulated map은 독립적인 reconstruction chunk가 이미 metric World / Map frame을 공유한다고 가정해서는 안 된다.
- **REC-PROC-20**: chunk에 유효한 World-frame alignment transform이 없을 경우, accumulated map은 이를 최종 map으로 묵시적으로 병합하지 말고 `UNALIGNED` 또는 `PARTIAL_ALIGNMENT` 상태로 저장해야 한다.
- **REC-PROC-21**: accumulated map update 경로는 raw reconstruction artifact를 수정하지 않고도 Pose / Frame Alignment 모듈이 manifest update 인터페이스를 통해 chunk transform metadata를 갱신하거나 교체할 수 있게 해야 한다.
- **REC-PROC-22**: process restart 이후에도 map state를 복구할 수 있도록 accumulated map manifest는 지상국 파일 형태로 영속 저장되어야 한다.
- **REC-PROC-23**: 명시적인 replacement policy가 설정되지 않는 한, accumulated map append 작업은 기본적으로 중복 `job_id` 항목을 거부해야 한다.
- **REC-PROC-24**: accumulated map은 raw artifact를 삭제하지 않고도 chunk를 invalidated 상태로 표시할 수 있어야 한다.
- **REC-PROC-24A**: backend가 독립적인 chunk artifact 대신 session-level map state를 생성하는 경우, accumulated map 경로는 같은 session의 별도 3D artifact 사이에 사후 alignment를 요구하지 않고도 해당 session state의 증분 갱신을 지원해야 한다. session-state ownership은 REC-PROC-17에 정의된 지상국 reconstruction 경로에 남는다.

### 4.6 처리된 이미지 수명주기

- **REC-PROC-25**: 지상국 시스템은 항상 미처리 이미지(inbox)와 처리된 이미지(processed directory)를 엄격히 분리해서 유지해야 한다. 각 이미지 파일은 어느 시점이든 정확히 한 위치에만 존재해야 한다.
- **REC-PROC-26**: 이미지 파일은 해당 파일을 포함한 reconstruction job이 성공적으로 dispatch된 뒤 inbox에서 processed directory로 원자적으로 이동되거나 rename 연산으로 이동되어야 한다. copy-then-delete 전략은 다음 monitoring cycle이 inbox를 읽기 전에 delete 단계가 보장되는 경우에만 허용된다.
- **REC-PROC-27**: 지상국 시스템은 이미 processed directory로 이동한 이미지를 다시 읽거나 다시 buffer에 넣어서는 안 된다.

---

## 5. 출력 요구사항

### 5.1 Reconstruction 출력

- **REC-OUT-01**: reconstruction 모듈은 시스템이 정의한 표현 형식으로 reconstruction result를 출력해야 한다. 이 표현은 artifact 지향 출력과 sequence/session map-state 출력을 모두 지원해야 한다. *(주요 artifact 후보는 GLB/PLY 유지, OI-REC-03 참조)*
- **REC-OUT-02**: 출력에는 reconstruction job 식별자와 처리 timestamp가 포함되어야 한다.
- **REC-OUT-03**: 출력에는 reconstruction을 생성하는 데 사용된 입력 image set의 식별자가 포함되어야 한다.
- **REC-OUT-04**: reconstruction 모듈은 향후 개정에서 전체 모듈을 다시 설계하지 않고도 외부 output format을 변경할 수 있도록 모듈화되어야 한다.
- **REC-OUT-04A**: 선택된 backend가 continuous-sequence mode로 동작하는 경우, 출력에는 session identifier와 camera trajectory 또는 동등한 pose stream reference가 포함되어야 한다.

### 5.2 품질 Metadata

- **REC-OUT-05**: reconstruction 모듈은 각 출력에 quality metadata를 포함해야 한다.
- **REC-OUT-06**: quality metadata에는 최소한 사용한 입력 이미지 수, processing status, 하나 이상의 reconstruction quality indicator가 포함되어야 한다. *(정확한 indicator는 OI-REC-04 참조)*
- **REC-OUT-07**: reconstruction 모듈은 시스템이 정의한 threshold에 대한 quality evaluation을 지원해야 한다. *(threshold 값은 OI-REC-04 참조)*

### 5.3 Failure 및 Degraded 출력

- **REC-OUT-08**: reconstruction이 실패한 경우, 모듈은 downstream 모듈이 일관되게 감지할 수 있는 failure result structure를 반환해야 한다.
- **REC-OUT-09**: 부분적이거나 낮은 신뢰도의 reconstruction만 가능한 경우, 모듈은 결과를 degraded로 표시해야 한다.
- **REC-OUT-10**: 모든 failure 및 degraded 출력은 log 또는 status field를 통해 추적 가능한 error 또는 status code를 포함해야 한다.

### 5.4 고정 좌표계 시각화 출력 (지상국 검증)

- **REC-OUT-11**: The reconstruction output SHALL expose camera trajectory metadata as defined in 03-interface-specification.md Section 3.3.
- **REC-OUT-12**: The reconstruction output SHALL expose fixed-frame visualization metadata as defined in 03-interface-specification.md Section 3.3.
- **REC-OUT-13**: The ground-side validation UI SHALL use the image linkage fields defined in 03-interface-specification.md Section 3.3.
- **REC-OUT-13A**: For sequence-based backends, the validation UI SHALL be able to consume incremental camera trajectory and map-state updates without requiring export of a new independent artifact for each short frame batch.
- **REC-OUT-13B**: For sequence-based backends, the session-state output SHALL include the latest accepted frame reference, frame count, and runtime tracking state when those values are available from the backend adapter.

### 5.5 Accumulated Map 출력

- **REC-OUT-14**: The reconstruction module SHALL support an accumulated map manifest that references one or more reconstruction chunks.
- **REC-OUT-15**: The accumulated map manifest SHALL include map identifier, chunk list, artifact references, per-chunk alignment status, per-chunk transform metadata, and quality metadata.
- **REC-OUT-16**: The accumulated map viewer SHALL be able to render multiple chunks in a shared display frame while distinguishing unaligned chunks from aligned chunks.
- **REC-OUT-17**: The accumulated map output SHALL preserve traceability from each displayed map chunk back to its source artifact reference, source_path set when available, and reconstruction job.
- **REC-OUT-18**: Unaligned chunks SHALL be rendered in their own reconstruction frame for diagnostic visualization and SHALL be visually marked as non-metric map contributions.

### 5.6 Live Viewer 출력

- **REC-OUT-19**: The ground-side accumulated map viewer SHALL update its displayed content automatically when a new reconstruction chunk is appended to the manifest, without requiring the user to close and reopen the viewer.
- **REC-OUT-20**: The viewer SHALL use a server-push or browser-polling mechanism to detect manifest changes. The update interval for polling-based implementations SHALL be configurable and SHALL default to no more than 5 seconds.
- **REC-OUT-21**: The viewer SHALL display the current chunk count, rendered point count, and last-updated timestamp in the UI panel so the user can confirm that live updates are being received.
- **REC-OUT-22**: A viewer update SHALL NOT require a full page reload. New chunk data SHALL be merged into the existing 3D scene incrementally.
- **REC-OUT-23**: For session-based backends, the ground-side live viewer SHALL support a session-state mode that displays session identifier, frame count, keyframe count when available, rendered point count when available, tracking state, alignment status, and last-updated timestamp.
- **REC-OUT-24**: The session-state live viewer SHALL be able to render camera trajectory and current map-state visualization from the session-state contract without requiring that the session first be exported into the accumulated map manifest.

---

## 6. 오류 처리 요구사항

- **REC-ERR-01**: The reconstruction module SHALL stop and reject processing when the minimum required image count is not satisfied.
- **REC-ERR-02**: The reconstruction module SHALL report corrupted or unusable input images in logs or status metadata before job submission.
- **REC-ERR-03**: The reconstruction module SHALL report remote server execution failure or timeout as a reconstruction failure condition.
- **REC-ERR-04**: The reconstruction module SHALL return a consistent degraded or failed status when the returned result quality is below the accepted threshold.
- **REC-ERR-05**: All failure cases SHALL be traceable through logs, status fields, or verification artifacts.

---

## 7. 성능 요구사항

- **REC-PERF-01**: The reconstruction pipeline SHALL be executable on a remote GPU server environment separate from the ground-side receiver.
- **REC-PERF-02**: The reconstruction module SHALL support NVIDIA RTX A6000-class GPU execution as the baseline deployment target.
- **REC-PERF-03**: The reconstruction module SHALL record job execution outcome and processing duration for each reconstruction request.
- **REC-PERF-04**: Detailed runtime and throughput targets SHALL be finalized in the system requirements and verification plan. *(See OI-REC-06)*

---

## 8. 인터페이스 명세에서 정의해야 할 항목

아래 항목은 reconstruction 모듈 경계 계약이며, `03-interface-specification.md`에서 정식으로 정의되어야 한다.

- **REC-IFC-01**: The message structure for reconstruction job request (ground-side → server), including job ID, image payload reference, and optional auxiliary input fields.
- **REC-IFC-02**: The message structure for reconstruction result return (server → ground-side), including job ID, output format reference, quality metadata, and status/error code.
- **REC-IFC-03**: The error/status code enumeration for reconstruction outcomes (success, degraded, failed, timeout).
- **REC-IFC-04**: The quality metadata field definitions and their types.
- **REC-IFC-05**: The output format identifier field and the mechanism by which the output format can be changed without breaking the module boundary contract.
- **REC-IFC-06**: Timestamp convention for reconstruction job request and result messages (reference: 03-interface-specification.md Section 6).

---

## 9. 검증 계획에서 정의해야 할 항목

reconstruction 검증 사례와 traceability는 `08-verification-plan.md`가 담당한다. 이 모듈 문서는 `REC-VER-01`부터 `REC-VER-19`까지의 requirement identifier만 예약한다.

예약된 verification identifier:

| ID | Verification intent |
| --- | --- |
| REC-VER-01 | Nominal end-to-end reconstruction |
| REC-VER-02 | Image-only reconstruction path |
| REC-VER-03 | Optional auxiliary input path |
| REC-VER-04 | Reconstruction failure handling |
| REC-VER-05 | Remote job submission and result return |
| REC-VER-06 | Reconstruction backend replacement |
| REC-VER-07 | Output format replacement |
| REC-VER-08 | Fixed-frame visualization consistency |
| REC-VER-09 | Camera trajectory / image linkage |
| REC-VER-10 | Accumulated map append or session-map update |
| REC-VER-11 | Accumulated map rendering |
| REC-VER-12 | Raw artifact preservation or non-destructive session export |
| REC-VER-13 | Per-chunk alignment update and unaligned chunk handling, excluding same-session SLAM map updates |
| REC-VER-14 | Inbox monitoring: automatic image detection and buffer accumulation |
| REC-VER-15 | Inbox monitoring: automatic job dispatch when buffer reaches chunk size |
| REC-VER-16 | Processed image lifecycle: inbox/processed separation and no re-read |
| REC-VER-17 | Live viewer: automatic update without page reload when new chunk is appended |
| REC-VER-18 | Live viewer: chunk count, point count, and last-updated timestamp displayed |
| REC-VER-19 | Session-state live viewer: status, trajectory, and map-state update |

---

## 10. 미정 항목

| ID         | Description                                                                                  | Owner | Status |
|------------|----------------------------------------------------------------------------------------------|-------|--------|
| OI-REC-01  | Minimum image count for starting the selected reconstruction backend needs to be finalized.   | TBD   | Open   |
| OI-REC-02  | Camera intrinsic parameter provisioning method needs to be finalized.                        | TBD   | Open   |
| OI-REC-03  | GLB is the current primary external output format candidate; the officially frozen output format needs to be confirmed and recorded in the interface specification. | TBD | Open |
| OI-REC-04  | Reconstruction quality indicators and acceptance thresholds need to be finalized.            | TBD   | Open   |
| OI-REC-05  | Criteria distinguishing degraded versus failed reconstruction outcomes need to be finalized. | TBD   | Open   |
| OI-REC-06  | Runtime and throughput targets for the reconstruction pipeline need to be finalized.         | TBD   | Open   |
| OI-REC-07  | Prototype remote execution transport is resolved as HTTP polling and defined in 03-interface-specification.md Section 3.4. Authentication, retry policy, and event-driven alternatives remain future work. | HTTP polling prototype | Resolved for prototype |
| OI-REC-08  | Fixed-frame identifier and transform metadata fields for validation UI output need to be frozen in interface specification. | TBD | Open |
| OI-REC-09  | Accumulated map manifest schema is defined in 03-interface-specification.md Section 3.5. Default storage location remains to be finalized. | TBD | Partially resolved |
| OI-REC-10  | Default policy: UNALIGNED chunks may be displayed diagnostically but SHALL NOT be treated as metric map contributions. PARTIAL_ALIGNMENT criteria remain to be finalized. | TBD | Partially resolved |
| OI-REC-11  | Inbox monitoring poll interval and filesystem watch mechanism (inotify, polling, or OS-native) need to be finalized for the target deployment platform. | TBD | Open |
| OI-REC-12  | Live viewer update mechanism (SSE, WebSocket, or browser polling) and update interval need to be finalized. Current prototype assumption is browser polling at ≤5 s interval. | TBD | Open |
| OI-REC-13  | Prototype session-state resource policy: raw backend-native trajectory files are preserved as the authoritative `pose_stream_ref` resource and backend-native map snapshots are preserved as the authoritative `map_state_ref` resource. Any normalized summary data is viewer-only and does not replace the file reference. | TBD | Resolved for prototype |
