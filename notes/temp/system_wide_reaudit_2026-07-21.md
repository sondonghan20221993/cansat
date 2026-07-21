# 시스템 전체 재점검 — 미구현/모순 항목 (2026-07-21, 3차)

## 이번 점검의 축

1차(명령 dead-end), 2차(spec 수치/구현상태 주장 대조)에서 안 본 축으로 재점검:

- 코드 내 TODO/미구현 마커가 **아직 유효한지**(stale TODO 탐지)
- MID 값 충돌 / publish-subscribe 짝 불일치
- **정의만 되고 한 번도 발생하지 않는 EID**
- **정의만 되고 코드가 읽지 않는 config 상수**(죽은 설정)
- 텔레메트리 필드 중 하드코딩/미채움
- 문서의 stale 식별자(존재하지 않는 파일명, 틀린 MID)

대상: 살아있는 4개 앱만(`telemetry_app`/`legacy/`는 빌드 미참조 확인 후 제외).

---

## [상] F-1. spec이 ✅로 표기한 EID 6종이 실제로는 한 번도 발생 안 함

**spec 주장** — `notes/lora_tdm_app_behavior_spec.md:381-386`, 구현상태 열이
전부 `✅`:

| # | EID | spec이 적은 발생 조건 |
|---|---|---|
| 13 | `LINK_LOST_EID` | LinkState → DISCONNECTED 전이 |
| 14 | `LINK_DEGRADED_EID` | LinkState → DEGRADED 전이 |
| 15 | `LINK_RESTORED_EID` | LinkState → CONNECTED 복구 |
| 16 | `PIPE_ERR_EID` | SB 파이프 수신 오류 |
| 17 | `SUB_ERR_EID` | SB 구독 실패 |
| 18 | `SB_SEND_ERR_EID` | SB 메시지 송신 실패 |

**코드 실제** — 6개 전부 `lora_tdm_app/fsw/inc/lora_tdm_app_eventids.h`에
정의만 돼 있고 `fsw/src/*.c` 어디에서도 `CFE_EVS_SendEvent()` 인자로
쓰이지 않음(전수 grep 0건).

특히 링크 상태 3종의 근본 원인은 `LORA_TDM_APP_UpdateLinkState()`
(`lora_tdm_app_utils.c`)가 **전이 감지 자체를 안 하기 때문**:

```c
if (Elapsed > LINK_TIMEOUT_MS)          AppData->LinkState = DISCONNECTED;
else if (NoAckCount >= LOSS_THRESHOLD)  AppData->LinkState = DEGRADED;
else                                    AppData->LinkState = CONNECTED;
```

이전 값과 비교하지 않고 매 사이클 무조건 대입 → "전이" 개념이 없어
이벤트를 낼 지점이 없음.

**운용 영향**: RF 링크가 끊기거나 열화/복구돼도 **기체측 이벤트 로그가
전혀 안 남음**. 지상은 SH 다운링크의 `link_state` 필드로 상태 자체는
볼 수 있으나(값은 정상 갱신됨), 엣지 트리거 알림과 기체측 사후분석
로그가 없음. `SUB_ERR_EID` 부재는 더 위험 — 초기화 시 구독 실패하면
`return Status`만 하고 조용히 넘어가, 앱이 메시지를 못 받는 상태로
떠 있을 수 있음.

---

## [상] F-2. stale TODO — "CONFIG 커맨드 미배선"인데 실제론 배선됨

**주석 주장** — `lora_tdm_app/fsw/src/lora_tdm_app.h:68`:
> `§8 단계적 전환: 기본 v1 유지, CONFIG 명령으로 전환(TODO — 아직 커맨드
> 미배선, 지금은 memset(0)으로 항상 v1).`

**코드 실제** — 두 경로로 이미 배선됨:
- `lora_tdm_app_cmds.c:36` — `SET_DOWNLINK_PROTO_CC` 핸들러
- `lora_tdm_app_utils.c:720` — `CONFIG_CMD_MID`의 `PARAM_DOWNLINK_PROTOCOL`

