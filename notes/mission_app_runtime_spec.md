# 미션 앱 런타임 사양 초안

## 1. 목적

이 문서는 cFS 기반 시스템에 대한 1차 임무 애플리케이션 런타임 사양을 정의한다. 최소한의 MAVLink bridge 및 telemetry downlink 초기 구성을 마친 뒤, 후속 구현을 안내하는 것을 목적으로 한다.

주요 목표는 다음과 같습니다.

- 애플리케이션 책임과 소프트웨어 버스 메시지 계약을 정의합니다.
- 임무 수행 중에 변경될 수 있는 런타임 값을 정의합니다.
- 하드웨어 오류, 앱 오류, 다시 시작 및 재부팅 동작을 처리하는 방법을 정의합니다.
- 비정상적인 종료 및 재부팅을 견뎌야 하는 지속 상태를 정의합니다.
- 중요한 런타임 변경 사항에 대한 활성/보류 이중 버퍼링 규칙을 정의합니다.

이 문서는 초안입니다. 이는 현재 의도된 아키텍처를 포착하며 반드시
각 앱이 추가될 때마다 구현을 조정해야 합니다.

## 2. 범위 및 현황

현재 저장소에는 cFS sample app 패턴에서 파생된 최소 실행 가능 cFS 애플리케이션 기준 구현이 포함되어 있으며, 현재 다음 항목을 보여준다.

- cFS 앱 로드 및 시작.
- 소프트웨어 버스 파이프 생성 및 구독.
- HK 및 명령 처리.
- 미션 앱에 필요한 기본 메시지 라우팅 및 상태 게시 패턴.
- `NOMINAL`, `DEGRADED`과 같은 복구 지향 상태 처리 패턴,
  `LOST` 및 재시작 또는 복구 전환.

아래에 설명된 광범위한 미션 앱 세트는 아직 완전히 구현되지 않았습니다.
저장소. 이 사양은 후속 구현을 위한 대상 계약입니다.

**배포 현황 (2026-06-17 기준):**
- **배포됨** (`cpu1_cfe_es_startup.scr`): `mavlink_bridge_app`(prio50), `cfs_core_app`(55), `uplink_app`(57), `lora_tdm_app`(58) + lab apps(`ci_lab`, `to_lab`, `sch_lab`)
- **삭제됨**: `lora_fc_downlink_app`(downlink 역할을 `lora_tdm_app`으로 전환 후 저장소에서 제거 — commit `7c080f1`)
- **미배포·코드 보존**: `telemetry_app`, `legacy/img_app`(`img_app`은 향후 항목으로 `legacy/`로 이동)
- **SCH_LAB 스케줄**: `mission_defs/tables/cpu1_sch_lab_table.c`로 커스텀 앱 4개 SEND_HK ~1Hz 스케줄링 (2026-06-17 추가)

이 초안에서 별도 구현체가 확정되지 않은 공통 기능은 다음 임시 기준을 따른다.

- `recovery authority`: 별도 health/safety app이 정의되기 전까지 `cfs_core_app`이 담당한다.
- 내부 mission MID timestamp 기준: 별도 절대 시간 정책이 확정되기 전까지 `CFE_TIME` 기반 mission elapsed millisecond를 사용한다.
- persistent storage backend: 별도 저장소 백엔드가 확정되기 전까지 Raspberry Pi 파일 시스템의 atomic record write를 기준 구현으로 간주한다.

## 3. 플랫폼 경계 모델

임무 센서와 비행 제어 하드웨어는 MicoAir H743 V2 비행 컨트롤러 보드에 물리적으로 연결된다. Raspberry Pi는 cFS 호스트이자 비행 컨트롤러와 지상국 사이의 통신 브리지 역할을 수행한다. 기본 FC-Raspberry Pi 텔레메트리 경로는 MAVLink 메시지를 전달하는 UART 링크이다.

기본 플랫폼 책임:

| 요소 | 책임 |
| --- | --- |
| 비행 컨트롤러 보드 | 직접 센서 인터페이스, 비행 제어 루프, 액추에이터 제어 및 비행 필수 상태를 소유합니다. |
| 비행 컨트롤러 펌웨어 | FC가 노출하는 센서 드라이버, 차량 안정화, 내비게이션 추정치 및 FC 수준 상태를 소유합니다. |
| 라즈베리 파이 | cFS 앱 호스팅, FC 데이터 수신, 미션 MID 패키징, 텔레메트리 전달, 통신 경로 복구 관리 |
| cFS 앱 | 수신된 데이터의 유효성을 검사하고, 소프트웨어 버스 상태를 게시하고, 링크/앱 상태를 모니터링하고, 오류를 보고하고, 비행 제어 이외의 복구 결정을 내립니다. |
| 지상국 | 텔레메트리를 수신하고 승인된 명령을 전송하며, 운용자 수준 모니터링을 수행한다. |
| 카메라 및 WiFi 모듈 | 이미지 데이터를 직접 지상국으로 전송하는 독립 통신 경로 제공 |
| 지상국 서버 | 수신된 이미지 데이터를 복원 연산하고, 복원 결과를 지상국 화면에 표시 |

이미지 데이터 경로(카메라 및 WiFi 모듈 → 지상국 → 지상국 서버)는 cFS 소프트웨어 버스를 거치지 않는 독립 통신 경로이며, 본 cFS 앱 책임 모델(Section 4)의 범위 밖이다. 따라서 이미지 촬영·전송·복원은 어떤 cFS 앱의 publish/subscribe MID 계약에도 포함되지 않는다. 레포지토리의 `legacy/img_app`(향후 항목으로 `legacy/`에 보관)은 현재 baseline 미션 앱 집합에 포함되지 않으며, cFS와의 연계(예: 이미지 메타데이터 또는 카메라 상태의 MID 게시)가 필요해질 경우 별도 MID 계약과 함께 Section 4에 명시적으로 추가한다.

Raspberry Pi에서 실행되는 cFS 앱은 직접적인 전기 제어를 가정하지 않습니다.
비행 컨트롤러에 연결된 센서를 통해. 센서 재설정, 센서
전원 주기, 비행 제어 재설정, 액추에이터 제어 및 비행
컨트롤러 재부팅은 기본 cFS 복구 권한을 벗어납니다.

본 문서에서 "센서 복구"라는 용어는 논리적 또는 통신 경로를 의미합니다.
하드웨어 설계가 나중에 명시적으로 승인된 사항을 노출하지 않는 한 복구
비행에 영향을 주지 않는 소프트웨어 제어 가능 재설정 또는 전원 인터페이스
안정.

기본적으로 허용되는 cFS 측 복구 작업:

- Raspberry Pi 통신 엔드포인트를 다시 열거나 다시 초기화합니다.
- FC 텔레메트리 스트림을 재구독, 재시작 또는 재요청한다.
- 데이터 소스를 유효하지 않음, 성능 저하, 손실 또는 실패로 표시합니다.
- 텔레메트리에 오류 및 복구 상태를 게시합니다.
- 나머지 유효한 온보드 데이터 소스만 사용하여 저하된 작업을 계속합니다.

기본적으로 금지된 cFS 측 복구 작업:

- 비행 컨트롤러 재부팅.
- 비행 제어 펌웨어 재설정.
- 모터 또는 액추에이터 명령.
- FC를 통해 센서 전원을 껐다 켭니다.
- 안정성에 영향을 미칠 수 있는 비행 모드 또는 비행 제어 매개변수 변경.

이 기준선에 대한 예외는 별도의 임무 안전 정책과
명시적인 명령 권한 부여.

향후 임무 페이로드 경로에 대한 하드웨어 연결 토폴로지는 장치별 복구를 활성화하기 전에 문서화해야 한다. 토폴로지가 정의되기 전까지 해당 경로의 복구는 데이터 유효성 처리, 로컬 파서 또는 전송 복구, 텔레메트리 보고, 성능 저하 운용으로 제한한다.

## 4. 애플리케이션 책임 모델

각 애플리케이션은 하나의 명확한 책임을 갖고 해당 상태를 다음을 통해 게시해야 합니다.
MID 계약을 정의했습니다. 앱은 다른 앱이 소유한 앱을 직접 덮어쓰면 안 됩니다.
상태.

| 앱 | 책임 | Publish MID | Subscribe MID |
| --- | --- | --- | --- |
| `mavlink_bridge_app` | FC가 제공하는 MAVLink 텔레메트리를 수신하고 임무 상태 필드를 추출하여 임무 상태 MID를 게시한다. | `FC_EKF_LOCAL_STATE_MID`, `FC_ATTITUDE_STATE_MID`, `FC_GPS_RAW_STATE_MID`, `FC_EKF_STATUS_MID`, `MAVLINK_BRIDGE_APP_HK_TLM_MID` | FC의 UART 기반 MAVLink 입력 |
| `cfs_core_app` | 수신된 임무 상태를 검증하고, 상태 및 복구 정책을 관리하며, 시스템 상태를 게시한다. | `SYSTEM_HEALTH_MID` | `FC_EKF_LOCAL_STATE_MID`, `FC_ATTITUDE_STATE_MID`, `FC_GPS_RAW_STATE_MID`, `FC_EKF_STATUS_MID`, 앱 health/status MID |
| `lora_tdm_app` (**downlink 역할 현재 배포 구현체**, 2026-06-16~) | TDM(Time-Division Multiplexing) 방식으로 FC 상태·시스템 헬스를 LoRa를 통해 지상국으로 다운링크하고, 지상국 uplink 원문을 수신하여 `uplink_app`으로 전달한다. | `LORA_TDM_APP_HK_TLM_MID`(`0x08E0`), `LORA_TDM_APP_LINK_STATUS_MID`(`0x1911`) | `FC_EKF_LOCAL_STATE_MID`, `FC_ATTITUDE_STATE_MID`, `FC_GPS_RAW_STATE_MID`, `FC_EKF_STATUS_MID`, `FC_SYS_TIME_MID`(`0x1909`), `SYSTEM_HEALTH_MID`, `UPLINK_STATUS_MID`(`0x190A`), `DIAGNOSTIC_CMD_MID`(`0x1910`) |
| `lora_fc_downlink_app` (**삭제됨**, 구 downlink 구현체) | Software Bus에서 승인된 임무 상태 및 텔레메트리 MID를 수집하고, downlink packet을 구성하여 지상국으로 전송했다. `lora_tdm_app`으로 대체되어 `cpu1_cfe_es_startup.scr`에서 제거된 뒤 저장소에서도 삭제됨(commit `7c080f1`). 아래는 이력 참고용. | 자체 HK(`LORA_FC_DOWNLINK_APP_HK_TLM_MID`)만 게시 | `FC_EKF_LOCAL_STATE_MID`, `FC_ATTITUDE_STATE_MID`, `FC_GPS_RAW_STATE_MID`, `FC_EKF_STATUS_MID`, `SYSTEM_HEALTH_MID` |
| `uplink_app` | 지상국 명령을 수신하고 업링크 패킷을 검증한 뒤, 승인된 런타임 설정, 경로 수정, viewpoint, 복구 명령을 임무 앱으로 전달한다. | `UPLINK_STATUS_MID`, `ROUTE_UPDATE_MID`(검증 후 publish) | `UPLINK_APP_CMD_MID` ingress 또는 승인된 전송 입력 |

별도 recovery authority 앱이 정의되기 전까지 `cfs_core_app`은 시스템 수준 복구 판단과 복구 요청 집계를 담당하는 recovery authority로 간주한다. 본 문서에서 "복구 권한" 또는 "recovery authority"에 요청한다고 기술된 경우, 현재 기준 구현 대상은 `cfs_core_app`이다.

## 5. MID 계약 규칙

모든 임무 MID에는 문서화된 소유자, 생산자, 소비자 목록이 있어야 합니다.
페이로드 레이아웃, 게시 속도, 유효성 규칙 및 오류 동작.

명시적으로 예외가 정의되지 않는 한, 각 상태 또는 텔레메트리 페이로드에는 다음 항목이 포함되어야 한다.

- `Timestamp`: 측정 또는 생성 시간입니다.
- `TimeValid`: 타임스탬프를 신뢰할 수 있는지 여부입니다.
- `SequenceCounter`: 오래되고, 중복되고, 삭제된 데이터 감지를 위한 단조 카운터입니다.
- `Valid`: 소비자가 페이로드를 사용할 수 있는지 여부입니다.
- `HealthState`: `NOMINAL`, `DEGRADED`, `LOST` 또는 `FAILED`.
- `FaultCode`: 마지막 또는 현재 오류 이유.
- `SourceId`: 센서, 전송 또는 앱 인스턴스 식별자입니다.
- `AgeMs`: 해당되는 경우 마지막으로 알려진 양호한 값의 연령입니다.

타임스탬프 기준은 상태 상관관계 및 건강 정책 이전에 정의되어야 합니다.
구현. 후보 기지는 cFS 임무 경과 시간, GPS 시간, Unix
시간 또는 단조로운 플랫폼 시계. `cfs_core_app`은(는) 결합하거나 비교할 수 없습니다.
시간 기준이나 시간 유효성을 알 수 없는 입력입니다.

현재 기준 시간 정책:

- 모든 내부 mission MID의 `Timestamp`는 `CFE_TIME` 기반 mission elapsed millisecond를 사용한다.
- `TimeValid=true`는 생산자가 해당 값을 위 기준으로 채웠고 로컬 시간 취득 오류가 없을 때만 설정한다.
- GPS 시간, Unix 시간 또는 기타 절대 시간 기준은 별도 payload 필드로 병행 보고할 수 있으나, `cfs_core_app`의 기본 freshness/ordering 검증은 mission elapsed millisecond를 기준으로 수행한다.
- 다른 시간 기준을 사용하는 입력은 변환 규칙이 정의되기 전까지 `cfs_core_app` freshness 판단의 직접 입력으로 사용할 수 없다.

### 5.1 MID 계약 테이블 템플릿

각 임무 MID는 이전에 다음 계약 필드를 사용하여 지정됩니다.
제작 앱이 구현되었습니다.

| 필드 | 의미 |
| --- | --- |
| MID 이름 | 기호 메시지 ID |
| 소유자 앱 | 페이로드 정의를 담당하는 앱 |
| 생산자 | MID를 게시하는 앱 또는 구성 요소 |
| 소비자 | MID를 구독하는 앱 |
| 명령 MID | 해당하는 경우 소유 앱에 명령을 보내는 데 사용되는 MID |
| 출판률 | 주기율 또는 이벤트 중심 조건 |
| 페이로드 레이아웃 | 구조체 정의 또는 필드 목록 참조 |
| 유효성 규칙 | `Valid=true`의 필수 조건 |
| 오류 동작 | 생산자의 성능이 저하되거나 손실되거나 실패할 때의 동작 |
| 시간 기준 | 타임스탬프 소스 및 단위 |
| 시퀀스 규칙 | 카운터 증가 및 줄 바꿈 동작 |

완전한 MID별 계약 테이블은 각 앱을 구현하기 전에 채워져야 한다.
앱별 MID 계약 테이블은 각 앱 구현 전에 채워야 하며, baseline MID 값은 Section 17.1을 따른다.

### 5.1.1 Baseline MID 계약 테이블 (8개 MID)

아래 표는 현재 구현 기준에서 즉시 검증 가능한 baseline 계약이다. 각 앱 구현은 본 표를 계약 원본으로 사용한다.

| MID 이름 | 소유자 앱 | 생산자 | 소비자 | 명령 MID | 출판률 | 페이로드 레이아웃 | 유효성 규칙 | 오류 동작 | 시간 기준 | 시퀀스 규칙 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `FC_ATTITUDE_STATE_MID` (`0x1906`) | `mavlink_bridge_app` | `mavlink_bridge_app` | `cfs_core_app`, `lora_tdm_app` | `MAVLINK_BRIDGE_APP_CMD_MID` (`0x18A0`) | MAVLink `ATTITUDE` 수신 시 (stream 요청 5 Hz / 200ms, `ATTITUDE_INTERVAL_US`; 실제 publish는 FC 송신율 의존) | Section 6.1 (논리명 `IMU_STATE_MID`) | MAVLink `ATTITUDE` 기반 필수 필드 파싱 성공, `TimeValid=true`, 데이터 범위 검사 통과 | 파싱 실패/timeout 시 `Valid=false`, `Stale=1`, 오류 코드 반영 | `CFE_TIME` mission elapsed ms | 생산자 로컬 단조 증가, wrap 허용, 역행/중복은 소비자가 stale로 처리 |
| `FC_EKF_STATUS_MID` (`0x1908`) | `mavlink_bridge_app` | `mavlink_bridge_app` | `cfs_core_app`, `lora_tdm_app` | `MAVLINK_BRIDGE_APP_CMD_MID` (`0x18A0`) | MAVLink `EKF_STATUS_REPORT` 수신 시 (stream 요청 2 Hz / 500ms, `EKF_STATUS_INTERVAL_US`; 실제는 FC 송신율 의존) | Section 6.3 (논리명 `EKF_STATE_MID`) | `EKF_STATUS_REPORT` 기반 필수 상태 플래그 파싱 성공 | Flags=0/invalid/stale 시 `Valid=false`, FaultCode 반영 | `CFE_TIME` mission elapsed ms | 생산자 로컬 단조 증가, wrap 허용 |
| `FC_GPS_RAW_STATE_MID` (`0x1907`) | `mavlink_bridge_app` | `mavlink_bridge_app` | `cfs_core_app`, `lora_tdm_app` | `MAVLINK_BRIDGE_APP_CMD_MID` (`0x18A0`) | MAVLink `GPS_RAW_INT` 수신 시 (stream 요청 2 Hz / 500ms, `GPS_RAW_INTERVAL_US`; 실제는 FC 송신율 의존) | Section 6.2 (논리명 `GPS_STATE_MID`) | MAVLink `GPS_RAW_INT` 기반 필수 필드 파싱 성공, fix/type 정책 통과 시 `Valid=true` | fix 미달/timeout 시 `Valid=false`, `Stale=1`, 오류 코드 반영 | `CFE_TIME` mission elapsed ms | 생산자 로컬 단조 증가, wrap 허용 |
| `FC_EKF_LOCAL_STATE_MID` (`0x1905`) | `mavlink_bridge_app` | `mavlink_bridge_app` | `cfs_core_app`, `lora_tdm_app` | `MAVLINK_BRIDGE_APP_CMD_MID` (`0x18A0`) | MAVLink `LOCAL_POSITION_NED` 또는 `GLOBAL_POSITION_INT` 수신 시 (stream 요청 5 Hz / 200ms, `LOCAL/GLOBAL_POSITION_INTERVAL_US`; 실제는 FC 송신율 의존) | Section 6.3 (논리명 `EKF_STATE_MID`) | 로컬 위치/속도 필수 필드 파싱 성공 | 파싱 실패/timeout 시 `Valid=false`, `Stale=1` | `CFE_TIME` mission elapsed ms | 생산자 로컬 단조 증가, wrap 허용 |
| `FC_SYS_TIME_MID` (`0x1909`) | `mavlink_bridge_app` | `mavlink_bridge_app` | `lora_tdm_app` (DL2 SysTime 확장 블록) | `MAVLINK_BRIDGE_APP_CMD_MID` (`0x18A0`) | MAVLink `SYSTEM_TIME` 수신 시 (stream 요청 1 Hz, `SYS_TIME_INTERVAL_US`; 실제는 FC 송신율 의존) | mavlink_bridge_app_behavior_spec.md §16 | GPS unix 시각 필드 파싱 성공, `TimeValid=true` | 파싱 실패/미수신 시 `TimeValid=false` | `CFE_TIME` mission elapsed ms | 생산자 로컬 단조 증가, wrap 허용 |
| `UPLINK_APP_HK_TLM_MID` (`0x08D0`) | `uplink_app` | `uplink_app` | `cfs_core_app` (생존 감시 — 5s 타임아웃 시 DEGRADED + 자동 재시작, cfs_core spec §13.6) | `UPLINK_APP_SEND_HK_MID` (`0x18D1`) | 1 Hz (HK request) | Section 18.7 | HK 카운터/상태 필드 갱신 | — | `CFE_TIME` mission elapsed ms | HK 카운터 단조 증가 |
| `LORA_TDM_APP_HK_TLM_MID` (`0x08E0`) | `lora_tdm_app` | `lora_tdm_app` | `cfs_core_app` (생존 감시 — 5s 타임아웃 시 DEGRADED + 자동 재시작, cfs_core spec §13.7) | `LORA_TDM_APP_SEND_HK_MID` (`0x18E1`) | 1 Hz (HK request) | lora_tdm_app_behavior_spec.md | HK 카운터/상태 필드 갱신 | — | `CFE_TIME` mission elapsed ms | HK 카운터 단조 증가 |
| `MAVLINK_BRIDGE_APP_HK_TLM_MID` (`0x08A0`) | `mavlink_bridge_app` | `mavlink_bridge_app` | `cfs_core_app`, `uplink_app`(FC MISSION_ACK 결과 캐시 — §18.7) | `MAVLINK_BRIDGE_APP_CMD_MID` (`0x18A0`), `MAVLINK_BRIDGE_APP_SEND_HK_MID` (`0x18A1`) | 1 Hz (HK request) | Section 6.4 (논리명 `BRIDGE_STATUS_MID`) | 링크 상태 평가 주기 내 필수 카운터/상태 필드 갱신 | open/reopen 실패, parser error 누적, timeout 시 오류 카운터/상태 반영 | `CFE_TIME` mission elapsed ms | HK 카운터 단조 증가 |
| `SYSTEM_HEALTH_MID` (`0x1904`) | `cfs_core_app` | `cfs_core_app` | `lora_tdm_app`, `uplink_app`, 운영자 모니터링 소비자 | `CFS_CORE_APP_CMD_MID` (`0x18C0`) | 1 Hz periodic + 상태 전이 이벤트 | Section 6.5 | 필수 입력(자세/EKF/bridge) freshness/유효성 규칙 통과 (GPS는 헬스 비반영, `GpsValid`로 보고만 — §15 GPS 정책) | 입력 부족 시 `CFS_DEGRADED` 또는 `CFS_RECOVERY` 게시, FaultCode로 원인 구분 | `CFE_TIME` mission elapsed ms | 생산자 로컬 단조 증가, wrap 허용 |
| `UPLINK_STATUS_MID` (`0x190A`) | `uplink_app` | `uplink_app` | `cfs_core_app`, 운영자 모니터링 소비자 | `UPLINK_APP_CMD_MID` (`0x18D0`) | 1 Hz periodic + 명령 처리 결과 이벤트 | Section 18.7 | 프레임 검증/라우팅 처리 결과를 상태 필드에 반영 | CRC/길이/인증/시퀀스 실패 시 reject 카운터와 오류 코드 게시 | `CFE_TIME` mission elapsed ms | 수락된 uplink command sequence는 단조 증가, 회귀/중복 거부 |
| `ROUTE_UPDATE_MID` (`0x190B`) | `cfs_core_app` | `uplink_app`(입력 생산), `cfs_core_app`(cache 반영 상태 생산) | `cfs_core_app`(입력 소비), 임무 경로 소비자 앱 | `UPLINK_APP_CMD_MID` (`0x18D0`) ingress, 내부 route 반영 인터페이스 | 이벤트 기반(유효 route update 수락 시) | Section 18.5.2 route payload + Section 6.5 연계 상태 | waypoint 개수(`1..16`), 필드 범위, route version/sequence, CRC/길이, 인접 waypoint 거리(`2m..2m`) 검증 통과 | 검증 실패 시 `uplink_app`에서 거부, `UPLINK_STATUS_MID`에 원인 게시, 기존 active route 유지 | `CFE_TIME` mission elapsed ms | route update sequence는 소스별 단조 증가, 회귀/중복은 거부 |
| `FC_MISSION_READBACK_MID` (`0x1914`, BL-41 route 2026-07-23) | `mavlink_bridge_app` | `mavlink_bridge_app` (FC 미션 다운로드 완료 시) | `cfs_core_app` (`MissionRoute` RAM 캐시 갱신 — FC가 유일 진실원본, 파일 영속화 없음) | 트리거 3종: FC 링크 CONNECTED 전이 / 미션 업로드 완료 / `MISSION_QUERY_CC`(0x18A0 CC=2) | 이벤트 기반(다운로드 완료 시) | `ROUTE_UPDATE_TLM_t` 레이아웃 재사용 (`RouteType=1` MISSION 고정, `SourceSequence=0`, lat/lon→로컬 미터 역변환 — mavlink spec §10) | 다운로드 상태머신 CRC/타임아웃 통과분만 게시, `WaypointCount=min(N,16)` 클램프 | timeout 시 지수 백오프(1→2→4→5s 상한) 무한 재시도, DISCONNECTED 전이 시 중단 | `CFE_TIME` mission elapsed ms | 생산자 로컬 단조 증가(`MissionReadbackSeq`) |

> **MID 정합 주의**: 위 표는 **실제 코드 config 헤더의 MID 값**을 계약 원본으로 한다. 이전 초안의 aspirational MID(`IMU_STATE_MID 0x1900`, `GPS_STATE_MID 0x1901`, `EKF_STATE_MID 0x1902`, `BRIDGE_STATUS_MID 0x1903`)는 구현되지 않았으며, Section 6의 논리적 페이로드 이름으로만 남는다(대응은 6장 상단 매핑 표 참조). `DOWNLINK_STATUS_MID`는 미구현이며 `0x1905`는 `FC_EKF_LOCAL_STATE_MID`에 할당되어 사용할 수 없다 — (역사 참고) 폐기·삭제된 `lora_fc_downlink_app`은 별도 status MID 없이 자체 HK(`LORA_FC_DOWNLINK_APP_HK_TLM_MID`)만 publish했다.

### 5.2 시간 기준 유효성 정책

페이로드 시간 기준은 생산자가 선언한 경우에만 알려진 것으로 간주됩니다.
타임스탬프 소스, 타임스탬프 단위 및 `TimeValid=true`.

`cfs_core_app`은(는) 다음과 같은 경우 개별 입력을 거부하거나 다운그레이드해야 합니다.

- 해당 입력에 대해서는 `TimeValid=false`입니다.
- 타임스탬프 소스를 알 수 없습니다.
- 타임스탬프 단위를 알 수 없습니다.
- 타임스탬프 기간이 구성된 유효 기간을 초과합니다.
- 시퀀스 카운터는 오래되거나 중복되거나 순서가 잘못된 데이터를 나타냅니다.
  구성된 공차.
- 필수 입력은 정의된 변환 규칙 없이 호환되지 않는 시간축을 사용합니다.

`TimeValid=false`으로 입력을 거부해도 자동으로 무효화되지는 않습니다.
전체 시스템 출력. 나머지 입력이 있는 경우 게시된 상태를 계속 사용할 수 있습니다.
활성 임무 정책을 충족합니다. 남은 입력이 부족한 경우
현재 모드인 `cfs_core_app`은 성능이 저하되거나 유효하지 않은 시스템 상태를 게시해야 합니다.

