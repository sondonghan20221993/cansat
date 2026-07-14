# cfs_core_app health FAILED(BRIDGE_TIMEOUT) 재발 — SCH_LAB 스케줄 미동작 추정 (2026-07-14 도출)

## 문제

`spec_code_audit.md:181`(2026-06-17 최초 발견·해결 기록)과 **동일 증상이 재발**.
Pi 재빌드/재기동(오늘 lora_tdm 200ms 변경 적용 세션) 후:

- `MAVLINK_BRIDGE_APP`은 FC로부터 ATTITUDE/LOCAL_POSITION_NED를 정상 디코드 중
  (로그로 확인, 계속 seq 증가)
- 그런데도 `cfs_core_app: health 0->2->3 fault=1(BRIDGE_TIMEOUT)`로 부팅 30초 만에
  고착 — 6월 17일에 고쳤던 것과 완전히 같은 패턴
- 부수 증상: `UPLINK_APP: command blocked by health state=3 class=1` — CONFIG류
  명령이 health FAILED 때문에 전부 거부됨 (이번에 `lora_tdm.downlink_protocol=1`
  CONFIG 명령 보낼 때 처음 발견)

## 왜 문제인가

- 6월 17일 조치(`mission_defs/tables/cpu1_sch_lab_table.c` 신규 추가 — 커스텀 앱
  4개 `SEND_HK` MID를 ~1Hz로 스케줄링)로 해결됐던 문제인데, **원인 소스 자체는
  이번 재확인 결과 존재하고 내용도 정상**(`MAVLINK_BRIDGE_APP_SEND_HK_MID_VALUE`
  0x18A1 포함 4건 전부 있음, `~/cfs-telemetry-app/mission_defs/tables/cpu1_sch_lab_table.c`).
  즉 **소스 회귀는 아님.**
- 그런데 오늘 로그에는 `SCH_LAB` 로드 이후 **`SCH_LAB Initialized` 등 후속 EVS
  이벤트가 전혀 없음** — 6월 17일 수정이 반영된 상태에서도 `SCH_LAB` 앱 자체가
  스케줄을 정상 구동하지 못하는 것으로 보임(테이블 파일 자체 로드 실패 가능성,
  또는 EVS 필터로 로그만 안 보이고 실제로는 동작 중일 가능성 — 미확정).
- CONFIG류 명령이 health FAILED에서 전부 막히므로, **`FORCE_FLAG`(벤치 전용
  health gate 우회) 없이는 사실상 어떤 CONFIG 명령도 Pi에 반영 불가능한 상태** —
  이번엔 force로 우회해서 `lora_tdm.downlink_protocol=1` 전환에 성공했으나,
  근본 원인 미해결 상태로 넘어감.

## 결정

미정 — 오늘 세션 목표(Stage 3 5Hz soak)가 급해 `FORCE_FLAG`로 즉시 우회하고
진행. 근본 원인(SCH_LAB이 왜 6월 17일 수정 이후에도 스케줄을 못 돌리는지)은
후속 조사 필요.

**후보 원인**:
- A: `sch_lab_table.tbl` 바이너리가 `make install`에서 최신 소스로 재생성 안 됨
  (오늘 `lora_tdm_app`만 부분 재빌드(`make lora_tdm_app`)했는데, 테이블 재생성은
  전체 `make`/`make install` 경로에 걸려있어 놓쳤을 가능성 — 오늘 세션에서
  `make lora_tdm_app -j4` 후 `make install`은 돌렸으나 테이블 리빌드 트리거
  여부 미확인)
- B: `SCH_LAB` 앱 자체가 다른 이유로 초기화 실패(테이블 등록 실패 등) —
  EVS 필터링으로 로그만 안 보일 수도 있음, `SCH_LAB` HK/카운터 직접 확인 필요
- C: 오늘 재빌드 과정에서 `sch_lab.so`가 기본(빈) 버전으로 되돌아갔을 가능성 —
  타임스탬프 확인 필요

## 상태

- [x] 재발 확인 (2026-07-14) — health FAILED 고착, CONFIG 차단
- [x] `FORCE_FLAG`로 즉시 우회, `lora_tdm.downlink_protocol=1` 전환 성공
      (`LORA_TDM_APP: downlink protocol set to v2(DL2)` 확인, 21:50:23)
- [ ] 근본 원인 확정 (A/B/C 중 어느 것인지)
- [ ] `sch_lab_table.tbl` 재생성 여부/타임스탬프 확인
- [ ] `SCH_LAB` 앱 초기화 상태 직접 확인 (HK, 카운터 등)
- [ ] 재발 방지 조치 — 매 빌드마다 테이블 재생성이 누락되지 않도록 빌드 절차에
      명시(또는 `notes/build_environment.md`/`integration_steps.md`에 체크리스트 추가)
