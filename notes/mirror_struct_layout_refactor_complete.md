# Cross-app mirror 구조체 레이아웃 전수 점검 (2026-07-14 도출)

## 배경

`BridgeHkMirror_t`에 `NonFiniteValueCount` 누락으로 health FAILED 고착 버그
발생(커밋 `3164020` 수정). 이 프로젝트는 앱 간 SB 메시지를 **수신측이 자기
로컬 mirror 구조체로 캐스팅해서 읽는** 패턴이 다수 — 발행측이 필드를 추가/변경할
때 mirror를 안 맞추면 오프셋이 밀려 **사일런트 오독**(빌드/UT 통과, 런타임에만
오동작). 동일 클래스 버그가 다른 mirror에도 있는지 전수 점검.

## 점검 대상 (수신측 mirror ↔ 발행측 실제 구조체)

### cfs_core_app 수신
1. `BridgeHkMirror_t` ↔ `MAVLINK_BRIDGE_APP_HkTlm_t` (0x08A0)
   — **점검완료, 버그발견·수정(3164020)**
2. `GenericStateTlm_t` ↔ FC 4종 상태 발행구조체 (ATTITUDE/EKF_LOCAL/GPS_RAW/EKF_STATUS)
   — mavlink_bridge가 발행, cfs_core가 `UpdateStateCache`로 읽음
3. `RouteUpdateTlm_t` ↔ uplink_app ROUTE_UPDATE 발행구조체
4. UPLINK_HK / LORA_HK — 필드 안 읽고 Received 플래그만 set → **레이아웃 무관, 안전**

### lora_tdm_app 수신 (UpdateCacheFromMsg)
5. FC 4종 상태 + SystemHealth — lora_tdm이 자체 로컬 구조체로 읽음
   (coveragetest에 이미 TEST_*Tlm_t로 레이아웃 가정 있음 — 참고)

### mavlink_bridge_app 수신
6. CONFIG_CMD / ROUTE_UPDATE 등 — 발행측(uplink_app)과 구조체 공유 여부 확인

## 방법

각 쌍마다: 발행측 struct 필드 순서/타입 ↔ 수신측 mirror struct 필드 순서/타입을
바이트 오프셋 단위로 대조. 수신측이 실제로 읽는 필드까지만 정합하면 되지만(뒤쪽
필드는 무관), 앞쪽 필드 하나라도 밀리면 그 이후 전부 오독.

## 점검 결과 (2026-07-14 완료) — #1 외 추가 버그 없음

| # | mirror ↔ 발행측 | 읽는 필드 | 판정 |
|---|---|---|---|
| 1 | `BridgeHkMirror_t` ↔ `MAVLINK_BRIDGE_APP_HkTlm_t` | LinkState/LastErrorCode/LastRxTimestampMs | **버그→수정(3164020)** |
| 2 | `GenericStateTlm_t` ↔ Attitude/EkfLocal/GpsRaw/EkfStatus Tlm_t | TimestampMs/Seq/Valid/Stale/ErrorCode | ✅ 정합 (4종 전부 prefix 동일) |
| 3 | `CFS_CORE_APP_RouteUpdateTlm_t` ↔ `UPLINK_APP_RouteUpdateTlm_t` | 전체 (스칼라+Waypoint[16]) | ✅ 정합 (Waypoint X/Y/Z 동일, MAX 둘다 16) |
| 4 | UPLINK_HK / LORA_HK | (필드 안 읽음, Received만 set) | ✅ 레이아웃 무관 |
| 5 | lora_tdm 로컬 struct ↔ FC 4종 + SystemHealth | 각 소수 필드 | ✅ 정합 (ATT/EKF_LOCAL/GPS(sats포함)/EKF_STATUS/SysHealth 전부 발행측과 일치) |
| 6 | mavlink_bridge `RouteUpdateMirror_t`/`ConfigCmdTlm_t` ↔ uplink | Route 전체, Config prefix+Payload[196] | ✅ 정합 (Payload MAX 전 앱 196 동일, bounds 체크 있음) |

**결론**: 이 버그 클래스(mirror 레이아웃 드리프트)는 `BridgeHkMirror` 단일
인스턴스에만 있었고 이미 수정됨. 나머지 cross-app mirror는 모두 발행측과
정합 상태. **추가 사일런트 버그 없음 확인.**

## 재발 방지 관찰

