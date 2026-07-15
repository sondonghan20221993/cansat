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
- [x] BridgeHk 병합 UT 회귀 확인 (완료, 나머지 병합은 각자 진행 시 확인)
- [ ] (선택/병행) `_Static_assert` 가드 선제 삽입
