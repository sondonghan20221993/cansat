# Code Quality Review — NASA Power of 10 Rules

**프로젝트:** cfs-telemetry-app  
**분석 일자:** 2026-06-13  
**참고:** [NASA Power of 10 Rules](https://news.hada.io/topic?id=19260)  
**총 파일 수:** 34개 .c 파일 / 6,716줄

---

## 종합 평가

| 규칙 | 준수도 | 점수 | 비고 |
|------|--------|------|------|
| 1. 단순 제어 흐름 | ⚠️ 부분 | 7/10 | goto 8개 발견 |
| 2. 루프 상한 | ⚠️ 부분 | 6/10 | while(true) 2개, 무한 읽기 루프 1개 |
| 3. 동적 메모리 | ✅ 완전 | 10/10 | malloc/free 없음 |
| 4. 함수 크기 | ❌ 심각 | 2/10 | 313줄 함수 존재 |
| 5. Assert | ❓ 미사용 | 5/10 | 프로젝트 전체 0개 |
| 6. 반환값 검증 | ✅ 대부분 | 8/10 | 라이브러리 함수 제외 |
| 7. 이중 포인터 | ✅ 완전 | 10/10 | 사용 안 함 |
| 8. 함수 포인터 | ✅ 완전 | 10/10 | 사용 안 함 |
| 9. 복잡한 매크로 | ✅ 준수 | 9/10 | 간단함 |
| **평균** | | **7.4/10** | |

---

## 규칙별 상세

### 1. 단순 제어 흐름 (goto 금지)

**위반:** `mavlink_bridge_app/fsw/src/mavlink_bridge_app_utils.c`  
줄 1581, 1586, 1591, 1596, 1601, 1606, 1611, 1631 — `goto reject_value;`

```c
case MAVLINK_BRIDGE_PARAM_ATTITUDE_INTERVAL_US:
    if (Value < MIN || Value > MAX)
        goto reject_value;  // 위반
    ...
reject_value:  // Line 1651
    MAVLINK_BRIDGE_APP_Data.ErrCounter++;
```

### 2. 루프 상한

**위반:**
- `lora_tdm_app/fsw/src/lora_tdm_app.c:89` — `while(true)` (타임아웃 break 있음)
- `lora_tdm_app/fsw/src/lora_tdm_app.c:221` — `while(true)` (명시적 상한 없음)
- `mavlink_bridge_app/fsw/src/mavlink_bridge_app_utils.c:1756` — `while(read(...) > 0)` (상한 없음)

### 3. 동적 메모리

**완전 준수.** malloc / calloc / realloc / free 사용 없음.

### 4. 함수 크기 (60줄 이하)

**위반 함수:**

| 함수명 | 파일 | 줄 수 |
|--------|------|-------|
| MAVLINK_BRIDGE_APP_HandleFrameComplete | mavlink_bridge_app_utils.c | 313 |
| MAVLINK_BRIDGE_APP_ProcessConfigCommand | mavlink_bridge_app_utils.c | 143 |
| MAVLINK_BRIDGE_APP_ProcessReceivedByte | mavlink_bridge_app_utils.c | 112 |
| MAVLINK_BRIDGE_APP_OpenSerial | mavlink_bridge_app_utils.c | 83 |
| MAVLINK_BRIDGE_APP_RequestTelemetryStreams | mavlink_bridge_app_utils.c | 74 |

`HandleFrameComplete`는 13개의 else-if 체인으로 구성 — 메시지 타입별 함수로 분리 필요.

### 5. Assert

**미사용.** 프로젝트 전체에 assert 없음. 입력 검증 및 불변 조건에 추가 권고.

### 6. 반환값 검증

**대부분 준수.** CFE_SB_*, CFE_MSG_* 등 커스텀 함수 반환값은 모두 검증.  
memset/memcpy는 void 반환이므로 검증 불가 — 규칙 위반 아님.

### 7. 이중 포인터

**완전 준수.** `**` 사용 없음.

### 8. 함수 포인터

**완전 준수.** 함수 포인터 및 콜백 없음.

### 9. 복잡한 매크로

**준수.** 대부분 단순 상수 매크로. 다중줄 매크로는 버전 문자열 생성용으로 한정.

---

## 개선 우선순위

### 높음
1. `MAVLINK_BRIDGE_APP_HandleFrameComplete` (313줄) — 메시지별 �핸들러 함수로 분리
2. `goto reject_value` 8개 — if-else 구조로 교체

### 중간
3. `while(true)` 루프에 명시적 반복 상한 추가
4. 주요 함수 입력 검증에 assert 추가

### 낮음
5. 긴 함수(ProcessConfigCommand, ProcessReceivedByte) 추가 분리 검토