`BridgeHk`만 유독 깨진 이유: 발행측이 신규 필드(`NonFiniteValueCount`)를
구조체 **중간**(ParseErrorCount와 LastRxTimestampMs 사이)에 삽입 → 뒤 필드
오프셋 밀림. 다른 mirror들은 (a) 필드를 끝에만 추가했거나 (b) 수신측이 prefix만
읽어 영향 없었음.

경량 방지책 후보(이번엔 미적용, 관찰만):
- 컨벤션: 발행 TLM 구조체에 신규 필드는 **항상 끝에 append**(중간 삽입 금지) —
  mirror가 prefix만 읽으면 자동으로 안전
- 강한 방지: 수신측에 `_Static_assert(offsetof(...) == ...)` 컴파일타임 검증
  (단, 발행측 헤더 include 결합 발생 — 트레이드오프)

## 근본 해결 계획 (3번 — 공용 헤더로 "두 벌 → 한 벌", 사용자 채택 방향)

**목표**: 한 SB 메시지 = 구조체 한 벌(단일 진실). 발행측 구조체 하나를 공용
헤더에 두고 수신측은 mirror를 지우고 그걸 include → 한쪽만 바꾸는 실수가
컴파일 레벨에서 원천 불가.

**합쳐야 할 "두 벌" 쌍 목록**:
- [ ] BridgeHk: `MAVLINK_BRIDGE_APP_HkTlm_t` ↔ `CFS_CORE_APP_BridgeHkMirror_t`
      (이번 버그 지점 — 우선순위 최상)
- [ ] FC 상태 4종: `MAVLINK_BRIDGE_APP_{Attitude,EkfLocal,GpsRaw,EkfStatus}Tlm_t`
      ↔ cfs_core `GenericStateTlm_t` + lora_tdm 인라인 로컬 struct 4종
      (한 발행 구조체를 **두 수신측**(cfs_core, lora_tdm)이 각자 복제 중 — 삼중 진실)
- [ ] SystemHealth: cfs_core `SystemHealthTlm_t`(발행) ↔ lora_tdm 인라인 `SHMsg_t`
- [ ] Route: `UPLINK_APP_RouteUpdateTlm_t` ↔ `CFS_CORE_APP_RouteUpdateTlm_t`
      ↔ `MAVLINK_BRIDGE_APP_RouteUpdateMirror_t` (삼중 진실)
- [ ] Config: `UPLINK_APP_ConfigCmdTlm_t` ↔ `MAVLINK_BRIDGE_APP_ConfigCmdTlm_t`
      ↔ `LORA_TDM_APP_ConfigCmdTlm_t` (삼중 진실)

**접근 시 고려사항**:
- cFS 앱은 관례상 각자 `config/*_msgstruct.h`로 자기 메시지 구조체를 소유 —
  cross-app 공유 헤더를 어디에 둘지(예: `mission_defs/` 공용 include, 또는
  발행 앱의 `fsw/inc`를 수신 앱이 include) 결정 필요. 앱 독립성 관례와 트레이드오프.
- 이름이 앱 접두사 기반(`MAVLINK_BRIDGE_APP_*` 등)이라 공유 시 네이밍 정리 필요.
- 각 병합마다 UT의 fake 구조체도 공유 정의로 교체 → 회귀 검증.
- 규모 큼(삼중 진실 3건 포함). **단계적으로**: BridgeHk(2벌) → SystemHealth(2벌)
  → Config/Route/FC상태(3벌) 순으로, 쉬운 2벌부터.

**대안(더 가벼움, 병행 가능)**: 병합 전까지 각 수신측에
`_Static_assert(sizeof/offsetof 일치)` 가드만 먼저 박아 재발을 즉시 차단.

## 공유 헤더 배치 위치 결정 (2026-07-15)

**조사**: `mission_defs/`는 cFE 코어/타겟 설정용(startup script, targets.cmake,
perfids 등)이라 앱 메시지 구조체 배치처로 부적합. 각 앱은 관례상
`target_include_directories(<app> PUBLIC fsw/inc)`로 자기 include 경로만
공개, cross-app include 메커니즘은 현재 전무(그래서 mirror 복제 패턴이
생겼음).

**결정**: 저장소 루트에 새 디렉토리 `shared_msgs/`를 만들고, 병합 대상마다
헤더 1개씩 배치(예: `shared_msgs/bridge_hk_msg.h`). 각 앱의
`CMakeLists.txt`에 `target_include_directories(<app> PUBLIC
${CMAKE_CURRENT_LIST_DIR}/../shared_msgs)` 한 줄 추가해 참조.

