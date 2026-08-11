# 시스템 명세 문서 + 모듈 소스

이 저장소는 전체 시스템 명세 문서와, 각 모듈의 실제 소스를 함께 담는다.

**2026-08-02**: Raspberry Pi 컴패니언 컴퓨터를 쓸 수 없게 되어 cFS 기반 아키텍처를
현재 구성으로는 사용할 수 없게 되었고, QGroundControl 기반으로 전환했다. 관련 소스는
버려진 것이 아니라 `modules/telemetry/`로 옮겨 보존·유지한다. 상세는
[`docs/02-system-architecture.md`](docs/02-system-architecture.md) 참조.

**2026-08-12**: 별도 저장소로 운영하던 `3d_reconstruction`과 `cfs-telemetry-app`을
git subtree로 이 저장소에 통합했다. 이제 클론 하나로 전부 작업한다. 두 원본 저장소는
GitHub에 그대로 남아 있으나, 이후 작업은 이 저장소를 기준으로 한다.

## 저장소 구조

```text
docs/
  02-system-architecture.md   QGC 기반 아키텍처
  payload-spec.md             MAVLink 페이로드 구성
  reconstruction/             복원 모듈 파이썬 프로토타입 (inbox/chunk/session 백엔드)
modules/
  reconstruction/             3D 복원 실험 저장소 (구 3d_reconstruction)
    STATUS.md                 ⭐ 실험 현황 — PART A~F
    README.md                 실험 개요·데이터셋·핵심 교훈
  telemetry/                  cFS 텔레메트리 앱 (구 cfs-telemetry-app)
    cfs_core_app/ mavlink_bridge_app/ lora_tdm_app/ uplink_app/ bridge/ ...
```

## 문서 읽기 순서

1. [시스템 아키텍처](docs/02-system-architecture.md)
2. [페이로드 명세](docs/payload-spec.md)
3. [3D 복원 실험 현황](modules/reconstruction/STATUS.md)
4. [텔레메트리 앱](modules/telemetry/README.md)

## 역할 분리 기반 개발 구조

이 저장소는 3D mapping, pose alignment 기능을 병렬로 개발한 뒤, 공통 아키텍처 문서를 통해 통합할 수 있도록 구성되어 있다.

### 문서별 담당 범위

- `docs/02-system-architecture.md`: 모듈 구성, 책임, end-to-end 흐름
- `docs/payload-spec.md`: FC↔지상 MAVLink 페이로드 구성, 레이트
- `docs/reconstruction/`: 복원 모듈 프로토타입 구현 (inbox 감시, 청크, 세션 백엔드)
- `modules/reconstruction/`: 3D 복원 실험·평가 기록과 실행 스크립트
- `modules/telemetry/`: cFS 앱 소스, MAVLink 브리지, LoRa TDM

### 개발 원칙

- 모듈 내부 동작은 각 모듈 문서 안에만 유지한다.
- 모듈 간 메시지 형식과 데이터 필드는 인터페이스 문서에 유지한다.
- 시스템 전반의 가정과 제약조건은 상위 요구사항 문서에 유지한다.
- 통합 동작은 아키텍처 문서와 cFS 문서에 유지한다.
- 시험 방법과 수용 기준은 검증 계획 문서에 유지한다.

## 작성 규칙

- 모든 요구사항은 명확하고, 시험 가능하며, 버전 관리가 가능해야 한다.
- 모든 문서에서 일관된 용어를 사용한다.
- 모듈 구현을 확정하기 전에 인터페이스를 먼저 정의한다.
- 요구사항이 변경되면 검증 기준도 함께 갱신한다.

## 현재 상태

- 명세 문서(`docs/`)와 모듈 소스(`modules/`)가 한 저장소에 통합되어 있다.
- 구 명세 문서 중 `01/03/04/07/08`번과 `module-ownership-guide.md`는 2026-08-02
  아키텍처 전환 때 제거되었다. 필요하면 `git log` 로 복원할 수 있다.
