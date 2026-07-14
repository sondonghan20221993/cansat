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

## 근본 원인 확정 (2026-07-14) — SCH_LAB 아님, HK mirror 구조체 레이아웃 불일치 회귀

**후보 A/B/C(SCH_LAB 미동작) 전부 오진.** 실측으로 SCH_LAB은 정상 동작 확인:
- `CFS_CORE_APP HK: mission_wp=...` 로그가 매 초 출력됨 — 이건
  `CFS_CORE_APP_ReportHousekeeping()`가 SEND_HK(`0x18C1`)로 트리거된 것.
  즉 SCH_LAB이 스케줄 테이블(4개 엔트리 전부 포함)을 로드·실행 중이라는
  직접 증거. `sch_lab_table.tbl`도 정상(2552B, 당일 재생성), 시스로그 에러 0건.

**진짜 원인 — 구조체 레이아웃 불일치:**
- 발행측 `MAVLINK_BRIDGE_APP_HkTlm_t`(msgstruct.h): `...ParseErrorCount(u32),
  NonFiniteValueCount(u32), LastRxTimestampMs(u32)...`
- 수신측 `CFS_CORE_APP_BridgeHkMirror_t`(cfs_core_app_utils.h): `...ParseErrorCount(u32),
  LastRxTimestampMs(u32)` — **`NonFiniteValueCount`가 빠져 있음.**
- 결과: mirror가 `LastRxTimestampMs`를 4바이트 앞에서 읽어 발행측의
  `NonFiniteValueCount`(정상 운용 시 0) 값을 타임스탬프로 오독.
  `LinkState`/`LastErrorCode`는 오프셋이 같아 정상.
- `cfs_core_app_utils.c:245`:
  `BridgeTimedOut = !Received || (NowMs - LastRxTimestampMs) > BridgeTimeoutMs`.
  `LastRxTimestampMs`가 0으로 읽히니 `NowMs - 0 = NowMs`(부팅 후 경과 ms)가
  항상 `BridgeTimeoutMs` 초과 → **BRIDGE_TIMEOUT 영구 참 → health FAILED 고착.**

**회귀 출처**: 커밋 `947b3cf`(2026-07-13, `fc_value_validation_gap` NaN/Inf 검증
수정)가 발행측 `HkTlm_t`에 `NonFiniteValueCount`를 삽입했으나, cfs_core의 mirror
구조체를 동기화하지 않음(`git log -S NonFiniteValueCount -- cfs_core_app_utils.h`
결과 없음 = mirror엔 한 번도 존재한 적 없음). 6월 17일 SCH_LAB 이슈와는 무관한
별개의 신규 버그였고, "재발"로 오인한 것.

## 결정

mirror 구조체(`CFS_CORE_APP_BridgeHkMirror_t`)에 `NonFiniteValueCount(u32)`를
발행측과 동일하게 `ParseErrorCount`와 `LastRxTimestampMs` 사이에 삽입해 레이아웃
정합. (더 근본적으로는 mirror-복사 패턴 자체가 이런 드리프트에 취약 — 공용 헤더
공유가 이상적이나 이번은 최소 수정으로 필드 추가만.)

## 상태

- [x] 재발 확인 (2026-07-14) — health FAILED 고착, CONFIG 차단
- [x] `FORCE_FLAG`로 즉시 우회, `lora_tdm.downlink_protocol` v2 전환 성공
- [x] 근본 원인 확정 — SCH_LAB 정상, mirror 구조체 `NonFiniteValueCount` 누락
      (커밋 `947b3cf` 회귀). A/B/C 후보 전부 기각.
- [x] SCH_LAB 정상 동작 실측 확인 (cfs_core HK 매 초 = SEND_HK 수신 증거)
- [ ] mirror 구조체에 `NonFiniteValueCount` 추가 (레이아웃 정합)
- [ ] Pi 재빌드/재기동 후 health가 NOMINAL/DEGRADED로 정상 판정되는지 확인
      (bridge 살아있으면 최소 BRIDGE_TIMEOUT은 해소돼야 함)
- [ ] 단위테스트 회귀 확인
