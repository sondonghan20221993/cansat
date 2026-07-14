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

## 상태

- [x] 대상 식별 + 계획
- [x] #1~#6 전수 대조 완료 — #1 외 버그 없음
- [x] 결과 문서화 (본 표)
- [ ] (선택) 재발방지 컨벤션을 spec/CLAUDE.md에 명문화할지 결정 — 미정
