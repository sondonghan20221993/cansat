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
| `mavlink_bridge_app` | FC가 제공하는 MAVLink 텔레메트리를 수신하고 임무 상태 필드를 추출하여 임무 상태 MID를 게시한다. | `IMU_STATE_MID`, `GPS_STATE_MID`, `EKF_STATE_MID`, `BRIDGE_STATUS_MID` | FC의 UART 기반 MAVLink 입력 |
| `cfs_core_app` | 수신된 임무 상태를 검증하고, 상태 및 복구 정책을 관리하며, 시스템 상태를 게시한다. | `SYSTEM_HEALTH_MID` | `IMU_STATE_MID`, `GPS_STATE_MID`, `EKF_STATE_MID`, `BRIDGE_STATUS_MID`, 앱 health/status MID |
| `downlink_app` | Software Bus에서 승인된 임무 상태 및 텔레메트리 MID를 수집하고, downlink packet을 구성하여 지상국으로 전송한다. MAVLink를 직접 파싱하거나 상태 유효성을 평가하지 않는다. | `DOWNLINK_STATUS_MID` | `IMU_STATE_MID`, `GPS_STATE_MID`, `EKF_STATE_MID`, `SYSTEM_HEALTH_MID`, 승인된 텔레메트리 MID |
| `uplink_app` | 지상국 명령을 수신하고 업링크 패킷을 검증한 뒤, 승인된 런타임 설정, 경로 수정, viewpoint, 복구 명령을 임무 앱으로 전달한다. | `UPLINK_STATUS_MID` | `UPLINK_CMD_MID` 또는 승인된 전송 입력 |

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
| `IMU_STATE_MID` (`0x1900`) | `mavlink_bridge_app` | `mavlink_bridge_app` | `cfs_core_app`, `downlink_app` | `MAVLINK_BRIDGE_APP_CMD_MID` (`0x18A0`) | 20 Hz target, timeout 시 상태 갱신 이벤트 publish | Section 6.1 | MAVLink `ATTITUDE` 기반 필수 필드 파싱 성공, `TimeValid=true`, 데이터 범위 검사 통과 | 파싱 실패/timeout 시 `Valid=false`, `HealthState=DEGRADED 또는 LOST`, 오류 코드 반영 | `CFE_TIME` mission elapsed ms | 생산자 로컬 단조 증가, wrap 허용, 역행/중복은 소비자가 stale로 처리 |
| `GPS_STATE_MID` (`0x1901`) | `mavlink_bridge_app` | `mavlink_bridge_app` | `cfs_core_app`, `downlink_app` | `MAVLINK_BRIDGE_APP_CMD_MID` (`0x18A0`) | 5 Hz target, timeout 시 상태 갱신 이벤트 publish | Section 6.2 | MAVLink `GPS_RAW_INT` 기반 필수 필드 파싱 성공, fix/type 정책 통과 시 `Valid=true` | fix 미달/timeout 시 `Valid=false` 또는 `DEGRADED`, 오류 코드 반영 | `CFE_TIME` mission elapsed ms | 생산자 로컬 단조 증가, wrap 허용 |
| `EKF_STATE_MID` (`0x1902`) | `mavlink_bridge_app` | `mavlink_bridge_app` | `cfs_core_app`, `downlink_app` | `MAVLINK_BRIDGE_APP_CMD_MID` (`0x18A0`) | 10 Hz target, 상태 변화 시 즉시 publish | Section 6.3 | `EKF_STATUS_REPORT` 기반 필수 상태 플래그 파싱 성공 | EKF invalid/stale 시 `Valid=false`, `DEGRADED_EKF` 계열 FaultCode 반영 | `CFE_TIME` mission elapsed ms | 생산자 로컬 단조 증가, wrap 허용 |
| `BRIDGE_STATUS_MID` (`0x1903`) | `mavlink_bridge_app` | `mavlink_bridge_app` | `cfs_core_app`, `downlink_app` | `MAVLINK_BRIDGE_APP_CMD_MID` (`0x18A0`) | 1 Hz periodic + 링크 상태 변화 이벤트 | Section 6.4 | 링크 상태 평가 주기 내 필수 카운터/상태 필드 갱신 | open/reopen 실패, parser error 누적, timeout 발생 시 상태 저하와 FaultCode 게시 | `CFE_TIME` mission elapsed ms | 생산자 로컬 단조 증가, wrap 허용 |
| `SYSTEM_HEALTH_MID` (`0x1904`) | `cfs_core_app` | `cfs_core_app` | `downlink_app`, 운영자 모니터링 소비자 | `CFS_CORE_APP_CMD_MID` (`0x18C0`) | 1 Hz periodic + 상태 전이 이벤트 | Section 6.5 | 필수 입력(IMU/GPS/EKF/BRIDGE) freshness/유효성 규칙 통과 | 입력 부족 시 `CFS_DEGRADED` 또는 `CFS_RECOVERY` 게시, FaultCode로 원인 구분 | `CFE_TIME` mission elapsed ms | 생산자 로컬 단조 증가, wrap 허용 |
| `DOWNLINK_STATUS_MID` (`0x1905`) | `downlink_app` (`lora_fc_downlink_app`) | `downlink_app` | `cfs_core_app`, 운영자 모니터링 소비자 | `DOWNLINK_APP_CMD_MID` (`0x18B0`), `DOWNLINK_APP_SEND_HK_MID` (`0x18B1`) | 1 Hz periodic + 송신 결과 이벤트 | Section 6.6 | downlink 처리 루프가 상태 필드와 카운터를 최신으로 유지 | 송신 실패 시 오류 카운터 증가, `DEGRADED` 상태와 마지막 오류 코드 게시 | `CFE_TIME` mission elapsed ms | 생산자 로컬 단조 증가, wrap 허용 |
| `UPLINK_STATUS_MID` (`0x190A`) | `uplink_app` | `uplink_app` | `cfs_core_app`, 운영자 모니터링 소비자 | `UPLINK_APP_CMD_MID` (`0x18D0`) | 1 Hz periodic + 명령 처리 결과 이벤트 | Section 18.7 | 프레임 검증/라우팅 처리 결과를 상태 필드에 반영 | CRC/길이/인증/시퀀스 실패 시 reject 카운터와 오류 코드 게시 | `CFE_TIME` mission elapsed ms | 수락된 uplink command sequence는 단조 증가, 회귀/중복 거부 |
| `ROUTE_UPDATE_MID` (`0x190B`) | `cfs_core_app` | `uplink_app`(입력 생산), `cfs_core_app`(cache 반영 상태 생산) | `cfs_core_app`(입력 소비), 임무 경로 소비자 앱 | `UPLINK_APP_CMD_MID` (`0x18D0`) ingress, 내부 route 반영 인터페이스 | 이벤트 기반(유효 route update 수락 시) | Section 18.5.2 route payload + Section 6.5 연계 상태 | waypoint 개수(`1..16`), 필드 범위, route version/sequence, CRC/길이, 인접 waypoint 거리(`2m..2m`) 검증 통과 | 검증 실패 시 `uplink_app`에서 거부, `UPLINK_STATUS_MID`에 원인 게시, 기존 active route 유지 | `CFE_TIME` mission elapsed ms | route update sequence는 소스별 단조 증가, 회귀/중복은 거부 |

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

