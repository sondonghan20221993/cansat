# 01. 시스템 요구사항

## 1. 목적

이 문서는 시스템의 전체 목표를 정의한다.

- 시스템이 존재하는 이유
- 해결하려는 임무 또는 운용 문제
- 예상 사용자 및 운용자

## 2. 범위

이 문서는 포함 범위와 제외 범위를 정의한다.

- 포함 기능
- 제외 기능
- 가정 및 제약조건

## 3. 시스템 구성요소

상위 수준 구성요소는 다음과 같다.

| 구성요소 | 설명 | 입력 | 출력 |
| --- | --- | --- | --- |
| GPS Interface | 사용 가능한 경우 전역 위치 측정값을 수신한다 | GPS 수신기 데이터 | GPS 위치/시간 메타데이터 |
| IMU Interface | 기체 자세, 각속도, 가속도 데이터를 수신한다 | IMU 센서 데이터 | IMU/body-frame motion 메타데이터 |
| MAVLink Bridge Module | Flight Controller에서 전달되는 MAVLink 바이트 스트림을 수신하고, 선택된 MAVLink 메시지를 파싱하여 하위 소비자를 위한 cFS SB 메시지로 변환한다 | MAVLink 바이트 스트림(serial/UART), FC MAVLink 메시지 | 파싱된 FC 상태 SB 메시지(위치, 자세, 속도, EKF 상태), bridge health/status |
| Reconstruction Module | 드론 이미지 세트를 기반으로 이미지 기반 3차원 복원 결과를 생성한다 | image set 메타데이터, 이미지 참조, 선택적 보조 pose/localization | reconstruction 결과 참조, 품질 메타데이터, camera trajectory 메타데이터 |
| Pose / Alignment Module | GPS, IMU, camera, MAVLink FC state, reconstruction 좌표계를 공통 World / Map frame으로 정렬한다 | source pose, transform, calibration parameter, reconstruction 메타데이터 | 정렬된 pose/transform 메타데이터, calibration status, source selection 메타데이터 |
| cFS Integration Layer | cFS app lifecycle, Software Bus 메시지, timer, configuration, event를 통해 runtime integration을 제공한다 | 모듈 메시지, timer event, configuration table, health/status event | publish된 SB 메시지, scheduled callback, event log, diagnostic telemetry |

## 4. End-to-End 데이터 흐름

전체 시스템에서 데이터가 흐르는 방식은 다음과 같다.

1. 센서 및 source 데이터가 수집된다. MAVLink Bridge Module은 FC MAVLink 바이트 스트림을 수신하여 다른 센서 경로와 독립적으로 cFS SB 메시지로 파싱한다.
2. GPS, IMU, MAVLink FC state, camera, image-source 메타데이터에 timestamp를 부여하고 패키징한다.
3. 위치 추정 및 reconstruction 처리를 수행한다.
4. 좌표계 정렬을 통해 시스템 World / Map frame으로 변환한다.
5. 결과를 integration layer를 통해 패키징하고 전달한다.

## 5. 공통 규칙

시스템 전반에 적용되는 규칙은 다음과 같다.