오늘 실기체로도 검증됨(`lora_tdm.downlink_protocol` CONFIG 수락 확인).

**영향**: 이 주석만 읽은 사람은 v2 전환이 불가능하다고 오판. 실제로는
이번 세션 내내 이 경로로 테스트했음.

---

## [중] F-3. stale TODO — DL2 SysTime 블록 "미지원"인데 구현됨

**주석 주장** — `lora_tdm_app/fsw/src/lora_tdm_app_utils.h:72`:
> `SysTime 확장 블록(§4.2) 없이 기본 47B만 지원(TODO: FC_SYS_TIME_MID
> 구독 후 추가).`

**코드 실제** — 전부 구현돼 있음:
- 구독: `lora_tdm_app.c:471` (`FC_SYS_TIME_MID_VALUE` = 0x1909)
- 캐시: `lora_tdm_app_utils.c:773-782`
- 인코딩: `lora_tdm_app_utils.c:194-236` (`IncludeSysTime`, `DL2_FLAG_SYSTEM`
  플래그 세팅, 버퍼 부족 시 47B 폴백까지 처리)

Python 디코더 쪽 `test_systime_block` 테스트도 통과 중.

---

## [중] F-4. 죽은 config 상수 — `LORA_TDM_APP_LORA_BAUDRATE`가 무시됨

- 정의: `lora_tdm_app/config/default_lora_tdm_app_mission_cfg.h:31` = `57600`
- 실제: `lora_tdm_app.c:35`가 `speed_t Baud = B57600;`로 **하드코딩**,
  위 상수를 전혀 참조하지 않음

현재는 두 값이 우연히 일치해 문제가 안 드러나지만, 설정만 바꾸고
"반영됐다"고 믿으면 조용히 실패하는 함정. **오늘 mavlink_bridge 쪽에서
겪은 baud 문제와 정확히 같은 계열**(그쪽은 `SERIAL_BAUDRATE` 상수를
제대로 읽어서 정상 동작).

---

## [하] F-5. 죽은 config 상수 — `SERIAL_REOPEN_DELAY_MS` 미사용

- 정의: `default_lora_tdm_app_mission_cfg.h:11` = `1000`
- 코드 참조 0건. 실제 재오픈은 `RunCycle()` 진입마다 `LoRaFd < 0`이면
  **지연 없이** 즉시 시도(`lora_tdm_app.c:377-385`).

장애 지속 시 매 200ms 사이클마다 `open()` 재시도 → 설정된 1초 백오프가
적용 안 됨.

---

## [하] F-6. 기타 미사용 정의

| 앱 | 상수/EID | 비고 |
|---|---|---|
| lora_tdm_app | `LORA_TDM_APP_PROTOCOL_VERSION` | 미참조 |
| lora_tdm_app | `LORA_TDM_APP_MAX_PAYLOAD_LENGTH` | 미참조(별도 `DL2_*` 상수 사용 중) |
| mavlink_bridge_app | `STREAM_REACQUIRE_TIMEOUT_MS` | 미참조 — 스트림 재획득 타임아웃이 실제로 강제되지 않음 |
| cfs_core_app | `CFS_CORE_APP_ROUTE_MAX_WAYPOINTS` | 미참조 |
| cfs_core_app / uplink_app | `CAMERA_ID_LEN`, `IMAGE_ID_LEN`, `ARTIFACT_REF_LEN` | `legacy/img_app` 잔재로 추정 |
| cfs_core_app | `CFS_CORE_APP_PUBLISH_EID` | 정의만, 미발생 |

---

## [하] F-7. 문서의 stale 식별자 — `uplink_lora_test_status.md`

- `:100` — `MID UPLINK_RAW=0x1909` → **틀림**. 0x1909는 `FC_SYS_TIME_MID`.
  지상→기체 uplink 포워딩 MID는 `UPLINK_APP_CMD_MID = 0x18D0`.
  (같은 줄의 `CONFIG_CMD=0x190E`는 정확함)
