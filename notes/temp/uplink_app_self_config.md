# uplink_app 자신의 CONFIG 처리 현황 (2026-07-15)

## 설계 (spec 13.1)

```
uplink_app: 제한적
- 자신의 파라미터(MAX_PAYLOAD, PROTOCOL_VERSION 등) 필요 시만 로컬 버퍼 유지
- 다른 앱 대상 config는 해당 앱 MID로 forward만 수행 (자신이 소유하지 않음)
```

## 현황

### 구현됨 (Forward only)
- ✅ CONFIG_CMD_MID 수신 후 checksum 검증
- ✅ 대상 앱(cfs_core_app/mavlink_bridge_app)으로 forward
- ✅ ConfigPendingState 추적 (forward 상태)

### 미구현 (Self-processing)
- ❌ UPLINK_APP_CONFIG_SCOPE 정의 없음
- ❌ ProcessConfigCommand 없음
- ❌ 자신의 파라미터 런타임 변경 불가

### 런타임 고정 파라미터
```c
#define UPLINK_APP_MAX_PAYLOAD_LENGTH     196
#define UPLINK_APP_PROTOCOL_VERSION       1
#define UPLINK_APP_ROUTE_FLYABLE_X_MIN_M  -50.0f
// ... (모두 컴파일 타임 값)
```

## 설계 의도 해석

**두 가지 해석 가능:**

### 해석 A: 의도된 제한 (현재 상태 유지)
- uplink_app은 자신의 config를 수신하지 않음
- 이유: 비행 중 uplink 프로토콜 변경 위험
- cfs_core_app/mavlink_bridge_app만 런타임 재구성 가능

### 해석 B: 미완성 기능 (구현 필요)
- uplink_app도 자신의 config를 처리해야 함
- 예: MAX_PAYLOAD를 지상국에서 조정
- 예: PROTOCOL_VERSION 호환성 런타임 변경

## 위험 평가

**현재 상태로 문제:**
- 중: uplink 프로토콜 변경 불가 (fw 업데이트만 가능)
- 낮: MAX_PAYLOAD 고정 (보통 충분)

**구현 시 위험:**
- 높: uplink_app 재구성 중 명령 유실
- 해결: ConfigPendingState, active/pending 이중 버퍼 필요

## 다음 단계 (확인 필요)

- [ ] spec 의도 명확화: "제한적"의 범위?
- [ ] 지상국 요구사항: uplink 파라미터 변경 필요?
- [ ] UT 확인: 자신의 config 처리 UT 있는가?
- [ ] 추가 구현: 필요 시 ProcessConfigCommand 추가

## 관련 파일
- uplink_app/fsw/src/uplink_app_utils.c:350 (ForwardConfigCommand)
- uplink_app/config/default_uplink_app_msgstruct.h (파라미터 정의)
- notes/mission_app_runtime_spec.md:§13.1 (설계)
