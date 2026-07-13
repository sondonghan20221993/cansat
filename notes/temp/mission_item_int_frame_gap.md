# MISSION_ITEM_INT frame 미수정 갭 — ArduPilot 거부 위험 (2026-07-11 도출)

## 문제

`mavlink_bridge_app_utils.c`의 두 미션 업로드 경로가 서로 다른 frame 처리 상태를 가짐:

| 함수 | MAVLink 메시지 | frame | 상태 |
|---|---|---|---|
| `MAVLINK_BRIDGE_APP_SendMissionItem` (line 388) | `MISSION_ITEM`(msg 39, legacy) | `MAV_FRAME_GLOBAL_RELATIVE_ALT` + lat/lon 변환 (`mavlink_bridge_app_behavior_spec.md` §13.1 공식과 일치) | 수정 반영됨 |
| `MAVLINK_BRIDGE_APP_SendMissionItemInt` (line 361) | `MISSION_ITEM_INT`(msg 73) | **`MAV_FRAME_LOCAL_NED`** (원본 미터 좌표 그대로, 변환 없음) | §13.1 수정 미반영 |

`mavlink_bridge_app_behavior_spec.md` §13.1은 실측으로 확인된 제약을 명시함:

> "ArduPilot은 미션 아이템에서 `MAV_FRAME_LOCAL_NED` (= 1)을 거부한다
> (`MISSION_ACK result=2 = MAV_MISSION_UNSUPPORTED_FRAME`). `MAV_FRAME_GLOBAL_RELATIVE_ALT`
> (= 3)를 사용해야 한다."

그런데 §6.1은 INT 경로(`MISSION_REQUEST_INT`→`MISSION_ITEM_INT`)를 **"권장, MAVLink2 /
ArduPilot 4.x+"**로 명시하고 있음 — 즉 실제 ArduPilot 4.x가 요청할 가능성이 가장 높은
경로가, 같은 문서가 "FC가 거부한다"고 확인한 frame을 그대로 보내고 있음.

§6.1 안에 이미 인라인 경고가 있었음(당시엔 미확정 리스크로 기록된 듯):

> "FC가 INT 경로에서 `MISSION_ACK result = MAV_MISSION_UNSUPPORTED_FRAME`을 반환하면,
> INT 경로에도 global frame 변환 적용이 필요하다."

## 왜 문제인가

- FC가 `MISSION_REQUEST_INT`로 요청하는 환경(ArduPilot 4.x+ 다수)에서 미션 업로드 자체가
  `MAV_MISSION_UNSUPPORTED_FRAME`으로 거부될 수 있음.
- Legacy 경로만 실제로 동작하고 INT 경로는 사실상 죽어있는 코드일 가능성.
- `[[route_gps_active_correction]]`(2-pass GPS 보정 기능)의 §3.9/§3.11도 이 업로드 경로에
  의존하므로, 이 버그가 해소 안 되면 FC가 INT 경로를 쓰는 환경에서 보정 route 업로드가
  실패할 수 있음. 단, 원 피팅 계산 자체(로컬 미터 좌표)는 이 문제와 무관 — 업로드
  wire-protocol 변환 단계에서만 영향.

## 관련 항목

- `notes/mavlink_bridge_app_behavior_spec.md` §6.1, §13.1
- `notes/temp/route_gps_active_correction.md` §3.9, §3.11 (armed 업로드/재출발 트리거)

## 상태

- [x] `SendMissionItemInt`에 `SendMissionItem`(legacy)과 동일한 GLOBAL_RELATIVE_ALT +
      lat/lon 변환 적용 (2026-07-13). MISSION_ITEM_INT는 lat/lon을 degE7 int32로
      인코딩(MISSION_ITEM의 float degree와 다름)하는 점만 반영해 별도 구현 —
      `mavlink_bridge_app_utils.c:363-410` (`MAVLINK_BRIDGE_APP_SendMissionItemInt`).
      미션 재조회(download) 로그 파싱 경로(`MISSION_ITEM_INT` 수신, 진단 EVS 로그 전용,
      상태 미반영)도 lat/lon degE7 해석으로 함께 정정 (`mavlink_bridge_app_utils.c` §1522-1541).
      UT 빌드 회귀 없음 (mavlink_bridge_app UT 전체: utils 105, main 14, cmds 4, dispatch 26 PASS) —
      단, 이 두 함수 자체를 직접 겨냥한 UT는 기존에도 없었고 이번에도 추가 안 함
      (static 함수, dispatch 경유 간접 커버만 존재).
- [ ] 실물 FC로 INT 경로 검증 (`MISSION_REQUEST_INT` 요청 여부 확인 + 새 인코딩으로
      `MISSION_ACK result=0 ACCEPTED` 확인) — FC가 다른 작업에 사용 중이라 이 세션에서
      미실행. §13.1의 legacy 경로 실측 결과(GLOBAL_RELATIVE_ALT 허용)에 근거해 코드
      수정은 선반영했으나, INT 경로 자체의 실물 확인은 여전히 잔여.