### 6.6 `DOWNLINK_STATUS_MID`

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
  잘못된 MAVLink 프레임 시퀀스; 성능이 저하되거나 손실된 게시 `BRIDGE_STATUS_MID`
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

### 11.1 권장되는 1차 복구 한계

다음 값은 첫 번째 통과 임무 기본값입니다. cFS로 정의되지 않았습니다.
표준이며 하드웨어 테스트 후에 개정됩니다.

| 목표 | 결함 상태 | 첫 번째 복구 | 재시도 간격 | 최대 재시도 횟수 | 단계적 확대 |
| --- | --- | --- | --- | --- | --- |
| `mavlink_bridge_app` | FC 전송 시간 초과, 파서 오류 버스트, 잘못된 MAVLink 프레임 시퀀스 | 로컬 파서를 다시 초기화하거나 FC 스트림을 다시 요청하세요. | 5초 | 3 | 마크 브리지 `FAILED`; 복구 권한 평가 요청 |
| `downlink_app` | 다운링크 전송 시간 초과, 전송 실패 버스트, 패킷 포맷 오류 | 전송 엔드포인트를 다시 열고 전송 경로를 다시 시도하세요. | 5초 | 3 | 다운링크를 `FAILED`로 표시; 허용되는 경우 지상 배송 없이 로컬 cFS 운영을 계속합니다. |
| `cfs_core_app` | 반복되는 앱 오류 또는 시스템 오류 | 허용되는 경우 실패한 앱을 다시 시작하세요. | 60초 | 앱당 3개 | `CFS_RECOVERY` 또는 최소 보고를 입력하세요. |
| 라즈베리 파이/cFS 호스트 | 시스템 수준의 복구 불가능한 cFS 호스트 오류 | Pi/cFS 프로세스 또는 호스트 재설정 | 해당 없음 | 30초당 1개의 복구 창 | 오류가 반복되는 경우 최소 보고 시작 |

