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
- **REC-IN-06**: The reconstruction module SHALL accept external camera pose, UWB position, or other localization data as optional auxiliary input when available.
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
- **REC-PROC-07**: When optional auxiliary pose or localization input is provided, the reconstruction module SHALL use it only as auxiliary information and SHALL NOT treat it as a required input.
- **REC-PROC-08**: The reconstruction module SHALL continue to support image-only reconstruction when no auxiliary pose input is available.

### 4.3 Remote Execution

- **REC-PROC-09**: The ground-side computer SHALL submit reconstruction jobs to the remote GPU server.
- **REC-PROC-10**: The remote GPU server SHALL execute DUSt3R-family inference on an NVIDIA RTX A6000-class GPU environment.
- **REC-PROC-11**: The remote GPU server SHALL return reconstruction outputs and execution status to the ground-side computer after processing.
- **REC-PROC-12**: The reconstruction module SHALL preserve job identity between request and response so that returned outputs can be matched to the originating image set.
- **REC-PROC-13**: The reconstruction module SHALL record reconstruction failure status when remote execution fails, times out, or returns invalid outputs.
- **REC-PROC-13A**: The prototype remote execution path SHALL support HTTP polling using `POST /jobs`, `GET /jobs/{job_id}`, and `GET /jobs/{job_id}/artifact` until the final transport is frozen.
- **REC-PROC-13B**: The ground-side client SHALL download completed reconstruction artifacts automatically after successful remote execution and SHALL pass the downloaded artifact to the fixed-frame visualization or downstream integration path.

### 4.4 Result Packaging

- **REC-PROC-14**: The reconstruction module SHALL package the 3D reconstruction output together with quality metadata and processing status.
- **REC-PROC-15**: The reconstruction module SHALL distinguish successful, degraded, and failed reconstruction outcomes. *(Criteria: see OI-REC-05)*
- **REC-PROC-16**: The reconstruction module SHALL make the returned result available to downstream alignment or integration modules through the defined interface.

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

- **REC-OUT-11**: The reconstruction output SHALL include camera trajectory metadata linked to input image identifiers for validation visualization.
- **REC-OUT-12**: The reconstruction output SHALL provide enough metadata for fixed-frame visualization, including frame identifier and transform parameters applied by the ground-side viewer.
- **REC-OUT-13**: The ground-side validation UI SHALL be able to select a camera trajectory node and identify the corresponding source image.

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

The following verification items SHALL be formally defined in
08-verification-plan.md:

- **REC-VER-01**: Verify successful end-to-end reconstruction using representative image sets (nominal scenario).
- **REC-VER-02**: Verify reconstruction behavior when no auxiliary pose input is provided (image-only path).
- **REC-VER-03**: Verify reconstruction behavior when optional auxiliary pose or localization input is provided.
- **REC-VER-04**: Verify failure handling for corrupted images, insufficient image count, and remote execution failure.
- **REC-VER-05**: Verify remote job submission from the ground-side computer to the A6000-class server and result return (integration test).
- **REC-VER-06**: Verify that the reconstruction pipeline can be replaced (model swap) without breaking the module boundary contract.
- **REC-VER-07**: Verify that the output format can be changed without requiring redesign of the full reconstruction module.
- **REC-VER-08**: Verify fixed-frame visualization consistency (point cloud + camera trajectory) under the selected frame preset and transform parameters.
- **REC-VER-09**: Verify image linkage in validation UI by selecting a camera node and confirming corresponding image identifier/path metadata.

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
| OI-REC-07  | Remote execution protocol between the ground-side computer and the A6000 server is currently HTTP polling for prototype use. Authentication, retry policy, and event-driven alternatives need to be finalized. | HTTP polling prototype | Open |
| OI-REC-08  | Fixed-frame identifier and transform metadata fields for validation UI output need to be frozen in interface specification. | TBD | Open |
