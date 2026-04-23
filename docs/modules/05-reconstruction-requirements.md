# 05. Reconstruction Requirements

## 1. Purpose

This document defines the requirements for the image-based 3D reconstruction module.

The module SHALL use a DUSt3R-family pipeline as the primary reconstruction approach.
It SHALL accept image inputs collected from the drone, execute reconstruction on a
remote GPU server, and return 3D reconstruction outputs and quality metadata for
downstream integration.

At the current system planning stage, GLB is the most likely primary external output
format. Both the reconstruction model and the output format may change in future
revisions; therefore the module SHALL be structured to support replacement without
breaking the module boundary contract.

Coordinate systems, timestamps, and shared interface rules SHALL follow the top-level
system documents and the interface specification (03-interface-specification.md).

---

## 2. Functional Scope

### 2.1 In Scope

- Image input validation and metadata consistency checking
- Image and metadata packaging for remote job submission
- Remote job submission to the reconstruction server
- DUSt3R-family inference and reconstruction processing on the remote GPU server
- 3D result generation and quality evaluation
- Result packaging and return to the ground-side system
- Ground-side fixed-frame visualization metadata generation for validation UI
- Accumulated map input/output contract for appending multiple reconstruction chunks

### 2.2 Out of Scope

- UWB-based position estimation (see 04-uwb-requirements.md)
- cFS application lifecycle control (see 07-cfs-integration-requirements.md)
- System-level coordinate frame alignment policy (see 06-pose-frame-alignment-requirements.md)
- Low-level camera device driver implementation

---

## 3. Input Requirements

### 3.1 Required Image Inputs

- **REC-IN-01**: The reconstruction module SHALL accept a set of input images captured by the drone.
- **REC-IN-02**: Each input image SHALL carry a unique identifier and an acquisition timestamp.
- **REC-IN-03**: The reconstruction module SHALL reject corrupted or undecodable images before job submission.
- **REC-IN-04**: The reconstruction module SHALL require at least the system-defined minimum image count before starting reconstruction. *(Minimum count: see OI-REC-01)*

### 3.2 Optional Auxiliary Inputs

- **REC-IN-05**: The reconstruction module SHALL accept camera intrinsic parameters as an optional input when available.
- **REC-IN-06**: The reconstruction module SHALL accept external camera pose, UWB position, or other localization data only as optional auxiliary metadata for job packaging and traceability. Primary sensor fusion and World / Map frame alignment SHALL be owned by the Pose / Frame Alignment module.
- **REC-IN-07**: Optional auxiliary inputs SHALL NOT be a mandatory precondition for starting reconstruction.
- **REC-IN-08**: The reconstruction module SHALL be capable of producing a reconstruction result using image inputs alone, without any auxiliary input.

### 3.3 Ground-Side Input Handling

- **REC-IN-09**: The ground-side computer SHALL receive input images and associated metadata before submitting a reconstruction request.
- **REC-IN-10**: The ground-side computer SHALL package reconstruction inputs and forward them to the remote reconstruction server.

---

## 4. Reconstruction Pipeline Requirements

### 4.1 Input Preparation

- **REC-PROC-01**: The reconstruction module SHALL validate image completeness and metadata consistency before launching reconstruction.
- **REC-PROC-02**: The reconstruction module SHALL record the number of input images, image resolution, and metadata availability at job start.
- **REC-PROC-03**: The reconstruction module SHALL support preprocessing steps required by the selected DUSt3R-family pipeline.

### 4.2 Reconstruction Processing

- **REC-PROC-04**: The reconstruction module SHALL use a DUSt3R-family method as the primary reconstruction pipeline.
- **REC-PROC-05**: The reconstruction module SHALL estimate scene structure from image inputs using the selected DUSt3R-family model.
- **REC-PROC-06**: The reconstruction module SHALL be modularized so that the selected reconstruction model can be replaced, upgraded, or reconfigured without changing the module boundary contract.
- **REC-PROC-07**: When optional auxiliary pose or localization input defined in REC-IN-06 is provided, the reconstruction module SHALL use it only for auxiliary metadata packaging, traceability, or backend hints and SHALL NOT treat it as a required input or perform primary sensor fusion.
- **REC-PROC-08**: The reconstruction module SHALL continue to support image-only reconstruction when no auxiliary pose input is available.

### 4.3 Remote Execution