- **명명 규칙**: 모듈이 소유하는 메시지, artifact, manifest field는 안정적인 snake_case field name과 문서화된 ID를 사용해야 한다.
- **데이터 소유 규칙**: 각 모듈은 자신의 1차 출력 생성에 대한 책임을 가진다. 다른 모듈은 인터페이스 계약으로 명시된 metadata field만 갱신할 수 있다. raw reconstruction artifact는 alignment 또는 viewer 모듈이 수정해서는 안 된다.
- **로그 규칙**: log에는 timestamp, source module, severity, 사용 가능한 경우 status/error code, 관련 payload 또는 artifact reference를 포함해야 한다.
- **시간 동기화 규칙**: prototype interface에서 임시 직렬화 형식을 명시적으로 문서화하지 않는 한 `cFS_TIME`을 시스템 기준 timestamp로 사용한다.
- **결함 처리 원칙**: 누락되거나 성능이 저하된 센서 데이터는 명시적으로 보고해야 하며, 정상적인 fused output을 조용히 생성해서는 안 된다.
- **버전 호환 규칙**: 인터페이스 변경 시 가능한 경우 하위 호환 가능한 optional field를 유지해야 하며, 구현 전에 `03-interface-specification.md`를 먼저 갱신해야 한다.
- **모듈 선택 규칙**: GPS, IMU, MAVLink Bridge, camera, reconstruction을 포함한 센서/source 모듈은 임무 모드가 허용하는 경우 configuration으로 각각 enable/disable 가능해야 한다. 비활성화된 모듈은 관련 없는 모듈을 막지 말고 명시적인 unavailable/degraded status를 생성해야 한다.
- **통신 링크 분리 규칙**: 시스템은 LoRa telemetry link와 image/video link라는 두 개의 독립된 통신 링크 역할을 유지해야 하며, 각각의 health state를 독립적으로 추적해야 한다. LoRa link는 heartbeat, HK, status, fault/event, command traffic을 담당한다. image/video link는 image, video, large payload, reconstruction artifact traffic을 담당한다. 한 링크의 health state를 다른 링크의 상태로 추론해서는 안 된다.
- **Timestamp 기준 규칙**: control/health 경로의 모든 downlink 및 uplink 메시지는 차량 측에서 생성한 `cFS_TIME` timestamp를 권위 있는 event time으로 포함해야 한다. 지상국 수신 시각은 별도로 기록할 수 있지만, event correlation을 위한 기준 시각을 대체해서는 안 된다. 다만 image 및 video metadata의 최종 authoritative timestamp policy는 현재 baseline에서 확정되지 않았으며, 추후 통합 시험 결과에 따라 `cFS_TIME` 기반 정책 또는 ground-side receive-time approximation 정책 중 하나로 확정한다.
- **상관 식별자 규칙**: 동일한 차량 이벤트를 설명하는 메시지는 `03-interface-specification.md`에 정의된 `frame_id`, `job_id`, `seq` correlation field를 사용해야 한다. 지상국 소비자는 이 field를 이용해 동일 이벤트의 LoRa status 데이터와 image/video 데이터를 연결해야 한다.

## 6. 시스템 수준 요구사항

### 6.1 기능 요구사항

- 시스템은 센서/source 데이터 수집, reconstruction 출력 생성, 공통 World / Map frame 정렬을 위한 모듈식 파이프라인을 제공해야 한다.
- 시스템은 GPS, IMU, MAVLink FC state, camera, reconstruction 데이터를 독립적인 센서/source 입력으로 지원해야 한다.
- 시스템은 source별 측정값을 공통 World / Map 좌표계로 변환하기 전에 원래 형태로 보존해야 한다.
- 시스템은 alignment metadata가 준비되기 전까지 reconstruction 출력을 상대 reconstruction frame으로 유지할 수 있어야 한다.
- MAVLink Bridge Module은 Flight Controller MAVLink 메시지를 파싱하고 cFS SB 메시지로 변환해야 한다. raw MAVLink frame을 cFS Software Bus에 직접 전달해서는 안 된다.
- MAVLink Bridge Module은 독립적으로 enable/disable 가능해야 한다. 비활성화된 경우에도 다른 센서 또는 alignment 모듈의 정상 동작을 방해해서는 안 된다.
- 시스템은 LoRa telemetry link와 image/video link에 대해 별도의 health 및 state tracking을 유지해야 한다. 각 링크는 `03-interface-specification.md`에 정의된 `ALIVE`, `DEGRADED`, `LOST` 분류를 사용해 독립적인 link state를 보고해야 한다.
- 시스템은 control/health 경로의 모든 downlink 및 uplink 메시지 생성 시점에 차량 측 `cFS_TIME` timestamp를 부여해야 한다. 지상국 소비자는 이 차량 생성 timestamp를 cross-link correlation을 위한 기준 event time으로 사용해야 한다. image/video 경로의 timestamp 기준은 현재 미정이며, 추후 통합 시험 결과에 따라 별도 확정한다.
- 동일한 차량 이벤트를 설명하는 메시지에는 `frame_id`, `job_id`, `seq` correlation field를 포함해야 하며, 지상국 소비자가 LoRa status 데이터와 image/video 데이터를 연결할 수 있어야 한다.

