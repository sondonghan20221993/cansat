# cfs_core_app

`cfs_core_app` 스캐폴드이다.

이 앱은 `mavlink_bridge_app`와 다른 상태 입력의 결과를 종합해 `SYSTEM_HEALTH_MID`를 게시하는 상위 관리 계층으로 확장하기 위한 시작점이다. 현재 구현은 최소 HK 및 상태 텔레메트리 발행 구조만 포함한다.

현재 구현 기준 동작 명세와 검증 절차는 [notes/cfs_core_app_behavior_spec.md](/C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/notes/cfs_core_app_behavior_spec.md)에 정리되어 있다.