임무가 수행되는 경우에만 상대적인 시간 측정을 위해 단조로운 플랫폼 시계를 사용할 수 있습니다.
현지 주문 및 연령 확인이 필요합니다. GPS 시간 또는 다른 절대 시간 기준
임무 정책에 따라 절대적인 시기가 필요한 경우에만 필요합니다.

시간 기준 선택 및 유효성 규칙은 구현 전에 정의되어야 한다. 현재 기준 시간 정책은 Section 17.8에서 고정하며, `cfs_core_app` 유효성 검사 구현은 그 기준을 따라야 한다.

## 6. 최소 페이로드 후보

> **주의 (baseline 매핑)**: 본 섹션 제목(`IMU_STATE_MID` 등)은 **payload 필드 설계 후보의 논리적 이름**이며 실제 구현 MID가 아니다. 구현 MID와 값은 Section 5.1.1을 따른다. 논리 이름 → baseline MID 대응:
>
> | 논리 이름 (Section 6) | baseline MID (Section 5.1.1) | 비고 |
> | --- | --- | --- |
> | `IMU_STATE_MID` | `FC_ATTITUDE_STATE_MID` (`0x1906`) | FC가 raw IMU를 직접 노출하지 않아 자세 기반 |
> | `GPS_STATE_MID` | `FC_GPS_RAW_STATE_MID` (`0x1907`) | |
> | `EKF_STATE_MID` | `FC_EKF_LOCAL_STATE_MID` (`0x1905`), `FC_EKF_STATUS_MID` (`0x1908`) | 로컬 위치/추정기 상태로 분리 |
> | `BRIDGE_STATUS_MID` | `MAVLINK_BRIDGE_APP_HK_TLM_MID` (`0x08A0`) | 별도 bridge status MID 미구현, HK로 보고 |
> | `SYSTEM_HEALTH_MID` | `SYSTEM_HEALTH_MID` (`0x1904`) | 구현됨 |
> | `DOWNLINK_STATUS_MID` | (미구현) | `lora_fc_downlink_app`은 HK만 publish |

### 6.1 `IMU_STATE_MID`

최소 필드:

- 타임스탬프 및 시간 유효성.
- 시퀀스 카운터.
- 유효한 플래그 및 상태입니다.
- 가속도 X/Y/Z.
- 자이로 X/Y/Z.
- 선택적 자세 추정: 쿼터니언 또는 롤/피치/요.
- 센서 품질 또는 공분산 지표.
- 오류 코드.

### 6.2 `GPS_STATE_MID`

최소 필드:

- 타임스탬프 및 시간 유효성.
- 시퀀스 카운터.
- 유효한 플래그 및 상태입니다.
- 위도, 경도, 고도.
- 유형을 수정합니다.
- 위성 수.
- 정확도 또는 HDOP/VDOP.
- 오류 코드.

### 6.3 `EKF_STATE_MID`

최소 필드:

- 타임스탬프 및 시간 유효성.
- 시퀀스 카운터.
- 유효한 플래그 및 상태입니다.
- FC에서 사용 가능한 로컬 위치 X/Y/Z.
- FC에서 사용 가능한 경우 로컬 속도 X/Y/Z.
- EKF 상태 플래그 또는 이에 상응하는 FC 추정기 상태.
- 오류 코드.

### 6.4 `BRIDGE_STATUS_MID`

최소 필드:

- 타임스탬프 및 시간 유효성.
- 시퀀스 카운터.
- 유효한 플래그 및 상태입니다.
- 활성 전송 ID입니다.
- 마지막으로 유효한 업데이트 기간.
- 파서 오류 수.
- 스트림 시간 초과 횟수.
- 복구 횟수.
- 오류 코드.

`BRIDGE_STATUS_MID`의 의미는 좁게 유지되어야 합니다. 이는 FC-to-Pi를 나타냅니다.
전체 시스템 상태가 아닌 브리지 및 통신 상태입니다.

### 6.5 `SYSTEM_HEALTH_MID`

최소 필드:

- 타임스탬프 및 시간 유효성.
- 시퀀스 카운터.
- 유효한 플래그 및 상태입니다.
- 활성 cFS 상태.
- 입력별 요약: IMU, GPS, EKF, 브리지 및 다운링크 상태.
- 복구 모드 또는 에스컬레이션 단계.
- 마지막 복구 조치.
- 오류 코드.

### 6.6 `DOWNLINK_STATUS_MID` (미구현)

> **미구현**: 아래는 향후 별도 downlink status MID를 둘 경우의 payload 후보이다. (역사 참고) 폐기·삭제된 `lora_fc_downlink_app`은 별도 status MID 없이 자체 HK(`LORA_FC_DOWNLINK_APP_HK_TLM_MID`)만 publish했으며, `0x1905`는 `FC_EKF_LOCAL_STATE_MID`에 할당되어 `DOWNLINK_STATUS_MID`로 사용할 수 없다.

최소 필드:

- 타임스탬프 및 시간 유효성.
- 시퀀스 카운터.
- 유효한 플래그 및 상태입니다.
- 활성 전송 ID입니다.
- 마지막 전송 시간.
- 전송 횟수.
- 전송 오류 수.
- 마지막 전송 오류 코드입니다.
- 오류 코드.

기준 publish rate:

- `DOWNLINK_STATUS_MID`: 1 Hz periodic publish를 기본값으로 한다.
- 상태 전이 또는 전송 오류 burst가 발생한 경우 추가 event-driven publish를 허용한다.

## 7. 런타임 변경 가능 매개변수

런타임 변경은 세 가지 메커니즘으로 구분됩니다.

| 기구 | 목적 | 예 |
| --- | --- | --- |
| 명령 버퍼 | 즉시 또는 일회성 조치 | 활성화/비활성화, 재설정 요청, 진단 캡처 |
| 테이블/구성 버퍼 | 검증된 런타임 구성 | 임계값, 시간 초과, 교정, 재시도 제한 |
| 영구 저장 | 재부팅 후에도 유지되는 상태/구성 | 부팅 카운터, 선택한 구성 버전, 맵 체크포인트 |

앱은 수신되는 검증되지 않은 런타임 매개변수를 직접 적용해서는 안 됩니다.
버퍼. 각 앱은 범위, 버전, 체크섬 또는 CRC 및 현재의 유효성을 검사해야 합니다.
활성화 전 모드 호환성.

각 앱은 지상 또는 시스템 명령을 수신하기 위한 명령 MID를 정의해야 합니다.
명령 라우팅은 명령 권한 부여가 완료되기 전에 정의되어야 합니다.
앱별 명령 MID와 명령 라우팅 정책의 baseline은 Section 17.1, Section 17.6, Section 17.7에서 고정한다.

런타임 변경 가능 예:

- 센서 활성화/비활성화.
- 센서 시간 초과 임계값.
- 품질 게이트.
- 복구 재시도 제한.
- 텔레메트리 링크 시간 초과 임계값.
- 진단 캡처 설정.
- 임무 승인을 받은 경우 교정 값.

런타임 제한 또는 부팅 시간 전용 예:

- 앱 작업 우선순위.
- 스택 크기.
- MID 할당.
- 파이프 깊이.
- 시동 순서.
- 중요한 앱 분류.

제한된 값을 변경하려면 앱을 다시 시작해야 합니다. Raspberry Pi/cFS 호스트
최종 운영 정책에 따라 재설정, 재부팅 또는 임무 재구축이 가능합니다.
비행 컨트롤러에 영향을 미치는 변경 사항은 기본 cFS 권한 외부에 있습니다.

## 8. 앱 기간 및 우선순위 정책

앱 기간과 앱 우선순위는 별도로 지정됩니다.

앱 기간은 다음을 통해 제어할 수 있습니다.

- SCH 깨우기 메시지이다.
- 앱-로컬 시간 초과 또는 폴링 루프.
- 이벤트 기반 입력 메시지이다.

앱이 SCH로 구동되는 경우 기간 변경은 SCH 테이블 또는
앱-로컬 상태만 변경하는 것이 아니라 일정 정책을 적용합니다. 앱이 구동되는 경우
내부 루프, 기간 변경은 검증된 앱 구성을 통해 관리될 수 있습니다.

앱 우선순위는 제한된 작동 매개변수로 처리됩니다. 실행 시간
우선순위 변경은 명시적으로 승인되지 않는 한 기본 정책의 일부가 아닙니다.
나중에 임무 안전 검토를 통해.

## 9. 결함 분류

복구 조치를 선택하기 전에 오류를 분류해야 합니다.

| 결함 등급 | 의미 | 예 |
| --- | --- | --- |
| `APP_FAULT` | 앱 로직 또는 런타임 오류 | 파이프 오류, 잘못된 명령 처리, 메모리 오류 |
| `HW_FAULT` | 하드웨어 장치 오류 | IMU 읽기 시간 초과, GPS 수신기 응답 없음 |
| `DATA_FAULT` | 장치가 응답하지만 데이터를 사용할 수 없습니다 | 오래된 값, 이상값, 잘못된 수정 |
| `LINK_FAULT` | 통신 경로 오류 | 텔레메트리 링크 시간 초과, 패킷 손실 |
| `SYSTEM_FAULT` | 공유 플랫폼 오류 | CPU, 파일 시스템, 스케줄러, 전원 문제 |

센서 앱은 Pi/cFS 호스트 재설정, 비행 컨트롤러를 직접 트리거해서는 안 됩니다.
단일 하드웨어 오류에 대한 재설정 또는 센서 전원 주기. 센서 앱은 다음과 같습니다
먼저 유효하지 않거나 성능이 저하되었거나 손실되었거나 실패한 상태를 게시합니다. Raspberry Pi/cFS 측
복구 결정은 recovery authority가 내려야 한다. 현재 기준 recovery authority는 `cfs_core_app`이다. 별도 health/safety app이 도입되기 전까지 본 문서의 복구 요청 대상은 모두 `cfs_core_app`으로 해석한다. 비행 컨트롤러에 영향을 미치는 회복은
이 사양에서 정의한 기본 권한을 벗어났습니다.

## 10. 하드웨어 오류 응답

하드웨어 관련 오류 처리는 단계적 복구를 따라야 합니다.

| 단계 | 상태 | 앱 동작 |
| --- | --- | --- |
| 0 | 정상 | 유효한 상태 게시 |
| 1 | 데이터 품질이 저하됨 | 품질/결함 세부정보를 포함하여 유효하거나 저하된 게시 |
| 2 | 시간 초과 또는 오래된 데이터 | 유효하지 않거나 손실된 상태 및 마지막으로 좋은 기간 게시 |
| 3 | 소프트 복구 | 로컬 파서를 다시 초기화하거나 Pi 통신 엔드포인트를 다시 열거나 다시 구독하세요. |
| 4 | 하드 복구 요청 | 복구권한을 통해 통신경로 복구 요청 |
| 5 | 성능 저하된 cFS 작업 | 데이터 소스가 줄어들거나 유효하지 않은 경우 cFS 텔레메트리 보고를 계속하세요. |
| 6 | 최소한의 보고 | 복구가 정상 작동을 복원할 수 없는 경우 필수 cFS 텔레메트리/오류 보고만 활성 상태로 유지합니다. |

하드웨어 응답 예시:

- `mavlink_bridge_app`: FC 전송 시간 초과, 파서 오류 버스트 감지 또는
  잘못된 MAVLink 프레임 시퀀스; 성능이 저하되거나 손실된 상태를 `MAVLINK_BRIDGE_APP_HK_TLM_MID`에 게시하고
  재시도 임계값 이후에 통신 경로 복구를 요청합니다.

### 10.1 `mavlink_bridge_app` transport 책임 경계

`mavlink_bridge_app`과 그 하위 transport 계층의 책임은 `uplink_app`과 동일한 수준으로 분리되어야 한다.

| 항목 | transport 계층 책임 | `mavlink_bridge_app` 책임 |
| --- | --- | --- |
| UART open/reopen | 필수 | 아님 |
| raw byte framing 및 parser feed | 필수 | 아님 |
| MAVLink frame CRC/checksum | 필수 | 아님 |
| raw frame length/sequence parse | 필수 | 아님 |
| MAVLink message ID 허용 여부 판단 | 아님 | 필수 |
| mission MID 변환 | 아님 | 필수 |
| FC stream timeout/quality 판단 | 아님 | 필수 |
| Software Bus publish 및 상태 보고 | 아님 | 필수 |

이 경계는 Section 18.4.4의 uplink transport 경계와 대칭이어야 하며, 한쪽만 transport 책임을 상세히 정의하고 다른 쪽을 생략해서는 안 된다.
## 11. 복구 한도 및 에스컬레이션 정책

cFS는 고정된 재시도, 다시 시작 또는 재부팅 타이밍 값을 정의하지 않습니다. 회복
한계는 하드웨어 특성, 결함을 기반으로 임무에 따라 정의되어야 합니다.
중요성, 전력 예산, 운영 위험 및 운영자 복구 정책.

각 복구 조치는 다음을 정의해야 합니다.

- 트리거 조건.
- 재시도 간격.
- 최대 재시도 횟수.
- 에스컬레이션 대상.
- 지속적인 카운터 업데이트 규칙.
- 카운터 재설정 조건.
- 필수 모드 또는 인증 조건.

복구 조치가 준비되어야 합니다. 앱은 즉시 프로세서를 요청해서는 안 됩니다.
복구 가능한 로컬 오류를 위해 재설정하거나 재부팅합니다. 로컬 앱은 먼저
텔레메트리 및 HK을 통해 오류 상태를 파악합니다. 시스템 수준 복구 결정
복구 기관이 수행합니다.

기본 복구 흐름:

1. 일반 오류: 로컬 재시도를 수행합니다.
2. 반복적인 장애: 복구권한을 통해 통신경로 복구를 요청한다.
3. 계속되는 실패: `CFS_DEGRADED`, `CFS_RECOVERY` 또는 최소 보고를 입력하세요.
4. 시스템 중단: Watchdog 재설정을 허용합니다.
5. 반복 재부팅: `CFS_RECOVERY` 또는 최소 보고 시작을 강제합니다.

### 11.1 1차 복구 한계 (코드 기준, 2026-07-20 갱신)

> **구현 현황 주석**: `cfs_core_app`은 `mavlink_bridge_app`·`uplink_app`·`lora_tdm_app` 3개 앱 전부에 대해 HK 생존 감시 + `CFE_ES_RestartApp` 자동 재시작을 구현한다 (bridge와 동일 패턴, 각 5초 간격/최대 3회 — cfs_core spec §13.6/§13.7/§14.4). 아래 표의 ★ 항목이 실제 코드로 구현된 동작이다.

| 목표 | 결함 상태 | 1차 복구 | 재시도 간격 | 최대 재시도 횟수 | 단계적 확대 | 구현 상태 |
| --- | --- | --- | --- | --- | --- | --- |
| `mavlink_bridge_app` | FC 전송 시간 초과, 파서 오류 버스트, 잘못된 MAVLink 프레임 시퀀스 | `cfs_core_app`이 `CFE_ES_RestartApp`으로 재시작 | **5초** (`BRIDGE_RESTART_INTERVAL_MS`) | **3회** (`BRIDGE_MAX_RESTARTS`) | `BRIDGE_TIMEOUT`→`FAILED` 전환; 지상국 RECOVERY 명령으로 재시도 횟수 초기화 가능 | ★ **구현됨** |
| `lora_tdm_app` | HK 5초 미수신 (`FAULT_LORA_TIMEOUT`) | `cfs_core_app`이 `CFE_ES_RestartApp`으로 재시작 | **5초** (`LORA_RESTART_INTERVAL_MS`) | **3회** (`LORA_MAX_RESTARTS`) | DEGRADED 보고 지속; `DIAGNOSTIC_CMD_MID`로 상태 조회 가능 | ★ **구현됨** (2026-07, `LORA_RESTART_EID 16`) |
| `uplink_app` | HK 5초 미수신 (`FAULT_UPLINK_TIMEOUT`) | `cfs_core_app`이 `CFE_ES_RestartApp`으로 재시작 | **5초** (`UPLINK_RESTART_INTERVAL_MS`) | **3회** (`UPLINK_MAX_RESTARTS`) | DEGRADED 보고 지속 | ★ **구현됨** (2026-07, `UPLINK_RESTART_EID 15`) |
| `cfs_core_app` 자체 | 반복 앱 오류 또는 시스템 오류 | cFS watchdog 또는 외부 Pi 프로세스 감시 | 해당 없음 | 해당 없음 | `CFS_RECOVERY` 또는 최소 보고 | ⚪ 외부 감시 |
| 라즈베리 파이/cFS 호스트 | 복구 불가능한 cFS 호스트 오류 | Pi/cFS 프로세스 또는 호스트 재설정 | 해당 없음 | 30초당 1 복구 창 | 오류 반복 시 최소 보고 | ⚪ 외부 감시 |

재시도 간격은 완료 또는 실패 감지 시간부터 계산됩니다.
이전 복구 시도.

> 🔴 **실측 발견 결함 (2026-07-22, RT-CORE-003 실기 시험, 미수정)**:
> 위 표의 uplink/lora 자동 재시작이 **fault 우선순위 체인에 종속**되어
> 있어 상위 fault가 지속되면 발동하지 않는다.
> `PublishSystemHealth()`의 판정이 `BridgeTimedOut → EkfTimedOut →
> LocalTimedOut → AttitudeTimedOut → UplinkTimedOut → LoraTimedOut`
> 순서의 단일 `else if` 체인이고 재시작 로직이 각 분기 **내부**에 있어서,
> 예컨대 GPS 음영/실내(`EkfTimedOut` 상시 참) 상황에서는 uplink_app이
> 죽어도 EKF 분기에서 체인이 끝나 재시작 분기에 도달 불가 — 심지어 EKF
> 분기가 `UplinkRestartCount=0` 리셋까지 수행. 실기 재현: uplink_app
> STOP 후 자동 재시작 미발동, `Msg Limit Err(0x1904, UPLINK_CMD)` 지속.
> **수정 설계 확정 (2026-07-23 합의, 미구현)** — A안(분리):
>
> 1. **구조**: 재시작 로직을 `PublishSystemHealth()`에서 제거하고 별도
>    함수 `CheckAppRestarts()`로 분리, 매 사이클 독립 호출.
>    `PublishSystemHealth()`의 else-if 체인은 **FaultCode/HealthState
>    보고 전용**으로 순수화(기존 우선순위 의미 불변). 타임아웃 플래그
>    계산은 1회 수행 후 두 함수가 공유.
> 2. **사이클당 재시작 1건 제한**: Bridge/Uplink/Lora 타임아웃을 각각
>    독립 검사하되 발행은 사이클당 1건, 고정 우선순위
>    `bridge > uplink > lora`. 스킵된 앱은 다음 사이클에 차례가 오며,
>    발행한 앱만 쿨다운에 들어가므로 기아(starvation) 없음
>    (사이클 1초 ≪ 쿨다운 5초 전제).
> 3. **무한 재시도, 포기 없음**: `*_MAX_RESTARTS` 한도 제거. 근거 —
>    운용 시간이 짧아(≤5분) 소진 후 방치가 실질적 기능 상실이고, 특히
>    uplink_app은 지상 RESTART 명령 경로 자체가 uplink 경유라 자동
>    재시도가 유일한 복구 수단. 고정 쿨다운 5초
>    (`*_RESTART_INTERVAL_MS`)가 빈도 상한(지수 백오프는 짧은 운용
>    시간에 복구만 늦춰 기각, 우선순위 스왑/타이브레이크는 복잡도 대비
>    이득 없어 기각).
> 4. **재시도 횟수는 HK 카운터로 노출만**(관측용, 제한 아님). EKF 등
>    무관 fault 분기가 타 앱 재시작 카운터를 리셋하던 버그는 분리로
>    자연 소멸.
> 5. **병행책 ⓑ (확정, 2026-07-23 정정)**: `mission_defs/cpu1_cfe_es_startup.scr`
>    8번째 필드(Exception Action)의 실제 cFE 정의를 재확인한 결과
>    `0 = 앱만 재시작`, `Non-Zero = 프로세서 전체 리셋`으로, **당초
>    대화 중 "0=프로세서 리셋"이라 설명한 것은 오류였다** — 정반대.
>    4개 앱 전부 이미 `0`으로 설정되어 있어 **크래시 시 앱만 재시작이
>    이미 적용 중** — startup.scr 수정 불필요, 코드 변경 없음.
>    소프트 장애(hang/STOP/HK 중단)는 cfs_core의 HK 타임아웃 감시가
>    담당 — 상호 사각 보완 관계는 그대로 유효. **cfs_core_app 자체도
>    이미 `0`**(감시자 크래시 시에도 ES가 자동으로 cfs_core만 재시작).
>    한계: cfs_core 자신의 hang은 양쪽 다 못 잡음(기존 표의 ⚪ 외부
>    감시 영역, systemd watchdog은 별도 논점).
> 6. **FaultCode 보고 불변 (확정)**: HK `FaultCode`는 기존 우선순위
>    체인의 "최상위 1개" 의미 유지. 동시 다발 fault는 이미 HK의 개별
>    상태 필드(`UplinkStatus.TimedOut` 등)로 지상에서 관측 가능하므로
>    비트마스크 전환(다운링크 프로토콜·지상 디코더 연쇄 수정)은 불필요
>    — 기각.

재시도 횟수는 대상이 유효한 상태로 유지된 후에만 재설정됩니다.
구성된 안정 기간에 대한 명목 상태.

권장되는 1차 통과 안정 기간:

| 목표 | 재시도 카운터 재설정 전 안정 기간 |
| --- | --- |
| 센서 앱 | 30초 |
| 텔레메트리 링크 | 30초 |
| 시스템 수준 복구 | 30초 |

### 11.2 에스컬레이션 규칙

복구 에스컬레이션은 위험도가 가장 낮은 조치를 먼저 따라야 합니다.

기본 에스컬레이션 순서:

1. 이벤트 및 관리 텔레메트리을 통해 오류를 보고합니다.
2. 현재 출력을 유효하지 않음, 성능 저하, 손실 또는 실패로 표시합니다.
3. 로컬 작업을 다시 시도하십시오.
4. 로컬 드라이버, 파이프, 파일 핸들 또는 전송을 다시 초기화합니다.
5. 복구 권한을 통해 Pi 측 통신 경로 복구를 요청합니다.
6. `CFS_DEGRADED`에서 계속하세요.
7. `CFS_RECOVERY`을(를) 입력하세요.
8. 최소 보고 작업을 시작합니다.
9. 시스템 수준 오류 또는 복구할 수 없는 반복 오류에 대해서만 Raspberry Pi/cFS 호스트 재설정을 요청하세요.

앱은 로컬 오류 감지에서 Pi/cFS 호스트 재설정으로 직접 건너뛰어서는 안 됩니다.
임무 안전 정책에 의해 명시적으로 허용되지 않는 한. 앱은 항공편을 요청해서는 안 됩니다.
기본 정책에 따라 컨트롤러를 재부팅하거나 비행 제어를 재설정합니다.

#### 11.2.1 에스컬레이션 결정 규칙

| 상태 | 필수 에스컬레이션 |
| --- | --- |
| 단일 유효하지 않은 샘플 | 샘플을 거부합니다. 앱을 계속 실행하세요 |
| 재시도 임계값 미만의 연속 잘못된 샘플 | 저하된 상태 게시 |
| 시간 초과가 재시도 임계값을 초과했습니다. | 로컬 소프트 복구 수행 |
| 로컬 소프트 복구 제한을 초과했습니다. | 복구권한을 통해 통신경로 복구 요청 |
| 하드 복구 제한을 초과했습니다. | 대상을 실패로 표시하고 `CFS_DEGRADED`을(를) 입력하세요. |
| 필수 cFS 기능을 사용할 수 없습니다. | `CFS_RECOVERY` 또는 최소 보고 작업 요청 |
| 복구 기간 내에 반복되는 앱 충돌 | 앱 재시작을 중지하고 복구 권한 조치를 요청하세요. |
| 재부팅 루프 창 내에서 Pi/cFS 호스트 재설정이 반복됨 | 최소 보고 작업부터 시작하여 안전하지 않은 행위 차단 |

### 11.3 watchdog 재설정 정책

watchdog은 시스템이 안정적으로 작동할 수 없는 오류에만 사용해야 합니다.
실행을 계속하거나 정상적인 소프트웨어 복구에 의존할 수 없습니다.

watchdog은 일반 복구의 기본 복구 방법으로 사용되어서는 안 됩니다.
센서 오류, 잘못된 데이터, GPS 수정 누락, 일시적인 링크 손실 또는 단일 앱
오류.

Watchdog 재설정은 다음에 대해 허용될 수 있습니다.

- 주요 실행 루프가 중단됩니다.
- 스케줄러 또는 타임베이스 오류.
- 복구 권한이 정지되었습니다.
- 구성된 하트비트 시간 제한을 초과하여 중요한 앱이 응답하지 않습니다.
- 메모리 손상 표시.
- 시스템 수준 제어에 영향을 미치는 교착 상태 또는 작업 부족.
- 필수 상태 점검 서비스가 반복적으로 실패했습니다.

#### 11.3.1 watchdog heartbeat 규칙

각 중요 앱은 정의된 간격으로 하트비트를 게시하거나 업데이트해야 합니다.
복구 기관은 심장 박동 기간과 건강 상태를 모니터링해야 합니다.

장기 실행 앱은 다음과 같은 경우 작업 완료와 하트비트 활성을 분리해야 합니다.
내부 처리는 여러 주기 또는 I/O 경계에 걸쳐 있습니다.

| 요소 | 하트비트 시간 초과 | 첫 번째 조치 | 단계적 확대 |
| --- | --- | --- | --- |
| 중요한 앱 | 3번의 놓친 심장 박동 | 다시 시작이 허용되면 앱을 다시 시작하세요. | 다시 시작하지 못한 경우 `CFS_RECOVERY`을 입력하세요. |
| 복구 권한 | 2회 heartbeat 누락 | watchdog 재설정 | 반복되는 경우 최소 보고 시작 |
| 스케줄러/시간 서비스 | 2회 heartbeat 누락 | watchdog 재설정 | 최소 보고 시작 |
| 중요하지 않은 앱 | 5번의 놓친 심장 박동 | 실패로 표시하거나 앱을 다시 시작하세요. | 허용되는 경우 계속 저하됨 |

누락된 하트비트 수는 다음을 사용하여 시간 초과 기간으로 변환됩니다.
각 구성요소의 하트비트 기간을 구성했습니다. 예를 들어 다음과 같은 중요한 앱이 있습니다.
5초 heartbeat 주기와 3회 heartbeat 누락 임계값이 효과적입니다.
30초의 시간 초과

watchdog 서비스 간격은 watchdog timeout보다 짧아야 합니다.
watchdog timeout은 예상 피크 동안 재설정을 방지할 수 있을 만큼 길어야 합니다.
처리 또는 파일 I/O 경계.