재시도 간격은 완료 또는 실패 감지 시간부터 계산됩니다.
이전 복구 시도.

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

현재 기준 저장소 백엔드:

- persistent state는 Raspberry Pi 파일 시스템에 record 단위로 저장한다.
- 각 record write는 replace 또는 rename 기반 atomic write를 사용해야 한다.
- 저장 빈도는 event-driven update를 기본으로 하며, 고속 sensor sample은 저장 대상이 아니다.
- 다른 백엔드로 교체하더라도 본 문서의 record integrity field와 reboot recovery contract를 유지해야 한다.

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
| `downlink_app` | **예** | 전송 주기, 전송 타임아웃, 활성 MID 목록 등 런타임 변경 가능 파라미터를 보유한다 |
| `uplink_app` | **제한적** | `uplink_app` 자체 파라미터(최대 payload 길이, 프로토콜 버전 허용 범위 등)가 필요한 경우에만 로컬 버퍼를 유지한다. 다른 앱 대상 config는 해당 앱 MID로 전달만 수행하며 버퍼를 소유하지 않는다 |

`uplink_app`은 config 명령을 수신하면 `config_scope` 필드로 대상 앱을 판별하고, 대상 앱의 `CONFIG_CMD_MID`로 검증된 payload를 전달한다. 각 target 앱이 자신의 pending 버퍼에서 최종 검증 및 적용을 수행한다.

### 13.2 앱별 필수 상태 변수

pending/active 버퍼를 소유하는 각 앱은 최소한 다음 상태 변수를 선언해야 한다.

| 변수 | 형식 | 의미 |
| --- | --- | --- |
| `ConfigPendingState` | `uint8` | `IDLE=0`, `PENDING=1`, `REJECTED=2` |
| `LastConfigResult` | `uint8` | `0=성공`, `1=실패` (최근 활성화 결과) |
| `LastRollbackReason` | `uint8` | 롤백 발생 시 원인 코드, 없으면 `0` |
| `ConfigGeneration` | `uint32` | 활성화 성공마다 단조 증가 |

`previous_config`(롤백 버퍼)와 `active_config_version` / `pending_config_version`은 각 앱의 파라미터 스키마 확정 이후 앱별로 추가한다.

### 13.3 활성화 경계

활성화 경계는 앱마다 다릅니다.

| 앱 | 활성화 경계 |
| --- | --- |
| `mavlink_bridge_app` | 다음 파서 또는 FC 스트림 처리 경계 |
| `downlink_app` | 다음 전송 스케줄링 경계 |
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
- `DEGRADED_GPS`
- `DEGRADED_EKF`
- `DEGRADED_LINK`
- `DEGRADED_APP`

