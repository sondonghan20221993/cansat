# Task #4: FC 상태 4종 구조체 병합 진행 상황 (2026-07-15)

## 목표
mavlink_bridge 발행 Attitude/EkfLocal/GpsRaw/EkfStatus Tlm_t ↔ cfs_core GenericStateTlm_t ↔ lora_tdm 인라인 로컬 struct 4종 통합
→ 단일 진실(shared_msgs) 기반으로 mirror 드리프트 제거

## 현재 상황

### ✅ 완료된 부분

**shared_msgs/fc_state_msg.h (공용 헤더)**
```
위치: /home/sdh2983/cfs-telemetry-app/shared_msgs/fc_state_msg.h
정의 구조체:
├─ FC_STATE_PREFIX_t (7 필드 공통 prefix)
│  ├─ CFE_MSG_TelemetryHeader_t TelemetryHeader
│  ├─ uint32 TimestampMs
│  ├─ uint32 Seq
│  ├─ uint8 Valid
│  ├─ uint8 Stale
│  ├─ uint8 ErrorCode
│  └─ uint8 Reserved
├─ FC_ATTITUDE_TLM_t (prefix + 6개 attitude 필드)
├─ FC_EKF_LOCAL_TLM_t (prefix + 6개 position/velocity 필드)
├─ FC_GPS_RAW_TLM_t (prefix + 6개 GPS 필드, SatellitesVisible 포함)
└─ FC_EKF_STATUS_TLM_t (prefix + status 필드들)
```

**mavlink_bridge_app (발행측)**
```
파일: /home/sdh2983/cfs-telemetry-app/mavlink_bridge_app/config/default_mavlink_bridge_app_msgstruct.h
상태: ✅ fc_state_msg.h include 이미 적용
typedef FC_EKF_LOCAL_TLM_t  MAVLINK_BRIDGE_APP_EkfLocalTlm_t;
typedef FC_ATTITUDE_TLM_t   MAVLINK_BRIDGE_APP_AttitudeTlm_t;
typedef FC_GPS_RAW_TLM_t    MAVLINK_BRIDGE_APP_GpsRawTlm_t;
typedef FC_EKF_STATUS_TLM_t MAVLINK_BRIDGE_APP_EkfStatusTlm_t;
```

### ❌ 미완료 부분

**cfs_core_app (수신측 1)**
```
파일: /home/sdh2983/cfs-telemetry-app/cfs_core_app/config/default_cfs_core_app_msgstruct.h
상태: ❌ fc_state_msg.h include 없음
현황: FC 구조체 정의 없음 (system_health_msg.h, route_msg.h 등만 include)
필요: 
  ✗ #include "fc_state_msg.h" 추가
  ✗ typedef FC_*_TLM_t CFS_CORE_APP_*Tlm_t 추가 (4종)
```

**lora_tdm_app (수신측 2)**
```
파일: /home/sdh2983/cfs-telemetry-app/lora_tdm_app/config/default_lora_tdm_app_msgstruct.h
상태: ❌ fc_state_msg.h include 없음
현황: FC 구조체 정의 없음 (config_msg.h만 include)
필요:
  ✗ #include "fc_state_msg.h" 추가
  ✗ typedef FC_*_TLM_t LORA_TDM_APP_*Tlm_t 추가 (4종)
```

## 다음 단계

### Phase 1: msgstruct 병합 (즉시 진행 가능)