권장되는 1차 통과 값:

| 목 | 값 |
| --- | --- |
| 복구 기관 상태 주기 | 1초 |
| 중요한 앱 하트비트 기간 | 1초 ~ 5초 |
| watchdog 서비스 주기 | 1초 |
| watchdog timeout | 30초 |
| 재부팅 루프 감지 창 | 30초 |

이 값은 첫 번째 통과 기본값이며 타이밍 테스트 후에 수정되어야 합니다.

### 11.4 재부팅 루프 방지 정책

시스템은 반복적인 자동 재부팅을 방지하여 플랫폼을 유지하거나
시스템이 안전하지 않거나 사용할 수 없는 상태입니다.

Raspberry Pi/cFS 호스트 수가 재설정되면 재부팅 루프가 감지됩니다.
구성된 기간 내에 허용된 한도를 초과합니다. 비행 컨트롤러
재부팅은 이 기본 정책의 일부가 아닙니다.

권장되는 첫 번째 통과 규칙:

| 상태 | 행동 |
| --- | --- |
| 30초 내에 2 Pi/cFS 호스트 재설정 | `CFS_RECOVERY`에서 시작 |
| 30초 내에 3개의 Pi/cFS 호스트 재설정 | 최소 보고 작업 시작 |
| 재부팅 루프로 인한 최소 보고 시작 | 최소 구성을 로드하고 불필요한 앱 시작을 차단합니다. |
| 지속적인 상태 검증 실패 | 손상된 기록을 무시하고 기본 안전 구성을 로드합니다. |
| 동일한 앱으로 인해 600초 내에 3번 다시 시작됨 | 해당 앱 다시 시작을 중지하고 `FAILED`로 표시하세요. |
| 동일한 하드웨어 대상이 30초 내에 3번의 하드 복구에 실패함 | 대상을 비활성화하고 허용되는 경우 계속 성능 저하됨 |

#### 11.4.1 재부팅 루프 방지에 필요한 영구 카운터

다음 카운터는 지속적으로 저장됩니다.

- 부팅 횟수.
- Pi/cFS 호스트 재설정 횟수.
- watchdog 재설정 횟수.
- 마지막 재설정 이유.
- 마지막 재설정 타임스탬프입니다.
- 재부팅 루프 창 시작 타임스탬프입니다.
- 앱별 다시 시작 횟수입니다.
- 앱별 마지막 오류 코드입니다.
- 하드웨어 대상별 복구 시도 횟수입니다.
- 최소 보고 입력 이유.
- 마지막으로 성공한 `CFS_NOMINAL` 타임스탬프.

각 영구 카운터 기록에는 버전, 크기, CRC/체크섬,
타임스탬프 및 생성 카운터.

#### 11.4.2 카운터 재설정 규칙

영구 복구 카운터는 재부팅 후 즉시 지워지지 않습니다.
카운터는 시스템이 구성된 기간 동안 안정된 후에만 지워질 수 있습니다.
안정된 기간.

권장되는 첫 번째 재설정 규칙:

| 계수기 | 조건 재설정 |
| --- | --- |
| 앱 재시작 횟수 | 앱은 30초 동안 `CFS_NOMINAL` 상태로 유지됩니다. |
| 하드웨어 복구 횟수 | 대상은 30초 동안 유효하게 유지됩니다. |
| Pi/cFS 호스트 재설정 창 | 시스템이 30초 동안 재설정 루프를 벗어났습니다. |
| watchdog 재설정 횟수 | 30초 동안 watchdog 재설정 없음 |
| 최소 보고 입력 사유 | 운영자 명령 또는 검증된 임무 정책 |

수동 카운터 삭제에는 명시적인 명령 승인이 필요합니다.

window 기반 제한과 카운터 재설정의 상호작용 규칙:

- 안정 기간 충족만으로 모든 window 기반 카운터가 즉시 삭제되는 것은 아니다.
- 각 카운터는 자신의 관측 window가 만료되고, 동시에 해당 안정 조건이 충족되었을 때만 재설정될 수 있다.
- 예를 들어 `600초 내 3회 앱 재시작` 제한은 마지막 관련 재시작 시각으로부터 600초 window가 닫히기 전까지 유지된다.
- 30초 안정 조건은 다음 window를 새로 시작할 수 있는 최소 조건일 뿐, 아직 닫히지 않은 기존 window를 소급 삭제하지 않는다.
- 카운터가 재설정되면 다음 관련 fault event가 새 window의 시작점이 된다.

## 12. 지속 상태 및 재부팅 복구

지속 상태는 임무 상태, 오류 및 복구 카운터로 제한됩니다.
마지막으로 알려진 유효한 내비게이션 또는 지도 참조 및 운영자가 수정한 것
구성. 고속 원시 센서 샘플은 직접 기록되어서는 안 됩니다.
제한된 로그나 명시적 진단 캡처를 통한 경우를 제외한 영구 저장소입니다.

후보 영구 값:

| 범주 | 영구 값 |
| --- | --- |
| 부팅/오류 | 마지막 재설정 이유, 부팅 횟수, Pi/cFS 호스트 재설정 횟수, watchdog 재설정 횟수, watchdog 표시, 재부팅 루프 창 시작 타임스탬프 |
| 앱 상태 | 앱 상태, 재시작 횟수, 마지막 오류 코드 |
| 하드웨어 상태 | 센서 상태, 마지막 하드웨어 오류 코드, 복구 시도 횟수 |
| 임무 상태 | 임무 단계, 활성 cFS 상태, 성능 저하/복구/최소 보고 항목 |
| 항해 | 마지막 유효한 GPS 수정, 마지막 유효한 EKF 상태, 마지막 유효한 타임스탬프 |
| 텔레메트리 | 마지막 링크 상태, 마지막으로 양호한 접촉 시간, 활성 전송 ID |
| 구성 | 운영자가 수정한 구성 버전 및 검증된 테이블 버전 |
| 회복 | 보류 중인 복구 작업, 마지막 복구 결과 |

재부팅 처리:

| 이벤트 | 복구 동작 |
| --- | --- |
| 앱 다시 시작 | 유효하고 호환되는 경우에만 앱-로컬 상태를 복원합니다. |
| Pi/cFS 호스트 재설정 또는 소프트 부팅 | 영구 구성, cFS 상태, 마지막 상태 및 체크포인트 복원 |
| Pi/cFS 호스트 하드 부팅 또는 전원 주기 | 검증된 지속 상태 및 기본 안전 구성만 복원 |
| 최소 보고 시작 | 최소 구성을 로드하고 오류 로그를 노출합니다. 안전하지 않은 복구 작업 차단 |

모든 영구 기록에는 검증을 위한 충분한 무결성 메타데이터가 포함되어야 합니다.
버전, 크기, CRC/체크섬, 타임스탬프, 생성 카운터 등이 있습니다.

> **구현 편차 명시(2026-07-23)**: 실제 구현된 레코드(§12.1 uplink_app,
> cfs_core_app, 그리고 BL-41로 구현된 mavlink_bridge/lora_tdm 신규
> 레코드 — §12.2)는 위 목록 중 **Magic(식별자)·버전(`ConfigVersion`)·체크섬만**
> 포함하고 크기·타임스탬프·생성 카운터는 생략한다 — 레코드가 고정
> 크기 단일 구조체(read 바이트 수 검증으로 크기 확인 대체)이고,
> 타임스탬프/생성 카운터는 현 소비처가 없어서다. uplink_app 구현
> (BL-17, 2026-07-22) 때부터 수용된 편차이며 새 레코드도 동일 패턴을
> 따른다. 소비처가 생기면 그때 필드 추가.

현재 기준 저장소 백엔드:

- persistent state는 Raspberry Pi 파일 시스템에 record 단위로 저장한다.
- 각 record write는 replace 또는 rename 기반 atomic write를 사용해야 한다.
- 저장 빈도는 event-driven update를 기본으로 하며, 고속 sensor sample은 저장 대상이 아니다.
- 다른 백엔드로 교체하더라도 본 문서의 record integrity field와 reboot recovery contract를 유지해야 한다.

### 12.1 `uplink_app` 영속 상태 구현 (2026-07-22, BL-17/BL-18 — spec에 구체 수치 없어 신규 기록)

`UPLINK_APP_LoadState()`/`SaveState()`(`uplink_app_utils.c`)가 위 일반 원칙을
`cf/uplink_app_state.bin`(경로는 `UPLINK_APP_STATE_FILE_PATH`, 테스트 환경에서
`UPLINK_APP_STATE_FILE_PATH` env var로 주입 가능 — BL-17 커버리지 갭 해소 시
lora_tdm_app/mavlink_bridge_app의 시리얼 경로 주입과 동일 패턴 적용)에
구체적으로 구현한다. 이 절의 수치/EID는 spec 원문에 없던 것을 구현 시
확정해 기록한 것이다.

> 🔴→✅ **BL-39 (2026-07-22 발견, 2026-07-23 수정)**: 원래 경로가
> 절대경로 `/cf/uplink_app_state.bin`이었으나, raw POSIX `open()`은
> cFE OSAL의 가상 경로 매핑을 거치지 않아 Pi 실파일시스템에 없는
> `/cf`를 그대로 찾다 `ENOENT`로 실패 — **영속화가 실기에서 한 번도
> 동작한 적 없었음**(BL-12/BL-03 실질 무효, Pi 재연결 직접 확인:
> `/cf` 부재, `cfs.service`는 `User=root`라 권한 문제 아님).
> `cfs_core_app_state.bin`(`CFS_CORE_APP_STATE_FILE_PATH`,
> `cfs_core_app_utils.c`의 `SaveState()`/`LoadState()`, health 전이
> 시 저장 — 이 spec엔 별도 절 없이 §11.1 재시작 로직과 같은 파일에
> 구현됨)도 동일 절대경로 패턴이라 동일 결함, 동일 수정 적용.
> **수정**: 상대경로 `cf/...`로 변경 — `cfs.service`의
> `WorkingDirectory=~/cFS_clean/build/exe/cpu1` 기준 `cf/`가
> `EEPROM.DAT` 등이 이미 쓰는 실경로와 일치. SaveState의 open/write/
> rename 3개 실패 지점 전부에 `UPLINK_APP_STATE_SAVE_FAIL_EID`(10)
> ERROR 이벤트(errno 포함) 추가 — 종전엔 실패해도 침묵이라 발견이
> 늦어졌음. Pi 재검증(부팅 카운트 실측 증가 확인)은 최종 검증 때 일괄.

**레코드 구조** (16바이트, 전부 `uint32`): `Magic`(`0x55504C4BU`="UPLK") /
`LastAcceptedSequence` / `BootCount` / `Checksum`(`Magic+LastAcceptedSequence+BootCount`
단순합, 암호학적 강도 아님 — 비트플립 탐지 목적).
BL-43(2026-07-23)으로 `LastResetReason`/`SurvivedMark`/`ShortBootStreak`
필드 확장 예정 — §12.3 참조.

**쓰기(atomic write)**: `.tmp` 파일에 쓰고 `fsync()` 후 `rename()`, 이어서 부모
디렉터리(`/cf`) fd도 `fsync()`(BL-18, 2026-07-21) — POSIX상 rename 자체의
디렉터리 엔트리 갱신도 정전 시 유실 가능해 디렉터리 fsync 없이는 rename의
atomic 보장이 불완전하다.

**읽기 시 손상 판정** (BL-17, 2026-07-22, 신규 `UPLINK_APP_STATE_CORRUPT_EID`=9):

| 조건 | 판정 | 이벤트 |
| --- | --- | --- |
| `open()` 실패, `errno==ENOENT` | 첫 부팅(정상) | 없음(조용히 기본값) |
| `open()` 실패, 그 외 errno | 이례적 — 손상과 동일 취급 | `STATE_CORRUPT_EID` ERROR |
| `read()` 바이트 수 불일치(truncated) | 손상 | `STATE_CORRUPT_EID` ERROR |
| `Magic` 불일치 | 손상 | `STATE_CORRUPT_EID` ERROR |
| `Checksum` 불일치 | 손상 | `STATE_CORRUPT_EID` ERROR |
| 전부 통과 | 복원 성공 | `STARTUP_EID` INFO |

손상 판정 시 전부 기본값(`LastAcceptedSequence=0` 등)으로 폴백하며 크래시하지
않는다 — "첫 부팅 오탐 없이 진짜 손상만 구분해서 보고"가 이 설계의 목적.

### 12.2 CONFIG 영속화 (2026-07-23, BL-41)

§12 표의 "구성(운영자가 수정한 구성 버전)" 범주를 구현 — 운영 중 CONFIG
명령으로 조정한 파라미터가 재부팅 후에도 유지된다. 3개 앱 모두 §12.1의
uplink_app 패턴(매직+체크섬+원자적 tmp-write→fsync→rename→부모 디렉터리
fsync(BL-18), ENOENT만 침묵, 그 외 손상은 `STATE_CORRUPT_EID` ERROR 폴백,
env var로 테스트 경로 주입)을 그대로 따르고, 추가로 레코드에
`ConfigVersion`을 포함해 **불일치 시 range 재검증 없이 전체 기본값 폴백**
한다(구버전 레코드 필드 오해석 방지; 저장값은 ActiveConfig 승격 전 이미
검증됐으므로 range 재검증 불필요).

| 앱 | 파일 | Magic | 영속 필드 | 저장 시점 |
| --- | --- | --- | --- | --- |
| `cfs_core_app` | `cf/cfs_core_app_state.bin` | `0xCF5C0A00` | `LastHealthState` + `ActiveConfig` 6필드 | health 전이 시 + CONFIG 적용 성공 시 |
| `mavlink_bridge_app` | `cf/mavlink_bridge_app_state.bin` | `0x3AB51DE0` | `ActiveConfig` 7필드 | CONFIG 적용 성공 시 |
| `lora_tdm_app` | `cf/lora_tdm_app_state.bin` | `0x10A7D3B0` | `UseV2Downlink` 1필드 | CONFIG_CMD_MID 적용 성공 시 **및** 전용 지상 명령 `SET_DL_PROTO_CC` 성공 시(두 변이 지점 모두 배선) |

복원은 각 앱 `Init()`에서 컴파일타임 기본값 설정 직후 `LoadState()` 호출로
수행(파일 없으면 무동작 = 기본값 유지). 상세: cfs_core는
`cfs_core_app_behavior_spec.md` §14.5, 설계 이력은
`notes/bl41_config_persistence_design_2026-07-23_completed.md`.

### 12.3 부팅/오류·앱 상태 영속화 (2026-07-23 설계 확정, BL-43)

§12 표의 "부팅/오류"·"앱 상태" 범주 구현. 나머지 4범주(하드웨어/항해/
텔레메트리/회복)는 **의도적 제외** — 사유는
`persistent_state_gap_audit_2026-07-23.md` 결정 표 참조(원천 부재/FC
진실원본/정보 중복/원자적 복구). 두 항목 모두 **보고 전용** — 복원값이
기체 동작을 바꾸지 않으며, 대응 판단은 지상국 몫.

**① uplink_app — 부팅/오류** (`uplink_app_state.bin` 확장):

| 신규 필드 | 의미 |
| --- | --- |
| `LastResetReason` (u8) | 직전 부팅의 PSP reset type (POWER_ON/PROCESSOR 등) |
| `SurvivedMark` (u8) | 이번 세션이 안정 가동 임계(120s)를 넘겼는지 |
| `ShortBootStreak` (u8) | 연속 단명 부팅 횟수 (120s 미만 생존 연속) |
| (Reserved u8) | 정렬 |

**재부팅 루프 감지 — "생존 마커" 방식**: `CFE_TIME`이 부팅마다 0에서
재시작해 부팅 간 벽시계 비교가 불가하므로, 시계 없이 동작하는 마커
방식을 쓴다. ⓐ Init에서 직전 세션의 `SurvivedMark`를 검사 — 0이면(직전
세션이 120s를 못 버팀) `ShortBootStreak++`, 1이면 0으로 리셋. 이어서
`SurvivedMark=0`으로 저장. ⓑ 가동 120s(`UPLINK_APP_BOOT_SURVIVE_MS`)
도달 시 `SurvivedMark=1` 1회 저장. ⓒ `ShortBootStreak >= 5`
(`UPLINK_APP_BOOT_LOOP_THRESHOLD`)이면 HK에 `BootLoopSuspect=1` 노출 —
기체 동작 변경 없음(안전모드 등은 미구현, 지상국 판단). 세션당 파일
쓰기 3회(Init에서 BootCount++ 저장 + 마커0 저장, 생존 확정 시 1회)로
flash 마모 무시 가능.

HK 노출(신규): `BootLoopSuspect`, `ShortBootStreak`, `LastResetReason`
(BootCount는 기존 HK/DL2 노출 유지).

**② cfs_core_app — 앱 상태** (`cfs_core_app_state.bin` 확장):

| 신규 필드 | 의미 |
| --- | --- |
| `BridgeRestartCount` (u32) | bridge 자동/지상 재시작 누계 |
| `UplinkRestartCount` (u32) | uplink 재시작 누계 |
| `LoraRestartCount` (u32) | lora 재시작 누계 |
| `LastFaultCode` (u8) + Reserved[3] | 마지막 fault 코드 |

저장 시점: ⓐ `CheckAppRestarts()`의 `CFE_ES_RestartApp()` 발행 직후
(3분기 각각), ⓑ 지상 RECOVERY `RESET_COUNTER` 처리 시(리셋값 동기화),
ⓒ health 전이 시(기존 SaveState 호출에 `LastFaultCode` 동승). 복원:
Init `LoadState()`에서 자동 — 재부팅 후에도 누계 지속, 운영자 개입 불필요.

