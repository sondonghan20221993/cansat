# 이중 버퍼 토글 — mavlink_bridge_app/lora_tdm_app 검증 미실행

## 배경

`notes/config_double_buffer_completed.md`에서 구현 현황을 정리했는데:

| 앱 | 상태 |
|---|---|
| cfs_core_app | ✅ 완전 구현 (토글 로직 확인) |
| mavlink_bridge_app | ❓ 미확인 (구현 여부 확인 필요) |
| lora_tdm_app | ❓ 미확인 (구현 여부 확인 필요) |

현재 문서는 "_completed"로 분류되었지만 **마지막 두 앱의 실제 코드 검증이 아직 미실행**.

## 해야 할 일

- [ ] `mavlink_bridge_app`의 config 이중 버퍼 구조 확인
  - Active/Pending/Previous 세 버퍼 존재 여부
  - 토글 로직 (ProcessConfigCommand 경로)
  - 상태 머신 구현 확인
  
- [ ] `lora_tdm_app`의 config 이중 버퍼 구조 확인
  - Active/Pending/Previous 세 버퍼 존재 여부
  - 토글 로직 (ProcessConfigCommand 경로)
  - 상태 머신 구현 확인

- [ ] 필요 시 두 앱의 구현 보충 (cfs_core_app 패턴 참조)

## 상태

- [x] cfs_core_app 검증됨
- [ ] mavlink_bridge_app 코드 읽기
- [ ] lora_tdm_app 코드 읽기
