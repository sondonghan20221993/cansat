# [TEMP] Route GPS 오차 능동 보정 (작업 노트)

> 상태: 설계 확정 (2026-07-11) / 스펙·코드 반영 대기
> 작성: 2026-07-07
> 성격: §18.4.6.2 등 스펙 본문 반영 전 최종 설계. 미결정 사항 전부 확정됨(§5).

## 1. 한 줄 요약

수신 route를 GPS 오차 기준으로 **단순 검증·수정**하고 끝내는 게 아니라,
직전 실측 위치와의 편차를 반영해 **능동적으로 route를 보정하는 과정**을 추가한다.
→ 단순 1-step 보정이 아니라 조금 긴 과정이 될 예정.

## 2. 관련 레포·데이터 흐름 (파악한 것)

```
[optimalpath / 지상]                 [cfs-telemetry-app / 기체 탑재]
PB-NBV가 route(waypoint) 생성  ──route update──▶ uplink_app
(drone_runner_node.cpp)          (0x190B)         (검증: waypoint 1~16,
       ▲                                          flyable area, 고도 2~8m,
       │                                          segment 2m)
       │                                              │
       │                                         cfs_core_app (route cache 소비)
       │                                              │  FC가 경로 비행
       │                                         mavlink_bridge_app
       └──실측 pose(편차)◀──downlink──── LOCAL_POSITION_NED(msg32)
                                          / LatE7·LonE7 = 실제 도달 위치
```

- 계획 위치 = optimalpath NBV waypoint (→ uplink_app route로 업로드)
- 실측 위치 = `mavlink_bridge_app`의 `LOCAL_POSITION_NED`
- 편차 = 계획 waypoint − 실측 pose

## 3. 범위 (현재 결정)

- **재계획(NBV 재계산) 아님.** 무거운 PB-NBV 루프는 건드리지 않는다.
- 기능 본체는 **cfs-telemetry-app `uplink_app`의 route 검증/보정 단계**.
- optimalpath는 원본 route 생성만 담당. `drone_runner_node.cpp:87`
  `current_pose = nbv`는 이번 범위에서 수정 대상 아님.
- **새 앱 아님.** 기존 4개 앱(mavlink_bridge/cfs_core/uplink/lora_tdm) 책임 경계상
  route 처리는 이미 uplink_app 소유. 새 앱을 만들면 route 소유권이 갈라지고
  SB 메시지/MID/EDS를 새로 만들어야 하는데 얻는 이득이 없음.

## 3.1 미션 구조 — 2-pass (원형 촬영 → 오차 보정 → 재비행) (2026-07-10 확정)

```
1바퀴: 업로드된 route(원형 촬영 경로) 비행
  → 각 waypoint 도달 시 실측 pose vs 계획 waypoint 편차 기록
  → 마지막 waypoint 도달 = "1바퀴 완료" (판정 기준, 아래 §3.2)
2바퀴 준비: uplink_app이 온보드에서 기록된 편차로 보정 route 계산 (아래 §3.3/§3.4)
2바퀴: 보정된 route로 동일 경로 재비행
```

- **총 랩 수 = 2로 고정.** 2바퀴 완료 후 추가 보정·재비행 없음 (3바퀴 이상 반복 없음).

- **랩 완료 판정**: route의 **마지막 waypoint 도달**(허용반경 이내)로 판정.
  지상 명시 명령이나 시간/거리 기준은 채택하지 않음 — 온보드에서 자기완결적으로 판단 가능해야
  지상 링크 상태와 무관하게 동작하기 때문.
- **보정 계산 주체**: **온보드(uplink_app)**. 지상 재계산 후 재업로드는 LoRa 반이중 TDM
  (300ms RX 윈도우) 왕복 지연이 커서 "1바퀴 끝나자마자 2바퀴 시작"에 부적합.
  구간별 보간에 필요한 연산은 가벼워 온보드 처리로 충분.
- **보정 방식**: **구간별 보간 보정**. 1바퀴 동안 각 waypoint에서 기록한 편차(복수)를
  구간(segment)별로 보간해 적용 — 단일 평행이동(uniform shift)보다 정확하지만
  구현 복잡도는 더 높음. (아래 §3.4에서 구체화 필요)