HK 노출(신규): 현재 RAM 카운터가 **어느 tlm에도 없음**(BL-38 당시 "HK
노출" 기술은 미구현이었음 — 이번에 실구현) → `CFS_CORE_APP_HkTlm`에
`BridgeRestartCount`/`UplinkRestartCount`/`LoraRestartCount`/`LastFaultCode`
추가.

**레코드 마이그레이션**: 두 파일 모두 구조 확장으로 기존 파일은 read
크기 불일치(truncated) → `STATE_CORRUPT_EID` 후 기본값 폴백 — 배포 후
첫 부팅 1회에 한해 기존 카운터(BootCount 등)가 리셋됨(수용, spec §12.2
ConfigVersion 정책과 동일 계열).

## 13. 활성/보류 구성 모델

중요한 앱은 별도의 활성 및 보류 구성 버퍼를 유지해야 합니다.
런타임 구성 업데이트는 보류 중인 버퍼에 먼저 기록되고
검증 및 활성화가 완료될 때까지 활성 작동에 영향을 미치지 않습니다.

유효성 검사에 실패하면 앱은 이전 활성 상태를 계속 사용합니다.
이벤트 및 관리 텔레메트리을 통해 거부를 구성하고 보고합니다.

권장 구성 상태:

- `active_config`: 현재 검증된 구성입니다.
- `pending_config`: 검증 중인 새로 로드되거나 명령된 구성입니다.
- `previous_config`: 중요한 앱의 선택적 롤백 대상입니다.
- `config_generation`: 활성화가 성공할 때마다 증가합니다.
- `active_config_version`.
- `pending_config_version`.
- `last_config_result`.
- `last_config_error`.

### 13.1 pending/active 버퍼 소유 앱

다음 표는 pending config 버퍼를 자체적으로 소유하는 앱을 고정한다.
버퍼를 소유한다는 것은 해당 앱이 `pending_config`, `active_config`, `last_config_result` 상태를 자체 데이터 구조에 선언하고 유지할 책임이 있음을 의미한다.

| 앱 | 버퍼 소유 | 근거 |
| --- | --- | --- |
| `cfs_core_app` | **예** | 헬스 분류 임계값, 안정화 타이머, publish 주기 등 런타임 변경 가능 파라미터를 보유한다 |
| `mavlink_bridge_app` | **예** | FC 스트림 요청 파라미터, 재시도 한도, 타임아웃 임계값 등 런타임 변경 가능 파라미터를 보유한다 |
| `lora_tdm_app` | **예(축소 구현)** | 실구현은 `UseV2Downlink`(다운링크 프로토콜, 0/1) 1개뿐이며 pending/active 이중 버퍼 없이 검증 통과 시 직접 대입한다(`ProcessConfigCommand`). "TDM 슬롯 주기, 전송 타임아웃, 활성 MID 목록"은 애초 후보였으나 미구현 — 파라미터가 늘어나면 그때 이중 버퍼 도입(2026-07-23 실태 반영) |
| `uplink_app` | **제한적** | `uplink_app` 자체 파라미터(최대 payload 길이, 프로토콜 버전 허용 범위 등)가 필요한 경우에만 로컬 버퍼를 유지한다. 다른 앱 대상 config는 해당 앱 MID로 전달만 수행하며 버퍼를 소유하지 않는다 |

`uplink_app`은 config 명령을 수신하면 다음을 수행한다:

1. **Checksum 검증** (§13.4 참조): `ConfigPayloadHdr`의 checksum을 검증 → 실패 시 거부 후 event 발생
2. **Scope 판별**: `config_scope` 필드로 대상 앱 결정
3. **Forward**: 검증된 payload를 대상 앱의 `CONFIG_CMD_MID`로 전달

각 target 앱(`cfs_core_app`, `mavlink_bridge_app`)이 자신의 pending 버퍼에서 최종 검증 및 적용을 수행한다.

### 13.2 앱별 필수 상태 변수

pending/active 버퍼를 소유하는 각 앱은 최소한 다음 상태 변수를 선언해야 한다.

| 변수 | 형식 | 의미 |
| --- | --- | --- |
| `ConfigPendingState` | `uint8` | `IDLE=0`, `PENDING=1`, `REJECTED=2` |
| `LastConfigResult` | `uint8` | `0=성공`, `1=실패` (최근 활성화 결과) |
| `LastRollbackReason` | `uint8` | 롤백 발생 시 원인 코드, 없으면 `0` |
| `ConfigGeneration` | `uint32` | 활성화 성공마다 단조 증가 |

`previous_config`(롤백 버퍼)와 `active_config_version` / `pending_config_version`은 각 앱의 파라미터 스키마 확정 이후 앱별로 추가한다.

### 13.4 CONFIG_CMD_MID Checksum 검증

모든 CONFIG 명령 수신 앱(`uplink_app`, `cfs_core_app`, `mavlink_bridge_app`)은 `ConfigPayloadHdr.Checksum` 검증을 수행해야 한다.

**Checksum 정의** (§17.2 Payload 정밀도):

```
ConfigChecksum = additive sum (uint16 wrapping):
  ConfigScope (1B)
  + ConfigVersion (1B)  
  + ParameterId[0] (1B, LE)
  + ParameterId[1] (1B, LE)
  + ValueType (1B)
  + ValueLength (1B)
  + ValueBytes[0..N] (N B)
```

**검증 정책**:
- ConfigScope ≠ 자신의 scope → 조용히 무시 (다른 앱 대상)
- ConfigScope = 자신의 scope
  - Checksum 불일치 → error event 발생 + 거부
  - Checksum 일치 → 다음 검증 단계로 진행 (ConfigVersion, PayloadLength 등)

이 두 단계 checksum 검증(lora_tdm_app의 프레임 CRC + 각 app의 payload checksum)으로 SB 메시지 계층까지의 무결성을 보증한다.

> **지상 노출 정책 (2026-07-22 사용자 결정)**: `cfs_core_app`(scope=1)의
> CONFIG 파라미터 6종(`attitude/local/gps/ekf/bridge_timeout_ms`,
> `publish_period_ms`)은 전부 헬스판정 튜닝 노브로, 실운용에서 지상이
> 바꿀 일이 없고 비행 중 timeout 상향은 FC 데이터 끊김을 가리는 위험이
> 있다. 따라서 **지상 GUI(uplinkGUI) CONFIG scope 드롭다운에서 cfs_core는
> 제외**한다(openMCT ⑦). 기체측 scope=1 처리·서버 meta·CLI 경로는 벤치
> 튜닝용으로 유지 — 기능 삭제가 아니라 GUI 노출 제한이다.

### 13.5 활성화 경계

활성화 경계는 앱마다 다릅니다.

| 앱 | 활성화 경계 |
| --- | --- |
| `mavlink_bridge_app` | 다음 파서 또는 FC 스트림 처리 경계 |
| `lora_tdm_app` | 다음 TDM 슬롯 경계 |
| `cfs_core_app` | 운영자 승인 활성화 또는 안전한 시스템 경계 |

## 14. 이중 버퍼 런타임 데이터 모델

앱 내부 또는 앱 작업 ​​간에 공유되는 생산자/소비자 데이터는
부분 업데이트가 필요한 경우 이중 버퍼 또는 동등한 원자 핸드오프 모델
위험한.

일반적인 모델:

- 작성자는 `write_buffer`을(를) 채웁니다.
- 작성자는 전체 샘플 또는 결과를 검증합니다.
- 작성자는 짧은 활성화 지점에서 읽기 가능한 활성 버퍼를 교환합니다.
- 리더는 `read_buffer`을 사용하며 부분적으로 작성된 값을 관찰하지 않습니다.

이 모델은 다음과 같은 경우에 권장됩니다.

- 최신 IMU 샘플.
- 최신 GPS 수정.
- 최신 EKF 상태.
- 중요한 상태/복구 정책 스냅샷.

## 15. cFS 상태 및 명령 안전

cFS 상태는 Raspberry Pi/cFS 통신만 설명하고
상태 관리 계층. FC 비행 모드 또는 FC로 취급되지 않습니다.
안전 상태.

기본 cFS 상태는 다음과 같습니다.

- `CFS_NOMINAL`
- `CFS_DEGRADED`
- `CFS_RECOVERY`

`SHUTDOWN`은 정상적인 작동 상태가 아닌 주문된 절차로 처리됩니다.
FC 비행 모드 및 안전 장치 상태는 비행 컨트롤러 펌웨어에 속합니다.
cFS에서는 텔레메트리으로만 보고될 수 있습니다.

각 명령은 허용되는 모드를 정의해야 합니다. 위험한 명령
Pi/cFS 호스트 재설정, 테이블 활성화, 우선순위 변경, 복구 종료 등
이후 명령 안전 사양에서는 명시적인 인증 정책이 필요합니다.
비행 컨트롤러 재부팅, 비행 제어 재설정, 센서 전원 사이클을 통한
FC, 모터 또는 액추에이터 명령 및 비행 제어 매개변수
기본 정책에 따라 변경이 금지됩니다.

### 15.1 주별 앱 운영 정책

각 앱은 활성화, 정지, 성능 저하 또는 차단 여부를 정의해야 합니다.
각 cFS 상태. 아래 표는 기본 cFS 계층 정책을 정의합니다.
각 앱을 구현할 때 앱별 편차를 문서화해야 합니다.

| cFS 상태 | 기본 정책 |
| --- | --- |
| `CFS_NOMINAL` | FC 데이터 수신, 검증, 소프트웨어 버스 게시 및 지상 텔레메트리이 정상적으로 작동 중입니다. |
| `CFS_DEGRADED` | 일부 데이터, 앱 또는 통신 경로가 오래되었거나 유효하지 않거나 불안정합니다. cFS는 저하된 상태로 사용 가능한 텔레메트리을 계속 보고합니다. |
| `CFS_RECOVERY` | cFS가 앱, 파서, 전송 또는 통신 경로를 다시 초기화하는 중입니다. 복구 중에는 필수적이지 않은 cFS 처리가 제한될 수 있습니다. |

최종 앱별 상태 테이블은 명령 승인이 이루어지기 전에 정의되어야 합니다.
최종 확정되었으며 섹션 17에서 추적됩니다.

`CFS_DEGRADED`는 최상위 상태로 유지하되, 테스트와 운용 판단을 위해 최소한 다음 세부 원인을 `FaultCode` 또는 동등한 세부 필드로 구분해야 한다.

- `DEGRADED_BRIDGE`
- `DEGRADED_EKF`
- `DEGRADED_LINK`
- `DEGRADED_APP`

즉 상태 기계는 단일 `CFS_DEGRADED`를 유지할 수 있지만, 테스트 기대 결과와 운용자 판단은 세부 결함 원인까지 관찰 가능해야 한다.

> **GPS 정책 (2026-06-15 개정)**: GPS 가용성(fix/valid/stale)은 **cFS health를 저하시키지 않는다.**
> cFS health는 Pi/cFS 통신·파이프라인 상태(§15)이며 GPS fix는 센서/비행 조건이므로 health 게이트의 입력이 아니다.
> GPS no-fix는 실내/프리플라이트에서 정상 발생하며, 이를 DEGRADED로 처리하면 GPS와 무관한 비위험 uplink 명령(CONFIG/DIAGNOSTIC)까지 차단되어 운용이 막힌다.
> 따라서 `DEGRADED_GPS`는 폐지하고, GPS 상태는 `GpsValid` per-input 필드로 **보고만** 한다.
> (구현 상세: `cfs_core_app_behavior_spec.md` §12.7)

## 16. 테스트 요구 사항

각 앱은 다음을 지원하거나 테스트할 수 있어야 합니다.

- 정상 메시지 입력.
- 잘못된 페이로드 길이 처리.
- 오래되고, 중복되고, 순서가 잘못된 시퀀스 처리.
- 시간 초과 및 저하된 입력 처리.
- 가능한 경우 하드웨어 결함 주입.
- 런타임 구성 로드, 거부, 활성화 및 롤백.
- 앱 다시 시작 및 소프트 부팅 복구.
- 지속 상태 CRC/체크섬 오류.

현재 기준 E2E sender는 모니터 및 링크 상태 동작의 일부만 검증한다. 추가로 MAVLink Bridge, IMU, GPS, EKF, downlink, 시스템 상태, uplink 메시지 흐름에 대한 테스트가 필요하다.

하드웨어 연결 전 시험 단계에서는 UART 또는 LoRa 실장 대신 `localhost` 기반 UDP 입력/출력 또는 동등한 mock sink를 임시 시험 구성으로 사용할 수 있다. 이 구성은 실제 FC mission 반영이나 LoRa 물리 송신을 대체하는 것이 아니라, 입력 수신, 패킷 검증, Software Bus 게시, 내부 상태 반영, HK/Event 확인과 같은 내부 계약을 검증하기 위한 임시 수단으로만 사용해야 한다. UDP 기반 시험 입력 경로, 포트 번호, sink 방식은 시험 환경에 맞게 변경 가능해야 하며, 최종 하드웨어 통합 구성으로 고정된 것으로 간주해서는 안 된다.

### 16.1 기능별 테스트

| 기능 | 입력 | 기대 결과 | 검증 방법 |
| --- | --- | --- | --- |
| `mavlink_bridge_app` MAVLink 수신 | UART 기반 `ATTITUDE`, `GPS_RAW_INT`, `EKF_STATUS_REPORT` 입력 | 해당 상태 MID(`FC_ATTITUDE_STATE_MID`, `FC_GPS_RAW_STATE_MID`, `FC_EKF_STATUS_MID`)가 정상 publish되고 `MAVLINK_BRIDGE_APP_HK_TLM_MID`가 갱신된다. | 수신 로그, Software Bus subscribe 결과, HK 카운터 확인 |
| `mavlink_bridge_app` 오류 처리 | 잘못된 길이, CRC 오류, 순서 오류, timeout | invalid frame이 폐기되고 링크 상태가 성능 저하 또는 손실로 반영된다. | `MAVLINK_BRIDGE_APP_HK_TLM_MID`의 오류 카운터/상태, discard count, 상태 MID 확인 |
| `cfs_core_app` 상태 종합 | 정상 IMU/GPS/EKF/bridge 입력 | `SYSTEM_HEALTH_MID`가 정상 상태로 게시된다. | health 상태 필드, `FaultCode=FAULT_NONE`, sequence 증가 확인 |
| `cfs_core_app` 복구 판단 | bridge timeout, EKF invalid, local/attitude timeout | 성능 저하 또는 복구 필요 상태가 게시되고 필요한 복구 정책이 요청된다. | health 상태와 함께 `FaultCode=DEGRADED_BRIDGE`, `DEGRADED_EKF`, `LOCAL_TIMEOUT`, `ATTITUDE_TIMEOUT` 중 해당 값, recovery request 로그 확인 (GPS stale은 헬스 비반영 — §15 GPS 정책) |
| `lora_tdm_app` 패킷 구성 | 승인된 상태 MID 입력 | TDM 슬롯별 downlink packet이 생성되고 송신 카운터와 마지막 송신 시각이 갱신된다. | 송신 로그, 자체 HK(`LORA_TDM_APP_HK_TLM_MID`, 송신 카운터/마지막 송신 시각) 확인 |
| `lora_tdm_app` 송신 오류 처리 | LoRa 또는 지상국 송신 실패 | 송신 오류 수가 증가하고 링크 상태가 반영된다. | `NoAckCount`, `LinkState`, `LORA_TDM_APP_LINK_STATUS_MID` 확인 |
| `uplink_app` 설정 반영 | 유효한 runtime configuration 명령 | pending 검증 후 active 설정이 갱신된다. | 설정 상태, sequence, 적용 로그 확인 |
| `uplink_app` 설정 거부 | 범위 오류, CRC 오류, 비호환 설정 | 설정이 거부되고 active 값은 유지된다. | reject code, active config 불변 확인 |
| `uplink_app` UDP 임시 입력 검증 | `localhost` UDP 또는 동등한 mock transport를 통해 정상/비정상 uplink packet을 입력한다. | 실제 하드웨어 없이도 수신, 길이 검증, CRC 검증, sequence 검증, route/viewpoint payload 검증이 수행된다. | accept/reject count, 오류 이벤트, `UPLINK_STATUS_MID`, 로그 확인 |
| `uplink_app` 경로 수정 | 승인된 기존 경로 수정 payload | 경로 수정 정보가 수신·검증되고 상위 임무 계층에 전달된다. | uplink status, route update 처리 로그 확인 |
| `uplink_app` 내부 복구 명령 | parser reset, serial reconnect, app restart request | 승인된 명령만 전달되고 결과가 `UPLINK_STATUS_MID`에 반영된다. | command result, reject/accept count 확인 |
| `cfs_core_app` 경로 상태 반영 | 정상 route update 또는 landing route update 입력 | mission route/landing route cache, route update counter, 마지막 route update 시각이 갱신된다. | HK, route update 로그, `SYSTEM_HEALTH_MID` 연계 상태 확인 |
| `lora_tdm_app` mock sink 출력 | 실제 LoRa 장치 대신 file sink, stdout sink, UDP localhost sink 또는 동등한 mock sink로 송신한다. | TDM downlink packet 생성, `DownlinkSeq` 증가, 송신 카운터 증가, 실패 시 `NoAckCount` 반영이 확인된다. | HK, 송신 로그, mock sink 출력 확인 |
### 16.2 통합 테스트

| 통합 시나리오 | 절차 | 기대 결과 |
| --- | --- | --- |
| FC 상태 수신부터 downlink 송신까지 | FC가 MAVLink 상태를 송신하고 Raspberry Pi가 이를 받아 지상국으로 전송한다. | `mavlink_bridge_app` → `cfs_core_app` → `lora_tdm_app` 경로가 연속 동작하고 지상국에서 상태를 확인할 수 있다. |
| 하드웨어 미연결 내부 계약 시험 | 실제 FC 또는 LoRa 장치 없이 `localhost` UDP 입력과 mock sink 출력을 사용해 uplink/downlink 흐름을 구동한다. | uplink packet 수신, 검증, SB route 전달, `cfs_core_app` 상태 반영, downlink packet 생성까지가 하드웨어 없이 검증된다. 실제 FC mission 반영과 LoRa 물리 송신은 이 시험 범위에 포함되지 않는다. |
| uplink 기반 운용 설정 변경 | 지상국에서 설정 변경 명령을 송신한다. | `uplink_app`가 명령을 검증하고 승인된 값만 active 설정에 반영한다. |
| uplink 기반 경로 수정 | 지상국에서 기존 경로 수정 payload를 송신한다. | 기체 측 cFS가 경로 수정 정보를 수신·검증하고 상위 임무 계층에 반영한다. |
| 통신 장애 후 복구 | FC-UART 링크 또는 LoRa 송신을 일시적으로 차단한 뒤 복구한다. | 시스템 상태가 성능 저하 또는 복구 필요 상태로 전이되었다가, 링크 복구 후 정상 상태로 돌아온다. |
| 앱 재시작 후 지속 상태 복원 | 앱을 재시작하거나 소프트 재부팅을 수행한다. | 필요한 persistent state와 active 설정이 정책에 맞게 복원된다. |

## 17. 기준 결정 사항

이 섹션은 본 문서의 현재 기준값과 정책을 고정한다. 이후 구현은 아래 결정을 기본 계약으로 사용해야 하며, 변경이 필요할 경우 해당 섹션을 직접 수정해야 한다.

### 17.1 MID 및 명령 MID 기준값

현재 기준 MID는 다음과 같이 고정한다.

| 항목 | 값 |
| --- | --- |
| `MAVLINK_BRIDGE_APP_CMD_MID` | `0x18A0` |
| `MAVLINK_BRIDGE_APP_SEND_HK_MID` | `0x18A1` |
| `DOWNLINK_APP_CMD_MID` | `0x18B0` |
| `DOWNLINK_APP_SEND_HK_MID` | `0x18B1` |
| `CFS_CORE_APP_CMD_MID` | `0x18C0` |
| `CFS_CORE_APP_SEND_HK_MID` | `0x18C1` |
| `UPLINK_APP_CMD_MID` | `0x18D0` |
| `UPLINK_APP_SEND_HK_MID` | `0x18D1` |
| `SYSTEM_HEALTH_MID` | `0x1904` |
| `FC_EKF_LOCAL_STATE_MID` | `0x1905` |
| `FC_ATTITUDE_STATE_MID` | `0x1906` |
| `FC_GPS_RAW_STATE_MID` | `0x1907` |
| `FC_EKF_STATUS_MID` | `0x1908` |
| ~~`UPLINK_APP_LORA_RAW_MID`~~ | `0x1909` — **삭제됨(2026-07-14)**. 구 `lora_fc_downlink_app` raw-forward 경로 잔재로 발행자 없이 구독만 남아있었고, `mavlink_bridge_app`의 `FC_SYS_TIME_MID`(동일 `0x1909`, commit `38c2f22`)와 충돌해 코드에서 제거(`ParseLoRaFrame` 포함) |
| `UPLINK_STATUS_MID` | `0x190A` |
| `ROUTE_UPDATE_MID` | `0x190B` |
| `RECOVERY_CMD_MID` | `0x190C` (uplink_app 라우팅) |
| `VIEWPOINT_CMD_MID` | `0x190D` (uplink_app → cfs_core_app) |
| `CONFIG_CMD_MID` | `0x190E` (uplink_app → cfs_core_app / mavlink_bridge_app) |
| `MODE_CMD_MID` | `0x190F` (uplink_app 라우팅) |
| `DIAGNOSTIC_CMD_MID` | `0x1910` (uplink_app 라우팅) |
| `LORA_TDM_APP_LINK_STATUS_MID` | `0x1911` (lora_tdm_app; baseline 등록됨, 2026-06-16 — `lora_fc_downlink_app` 대체) |
| `EXEC_RESULT_MID` | `0x1912` (대상앱→uplink_app 실행결과 회신, BL-08 2026-07-22) |
| `ROUTE_SNAPSHOT_MID` | `0x1913` (cfs_core_app→lora_tdm_app waypoint readback 다운링크, 2026-07-23) |
| `FC_MISSION_READBACK_MID` | `0x1914` (mavlink_bridge_app→cfs_core_app FC 미션 재조회 결과, BL-41 route 2026-07-23 — §5.1.1 표 참조) |

> 명령 라우팅 MID(`0x190C`~`0x1910`)는 `uplink_app`이 검증된 uplink 명령을 클래스별로 publish하는 대상이다(§18.4.7 라우팅 표 참조). `0x1909`는 `lora_tdm_app`이 LoRa로 수신한 "UP,..." 원문을 `uplink_app`에 전달하는 raw frame MID이다(`lora_fc_downlink_app`에서 이관).

`DOWNLINK_STATUS_MID`는 초안 기준 `0x1905`로 할당되었으나, **현재 구현에서 `0x1905`는 `FC_EKF_LOCAL_STATE_MID`로 사용 중**이어서 충돌한다. `lora_fc_downlink_app`의 HK TLM MID는 topic-id 기반으로 별도 할당된다. `DOWNLINK_STATUS_MID = 0x1905`는 구현에 반영되지 않은 초안 할당이므로 사용하지 말 것.

현재 코드베이스의 `lora_fc_downlink_app`은 cFS topic-id 기반 매핑(`DEFAULT_LORA_FC_DOWNLINK_APP_MISSION_CMD_TOPICID`, `...SEND_HK_TOPICID`)을 사용한다. 해당 topic-id 매핑은 본 섹션의 `0x18B0/0x18B1` baseline과 동등한 명령 ingress 의미를 가져야 하며, 플랫폼 설정 시 값 충돌이 없도록 동일 baseline으로 유지해야 한다.

본 문서에서 `UPLINK_CMD_MID`라는 일반 표현은 현재 구현 기준 `UPLINK_APP_CMD_MID`와 동일한 명령 ingress MID를 의미한다. 별도 uplink command gateway MID를 두지 않는 한 두 용어를 동의어로 사용한다.

### 17.2 payload 정밀도 및 엔디안 정책

- cFS 내부 mission payload의 부동소수점 필드는 `IEEE-754 32-bit float`를 사용한다.
- cFS 내부 구조체 직렬화는 Raspberry Pi host endianness에 맞춘 little-endian을 기본값으로 한다.
- transport-specific ASCII 또는 hex frame은 엔디안에 독립적인 canonical text 표현을 사용한다.
- 새 binary transport를 추가할 경우 별도 엔디안 변환 규칙을 명시해야 하며, 명시하지 않으면 little-endian을 사용한다.

### 17.3 앱 시작 순서, 우선순위, 스택

기준 startup 순서는 다음과 같다.

1. `mav_bridge_app`
2. `cfs_core_app`
3. `uplink_app`
4. `lora_tdm_app` (Section 4 downlink 역할 현재 배포 구현체; 2026-06-16~ `lora_fc_downlink_app` 대체)

기준 우선순위 정책:

- `mav_bridge_app`와 `cfs_core_app`은 downlink/uplink 앱보다 높은 우선순위를 가져야 한다.
- 본 섹션의 `mav_bridge_app`은 Section 4의 `mavlink_bridge_app`과 동일한 앱을 의미한다.
- `uplink_app`은 `lora_tdm_app`와 같거나 더 높은 우선순위를 가져야 한다.
- 스택 크기는 각 앱의 현재 cFS sample baseline을 사용하되, parser 또는 route validation 확장으로 인해 오버플로우 위험이 확인되면 그 앱만 상향 조정한다.

### 17.4 앱 기간 제어 기준

- `mavlink_bridge_app`: app-local polling loop를 사용한다.
- `cfs_core_app`: event-driven 입력 처리와 1 Hz health publish를 병행한다.
- `uplink_app`: event-driven command 처리와 1 Hz status publish를 사용한다.
- `lora_tdm_app`: TDM 슬롯 기반 event-driven packet compose를 사용하고, status/HK는 1 Hz 기준을 따른다.

SCH 기반 제어가 추가되더라도 위 publish rate와 event-driven 계약을 깨뜨려서는 안 된다.

### 17.5 명령 권한 부여 기준

기준 권한 수준은 다음과 같이 고정한다.

| 권한 수준 | 허용 명령 |
| --- | --- |
| Level 1 | NOOP, HK request, diagnostic read-only, 상태 조회 |
| Level 2 | route update, viewpoint update, runtime configuration |
| Level 3 | recovery command, mode command, counter management |

Level 3 명령은 명시적 운용자 승인과 request token 검증을 모두 만족해야 한다.

이 표는 권한 수준 체계와 명령 클래스별 기본 권한 수준을 고정한다. 개별 function code 또는 세부 command code와 Level 간의 1:1 매핑은 각 앱 command dictionary 구현 시 본 표를 따라 채워야 한다.

### 17.6 `uplink_app` 명령 라우팅 기준

| 명령 클래스 | 기본 대상 |
| --- | --- |
| runtime configuration | 대상 앱 설정 인터페이스 또는 `cfs_core_app` |
| route update | `cfs_core_app` mission route consumer |
| viewpoint update | `cfs_core_app` 또는 후속 planner consumer |
| recovery command | `cfs_core_app` 또는 대상 bridge/app component |
| mode command | `cfs_core_app` |
| diagnostic command | 대상 앱 diagnostic interface |
| counter management | `cfs_core_app` 또는 대상 앱 |

### 17.7 앱별 command interface 기준

- `mavlink_bridge_app`의 외부 승인 command는 `NOOP`, `RESET_COUNTERS`, `SEND_HK`로 제한한다.
- `cfs_core_app`의 외부 승인 command는 `NOOP`, `RESET_COUNTERS`, `SEND_HK`로 제한한다.
- `uplink_app`의 외부 승인 command는 `NOOP`, `RESET_COUNTERS`, `SEND_HK`, `PROCESS_UPLINK`를 포함한다.
- `lora_tdm_app`의 외부 승인 command는 `NOOP`, `RESET_COUNTERS`, `SEND_HK`로 제한한다(`DIAGNOSTIC_CMD_MID`는 `uplink_app` 라우팅 경로로만 수신).

### 17.8 시간 기준 유지 정책

- 현재 기준 시간축은 `mission elapsed millisecond`로 고정한다.
- 하드웨어 테스트 이후에도 절대 시간 기준을 기본값으로 승격하지 않는다. 절대 시간은 필요 시 보조 필드로만 추가한다.

### 17.9 상태별 앱 운영 정책 확정

- `CFS_NOMINAL`: 모든 기준 앱 활성
- `CFS_DEGRADED`: 모든 기준 앱 활성, 단 정상 상태가 필요한 명령만 차단
- `CFS_RECOVERY`: `cfs_core_app`와 필수 bridge/status path 유지, 구성 변경과 mode 변경 차단
- 최소 보고 시작: `cfs_core_app`, 필수 bridge/status publish, 오류 보고만 유지
- `CFS_DEGRADED -> CFS_NOMINAL` 복귀 조건: 필수 입력(`FC_ATTITUDE_STATE_MID`, `FC_EKF_LOCAL_STATE_MID`, `FC_EKF_STATUS_MID`, `MAVLINK_BRIDGE_APP_HK_TLM_MID`)이 freshness/유효성 규칙을 모두 만족하고 active critical FaultCode가 없는 상태가 연속 `10 s` 유지될 때 복귀한다. (GPS는 §15 정책에 따라 health 게이트에서 제외, 보고 전용.)
- `CFS_RECOVERY -> CFS_NOMINAL` 복귀 조건: 복구 대상으로 지정된 필수 입력이 모두 복원된 뒤, 위 `CFS_DEGRADED -> CFS_NOMINAL` 조건을 동일하게 연속 `10 s` 만족할 때 복귀한다.

### 17.10 시퀀스 및 정확성 기준

- 모든 mission state payload는 단조 증가 `SequenceCounter`를 사용한다.
- `uplink_app` command sequence는 strict monotonic increase를 기본 정책으로 사용한다.
- 동일 sequence 또는 감소한 sequence는 replay로 거부한다.
- wraparound는 baseline 구현에서 지원하지 않으며, wraparound가 필요하면 boot boundary와 함께 별도 정책을 추가해야 한다.

### 17.11 복구 상수 기준값

- link/parser soft retry interval: `5 s`
- soft retry max count: `3`
- critical app restart interval: `60 s`
- critical app restart max count: 앱당 `3`
- watchdog service period: `1 s`
- watchdog timeout: `30 s`
- reboot loop detection window: `30 s`
- app restart abuse window: `600 s`

### 17.12 하드웨어 연결 토폴로지 기준

기준 하드웨어 토폴로지는 다음과 같다.

- FC -> Raspberry Pi: UART MAVLink
- Ground uplink -> Raspberry Pi: LoRa 또는 동등한 승인된 transport, host-side bridge를 통해 표준 uplink envelope으로 변환
- Raspberry Pi -> Ground downlink: LoRa 또는 동등한 승인된 downlink transport

이 baseline에 포함되지 않은 미래 payload path는 현재 스펙 범위 밖으로 두며, 추가 시 별도 섹션으로 정의한다.

## 18. `uplink_app` 사양

### 18.1 목적 및 범위

`uplink_app`은 지상국 또는 운용자로부터 명령을 수신하고 각 업링크 패킷의 유효성을 검사한다. 이후 승인된 runtime configuration 변경, 기존 임무 경로 수정 payload, 승인된 viewpoint payload, 내부 복구 명령을 적절한 mission app으로 전달한다.

`uplink_app`은 지상국 기원 명령이 cFS mission app layer로 진입하는 유일한 승인 경로이다. 다른 앱은 raw ground command를 직접 수신해서는 안 된다.

`uplink_app`은 경로 수정 명령을 제외한 FC 직접 제어 명령을 발행해서는 안 된다. 여기서 FC 직접 제어 명령에는 비행 모드 변경, FC-level mission 변경, 모터 또는 액추에이터 제어, FC-level 파라미터 변경이 포함되며, 이러한 명령은 Section 3의 기본 플랫폼 경계 정책에서 금지된다. 경로 수정 명령은 비행체가 따라갈 임무 경로에 영향을 줄 수 있는 예외적인 명령 클래스이지만, `uplink_app`이 이를 직접적인 자세 제어, 모터 제어, 또는 FC 내부 제어 명령으로 실행하는 것은 아니다. `uplink_app`은 수신된 경로 정보를 검증한 뒤 상위 임무 계층 또는 경로 관리 계층에 전달하는 역할만 수행하며, FC 안전 정책을 우회해서는 안 된다.

route update의 downstream 경계는 다음과 같이 고정한다.

- `uplink_app`은 route update를 검증된 mission-layer route segment로만 변환한다.
- 현재 기준 route consumer는 `cfs_core_app` 또는 그가 소유한 mission route cache/interface이다.
- 이 문서 범위에서 route update 이후 FC 반영은 "mission layer가 자체 안전 정책과 승인 경계를 거쳐 수행하는 별도 단계"로 정의한다.
- `uplink_app`과 `cfs_core_app`은 FC-level mission upload, FC mode change, actuator command를 직접 수행해서는 안 된다.
- FC 반영 경로가 이후 정의되더라도, 그것은 별도 mission execution contract로 문서화되어야 하며 본 `uplink_app` 예외 규정에 암묵적으로 포함되지 않는다.

### 18.2 승인된 명령 클래스

`uplink_app`은(는) 다음 클래스의 명령만 수락하고 라우팅해야 합니다.

| 명령 클래스 | 예 | 메모 |
| --- | --- | --- |
| 런타임 구성 | 수신 기간 변경, 메시지 활성화/비활성화, 게시 속도 변경, 제한 시간 임계값 변경, 진단/로그 수준 변경 | 보류 중인 구성 버퍼를 통해 라우팅됩니다. 활성화 전에 검증됨 |
| 경로 업데이트 | 기존 경로 포인트 수정, 승인된 경로 업데이트 페이로드 | FC 직접 제어 명령의 일반 금지 범위에서 예외적으로 허용되는 클래스이지만, `uplink_app`은 이를 검증된 경로 정보 전달로만 처리해야 한다. FC 비행 모드를 직접 변경하거나 FC 안전 점검을 우회해서는 안 된다. |
| 관점 업데이트 | 지상 세그먼트에서 승인된 관측점 또는 관측점 페이로드 | 미션 계층 경로 계획 입력으로 사용됩니다. 경로 업데이트를 사용하기 전에 유효성을 검사해야 합니다. |
| 복구 명령 | 파서 재설정, 직렬 재연결, 카운터 재설정, 앱 재시작 요청 | 대상 앱 또는 복구 기관으로 라우팅됩니다. 승인 정책에 따라 |
| 모드 명령 | 복구 모드 진입, 복구 모드 종료 | 승인된 cFS 상태 및 운영자 권한 수준 내에서만 허용됩니다. |
| 진단 명령 | 진단 캡처 활성화/비활성화, 로그 수준 변경 | 즉시 또는 일회성; 보류 중인 버퍼가 필요하지 않습니다. |
| 카운터 관리 | 지속적인 카운터 재설정 | 섹션 11.4.2에 따라 명시적인 명령 권한이 필요합니다. |

이 클래스 외부의 명령은 다음을 통해 거부되고 보고됩니다.
`UPLINK_STATUS_MID`에 오류 코드가 있습니다.

### 18.3 금지된 명령 클래스

`uplink_app`은(는) 다음 클래스의 명령을 거부하고 보고합니다.

- 비행 컨트롤러 재부팅 또는 재설정.
- 비행 제어 펌웨어 재설정.
- 모터 또는 액추에이터 명령.
- FC를 통해 센서 전원을 껐다 켭니다.
- 비행 모드, FC 수준 임무 업로드 또는 비행 제어 매개변수 변경.
- 보류 중인 구성 검증 모델을 우회하는 모든 명령입니다.

### 18.4 업링크 패킷 검증

수신된 모든 업링크 패킷은 라우팅 전에 검증되어야 하며, 검증 항목은 다음과 같다.

- 패킷 길이 및 형식.
- 명령 코드 및 대상 앱 식별자.
- 패킷 버전 및 호환성.
- CRC 또는 체크섬 무결성.
- 중복 및 재생 감지를 위한 시퀀스 번호입니다.
- 명령 클래스에 필요한 권한 수준입니다.
- 경로 또는 시점 페이로드 형식, 경계 및 임무 계층 호환성
  경로 업데이트 또는 관점 명령.
- 현재 cFS 상태 호환성(일부 명령은 `CFS_RECOVERY`에서 차단됨)
  또는 최소 보고 시작).