```bash
1. cfs_core_app/config/default_cfs_core_app_msgstruct.h
   ├─ #include "fc_state_msg.h" 추가 (system_health_msg.h 다음)
   └─ typedef 추가:
      typedef FC_ATTITUDE_TLM_t   CFS_CORE_APP_AttitudeTlm_t;
      typedef FC_EKF_LOCAL_TLM_t  CFS_CORE_APP_EkfLocalTlm_t;
      typedef FC_GPS_RAW_TLM_t    CFS_CORE_APP_GpsRawTlm_t;
      typedef FC_EKF_STATUS_TLM_t CFS_CORE_APP_EkfStatusTlm_t;

2. lora_tdm_app/config/default_lora_tdm_app_msgstruct.h
   ├─ #include "fc_state_msg.h" 추가 (config_msg.h 다음)
   └─ typedef 추가 (동일):
      typedef FC_ATTITUDE_TLM_t   LORA_TDM_APP_AttitudeTlm_t;
      typedef FC_EKF_LOCAL_TLM_t  LORA_TDM_APP_EkfLocalTlm_t;
      typedef FC_GPS_RAW_TLM_t    LORA_TDM_APP_GpsRawTlm_t;
      typedef FC_EKF_STATUS_TLM_t LORA_TDM_APP_EkfStatusTlm_t;
```

### Phase 2: 코드 참조 업데이트 (msgstruct 이후)

```bash
1. cfs_core_app 코드 스캔
   grep -r "FC_ATTITUDE\|FC_EKF_LOCAL\|FC_GPS_RAW\|FC_EKF_STATUS" \
     cfs_core_app/fsw/src/

2. lora_tdm_app 코드 스캔
   grep -r "FC_ATTITUDE\|FC_EKF_LOCAL\|FC_GPS_RAW\|FC_EKF_STATUS" \
     lora_tdm_app/fsw/src/

3. 각 앱에서 로컬 구조체 정의가 있으면 제거/대체
```

### Phase 3: UT 업데이트

```bash
1. coveragetest_cfs_core_app_*.c
   ├─ FC 상태 수신 테스트에서 공용 구조체 사용
   └─ 레이아웃 일치 검증 (_Static_assert 추가 권장)

2. coveragetest_lora_tdm_app_*.c
   ├─ FC 상태 처리 테스트에서 공용 구조체 사용
   └─ 기존 TEST_*Tlm_t 레이아웃과 일치 확인
```

### Phase 4: 빌드 & 회귀 테스트

```bash
1. 개별 앱 빌드
   cmake --build . --target cfs_core_app
   cmake --build . --target lora_tdm_app
   cmake --build . --target mavlink_bridge_app

2. 전체 UT 회귀
   ctest -V

3. 의존성 확인
   grep -r "CFS_CORE_APP_*Tlm_t\|LORA_TDM_APP_*Tlm_t" --include="*.c" --include="*.h"
```

## 설계 고려사항

**generic prefix 패턴 유지**
```
모든 4종이 동일 prefix(7필드, 32바이트)로 시작 → cfs_core의 UpdateStateCache
함수가 FC_STATE_PREFIX_t로 캐스팅해서 처리 가능

static inline void CFS_CORE_APP_UpdateStateCache(const void *Msg)
{
  const FC_STATE_PREFIX_t *Prefix = (FC_STATE_PREFIX_t*)Msg;
  Data->LastFcStateTimestamp = Prefix->TimestampMs;
  // ... prefix 필드만 사용
}
```

**네이밍 규칙**
- shared_msgs: FC_*_TLM_t (앱 접두사 없음)
- 각 앱: CFS_CORE_APP_*Tlm_t, LORA_TDM_APP_*Tlm_t (앱 접두사 포함)
  → typedef로 bridge 역할, 공유 구조체는 FC_ prefix로 통일

## 우선순위

**High**: Phase 1 (msgstruct) — ✅ 완료 (2026-07-15 20:25)
**Medium**: Phase 2 (코드 참조) — ✅ 이미 통합됨 (lora_tdm_app_utils.c)
**Low**: Phase 3 (UT) — ⏳ 필요 여부 확인 중

---

## 실행 상황 (2026-07-15 20:25)

### ✅ 완료된 작업