- **보정 축 = X/Y(수평)만. 고도(Z)는 유지** (2026-07-10 확정). 원형 촬영 경로는
  고정 고도를 도는 것이 전제이므로, GPS 편차 보정은 수평 위치에만 적용하고
  Z는 1바퀴 원본 waypoint 값을 그대로 사용한다. → 고도는 애초에 안 바뀌므로
  보정 후 altitude 재검증(2~8m)은 항상 통과(원본이 이미 검증 통과한 값이므로).

## 4. "능동적 보정 과정" — 단계 (갱신)

1. 1바퀴 비행 중 각 waypoint 도달 시 실측 pose(직전 downlink) vs 계획 waypoint 편차를 기록
   (신선도 판단 포함 — 몇 초 이내만 유효로 볼지는 §5 미결정)
2. 마지막 waypoint 도달 시 "1바퀴 완료" 판정, 기록된 편차 목록으로 보정 단계 진입
3. 구간별 보간으로 2바퀴 route 계산 (계산식은 §3.4 확정 필요)
4. 보정된 route 재검증 (flyable area(X/Y)만 실질 대상 — altitude는 Z 미변경으로 항상 통과,
   segment distance 규칙은 §3.5에 따라 제외)
5. 재검증 통과 → 2바퀴 route를 active route로 전환 + 자동 비행 시작
   재검증 실패 → 원안(1바퀴) route 유지, 2바퀴도 원안으로 재비행 (§3.8)
6. 보정 결과 상태 게시 (`UPLINK_STATUS_MID`) + 로그

## 3.5 segment 거리 규칙과의 충돌 — 해소 (2026-07-10)

- **문제**: 기존 route 검증(`uplink_app_utils.c` `UPLINK_APP_IsWaypointSegmentDistanceValid`)은
  인접 waypoint 간 거리를 **정확히 2.0m ± 0.0001m**로 강제(`UPLINK_APP_ROUTE_SEGMENT_DIST_M`/
  `_TOL_M`). 구간별 보간 보정은 waypoint마다 다른 양만큼 이동시키므로 이 규칙과 충돌해
  거의 항상 재검증 실패로 이어짐.
- **결정**: 이 segment 거리 고정 규칙은 **이전 프로토타입 단계의 잔재로 판단, 제외**한다.
  보정된(2바퀴) route 재검증에서는 segment 거리 체크를 적용하지 않음 — flyable area/altitude/
  finite 체크만 유지.
- **영향 범위(구현 시 확인 필요)**: `default_uplink_app_mission_cfg.h`의
  `UPLINK_APP_ROUTE_SEGMENT_DIST_M`/`_TOL_M`이 1바퀴(원본) route 검증에도 쓰이고 있어,
  이 규칙을 완전히 제거할지 아니면 "보정 route 재검증 경로에서만 우회"할지는 구현 시
  코드 구조를 보고 정할 것.

## 3.4 원 피팅 기반 보정 — 확정 (2026-07-11, "구간별 보간"에서 교체)

- **결정**: 개별 waypoint 편차를 낱개로 보간하는 대신, **1바퀴 실측 위치 전체에 원을
  피팅**해서 보정한다.
  1. 1바퀴 각 waypoint 도달 시 기록한 실측 pose(X,Y)들로 **최소자승 원 피팅** →
     실측 중심(cx, cy)·반지름(r) 도출.
  2. 원래 계획 waypoint들은 이미 원 위에 있으므로, 각 waypoint의 **각도(θ_i)는 유지**.
  3. 2바퀴 waypoint 계산: `보정_i = (cx + r·cosθ_i, cy + r·sinθ_i)`.
- **근거**: "구간별 보간"보다 원의 전체 구조(중심/반지름 단 2~3개 파라미터)에
  최소자승으로 한 번에 피팅하므로 노이즈 상쇄 효과가 더 크고, 계산도 더 명확함.
  §3.6(노이즈를 게이트 없이 다 반영)의 전제(평균화로 노이즈 상쇄)와도 정확히 부합.
- **원 피팅 입력**: **waypoint 도달 시점 실측치만 사용** (1바퀴 동안 계속 들어오는
  `LOCAL_POSITION_NED` 스트림 전체가 아니라, §4 step1에서 이미 기록하기로 한
  waypoint별 실측 pose만 재사용). 추가 스트리밍 샘플까지 쓰는 건 저장/처리 구조가
  더 필요해 이번 범위에서 제외.