유효성 검사에 실패한 패킷은 폐기됩니다. 거부는 다음과 같습니다
해당 오류 코드와 함께 `UPLINK_STATUS_MID`을(를) 통해 보고됩니다. 그만큼
활성 구성 및 시스템 상태는 거부된 패킷으로 수정되어서는 안 됩니다.

#### 18.4.1 권장 내부 처리 흐름

`uplink_app`의 내부 처리 흐름은 최소한 다음 단계로 분리되어야 한다.

1. **수신 단계**
   - 승인된 전송 입력에서 raw uplink packet을 수신한다.
   - 수신 시각, 전송 식별자, raw length를 임시 수신 컨텍스트에 기록한다.
2. **구문 및 무결성 검증 단계**
   - 패킷 길이, 헤더 형식, 버전, CRC 또는 체크섬을 검사한다.
   - 이 단계에서 실패한 패킷은 즉시 폐기하고, reject counter와 오류 코드를 갱신한다.
3. **명령 분류 단계**
   - 명령 코드를 기준으로 runtime configuration, route update, viewpoint update, recovery command, diagnostic command 중 하나로 분류한다.
   - 대상 앱 또는 대상 기능 식별자를 확인한다.
4. **정책 검증 단계**
   - 현재 cFS 상태, 권한 수준, 명령 클래스 허용 여부를 검사한다.
   - 금지된 명령 클래스 또는 현재 상태에서 허용되지 않는 명령은 거부한다.
5. **명령별 처리 단계**
   - runtime configuration은 pending buffer 경로로 보낸다.
   - route/viewpoint payload는 형식과 경계 검증 후 상위 임무 계층에 전달 가능한 내부 표현으로 변환한다.
   - recovery command는 대상 앱 또는 `cfs_core_app`으로 라우팅한다.
6. **결과 보고 단계**
   - 명령 수락/거부/라우팅 실패/롤백 결과를 `UPLINK_STATUS_MID`에 반영한다.
   - 마지막 명령 코드, 마지막 명령 결과, reject count, route update count 등을 갱신한다.

위 단계는 단일 함수에 혼합하지 말고, 최소한 `수신`, `검증`, `분류`, `명령별 처리`, `상태 보고` 책임으로 분리하는 것을 권장한다.

#### 18.4.2 권장 내부 상태

`uplink_app`은 최소한 다음 내부 상태를 유지해야 한다.

- 마지막으로 수신된 명령 코드
- 마지막으로 수신된 명령 시퀀스 번호
- 마지막 명령 처리 결과
- 누적 수락 횟수 및 거부 횟수
- 라우팅 실패 횟수
- 전송 상태(`NOMINAL`, `DEGRADED`, `LOST`, `FAILED`)
- pending configuration 존재 여부
- 마지막 route update 처리 결과
- 마지막 viewpoint update 처리 결과

이 상태는 HK 또는 `UPLINK_STATUS_MID`를 통해 검증 가능해야 한다.

#### 18.4.3 공통 명령 envelope 계약

`uplink_app`이 소비하는 내부 표준 uplink command envelope은 최소한 다음 필드를 포함해야 한다.

| 필드 | 형식 | 의미 | 검증 규칙 |
| --- | --- | --- | --- |
| `version` | `uint8` | uplink protocol version | 현재 지원 버전과 일치해야 한다. |
| `command_class` | `uint8` | 명령 클래스 식별자 | Section 18.2의 승인된 클래스 중 하나여야 한다. |
| `payload_length` | `uint8` 또는 동등한 길이 필드 | payload 실제 길이 | 선언 길이와 실제 길이가 일치해야 하며, 최대 payload 길이를 초과해서는 안 된다. |
| `flags` | `uint8` | 전송 또는 처리 옵션 | 정의되지 않은 비트는 `0`이어야 한다. |
| `sequence` | `uint16` 또는 동등한 단조 증가 시퀀스 | replay/duplicate 탐지용 식별자 | 허용된 시퀀스 정책을 만족해야 한다. |
| `target_id` | `uint8` 또는 동등한 대상 식별자 | 대상 앱 또는 대상 기능 식별자 | 해당 `command_class`에서 허용된 대상 집합에 포함되어야 한다. |
| `payload` | 가변 길이 byte array | 클래스별 데이터 | 클래스별 payload 계약과 일치해야 한다. |

`target_id`가 wire format에 독립 필드로 존재하지 않는 경우, `command_class`와 `payload` 내부의 첫 필드 조합으로 대상 기능을 판별할 수 있다. 그러나 구현은 입력 계약 문서에 대상 식별 방식이 명시되어 있지 않은 payload를 수락해서는 안 된다.

**Flags 비트 정의** (§18.4.3.1, 2026-07-22 정정·확장):

| Bit | 이름 | 의미 | 정의 시점 |
|---|---|---|---|
| [7:6] | `AUTH_LEVEL` | 요청 권한 수준 | §18.11 권한 검증 (Phase 3.2) |
| [5:3] | 예약 | 향후 확장용 | 0으로 설정 |
| [2:1] | `RETX_IDX` | 재전송 슬롯 인덱스: 0~3 = (전송 슬롯 번호 − 1). 0=최초 전송. 지상 4x 슬롯 재전송(§4x)에서 각 사본마다 다르게 설정 — 기체는 검증에 사용하지 않고 EVS 진단 로그에만 표기(RF 링크 마진 진단, BL-14) | 2026-07-22 (BL-14) |
| [0] | `FORCE_FLAG` (`0x01`) | health gate 강제 통과 (§18.10.2) | 2026-07-21 |

> **정정 이력(2026-07-22)**: 이 표는 원래 `[5:0]=예약`으로만 기재돼 있었으나,
> bit[0] `FORCE_FLAG`는 2026-07-21에 이미 구현됐음에도 표에 미반영된 stale
> 상태였음(§18.10.2 본문에만 존재). BL-14 `RETX_IDX` 추가와 함께 정정.
> `RETX_IDX`는 명령 정확성과 무관한 순수 진단 필드 — 기체는 이 값으로
> 동작을 바꾸지 않으며(수락/거부 판정에 미사용), CRC canonical 문자열에는
> Flags 전체가 포함되므로 사본마다 CRC가 달라진다(같은 seq이므로 기체
> DUPLICATE 판정에는 영향 없음).

`AUTH_LEVEL` 값:
- `0b00` (0): Level 1 (읽기 전용, NOOP/DIAGNOSTIC)
- `0b01` (1): Level 2 (구성 명령, runtime cfg/route/viewpoint update)
- `0b10` (2): Level 3 (복구 명령, recovery/mode/counter management)
- `0b11` (3): 예약

`uplink_app`은 transport-specific raw frame을 직접 표준 envelope으로 간주해서는 안 된다. LoRa ASCII frame, UDP mock frame, 또는 동등한 승인된 transport-specific 입력은 먼저 transport 계층에서 framing/CRC를 처리한 뒤 표준 envelope으로 변환되어야 한다.

#### 18.4.4 transport 계층과 `uplink_app` 책임 경계

transport 계층과 `uplink_app`의 책임은 다음과 같이 분리되어야 한다.

| 항목 | transport 계층 책임 | `uplink_app` 책임 |
| --- | --- | --- |
| serial open/reopen | 필수 | 아님 |
| LoRa ASCII framing | 필수 | 아님 |
| raw frame CRC 또는 checksum | 필수 | 아님 |
| raw frame length/field parse | 필수 | 아님 |
| 표준 envelope version 확인 | 가능 | 필수 |
| class 허용 여부 판단 | 아님 | 필수 |
| replay/duplicate 방지 | 권장 | 필수 |
| payload semantic validation | 아님 | 필수 |
| cFS 상태/권한 정책 판단 | 아님 | 필수 |
| 대상 앱 라우팅 및 상태 보고 | 아님 | 필수 |

transport 계층이 replay 또는 duplicate frame을 선행 폐기하더라도, `uplink_app`은 마지막으로 승인된 시퀀스 또는 동등한 replay 보호 상태를 자체적으로 유지해야 한다. 이중 보호는 transport 교체나 bypass test path에서도 동일한 안전 정책을 보장하기 위함이다.

#### 18.4.5 명령 클래스별 계약 개요

각 명령 클래스는 최소한 다음 계약 요소를 문서화해야 한다.

| 명령 클래스 | payload 정의 상태 | 최소 출력 계약 | 현재 구현 우선순위 |
| --- | --- | --- | --- |
| runtime configuration | 필수 | pending buffer 또는 대상 앱 설정 인터페이스 | 높음 |
| route update | 필수 | 검증된 route segment 구조 또는 `ROUTE_UPDATE_MID` | 구현 우선 완료 |
| viewpoint update | 필수 | 검증된 viewpoint 구조 또는 대상 planner 입력 | 높음 |
| recovery command | 필수 | 대상 앱 또는 `cfs_core_app`로의 recovery request | 높음 |
| mode command | 필수 | 모드 전이 요청 또는 거부 결과 | 중간 |
| diagnostic command | 필수 | 진단 실행 또는 진단 설정 반영 결과 | 중간 |
| counter management | 필수 | 허용된 카운터 reset 요청 결과 | 중간 |

payload 정의 상태가 `필수`인 클래스는 payload 필드, 길이, 값 범위, 허용 상태, reject code를 모두 명시하기 전까지 구현 완료로 간주해서는 안 된다.

#### 18.4.6 명령 클래스별 입력/출력 계약

##### 18.4.6.1 runtime configuration

runtime configuration payload는 최소한 다음 필드를 포함해야 한다.

| 필드 | 형식 | 의미 | 검증 규칙 |
| --- | --- | --- | --- |
| `config_scope` | `uint8` | 대상 앱 또는 전역 범위 | 승인된 대상만 허용 |
| `config_version` | `uint8` | 구성 payload 버전 | 현재 지원 버전과 일치 |
| `parameter_id` | `uint16` 또는 동등한 식별자 | 변경 대상 파라미터 | 승인된 파라미터 집합에 포함 |
| `value_type` | `uint8` | 값 형식 | 대상 파라미터 타입과 일치 |
| `value_length` | `uint8` 또는 `uint16` | 값 길이 | 선언 길이와 실제 길이 일치 |
| `value` | 가변 길이 byte array | 새 값 | 범위, 단위, 상태 호환성 충족 |

출력 계약:

- 유효한 payload는 대상 앱의 pending buffer 또는 동등한 설정 인터페이스에 전달되어야 한다.
- `uplink_app`은 active configuration을 직접 덮어쓰지 않아야 한다.
- 설정 반영 결과는 `pending`, `accepted`, `rejected`, `rolled_back` 중 하나로 `UPLINK_STATUS_MID`에 반영되어야 한다.

거부 조건:

- 알 수 없는 `parameter_id`
- 허용 범위 초과
- 대상 앱 상태와 비호환
- 체크섬 또는 CRC 검증 실패
- 권한 부족

##### 18.4.6.2 route update

> **개정 이력 (BL-56, 2026-07-25 설계 확정 — 구현 전)**
> 원 설계(REPLACE/APPEND/DELETE, 좌표만 있는 waypoint, 2.0m 정확 간격 강제)는
> 다음 세 가지 실사용 문제로 재설계한다: ① REPLACE 기반 워크플로가 PX4 미션 인덱스를
> 리셋시켜 비행 중 경로 수정이 사실상 불가능했음(§18.4.6.8 flight_mode의
> waypoint_start_index로 우회하던 번거로움) ② 세그먼트 거리 2.0m 강제가 BL-44
> 2-pass 원형 보정 전용 제약인데 일반 경로(BL-57 지도 입력 등)에도 걸려 있어
> RT-ROUTE 실기 테스트(BL-55)에서 실제로 정상 요청이 거부됨 ③ waypoint가 좌표뿐이라
> 호버/loiter 등 명령 파라미터를 표현할 수 없었음(2-pass lap1의 `NAV_LOITER_UNLIM` 등).
> 아래는 개정된 설계다.

**waypoint 구조 확장** (`ROUTE_WAYPOINT_t`, `shared_msgs/route_msg.h`):

| 필드 | 형식 | 의미 |
| --- | --- | --- |
| `CmdType` | `uint8` | MAVLink MAV_CMD: `NAV_WAYPOINT=16`(기본), `NAV_LOITER_UNLIM=17`, `NAV_LOITER_TIME=19` 등 |
| `Param1`~`Param4` | `float` | CmdType별 의미(hold time, loiter radius, acceptance radius, yaw 등 — MAVLink MISSION_ITEM param1~4와 동일 관례) |
| `UseGlobal` | `uint8` | `1`=절대좌표(`LatE7`/`LonE7`) 사용, `0`=로컬 NED(`X`/`Y`) 사용 |
| `LatE7`, `LonE7` | `int32` | 절대 위경도(degE7). `UseGlobal=1`일 때만 유효 |
| `X`, `Y` | `float` | 로컬 NED 좌표(m). `UseGlobal=0`일 때만 유효 |
| `Z` | `float` | 고도(m, relative alt) — `UseGlobal` 값과 무관하게 항상 유효 |

**절대좌표 경로 (BL-57 지도 입력용, 2026-07-25 확정)**: `mavlink_bridge_app`이 로컬→전역 변환에 쓰는
`RefLatE7`/`RefLonE7`(§ 위 참조)는 고정 홈이 아니라 **가장 최근 `GLOBAL_POSITION_INT` 수신 시점의
기체 현재 위치**로 매번 갱신되는 값이다 — 지상국이 지도 클릭 시점에 이 값을 내려받아 lat/lon→로컬
X/Y로 변환해도, 실제 미션 업로드 시점엔 기체가 이동해 기준점이 달라져 있을 수 있어 waypoint가
의도한 절대 위치에서 어긋난다(드리프트). 이를 피하기 위해 지도 입력은 `UseGlobal=1` +
`LatE7`/`LonE7`로 **절대좌표를 그대로** 실어 보낸다. `mavlink_bridge_app`은 `UseGlobal=1`인
waypoint에 대해 `RefLatE7`/`RefLonE7` 기반 로컬→전역 변환을 **수행하지 않고**, 수신한 `LatE7`/`LonE7`을
`MISSION_ITEM_INT`에 그대로 기입한다(현재 `MAVLINK_BRIDGE_APP_SendMissionItemInt()`의 변환 분기를
`UseGlobal` 값으로 스킵). 일반 로컬 좌표 경로(REPLACE/2-pass 등 기존 X/Y/Z 기반)는 `UseGlobal=0`으로
기존 변환 로직을 그대로 사용한다.

route update payload는 다음 필드를 포함한다.

| 필드 | 형식 | 의미 | 검증 규칙 |
| --- | --- | --- | --- |
| `route_op` | `uint8` | 경로 연산 타입: `REPLACE=1`, `ADD=2`, `DELETE=3`, `MODIFY=4` | 승인된 op 값만 허용 |
| `route_version` | `uint8` | payload 버전 | 현재 지원 버전과 일치 |
| `index_or_count` | `uint8` | REPLACE/ADD: 신규 waypoint 개수; DELETE/MODIFY: 대상 인덱스(단일) | REPLACE/ADD는 `1` 이상 MAX 이하; DELETE/MODIFY는 `0` 이상 현재 waypoint 개수 미만 |
| `waypoints` | waypoint 배열 | route segment (REPLACE/ADD: N개; MODIFY: 1개; DELETE: 없음) | finite, flyable area, altitude 조건 충족(세그먼트 거리 제약 없음) |

연산 타입별 의미 및 ARMED 정책:

- **REPLACE**: 기존 active route를 `waypoints` 배열로 완전 대체한다. **ARMED 상태에서 차단**(기존 `IsArmed` 가드 유지) — 전체 교체는 위험한 연산으로 간주.
- **ADD**: active waypoint cache 끝에 `waypoints`를 추가한다(합계가 MAX 초과 시 MAX에서 절단). ARMED 허용. `ActiveResumeIndex` 불변.
- **DELETE(index)**: active cache에서 `index` 위치의 waypoint 1개를 제거하고 뒤 인덱스를 당긴다. ARMED 허용. `index == ActiveResumeIndex`(현재 향하고 있는 지점)이면 **거부**(REJECT_ROUTE) — 진행 중인 목표점은 삭제 불가, 필요 시 먼저 다른 인덱스로 진행되길 기다리거나 MODIFY로 좌표만 바꿀 것. `index < ActiveResumeIndex`면 `ActiveResumeIndex -= 1`.
- **MODIFY(index)**: active cache의 `index` 위치 waypoint를 새 값으로 덮어쓴다. ARMED 허용. `ActiveResumeIndex` 불변(같은 슬롯 좌표만 변경 — `index == ActiveResumeIndex`인 경우 기체는 재업로드 직후 새 좌표를 향해 즉시 진로를 바꾼다).

**비행 중 재개 지점 유지 (인덱스 리셋 문제 해결)**: PX4(`mavlink_mission.cpp`)는 미션 업로드 트랜잭션 중 `current=1`이 찍힌 `MISSION_ITEM_INT`의 seq를 그대로 재개 인덱스로 채택한다(`update_active_mission()`이 그 seq를 `current_seq`로 발행) — 별도 `MISSION_SET_CURRENT` 명령이 불필요하다. 따라서 ADD/DELETE/MODIFY로 인한 모든 재업로드는 `mavlink_bridge_app`이 유지하는 `ActiveResumeIndex`(FC의 `MISSION_CURRENT` 텔레메트리로 갱신)에 해당하는 항목에 `current=1`을 찍어 전송한다. REPLACE는 ARMED 차단으로 항상 지상(정지) 상태에서만 발생하므로 `ActiveResumeIndex`는 항상 0(필요 시 §18.4.6.8 `FLIGHT_MODE(WAYPOINT, waypoint_start_index)`로 별도 지정).

route update baseline 수치 기준:

- `MAX_ROUTE_WAYPOINT_COUNT = 16`
- 인접 waypoint 간 거리 제약: **폐지**(2026-07-25) — flyable area(`±50m` X/Y)·고도(`2m~8m`) 범위 검증만 유지. 2.0m 정확 간격이 필요한 2-pass 원형 보정(§18.4.6.2.1)은 지상국이 스스로 그렇게 계산해 올리는 것으로 충분, 기체측 강제 불필요.

출력 계약:

- 유효한 payload는 mission layer가 직접 사용할 수 있는 검증된 route segment 구조로 변환되어야 한다.
- 최소 출력 필드는 `route_op`, `route_version`, `index_or_count`, waypoint 배열(REPLACE/ADD/MODIFY)이다.
- 현재 구현에서 내부 bus message를 사용하는 경우, 그 message는 raw payload copy가 아니라 검증된 구조 표현이어야 한다.

거부 조건:

- payload 길이 불일치 (DELETE는 고정 길이, REPLACE/ADD는 `N * sizeof(ROUTE_WAYPOINT_t)` 가변, MODIFY는 `1 * sizeof(ROUTE_WAYPOINT_t)` 고정)
- 승인되지 않은 `route_op` 값
- waypoint 수/인덱스 위반 (REPLACE/ADD: `0` 또는 MAX 초과; DELETE/MODIFY: 범위 밖 인덱스)
- 좌표가 finite가 아님 (REPLACE/ADD/MODIFY)
- 비행 가능 영역 위반 (REPLACE/ADD/MODIFY)
- 고도 제약 위반 (REPLACE/ADD/MODIFY)
- REPLACE가 ARMED 상태에서 수신됨
- DELETE의 `index == ActiveResumeIndex`

##### 18.4.6.2.1 route update — 2-pass GPS 능동 보정 (2026-07-24 개정 — 지상국 연산 방식)

> **아키텍처 개정 이력 (BL-44, 2026-07-24)**
> 원 설계(2026-07-11)는 원 피팅·상태머신·1500샘플 버퍼를 **기체(companion) 온보드**에
> 두고 지상국을 거치지 않고 자동 수행하도록 했다. 재검토 결과, 이 시스템에서는
> **연산을 지상국으로 이관**하는 것으로 확정한다(사용자 결정). 근거: ① 최소자승
> 원 피팅·상태머신·대용량 버퍼는 C 비행 소프트웨어에서 크고 테스트하기 어렵고
> 안전 민감한 덩어리인데, 지상 Python(numpy)에선 수 줄이고 사람이 보정 경로를
> 검토 후 업로드할 수도 있다. ② route 명령·위치 텔레메트리 경로가 이미 존재한다.
> ③ 기체는 "주어진 경로를 난다"만 하면 되고 그건 이미 구현돼 있다. 아래는 개정된
> 책임 분담이다.

REPLACE 연산에 한해, payload의 `reserved` 필드를 2-pass 보정 활성화 플래그로 사용한다.
총 랩 수는 2로 고정하며 추가 반복은 없다.

**대상 범위**: 보정은 `MISSION_EXTENSION` 세그먼트에만 적용된다. `LANDING` 세그먼트
(별도 route 캐시)와 이륙 절차(이 시스템의 route 관리 범위 밖)는 영향받지 않는다.

**책임 분담 (개정)**:

| 단계 | 담당 | 내용 |
| --- | --- | --- |
| 상태머신(`IDLE→LAP1→CORRECTING→LAP2→DONE`) | **지상국** | 랩 진행·전이·타임아웃 관리 |
| lap 1 위치 샘플 수집 | **지상국** | 다운링크되는 위치 텔레메트리(DL2)를 지상에서 누적 |
| 최소자승 원 피팅 | **지상국** | 실측 (cx,cy,r) 산출, 계획 원과 편차로 waypoint 보정 |
| 보정 route 업로드 | **지상국** | 보정 계산 결과 점(a,b,c)을 **평범한 REPLACE waypoint로 재업로드** (특수 item·플래그 없음) |
| 호버링/재개/착륙 | **지상국→기체** | 명시 **비행모드 명령 3종(HOVER/WAYPOINT/LAND)**으로 오케스트레이션 (아래 base 배선) |

**좌표계 (2026-07-25 확정)**: 원 피팅·편차 계산은 전부 **로컬 접평면(local tangent plane) 미터
좌표**로 수행한다. 위경도(도)는 위도·경도의 실거리 축척이 달라(경도는 `cos(위도)`만큼 수축) 원을
그대로 피팅하면 타원으로 왜곡되고, 이 시스템 나머지(flyable area, LOCAL_POSITION_NED 샘플, 기존
REPLACE waypoint)도 전부 미터 단위라 통일이 맞다(PX4/ArduPilot EKF 로컬 origin, ROS `map` 프레임
등 GPS 항법 시스템의 표준 패턴과 동일). 이 로컬 좌표계의 원점은 **lap 1 시작 시점 기체 위치를
한 번 스냅샷**해서 세션 내내 고정한다 — `mavlink_bridge_app`의 `RefLatE7`/`RefLonE7`(매
`GLOBAL_POSITION_INT`마다 갱신되는 현재 위치, §18.4.6.2 참조)을 그대로 쓰면 세션 도중 기준점이
계속 움직여 원피팅 좌표계가 흔들리므로 **사용 금지** — 지상국이 별도로 lap 1 시작 시점 값을
한 번 캡처해 세션 동안 재사용한다.

**lap 1 — 데이터 수집(지상)**: 지상국은 lap 1 동안 다운링크되는 위치(DL2 `x,y,z` i16 cm,
또는 `lat/lon` — 위 고정 원점으로 변환 후 사용)를 누적한다. 온보드 5Hz 1500샘플 대신 다운링크된
더 적고 거친 샘플을 쓰지만, 원 피팅은 원 둘레에 잘 퍼진 수십 점이면 충분하다. lap 1 완료 판정은
FC가 발행하는 `MISSION_ITEM_REACHED`(마지막 waypoint seq)를 다운링크로 관측한 시점.

**CORRECTING — 보정 계산(지상)**:
1. 누적 샘플에 최소자승 원 피팅 → 실측 중심(cx, cy)·반지름(r). 유효 샘플이 3개 미만이면
   피팅하지 않고 원본(lap 1) route로 lap 2를 재비행(폴백).
2. 계획 waypoint 배열에도 동일 피팅 → 계획 중심(cx_plan, cy_plan), 각도
   `θ_i = atan2(Y_i − cy_plan, X_i − cx_plan)`.
3. 보정 waypoint `i` = `(cx + r·cosθ_i, cy + r·sinθ_i)`. 고도(Z)는 원본 유지(수평만 보정).
4. 보정 route는 기체측 §18.4.6.2 REPLACE 검증을 다시 통과해야 한다(비행 가능 영역·finite).
   인접 waypoint 거리 제약은 적용하지 않는다(프로토타입 잔재). 편차 크기 임계값은 두지
   않으며 측정 편차는 크기와 무관하게 항상 반영(노이즈 상쇄는 다중 샘플 평균화에 의존).
5. **전송 시 반드시 절대좌표(`UseGlobal=1`) 사용(2026-07-25 확정)**: 보정 계산은 lap 1 시작
   시점 고정 원점 기준 로컬 좌표(cx, cy 등)로 하지만, 그 결과를 로컬 X/Y(`UseGlobal=0`)로
   그대로 REPLACE 전송하면 `mavlink_bridge_app`이 **업로드 시점의 현재 위치**(드리프트하는
   `RefLatE7`/`RefLonE7`)를 기준으로 재변환해 lap 1 시작 이후 이동한 거리만큼(최대 원주 전체,
   수 m 단위) 어긋난다 — 보정 자체가 무의미해질 수 있는 오차. 따라서 지상국이 보정된 로컬
   좌표를 **lap 1 고정 원점으로 직접** 절대 위경도(LatE7/LonE7)로 변환한 뒤, §18.4.6.2에서
   정의한 `UseGlobal=1` 경로로 전송해 기체측 재변환(및 그로 인한 기준점 불일치)을 건너뛴다.

**LAP2 전이**:
- 보정 성공 시: 지상국이 보정 계산 결과 점(a,b,c)을 **평범한 REPLACE waypoint로 재업로드**한다
  (미션에 LOITER item을 심거나 특수 플래그를 두지 않는다). 재업로드는 기체가 **HOVER 모드로
  대기 중인 동안** 수행하고, 완료(`MISSION_ACK` accepted) 후 지상이 **WAYPOINT 모드 명령**을
  보내 lap 2를 시작한다.
- 보정 실패/피팅 무효, 또는 **타임아웃 내 보정 route 미도착(LoRa 링크 단절 등)** 시:
  지상은 원본(lap 1) route로 WAYPOINT 모드를 재명령해 lap 2를 재비행. 미션을 reject/중단하지
  않는다. (링크 의존은 지상 연산 방식의 대가 — 이 폴백으로 안전 확보.)

