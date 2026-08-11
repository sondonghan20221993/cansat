# BL-41(route 부분) 설계 확정 — cfs_core_app MissionRoute를 FC 미러 버퍼로 전환 (2026-07-23)

## 배경
`persistent_state_gap_audit_2026-07-23.md`에서 발견된 8범주 중 실질 영향
큰 2건(CONFIG 조정값, mission route) 중 **route 쪽만 여기서 확정**.
CONFIG 쪽은 별도 결정 필요(미착수).

## 핵심 결정: "파일 지속" 대신 "FC를 진실원본으로 하는 버퍼"

**기각안**: `cf/route_state.bin` 등 파일 저장(BootCount/LastHealthState와
동일 패턴).
**채택안**: `cfs_core_app.MissionRoute`는 **RAM 전용 버퍼** — 자체
영속화 안 함. 아래 3개 트리거 시점마다 FC에서 다시 읽어 채움.

**기각 이유**: FC(ArduPilot/PX4)가 미션을 자체 플래시에 영속 저장하므로
Pi 파일은 "사본의 사본"이 됨 — 업로드 중 재부팅 등으로 Pi 파일과 FC
실물이 어긋나는 이중 원본 문제가 구조적으로 생김. FC를 유일 진실원본으로
하고 필요할 때마다 재조회하면 이 버그 클래스가 아예 불가능해짐.

## 재조회(다운로드→캐시 로딩) 트리거 3개

1. **FC 링크 CONNECTED 전이 시** (엣지 트리거, upload/download 둘 다 IDLE
   조건) — Pi 재부팅/FC 재부팅/링크 복구 전부 커버. 오늘 발견된
   `wp_count=0` 문제의 직접 해법.
2. **ROUTE_UPDATE(경로 수정) 업로드 완료 후** — "업로드했다고 믿는 값"이
   아니라 FC가 실제로 받아들인 값을 재확인해 캐시 확정. 검증까지 겸함.
3. **MISSION_QUERY_CC(조회명령) 수신 시** — 기존엔 다운로드 결과를 EVS
   로그로만 찍고 버렸음(`mavlink_bridge_app_utils.c:1580` 부근 주석
   "이 응답값은 캐시 상태에 반영되지 않음") — 이제 같은 다운로드 결과를
   캐시에도 반영.

3개 트리거 모두 mavlink_bridge_app의 기존 `MissionDownload*` 상태머신
하나를 공유 — 코드 경로 하나로 처리.

## MID 분리 (지상국 발 ROUTE_UPDATE와 혼동 방지)

신규 게시 MID(가칭 `FC_MISSION_READBACK_MID`, 0x1914 대역 — 0x1912
EXEC_RESULT_MID, 0x1913 ROUTE_SNAPSHOT_MID 다음 순번 확인 필요)로
mavlink_bridge_app이 FC 다운로드 결과를 게시 → cfs_core_app이 구독해서
`MissionRoute` 캐시 갱신. 기존 `ROUTE_UPDATE_MID`(0x190B, 지상국→
cfs_core_app+mavlink_bridge_app)와는 완전히 별개 채널 — 출처가
"지상국이 보낸 것"인지 "FC 실물 readback"인지 MID로 구분됨.

## RAM only 확인 (사용자 확정, "그리고 ram에다가 하면 되는거지")

`MissionRoute` 캐시 자체는 파일 저장 없이 RAM에만 유지. 재부팅 시
비어있는 게 정상 — 위 3개 트리거 중 하나(대개 트리거 1, FC 링크
재연결)가 곧 다시 채움. 파일 I/O·CRC 검증·corruption 처리 전부 불필요
(BL-17/18/19 같은 패턴 적용 대상 아님).

## MID 값 확정

`0x1914` (기존 배정 최댓값 `0x1913` ROUTE_SNAPSHOT_MID 다음 순번,
0x190B~0x1913 전부 사용 중 확인됨 — 2026-07-23 grep 검증). 이름은
`FC_MISSION_READBACK_MID` 유지.

## 재시도 정책 확정

readback 전체 시퀀스(MISSION_REQUEST_LIST 발신 ~ 마지막 wp 수신)가
timeout으로 실패하면 **무한 재시도, 단 지수 백오프(1s→2s→4s→5s 상한,
5s 도달 후 고정)**로 간격을 두고 처음부터 재시도. 근거: 사용자 판단
— readback 실패 시 사실상 임무 수행 자체가 불가능하므로 포기하면 안
됨. 단, 링크 불안정 시 요청 폭주 방지 위해 즉시 재시도 대신 백오프
적용(BL-38의 "무한 재시도, 카운터는 관측용" 패턴과 동일 정신).
다운로드 도중 링크가 DISCONNECTED로 전이하면 이 백오프 루프는 즉시
중단 — 이후 트리거 1(재연결)이 백오프를 리셋하고 처음부터 재개.