- **REC-PROC-09**: The ground-side computer SHALL submit reconstruction jobs to the remote GPU server.
- **REC-PROC-10**: The remote GPU server SHALL execute DUSt3R-family inference on an NVIDIA RTX A6000-class GPU environment.
- **REC-PROC-11**: The remote GPU server SHALL return reconstruction outputs and execution status to the ground-side computer after processing.
- **REC-PROC-12**: The reconstruction module SHALL preserve job identity between request and response so that returned outputs can be matched to the originating image set.
- **REC-PROC-13**: The reconstruction module SHALL record reconstruction failure status when remote execution fails, times out, or returns invalid outputs.
- **REC-PROC-13A**: The prototype remote execution path SHALL support the HTTP polling contract defined in 03-interface-specification.md Section 3.4 until the final transport is frozen.
- **REC-PROC-13B**: The ground-side client SHALL download completed reconstruction artifacts automatically after successful remote execution and SHALL pass the downloaded artifact to the fixed-frame visualization or downstream integration path.

### 4.4 Result Packaging

- **REC-PROC-14**: The reconstruction module SHALL package the 3D reconstruction output together with quality metadata and processing status.
- **REC-PROC-15**: The reconstruction module SHALL distinguish successful, degraded, and failed reconstruction outcomes. *(Criteria: see OI-REC-05)*
- **REC-PROC-16**: The reconstruction module SHALL make the returned result available to downstream alignment or integration modules through the defined interface.

### 4.5 Accumulated Map Handling

- **REC-PROC-17**: The ground-side reconstruction path SHALL own creation and append operations for the persistent accumulated map manifest.
- **REC-PROC-18**: Each map chunk SHALL preserve its originating reconstruction `job_id`, `image_set_id`, local artifact reference, output format, timestamp, quality metadata, and frame/alignment metadata.
- **REC-PROC-19**: The accumulated map SHALL NOT assume that independent reconstruction chunks already share a metric World / Map frame unless a valid Reconstruction-to-World transform is attached.
- **REC-PROC-20**: When a chunk does not have a valid World-frame alignment transform, the accumulated map SHALL store it as `UNALIGNED` or `PARTIAL_ALIGNMENT` rather than silently merging it as a final map.
- **REC-PROC-21**: The accumulated map update path SHALL allow the Pose / Frame Alignment module to update or replace chunk transform metadata through the manifest update interface without modifying the raw reconstruction artifact.
- **REC-PROC-22**: The accumulated map manifest SHALL be persisted as a ground-side file so that map state can be recovered after process restart.
- **REC-PROC-23**: The accumulated map append operation SHALL reject duplicate `job_id` entries by default unless an explicit replacement policy is configured.
- **REC-PROC-24**: The accumulated map SHALL support marking a chunk as invalidated without deleting the raw artifact.

---

## 5. Output Requirements

### 5.1 Reconstruction Output

- **REC-OUT-01**: The reconstruction module SHALL output a 3D reconstruction result in a system-defined representation. *(Current primary candidate: GLB; see OI-REC-03)*
- **REC-OUT-02**: The output SHALL include a reconstruction job identifier and a processing timestamp.
- **REC-OUT-03**: The output SHALL include the identifier of the input image set used to generate the reconstruction.
- **REC-OUT-04**: The reconstruction module SHALL be modularized so that the external output format can be changed in future revisions without requiring redesign of the full reconstruction module.

### 5.2 Quality Metadata

- **REC-OUT-05**: The reconstruction module SHALL include quality metadata in each output.
- **REC-OUT-06**: Quality metadata SHALL include, at minimum: the number of input images used, processing status, and one or more reconstruction quality indicators. *(Exact indicators: see OI-REC-04)*
- **REC-OUT-07**: The reconstruction module SHALL support quality evaluation against system-defined thresholds. *(Threshold values: see OI-REC-04)*

### 5.3 Failure and Degraded Output

- **REC-OUT-08**: When reconstruction fails, the module SHALL return a failure result structure that downstream modules can detect consistently.
- **REC-OUT-09**: When only partial or low-confidence reconstruction is available, the module SHALL mark the result as degraded.
- **REC-OUT-10**: All failure and degraded outputs SHALL carry an error or status code that is traceable through logs or status fields.

### 5.4 Fixed-Frame Visualization Output (Ground-Side Validation)

- **REC-OUT-11**: The reconstruction output SHALL expose camera trajectory metadata as defined in 03-interface-specification.md Section 3.3.
- **REC-OUT-12**: The reconstruction output SHALL expose fixed-frame visualization metadata as defined in 03-interface-specification.md Section 3.3.
- **REC-OUT-13**: The ground-side validation UI SHALL use the image linkage fields defined in 03-interface-specification.md Section 3.3.

### 5.5 Accumulated Map Output

