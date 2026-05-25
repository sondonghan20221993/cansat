# 02. 시스템 아키텍처

## 1. 개요

이 문서는 전체 아키텍처와 설계 의도를 설명한다.

현재 시스템 아키텍처의 baseline cFS 입력 경로는 다음 SB 메시지를 사용한다.

- `imu_app`가 publish하는 `IMU_STATE_MID (0x1901)`
- `gps_app`가 publish하는 `GPS_STATE_MID (0x1902)`
- `telemetry_app`가 publish하는 `TELEMETRY_STATUS_MID (0x1903)`
- `mavlink_bridge_app`가 publish하는 `FC_LOCAL_POS_MID (0x1905)`, `FC_ATTITUDE_MID (0x1906)`, `FC_GPS_RAW_MID (0x1907)`, `FC_ODOMETRY_MID (0x1908)` (조건부), `FC_EKF_STATUS_MID (0x1909)` (조건부), `MAVLINK_BRIDGE_STATUS_MID (0x190A)`

## 2. 모듈 구성

주요 모듈과 관계는 다음과 같다.

| 모듈 | 책임 | 의존 대상 |
| --- | --- | --- |
| GPS Interface | 전역 위치 획득 및 local frame 변환 지원 | GPS 수신기 |
| IMU Interface | 자세, 각속도, 가속도 획득 | IMU 센서 |
| MAVLink Bridge | FC MAVLink 바이트 스트림 수신, 파싱, cFS SB 메시지 변환 publish | Flight Controller (serial/UART) |
| Telemetry Interface | 통신 링크 상태 평가 및 publish | 임무 transport 경로 |
| Image / Video Path | 지상국 측 image/video 입력 전달 | camera capture 경로 |
| Reconstruction Module | image/sensor 입력 기반 3D reconstruction | camera 및 sensor 입력 |
| Pose / Frame Alignment Module | 좌표계 정렬 및 calibration | GPS, IMU, MAVLink FC state, camera, reconstruction 출력 |
| cFS Integration Layer | runtime integration, messaging, configuration, logging | 모든 기능 모듈 |

## 3. 모듈 책임

### 3.1 Reconstruction Module

- 입력 이미지와 센서 데이터를 처리한다
- 3D reconstruction 출력을 생성한다
- 품질 검사를 적용한다

### 3.2 GPS Interface

- GPS 위치 및 timestamp 데이터를 수신한다
- raw WGS84 측정값을 보존한다
- alignment module에 local-frame 변환 입력을 제공한다

### 3.3 IMU Interface

- 기체 자세, 각속도, 가속도 데이터를 수신한다
- IMU/body-frame 메타데이터를 보존한다
- alignment module에 orientation constraint를 제공한다

### 3.4 Telemetry Interface

- 활성 transport 경로에서 communication-link health를 평가한다
- `TELEMETRY_STATUS_MID (0x1903)`를 publish한다
- `ALIVE`, `DEGRADED`, `LOST` link-state 전이를 보고한다

### 3.5 Image / Video Path

- camera image 또는 video frame 입력을 지상국 측 reconstruction 경로로 전달한다
- raw image payload는 baseline cFS SB 경로를 직접 통과하지 않는다

### 3.6 Pose / Frame Alignment Module

- frame transform을 관리한다
- offset과 calibration을 적용한다
- 통합된 좌표 출력값을 생성한다
- GPS, IMU, MAVLink FC state (FC_LOCAL_POS_MID, FC_ATTITUDE_MID, FC_ODOMETRY_MID), camera, reconstruction 좌표계를 World / Map frame으로 정렬한다

### 3.7 cFS Integration Layer

- application lifecycle을 관리한다
- 메시지를 publish/subscribe 한다
- configuration, event, timer를 처리한다

## 4. 데이터 흐름

모듈 간 정보 이동 방식은 다음과 같다.

1. `imu_app`, `gps_app`, `telemetry_app`가 baseline 필수 SB 입력 집합을 publish한다. `mavlink_bridge_app`은 FC MAVLink 바이트 스트림을 수신하여 `FC_LOCAL_POS_MID`, `FC_ATTITUDE_MID`, `FC_GPS_RAW_MID` 등 typed SB 메시지로 변환하여 publish한다.
2. Reconstruction Module은 지상국 image/video 경로를 통해 전달된 image 입력을 소비한다.
3. Alignment 로직은 GPS, IMU, MAVLink FC state, camera, reconstruction source 출력을 공통 World / Map frame으로 변환한다.
4. cFS integration이 출력을 downstream consumer에 배포한다.

## 5. 모듈 간 연결 관계

| Source Module | Target Module | Interface Type | 비고 |
| --- | --- | --- | --- |
| GPS Interface | Pose / Frame Alignment Module | Data message | GPS 위치 및 timestamp |
| IMU Interface | Pose / Frame Alignment Module | Data message | 자세, 각속도, 가속도 |
| MAVLink Bridge | Pose / Frame Alignment Module | SB message | `FC_LOCAL_POS_MID (0x1905)`, `FC_ATTITUDE_MID (0x1906)`, `FC_ODOMETRY_MID (0x1908)` (선택적 alignment 입력) |
| MAVLink Bridge | cFS Integration Layer | SB message | `MAVLINK_BRIDGE_STATUS_MID (0x190A)` — bridge health 및 link state |
| Telemetry Interface | cFS Integration Layer | SB message | `TELEMETRY_STATUS_MID (0x1903)` |
| Image / Video Path | Reconstruction Module | Ground-side transfer path | camera image 또는 video frame 입력 |
| Reconstruction Module | Pose / Frame Alignment Module | Data message | 3차원 결과 및 메타데이터 |
| Pose / Frame Alignment Module | Reconstruction Module / Map Manifest | Metadata update | accumulated map manifest interface를 통한 per-chunk transform 및 `alignment_status` 갱신 |
| Pose / Frame Alignment Module | cFS Integration Layer | Data message | 통합 출력 |
| cFS Integration Layer | All Modules | Control/config interface | runtime control |

## 6. 아키텍처 제약조건

- 모듈 간 인터페이스 정의는 일관되게 유지해야 한다.
- 요구사항과 검증 사이의 traceability를 보존해야 한다.
- 가능하면 모듈 책임을 분리해야 한다.

## 7. 미정 항목

- `OI-ARCH-01`: 지상국 Raspberry Pi, 원격 GPU server, onboard/drone-side process 간의 배치 분할을 확정해야 한다.
- `OI-ARCH-02`: reconstruction 출력용 large artifact 전송 경로를 cFS SB payload 한계 및 저장 제약과 함께 확정해야 한다.
- `OI-ARCH-03`: reconstruction, alignment, cFS integration 간 failure isolation policy를 확정해야 한다.