### 6.2 성능 요구사항

- reconstruction runtime과 throughput 목표는 image set 단위로 측정해야 하며, prototype reconstruction backend benchmarking 이후 최종 확정한다.
- 시스템 수준 최대 지연 시간과 정확도 임계값은 `OI-SYS-01`에서 추후 확정한다.

### 6.3 신뢰성 요구사항

- 임무 모드가 허용하는 경우, 비활성화되거나 실패한 source 모듈을 격리하여 관련 없는 활성화 모듈이 계속 동작할 수 있어야 한다.
- 시스템은 누락된 GPS, IMU, MAVLink FC state, camera, reconstruction, alignment 데이터에 대해 명시적인 degraded/unavailable status를 제공해야 하며, nominal fused output을 조용히 publish해서는 안 된다.
- MAVLink Bridge parse failure 또는 serial connection loss는 degraded 또는 unavailable status로 보고해야 하며, downstream FC state consumer를 조용히 차단해서는 안 된다.
- availability target과 recovery timing은 `OI-SYS-01`, `OI-SYS-02`에서 추후 확정한다.

### 6.4 Runtime Configuration 및 복구 요구사항

- timing 관련 telemetry parameter는 activation 전에 pending configuration buffer를 통한 단계적 runtime update를 지원해야 한다.
- runtime update는 active configuration을 교체하기 전에 검증되어야 한다.
- 잘못된 runtime configuration 값은 active configuration을 덮어써서는 안 되며, event와 HK telemetry를 통해 보고해야 한다.
- active runtime configuration은 문서화된 safe application point에서만 교체되어야 한다.
- 시스템은 최소한 다음 reset/restart class를 구분해야 한다: app restart, cFS host soft reset, host hard reset 또는 power cycle.
- host hard reset 또는 power cycle 시에는 검증된 persistent state와 안전한 default configuration만 복원해야 한다.
- cFS host soft reset 시에는 persistent runtime configuration, cFS state, last known health state, 허용된 checkpoint를 복원해야 한다.
- 구성된 reboot-loop detection window 내에서 반복적인 복구 실패가 발생하면 minimum-reporting startup mode에 강제로 진입해야 한다.
- 필수적이지 않은 센서 또는 source failure는 임무 모드가 허용하는 경우 degraded startup 및 degraded nominal operation을 허용해야 한다.
- 필수 telemetry, command, 또는 health-management 경로의 failure는 nominal mission continuation 전에 recovery handling을 유발해야 한다.

### 6.5 운용 요구사항

- 시스템은 image-based reconstruction 또는 sequence-based SLAM 처리를 위한 원격 GPU reconstruction server와, 지상국 측 cFS-managed execution environment를 지원해야 한다.
- GPS, IMU, MAVLink Bridge serial device, camera, reconstruction endpoint, output format, module enable flag, alignment transform parameter는 startup 시 configuration 가능해야 한다.
- 정확한 배치 분할과 hardware dependency 목록은 `OI-SYS-02`에서 추후 확정한다.

### 6.6 안전 및 보안 요구사항

- prototype 운용 중 원격 reconstruction 접근은 구성된 endpoint 또는 tunnel로 제한해야 한다.
- large artifact는 path/URI reference로 다루어야 하며, cFS Software Bus 메시지에 조용히 내장해서는 안 된다.
- 최종 access control, data protection, fail-safe policy는 `OI-SYS-02`에서 추후 확정한다.

## 7. 미정 항목

- `OI-SYS-01`: 시스템 수준 latency, update-rate, accuracy target을 확정해야 한다.
- `OI-SYS-02`: 배치 환경, hardware dependency, security policy를 확정해야 한다.
