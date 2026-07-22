# route update 2-pass GPS 능동 보정 — 착수 범위 정리 (2026-07-22)

## 배경

BL-10(VIEWPOINT_CMD 캐시 미사용) 논의 중, 사용자가 별도로 요청한 기능
("기체가 원형으로 이동, GPS 실측과 계획 원형의 차이를 계산해 waypoint를
보정")이 이미 spec에 상세 설계돼 있음을 확인:

**`notes/mission_app_runtime_spec.md` §18.4.6.2.1 "route update — 2-pass
GPS 능동 보정" (2026-07-11 설계 확정)**

BL-10(VIEWPOINT_CMD)과는 무관한 별개 기능 — VIEWPOINT_CMD는 §18.4.6.3,
이 보정 기능은 §18.4.6.2.1(route update REPLACE의 확장)이라 혼동 주의.

## 설계 요약 (전문은 spec 참조)

- REPLACE route update의 `reserved` 필드를 2-pass 보정 활성화 플래그로 사용
- 상태머신: `IDLE → LAP1_ACTIVE → CORRECTING → LAP2_ACTIVE → DONE` (총 2랩 고정)
- lap 1: `LOCAL_POSITION_NED`(200ms) 실측 스트림을 정적 원형 버퍼(최대 1500
  샘플)에 축적. 마지막 waypoint는 `MAV_CMD_NAV_LOITER_UNLIM`으로 전송(확정적
  호버링). 완료 판정은 FC의 `MISSION_ITEM_REACHED`.
- CORRECTING: 실측 스트림 + 계획 waypoint 각각에 최소자승 원 피팅 →
  실측(cx,cy,r) vs 계획(cx_plan,cy_plan) → 각 waypoint 각도
  `θ_i = atan2(Y_i−cy_plan, X_i−cx_plan)` → 보정 waypoint
  `(cx+r·cosθ_i, cy+r·sinθ_i)` (고도는 원본 유지, 수평만 보정)
- 유효 샘플 3개 미만 또는 보정 route 검증 실패 시: 원본(lap 1) route로 lap 2
  재비행(폴백, reject/중단 없음)
- LAP2 전이: 보정 route 재업로드 → `MISSION_ACK` accepted 확인 →
  `MISSION_SET_CURRENT(0)`으로 호버링 해제(온보드 자동, 지상국 경유 안 함)
- 출력: 랩 번호/보정 성공 여부/(cx,cy,r)를 `UPLINK_STATUS_MID`에 반영

## 구현 현황

**설계만 존재, 코드 0%** — `LAP1_ACTIVE`/`CORRECTING`/원 피팅 관련 코드
grep 결과 전무 확인(2026-07-22).

## 담당 앱 미확정 (spec에 명시 약함 — 착수 전 확인 필요)

- 원 피팅 계산 + 상태머신: `cfs_core_app`(route 관리 주체로 보임) 또는
  `uplink_app`(route update 1차 검증 주체) 중 어디가 맞는지 spec 재확인 필요
- `LOCAL_POSITION_NED` 구독은 `mavlink_bridge_app`이 FC 상태 게시 주체라
  그쪽 캐시를 참조하는 형태일 가능성 — 크로스앱 데이터 흐름 확인 필요
- FC로의 `MISSION_SET_CURRENT`/재업로드 트리거는 `mavlink_bridge_app`이
  전송 주체일 것으로 보임(기존 route update 업로드 경로와 동일)

## 제안했던 착수 단계 (사용자 승인 대기, 미착수)

1. 원 피팅 수학(최소자승 circle fit) — FC 의존 없는 순수 계산, 단독 UT 가능
2. lap 1 데이터 수집 — `LOCAL_POSITION_NED` 구독 + 정적 원형 버퍼
3. 상태머신 — `IDLE→LAP1_ACTIVE→CORRECTING→LAP2_ACTIVE→DONE`, reserved
   플래그, `MISSION_ITEM_REACHED` 감지
4. 보정 route 재업로드 + FC 트리거(`LOITER_UNLIM`, `MISSION_SET_CURRENT`,
   armed 상태 업로드 허용)

## 상태

- [ ] 담당 앱 확정 (spec 재확인 또는 사용자 결정)
- [ ] 1~4단계 착수 여부/순서 확정
- 이번 세션에서는 착수하지 않고 범위만 기록 — BL-10과 혼동하지 않도록 별도
  백로그 ID 부여 필요(BACKLOG.md에 아직 미등재, 다음 정리 시 추가)
