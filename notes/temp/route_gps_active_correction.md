# [TEMP] Route GPS 오차 능동 보정 (작업 노트)

> 상태: 초안 / 설계 중
> 작성: 2026-07-07
> 성격: 아직 스펙 본문(§18.4.6.2) 반영 전. 방향·미결정 사항 기록용.

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

## 4. "능동적 보정 과정" — 단계 (갱신)

1. 1바퀴 비행 중 각 waypoint 도달 시 실측 pose(직전 downlink) vs 계획 waypoint 편차를 기록
   (신선도 판단 포함 — 몇 초 이내만 유효로 볼지는 §5 미결정)
2. 마지막 waypoint 도달 시 "1바퀴 완료" 판정, 기록된 편차 목록으로 보정 단계 진입
3. 구간별 보간으로 2바퀴 route 계산 (계산식은 §3.4 확정 필요)
4. 보정된 route 재검증 (flyable area, altitude — segment distance 규칙은 §3.5에 따라 제외)
5. 재검증 통과 → 2바퀴 route를 active route로 전환 + 자동 비행 시작
   재검증 실패 → §5 처리 정책에 따름
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

## 3.4 구간별 보간 — 구체화 필요 (신규 미결정)

- [ ] 보간 입력: waypoint별 편차 벡터 전부 사용 vs 일부(예: 급격한 이상치 제외)만 사용?
- [ ] 보간 함수: 선형 보간 vs 스플라인 등?
- [ ] 편차 기록 저장 위치/최대 개수 (route 1~16 waypoint 한도와 연동)

## 5. 미결정 사항 (TODO)

- [ ] 허용 오차 기준값 ±Xm (config 상수로? `UPLINK_APP_ROUTE_*`)
- [ ] 실측 pose 소스·신선도: 몇 초 이내 downlink만 유효로 볼지
- [ ] 보정 후 재검증 실패 시 처리 (기존 active route 유지 / reject / 1바퀴 route로 재비행)
- [x] **2바퀴로 종료 확정 (2026-07-10)** — 3바퀴 이상 반복 없음. 2바퀴 완료 후 재검증 실패 시에도
      추가 랩을 돌리지 않고 §5 처리 정책(기존 route 유지/reject 등)으로 종결.
- [ ] §3.4 보간 세부 방식

## 6. 반영 예정 위치 (확정 후)

- 스펙: `notes/mission_app_runtime_spec.md` §18.4.6.2 route update
  (line 1179~1189 waypoint 검증 규칙 + baseline 수치에 GPS 보정 규칙 추가)
- 코드: `uplink_app/fsw/src/uplink_app_cmds.c`
  route payload 검증부(`UPLINK_APP_ParseRouteUpdatePayload` 근처)
- config: `uplink_app/config/default_uplink_app_mission_cfg.h`
  (`UPLINK_APP_ROUTE_*` 상수에 허용 오차 추가)