- **잔여 세부사항 (구현 시)**:
  - [ ] 원 피팅 알고리즘 구체 선택 (예: Kasa method 등 대수적 최소자승 원 피팅)
  - [ ] 최소 몇 개 점부터 원 피팅이 유효한지 (route가 최대 16 waypoint 한도)
  - [ ] θ_i를 원래 계획 waypoint에서 어떻게 구할지 (원래 waypoint 자체도 이미
        계획상 원 중심 기준 각도를 가지므로, 그 값 재사용 여부)

## 3.6 허용오차 게이트 — 없음 (2026-07-10 확정)

- **결정**: 허용오차 ±Xm 임계값 자체를 두지 않는다. 측정된 편차는 크기와 무관하게
  항상 보정 계산에 반영한다 (통과/보정진입을 나누는 게이트 단계 삭제).
- **근거**: GPS 노이즈 처리를 별도 임계값 판정으로 거를 필요 없이, §3.4 구간별 보간이
  여러 waypoint의 편차값을 함께 사용하는 평균화 성격이라 무작위 노이즈(방향 무작위,
  평균 0에 가까움)는 보간 과정에서 자연히 상쇄됨. 실제 계통적 드리프트만 보정 결과에
  유의미하게 반영됨.
- **주의(구현 시)**: 이 가정은 §3.4 보간 함수가 여러 점을 같이 쓰는 평균화 방식일 때만
  성립. 만약 waypoint별 단순 1:1 치환(평균화 없는 보간)으로 구현되면 노이즈가 그대로
  전파되므로, §3.4 구현 시 평균화 성격을 유지하도록 확인 필요.

## 3.7 실측 pose 신선도 — 확정 (2026-07-10)

- **결정**: 신선도 기준 = **2000ms**, 기존 `cfs_core_app`의
  `CFS_CORE_APP_LOCAL_TIMEOUT_MS`(=2000) 재사용.
- **근거**: 편차 계산에 쓰는 실측 위치가 health 판정에 쓰는 `LOCAL_POSITION_NED`와
  동일 메시지. 새 기준을 따로 만들면 "health는 fresh인데 보정 기능은 stale로 본다"
  같은 모순이 생길 수 있어, 기존 값과 일치시킴.

## 3.8 재검증 실패 시 처리 — 확정 (2026-07-10)

- **결정**: 보정된(2바퀴) route가 flyable area(X/Y) 재검증에 실패하면, 보정 route는
  버리고 **원안(1바퀴) route를 그대로 유지해 2바퀴도 원안으로 재비행**한다.
  reject로 미션 자체를 중단시키지 않는다.
- **근거**: 기존 uplink_app route update 검증 실패 처리(`uplink_app_cmds.c:217-227`,
  `UPLINK_APP_RESULT_REJECT_ROUTE`)와 동일한 패턴 — 새 route가 무효하면 그냥 반환하고
  기존 active route를 손대지 않는다. 원안 route는 이미 1바퀴 때 검증을 통과한 안전한
  경로이므로, 보정 실패 시 "보정 없이 원래 계획대로 계속 진행"이 합리적.

## 3.9 1바퀴 종료 후 처리 — 호버링 대기, armed 업로드 허용 (2026-07-11 확정)

- **문제(코드 확인)**: 2-pass는 "1바퀴 끝나자마자 armed(비행 중) 상태에서 보정 route를
  업로드"해야 하는데, 현재 코드에 두 가지 장애물이 있음.
  1. `mavlink_bridge_app_utils.c` `MAVLINK_BRIDGE_APP_StartMissionUpload()` —
     `IsArmed`일 때 미션 업로드 자체를 거부하고 return (`MAVLINK_BRIDGE_APP_ARMED_WARN_EID`).
  2. `MAVLINK_BRIDGE_APP_SendMissionItemInt`/`SendMissionItem` — `autocontinue`가
     모든 waypoint에 대해 `1U`로 하드코딩. 마지막 waypoint도 autocontinue=1이므로
     1바퀴 완료 시 FC가 자동으로 다음 동작(RTL/착륙 등, FC 파라미터에 따라 다름)으로
     넘어감 — "호버링 대기"가 보장되지 않음.
