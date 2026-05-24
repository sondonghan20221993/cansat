# cfs-telemetry-app

작동 중인 cFS 통합 환경에서 추출한 `telemetry_app` 백업 저장소이다.

## 구성

- `telemetry_app/`
  - `sample_app` 기반 cFS 앱 구현
  - HK/CMD 처리
  - `TELEMETRY_STATUS_MID`
  - `TELEMETRY_MONITOR_MID`
  - `ALIVE` / `DEGRADED` / `LOST` / 복구 상태 머신
  - 현재 구현 기준으로 정리된 단위 테스트 스캐폴딩
- `tools/telemetry_app_e2e_sender.py`
  - CI_LAB UDP 패스스루 기반 종단 간 검증용 보조 스크립트
- `notes/integration_steps.md`
  - 공식 `nasa/cFS` 작업공간에 다시 통합하기 위한 절차 메모

## 검증 상태

다음 항목은 공식 `nasa/cFS` 작업공간에서 검증되었다.

- 빌드 및 설치 성공
- `telemetry_app.so` 런타임 로드
- 운영 시작
- 모니터 기반 종단 간 상태 전이
- `telemetry_app` 단위 테스트 대상 빌드 및 실행

## 재통합 개요

1. 공식 `nasa/cFS`를 복제하거나 포크한다.
2. `telemetry_app/`을 `apps/telemetry_app`에 복사한다.
3. 미션 또는 샘플 정의에 `telemetry_app`을 등록한다.
4. cFS 작업공간를 빌드하고 설치한다.
5. `tools/telemetry_app_e2e_sender.py`로 E2E 검증을 수행한다.