즉 상태 기계는 단일 `CFS_DEGRADED`를 유지할 수 있지만, 테스트 기대 결과와 운용자 판단은 세부 결함 원인까지 관찰 가능해야 한다.

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
| `mavlink_bridge_app` MAVLink 수신 | UART 기반 `ATTITUDE`, `GPS_RAW_INT`, `EKF_STATUS_REPORT` 입력 | 해당 MID가 정상 publish되고 `BRIDGE_STATUS_MID`가 갱신된다. | 수신 로그, Software Bus subscribe 결과, HK 카운터 확인 |
| `mavlink_bridge_app` 오류 처리 | 잘못된 길이, CRC 오류, 순서 오류, timeout | invalid frame이 폐기되고 링크 상태가 성능 저하 또는 손실로 반영된다. | `BRIDGE_STATUS_MID`의 `FaultCode`, discard count, 상태 MID 확인 |
| `cfs_core_app` 상태 종합 | 정상 IMU/GPS/EKF/bridge 입력 | `SYSTEM_HEALTH_MID`가 정상 상태로 게시된다. | health 상태 필드, `FaultCode=FAULT_NONE`, sequence 증가 확인 |
| `cfs_core_app` 복구 판단 | bridge timeout, EKF invalid, GPS stale | 성능 저하 또는 복구 필요 상태가 게시되고 필요한 복구 정책이 요청된다. | health 상태와 함께 `FaultCode=DEGRADED_BRIDGE`, `DEGRADED_EKF`, `DEGRADED_GPS` 중 해당 값, recovery request 로그 확인 |
| `downlink_app` 패킷 구성 | 승인된 상태 MID 입력 | downlink packet이 생성되고 송신 카운터와 마지막 송신 시각이 갱신된다. | 송신 로그, `DOWNLINK_STATUS_MID`, HK 확인 |
| `downlink_app` 송신 오류 처리 | LoRa 또는 지상국 송신 실패 | 송신 오류 수가 증가하고 오류 상태가 유지된다. | error count, last fault code 확인 |
| `uplink_app` 설정 반영 | 유효한 runtime configuration 명령 | pending 검증 후 active 설정이 갱신된다. | 설정 상태, sequence, 적용 로그 확인 |
| `uplink_app` 설정 거부 | 범위 오류, CRC 오류, 비호환 설정 | 설정이 거부되고 active 값은 유지된다. | reject code, active config 불변 확인 |
| `uplink_app` UDP 임시 입력 검증 | `localhost` UDP 또는 동등한 mock transport를 통해 정상/비정상 uplink packet을 입력한다. | 실제 하드웨어 없이도 수신, 길이 검증, CRC 검증, sequence 검증, route/viewpoint payload 검증이 수행된다. | accept/reject count, 오류 이벤트, `UPLINK_STATUS_MID`, 로그 확인 |
| `uplink_app` 경로 수정 | 승인된 기존 경로 수정 payload | 경로 수정 정보가 수신·검증되고 상위 임무 계층에 전달된다. | uplink status, route update 처리 로그 확인 |
| `uplink_app` 내부 복구 명령 | parser reset, serial reconnect, app restart request | 승인된 명령만 전달되고 결과가 `UPLINK_STATUS_MID`에 반영된다. | command result, reject/accept count 확인 |
| `cfs_core_app` 경로 상태 반영 | 정상 route update 또는 landing route update 입력 | mission route/landing route cache, route update counter, 마지막 route update 시각이 갱신된다. | HK, route update 로그, `SYSTEM_HEALTH_MID` 연계 상태 확인 |
| `downlink_app` mock sink 출력 | 실제 LoRa 장치 대신 file sink, stdout sink, UDP localhost sink 또는 동등한 mock sink로 송신한다. | downlink packet 생성, sequence 증가, 송신 카운터 증가, 실패 시 error count 반영이 확인된다. | HK, 송신 로그, mock sink 출력 확인 |
### 16.2 통합 테스트

| 통합 시나리오 | 절차 | 기대 결과 |
| --- | --- | --- |
| FC 상태 수신부터 downlink 송신까지 | FC가 MAVLink 상태를 송신하고 Raspberry Pi가 이를 받아 지상국으로 전송한다. | `mavlink_bridge_app` → `cfs_core_app` → `downlink_app` 경로가 연속 동작하고 지상국에서 상태를 확인할 수 있다. |
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
| `UPLINK_STATUS_MID` | `0x190A` |
| `ROUTE_UPDATE_MID` | `0x190B` |

`DOWNLINK_STATUS_MID`는 현재 기준으로 `0x1905`를 사용한다.

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
4. `lora_fc_downlink_app` (Section 4의 `downlink_app` 구현체)

기준 우선순위 정책:

- `mav_bridge_app`와 `cfs_core_app`은 downlink/uplink 앱보다 높은 우선순위를 가져야 한다.
- 본 섹션의 `mav_bridge_app`은 Section 4의 `mavlink_bridge_app`과 동일한 앱을 의미한다.
- `uplink_app`은 `lora_fc_downlink_app`와 같거나 더 높은 우선순위를 가져야 한다.
- 스택 크기는 각 앱의 현재 cFS sample baseline을 사용하되, parser 또는 route validation 확장으로 인해 오버플로우 위험이 확인되면 그 앱만 상향 조정한다.