## 남은 구현 작업 (✅ 전부 완료, 2026-07-23 — SDD→TDD)

- [x] `shared_msgs/`에 readback MID용 메시지 구조체 신설 (route_msg.h
      재사용 가능한지, 아니면 신규 파일)
- [x] MID 값 `0x1914` 실제 반영 (`default_cfs_core_app_msgid_values.h`,
      `default_mavlink_bridge_app_msgid_values.h` 동기화)
- [x] 지수 백오프 재시도 상태(현재 백오프 간격, 다음 재시도 타임스탬프)를
      `MissionDownload*` 그룹에 필드 추가
- [x] mavlink_bridge_app: `MissionDownload*` 완료 시 readback MID 게시
      배선 (현재는 EVS 로그로만 끝남 — `mavlink_bridge_app_utils.c`
      MISSION_ITEM_INT 응답 처리 블록, 다운로드 완료 지점)
- [x] mavlink_bridge_app: 트리거 1(CONNECTED 전이) 자동 MISSION_QUERY
      발동 로직 추가 — 현재 MISSION_QUERY는 지상 명령으로만 시작됨
- [x] mavlink_bridge_app: 트리거 2(업로드 완료 후) 자동 재조회 호출 추가
      (`MISSION_UPLOAD_ACCEPTED` 분기, utils.c:1505 부근)
- [x] cfs_core_app: readback MID 구독 + `MissionRoute` 캐시 갱신 처리
      추가 (기존 `ROUTE_UPDATE_MID` 처리와 별도 분기)
- [x] spec 갱신(SDD로 구현 전 선반영): `mission_app_runtime_spec.md`(MID 계약),
      `cfs_core_app_behavior_spec.md`, `mavlink_bridge_app_behavior_spec.md`
- [x] UT 추가(15개: mavlink 11 + cfs_core utils 2 + app 1 + dispatch 1): mavlink_bridge_app(readback 게시), cfs_core_app(캐시 갱신
      분기), 트리거 3종 각각의 엣지 케이스(경합 방지 조건 포함)
- [x] BACKLOG.md BL-41 항목을 이 설계로 갱신, 착수 시작하면 진행상황
      기록

## 미결정 (다음 세션 또는 착수 중 결정)

- ~~CONFIG 조정값 쪽 지속 방안~~ — ✅ 별도 완료(2026-07-23, BL-41 CONFIG 부분).

---

## 상세 구현 설계 (SDD, 2026-07-23 확정)

### D1. 메시지 — 신규 구조체 불필요, `ROUTE_UPDATE_TLM_t` 재사용

`shared_msgs/route_msg.h`의 `ROUTE_UPDATE_TLM_t`를 그대로 MID
`FC_MISSION_READBACK_MID(0x1914)`에 실어 게시 — cfs_core_app의 기존
`UpdateRouteCache()` 파서를 재사용할 수 있고, 출처 구분은 MID로 충분.
필드 채움 규칙: `SourceSequence=0`(지상 시퀀스 아님), `RouteType=1`
(MISSION — FC에 landing 세그먼트 개념 없음), `RouteVersion=0`,
`Seq`=mavlink_bridge 자체 증가 카운터, `WaypointCount=min(수신 개수, 16)`
(초과 시 16 클램프 + WARN 이벤트).

### D2. 좌표 역변환 (다운로드 lat/lon → 로컬 미터)

캐시(`ROUTE_WAYPOINT_t`)는 로컬 미터 좌표이므로, FC가 돌려주는
lat/lon degE7을 업로드 변환(`SendMissionItemInt`)의 **정확한 역함수**로
되돌린다 (`RefLatE7`/`RefLonE7` 기준 equirectangular):

```
X = (WpLatDeg - RefLatDeg) * DEG_TO_RAD * EARTH_RADIUS_M
Y = (WpLonDeg - RefLonDeg) * DEG_TO_RAD * EARTH_RADIUS_M * cos(RefLatRad)
Z = Alt (무변환)
```

Ref가 (0,0)(GPS 미수신)이면 업로드와 동일하게 그대로 진행(왕복 정합 유지).

### D3. mavlink_bridge_app 상태 추가 (`MAVLINK_BRIDGE_APP_Data`)

- `ROUTE_WAYPOINT_t MissionDownloadWaypoints[16]` — 수신 중 wp 버퍼
  (현재는 EVS 로그 후 버림 → 항목별 역변환해 저장)
- `uint8  MissionReadbackPending` — 재시도 대기 플래그
- `uint32 MissionReadbackBackoffMs` — 현재 백오프(1000 시작)
- `uint32 MissionReadbackNextRetryMs` — 다음 재시도 시각
- `uint32 MissionReadbackSeq` — 게시 Seq 카운터
- `ROUTE_UPDATE_TLM_t FcMissionReadbackTlm` — 게시 버퍼(Init에서 CFE_MSG_Init)