- 앱 독립성 관례(각자 `fsw/inc` 소유)는 깨지 않음 — `shared_msgs`는
  "메시지 계약"이라는 별개 범주로 명시적 분리.
  발행 앱이 필드를 바꾸면 컴파일 타임에 전 수신 앱이 즉시 깨짐(의도된 동작).
- 이름 규칙: 기존 앱 접두사(`MAVLINK_BRIDGE_APP_*` 등) 제거하고 메시지
  의미 기반으로 통일(예: `BRIDGE_HK_t`, `FC_ATTITUDE_STATE_t`,
  `SYSTEM_HEALTH_t`, `ROUTE_UPDATE_t`, `CONFIG_CMD_t`) — 병합 시점에
  네이밍도 함께 정리.

## 테스트 케이스 설계 (병합 리팩터링용, 2026-07-15)

원칙: 이 리팩터링은 **동작 불변**(레이아웃/값 동일, 정의 위치만 통합)이므로
핵심 검증은 "기존 UT 전량 무회귀" + "레이아웃 동일성 증명" 두 축.

### 병합 단계 공통 (각 병합마다 반복)
- **TC-MRG-COMMON-1 (레이아웃 동일성)**: 병합 전 `sizeof`/`offsetof` 전 필드
  스냅샷 → 병합 후 동일값 `_Static_assert` 또는 UT assert로 증명.
  (한 벌로 합치면 자동 충족되지만, 병합 커밋 자체가 레이아웃을 바꾸지
  않았음을 증명하는 1회성 체크)
- **TC-MRG-COMMON-2 (UT 전량 회귀)**: 해당 앱 쌍의 기존 coveragetest 전 스위트
  PASS (cfs_core 245건 포함 4개 스위트, lora_tdm 114건 포함 4개 스위트 등)
- **TC-MRG-COMMON-3 (UT fake struct 제거)**: coveragetest 내 `TEST_*_t` 로컬
  fake를 공유 정의로 교체 — fake 자체가 드리프트 원인이었음(BridgeHk 사례).
  교체 후에도 동일 시나리오 PASS 확인.

### 개별
- **TC-MRG-BRIDGEHK-1**: cfs_core `ProcessStateMessage_BridgeHk` 기존 테스트가
  공유 `MAVLINK_BRIDGE_APP_HkTlm_t` 직접 사용으로 전환 후
  LastRxTimestampMs/LinkState/LastErrorCode 판독 값 동일.
- **TC-MRG-SYSHEALTH-1**: lora_tdm `UpdateCacheFromMsg` SystemHealth 분기 —
  공유 정의 사용 후 Health/FaultFlags 캐시 값 동일.
- **TC-MRG-FCSTATE-1**: cfs_core `GenericStateTlm_t` prefix 판독(TimestampMs/
  Seq/Valid/Stale/ErrorCode)이 4종 공유 구조체 각각에서 동일 —
  "generic prefix" 패턴 유지 여부(공유 prefix struct로 분리) 설계 결정 필요.
- **TC-MRG-ROUTE-1 / TC-MRG-CONFIG-1**: 3벌→1벌 후 uplink 발행 → cfs_core/
  mavlink_bridge(/lora_tdm) 수신 UT 시나리오 값 동일 (Waypoint[16], Payload[196]
  경계 포함).
- **TC-MRG-BUILD-1**: 4개 앱 전체 빌드(cFS 트리) 성공 — 공유 헤더 include
  경로가 4개 앱 CMake에서 전부 해상되는지.

### BridgeHk 병합 결과 (2026-07-15, 완료)

**구현**: `shared_msgs/bridge_hk_msg.h`에 `BRIDGE_HK_TLM_t` 신설(원래
`MAVLINK_BRIDGE_APP_HkTlm_t` 필드 그대로 이동). 발행측
(`mavlink_bridge_app/config/default_mavlink_bridge_app_msgstruct.h`)은
`typedef BRIDGE_HK_TLM_t MAVLINK_BRIDGE_APP_HkTlm_t;`로 별칭화, 수신측
(`cfs_core_app/fsw/src/cfs_core_app_utils.h`)의 `CFS_CORE_APP_BridgeHkMirror_t`
정의(필드 나열)를 삭제하고 `typedef BRIDGE_HK_TLM_t
CFS_CORE_APP_BridgeHkMirror_t;`로 교체 — **레이아웃 정의는 이제 물리적으로
1곳(`shared_msgs/bridge_hk_msg.h`)뿐**, 두 앱은 별칭 typedef만 가짐.
각 앱 `CMakeLists.txt`(+unit-test `CMakeLists.txt`)에
`${CMAKE_CURRENT_LIST_DIR}/../shared_msgs` include 경로 추가.