- **REC-OUT-14**: The reconstruction module SHALL support an accumulated map manifest that references one or more reconstruction chunks.
- **REC-OUT-15**: The accumulated map manifest SHALL include map identifier, chunk list, artifact references, per-chunk alignment status, per-chunk transform metadata, and quality metadata.
- **REC-OUT-16**: The accumulated map viewer SHALL be able to render multiple chunks in a shared display frame while distinguishing unaligned chunks from aligned chunks.
- **REC-OUT-17**: The accumulated map output SHALL preserve traceability from each displayed map chunk back to its source images and reconstruction job.
- **REC-OUT-18**: Unaligned chunks SHALL be rendered in their own reconstruction frame for diagnostic visualization and SHALL be visually marked as non-metric map contributions.

---

## 6. Error Handling Requirements

- **REC-ERR-01**: The reconstruction module SHALL stop and reject processing when the minimum required image count is not satisfied.
- **REC-ERR-02**: The reconstruction module SHALL report corrupted or unusable input images in logs or status metadata before job submission.
- **REC-ERR-03**: The reconstruction module SHALL report remote server execution failure or timeout as a reconstruction failure condition.
- **REC-ERR-04**: The reconstruction module SHALL return a consistent degraded or failed status when the returned result quality is below the accepted threshold.
- **REC-ERR-05**: All failure cases SHALL be traceable through logs, status fields, or verification artifacts.

---

## 7. Performance Requirements

- **REC-PERF-01**: The reconstruction pipeline SHALL be executable on a remote GPU server environment separate from the ground-side receiver.
- **REC-PERF-02**: The reconstruction module SHALL support NVIDIA RTX A6000-class GPU execution as the baseline deployment target.
- **REC-PERF-03**: The reconstruction module SHALL record job execution outcome and processing duration for each reconstruction request.
- **REC-PERF-04**: Detailed runtime and throughput targets SHALL be finalized in the system requirements and verification plan. *(See OI-REC-06)*

---

## 8. Items to Be Defined in Interface Specification

The following items are reconstruction module boundary contracts that SHALL be
formally defined in 03-interface-specification.md:

- **REC-IFC-01**: The message structure for reconstruction job request (ground-side → server), including job ID, image payload reference, and optional auxiliary input fields.
- **REC-IFC-02**: The message structure for reconstruction result return (server → ground-side), including job ID, output format reference, quality metadata, and status/error code.
- **REC-IFC-03**: The error/status code enumeration for reconstruction outcomes (success, degraded, failed, timeout).
- **REC-IFC-04**: The quality metadata field definitions and their types.
- **REC-IFC-05**: The output format identifier field and the mechanism by which the output format can be changed without breaking the module boundary contract.
- **REC-IFC-06**: Timestamp convention for reconstruction job request and result messages (reference: 03-interface-specification.md Section 6).

---

## 9. Items to Be Defined in Verification Plan

The reconstruction verification cases and traceability SHALL be owned by
08-verification-plan.md. This module document only reserves the REC-VER-01
through REC-VER-13 requirement identifiers.

Reserved verification identifiers:

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
| REC-VER-10 | Accumulated map append |
| REC-VER-11 | Accumulated map rendering |
| REC-VER-12 | Raw artifact preservation |
| REC-VER-13 | Per-chunk alignment update and unaligned chunk handling |

---

## 10. Open Items

| ID         | Description                                                                                  | Owner | Status |
|------------|----------------------------------------------------------------------------------------------|-------|--------|
| OI-REC-01  | Minimum image count for starting DUSt3R-family reconstruction needs to be finalized.         | TBD   | Open   |
| OI-REC-02  | Camera intrinsic parameter provisioning method needs to be finalized.                        | TBD   | Open   |
| OI-REC-03  | GLB is the current primary external output format candidate; the officially frozen output format needs to be confirmed and recorded in the interface specification. | TBD | Open |
| OI-REC-04  | Reconstruction quality indicators and acceptance thresholds need to be finalized.            | TBD   | Open   |
| OI-REC-05  | Criteria distinguishing degraded versus failed reconstruction outcomes need to be finalized. | TBD   | Open   |
| OI-REC-06  | Runtime and throughput targets for the reconstruction pipeline need to be finalized.         | TBD   | Open   |
| OI-REC-07  | Prototype remote execution transport is resolved as HTTP polling and defined in 03-interface-specification.md Section 3.4. Authentication, retry policy, and event-driven alternatives remain future work. | HTTP polling prototype | Resolved for prototype |
| OI-REC-08  | Fixed-frame identifier and transform metadata fields for validation UI output need to be frozen in interface specification. | TBD | Open |
| OI-REC-09  | Accumulated map manifest schema is defined in 03-interface-specification.md Section 3.5. Default storage location remains to be finalized. | TBD | Partially resolved |
| OI-REC-10  | Default policy: UNALIGNED chunks may be displayed diagnostically but SHALL NOT be treated as metric map contributions. PARTIAL_ALIGNMENT criteria remain to be finalized. | TBD | Partially resolved |
