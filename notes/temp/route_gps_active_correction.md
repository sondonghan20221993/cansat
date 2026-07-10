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

## 4. "능동적 보정 과정" — 채워야 할 부분

단순 평행이동 한 방으로 끝나지 않고 아래 단계로 확장 예정 (초안):

1. 실측 위치(직전 downlink pose) 확보 / 신선도 판단
2. 수신 route waypoint 대비 편차 계산
3. 편차가 허용 오차(±Xm) 이내 → 통과 / 초과 → 보정 진입
4. 보정: (편차만큼 평행이동? / 오차 벗어난 구간만 재정렬?) — **미결정**
5. 보정된 route 재검증 (flyable area, altitude, segment distance 재확인)
6. 보정 결과 상태 게시 (`UPLINK_STATUS_MID`) + 로그

## 5. 미결정 사항 (TODO)

- [ ] 허용 오차 기준값 ±Xm (config 상수로? `UPLINK_APP_ROUTE_*`)
- [ ] "보정" 정의: (a) 편차만큼 waypoint 평행이동 vs (b) 오차 초과 route reject
      vs (c) 능동 재정렬 — 어느 것 / 조합?
- [ ] 실측 pose 소스·신선도: 몇 초 이내 downlink만 유효로 볼지
- [ ] 보정 후 재검증 실패 시 처리 (기존 active route 유지 / reject)
- [ ] "능동적 과정"의 단계 확정 (위 4절 초안 구체화)

## 6. 반영 예정 위치 (확정 후)

- 스펙: `notes/mission_app_runtime_spec.md` §18.4.6.2 route update
  (line 1179~1189 waypoint 검증 규칙 + baseline 수치에 GPS 보정 규칙 추가)
- 코드: `uplink_app/fsw/src/uplink_app_cmds.c`
  route payload 검증부(`UPLINK_APP_ParseRouteUpdatePayload` 근처)
- config: `uplink_app/config/default_uplink_app_mission_cfg.h`
  (`UPLINK_APP_ROUTE_*` 상수에 허용 오차 추가)
