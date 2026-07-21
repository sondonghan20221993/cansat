# 이중 버퍼 토글 — mavlink_bridge_app/lora_tdm_app 검증 완료 (2026-07-21)

## 배경

`notes/config_double_buffer_completed.md`에서 cfs_core_app만 검증됐고
mavlink_bridge_app/lora_tdm_app은 미확인 상태였음. 재검증 완료.

## 결과

| 앱 | Active/Pending/Previous | 토글 로직 | UT |
|---|---|---|---|
| cfs_core_app | ✅ 있음 | ✅ 구현됨 | ✅ 있음 |
| mavlink_bridge_app | ✅ 있음 | ✅ 구현됨 (mavlink_bridge_app_utils.c:1777~1901) | ✅ 있음 (`Test_ProcessConfig_DualBuffer_Activate`, `_Rejected_ActiveUnchanged`) |
| lora_tdm_app | ❌ 없음 | 단일 파라미터(`UseV2Downlink`)만 존재, 즉시 대입 (lora_tdm_app_utils.c:717~725) | 해당 없음 |

## 결론 — 갭 아님, 설계상 문제없음

- mavlink_bridge_app: 완전 구현·테스트 완료 확인. 애초에 갭이 아니었음.
- lora_tdm_app: 이중 버퍼 부재는 결함이 아니라 파라미터 개수 차이에서 오는
  자연스러운 설계 차이.
  - cfs_core/mavlink_bridge는 필드가 여러 개라 필드 간 제약(cross-field
    consistency)이 있을 수 있음 → Pending에 임시 반영 후 전체 조합을 검증하고
    통과해야 Active에 커밋(cFE `CFE_TBL` Load→Validate→Activate 패턴과 동일 계열).
  - lora_tdm_app은 파라미터가 boolean 1개뿐 → 비교할 다른 필드가 없어
    "조합 검증"이라는 개념 자체가 성립 안 함. 유효성 검사(`Value != 0`)는
    이미 그 자리에서 수행 중.
  - 오픈소스 관행과도 일치: PX4/ArduPilot의 단일 파라미터 `PARAM_SET`이나
    cFE `sample_app` 커맨드 핸들러도 스칼라 1개 대입에는 스테이징 없이
    범위 체크 후 즉시 반영.
  - 유일한 실질적 차이는 롤백(이전 값 보관) 부재인데, boolean 토글 특성상
    잘못 적용돼도 반대 값 재전송으로 즉시 복구 가능 → 리스크 낮음.

## 관련
- `notes/config_double_buffer_completed.md`
- `mavlink_bridge_app/fsw/src/mavlink_bridge_app_utils.c:1777~1901`
- `lora_tdm_app/fsw/src/lora_tdm_app_utils.c:670~731`
