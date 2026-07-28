# route_readback 타임아웃 조사 — 2026-07-28

## 배경
지상국(mapRouteGUI "READ" 버튼)에서 `route_readback` 요청(seq=217~221)이
매번 5초 타임아웃. Pi 로그는 매번 `cfs_core_app`이 요청을 받고
`lora_tdm_app`이 스냅샷(wp_count=16, total_pages=8)까지 정상 캐싱하는 걸 확인.

## 확인된 원인 #1 (해결됨) — Pi 배포 바이너리가 3일 전 것
- `~/cFS_clean/build/exe/cpu1/core-cpu1` 빌드 시각: **7/25 21:15**
- BL-70(ROUTE_MAX_WAYPOINTS 16→37), BL-71(waypoint 페이지에 CmdType 추가,
  12→13바이트/포인트, 블록 28→30바이트) 소스 반영은 **7/28**.
- 즉 Pi는 소스만 최신이고 실제 실행 중인 바이너리는 두 변경 모두 반영 안 된
  옛날 포맷(28바이트 블록, wp_count clamp=16)으로 계속 송신 중이었음.
- 지상 디코더(`lora_protocol_v2.py`)는 이미 30바이트 포맷 기준으로 파싱하도록
  고쳐놨기 때문에, 프레임 길이 검증(`len(frame) >= wp_offset + 30 + 2`)에서
  옛 포맷 프레임이 항상 미달 → `wp_waypoints`가 계속 `None` → readback이
  영원히 완료 안 됨 (크래시는 없음, 그냥 "idle"에 계속 머묾).
- **조치**: Pi에서 `make native_default_cpu1-all` 재빌드 후, `make
  mission-install`이 `CMAKE_INSTALL_PREFIX=/exe`(절대경로 오설정)로 실패해서
  `native/default_cpu1/{cpu1/core-cpu1, apps/lora_tdm_app/lora_tdm_app.so}`를
  `exe/cpu1/`로 수동 `cp` (서비스 정지 후 core-cpu1 교체, "Text file busy"
  회피). `cfs.service` 재시작 완료 (16:27).
- ⚠️ **후속 확인 필요**: `mission-install` 타겟의 `CMAKE_INSTALL_PREFIX`가 왜
  `/exe`(절대경로, 루트 밑)로 잡혀있는지 원인 파악 및 정상화 — 지금처럼 수동
  cp로 우회하면 다음 빌드 때마다 같은 문제 반복됨.

## 확인된 원인 #2 (조사 중) — QGC 미션 항목 수 불일치
- 재빌드 후에도 FC 미션 재조회 결과가 QGC에 보이는 개수보다 적게 들어옴
  (16개 clamp 로그 확인 후 재조회 시도 → 오히려 15개로 더 줄어듦).
- `mavlink_bridge_app_utils.c`의 MISSION_ITEM_INT 처리 루프 자체는 CmdType
  기준 필터링이 없음(모든 항목을 그대로 저장, ROUTE_MAX_WAYPOINTS=37 한도
  내에서만 clamp) — 즉 코드상 필터링 로직은 못 찾음. 원인 미확정.
- **다음 단계**: 사용자가 QGC에서 미션 항목을 순서/타입/좌표 기준으로 직접
  하나씩 대조 예정. 대조 결과 나오면 Pi의
  `MAVLINK_BRIDGE_APP: [wp %u] lat=... lon=... alt=... cmd=...` 로그와
  1:1 비교해서 정확히 몇 번 seq에서 왜 빠지는지 특정할 것.

## 배제된 가설 — CmdType 기반 필터링 (2026-07-28 코드 정독으로 확인)
"시작/호버 지점이 타입 때문에 걸러진다"는 가설은 **배제됨**. 5단계 전부 확인:
1. `mavlink_bridge_app_utils.c:1804-1819` MISSION_ITEM_INT 수신 — 받은 항목
   전부 저장, CmdType 검사 없음
2. `PublishFcMissionReadback()` — 전부 게시(ROUTE_MAX_WAYPOINTS 한도 clamp만)
3. `cfs_core_app_utils.c:122` `SetRouteCacheWaypoints` — 통째로 memcpy
4. `lora_tdm_app_utils.c:988` `ProcessRouteSnapshot` — 2개씩 페이징만, 필터 없음
5. 지상 `RouteReadbackAssembler.feed()` — 페이지 순서대로 concat
→ 남은 가능성은 (A) FC가 MISSION_COUNT로 주는 개수 자체가 적음,
  (B) 지도에 그려지는데 라벨이 없어 못 알아봄.

## ⚠️ 다음 조사 착수점 — 지도에 `cmd=0` 마커가 뜸 (2026-07-28 사용자 확인)
- `cmd=0`은 유효한 MAV_CMD가 아님. 펌웨어 인코더(`lora_tdm_app_utils.c:284,288`)가
  `InPage < 1`/`InPage < 2`일 때 CmdType 자리에 0을 써넣는 **패딩 값**과 일치.
- 즉 실제 웨이포인트가 아니라 빈 슬롯이 지상에 그려지고 있을 가능성 →
  `wp_in_page` 처리 또는 `RouteWaypointCount`/`RouteTotalPages` 정합성부터 볼 것.
- 사용자 지시로 상세 확인은 보류(2026-07-28) — 재개 시 여기부터 시작.

## 부수 발견 — CmdType 라벨 매핑이 3종류뿐
- `mapRouteGUI/plugin.js`의 `CMD_TYPE_LABELS`는 `16=WAYPOINT`,
  `17/19=LOITER(호버)`만 정의. TAKEOFF(22)/LAND(21)/RTL(20) 등은
  `cmd=NN`으로만 표시되어 눈에 잘 안 띔 — "안 보인다"고 느껴졌던 원인 중
  하나일 수 있음. 실제 미션 항목 수/타입 확정되면 라벨 추가 여부 결정.