**lap 2 종료**: lap 2 마지막 waypoint 도달 후 지상이 HOVER 또는 LAND 모드를 명령하고 DONE 종료.
추가 보정·3회차 이상 반복 없음.

**출력**: 랩 번호·보정 성공/폴백·산출 (cx,cy,r)는 **지상국**이 표시·기록한다.

**설계 재정의(BL-44, 2026-07-24)**: 보정된 경로는 특별한 게 아니라 **평범한 waypoint 목록**이므로
(지상이 점 a,b,c를 REPLACE로 올리면 끝), 미션 내 LOITER_UNLIM·2-pass 플래그·"호버 중에만 업로드"
게이트 같은 온보드 특수 처리를 모두 걷어낸다. 기체가 필요한 것은 **비행모드 base 명령 3종**뿐이고,
2-pass는 지상이 이 base를 조합해 오케스트레이션한다(waypoint→끝 감지→HOVER→보정 재업로드→
waypoint→…→HOVER/LAND).

> **base 배선 — 비행모드 명령 3종(구현 대상, 하드웨어 검증과 분리)**: `HOVER`/`WAYPOINT`/`LAND`.
> 신규 command class(FLIGHT_MODE)로 `uplink_app`이 수신 → 신규 MID로 게시 → `mavlink_bridge`가
> FC로 전달(`MAV_CMD_DO_SET_MODE`; WAYPOINT는 AUTO 모드, HOVER는 LOITER/POSHOLD, LAND는
> LAND/RTL). **재개 인덱스는 별도 `MISSION_SET_CURRENT` 명령이 아니라 §18.4.6.2의 current-flag
> 메커니즘(재업로드 트랜잭션 내 `ActiveResumeIndex` 항목에 `current=1`)으로 처리한다**(2026-07-25
> 정정 — PX4가 업로드 트랜잭션 안의 `current=1` 항목 seq를 그대로 재개 인덱스로 채택하므로 별도
> 명령 불필요, BL-56 설계 중 확인). 배선·파싱·단위테스트는 선행 가능하고, 실외 원형비행 실기
> 검증만 하드웨어를 대기한다(BL-44 등록). (구 설계의 reserved 2-pass 플래그 배선은 이 모델에서
> 불필요해 폐기.)

**알려진 제약(추후 해결 과제)**:
- **waypoint 개수 상한은 계속 16개(`ROUTE_MAX_WAYPOINTS=16`, 미션 배열 전체 상한, 캐시 기준),
  단 LoRa 프레임 1개당 담을 수 있는 waypoint 수는 BL-56 구조체 확장(2026-07-25)으로 줄었다**:
  구 `ROUTE_WAYPOINT_t`(X/Y/Z, 12바이트)는 `4 + 16×12 = 196`으로 프레임 페이로드(196바이트)에
  정확히 맞았으나, 신 `ROUTE_WAYPOINT_t`(CmdType+Param1~4+UseGlobal+LatE7+LonE7+X+Y+Z, 38바이트)는
  프레임당 `(196−4)/38 = 5`개가 한도. **16개를 채우려면 지상국(openMCT)이 ADD를 여러 번(예:
  4개씩 4프레임) 나눠 순차 전송**한다 — 세션/누적 대기 상태는 두지 않는다: ADD 프레임 하나가
  도착할 때마다 `mavlink_bridge_app`이 그 시점 캐시 전체를 즉시 PX4로 재업로드하며, 매 단계가
  독립적으로 완결된 유효 미션이다(예: 4개만 반영된 상태에서도 바로 비행 가능, 16개가 다 모여야
  "시작"되는 게이트는 없음). `HOVER`는 웨이포인트 배열 항목이 아니라 별도 비행모드 명령이라
  16개 용량을 잠식하지 않는다. 원형 촬영 밀도 요구량이 16개보다 많은 지점을 필요로 하는지는
  별도 검토가 필요하다.
- `MISSION_ITEM_INT`(INT 업로드 경로)의 frame 호환성 코드 수정은 완료(2026-07-13,
  legacy와 동일하게 GLOBAL_RELATIVE_ALT 전환). 다만 INT 경로 자체의 실물 FC
  검증(`MISSION_ACK` accepted 확인)은 FC 점유로 아직 미실행 — 이 보정 기능의
  업로드도 그 검증 완료 전까지는 완전히 신뢰할 수 없음.

##### 18.4.6.3 viewpoint update

viewpoint payload는 최소한 다음 필드를 포함해야 한다.

| 필드 | 형식 | 의미 | 검증 규칙 |
| --- | --- | --- | --- |
| `viewpoint_type` | `uint8` | absolute, relative, track-point 등 승인된 타입 | 승인된 타입만 허용 |
| `viewpoint_version` | `uint8` | payload 버전 | 현재 지원 버전과 일치 |
| `position_frame` | `uint8` | 좌표 기준 프레임 | 승인된 frame만 허용 |
| `position` | `float x,y,z` 또는 동등한 구조 | 목표 위치 | finite, 영역 제약 충족 |
| `orientation` | yaw/pitch 또는 동등한 구조 | 시점 방향 | 각도 범위 제약 충족 |
| `hold_time_ms` | `uint32` | 유지 시간 | 허용 범위 내 |

출력 계약:

- 유효한 viewpoint는 planner 또는 mission layer가 소비 가능한 내부 viewpoint 구조로 변환되어야 한다.
- route update와 동일하게 raw payload byte array를 직접 재사용해서는 안 된다.

거부 조건:

- 필수 필드 누락
- 좌표 또는 각도 범위 위반
- route planner가 지원하지 않는 `viewpoint_type`
- 현재 cFS 상태와 비호환

##### 18.4.6.4 recovery command

recovery payload는 최소한 다음 필드를 포함해야 한다.

| 필드 | 형식 | 의미 | 검증 규칙 |
| --- | --- | --- | --- |
| `recovery_action` | `uint8` | parser reset, serial reconnect, app restart request 등 | 승인된 action만 허용 |
| `target_component` | `uint8` | 대상 앱 또는 전송 계층 | 승인된 대상만 허용 |
| `reason_code` | `uint8` 또는 `uint16` | 운용자 요청 사유 | 정의된 코드 또는 `0` |
| `request_token` | `uint16` 또는 `uint32` | 요청 상관 식별자 | replay 정책과 양립 가능해야 함 |

출력 계약:

- `uplink_app` 자체 자원에 국한된 action만 로컬에서 처리할 수 있다.
- 그 외 action은 대상 앱 또는 `cfs_core_app`로 전달되어야 한다.
- `UPLINK_STATUS_MID`는 `전달 성공`과 `실행 성공`을 구분할 수 있어야 한다.

거부 조건:

- 승인되지 않은 `recovery_action`
- 대상 컴포넌트 불일치
- 현재 상태에서 금지된 recovery action
- 권한 부족

> **구현 상태 (2026-07-22 갱신, 최초 A-3 구현 2026-07-05)**: 완료.
> `uplink_app`의 `ForwardRecoveryCommand`는 `Payload[0:8]`을
> `RecoveryAction`/`TargetComponent`/`ReasonCode`(u16 LE)/`RequestToken`(u32 LE)으로
> 파싱해 `RECOVERY_CMD_MID(0x190C)`로 전달한다. `cfs_core_app`의
> `ProcessRecoveryCommand`는 `RecoveryAction` 6종별로 분기 — `RESET_COUNTER`는
> 카운터 리셋, `RESTART_BRIDGE`/`RESTART_UPLINK`/`RESTART_LORA`(2026-07-21
> BL-09 추가)는 실제 `CFE_ES_RestartApp()` 호출, **`PARSER_RESET`/
> `SERIAL_RECONNECT`도 2026-07-22(P1-a)에 실제 연결 완료** — `mavlink_bridge_app`의
> 기존 `CMD_MID`(`0x18A0`)를 FcnCode(`PARSER_RESET_CC=3`/`SERIAL_RECONNECT_CC=4`,
> 신규 값, spec엔 이 숫자가 없었음 — 구현 시 확정)로 재사용해
> `ResetParser()`/`CloseSerial()+OpenSerial()`을 실제 호출, 미정의 action은
> EVS 오류. 단위테스트: `A3_unittest_cases.md` A-3.1(5건),
> `notes/temp/a3_unittest_gap_implementation.md` 참조.
>
> **`전달 성공` vs `실행 성공` 구분 (2026-07-22, BL-08 완료)**: 위 §18.4.6.4
> "출력 계약" 요구사항이 실제로 구현됨. `cfs_core_app`/`mavlink_bridge_app`/
> `lora_tdm_app` 3개 대상 앱이 명령 처리를 마치면 공용 `EXEC_RESULT_MID`
> (0x1912, `shared_msgs/exec_result_msg.h`)로 `uplink_app`에 회신 —
> `SourceSequence`로 원본 지상 명령을 상관시키고, `GenericResult`(OK/FAILED)
> + `DetailCode`(대상앱 원시 결과코드, 진단 참고용)를 싣는다. `uplink_app`은
> `UPLINK_APP_RESULT_EXECUTED_OK/FAILED`(15/16)로 `LastCommandResult`를
> 갱신 — 이전엔 `ROUTED`(전달 성공)까지만 있었음. 타임아웃 없음: 응답의
> `SourceSequence`가 `uplink_app`이 추적 중인 최신 수락 seq와 일치할 때만
> 반영하고, 다음 명령이 오면 오래된 응답은 자연히 무시됨. 3개 앱이 각자
> 다른 세부 결과 스키마(2개는 동일한 7종 `CONFIG_RESULT` enum 중복 보유,
> 1개는 없었음)를 갖고 있어 스키마 통일 대신 대분류(GenericResult)로
> 단순화하는 방향으로 설계 — 현재는 CONFIG(3개 앱 전부)와 RECOVERY
> (`cfs_core_app`만, 해당 클래스가 그 앱에만 있으므로)에 배선됨.
> ROUTE_UPDATE/MODE/VIEWPOINT/DIAGNOSTIC은 범위 밖(EXEC_RESULT 미발행,
> `uplink_app` 쪽은 여전히 `ROUTED`까지만 보고) — 후속 작업.
>
> **MID/enum 구체값(2026-07-22, 신규 기재 — spec 원문엔 없던 값)**: MID
> `0x1912`는 기존 표에서 다음 빈 번호로 임의 배정. `EXEC_RESULT_TLM_t.SourceApp`
> 값은 `EXEC_RESULT_SOURCE_CFS_CORE=1`/`MAVLINK_BRIDGE=2`/`LORA_TDM=3` —
> CONFIG_CMD_MID의 기존 `ConfigScope` 값(cfs_core=1/mavlink_bridge=2/lora_tdm=3)과
> 맞춰 배정. `CommandClass` 필드는 uplink_app의 `UPLINK_APP_CLASS_*` 값을
> 그대로 매직넘버로 씀(cfs_core_app 등은 그 헤더를 참조하지 않아서) — CONFIG=1,
> RECOVERY=4.

##### 18.4.6.5 mode command

mode payload는 최소한 다음 필드를 포함해야 한다.

| 필드 | 형식 | 의미 | 검증 규칙 |
| --- | --- | --- | --- |
| `mode_action` | `uint8` | recovery mode enter/exit 등 | 승인된 action만 허용 |
| `requested_state` | `uint8` | 목표 상태 | 승인된 상태 집합에 포함 |
| `request_token` | `uint16` 또는 `uint32` | 상관 식별자 | replay 정책과 양립 가능해야 함 |

출력 계약:

- mode command는 직접 모드 전이를 수행하지 않고, mode authority로 전달되어야 한다.
- `uplink_app`은 허용 여부와 전달 결과만 기록한다.

거부 조건:

- 현재 cFS 상태에서 허용되지 않는 전이
- 권한 부족
- 승인되지 않은 목표 상태

> **구현 상태 (2026-07-14 갱신, 최초 A-3 구현 2026-07-05)**: ~~`Payload[0]` 캐시만~~ 완료.
> `cfs_core_app`의 `ProcessModeCommand`는 `ModeAction`(ENTER/EXIT)×`RequestedState`
> (NORMAL/RECOVERY) 조합을 검증해 허용된 전이(NORMAL→RECOVERY, RECOVERY→NORMAL)만
> `CurrentModeState`를 변경하고, 그 외(동일 상태 재요청·미정의 상태값 등)는 상태
> 불변 + EVS 오류만 발생시킨다. `RequestToken`은 `LastModeRequestToken`에 저장.
> 단위테스트: `A3_unittest_cases.md` A-3.2(4건),
> `notes/temp/a3_unittest_gap_implementation.md` 참조.

##### 18.4.6.6 diagnostic command

diagnostic payload는 최소한 다음 필드를 포함해야 한다.

| 필드 | 형식 | 의미 | 검증 규칙 |
| --- | --- | --- | --- |
| `diag_action` | `uint8` | log level change, capture enable/disable 등 | 승인된 action만 허용 |
| `diag_target` | `uint8` | 대상 앱 또는 하위 모듈 | 승인된 대상만 허용 |
| `diag_value` | 가변 길이 | action 인자 | action별 형식과 범위 충족 |

출력 계약:

- 유효한 diagnostic command는 즉시 실행되거나 대상 앱의 diagnostic interface로 전달되어야 한다.
- 진단 명령은 pending buffer를 요구하지 않는다.

거부 조건:

- 승인되지 않은 action
- payload 형식 불일치
- 현재 상태에서 금지된 진단 동작

> **구현 상태 (2026-07-14 갱신, 최초 A-3 구현 2026-07-05)**: ~~payload 미해석~~ 완료.
> 구독자 `lora_tdm_app`의 `ProcessDiagnosticCommand`는 `DiagAction`을
> LINK_STATUS/RX_STATS/TX_STATS로 분기해 각각 다른 EVS 요약(링크 상태·RX
> 카운터·TX 카운터)을 출력한다. `diag_target`/`diag_value` 세분화 처리는 아직
> 없음(액션 단위 EVS 요약까지가 현재 범위). 단위테스트: `A3_unittest_cases.md`
> A-3.3(4건), `notes/temp/a3_unittest_gap_implementation.md` 참조.

##### 18.4.6.7 counter management (2026-07-22 확정, 구현 전)

**class code**: `7`. 별도 MID 없이 `LORA_TDM_APP_UPLINK_PROCESS_UPLINK_CC` 프레임 내부 command_class로 식별.

counter management payload:

| 필드 | 형식 | 의미 | 검증 규칙 |
| --- | --- | --- | --- |
| `counter_scope` | `uint8` | 대상 범위. `1`=mavlink_bridge, `2`=cfs_core, `3`=uplink, `4`=lora_tdm | 1~4 외 값 거부 |
| `counter_action` | `uint8` | `0`=RESET(고정, reset 전용) | 0 외 값 거부(향후 확장 예약) |
| `request_token` | `uint32` | §18.4.7 표준 request_token 재사용 — 파괴적 동작 확인은 Level 3 공통 규칙(`request_token≠0`)으로 대체, 별도 `confirm_code` 필드 없음 | Level 3 공통 규칙 적용 |

**라우팅**: `cfs_core_app` 경유 왕복(round-trip) 없이 `uplink_app`이 직접 대상 앱으로 전달한다. 근거: `uplink_app`은 이미 `SYSTEM_HEALTH_MID`를 구독해 `CfsHealthState`를 로컬 보유하고 다른 명령 클래스(RECOVERY/MODE 등)와 동일하게 `IsAuthorized`/health-gate로 검수를 자체 수행하므로, Level 3 차단 판정에 `cfs_core_app` 왕복이 architecturally 불필요(cFE SB는 순수 비동기라 동기 왕복 자체가 불가능하기도 함). 검수와 최종 명령 실행 모두 `uplink_app`이 수행 — `cfs_core_app`은 counter management의 라우팅 경로에 관여하지 않는다(다른 클래스와 동일 패턴).

출력 계약:

- 승인된 경우 `uplink_app`이 `counter_scope`가 가리키는 대상 앱에 기존 `RESET_COUNTERS` CC(각 앱이 이미 보유)를 SB로 직접 전송한다. 새 MID/CC를 대상 앱에 추가하지 않는다.
- 결과는 `UPLINK_STATUS_MID`의 UFB에 반영한다 — 신규 코드 `0x0C`(COUNTER_MGMT_REJECTED, scope/action 오류 또는 Level 3 차단) 1종 추가. 정상 처리는 기존 `UFB_OK`(0x00) 재사용.

거부 조건:

- `counter_scope` 1~4 외
- `counter_action` 0 외
- Level 3 공통 차단 규칙(`request_token=0`, 시스템 헬스 DEGRADED/RECOVERY/FAILED 등)
- 최소 보고 시작 상태에서 금지된 범위

> 위 수치(class code 7, scope 1~4, UFB 0x0C)는 spec 원문에 없던 값으로 이번 세션 대화에서 확정. **구현 완료(2026-07-22)**: `uplink_app`에 `UPLINK_APP_CLASS_COUNTER_MGMT=7`/`UPLINK_APP_ForwardCounterMgmtCommand()` 추가 — scope=UPLINK(자신)는 로컬 카운터 직접 초기화, 그 외 3개 앱은 기존 CMD_MID(`0x18A0`/`0x18C0`/`0x18E0`)에 기존 `RESET_COUNTERS_CC=1`을 `CFE_MSG_SetFcnCode`로 얹어 직접 전송(P1-a `CFS_CORE_APP_SendBridgeCtrlCmd`와 동일 패턴, `cfs_core_app` 미경유). `lora_tdm_app` UFB `REJECT_COUNTER=12`(0x0C) 매핑 완료. 단위테스트: uplink_app_utils(120→131), uplink_app_cmds(+4), lora_tdm_app_dispatch(59→61), 전부 통과. **지상측도 완료(2026-07-22, openMCT `92bd3a6`)**: `/api/uplink/counter` 엔드포인트(payload=scope+action+token(LE), Level 3 token 자동생성) + uplinkCLI `counter <scope>` 명령 + uplinkGUI UFB 0x0C 디코딩(`82a8a82`).

##### 18.4.6.8 flight mode command (BL-44, 2026-07-24 설계 확정 — 구현 전)

**목적**: FC 비행모드 제어 base 명령. 2-pass GPS 능동 보정(§18.4.6.2.1)이 이 명령들을 조합해
지상에서 오케스트레이션한다. 기체=헬리콥터, FC 펌웨어=**PX4**(ArduPilot 아님, 2026-07-24 확정).

**class code**: `8` (신규 `UPLINK_APP_CLASS_FLIGHT_MODE`).

flight mode payload:

| 필드 | 형식 | 의미 | 검증 규칙 |
| --- | --- | --- | --- |
| `flight_mode` | `uint8` | `0`=HOVER, `1`=WAYPOINT, `2`=LAND | 0~2 외 값 거부 |
| `waypoint_start_index` | `uint8` | WAYPOINT 전용 — `MISSION_SET_CURRENT` 대상 인덱스. HOVER/LAND는 0 고정 | WAYPOINT 외 값이 0이 아니면 거부 |
| `request_token` | `uint32` | §18.4.7 표준 request_token 재사용(Level 3 공통 규칙) | Level 3 공통 규칙 적용 |

**PX4 모드 매핑** (`MAV_CMD_DO_SET_MODE`=176, 기존 `COMMAND_LONG` 송신 인프라 재사용 —
`MAVLINK_BRIDGE_APP_RequestMessageInterval()`과 동일 패턴, param1=base_mode(`MAV_MODE_FLAG_CUSTOM_MODE_ENABLED`),
param2=custom main mode, param3=custom sub mode):

| flight_mode | PX4 main/sub | 부가 동작 |
| --- | --- | --- |
| HOVER(0) | AUTO(4) / LOITER(3) ("Hold") | 없음 |
| WAYPOINT(1) | AUTO(4) / MISSION(4) | `MISSION_SET_CURRENT`을 `waypoint_start_index`로 별도 전송(모드 전환 뒤) |
| LAND(2) | AUTO(4) / LAND(6) | 없음 |

**인가/게이트 (2026-07-24 대화로 확정)**:
- **auth level**: Level 3 (비행 상태를 직접 바꾸는 안전 민감 명령 — RECOVERY/MODE/COUNTER와 동급, `request_token≠0` 필수).
- **health gate 예외**: HOVER·LAND는 "위험 축소"(새로운 걸 시도하지 않고 현재 상태를 유지/종료) 명령이라 시스템 헬스(DEGRADED/RECOVERY/FAILED)와 **무관하게 항상 허용**한다 — 상태가 나쁠수록 오히려 필요한 명령이므로 게이트가 막으면 안 된다. WAYPOINT는 "위험 증가"(새 경로를 새로 신뢰해 비행) 명령이라 §18.10.1 헬스 게이트를 **정상 적용**한다(다른 상태-의존 명령과 동일).

**라우팅**: counter management(§18.4.6.7)와 동일 패턴 — `cfs_core_app` 경유 없이 `uplink_app`이
검수(Level 3 + health-gate 예외 판단) 후 `mavlink_bridge_app`의 기존 `CMD_MID`(`0x18A0`)에
신규 FcnCode(`SET_FLIGHT_MODE_CC`)를 얹어 직접 전송한다. `mavlink_bridge_app`이 수신해 위
매핑대로 `COMMAND_LONG`(+WAYPOINT는 `MISSION_SET_CURRENT`)을 FC로 전송.

출력 계약:

- 결과는 `UPLINK_STATUS_MID`의 UFB에 반영 — 신규 코드 1종(`REJECT_FLIGHT_MODE`, class/health 무관하게 flight_mode 값 자체가 잘못된 경우) 추가 필요. 정상 처리는 기존 `EXECUTED_OK`/`EXECUTED_FAILED`(BL-08 EXEC_RESULT_MID) 재사용.

거부 조건:

- `flight_mode` 0~2 외
- `waypoint_start_index`가 WAYPOINT 외 모드에서 0이 아님
- Level 3 공통 차단 규칙(`request_token=0`)
- WAYPOINT에 한해 시스템 헬스 DEGRADED/RECOVERY/FAILED(HOVER/LAND는 이 규칙 면제)

**구현 현황(2026-07-24)**: uplink_app 파싱/포워딩 슬라이스 완료(SDD→TDD, 신규 10종 green).
mavlink_bridge_app 슬라이스는 아래 wire-level 계약까지 확정 후 착수.

**force flag(§18.10.2 `UPLINK_APP_FORCE_FLAG`) 지상국 노출**: 기체측 게이트는 이미 범용
force 비트를 지원하나(`uplink_app_cmds.c` health-gate 예외 처리), 지상국
`fc_serial_ws_server.py`/GUI는 CONFIG에만 force 체크박스를 연결해뒀고 FLIGHT_MODE에는
연결이 빠져 있어 실기 GUI 테스트(RT-FLIGHTMODE) 중 발견됨 — WAYPOINT가 DEGRADED에서
막혔는데 우회 수단이 없었음. `/api/uplink/flight_mode` 라우트와 GUI Flight Mode 패널에
force 체크박스 추가로 해결(openMCT, 2026-07-24).

##### 18.4.6.8.1 mavlink_bridge_app 구현 계약 (wire-level, 2026-07-24 확정)

**SB 명령 구조 정정 — SourceSequence 누락 보완**: uplink_app 슬라이스 구현 중 발견된 설계
공백 — `EXEC_RESULT_MID` 회신(`MAVLINK_BRIDGE_APP_PublishExecResult`)은 원본 지상 명령의
`SourceSequence`를 echo해야 하는데(다른 모든 CMD_TLM 구조가 이 필드를 가짐, 예:
`ConfigCmdTlm_t.SourceSequence`), 최초 `UPLINK_APP_FlightModeCtrlCmd_t`엔 이 필드가 없었다.
**정정**: 구조를 아래로 확장(uplink_app/mavlink_bridge_app 양쪽 동일 레이아웃 유지 필요).

```c
typedef struct
{
    CFE_MSG_CommandHeader_t CommandHeader;
    uint16                  SourceSequence;      /* 원본 지상 명령 seq — EXEC_RESULT echo용 */
    uint8                   FlightMode;
    uint8                   WaypointStartIndex;
} UPLINK_APP_FlightModeCtrlCmd_t;  /* mavlink_bridge_app은 동일 레이아웃을 자체 typedef */
```

`UPLINK_APP_ForwardFlightModeCommand()`는 `Cmd->Sequence`를 `SourceSequence`에 채운다.

**PX4 커스텀 모드 정수값** (MAVLink `base_mode`/`custom_mode` 필드, PX4 정의):

| 이름 | 값 |
| --- | --- |
| `PX4_MAIN_MODE_AUTO` | `4` |
| `PX4_SUB_MODE_AUTO_LOITER` | `3` |
| `PX4_SUB_MODE_AUTO_MISSION` | `4` |
| `PX4_SUB_MODE_AUTO_LAND` | `6` |

`custom_mode`(u32)는 PX4 인코딩상 `(main_mode << 16) | (sub_mode << 24)`이다 — main mode 바이트는
custom_mode의 2번째 바이트(bit 16~23), sub mode는 3번째 바이트(bit 24~31)에 위치한다(PX4
`px4_custom_mode` 유니온 정의 기준). 예: AUTO/LOITER → `custom_mode = (4 << 16) | (3 << 24)`.

**`MAV_CMD_DO_SET_MODE`(176) — 기존 `COMMAND_LONG` 인프라 재사용**
(`MAVLINK_BRIDGE_APP_RequestMessageInterval()`과 동일 33바이트 프레임 구조,
`MAVLINK_MSG_ID_COMMAND_LONG_LEN`/`MAVLINK_COMMAND_LONG_CRC_EXTRA` 그대로 사용):

| offset | 필드 | 값 |
| --- | --- | --- |
| 0 | param1 (float) | `base_mode` = `1.0f` (`MAV_MODE_FLAG_CUSTOM_MODE_ENABLED`) |
| 4 | param2 (float) | `custom_mode` — 위 인코딩값을 float로 재해석(bit pattern 그대로, `(float)(uint32)` 아님 — MAVLink 관례상 COMMAND_LONG의 mode 파라미터는 정수를 float 필드에 **bit-cast**하지 않고 **수치 그대로 float 변환**한다: `(float)custom_mode_uint32`) |
| 8~24 | param3~7 (float) | `0.0f` |
| 28 | command (u16) | `176`(`MAV_CMD_DO_SET_MODE`) |
| 30 | target_system (u8) | `MAVLINK_BRIDGE_APP_Data.TargetSystemId` |
| 31 | target_component (u8) | `MAVLINK_BRIDGE_APP_Data.TargetComponentId` |
| 32 | confirmation (u8) | `0` |

**`MISSION_SET_CURRENT`(신규 프레임, WAYPOINT 전용, 모드 전환 뒤 별도 전송)**:

| offset | 필드 | 형식 | 값 |
| --- | --- | --- | --- |
| 0 | seq | u16 | `waypoint_start_index` (0-base, 0도 유효 — "처음부터") |
| 2 | target_system | u8 | `MAVLINK_BRIDGE_APP_Data.TargetSystemId` |
| 3 | target_component | u8 | `MAVLINK_BRIDGE_APP_Data.TargetComponentId` |