- **결정**: 1바퀴 마지막 waypoint 자체를 **명시적 호버링 커맨드**(`MAV_CMD_NAV_LOITER_UNLIM`
  등, 제자리 무기한 대기)로 전송한다. `autocontinue=1`의 일반 WAYPOINT로 두고 FC의
  미션-종료-후 기본 동작(RTL/착륙 등, FC 파라미터에 따라 달라짐)에 기대는 대신,
  마지막 항목 자체를 호버링 명령으로 만들어 **FC 파라미터 설정과 무관하게 확정적으로
  제자리 대기**하도록 함.
  1. 마지막 waypoint 위치에서 `MAV_CMD_NAV_LOITER_UNLIM`(또는 동등한 hold 커맨드)로 전송.
  2. `StartMissionUpload()`의 `IsArmed` 차단은 **이 2-pass 보정 route 업로드 경로에
     한해 완화**한다 — 범용으로 armed 업로드를 다 허용하는 게 아니라, "마지막 항목이
     LOITER_UNLIM 상태에서 대기 중"이라는 조건이 성립할 때만 통과시킴 (일반 비행 중
     임의 route 재업로드로 인한 위험한 경로 급변경은 여전히 차단 유지).
- **잔여 확인사항 (구현 시)**:
  - [ ] "LOITER 대기 중"임을 무엇으로 판정할지 — 현재 mission sequence(마지막 waypoint
        도달 여부)를 추적하는 상태가 mavlink_bridge_app에 없음. `MISSION_CURRENT`
        구독 또는 온보드 도달 판정(§4 step1과 동일 로직)으로 추적 구조 신규 필요.
  - [ ] armed 상태에서 보정 route 재업로드 후 자동으로 새 미션이 시작되는지 확인 필요 —
        `MISSION_ITEM_INT` 업로드만으로 LOITER를 깨고 자동 진행되는지, 별도 재개
        커맨드(MISSION_SET_CURRENT 등)가 필요한지는 구현 시 FC 동작 확인 필요.

## 5. 미결정 사항 (TODO)

- [x] **허용오차 게이트 없음으로 확정 (2026-07-10)** — §3.6 참조.
- [x] **실측 pose 신선도 확정 (2026-07-10)** — §3.7 참조. 2000ms, `CFS_CORE_APP_LOCAL_TIMEOUT_MS` 재사용.
- [x] **재검증 실패 시 처리 확정 (2026-07-10)** — §3.8 참조. 원안(1바퀴) route 유지, reject 아님.
- [x] **2바퀴로 종료 확정 (2026-07-10)** — 3바퀴 이상 반복 없음. 2바퀴 완료 후 재검증 실패 시에도
      추가 랩을 돌리지 않고 원안 route로 종결(§3.8과 동일 정책).
- [x] **보정 계산 방식 확정 (2026-07-11)** — §3.4 참조. 원 피팅 기반 보정으로 교체
      (waypoint 도달 실측치만 사용, 각도 유지 + 중심·반지름 재배치). 원 피팅 알고리즘
      구체 선택 등 세부 구현은 코드 작성 시 결정.
- [x] **1바퀴 종료 후 armed 상태 재업로드 문제 확정 (2026-07-11)** — §3.9 참조.
      마지막 waypoint autocontinue=0으로 호버링, 2-pass 경로에 한해 armed 업로드 차단 완화.
      FC 파라미터(MIS_DONE_BEHAVE 등) 실물 확인은 구현 시 남음.

**설계 미결정 사항 전부 확정 — 스펙/코드 반영 준비 완료. (단, §3.9 FC 파라미터 실물 검증은 구현 단계 확인 필요)**

## 6. 반영 예정 위치 (확정 후)

- 스펙: `notes/mission_app_runtime_spec.md` §18.4.6.2 route update
  (line 1179~1189 waypoint 검증 규칙 + baseline 수치에 GPS 보정 규칙 추가)
- 코드: `uplink_app/fsw/src/uplink_app_cmds.c`
  route payload 검증부(`UPLINK_APP_ParseRouteUpdatePayload` 근처)
- config: `uplink_app/config/default_uplink_app_mission_cfg.h`
  (`UPLINK_APP_ROUTE_*` 상수에 허용 오차 추가)