### 17.4 앱 기간 제어 기준

- `mavlink_bridge_app`: app-local polling loop를 사용한다.
- `cfs_core_app`: event-driven 입력 처리와 1 Hz health publish를 병행한다.
- `uplink_app`: event-driven command 처리와 1 Hz status publish를 사용한다.
- `lora_fc_downlink_app`: subscribed state message 기반 event-driven packet compose를 사용하고, status/HK는 1 Hz 기준을 따른다.

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
- `lora_fc_downlink_app`의 외부 승인 command는 `NOOP`, `RESET_COUNTERS`, `SEND_HK`로 제한한다.

### 17.8 시간 기준 유지 정책

- 현재 기준 시간축은 `mission elapsed millisecond`로 고정한다.
- 하드웨어 테스트 이후에도 절대 시간 기준을 기본값으로 승격하지 않는다. 절대 시간은 필요 시 보조 필드로만 추가한다.

### 17.9 상태별 앱 운영 정책 확정

- `CFS_NOMINAL`: 모든 기준 앱 활성
- `CFS_DEGRADED`: 모든 기준 앱 활성, 단 정상 상태가 필요한 명령만 차단
- `CFS_RECOVERY`: `cfs_core_app`와 필수 bridge/status path 유지, 구성 변경과 mode 변경 차단
- 최소 보고 시작: `cfs_core_app`, 필수 bridge/status publish, 오류 보고만 유지
- `CFS_DEGRADED -> CFS_NOMINAL` 복귀 조건: 필수 입력(`IMU_STATE_MID`, `GPS_STATE_MID`, `EKF_STATE_MID`, `BRIDGE_STATUS_MID`)이 freshness/유효성 규칙을 모두 만족하고 active critical FaultCode가 없는 상태가 연속 `10 s` 유지될 때 복귀한다.
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

route update payload는 최소한 다음 필드를 포함해야 한다.

| 필드 | 형식 | 의미 | 검증 규칙 |
| --- | --- | --- | --- |
| `route_type` | `uint8` | `mission_extension` 또는 `landing` | 승인된 route type만 허용 |
| `route_version` | `uint8` | payload 버전 | 현재 지원 버전과 일치 |
| `waypoint_count` | `uint8` | waypoint 개수 | `1` 이상 최대 waypoint 제한 이하 |
| `waypoints` | waypoint 배열 | route segment 좌표 | finite, flyable area, altitude, segment distance 조건 충족 |

route update baseline 수치 기준:

- `MAX_ROUTE_WAYPOINT_COUNT = 16`
- 인접 waypoint 간 3D 거리: `2m 이상 2m 이하`

출력 계약:

- 유효한 payload는 mission layer가 직접 사용할 수 있는 검증된 route segment 구조로 변환되어야 한다.
- 최소 출력 필드는 `route_type`, `route_version`, `waypoint_count`, waypoint 배열이다.
- 현재 구현에서 내부 bus message를 사용하는 경우, 그 message는 raw payload copy가 아니라 검증된 구조 표현이어야 한다.

거부 조건:

- payload 길이 불일치
- waypoint 수 위반
- 좌표가 finite가 아님
- 비행 가능 영역 위반
- 고도 제약 위반
- 인접 waypoint 거리 제약 위반

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

##### 18.4.6.7 counter management

counter management payload는 최소한 다음 필드를 포함해야 한다.

| 필드 | 형식 | 의미 | 검증 규칙 |
| --- | --- | --- | --- |
| `counter_scope` | `uint8` | 대상 앱 또는 전역 범위 | 승인된 범위만 허용 |
| `counter_action` | `uint8` | reset 또는 동등한 관리 동작 | 승인된 action만 허용 |
| `confirm_code` | `uint16` 또는 동등한 확인 필드 | 파괴적 관리 동작 확인 | 정책상 요구되면 반드시 일치 |

출력 계약:

- 승인된 경우에만 대상 앱 또는 `cfs_core_app` 카운터 관리 인터페이스로 전달한다.
- 결과는 `UPLINK_STATUS_MID`에 명시적으로 반영되어야 한다.

거부 조건:

- 확인 코드 불일치
- 권한 부족
- 최소 보고 시작 상태에서 금지된 범위

#### 18.4.7 명령 클래스별 라우팅 대상

각 명령 클래스는 최소한 다음 라우팅 기본값을 가져야 한다.