msg id `41`, 길이 4바이트, `CRC_EXTRA=28`(MAVLink common.xml 고정값 — 신규 상수
`MAVLINK_MISSION_SET_CURRENT_CRC_EXTRA`로 추가). `MAVLINK_BRIDGE_APP_SendMavlinkV2()` 재사용.

**결과 판정 범위(명시 — 2026-07-24 결정)**: `EXECUTED_OK`는 **FC가 실제로 모드를 전환했음을
확인한 것이 아니라, COMMAND_LONG(+WAYPOINT는 MISSION_SET_CURRENT까지) 전송 자체가
성공했음**을 의미한다. 기존 `RequestMessageInterval`/`RESTART_BRIDGE` 등 다른 fire-and-forget
명령과 동일한 신뢰 수준(§18.4.7 request_token 계약과 별개로, FC `COMMAND_ACK`(77) 대기는
범위 밖 — 필요해지면 별도 항목으로 재검토). WAYPOINT는 두 전송(모드 전환 + MISSION_SET_CURRENT)
중 하나라도 실패하면 `EXECUTED_FAILED`.

**신규 EID**: `MAVLINK_BRIDGE_APP_SET_FLIGHT_MODE_EID` — 성공/실패 각각 INFORMATION/ERROR로 발생.

**핸들러 시그니처**: `void MAVLINK_BRIDGE_APP_ProcessSetFlightModeCmd(const MAVLINK_BRIDGE_APP_SetFlightModeCmd_t *Cmd)`
— `PARSER_RESET`과 달리 payload가 있고 `PublishExecResult` 호출까지 수행하는 첫 사례(P1-a
payload-less 패턴의 확장).

실외 원형비행 실기 검증은 하드웨어 대기(BL-44 등록).

#### 18.4.7 Request Token 계약

**목적**: `request_token` 필드의 역할, 수명 주기, 지상국-탑재 앱 간 계약을 명확히 하여 인증·재설계 단계에서의 혼동을 방지한다.

**적용 범위**: recovery command, mode command, counter management, flight mode command(BL-44,
§18.4.6.8) 페이로드에 포함된 `request_token` uint32 필드.

##### 기본 역할

`request_token`은 지상국이 생성한 불투명한 요청-응답 상관 식별자이다.

- **생성자**: 지상국(Ground Station) — 명령 전송 시 임의의 uint32 값 선택
- **전송**: uplink 명령 페이로드(RECOVERY/MODE/DIAGNOSTIC payload)에 포함
- **탑재 앱 처리**: 값을 해석·검증하지 않으며, 그대로 저장
- **응답**: 명령 결과를 게시하는 downlink 텔레메트리(`RecoveryCmdTlm_t`, `ModeCmdTlm_t`, `DiagnosticCmdTlm_t`)에 그대로 echo
- **지상국 수신**: downlink 텔레메트리의 echo token과 자신이 전송한 token을 비교하여 "이 결과가 내 명령에 대한 응답인가"를 확인

##### 미사용 값 약정

- `request_token = 0`은 "추적 불필요(fire-and-forget)" 의미로 예약한다. 지상국은 0을 명령에 담을 수 있지만, 별도의 echo 확인 메커니즘 없이 단순 전송만 수행한다.
- 추적이 필요한 명령은 항상 0이 아닌 값을 사용한다.

##### Replay 방어와의 관계

`request_token`과 **command sequence** (§17.10)는 상보적이지만 **역할이 다르다**:

- **Command Sequence** (§17.10): 탑재 앱이 관리하는 단조 증가 카운터. 중복 또는 감소한 sequence를 가진 명령은 `uplink_app`이 frame 검증 단계에서 거부한다. 역할: **replay 공격 방어**.
- **Request Token** (본 섹션): 지상국이 관리하는 추적용 opaque 값. 탑재 앱은 검증하지 않으며, downlink echo만 제공한다. 역할: **요청-응답 상관성 확인**.

replay 방어는 sequence에 전적으로 의존하며, token 값 유무와 무관하다. token을 이용한 별도의 anti-replay 메커니즘은 구현하지 않는다.

##### 인증·서명과의 관계

`request_token`은 인증 메커니즘과 무관하다:

- 본 token 필드는 지상국이 임의로 설정할 수 있으며, 탑재 앱이 검증하지 않으므로 암호학적 보증이 불가능하다.
- 인증(Authenticity/Authorization)이 필요한 경우 Phase 3.2(권한 검증)에서 별도의 프레임 헤더 Flags 또는 보안 필드를 사용한다.
- payload 내부 per-command token에 HMAC 서명을 포함시키지 않는다. 이는 명령 클래스마다 인증 로직을 분산시키고, 일부 명령(예: ROUTE_UPDATE)은 인증 대상에서 벗어나는 일관성 문제를 야기한다.

##### 구현 요구사항

1. **Downlink 텔레메트리에 echo 필드 포함** — RecoveryCmdTlm_t, ModeCmdTlm_t, DiagnosticCmdTlm_t의 RequestToken 필드에 수신한 값을 그대로 할당.
2. **탑재 앱은 token 값에 따라 동작을 변경하지 않음** — 예: "token이 0이면 명령 거부" 같은 로직 금지. token은 메타데이터일 뿐.
3. **지상국 통신 계층에서 echo 확인** — downlink 수신 시 echo token과 전송 token을 비교하여 상관성 판정. 이는 지상국 CI의 책임.

---

#### 18.4.8 명령 클래스별 라우팅 대상

각 명령 클래스는 최소한 다음 라우팅 기본값을 가져야 한다.

| 명령 클래스 | 기본 라우팅 대상 | 비고 |
| --- | --- | --- |
| runtime configuration | 대상 앱 설정 인터페이스 또는 `cfs_core_app` | active 적용은 대상 앱 경계에서 수행 |
| route update | mission layer route consumer | 현재 구현은 내부 route update message 사용 가능 |
| viewpoint update | planner 또는 mission layer viewpoint consumer | route update와 분리된 target 유지 |
| recovery command | 대상 앱 또는 `cfs_core_app` | action별 대상 허용 집합 필요 |
| mode command | `cfs_core_app` 또는 mode authority | 직접 상태 전이 금지 |
| diagnostic command | 대상 앱 diagnostic interface | 즉시 실행 가능 |
| counter management | `uplink_app` → 대상 앱 직접 전송(§18.4.6.7) | `cfs_core_app` 왕복 없음, destructive action 승인 필요 |

구현은 `command_class`만으로 모든 라우팅을 고정해서는 안 되며, 필요한 경우 `target_id` 또는 payload 내부 대상 식별자를 함께 사용해야 한다.

#### 18.4.9 명령 클래스별 최소 테스트 케이스

각 명령 클래스는 최소한 다음 테스트 케이스를 가져야 한다.

| 명령 클래스 | 필수 정상 케이스 | 필수 오류 케이스 |
| --- | --- | --- |
| runtime configuration | 범위 내 설정 1건 수락 및 pending 반영 | 범위 오류, 버전 오류, CRC 오류, 대상 상태 비호환 |
| route update | 유효한 `mission_extension`, 유효한 `landing` | 길이 오류, altitude 오류, distance 오류, replay |
| viewpoint update | 유효한 viewpoint 1건 수락 | 좌표 오류, orientation 오류, 타입 미지원 |
| recovery command | 승인된 parser reset 또는 restart request 전달 | 권한 부족, 대상 불일치, 상태 비호환 |
| mode command | 허용 상태 전이 요청 전달 | 금지된 상태 전이, 권한 부족 |
| diagnostic command | 승인된 log level 또는 capture toggle | action 미지원, payload 형식 오류 |
| counter management | 승인된 counter reset 전달 | 확인 코드 오류, 권한 부족 |

이 테스트 케이스는 unit test 또는 mock transport runtime test 중 하나 이상으로 검증되어야 하며, 파괴적이거나 상태 의존적인 케이스는 runtime test 전에 명시적으로 격리되어야 한다.

### 18.5 구성 명령 처리

런타임 구성 명령은 다음에 정의된 활성/보류 모델을 따라야 합니다.
섹션 13.

처리 단계:

1. 업링크 패킷을 수신하고 검증합니다.
2. 검증된 구성 페이로드를 `pending_config` 버퍼에 씁니다.
   대상 앱 또는 명령이 대상인 경우 `uplink_app`-로컬 보류 버퍼로
   `uplink_app` 그 자체.
3. 범위, 버전, 체크섬 또는 CRC, 현재 모드 호환성을 검증합니다.
4. 유효성 검사가 통과되면 보류 중인 상태를 다음 번에 활성 상태로 바꿔 활성화합니다.
   대상 앱에 허용되는 활성화 경계입니다.
5. 검증에 실패하면 보류 중인 구성을 삭제하고 활성 구성을 유지하며 보고합니다.
   `UPLINK_STATUS_MID`을(를) 통해 거부됩니다.
6. 활성화로 인해 런타임 오류가 발생하는 경우 `previous_config`로 롤백하고
   `UPLINK_STATUS_MID`을(를) 통해 롤백을 보고합니다.

대상 앱별 활성화 경계는 섹션 13에 정의되어 있습니다.

#### 18.5.1 구현 방향

- `uplink_app`은 활성 설정을 직접 덮어쓰지 않아야 한다.
- 검증된 설정은 항상 pending buffer에 먼저 기록되어야 한다.
- 대상 앱이 자체 pending/active 모델을 갖는 경우, `uplink_app`은 해당 앱의 명령 MID 또는 설정 인터페이스를 통해 전달만 수행한다.
- `uplink_app` 자체 설정이 필요한 경우에만 로컬 pending/active buffer를 유지한다.
- 설정 반영 결과는 성공, 거부, 롤백 중 하나로 명시적으로 기록되어야 한다.

즉 `uplink_app`의 역할은 **설정 적용 수행**보다는 **설정 검증 및 안전한 반영 절차의 시작점**으로 제한한다.

### 18.5.2 경로 및 viewpoint 명령 처리

경로 수정 및 viewpoint 명령은 runtime configuration과 별도 경로로 처리해야 한다. `route update` 명령은 경로 연산 타입(`route_op`)에 따라 **전체 대체(REPLACE)**, **끝에 추가(APPEND)**, **끝에서 일부 제거(DELETE)** 중 하나로 처리된다. 세 연산 모두 `mavlink_bridge_app`을 통해 FC에 업로드되며, active waypoint cache는 업로드 성공 확인 시에만 갱신된다.

처리 단계:

1. 경로 수정 또는 viewpoint payload를 수신한다.
2. payload 형식, 필수 필드, 값 범위, 경로 길이, 좌표 유효 범위를 검증한다.
3. 기존 경로 수정 명령인지, viewpoint 입력 명령인지 구분한다.
4. 유효한 payload만 상위 임무 계층이 사용할 수 있는 내부 표현으로 변환한다.
5. 변환된 결과를 대상 소비자에게 전달하고, 처리 결과를 `UPLINK_STATUS_MID`에 반영한다.

좌표 유효 검증은 수신된 추가 경로가 사전에 정의된 비행 가능 영역 안에 존재하는지, 위험 구역 또는 비행 금지 구역을 침범하지 않는지, 그리고 모든 waypoint의 고도가 최소 2m 이상 최대 8m 이하인지 확인하는 과정이다. 또한 추가 경로는 waypoint 개수 제한과 인접 waypoint 간 허용 거리 범위를 만족해야 하며, 이러한 조건을 충족하지 못하는 경로는 거부되어야 한다.

본 문서의 route update 기준 수치는 Section 18.4.6.2와 동일하게 고정한다.

- waypoint 개수: `1..16`
- 인접 waypoint 간 3D 거리: `2m..2m`

상위 임무 계층에는 검증된 route segment 정보가 전달되어야 하며, 여기에는 최소한 `route_type`, `route_version`, `waypoint_count`, 그리고 waypoint 배열이 포함되어야 한다. 이 내부 표현은 raw uplink payload를 그대로 재사용하는 것이 아니라, 경로 관리 계층이 직접 사용할 수 있는 검증된 route segment 구조로 변환되어야 한다.

경로 수정 및 viewpoint 명령 처리에서 `uplink_app`은 다음을 수행하지 않는다.

- FC 비행 모드 직접 변경
- FC 내부 mission upload 직접 실행
- FC safety gate 우회

즉 `uplink_app`은 경로 수정 명령에 한해 **상위 임무 계층용 경로 정보 갱신**을 담당하며, 이를 제외한 FC 직접 제어는 수행하지 않는다.

### 18.6 복구 명령 처리

복구 명령은 대상 앱이나 `cfs_core_app`로 라우팅되어야 합니다.
복구 권한. `uplink_app`은(는) 복구 작업을 직접 실행하지 않습니다.
작업이 `uplink_app` 자체에 국한되지 않는 한(예: 파서 재설정 또는
업링크 전송의 직렬 재연결).

복구 명령 라우팅:

| 복구 명령 | 라우팅 대상 | 승인 필요 |
| --- | --- | --- |
| 파서 재설정 | 대상 앱 또는 브리지 구성 요소 | 운영자 명령 |
| 직렬 재연결 | 브리지 구성 요소 또는 전송 계층 | 운영자 명령 |
| 카운터 재설정 | `cfs_core_app` 또는 대상 앱 | 명시적 명령 승인 |
| 앱 재시작 요청 | `cfs_core_app` | 모드 확인이 포함된 운영자 명령 |
| 복구 모드 진입 | `cfs_core_app` | 모드 확인이 포함된 운영자 명령 |
| 복구 모드 종료 | `cfs_core_app` | 모드 확인이 포함된 운영자 명령 |

복구 모드 진입 및 종료 명령은 현재 상태인 경우에만 허용됩니다.
cFS 상태 및 운영자 인증 수준은 전환을 허용합니다. `uplink_app`
모드 명령을 전달하기 전에 현재 cFS 상태를 확인하고
전환이 허용되지 않으면 명령을 거부하십시오.

#### 18.6.1 구현 방향

- `uplink_app`은 복구 명령을 **직접 실행하는 앱**이 아니라 **복구 명령을 검증하고 전달하는 앱**으로 구현해야 한다.
- 예외적으로 uplink 전송 계층 자체의 파서 재설정이나 직렬 재연결처럼 `uplink_app` 내부 자원에 국한된 명령만 로컬에서 처리할 수 있다.
- 그 외 복구 명령은 대상 앱 또는 `cfs_core_app`에 전달하고, 실제 실행 성공 여부는 대상 앱 또는 복구 권한 주체가 결정한다.
- `uplink_app`은 명령 전달 성공과 실제 복구 성공을 구분해서 상태를 기록해야 한다.

예를 들어 `app restart request`는 `uplink_app`이 재시작을 직접 수행하는 것이 아니라, `cfs_core_app`에 재시작 요청을 전달하고 그 요청의 수락/거부/실패 결과를 기록하는 방식이어야 한다.

### 18.7 `UPLINK_STATUS_MID` 페이로드

`uplink_app`은(는) 정의된 HK 주기로 `UPLINK_STATUS_MID`을(를) 게시해야 합니다.

최소 필드:

- 타임스탬프 및 시간 유효성.
- 시퀀스 카운터.
- 유효한 플래그 및 상태입니다.
- 마지막으로 수신된 명령 코드입니다.
- 마지막으로 수신된 명령 시퀀스 번호입니다.
- 마지막 명령 결과: 수락, 거부, 라우팅 또는 실패.
- 마지막 거부 오류 코드입니다.
- 허용된 명령 수입니다.
- 거부된 명령 수입니다.
- 라우팅 실패 횟수.
- 활성 전송 ID입니다.
- 업링크 링크 상태: `NOMINAL`, `DEGRADED`, `LOST` 또는 `FAILED`.
- 마지막으로 유효한 업링크 수신 시각.
- 구성 보류 중 상태: 유휴, 보류 중, 검증 중 또는 거부됨.
- 마지막 구성 활성화 결과.
- 해당되는 경우 마지막 롤백 이유입니다.
- FC MISSION_ACK 결과(`FcMissionResult`): `mavlink_bridge_app`의 `BRIDGE_HK_MID`
  `LastUploadResult`를 캐시한 값. uplink_app 자체 검증(수신 성공)과
  FC의 실제 accept/reject를 지상국이 구분할 수 있도록 별도 필드로 노출한다
  (2026-07-15, openMCT 피드백 갭 해결).
- FC 업로드 상태(`FcMissionUploadState`): `BRIDGE_HK_MID` 최신 수신 여부 기반
  0=IDLE(미수신) / 1=ACTIVE(수신됨).
- FC 누적 성공 업로드 수(`FcMissionUploadSuccessCount`): `BRIDGE_HK_MID`의
  `MissionUploadSuccessCount`를 캐시한 값.

기준 publish rate:

- `UPLINK_STATUS_MID`: 1 Hz periodic publish를 기본값으로 한다.
- 명령 수락/거부, replay reject, routing failure, transport state transition 발생 시 추가 event-driven publish를 허용한다.

### 18.8 업링크 전송 경계

`uplink_app`은(는) 승인된 호스트로부터 전달된 표준 uplink envelope을 소비하는 역할을 담당한다. transport-specific 입력은 LoRa 직렬, 지상국 무선 링크, 또는 다른 승인된 채널일 수 있으나, Section 18.4.4의 transport 계층이 이를 표준 envelope으로 변환한 뒤 `uplink_app`에 전달해야 한다.

> **[2026-07-05 현재 구현 상태]** (구 2026-06-14 서술 대체 — `lora_tdm_app` 배포 전환 반영)
> - **활성 경로**: `지상국 LoRa → RF → Pi LoRa serial → lora_tdm_app (TDM RX 300ms 창) → UPLINK_APP_CMD_MID(0x18D0, PROCESS_UPLINK_CC=2) SB 전달 → uplink_app`
> - `lora_tdm_app`이 transport 계층 역할(serial 독점 소유, framing/CRC16 검증)을 담당 (2026-06-16 배포 전환, `lora_uplink_bridge.py` 대체).
> - 테스트용 UDP 경로(`UDP:1234 → CI_LAB → uplink_app`)는 병행 유지.
> - `uplink_app`의 `ServiceLoRa()` 직접 serial 경로는 **코드에서 제거됨**. 레거시 `UPLINK_APP_LORA_RAW_MID(0x1909)` 구독·`ParseLoRaFrame()`도 **2026-07-14 코드에서 제거됨** (발행자 없는 죽은 경로였고, `mavlink_bridge_app`의 `FC_SYS_TIME_MID`와 MID 번호가 충돌하고 있었음).

`uplink_app` 운송 책임:

- transport 계층이 보고한 업링크 연결 상태를 소비하고 상태 보고에 반영한다.
- 전달된 표준 envelope의 version, class, replay, payload semantic validation을 수행한다.
- transport 계층 또는 bridge 구성요소에 parser reset 또는 reconnect 요청을 전달할 수 있다.
- `UPLINK_STATUS_MID`을(를) 통해 전송 상태를 보고합니다.

`uplink_app`은 다운링크 텔레메트리 경로에 대한 최종 링크 상태를 분류하지 않습니다.
downlink 텔레메트리 상태는 `lora_tdm_app`의 책임이며
`cfs_core_app`.

### 18.9 오류 및 복구 동작

| 결함 상태 | `uplink_app` 행동 |
| --- | --- |
| 잘못된 업링크 패킷 | 패킷을 폐기합니다. 거부된 횟수를 증가시킵니다. 오류 코드 보고 |
| 복제 또는 재생된 패킷 | 패킷을 폐기합니다. 거부 횟수 증가 |
| 전송 시간 초과 또는 연결 끊김 | 다시 연결을 시도하십시오. 성능이 저하되거나 손실된 상태로 `UPLINK_STATUS_MID` 게시 |
| 대상 앱으로의 라우팅 실패 | 라우팅 실패를 보고합니다. 자동으로 재시도하지 마세요 |
| 구성 검증 실패 | 활성 구성을 유지합니다. 거부 신고 |
| 구성 활성화 오류 | 이전 구성으로 롤백합니다. 보고서 롤백 |
| 승인되지 않은 명령 클래스 | 거부하고 보고합니다. 전달하지 마세요 |

`uplink_app`은 격리된 업링크 패킷에 대해 Pi/cFS 호스트 재설정을 요청하지 않습니다.
오류 또는 전송 시간 초과. `cfs_core_app`(으)로의 에스컬레이션은 다음을 따릅니다.
섹션 11의 단계적 복구 정책.

`uplink_app`에 대한 복구 제한:

| 결함 상태 | 첫 번째 복구 | 재시도 간격 | 최대 재시도 횟수 | 단계적 확대 |
| --- | --- | --- | --- | --- |
| 전송 시간 초과 또는 연결 끊김 | 전송 끝점 다시 열기 | 5초 | 3 | 업링크를 `LOST`로 표시; 복구 권한 평가 요청 |
| 반복되는 라우팅 실패 | `cfs_core_app`에 보고 | 장애 이벤트당 | 3 | 라우팅 경로를 사용할 수 없는 경우 `CFS_DEGRADED`로 전이 |

이 표는 Section 11.1의 시스템 수준 복구 한계를 `uplink_app`에 적용한 앱별 세부 규칙이다. `uplink_app` 전용 제한이 Section 11의 상위 규칙과 충돌할 경우, Section 11의 recovery authority 정책을 우선하고 본 표는 해당 앱의 로컬 선행 동작을 정의하는 것으로 해석한다.

### 18.10 상태별 운영 정책

| cFS 상태 | `uplink_app` 행동 |
| --- | --- |
| `CFS_NOMINAL` | 승인된 모든 명령 클래스를 수락하고 라우팅한다. |
| `CFS_DEGRADED` | 명령을 수락하고 전달하되, 정상 상태가 필요한 명령은 거부한다. |
| `CFS_RECOVERY` | 진단 명령과 복구 명령만 허용하고, 설정 변경 및 모드 관련 명령은 차단한다. |
| `CFS_FAILED` | 진단 명령과 복구 명령만 허용한다. **지상 개입(복구 명령) 경로는 어떤 헬스 상태에서도 차단하지 않는다.** (2026-07-05 정책 확정 — FAILED 고착 데드락 해소) |
| 최소 보고 시작 | 카운터 재설정 및 진단 명령만 허용하며, 그 외 명령 클래스는 차단한다. |

#### 18.10.1 구현된 health-block 매트릭스 (코드 기준, 2026-07-05)

`uplink_app`은 `SYSTEM_HEALTH_MID`를 구독해 `CfsHealthState`를 캐시하고, 라우팅 직전 다음 정책으로 차단한다 (`uplink_app_cmds.c`).

| HealthState | 차단 정책 |
| --- | --- |
| `NOMINAL(0)` | 전 클래스 허용 |
| `DEGRADED(1)` | `VIEWPOINT`, `CONFIG` 차단. `ROUTE_UPDATE` 등 나머지 클래스 허용 |
| `RECOVERY(2)` | `RECOVERY`, `DIAGNOSTIC` 허용. 나머지 차단 |
| `FAILED(3)` | `RECOVERY`, `DIAGNOSTIC` 허용. 나머지 차단 |
| **health 미수신** | **부팅 fail-safe: 첫 `SYSTEM_HEALTH_MID` 수신 전 모든 클래스 차단** |

차단 시 `REJECT_STATE` 결과, `RejectedCount` 증가, `UPLINK_APP_STATE_BLOCK_EID` EVS 발생.

> **✅ 정책 확정 및 코드 반영 완료 (2026-07-05 Phase 3.1)**:
> - `RECOVERY(2)`/`FAILED(3)` 상태에서 **RECOVERY class(4)와 DIAGNOSTIC class(6)를 허용**하도록 변경하여 데드락 해소. 이제 bridge 타임아웃으로 FAILED에 고착되어도 지상국 RECOVERY 명령이 통과.
> - **부팅 fail-safe 추가**: health 미수신 상태(부팅 직후)에 모든 명령을 차단. `CfsHealthReceived=false`일 때는 `SYSTEM_HEALTH_MID` 수신 대기.
> - 코드 변경: `uplink_app_cmds.c` (라인 94 이전 조건 반전 + fail-safe 블록 추가), EVS 메시지 분리 (health 미수신 vs 상태 기반 차단).
> - 단위 테스트 (미실행, build-ut 환경 부재): `FailSafeBootBlocksAll`, `AllowedRecoveryDiagnosticInFailed` 계열.

#### 18.10.2 벤치 테스트용 FORCE 플래그 (설계, 2026-07-13 — 코드 미착수)

**배경**: GPS 없는 실내 벤치 환경에서는 `EKF_STATUS_REPORT`의 flags가 0(unhealthy)으로
남아 `cfs_core_app`의 `EkfState.Valid`가 계속 false → `EkfTimedOut=true` → health가
구조적으로 `NOMINAL`에 도달하지 못한다. §18.10.1 정책상 `DEGRADED`에서 `CONFIG` 클래스가
차단되므로, DL2 프로토콜(§lora_protocol_v2_spec.md §8 `downlink_protocol` 전환 등)을
실내에서 검증할 방법이 없다.

**설계**: UP 프레임의 기존 `flags` 필드(현재 항상 0, 예약)의 **비트0**을
`UPLINK_APP_FORCE_FLAG(0x01)`로 정의한다. `uplink_app_cmds.c`의 §18.10.1 차단 판정 직후,
`Blocked==true`이면서 `Cmd->Flags & UPLINK_APP_FORCE_FLAG`가 설정된 경우에 한해 그 명령
**하나만** 차단을 우회한다.

**안전 설계 근거**:
- 컴파일타임 상수(항상 켜짐/꺼짐)가 아니라 **매 명령마다 무선으로 실제 전송되는 값** —
  기본값이 위험한 상태로 빌드/배포될 여지가 없음
- 지상에서 매번 명시적으로 세워야 하고, 우회될 때마다 `UPLINK_APP_STATE_BLOCK_EID`를
  INFORMATION 레벨로 발생시켜 항상 로그에 흔적이 남음(§18.10.1 표의 REJECT_STATE 로그와
  대비되는 별도 문구 "FORCED THROUGH"로 구분)
- 새 명령 클래스나 새 MID 구독을 추가하지 않음 — 기존 wire format의 예약 필드만 사용

**범위**: 벤치/지상 테스트 목적. 실비행 운용 절차에는 포함하지 않는다(§18.10.1의
FAILED/RECOVERY 상태 차단은 실비행 안전상 존재 이유가 있음 — 이 플래그로 그 의도를
무시하고 상시 사용하지 않을 것).

**구현 완료 (2026-07-13)**: 지상(openMCT `fc_serial_ws_server.py`) `/api/uplink/config`에
`force`(bool) 파라미터 추가, GUI에 "force" 체크박스 추가(경고 문구 포함, 기본 꺼짐).
`uplink_app_cmds.c`에 `Cmd->Flags & UPLINK_APP_FORCE_FLAG` 체크 및 UT(음성 대조 1종) 추가.
실기체 배포 후 실측: **health gate(§18.10.1)는 정상 우회 확인**("FORCED THROUGH" 로그).

