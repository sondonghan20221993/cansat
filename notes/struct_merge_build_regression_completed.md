# Task #7: 전체 빌드 + UT 전량 회귀 확인 결과 (2026-07-15)

## 빌드 환경
```
cFS 프레임워크: ~/cFS_clean (native UT 빌드)
소스: ~/cfs-telemetry-app
동기화 대상: uplink_app, cfs_core_app, mavlink_bridge_app, lora_tdm_app, shared_msgs
```

## 발견한 문제: 동기화 불완전

`cp -r`로 최초 동기화했으나 일부 파일이 실제로 덮어써지지 않음
(uplink_app_utils.c 등 다수 파일이 6월 시점 stale 버전으로 남아있었음).

**증상**: uplink_app_utils UT에서 `UPLINK_APP_ParseLoRaFrame` 테스트
4건 실패 — 이 함수/테스트는 현재 소스 저장소에 존재하지 않는
과거(cFS_clean 로컬) 전용 코드였음.

**원인**: `cp -r src/ dst/` 형태가 예상과 다르게 일부 파일을 덮어쓰지
못함(정확한 원인 미확정, WSL 파일시스템 관련 가능성).

**해결**: `rsync -av --delete`로 완전 재동기화 → 소스와 100% 일치 확보.
이후 `cmake .` 재실행(캐시된 CMakeLists 반영) → 재빌드 → 전량 PASS.

## 최종 빌드 결과

```
✅ 4개 앱 전체 빌드 성공 (컴파일 에러 0건)
✅ shared_msgs include 정상 동작 확인
   (fc_state_msg.h, route_msg.h, config_msg.h, system_health_msg.h,
    bridge_hk_msg.h 전부 정상 include)
```

## UT 회귀 결과 (16개 테스트 러너 전량 PASS)

| 앱 | 모듈 | TOTAL | PASS | FAIL |
|---|---|---|---|---|
| lora_tdm_app | cmds | 12 | 12 | 0 |
| lora_tdm_app | app | 40 | 40 | 0 |
| lora_tdm_app | utils | 114 | 114 | 0 |
| lora_tdm_app | dispatch | 30 | 30 | 0 |
| uplink_app | cmds | 91 | 91 | 0 |
| uplink_app | dispatch | 29 | 29 | 0 |
| uplink_app | app | 9 | 9 | 0 |
| uplink_app | utils | 88 | 88 | 0 |
| cfs_core_app | cmds | 7 | 7 | 0 |
| cfs_core_app | utils | 245 | 245 | 0 |
| cfs_core_app | dispatch | 35 | 35 | 0 |
| cfs_core_app | app | 19 | 19 | 0 |
| mavlink_bridge_app | cmds | 4 | 4 | 0 |
| mavlink_bridge_app | app | 14 | 14 | 0 |
| mavlink_bridge_app | utils | 136 | 136 | 0 |
| mavlink_bridge_app | dispatch | 26 | 26 | 0 |
| **합계** | | **899** | **899** | **0** |

## 결론

**구조체 병합(Task #2~#6) 관련 회귀 없음.**
- msgstruct typedef 변경(로컬→공용 헤더)은 바이트 레이아웃 불변이므로
  UT 코드 수정 불필요, 재컴파일만으로 전량 통과 확인.
- 유일한 실패는 병합과 무관한 로컬 동기화 문제(위 설명)였고
  재동기화 후 해결됨.

## 교훈: 향후 동기화 시 `rsync --delete` 권장

`cp -r`은 소스에서 삭제/변경된 파일을 dst에서 확실히 반영 안 할 수 있음.
`rsync -av --delete`가 소스-dst 완전 일치를 보장하므로 이후
cFS_clean 동기화 작업 시 기본으로 사용 권장.