| 명령 클래스 | 기본 라우팅 대상 | 비고 |
| --- | --- | --- |
| runtime configuration | 대상 앱 설정 인터페이스 또는 `cfs_core_app` | active 적용은 대상 앱 경계에서 수행 |
| route update | mission layer route consumer | 현재 구현은 내부 route update message 사용 가능 |
| viewpoint update | planner 또는 mission layer viewpoint consumer | route update와 분리된 target 유지 |
| recovery command | 대상 앱 또는 `cfs_core_app` | action별 대상 허용 집합 필요 |
| mode command | `cfs_core_app` 또는 mode authority | 직접 상태 전이 금지 |
| diagnostic command | 대상 앱 diagnostic interface | 즉시 실행 가능 |
| counter management | 대상 앱 또는 `cfs_core_app` | destructive action 승인 필요 |

구현은 `command_class`만으로 모든 라우팅을 고정해서는 안 되며, 필요한 경우 `target_id` 또는 payload 내부 대상 식별자를 함께 사용해야 한다.

#### 18.4.8 명령 클래스별 최소 테스트 케이스

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

경로 수정 및 viewpoint 명령은 runtime configuration과 별도 경로로 처리해야 한다. `route update` 명령은 기존 임무 경로를 대체하는 명령이 아니라, 기존 경로 뒤에 추가 경로 segment를 이어 붙이는 명령으로 해석한다. 본 시스템은 최소한 `mission_extension`과 `landing`의 두 종류의 추가 경로 segment를 지원해야 한다.

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

기준 publish rate:

- `UPLINK_STATUS_MID`: 1 Hz periodic publish를 기본값으로 한다.
- 명령 수락/거부, replay reject, routing failure, transport state transition 발생 시 추가 event-driven publish를 허용한다.

### 18.8 업링크 전송 경계

`uplink_app`은(는) 승인된 호스트로부터 전달된 표준 uplink envelope을 소비하는 역할을 담당한다. transport-specific 입력은 LoRa 직렬, 지상국 무선 링크, 또는 다른 승인된 채널일 수 있으나, Section 18.4.4의 transport 계층이 이를 표준 envelope으로 변환한 뒤 `uplink_app`에 전달해야 한다.

`uplink_app` 운송 책임:

- transport 계층이 보고한 업링크 연결 상태를 소비하고 상태 보고에 반영한다.
- 전달된 표준 envelope의 version, class, replay, payload semantic validation을 수행한다.
- transport 계층 또는 bridge 구성요소에 parser reset 또는 reconnect 요청을 전달할 수 있다.
- `UPLINK_STATUS_MID`을(를) 통해 전송 상태를 보고합니다.

`uplink_app`은 다운링크 텔레메트리 경로에 대한 최종 링크 상태를 분류하지 않습니다.
downlink 텔레메트리 상태는 `downlink_app`의 책임이며
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
| `CFS_RECOVERY` | 진단 및 상태 확인 명령만 허용하고, 설정 변경 및 모드 관련 명령은 차단한다. |
| 최소 보고 시작 | 카운터 재설정 및 진단 명령만 허용하며, 그 외 명령 클래스는 차단한다. |

### 18.11 `uplink_app`에 대한 미해결 항목

현재 baseline 구현에서 추가로 세분화가 필요한 항목은 다음과 같다.

- 명령 클래스별 기본 권한 수준은 Section 17.5에서 고정했으며, 개별 command code별 권한 매핑 표만 추가로 작성하면 된다.
- 시퀀스 번호 정책은 strict monotonic increase로 고정했으며, wraparound 허용 여부와 허용 시간 창은 추가 확장 시에만 세분화한다.
- 명령 라우팅은 Section 17.6과 Section 18.4.7에서 baseline target을 고정했으며, 세부 command code별 MID 매핑 표만 추가하면 된다.
- 업링크 packet 형식과 version 정책은 Section 18.4.3의 envelope과 현재 `version=1` 기준을 사용한다.

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
- 복구 중 허용 명령: `CFS_RECOVERY` 또는 최소 보고 상태에서 허용되는 명령 클래스의 최소 집합을 별도로 검토해야 한다.
- 권한 또는 출처 검증: sequence/replay 검증과 별도로, 명령 출처와 권한 수준을 어떻게 결합할지 검토해야 한다.