- `:26, :37, :56, :98` — `lora_fc_downlink_app` / `lora_fc_downlink_app_utils.c`
  참조. **그 앱은 존재하지 않음**(`lora_tdm_app`으로 대체됨,
  `mission_app_runtime_spec.md:1671`에 전환 명시).

---

## 문제없음으로 확인된 항목

- **MID 충돌 없음**: 4개 앱 전체 topicid 값 대조 결과 중복은 전부
  publisher/subscriber가 같은 토픽을 각자 이름으로 정의한 정상 케이스
  (0x08E0 HK, 0x1905~0x1908 FC 상태 등). 0x1909도 `mavlink_bridge_app`↔
  `lora_tdm_app` 양쪽이 `FC_SYS_TIME`으로 일관.
- **미사용 MID 없음**: 초기 탐지된 8건은 전부 `_MID` ↔ `_MID_VALUE`
  alias 체인에 의한 오탐.
- **다운링크 필드 하드코딩 없음**: `BuildShDownlinkLine()` 전 필드가
  실제 상태에서 채워짐.

---

## TODO (착수 전 — 반드시 관련 spec 먼저 확인)

> 아래 목록만 보고 바로 구현하지 말 것. 각 항목의 "관련 spec"을 먼저
> 읽고 기존 결정사항(TDM 타이밍 제약, UFB 판정 지속 정책 등)과
> 충돌하지 않는지 확인 후 범위를 정할 것.

- [ ] **F-1a**: `UpdateLinkState()`에 이전 상태 보존 + 전이 시에만
      `LINK_LOST/DEGRADED/RESTORED_EID` 발생하도록 구현
      → 관련 spec: `lora_tdm_app_behavior_spec.md` §11(링크 상태 관리),
      §13(EID 표). **주의**: 5Hz 사이클이라 플래핑 시 이벤트 폭주 가능 —
      전이 이벤트에 히스테리시스/레이트리밋 필요한지 spec에서 정할 것
- [ ] **F-1b**: `SUB_ERR_EID`/`PIPE_ERR_EID`/`SB_SEND_ERR_EID`를 각
      실패 지점에서 실제로 발생시키도록 연결(초기화 구독 실패가 조용히
      넘어가는 문제 해소)
- [ ] **F-1c**: 위 구현 후 spec의 ✅ 표기가 실제와 맞는지 재확인
      (지금은 spec이 과장 — 구현 안 하려면 표기를 정정해야 함)
- [ ] **F-2 / F-3**: stale TODO 주석 2건 제거 또는 "구현 완료" 문구로 정정
      (코드 변경 없음, 주석만)
- [ ] **F-4**: `OpenSerial()`이 `LORA_TDM_APP_LORA_BAUDRATE`를 실제로
      읽도록 수정(`mavlink_bridge_app`의 `GetBaudConstant()` 방식 참고)
      → 관련: `mavlink_bridge_app_utils.c`의 baud 상수 매핑 함수
- [ ] **F-5**: 재오픈 백오프에 `SERIAL_REOPEN_DELAY_MS` 적용, 또는
      상수를 삭제하고 "지연 없음"을 spec에 명시
- [ ] **F-6**: 미사용 상수 정리 — img_app 잔재(`CAMERA_ID_LEN` 등)는
      삭제, `STREAM_REACQUIRE_TIMEOUT_MS`는 원래 의도한 기능이
      누락된 건지 확인 필요(단순 삭제 전 spec 확인)
- [ ] **F-7**: `uplink_lora_test_status.md`의 stale MID/앱 이름 정정
      (문서만, 코드 무관)

## 관련
- `notes/temp/command_dead_end_audit_2026-07-21.md` (1차)
- `notes/temp/ground_controllable_capability_plan_2026-07-21.md` (2차 + 계획)
- `notes/temp/openmct_repo_gap_audit_2026-07-21.md` (지상국 repo)
- `notes/lora_tdm_app_behavior_spec.md` §11, §13
- `notes/uplink_lora_test_status.md`
