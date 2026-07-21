# 저장소 전체 점검 (2026-07-21)

Claude 진행, 정적 스캔 + 테스트 실행 기반 점검. 코드 수정 없음, 기록 목적.

## 요약

임베디드 cFS 기반 UAV 텔레메트리 시스템 치고 문서-코드 동기화, 커밋 품질, 정적 안전성이
이례적으로 잘 관리되고 있음. 발견된 결함은 CRLF 관련 1건(테스트가 이미 잡고 있었음)과
그 원인이 되는 저장소 설정(.gitattributes 부재) 1건.

## 잘 되어 있는 점

- **문서-코드 동기화**: README MID 계약표 + `notes/*_behavior_spec.md`. 커밋 로그에
  "spec↔코드 재감사" 커밋 실존(`21c407a`) — 스펙 드리프트를 능동적으로 점검.
- **커밋 품질**: 378개 커밋, 원인까지 남기는 메시지 스타일
  (예: `48c8d12 lora_tdm_app: SeqEcho 검증 로직의 타이밍 버그 수정`).
- **C 코드 안전성**: `strcpy/strcat/sprintf/gets/system()/malloc` 사용 0건, 정적 할당
  기반. `snprintf`/`memcpy` 위주.
- **테스트**: 앱별 `unit-test/` 존재(`mavlink_bridge_app`, `cfs_core_app`, `uplink_app`,
  `lora_tdm_app`) + Python 테스트 174 passed / 2 failed.

## 발견한 결함

1. **`camera/apply_camera_config.sh`, `camera/verify_camera.sh` 문법 오류(재현 확인)**
   - CRLF 줄바꿈 → `bash -n` 시 `unexpected end of file` / `unexpected token`.
   - CR 제거 후 `bash -n` 통과 확인 → CRLF가 직접 원인.
   - `tests/test_camera_prototype.py::ShellSyntaxTest`가 이미 탐지 중 (회귀 아님, 미해결
     상태로 방치).
2. **`.gitattributes` 부재** — 저장소 전체 289개 소스 파일이 CRLF. `.sh` 7개 전부 CRLF,
   그중 2개만 깨졌고 나머지 5개는 우연히 안전할 뿐. Windows(`D:\`) 편집 → WSL/Pi(Linux)
   실행 워크플로 특성상 재발 가능성 높음.
3. README 8행: FC 기종 전환(ArduPilot→PX4) 관련 미션 업로드 문서가 미검증 상태로 명시돼
   있음 (코드 결함 아님, 기존 TODO 확인 차원).

## 권장 조치

1. `.gitattributes`에 `*.sh text eol=lf` 최소 추가 + `git add --renormalize .`
2. `apply_camera_config.sh` / `verify_camera.sh` 줄바꿈 수정 (실패 테스트 2건 통과시키면 됨)
3. `tools/*.py` 등 다른 런타임 스크립트도 CRLF 여부 재확인 권장 (이번 스캔은 `.sh/.py/.c/.h`만 대상)

## 점검 방법

- `git log`, 커밋 메시지 샘플링
- 앱별 LOC, TODO/FIXME 카운트
- 위험 C 함수(`strcpy/system/malloc` 등) grep
- `python3 -m pytest tests/` 실행 (`pymavlink` 미설치로 `test_mavlink_uart_bridge.py`는 제외)
- 실패 테스트 원인 재현: `bash -n` + CR 제거 후 재검증