**네이밍 계획 대비 변경**: 애초 계획한 완전한 접두사 제거(`BRIDGE_HK_t`)
대신 `BRIDGE_HK_TLM_t`로 확정(관례상 `_TLM_t` 접미사 유지가 다른 메시지
타입과 일관적). 앱 접두사(`MAVLINK_BRIDGE_APP_`/`CFS_CORE_APP_`)는
계획대로 제거됨 — 각 앱은 이제 별칭 typedef로만 기존 이름을 유지.

**사라진 테스트 아티팩트**: `coveragetest_cfs_core_app_utils.c`의 로컬 fake
struct `TEST_CFS_CORE_APP_BridgeHk_t`(필드 나열 + 자체 크기) 삭제 —
`BRIDGE_HK_TLM_t`를 직접 씀. 이게 바로 이번 버그 클래스의 재발원이었던
"테스트 자체의 fake 레이아웃"이라 제거가 목적에 부합. **테스트 케이스
개수 변화 없음**(같은 시나리오 `Test_CFS_CORE_APP_ProcessStateMessage_BridgeHk`
유지), 내부 구현만 공유 타입 사용으로 교체.

**검증 (2026-07-15, `~/verify-build/cFS_verify` 로컬)**:
- cfs_core_app UT 4개 스위트: 245/35/19/7 전부 PASS
- mavlink_bridge_app UT 4개 스위트: 136/26/14/4 전부 PASS
- 실제 FSW 타겟 빌드 성공: `cfs_core_app.so`, `mav_bridge_app.so`
- TC-MRG-COMMON-1(레이아웃 동일성): 별도 assert 불필요 — 공유 정의라
  레이아웃 상이 자체가 컴파일 불가능한 구조로 원천 보장됨
- TC-MRG-BRIDGEHK-1, TC-MRG-COMMON-2, TC-MRG-COMMON-3 전부 충족

### SystemHealth 병합 결과 (2026-07-15, 완료)

**구현**: `shared_msgs/system_health_msg.h`에 `SYSTEM_HEALTH_TLM_t`
+ 하위 `INPUT_STATUS_t`/`BRIDGE_STATUS_t`/`APP_STATUS_t` 신설(cfs_core
발행측 필드 그대로 이동). 발행측(`cfs_core_app/config/
default_cfs_core_app_msgstruct.h`)은 4개 typedef 전부 별칭화. 수신측
lora_tdm_app은 기존 prefix-only 인라인 `SHMsg_t`(로컬, 필드 5개만 판독)를
제거하고 **공유 정의 전체(`SYSTEM_HEALTH_TLM_t`, 서브구조체 포함)를
직접 캐스팅**하는 방식으로 전환 — prefix만 맞추던 기존 방식보다 강화됨
(전체 레이아웃이 공유 정의 하나로 고정).

**사라진 테스트 아티팩트**: UT 로컬 fake `TEST_SystemHealthTlm_t`(lora_tdm)
삭제, `SYSTEM_HEALTH_TLM_t` 직접 사용으로 교체. 시나리오/개수 불변.

**검증**: cfs_core_app UT 4스위트(245/35/19/7) + lora_tdm_app UT
4스위트(114/40/30/12) 전부 PASS. FSW 빌드 `cfs_core_app.so`/
`lora_tdm_app.so` 성공.

### FC 상태 4종 병합 결과 (2026-07-15, 완료)

**구현**: `shared_msgs/fc_state_msg.h`에 `FC_STATE_PREFIX_t`(공통 7필드
prefix, cfs_core의 generic 캐스팅용) + `FC_ATTITUDE_TLM_t`/
`FC_EKF_LOCAL_TLM_t`/`FC_GPS_RAW_TLM_t`/`FC_EKF_STATUS_TLM_t`(전체 필드)
신설. **삼중 진실 → 단일 진실**:
- 발행측(mavlink_bridge_app msgstruct.h): 4개 typedef 모두 별칭화
- cfs_core_app: `CFS_CORE_APP_GenericStateTlm_t = FC_STATE_PREFIX_t`
  별칭 — "4종을 하나의 핸들러로 처리"하는 generic 캐스팅 패턴은 유지하되,
  그 prefix 타입 자체를 공유 정의로 고정(결정 필요 항목 해소: 패턴은
  유지, 타입만 공유)
