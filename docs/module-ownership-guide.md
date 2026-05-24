# 모듈 담당 범위 가이드

## 1. 목적

이 가이드는 3D mapping, pose alignment, cFS integration을 병렬로 개발하더라도 각 요구사항을 독립적으로 작성하고, 이후 충돌 없이 통합할 수 있도록 문서별 담당 범위를 정의한다.

## 2. 권장 역할 분리

### 2.1 3D Mapping 담당

담당 범위:

- 이미지 및 센서 입력 요구사항
- reconstruction pipeline
- reconstruction 출력 및 품질 기준
- mapping 측 시험

주요 문서:

- [03-interface-specification.md](03-interface-specification.md)
- [05-reconstruction-requirements.md](modules/05-reconstruction-requirements.md)
- [08-verification-plan.md](verification/08-verification-plan.md)

### 2.2 Pose / Frame Alignment 담당

담당 범위:

- GPS frame, camera frame, map frame 정의
- transform 로직
- calibration 및 offset 파라미터
- alignment 검증

주요 문서:

- [03-interface-specification.md](03-interface-specification.md)
- [06-pose-frame-alignment-requirements.md](modules/06-pose-frame-alignment-requirements.md)
- [08-verification-plan.md](verification/08-verification-plan.md)

### 2.3 cFS 담당

담당 범위:

- cFS app lifecycle
- SB 메시지 라우팅
- timer 동작
- configuration loading
- logging 및 event 처리
- 모듈 간 runtime integration

주요 문서:

- [02-system-architecture.md](02-system-architecture.md)
- [03-interface-specification.md](03-interface-specification.md)
- [07-cfs-integration-requirements.md](modules/07-cfs-integration-requirements.md)
- [08-verification-plan.md](verification/08-verification-plan.md)

## 3. 문서 배치 규칙

요구사항을 작성하거나 이동할 때는 아래 기준을 사용한다.

| 요구사항 내용 | 배치 문서 |
| --- | --- |
| 전체 임무, 범위, 공통 단위, 시스템 전역 제약조건 | [01-system-requirements.md](01-system-requirements.md) |
| 어떤 모듈이 어떤 모듈과 연결되는지 | [02-system-architecture.md](02-system-architecture.md) |
| 메시지 필드, 구조 정의, timestamp 규칙, 좌표계 규칙, 오류 표현 | [03-interface-specification.md](03-interface-specification.md) |
| 이미지 기반 3D reconstruction 동작 | [05-reconstruction-requirements.md](modules/05-reconstruction-requirements.md) |
| 좌표계 변환 및 calibration 동작 | [06-pose-frame-alignment-requirements.md](modules/06-pose-frame-alignment-requirements.md) |
| cFS 실행, SB, timer, config, event logging | [07-cfs-integration-requirements.md](modules/07-cfs-integration-requirements.md) |
| 모든 요구사항의 시험 및 검증 방법 | [08-verification-plan.md](verification/08-verification-plan.md) |

## 5. 병합 전략

병렬 개발 결과를 충돌 없이 병합하려면 다음 순서를 따른다.

1. 먼저 공유 인터페이스 정의를 고정한다.
2. 각 담당 주체는 자신이 맡은 모듈 문서 안에서만 내부 로직을 수정한다.
3. 모듈 간 계약 변경은 반드시 인터페이스 문서를 통해 반영한다.
4. 통합 영향은 아키텍처 문서에 반영한다.
5. 요구사항이 바뀌면 검증 항목도 함께 추가 또는 수정한다.

## 6. 피해야 할 안티패턴

- 동일한 메시지 필드를 여러 모듈 문서에 중복 정의하지 않는다.
- 서로 다른 모듈 문서에서 좌표계를 독립적으로 다시 정의하지 않는다.
- reconstruction 내부 구현을 cFS integration 문서에 넣지 않는다.
- 추적 가능한 검증 항목 없이 시험 절차만 모듈 문서에 두지 않는다.

## 7. 권장 다음 단계

이 가이드를 기준으로 reconstruction, alignment, cFS integration 요구사항의 경계를 유지하면서 각 문서의 책임을 분리한다.