#### 18.10.3 실측 중 발견 — §18.11.1 권한 레벨이 지상에서 전혀 전송되지 않고 있었음 (2026-07-13)

**증상**: FORCE_FLAG로 §18.10.1 health gate는 통과했으나, 곧바로 §18.11.1 권한검증에서
재차단됨 (`UPLINK_APP: command blocked (insufficient auth) auth=0 required=2`).

**원인**: `flags` 바이트는 **독립된 두 안전장치가 서로 다른 비트를 씀**:
- bit0 = `UPLINK_APP_FORCE_FLAG`(§18.10.2, health gate 우회, 오늘 구현)
- bit[7:6] = 권한 레벨(§18.11.1, `UPLINK_APP_IsAuthorized`가 검사, 이전부터 있던 요구사항)

지상 코드는 지금까지 `flags` 파라미터 자체를 항상 0으로만 호출해왔다(`_build_lora_frame`
호출부 어디에도 bit[7:6] 설정 코드가 없었음) — 즉 **CONFIG류 명령은 health가 NOMINAL이었던
과거에도 권한검증에서 막혔을 가능성**이 있다(이번 세션 전엔 CONFIG 클래스를 지상에서
실제로 쏴본 적이 없어서 발견되지 않았던 것으로 추정).

**설계(적용 예정)**: FORCE 옵션을 체크하면, health gate 우회(bit0)와 **더불어** 해당
명령 클래스가 요구하는 권한 레벨(bit[7:6])도 같이 채워서 보낸다 — 기체측
`UPLINK_APP_GetClassRequiredLevel()`에 이미 정의된 값을 그대로 미러링:

| 명령 클래스 | 요구 레벨 |
| --- | --- |
| CONFIG(1) | 2 |
| ROUTE_UPDATE(2) | 2 |
| RECOVERY(4) | 3 |

이건 **새 권한을 부여하는 게 아니라, 기체가 원래 요구하던 값을 지상이 이제야 채워
보내는 것**이다 — 권한 레벨 자체를 낮추거나 검증 로직을 바꾸지 않는다. FORCE 체크
없이 보낼 때도 이 레벨은 항상 채워야 정상 동작한다(별도 버그 수정, health gate 우회와
무관하게 필요).

**~~미해결~~ 해결 확인(2026-07-22 재검증)**: `UPLINK_APP_GetClassRequiredLevel()`의
case 불일치는 이미 수정돼 있음 — `uplink_app_cmds.c:37-56`이 raw 숫자 대신
`UPLINK_APP_CLASS_*` enum 이름을 직접 사용하도록 재작성됨(NONE=1/CONFIG=2/
ROUTE_UPDATE=2/VIEWPOINT=2/RECOVERY=3/MODE=3/DIAGNOSTIC=1, §18.10.4의
MODE/DIAGNOSTIC 스왑 버그 해소 이력이 함수 주석에 기재됨). 이 문단의 이전
"미해결" 표기는 낡은 것이었음.

#### 18.10.4 UT 실측으로 확인된 DIAGNOSTIC 인증 영구 실패 — §18.10.3 미해결 항목의 실제 파급 (2026-07-14)

**계기**: `uplink_app_cmds` 단위테스트(spec에 "미실행"으로 기록돼 있던 것)를 처음
로컬에서 돌려본 결과 86개 중 44개 FAIL. 원인은 두 가지로 분리됨.

**요인 A — 테스트 픽스처의 `Flags` 미설정 (테스트만의 문제, 프로덕션 무관)**

거의 모든 "성공(ROUTED) 기대" 테스트가 `TestMsg.Flags`를 세팅하지 않아
`memset`으로 0인 채 실행됨 → `auth_level=0` → §18.11.1 권한검증에서 요구레벨(2 또는 3)에
항상 미달 → `REJECT_STATE`로 귀결. 42개 실패가 여기 해당. 코드는 정상 동작 중이며,
테스트가 각 명령 클래스에 맞는 권한 레벨을 `Flags` bit[7:6]에 실어 보내도록 갱신하면
해결된다(§18.10.3에서 이미 설계한 "요구 레벨을 그대로 채워 보낸다" 원칙과 동일 — 코드
변경 없음).

**요인 B — DIAGNOSTIC 클래스가 구조적으로 영구 인증 불가 (실제 코드 버그, §18.10.3의
"미해결" 항목이 단순 라벨 오기가 아니라 기능적으로 심각하다는 뜻으로 재확인됨)**

- `GetClassRequiredLevel(DIAGNOSTIC=6)`은 case 6("counter management" 주석, 실제로는
  DIAGNOSTIC이 여기 걸림)에서 `3`을 반환.
- 레벨 3 명령은 `IsAuthorized()`에서 0이 아닌 `request_token`을 요구.
- `request_token`을 payload에서 파싱하는 분기는 `CommandClass == RECOVERY` 또는
  `== MODE`일 때만 존재 — **DIAGNOSTIC에 대한 파싱 분기가 아예 없다.**
- 결과: DIAGNOSTIC 명령은 `Flags` bit[7:6]을 무엇으로 채워도(심지어 3으로 채워도)
  `request_token`이 항상 0이라 `IsAuthorized()`가 항상 `false` → **DIAGNOSTIC 클래스는
  현재 코드에서 구조적으로 영구 차단 상태**.
- 스펙(§18.10.1)상 DIAGNOSTIC은 `RECOVERY`/`FAILED` 상태에서 `RECOVERY` 클래스와 함께
  유일하게 허용되는 "항상 통하는 개입 경로"로 의도됨 — 그런데 실제로는 그 경로 자체가
  auth 게이트에서 막혀 있어 **의도한 fail-safe 개입 수단이 작동하지 않는 상태**.
- **부작용**: 기존 테스트 `Test_UPLINK_APP_ProcessUplink_BlockedFailed`(FAILED 상태에서
  DIAGNOSTIC 전송, REJECT_STATE 기대)는 "스펙상 허용돼야 하는데 auth 버그로 막혀서
  우연히 기대값과 일치"하여 PASS로 위장돼 있었음 — 즉 이 테스트는 원래 의도(허용 확인)와
  반대로, 버그가 있어야 통과하는 상태였다.

**처리 방침**: 요인 A는 테스트 파일만 수정(진행). 요인 B(`GetClassRequiredLevel`
production 로직 수정)는 인증 게이트를 건드리는 변경이므로 사용자 승인 하에 별도 진행.

**완료 (2026-07-14)**: 사용자 승인 하에 요인 B 수정 진행. 추가로 조사 중 **세 번째 원인**을
발견 — `uplink_app_cmds.c`의 `CfsHealthReceived` 게이트가 커밋 `1112351`에서
`if (CfsHealthReceived) {...}`(미수신 시 통과, fail-open)에서 `if (!CfsHealthReceived)
{ REJECT_STATE }`(미수신 시 항상 차단, fail-safe boot)로 **의도적으로 극성이
뒤집혔으나, 그 이후 이 UT 스위트가 한 번도 실행되지 않아** 대부분의 "성공(ROUTED)
기대" 테스트가 사실 이 게이트에서부터 막히고 있었음(§18.11.1 auth 문제는 그다음
단계라 도달하지도 못했던 경우가 대부분). 즉 44개 실패의 실제 구성은:
1. (다수) `CfsHealthReceived=1U` 미설정 — 테스트가 오래된 fail-open 정책 가정 채로 방치
2. (다수, 1과 중첩) §18.11.1 auth 레벨 `Flags` 미설정
3. (2개, `DiagnosticAccept`/`ForwardFail`) 요인 B의 DIAGNOSTIC 영구 차단
4. `AllowedDegradedRouteUpdate` — 편집 누락으로 Flags만 빠짐(단순 실수)

수정 내역:
- 모든 "성공 기대" 테스트에 `CfsHealthReceived=1U` 및 클래스별 `Flags` 인증레벨 추가
- RECOVERY/MODE(레벨3) 테스트에 request_token 페이로드 바이트 추가
- `GetClassRequiredLevel`을 `UPLINK_APP_CLASS_*` named enum으로 재작성, DIAGNOSTIC↔MODE
  요구레벨 스왑(DIAGNOSTIC: 3→1, MODE: 1→3) — DIAGNOSTIC이 다시 인증 가능해짐
- `Test_UPLINK_APP_ProcessUplink_BlockedFailed`: 원래 DIAGNOSTIC class로 작성돼
  있었으나 FAILED에서 DIAGNOSTIC은 허용돼야 하므로 테스트 의도와 반대 — `BlockedRecovery`와
  동일하게 CONFIG class로 교체
- `Test_UPLINK_APP_ProcessUplink_FailOpenBeforeHealth` → `BlockedBeforeHealth`로 개명,
  기대값을 현재(의도된) fail-closed 정책에 맞게 REJECT_STATE로 정정
- 하네스 정상화로 그동안 제외했던 `ForceFlagBypassesDegradedBlock`/
  `ForceFlagNoOpWhenNotBlocked` 양성 테스트 추가 및 통과 확인

**결과**: `uplink_app_cmds` UT 91/91 통과(로컬 verify-build, native). `uplink_app`(8/8),
`uplink_app_dispatch`(13/13) 회귀 없음. `uplink_app_utils`에서 무관한 사전 결함 4건
발견(`ParseLoRaFrame` 관련, `Test_ParseLoRaFrame_...` — 오늘 변경 범위 밖, 별도 이슈로만
기록).

**반영 완료** (2026-07-21 정정): 커밋 `740521d`로 `uplink_app_cmds.c`(프로덕션 코드)에
git 커밋됨 — 위 "미반영" 기재는 stale이었음. 실제 배포(Pi) 여부는 별도 확인 필요하나,
저장소 기준으로는 baseline에 포함됨.

현재 baseline 구현에서 추가로 세분화가 필요한 항목은 다음과 같다.

- ✅ 명령 클래스별 기본 권한 수준은 Section 17.5에서 고정했으며, §18.11.1에서 권한 검증 정책 확정 (Phase 3.2 완료 예정).
- 시퀀스 번호 정책은 strict monotonic increase로 고정했으며, wraparound 허용 여부와 허용 시간 창은 추가 확장 시에만 세분화한다.
- 명령 라우팅은 Section 17.6과 Section 18.4.7에서 baseline target을 고정했으며, 세부 command code별 MID 매핑 표만 추가하면 된다.
- 업링크 packet 형식과 version 정책은 Section 18.4.3의 envelope과 현재 `version=1` 기준을 사용한다.

### 18.11.1 권한 검증 정책 (Phase 3.2)

**목적**: 명령 클래스별 기본 권한 수준을 넘어, 지상국이 보내는 개별 명령의 권한 수준을 검증하여 무인증 네트워크에서도 기본적인 출처 제어를 제공한다.

> **보안 범위 명시 (BL-46, 2026-07-24 재검토 확정 — 유지)**
> 이 권한 검증은 **위조(forgery) 방어가 아니라 운영자 실수 방지** 수준이다.
> `Flags[7:6]` 권한 레벨은 지상국이 **자기신고**로 채우는 값이라, 공유 비밀키
> 서명이 없는 현 구조에서는 프레임을 만들 수 있는 제3자(같은 LoRa 설정을 아는
> 외부인)가 그냥 `2`로 채워 우회할 수 있다. CRC도 잡음 무결성용이지 키 기반
> 인증이 아니다. 따라서 **RF 명령 주입(외부인이 기체에 명령 위조 전송)은 현
> 구조로 막지 못한다.** 실효는 ① 운영자가 파괴적 명령을 의도적으로 표시하게
> 강제(Level 3 `request_token≠0`), ② 클래스별 최소 권한 게이트뿐이다.
>
> **결정 근거**: 현 운용은 연구/취미 단계로 근거리 적대적 RF 공격자를 위협
> 모델에 포함하지 않는다(2026-07-24 사용자 확정). 따라서 현 구조를 유지한다.
> 위협 모델이 바뀌면(예: 공개 시연·경쟁·실운용) **강화**로 재검토 — 유력안은
> MAVLink 2 signing 방식의 경량 대칭키 MAC: LoRa 프레임 canonical 문자열(CRC와
> 동일 입력)에 공유키로 절단 HMAC(4~6B)를 붙이고, 기존 `seq`/`boot_count`를
> anti-replay nonce로 재사용. 참고: CCSDS SDLS(MAC+AES-GCM+anti-replay),
> MAVLink 2 message signing(HMAC-SHA256 6B + timestamp + link_id).

#### 명령 클래스별 권한 수준 맵핑

| 명령 클래스 | 코드 | 권한 수준 | 근거 |
|---|---|---|---|
| `NOOP` | 0 | Level 1 | §17.5: 읽기 전용 |
| `runtime configuration` | 1 | Level 2 | §17.5: 구성 명령 |
| `route update` | 2 | Level 2 | §17.5: 경로 업데이트 |
| `viewpoint update` | 3 | Level 2 | §17.5: 뷰포인트 업데이트 |
| `recovery command` | 4 | Level 3 | §17.5: 복구 명령 |
| `diagnostic command` | 5 | Level 1 | §17.5 미명시, RECOVERY/FAILED에서도 허용 → 낮은 권한 |
| `counter management` | 6 | Level 3 | §17.5: 파괴적 동작 |
| `mode command` | 7 | Level 3 | §17.5: 복구 명령 |

#### 권한 검증 규칙

**기본 규칙**: 요청의 권한 수준 ≥ 명령 클래스 요구 수준

```c
uint8 auth_level = (Cmd->Flags >> 6) & 0x3;  // Bits[7:6]
uint8 required_level = get_class_required_level(Cmd->CommandClass);

if (auth_level < required_level) {
    REJECT("AUTHZ_BLOCK: insufficient auth");
    EVS: "UPLINK_APP: command blocked (insufficient auth) auth=%u required=%u class=%u seq=%u"
    return;
}
```

**Level 3 추가 제약**: recovery command, mode command, counter management는 request_token ≠ 0 필수

```c
if (required_level == 3) {
    // 명령별 payload에서 RequestToken/request_token 필드 파싱
    if (request_token == 0) {
        REJECT("AUTHZ_BLOCK: Level 3 requires non-zero token");
        return;
    }
}
```

#### 구현 위치 및 통합

**Code 위치**: `uplink_app_cmds.c` ProcessUplinkCommand()
- 현재 위치: health-block → [NEW: 권한 검증] → command class 라우팅
- 검증 순서: (1) health-block, (2) 권한 검증, (3) class별 상세 검증

**EVS 이벤트**:
- 새 EID: `UPLINK_APP_AUTHZ_BLOCK_EID`
- 메시지: `"UPLINK_APP: command blocked (insufficient auth) auth=%u required=%u class=%u seq=%u"`

**Request Token 연계** (§18.4.7):
- Level 3 명령의 request_token은 §18.4.7 기본안에 따라 지상국이 채번한 불투명 값
- 탑재 앱은 token 유무만 검증 (값 해석 없음)
- token echo는 명령 결과 텔레메트리에 포함 (이미 구현)

#### 테스트 케이스 (통합 테스트용, 단위테스트 미실행)

| 시나리오 | 요청 권한 | 명령 클래스 | 부가 조건 | 결과 | EVS |
|---|---|---|---|---|---|
| **정상**: L2 request, L2 class | 1 | route_update | — | ✅ 허용 | 없음 |
| **정상**: L3 request, L3 class | 2 | recovery | token≠0 | ✅ 허용 | 없음 |
| **정상**: L3 upward compat | 2 | diagnostic(L1) | — | ✅ 허용 | 없음 |
| **오류**: 권한 부족 | 0 | runtime_config(L2) | — | ❌ 거부 | AUTHZ_BLOCK |
| **오류**: L2 → L3 | 1 | recovery(L3) | — | ❌ 거부 | AUTHZ_BLOCK |
| **오류**: L3 token=0 | 2 | counter_mgmt(L3) | token=0 | ❌ 거부 | AUTHZ_BLOCK |

---

### 18.12 추가 고려사항

다음 항목은 현 단계에서 상세 구현 또는 수치 정책까지 고정하지 않지만,
최종 명령 처리 및 복구 정책을 확정할 때 반드시 함께 검토해야 한다.

- 재부팅 후 상태 유실: `boot_count`, `reset_reason`, startup state, 마지막으로 승인된 command sequence, active configuration, route cache 복원 여부를 함께 고려해야 한다.
- 오래된 명령 또는 중복 명령의 재실행: sequence, timestamp, replay reject 외에도 boot 경계, sequence wraparound, 허용 시간 창을 고려해야 한다.
- app crash 또는 hang: heartbeat, restart count, app health 외에 restart 주체, restart 성공 기준, hang 판정 기준을 고려해야 한다.
- FC link 부분 장애: 완전한 링크 손실뿐 아니라 MAVLink heartbeat는 살아 있으나 attitude/GPS/EKF 일부만 stale한 경우도 별도로 고려해야 한다.
- 통신 복구 후 명령 폭주: queue limit, priority, stale drop 외에 burst rate limit, old command purge, 중복 route update 정리 정책을 고려해야 한다.
- 반복 재시작 또는 반복 복구 실패: recovery escalation과 minimum-reporting mode 외에 재시작 횟수 창, operator intervention 필요 조건, 최종 fail-safe 상태를 고려해야 한다.
- 시간 무효 상태: `TimeValid=false`일 때 어떤 명령은 제한하고 어떤 명령은 허용할지, route update를 포함한 상태 의존 명령 제한을 고려해야 한다.
- 부분 장애 상태 정의: Section 15의 `DEGRADED_*` fault code 구분을 유지하되, 필요한 경우 세부 fault code 집합만 확장한다.
- persistent state integrity: boot counter, command sequence cache, route cache, active configuration 저장본의 checksum 또는 CRC 오류 처리 정책을 고려해야 한다.
- route/config 적용 원자성: route update 또는 configuration 변경이 일부만 적용된 상태로 남지 않도록 atomic apply 또는 rollback 필요성을 고려해야 한다.
- **waypoint 개수 상한 재검토 (2026-07-11 도출)**: `MAX_ROUTE_WAYPOINT_COUNT=16`은 LoRa 프레임 페이로드(196바이트)에 정확히 맞춰 역산된 값(§18.4.6.2.1 참조)이며, APPEND로 여러 프레임에 나눠도 최종 합계는 16에서 절단된다. 원형 촬영 등 고밀도 waypoint가 필요한 미션 요구가 늘어나면, LoRa 프레임 분할 프로토콜 또는 메시지 구조체(3개 앱 공통) 재설계가 필요한지 검토해야 한다.
- 복구 중 허용 명령: `CFS_RECOVERY` 또는 최소 보고 상태에서 허용되는 명령 클래스의 최소 집합을 별도로 검토해야 한다.
- ✅ 권한 검증: §18.11.1에서 명령 클래스별 권한 수준 및 검증 규칙 확정 (Phase 3.2 구현 진행 중).

### 18.13 지상 Uplink CLI 명령 표면

지상 운영자 인터페이스(OpenMCT "FS Uplink CLI", 지상 `fc_serial_ws_server.py` 경유)는
표준 uplink envelope을 생성해 LoRa transport로 송신하는 운영자측 surface이다.
이 CLI는 `uplink_app` 명령 클래스(§18.2)의 일부만 노출하며, 노출 여부는 cFS측 계약과 독립적이다.

| CLI 명령 | 매핑 command_class | uplink_app 계약 | 현 노출 상태 |
| --- | --- | --- | --- |
| `config <scope> <param> <value>` | runtime configuration (`1`) | §18.4.6.1 / §18.5 | ✅ 노출 |
| `recovery [payload_hex]` | recovery command (`4`) | §18.4.6.4 / §18.6 | ✅ 노출 |
| `uplinktest` | (전송 없음, 서버 health/param 조회) | — | ✅ 노출 |
| `route <route_type> <wp...>` | route update (`2`) | §18.4.6.2 | ✅ 노출 (2026-06-29 구현) |

**route 명령 추가 가능성(결정)**: route update는 이미 `uplink_app`의 승인된 클래스이며
검증(`waypoint_count 1..16`, finite, 고도 `2..8m`, 인접 3D 거리 `2..2m`, CRC/길이/sequence)과
라우팅(`ROUTE_UPDATE_MID` → `cfs_core_app`)이 구현되어 있다(§18.4.6.2, §5.1.1).
따라서 CLI `route` 명령 추가는 **cFS측 변경 없이 지상 CLI의 frame 생성 로직만 확장**하면 된다.

`route` 명령 계약(지상 CLI 측):

- 구문: `route <route_op> <x,y,z> [<x,y,z> ...]`
  - `route_op ∈ {replace=1, append=2, delete=3}`.
  - REPLACE/APPEND: waypoint를 LOCAL_NED meters로 나열, Z = AGL 양수.
  - DELETE: waypoint 인수 없이 제거할 개수만 `waypoint_count`로 지정.
- envelope 프레임: `UP,<version=1>,<command_class=2>,<sequence>,<flags>,<payload_hex>,<crc16_hex>`
  (config/recovery와 동일 형식, CRC16-CCITT 적용 범위도 동일).
- route payload byte layout(little-endian): `route_op:u8, route_version:u8, waypoint_count:u8, reserved:u8`,
  이어서 REPLACE/APPEND 시 waypoint마다 `x:f32, y:f32, z:f32`. DELETE는 헤더 4바이트만. (`tools/uplink_route_update_sender.py`의 `build_route_payload`와 동일.)
- 검증 책임 경계: 지상 CLI는 형식/개수 등 최소 사전 점검만 수행하고, 권위 있는 검증은
  §18.4.4 원칙대로 `uplink_app`이 수행한다.
- 송신 정책: config/recovery와 동일하게 4개 연속 downlink 슬롯 자동 재전송(§참조 운영 노트),
  중복 sequence는 `uplink_app`이 replay로 무시(무해).
- health gate: route update의 cFS 상태별 허용은 §18.10 및 `uplink_app` health-block 정책을 따른다.

**구현 위치(2026-06-29)**: 지상 openMCT 리포(`문서/GitHub/openMCT`).
- 서버 `fc_serial_ws_server.py`: `UPLINK_CLASS_ROUTE_UPDATE=2`, `_build_route_payload`,
  `POST /api/uplink/route` (body `{route_type, waypoints:[[x,y,z],...]}`) → class=2 프레임 생성 후
  config/recovery와 동일한 4슬롯 TDM 재전송 큐(`_queue_uplink`)로 송신.
- CLI 플러그인 `my_openmct_app/src/plugins/uplinkCLI/plugin.js`: `route` 명령 + help 추가.
- 생성 프레임은 `tools/uplink_route_update_sender.py`(route-good) 출력과 byte 동일 검증 완료.

#### 18.13.1 향후 요구사항 (planned)

route 생성·전송은 다음 두 가지를 반영하도록 확장한다(현재 미구현, 차기 작업).

1. **누적(append) 방식 — 교체(replace) 아님**: 신규 route를 보낼 때 기존 route를 덮어쓰지 않고
   **기존 waypoint 뒤에 이어붙이는** 것을 기본/선택 모드로 지원한다. payload `Reserved` 바이트를
   `RouteMode`(0=replace, 1=append)로 사용한다. 누적은 `uplink_app`이 현재 route 버퍼를 유지하며
   합쳐진 **전체 목록을 `ROUTE_UPDATE_MID`로 publish**(cfs_core/mavlink_bridge는 무수정, 전체 교체 →
   결과적으로 append). 제약: 합산 ≤ `MAX_ROUTE_WAYPOINT_COUNT`(16), 이음새(기존 마지막↔신규 첫)
   segment 거리 규칙 적용.
2. **절대 고도 고려**: 현재 Z는 Home(0,0) 기준 상대 고도(AGL)다. 향후 **절대 고도**(MSL 또는
   절대 frame, 예: `MAV_FRAME_GLOBAL_RELATIVE_ALT`/global) 입력·해석을 함께 지원하도록 설계한다.
   route payload에 고도 frame 식별을 추가하고, mavlink_bridge 업로드 frame과 정합시킨다.

---

### 18.14 배포 구성 및 테스트 빌드 (Phase 3.4)

**목표**: 운영 환경(배포)과 개발 환경(테스트)의 startup script를 분리하여 배포 보안 강화 (CI_LAB 제거).

#### 배포 빌드 (production)

**파일**: `cpu1_cfe_es_startup.scr` (기본값)

```
CFE_LIB, cfe_assert, ...
CFE_LIB, sample_lib, ...
CFE_APP, mav_bridge_app, ...
CFE_APP, cfs_core_app, ...
CFE_APP, uplink_app, ...
CFE_APP, lora_tdm_app, ...
CFE_APP, to_lab, ...        (CI_LAB 제거 ✅)
CFE_APP, sch_lab, ...
```

**정책**:
- CI_LAB 미포함 → localhost UDP 1234 테스트 경로 비활성화
- LoRa + uplink_app 공식 경로만 활성
- 공격 표면 최소화

**사용 환경**: 실 배포(무인기, 지상국 운영)

#### 테스트 빌드 (development/testing)

**파일**: `cpu1_cfe_es_startup_test.scr` (신규 추가)

```
CFE_LIB, cfe_assert, ...
CFE_LIB, sample_lib, ...
CFE_APP, mav_bridge_app, ...
CFE_APP, cfs_core_app, ...
CFE_APP, uplink_app, ...
CFE_APP, lora_tdm_app, ...
CFE_APP, ci_lab, ...         (CI_LAB 포함 ✅)
CFE_APP, to_lab, ...
CFE_APP, sch_lab, ...
```

**정책**:
- CI_LAB 포함 → localhost UDP 1234 테스트 경로 활성
- `tools/query_fc_mission.py`, `tools/uplink_*.py` 등 테스트 도구 사용 가능
- 개발·통합테스트·CI 자동화에 사용

**사용 환경**: 개발, 단위테스트, 통합테스트, CI/CD 파이프라인

#### 빌드 선택

| 시나리오 | Startup Script | CI_LAB | 비고 |
|---|---|---|---|
| 실 배포 | `cpu1_cfe_es_startup.scr` | ❌ | 운영 환경, 보안 강화 |
| 로컬 테스트 | `cpu1_cfe_es_startup_test.scr` | ✅ | UDP 1234 로컬 명령 가능 |
| CI/CD 자동화 | `cpu1_cfe_es_startup_test.scr` | ✅ | `query_fc_mission.py` 등 사용 |
| 하드웨어 검증 | `cpu1_cfe_es_startup.scr` | ❌ | LoRa 실제 하드웨어 경로만 |

#### 구현 상태

✅ **완료** (2026-07-05 Phase 3.4):
- `cpu1_cfe_es_startup.scr`: CI_LAB 제거
- `cpu1_cfe_es_startup_test.scr`: 신규 작성 (CI_LAB 포함)