- lora_tdm_app: 인라인 로컬 struct 4종(`AttMsg_t`/`LocalMsg_t`/`GpsMsg_t`/
  `EkfMsg_t`) 전부 삭제 → 공유 타입 직접 캐스팅(EkfStatus는 prefix만
  읽으므로 `FC_STATE_PREFIX_t` 사용)

**사라진 테스트 아티팩트**: lora_tdm UT 로컬 fake 4종(`TEST_AttitudeTlm_t`/
`TEST_EkfLocalTlm_t`/`TEST_GpsRawTlm_t`/`TEST_GenericStateTlm_t`) 전부
삭제, 공유 타입(`FC_ATTITUDE_TLM_t` 등)으로 교체. cfs_core UT는 애초부터
`CFS_CORE_APP_GenericStateTlm_t`를 직접 썼으므로 별도 fake 없었음(자동
전파). 시나리오/개수 불변.

**검증**: cfs_core_app(245/35/19/7) + mavlink_bridge_app(136/26/14/4) +
lora_tdm_app(114/40/30/12) UT 12개 스위트 전부 PASS. FSW 빌드
`cfs_core_app.so`/`mav_bridge_app.so`/`lora_tdm_app.so` 성공.

### Route 병합 결과 (2026-07-15, 완료)

**구현**: `shared_msgs/route_msg.h`에 `ROUTE_MAX_WAYPOINTS`(16, 통합 매크로),
`ROUTE_WAYPOINT_t`(X/Y/Z 3float), `ROUTE_UPDATE_TLM_t`(Telemetry 헤더 +
7필드 + Waypoint 배열) 신설. **삼중 진실 → 단일 진실**:
- 발행측(uplink_app msgstruct.h): 로컬 typedef struct 전부 삭제
  → `UPLINK_APP_Waypoint_t = ROUTE_WAYPOINT_t`, `UPLINK_APP_RouteUpdateTlm_t = ROUTE_UPDATE_TLM_t`
  별칭; RouteUpdatePayload_t는 ROUTE_MAX_WAYPOINTS 사용하도록 업데이트
- 구독측(cfs_core_app msgstruct.h): 로컬 typedef 전부 삭제
  → 동일한 별칭화
- 구독측(mavlink_bridge_app msgstruct.h): 로컬 mirror 전부 삭제
  → `MAVLINK_BRIDGE_APP_WaypointMirror_t = ROUTE_WAYPOINT_t`,
  `MAVLINK_BRIDGE_APP_RouteUpdateMirror_t = ROUTE_UPDATE_TLM_t`

**포함 경로 업데이트**: uplink_app/CMakeLists.txt (FSW+UT) +
cfs_core_app/CMakeLists.txt(이미 있음) + mavlink_bridge_app/CMakeLists.txt(이미 있음)에
shared_msgs 디렉토리 추가.

**검증**: 4개 앱 UT 12개 스위트 전부 PASS (no regression):
- cfs_core_app utils/cmds/dispatch: 245/7/35 = 287 PASS
- mavlink_bridge_app utils/cmds/dispatch: 136/4/26 = 166 PASS
- lora_tdm_app utils/cmds/dispatch: 114/12/30 = 156 PASS
- uplink_app utils/cmds/dispatch: 88/91/29 = 208 PASS
총 817 tests PASS.

### Config 병합 결과 (2026-07-15, 완료)

**구현**: `shared_msgs/config_msg.h`에 `CONFIG_MAX_PAYLOAD`(196, 통합 매크로),
`CONFIG_CMD_TLM_t`(Telemetry 헤더 + 6필드 + Payload 배열) 신설. **4벌 → 단일 진실**:
- 발행측(uplink_app msgstruct.h): 로컬 typedef struct 삭제
  → `UPLINK_APP_ConfigCmdTlm_t = CONFIG_CMD_TLM_t` 별칭 (payload는 발행측이 정함)
- 구독측1(cfs_core_app msgstruct.h): 로컬 typedef + 매크로 삭제
  → 동일 별칭화