### D4. 트리거 배선 (3개 모두 기존 `StartMissionDownload` 경로 공유)

| 트리거 | 위치 | 조건 |
| --- | --- | --- |
| 1. CONNECTED 전이 | `SetLinkState()` — 현재 단순 대입(utils.c:2137)에 엣지 검출 추가 | 이전 상태 ≠ CONNECTED && 신규 == CONNECTED && upload/download 둘 다 IDLE. 전이 시 백오프 리셋(1s) + pending 해제 후 즉시 시작. DISCONNECTED 전이 시 pending 취소 |
| 2. 업로드 완료 | MISSION_ACK accepted 분기(utils.c:1505, upload ACTIVE→IDLE 직후) | download IDLE일 때 시작 |
| 3. MISSION_QUERY_CC | 기존 `MissionQuery()` 그대로 (시작 로직 변경 없음) | 기존 조건 유지 |

완료 게시는 3개 트리거 공통 — 다운로드 완료 지점(utils.c:1601~,
`download complete` 분기)에서 버퍼→`FcMissionReadbackTlm` 채워 0x1914
게시. 신규 EID `MISSION_READBACK_EID(16)` INFO.

### D5. 백오프 재시도

- `CheckMissionDownloadTimeout()`에서 IDLE로 떨굴 때: 링크 CONNECTED면
  `Pending=1`, `NextRetryMs = now + Backoff`, `Backoff = min(Backoff*2, 5000)`
- `ServiceSerial()` 주기 처리에 재시도 체크 추가: `Pending && now >=
  NextRetryMs && CONNECTED && upload/download IDLE` → 재시작
- DISCONNECTED 전이 시 `Pending=0` (트리거 1이 재연결 때 백오프 리셋 후 재개)

### D6. cfs_core_app 수신

- Init: `FC_MISSION_READBACK_MID(0x1914)` 구독 추가
  (`default_cfs_core_app_msgid_values.h`에 정의)
- `ProcessStateMessage()`: 0x1914 분기 신설 — `UpdateRouteCache(&MissionRoute)`
  호출(ROUTE_UPDATE_MID 분기와 별도, RouteType 검사 없이 MissionRoute 고정 —
  출처가 FC readback이므로), 기존 `ROUTE_READBACK_EID(18)` INFO 재사용

### D7. UT 목록 (TDD red 선작성 대상)

mavlink_bridge (utils 러너):
1. `SetLinkState_ConnectedEdge_StartsReadback` — 전이 → download WAIT_COUNT
2. `SetLinkState_ConnectedEdge_SkipWhenUploadActive`
3. `SetLinkState_NoEdge_NoReadback` — CONNECTED→CONNECTED 재호출 무동작
4. `SetLinkState_Disconnect_CancelsPendingRetry`
5. `UploadComplete_TriggersReadback` — MISSION_ACK accepted 주입 → WAIT_COUNT
6. `DownloadComplete_PublishesReadbackMid` — count+item 주입 → 0x1914 게시,
   wp 역변환 값 검증(업로드 공식 역산 왕복)
7. `DownloadCount_ClampedTo16`
8. `DownloadTimeout_SchedulesBackoffRetry` — 1s 예약
9. `BackoffDoubles_To5sCap` — 1→2→4→5(고정)
10. `RetryFires_WhenDue` — ServiceSerial 경유 재시작
11. `ReconnectResetsBackoff`

cfs_core (utils 러너 + app 러너):
12. `ProcessStateMessage_FcReadback_UpdatesMissionRoute`
13. `ProcessStateMessage_FcReadback_LandingUntouched`
14. `Init_Subscribes_FcMissionReadback` (app 러너, SubscribeEx 스텁 카운트)

### D8. spec 반영 (green 완료 후)

`mission_app_runtime_spec.md` §5.1 MID 계약 테이블 + §17.1에 0x1914 추가,
`cfs_core_app_behavior_spec.md` §16(경로 처리), `mavlink_bridge_app_behavior_spec.md`
§6(미션 다운로드) 갱신.

## 구현 완료 기록 (2026-07-23)

SDD(spec 선정의: mavlink §10 재정의, runtime §5.1.1/§17.1, cfs_core §16 2채널)
→ TDD(테스트 15개 red 선작성 → green). 신규 구조체 없이 ROUTE_UPDATE_TLM_t
재사용. dispatch 화이트리스트 누락 갭을 코드 검사로 발견해 테스트 1개 추가
(TaskPipe_FcMissionReadback). 테스트 픽스처 버그 1건 수정(TimeoutMs
0xFFFFFFF0은 wraparound 비교에서 즉시 만료 판정 → 0x40000000). 전체 회귀
16/16 PASS. 잔여: Pi 실기 검증(재배포 후 FC readback 실측).