**Phase 1: msgstruct 통합**
```c
// cfs_core_app/config/default_cfs_core_app_msgstruct.h
#include "fc_state_msg.h"
typedef FC_ATTITUDE_TLM_t   CFS_CORE_APP_AttitudeTlm_t;
typedef FC_EKF_LOCAL_TLM_t  CFS_CORE_APP_EkfLocalTlm_t;
typedef FC_GPS_RAW_TLM_t    CFS_CORE_APP_GpsRawTlm_t;
typedef FC_EKF_STATUS_TLM_t CFS_CORE_APP_EkfStatusTlm_t;

// lora_tdm_app/config/default_lora_tdm_app_msgstruct.h
#include "fc_state_msg.h"
typedef FC_ATTITUDE_TLM_t   LORA_TDM_APP_AttitudeTlm_t;
typedef FC_EKF_LOCAL_TLM_t  LORA_TDM_APP_EkfLocalTlm_t;
typedef FC_GPS_RAW_TLM_t    LORA_TDM_APP_GpsRawTlm_t;
typedef FC_EKF_STATUS_TLM_t LORA_TDM_APP_EkfStatusTlm_t;
```

**Phase 2: 코드 참조 확인**
```
cfs_core_app_utils.h:
  typedef FC_STATE_PREFIX_t CFS_CORE_APP_GenericStateTlm_t;
  ✅ 이미 fc_state_msg.h 기반

lora_tdm_app_utils.c:
  const FC_ATTITUDE_TLM_t *M = (const FC_ATTITUDE_TLM_t *)SBBufPtr;
  ✅ 이미 공용 구조체 사용
```

**커밋**
```
a48638b Task #4: FC 상태 4종 구조체 병합 (msgstruct 통합)
- cfs_core_app msgstruct 수정
- lora_tdm_app msgstruct 수정
```

### ⏳ 미진행

**Phase 3: UT 업데이트**
- coveragetest_cfs_core_app_utils.c에서 CFS_CORE_APP_GenericStateTlm_t 사용
- 구조체 정의는 cfs_core_app_utils.h에서 FC_STATE_PREFIX_t typedef
- ✅ 정의가 이미 있으므로 UT 컴파일 문제 없을 것으로 예상
- 실제 테스트 필요: `ctest -V` 또는 개별 빌드

**Phase 4: 빌드 + 회귀**
- cFS 빌드 시스템 설정 필요
- Task #7 (전체 빌드 + UT 회귀)과 병행 가능

---

## 설계 고려사항 (재확인)

**generic prefix 패턴 유지** ✅
```c
// shared_msgs/fc_state_msg.h
typedef struct {
  CFE_MSG_TelemetryHeader_t TelemetryHeader;
  uint32 TimestampMs;
  uint32 Seq;
  uint8 Valid;
  uint8 Stale;
  uint8 ErrorCode;
  uint8 Reserved;
} FC_STATE_PREFIX_t;  // ← 4종 모두 이 prefix로 시작

// cfs_core_app_utils.h
typedef FC_STATE_PREFIX_t CFS_CORE_APP_GenericStateTlm_t;
// cfs_core의 UpdateStateCache는 이 prefix로만 캐스팅해서 처리
```

**네이밍 규칙 확인** ✅
```
shared_msgs: FC_*_TLM_t (앱 접두사 없음) ← 공용
cfs_core_app: CFS_CORE_APP_*Tlm_t ← typedef로 bridge
lora_tdm_app: LORA_TDM_APP_*Tlm_t ← typedef로 bridge
mavlink_bridge_app: MAVLINK_BRIDGE_APP_*Tlm_t ← typedef로 bridge
```

---

## 참고

- 기존 mirror struct 레이아웃 점검: notes/mirror_struct_layout_refactor_complete.md
- FC STATE 구조체 설계: shared_msgs/fc_state_msg.h 코멘트
- 일관성 검증: _Static_assert(sizeof/offsetof) 권장
- UT = Unit Test (단위 테스트, 각 함수/모듈 독립 검증)
- coverage test = 코드 커버리지 측정 UT