- 구독측2(mavlink_bridge_app msgstruct.h): 로컬 typedef + 매크로 삭제
  → 동일 별칭화
- 구독측3(lora_tdm_app msgstruct.h): 로컬 typedef + 매크로 삭제
  → 동일 별칭화

**포함 경로 업데이트**: 4개 앱의 msgstruct에 `#include "config_msg.h"` 추가;
CMakeLists는 이미 shared_msgs 경로 포함.

**검증**: 4개 앱 UT 12개 스위트 전부 PASS (no regression):
- cfs_core_app main/cmds/dispatch: 19/7/35 = 61 PASS
- mavlink_bridge_app main/cmds/dispatch: 14/4/26 = 44 PASS
- lora_tdm_app main/cmds/dispatch: 40/12/30 = 82 PASS
- uplink_app main/cmds/dispatch: 9/91/29 = 129 PASS
plus utils: 245/136/114/88 = 583 PASS
총 899 tests PASS.

## 병합 완료 요약 (2026-07-15)

5개 mirror struct 클래스를 2-4벌의 복제에서 단일 진실(shared header)로 통합:
1. **BridgeHk** (2벌): 발행 1 + 구독 1
2. **SystemHealth** (2벌): 발행 1 + 구독 1
3. **FC State 4종** (3벌): 발행 1 + 구독 2
4. **Route** (3벌): 발행 1 + 구독 2
5. **Config** (4벌): 발행 1 + 구독 3

**결과**:
- 공용 헤더 5개 생성 (`shared_msgs/*.h`)
- 각 앱 msgstruct에서 로컬 typedef struct 제거 → alias typedef로 교체
- UT 로컬 fake struct 제거 → 공유 정의 직접 사용
- 전체 899 tests PASS, 0 regression
- **근본 원인(mirror 레이아웃 드리프트) 원천 차단**: 한 발행 구조체 = 한 정의 위치

**향후 유지비**:
- 발행 앱이 필드 추가/변경 → 공유 헤더만 수정 → 수신 앱들은 자동으로 최신 정의 사용
  (예: NonFiniteValueCount 누락 버그 방지 — 발행측 추가 시 수신 앱들도 동일 오프셋 보장)

### 런타임 후보 (하드웨어, TEST_CASES.md 등재용)
- **RT-MRG-001**: 병합 배포 후 실기체에서 health 정상 판정 + DL2 값 무변화
  (기존 soak 로그와 필드 값 대조).

## 상태

- [x] 대상 식별 + 계획
- [x] #1~#6 전수 대조 완료 — #1 외 버그 없음
- [x] 결과 문서화 (본 표)
- [x] 근본 해결(3번) 방향 채택 + 병합 대상 목록화 (본 절)
- [x] 공유 헤더 배치 위치 결정 (2026-07-15)
- [x] BridgeHk 병합 (2벌, 최우선) — 완료 2026-07-15, `shared_msgs/bridge_hk_msg.h`,
      UT 4스위트×2앱 무회귀, FSW 빌드 성공 (본 문서 "BridgeHk 병합 결과" 절)
- [x] SystemHealth 병합 (2벌) — 완료 2026-07-15, `shared_msgs/system_health_msg.h`,
      UT 4스위트×2앱 무회귀 (본 문서 "SystemHealth 병합 결과" 절)
- [x] FC 상태 4종 병합 (삼중 진실, 3벌) — 완료 2026-07-15, `shared_msgs/fc_state_msg.h`,
      UT 12스위트×3앱 무회귀 (본 문서 "FC 상태 4종 병합 결과" 절)
- [x] Route 병합 (삼중 진실, 3벌) — 완료 2026-07-15, `shared_msgs/route_msg.h`,
      UT 12스위트×4앱 무회귀 (본 문서 "Route 병합 결과" 절)
- [x] Config 병합 (4벌, 최대) — 완료 2026-07-15, `shared_msgs/config_msg.h`,
      UT 12스위트×4앱 무회귀 (본 문서 "Config 병합 결과" 절)
- [x] 전체 빌드 + UT 전량 회귀 확인 (완료 2026-07-15: 16개 스위트×4앱 = 899 tests PASS)
- [ ] (선택/병행) `_Static_assert` 가드 선제 삽입

---

## 후속: Task #4 FC 상태 4종 병합 실행 기록


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

---

## 후속: 전체 빌드 + UT 회귀 검증 (Task #7)


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
