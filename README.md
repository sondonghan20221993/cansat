# 시스템 명세 문서

이 저장소는 전체 시스템에 대한 명세 문서 프레임워크를 포함한다.

문서 흐름은 다음과 같다.

`전체 목표 -> 아키텍처 -> 인터페이스 -> 모듈 상세 -> 검증`

## 저장소 구조

```text
docs/
  01-system-requirements.md
  02-system-architecture.md
  03-interface-specification.md
  modules/
    05-reconstruction-requirements.md
    06-pose-frame-alignment-requirements.md
    07-cfs-integration-requirements.md
  verification/
    08-verification-plan.md
```

## 문서 읽기 순서

1. [시스템 요구사항](docs/01-system-requirements.md)
2. [시스템 아키텍처](docs/02-system-architecture.md)
3. [인터페이스 명세](docs/03-interface-specification.md)
4. [복원 모듈 요구사항](docs/modules/05-reconstruction-requirements.md)
5. [Pose / Frame 정렬 요구사항](docs/modules/06-pose-frame-alignment-requirements.md)
6. [cFS 연동 요구사항](docs/modules/07-cfs-integration-requirements.md)
7. [검증 계획](docs/verification/08-verification-plan.md)
8. [모듈 담당 가이드](docs/module-ownership-guide.md)

## 역할 분리 기반 개발 구조

이 저장소는 3D mapping, pose alignment, cFS integration 기능을 병렬로 개발한 뒤, 공통 아키텍처 문서와 인터페이스 문서를 통해 통합할 수 있도록 구성되어 있다.

### 문서별 담당 범위

- `docs/01-system-requirements.md`: 시스템 수준 목표, 공통 규칙, 전역 제약조건
- `docs/02-system-architecture.md`: 모듈 구성, 책임, end-to-end 흐름
- `docs/03-interface-specification.md`: 모듈 간 공유되는 계약
- `docs/modules/05-reconstruction-requirements.md`: 3D reconstruction 모듈 요구사항
- `docs/modules/06-pose-frame-alignment-requirements.md`: frame 및 calibration 로직
- `docs/modules/07-cfs-integration-requirements.md`: cFS app 동작, SB, timer, config, event
- `docs/verification/08-verification-plan.md`: 모듈별 및 통합 검증 전략

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

- 이 저장소는 현재 초기 명세 템플릿을 포함하고 있다.
- placeholder text는 프로젝트별 실제 내용으로 교체해야 한다.
