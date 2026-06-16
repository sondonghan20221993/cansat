# spec ↔ 코드 정합성 점검 (spec_code_audit)

- **점검일**: 2026-06-16
- **위치**: WSL 홈 사본 (`/home/sdh2983/cfs-telemetry-app`, origin 동기화본)
- **기준**: 코드 = 진실. spec을 코드에 맞춤.
- **모드**: 보고 우선 (본 문서엔 findings만 기록, spec 편집은 승인 후 별도 진행)
- **판정**: ❌ 실제 불일치 / ⚠️ aspirational(의도된 목표값) / 🕒 미구현(문서만) / ✅ 일치 확인
- **카테고리**: MID 이름·값 / 명령·fcncode / 이벤트ID / Pub·Sub / 설정·한도 / 페이로드 구조체 / 앱 등록·우선순위

## 진행 체크리스트
- [x] 1. `mavlink_bridge_app` ↔ `mavlink_bridge_app_behavior_spec.md`
- [x] 2. `cfs_core_app` ↔ `cfs_core_app_behavior_spec.md`
- [x] 3. `lora_tdm_app` ↔ `lora_tdm_app_behavior_spec.md`
- [x] 4. (교차) 통합 패스 ↔ `mission_app_runtime_spec.md`

---

## 1. `mavlink_bridge_app`

**코드 권위 요약**
- Publish MID: `FC_EKF_LOCAL_STATE_MID 0x1905`, `FC_ATTITUDE_STATE_MID 0x1906`, `FC_GPS_RAW_STATE_MID 0x1907`, `FC_EKF_STATUS_MID 0x1908`, `MAVLINK_BRIDGE_APP_HK_TLM_MID 0x08A0`
- Subscribe MID: `MAVLINK_BRIDGE_APP_CMD_MID 0x18A0`, `SEND_HK 0x18A1`, `ROUTE_UPDATE_MID 0x190B`, `CONFIG_CMD_MID 0x190E` (`mavlink_bridge_app.c:124,130,136,142`)
- CC: `NOOP_CC 0`, `RESET_COUNTERS_CC 1`, `MISSION_QUERY_CC 2`
- EID: 1~12 (STARTUP·COMMAND_ERR·NOOP·RESET·LINK·PARSE·STREAM·MISSION_UPLOAD_INF/ERR·MISSION_DOWNLOAD_INF/ERR·ARMED_WARN)
- stream request interval: ATTITUDE 200ms(5Hz), LOCAL/GLOBAL_POS 200ms, GPS_RAW 500ms(2Hz), EKF_STATUS 500ms(2Hz)

| # | 카테고리 | spec 위치 | 코드 위치 | spec 값 | 코드 값 | 판정 | 권고 |
|---|---|---|---|---|---|---|---|
| 1-1 | MID | spec §4,§10 | interface_cfg_values.h | `ROUTE_UPDATE_MID 0x190B`, `CMD 0x18A0` | 동일 | ✅ | — |
| 1-2 | 명령 CC | spec §10 | fcncodes | `MISSION_QUERY_CC=2` | `=2` | ✅ | — |
| 1-3 | 이벤트 | spec §5,§9,§11,§14 | eventids.h | `ARMED_WARN 12`, `PARSE`, `MISSION_*` | 동일 | ✅ | — |
| 1-4 | Pub/Sub | spec §4,§5 | `mavlink_bridge_app.c:142` | (미언급) | `CONFIG_CMD_MID 0x190E` 구독 | ❌ | behavior spec에 `CONFIG_CMD_MID` 구독(런타임 config 명령) 문서화 추가 |
| 1-5 | 설정·한도 | spec §8,§13.0 | `mavlink_bridge_app_utils.c:40-41,63-65` | timeout 2000/retry 3/CLEAR_DELAY 300, sysid 255/compid 190 | 동일 (로컬 `#define`, config 헤더 아님) | ✅ **해결** | 정의 위치 확인 완료, 값 일치. §8/§13.0에 정의 위치 주석 추가 (2026-06-16) |
| 1-6 | 게시율 | (mission spec §5.1.1) | internal_cfg_values.h | ATTITUDE ~20Hz, GPS ~5Hz, EKF ~10Hz | 요청 interval 5Hz/2Hz/2Hz | ❌ | **pass 4에서 처리**: 5.1.1 게시율을 코드 stream interval과 정합 |
| 1-7 | 책임 분리 | spec §2.1 | `fsw/src/*.c` 전체, `platform_cfg.h` | `LoRaFd`/`LoRaTxCount`/`ServiceLoRa` 제거됨 | src 잔재 없음 확인. `platform_cfg.h`의 미사용 `LORA_SERIAL_PATH`/`LORA_BAUDRATE`는 **코드에서 제거** | ✅ **해결(코드 수정)** | src 검증 완료, dead config 2건 삭제 (2026-06-16) |
| 1-8 | spec 내부 | spec §13.1.1/§332/§341 vs §15 | `mavlink_bridge_app_utils.c:341,368` | INT 경로 global frame "미구현" ↔ §15 "구현 완료" | `SendMissionItemInt`=LOCAL_NED(미변환), `SendMissionItem`=GLOBAL_RELATIVE_ALT(변환) — 서로 다른 경로, 모순 아님 | ✅ **해결** | §15 "구현 완료" 항목에 "Legacy 경로(msg 39) 한정" 명시해 모호성 해소 (2026-06-16) |

> 종합: MID/CC/EID 핵심 계약은 코드와 **일치**. 실 불일치는 ❌1-4(CONFIG_CMD_MID 미문서화), ❌1-6(게시율, pass 4). ⚠️1-8/🕒1-5/🕒1-7은 전부 해결(2026-06-16).

## 2. `cfs_core_app`

**코드 권위 요약**
- Publish MID: `SYSTEM_HEALTH_MID 0x1904`, `CFS_CORE_APP_HK_TLM_MID 0x08C0`
- Subscribe (dispatch.c): `CMD 0x18C0`, `SEND_HK 0x18C1`, `BRIDGE_HK 0x08A0`, `FC_* 0x1905~0x1908`, `ROUTE_UPDATE 0x190B`, **`CONFIG_CMD_MID 0x190E`** (dispatch.c:39), **`VIEWPOINT_CMD_MID 0x190D`** (dispatch.c:45)
- CC: `NOOP 0`, `RESET_COUNTERS 1`
- EID: 1~12 (…`HEALTH_TRANSITION 7`, `SEQ_ERR 8`, `TIMESTAMP_ERR 9`, `BRIDGE_RESTART 10`, `VIEWPOINT 11`, `SEQ_GAP 12`)
- internal_cfg: timeout 200/1000/att2000/local2000/gps3000/ekf2000/bridge3000, `NOMINAL_STABILITY 10000`, `FAILED_ESCALATION 30000`, `TIMESTAMP_MAX_FUTURE 5000`, **`BRIDGE_RESTART_INTERVAL 5000`/`BRIDGE_MAX_RESTARTS 3`/`BRIDGE_APP_NAME "mavlink_bridge_app"`**, **`STATE_FILE_PATH`/`STATE_MAGIC`** (영속화)

| # | 카테고리 | spec 위치 | 코드 위치 | spec | 코드 | 판정 | 권고 |
|---|---|---|---|---|---|---|---|
| 2-1 | MID/timing | §6,§9 | msgid/internal_cfg | 0x18C0/0x18C1/0x08C0/0x1904/0x190B, timeout 일습 | 동일 | ✅ | — |
| 2-2 | Pub/Sub | §6.1 | dispatch.c:39,45 | (미언급) | `CONFIG_CMD_MID 0x190E`, `VIEWPOINT_CMD_MID 0x190D` 구독·처리 | ❌ | §6.1 구독 목록에 두 MID 추가 |
| 2-3 | 명령 처리 | §17,§4 | dispatch.c:39-49 | "NOOP/RESET만 지원" | `ProcessConfigCommand`, `ProcessViewpointCommand` 존재 | ❌ | §17에 config·viewpoint 명령 처리 반영 |
| 2-4 | 복구/재시작 | §4,§14.4 | internal_cfg:16-18, EID 10 | "다른 앱 재시작 안 함, 추가 복구 부작용 없음" | bridge auto-restart 상수·`BRIDGE_RESTART_EID` 구현 | ❌ | §4/§14.4 갱신: bridge 재시작 구현 반영 |
| 2-5 | 상태 영속화 | §4,§20 | internal_cfg:19-20 | "재시작 후 헬스 상태 지속 안 함" | `STATE_FILE_PATH`/`STATE_MAGIC` 존재 | ❌ | §4/§20 재검토: 상태 파일 영속화 여부 명시 |
| 2-6 | FAILED 상태 | §12.7,§20 vs §10 | internal_cfg:14 | §12.7/§20 "FAILED 미생성" ↔ §10 "escalation 시 FAILED" | `FAILED_ESCALATION_MS 30000` 존재 | ⚠️ | §12.7/§20 stale 가능: FAILED 생성 여부 코드 확인 후 통일 |
| 2-7 | seq/timestamp 검사 | §7,§20 | EID 8/9/12, internal_cfg:15 | "seq 단조·timestamp 유효성 검사 미구현" | `SEQ_ERR/SEQ_GAP/TIMESTAMP_ERR_EID`, `TIMESTAMP_MAX_FUTURE 5000` | ⚠️ | 해당 EID 실사용 여부 확인 후 §7/§20 갱신 |
| 2-8 | spec 내부 | §12.5/§13.2 vs §21.5 | — | GPS 헬스 비반영 ↔ §21.5 표 "GPS 불가용→DEGRADED/FAULT_GPS_STALE" | — | ⚠️ | §21.5 표에서 GPS행 보고전용으로 정정 |
| 2-9 | spec 내부 | §18 | — | 테스트 목록 "local/attitude를 FAULT_EKF_INVALID로 분류"(구) ↔ §13/§21.5 분리 fault | — | ⚠️ | §18 커버리지 목록을 현재 fault 분리로 갱신 |

> 종합: cfs_core_app **코드가 spec보다 앞섬**. spec은 구버전(단순) 동작을 기술 → config/viewpoint 명령·bridge 재시작·상태 영속화·FAILED escalation·seq/timestamp 검사가 코드에 추가됨. ❌2-2~2-5가 핵심 불일치, ⚠️2-6~2-9는 spec 내부 staleness.
>
> ✅ **해결됨 (2026-06-16)**: 소스(`cfs_core_app.c`/`utils.c`/`cmds.c`/`dispatch.c`) 실동작 확인 후 `cfs_core_app_behavior_spec.md` 갱신 — 2-2(§6.1 CONFIG/VIEWPOINT 구독), 2-3(§17 config·viewpoint 처리), 2-4(§13.1·§14.4 bridge 재시작), 2-5(§14.5 영속화 신설), 2-6(§12.7 FAILED 생성), 2-7(§7.1 seq/timestamp 검사), 2-8(§21.5 GPS 보고전용), 2-9(§18 테스트목록), §2/§4/§9/§10/§20/§21.2 동반 정정. **검증: 단위테스트(`coveragetest_*`)가 GPS 비저하·FAULT 분리·FAILED·timestamp·config를 실제 검증함 확인.**

## 3. `lora_tdm_app`

**코드 권위 요약**
- Subscribe MID: `CMD 0x18E0`, `SEND_HK 0x18E1`, `SYSTEM_HEALTH 0x1904`, `FC_* 0x1905~0x1908`
- Publish MID: `HK_TLM 0x08E0`, `LINK_STATUS 0x190F`, `UPLINK_APP_CMD 0x18D0`(UP frame forward)
- 상수(mission_cfg): `CYCLE 1000`, `RX_WINDOW 300`, `LINK_LOSS_THR 3`, `LINK_TIMEOUT 5000`, `MAX_PAYLOAD 196`, baud 57600
- PacketType: `FC_STATE=1`, `SYSTEM_HEALTH=2`
- EID 1~18 (`SEQ_FAIL_EID 12` 정의, 로직 미구현)

| # | 카테고리 | spec 위치 | 코드 위치 | spec | 코드 | 판정 | 권고 |
|---|---|---|---|---|---|---|---|
| 3-1 | MID 값 | §5.1,§5.2 | topicid_values.h | 0x18E0/0x18E1/0x08E0/0x190F/0x1904~8/0x18D0 | 동일 | ✅ | — |
| 3-2 | TDM 상수 | §7,§14 | mission_cfg.h | 1000/300/3/5000, FB 0/1/2, link 0/1/2 | 동일 | ✅ | — |
| 3-3 | PacketType | §8 | mission_cfg.h:14 | "FC State (default = **0**)" | `FC_STATE_PACKET_TYPE = **1**` (SH=2) | ✅ **해결** | §8 표를 `FC_STATE=1`/`SYSTEM_HEALTH=2`로 정정 (2026-06-16) |
| 3-4 | 설정·한도 | §5.1 | `lora_tdm_app.c:268` | "파이프 깊이 **10**" | 코드 깊이 **50** | ✅ **해결** | §5.1을 깊이 50으로 정정 (2026-06-16). TEST_CASES.md baseline 주석도 갱신 |
| 3-5 | spec 내부 | §7.1 vs §15 | `lora_tdm_app_dispatch.c:71-75` | §7.1 "SEND_HK→HK+LinkStatus" ↔ §15 "SEND_HK→HK만" | **코드 수정**: SEND_HK 분기에 `LORA_TDM_APP_ReportLinkStatus()` 호출 추가 → §7.1이 정답이 되도록 코드를 바로잡음 (단위테스트 stub이 이미 `ReportLinkStatus` 호출 카운트를 추적 가능했던 것으로 보아 누락된 호출로 판단) | ✅ **해결(코드 수정)** | dead code였던 `ReportLinkStatus()`를 SEND_HK 경로에 연결. §7.1/§15 모두 "호출됨"으로 통일 (2026-06-16) |
| 3-6 | 이벤트 | (EID 표 없음) | eventids.h | — | `SEQ_FAIL_EID 12` 정의·로직 미구현 | ⚠️ | §15와 일관(경미). EID 표 추가 권장 |

> 종합: MID·TDM 상수 **완전 일치**. 실 불일치는 ❌3-3(PacketType 기본값), ❌3-4(파이프 깊이 10 vs 50), 모두 해결. ⚠️3-5도 해결(spec 내부 모순 통일 + dead code 발견 기록).

## 4. 교차 통합 패스 ↔ `mission_app_runtime_spec.md`

**배포 baseline (`mission_defs/cpu1_cfe_es_startup.scr`, 2026-06-16 갱신)**: `mav_bridge_app`(prio50), `cfs_core_app`(55), `uplink_app`(57), **`lora_tdm_app`(58, `lora_fc_dl_app` 대체)** + lab apps(ci/to/sch).
→ **`lora_tdm_app`·`telemetry_app`·`img_app`은 startup 미등록(미배포)**.

**uplink_app 명령 라우팅 MID (uplink_app/config)**: `UPLINK_STATUS 0x190A`, `ROUTE_UPDATE 0x190B`, `RECOVERY_CMD 0x190C`, `VIEWPOINT_CMD 0x190D`, `CONFIG_CMD 0x190E`, `MODE_CMD 0x190F`, `DIAGNOSTIC_CMD 0x1910`, `UPLINK_APP_LORA_RAW 0x1909`, HK `0x08D0`.

| # | 카테고리 | spec 위치 | 코드 위치 | spec | 코드 | 판정 | 권고 |
|---|---|---|---|---|---|---|---|
| 4-1 | **MID 충돌** | — | uplink_app:12 ↔ lora_tdm topicid:21 | — | `MODE_CMD_MID 0x190F`(uplink) = `LORA_TDM_APP_LINK_STATUS_MID 0x190F`(tdm) | ✅ **해결** | lora_tdm `LINK_STATUS_MID_VALUE` `0x190F`→**`0x1911`** 재할당(미배포 측 이동). uplink `MODE_CMD 0x190F` 유지. 문서(behavior spec/README) 동기화 완료 |
| 4-2 | MID 인벤토리 | §5.1.1,§17.1 | uplink_app msgid | (누락) | `RECOVERY_CMD 0x190C`, `VIEWPOINT_CMD 0x190D`, `CONFIG_CMD 0x190E`, `MODE_CMD 0x190F`, `DIAGNOSTIC_CMD 0x1910`, `UPLINK_APP_LORA_RAW 0x1909` | ✅ **해결** | §17.1에 FC 상태 MID·라우팅 명령 MID(0x190C~0x1910)·0x1909·0x1911 추가 (2026-06-16) |
| 4-3 (=1-6) | 게시율 | §5.1.1 | mavlink internal_cfg | ATTITUDE `~20Hz`, EKF `~10Hz`, GPS `~5Hz` | stream req ATTITUDE 5Hz(200ms), EKF 2Hz(500ms), GPS 2Hz(500ms) | ✅ **해결** | §5.1.1 게시율을 코드 stream 요청 간격으로 정정 + "FC 송신율 의존" 명시 (2026-06-16) |
| 4-4 | 앱 집합 | §4 | startup.scr | downlink 역할 = `lora_fc_downlink_app` | **변경**: startup.scr에서 `lora_fc_dl_app` 제거, `lora_tdm_app` 등록(prio 58) (2026-06-16) | ❌ | §4/§17 등 `lora_fc_downlink_app`을 downlink 역할 구현체로 서술하는 부분을 `lora_tdm_app`으로 갱신 필요 (대규모 — 별도 작업) |
| 4-5 | 배포 상태 | §2(현황) | startup.scr | — | `lora_tdm_app` baseline 등록됨(2026-06-16), `lora_fc_downlink_app`은 코드 보존·미배포로 전환. `telemetry_app`/`img_app` 미배포 유지 | 📝 | §2 범위/현황 갱신 필요 |

> **2026-06-16 배포 전환 후속 작업 (코드/문서 외부, 운영 단계):**
> - Pi에서 `bridge/lora_uplink_bridge.py`, `bridge/lora_telemetry_bridge.py` 프로세스 종료 필요 (둘 다 `lora_tdm_app`과 같은 LoRa serial을 점유하면 충돌).
> - Pi 크로스컴파일용 cFS 프레임워크의 앱 목록(`targets.cmake` 등, 이 저장소 밖)에 `lora_tdm_app` 추가 필요.
> - `mission_app_runtime_spec.md`는 `lora_fc_downlink_app`을 downlink 역할 구현체로 광범위하게 서술 중(§4,§6,§11,§17~18 등) — 전면 갱신은 별도 작업으로 분리.
| 4-6 | uplink 라우팅 | §18.4.x | dispatch 체인 | 명령 클래스 문서화됨 | `uplink_app`→(0x190C~0x1910)→`cfs_core_app`(viewpoint/config 구독) 실재 | ✅ | 클래스→MID 값 매핑만 보강(§4-2와 연계) |
| 4-7 | **dead-end 라우팅** | §18.4.x (명령 클래스 분류) | `uplink_app_utils.c` `ForwardRecoveryCommand`/`ForwardModeCommand`/`ForwardDiagnosticCommand` | spec: RECOVERY/MODE/DIAGNOSTIC 명령 클래스가 대상 앱으로 라우팅됨을 전제 | `RECOVERY_CMD_MID`(0x190C)/`MODE_CMD_MID`(0x190F)/`DIAGNOSTIC_CMD_MID`(0x1910) — `grep -rl` 결과 코드베이스 전체에서 **publish하는 uplink_app 자신 외 구독자가 0개**. `cfs_core_app`/`mavlink_bridge_app`/`lora_fc_downlink_app` 어디도 구독 안 함 | ❌ | uplink_app은 이 3개 클래스를 검증 통과시키고 "라우팅 성공"으로 카운트하지만 실제 수신·처리하는 앱이 없음(허공에 publish). 대상 앱에 구독 추가 또는 spec에 "미구현 라우팅 대상"으로 명시 필요 (2026-06-16 발견) |

> 종합: 핵심은 ❌4-1(**0x190F MID 충돌** — 최우선), ❌4-2(라우팅 MID 인벤토리 누락), ❌4-3(게시율), ❌4-7(**RECOVERY/MODE/DIAGNOSTIC 명령 dead-end** — 신규). §4 앱 집합·라우팅 체인 중 CONFIG/VIEWPOINT/ROUTE_UPDATE는 코드와 정합하나 나머지 3개 클래스는 수신처 없음.

---

## 부록 A. 마스터 MID 인벤토리 (코드 기준, 2026-06-16)

| 값 | 심볼 | 소유 앱 | 배포 |
|---|---|---|---|
| `0x08A0` | `MAVLINK_BRIDGE_APP_HK_TLM` | mavlink_bridge_app | ✅ |
| `0x08C0` | `CFS_CORE_APP_HK_TLM` | cfs_core_app | ✅ |
| `0x08D0` | `UPLINK_APP_HK_TLM` | uplink_app | ✅ |
| `0x08E0` | `LORA_TDM_APP_HK_TLM` | lora_tdm_app | 미배포 |
| `0x18A0/A1` | `MAVLINK_BRIDGE_APP_CMD/SEND_HK` | mavlink_bridge_app | ✅ |
| `0x18B0/B1` | `LORA_FC_DOWNLINK_APP_CMD/SEND_HK` (topic-id) | lora_fc_downlink_app | ✅ |
| `0x18C0/C1` | `CFS_CORE_APP_CMD/SEND_HK` | cfs_core_app | ✅ |
| `0x18D0/D1` | `UPLINK_APP_CMD/SEND_HK` | uplink_app | ✅ |
| `0x18E0/E1` | `LORA_TDM_APP_CMD/SEND_HK` | lora_tdm_app | 미배포 |
| `0x1904` | `SYSTEM_HEALTH_MID` | cfs_core_app | ✅ |
| `0x1905` | `FC_EKF_LOCAL_STATE_MID` | mavlink_bridge_app | ✅ |
| `0x1906` | `FC_ATTITUDE_STATE_MID` | mavlink_bridge_app | ✅ |
| `0x1907` | `FC_GPS_RAW_STATE_MID` | mavlink_bridge_app | ✅ |
| `0x1908` | `FC_EKF_STATUS_MID` | mavlink_bridge_app | ✅ |
| `0x1909` | `UPLINK_APP_LORA_RAW_MID` (= `LORA_FC_DOWNLINK..._UPLINK_RAW`) | lora_fc_downlink_app→uplink_app | ✅ |
| `0x190A` | `UPLINK_STATUS_MID` | uplink_app | ✅ |
| `0x190B` | `ROUTE_UPDATE_MID` | uplink_app→cfs_core/mavlink_bridge | ✅ |
| `0x190C` | `RECOVERY_CMD_MID` | uplink_app publish, **구독자 없음(dead-end, §4-7)** | ✅ |
| `0x190D` | `VIEWPOINT_CMD_MID` | uplink_app→cfs_core_app | ✅ |
| `0x190E` | `CONFIG_CMD_MID` | uplink_app→cfs_core/mavlink_bridge | ✅ |
| `0x190F` | `MODE_CMD_MID` | uplink_app publish, **구독자 없음(dead-end, §4-7)** | ✅ |
| `0x1910` | `DIAGNOSTIC_CMD_MID` | uplink_app publish, **구독자 없음(dead-end, §4-7)** | ✅ |
| `0x1911` | `LORA_TDM_APP_LINK_STATUS_MID` (구 `0x190F`, 충돌 해소 재할당) | lora_tdm_app | 미배포 |

> `0x190C`~`0x1910` 라우팅 명령 MID와 `0x1909`는 `mission_app_runtime_spec.md` MID 표에 미수록(§4-2). `0x190F` 이중 할당은 §4-1.

---

## 종합 요약 (2026-06-16)

**❌ 항목 — 전부 해결 (2026-06-16):**
- ✅ **4-1 `0x190F` MID 충돌** → lora_tdm `LINK_STATUS` `0x190F`→`0x1911` 재할당 (코드 수정, commit `2e81215`).
- ✅ **2-2~2-9 cfs_core_app** → config/viewpoint·bridge 재시작·영속화·FAILED·seq/timestamp 검사 등 코드 실동작 확인 후 `cfs_core_app_behavior_spec.md` 전면 정합 (commit `4d66241`).
- ✅ **4-2 MID 인벤토리** → mission spec §17.1에 FC 상태 MID·라우팅 명령 MID(`0x190C~0x1910`)·`0x1909`·`0x1911` 추가.
- ✅ **3-3 lora_tdm PacketType** → §8 `FC_STATE=1`/`SYSTEM_HEALTH=2` 정정.
- ✅ **3-4 lora_tdm 파이프 깊이** → §5.1 "10"→"50" (`lora_tdm_app.c:268`), TEST_CASES.md 정합 (commit `55262f5`).
- ✅ **4-3 / 1-6 게시율** → mission spec §5.1.1 게시율을 코드 stream 요청 간격(ATTITUDE 5Hz/EKF·GPS 2Hz)으로 정정 + FC 송신율 의존 명시.

**spec 내부 staleness (⚠️) — 전부 해소 (2026-06-16):** 2-6/2-7/2-8/2-9(cfs_core, commit `4d66241`), 3-5(lora_tdm §7.1/§15 SEND_HK 문구 통일 + `ReportLinkStatus()` dead code 발견), 1-8(mavlink INT/Legacy 경로 구분 명확화).

**확인 완료 (🕒 → ✅, 2026-06-16):** 1-5(sysid/timeout/retry/clear_delay 상수 — `mavlink_bridge_app_utils.c:40-41,63-65`에서 정의 확인, 값 일치), 1-7(mavlink src에 LoRa 잔재 없음 확인; `platform_cfg.h`의 dead config 2건 코드에서 제거).

**일치 확인 (✅):** 전 앱 MID 수치값, cfs_core timing, lora_tdm TDM 상수, 앱 집합/라우팅 체인.

**audit 중 발견되어 코드를 수정한 항목 (2026-06-16):**
- `lora_tdm_app_dispatch.c`: SEND_HK 분기에 `LORA_TDM_APP_ReportLinkStatus()` 호출 추가 (기존엔 dead code — `LINK_STATUS_MID 0x1911`이 전혀 게시되지 않았음).
- `mavlink_bridge_app/config/default_mavlink_bridge_app_platform_cfg.h`: 미사용 `MAVLINK_BRIDGE_APP_LORA_SERIAL_PATH`/`MAVLINK_BRIDGE_APP_LORA_BAUDRATE` 제거.

**`lora_tdm_app` 빌드/테스트 블로커 3건 해소 (2026-06-16, `tdm_refactor`→main 병합):** `cFS_clean` 빌드 환경에 한 번도 등록된 적이 없어 native unit-test가 실제로 실행된 적이 없었음(코드 정독 기준 "✓" 표시만 존재). 임시 등록 후 처음 빌드/실행해 버그 3건 발견·수정 — 상세는 `tests/TEST_CASES.md` `lora_tdm_app` 섹션 참고:
1. `lora_tdm_app_utils.h/.c` `ProcessRxLine`: `char*` → `const char*` (읽기 전용 함수, 테스트에서 문자열 리터럴 전달 시 컴파일 에러였음).
2. `coveragetest_lora_tdm_app_dispatch.c`: `CmdNoop`/`CmdReset` 테스트가 stub이 반영 안 하는 `CmdCounter`를 직접 검증하던 오류 → `UtAssert_STUB_COUNT`로 정정.
3. **실제 fsw 버그**: `ProcessUpFrame`의 `sscanf("%[^,]")`가 빈 payload(무페이로드 명령)를 파싱 못 해 전부 CRC_FAIL로 오판 → `,,` 대체 포맷 재시도 추가.
검증: native 빌드-환경에 임시 등록 → 4개 테스트 바이너리 75/75 PASS 확인 → 등록 해제(원상복구).

**audit 패스(1~4)의 기존 ❌/⚠️/🕒 항목 해결 완료 (문서 정정 + 코드 수정 2건 + 위 블로커 3건).**

**신규 발견 (2026-06-16):**
- ❌ **4-7 RECOVERY/MODE/DIAGNOSTIC 명령 dead-end** — `uplink_app`이 검증·라우팅까지는 하지만, `RECOVERY_CMD_MID`/`MODE_CMD_MID`/`DIAGNOSTIC_CMD_MID`를 구독하는 앱이 코드베이스 전체에 없음. 지상국이 이 3개 클래스 명령을 보내도 실제로 처리되지 않음.
- ❌ **`mission_app_runtime_spec.md` §11.1 vs 코드** — spec은 `cfs_core_app`을 "모든 앱의 복구 권한(recovery authority)"로 설계(반복 앱 오류 시 60초/앱당 3회로 재시작)했으나, 코드의 `CFE_ES_RestartApp` 호출은 전체에서 1곳뿐이고 대상이 `mavlink_bridge_app`으로 하드코딩됨(5초/3회). `uplink_app`/`lora_fc_downlink_app`에 대한 감시·재시작·에스컬레이션 수신 메커니즘 없음. §11.1 표를 코드 기준으로 정정 필요.
- ✅ **해결(3차 검증까지 완료) — `lora_tdm_app` SB Msg Limit Err (실 Pi 런타임에서 발견)** — `LORA_TDM_PIPE` 구독이 전부 `CFE_SB_Subscribe()`(기본 limit=4)라서, 1차로 FC_* 4개 MID(5Hz)에서 발생 확인 → `CFE_SB_SubscribeEx(MsgLim=10)`으로 수정 → 재검증 결과 그 4개는 해결됐으나 `SYSTEM_HEALTH_MID`에서 동일 에러 16회 추가 발견 → `SYSTEM_HEALTH_MID`도 `MsgLim=20`으로 수정(`cfs_core_app`이 1Hz가 아니라 FC 입력마다 강제 발행함을 코드로 확인, `cfs_core_app_utils.c:193`) → 3차 재검증 결과 에러가 줄었으나 부팅 시점에 `0x1905`/`0x1906`까지 재발. 전체 로그 분석 결과 **에러 16건 전부 부팅 후 130ms 안에만 발생, 이후 60초+ 0건** — 지속 문제 아니라 1회성 부팅 버스트로 확인. 근본 원인: `mavlink_bridge_app`이 `/dev/serial0`를 열 때 cFS가 꺼져있던 동안 FC가 보낸 누적 데이터를 한 번에 드레인(MsgLim을 올려도 다운타임이 길면 버스트가 커져 근본 해결 안 됨). **최종 조치**: `mavlink_bridge_app_utils.c`의 `OpenSerial()`에 `tcflush(Fd, TCIFLUSH)` 추가해 포트 open 시 묵은 입력 버퍼를 비움(버스트 자체를 제거). 상세는 `lora_tdm_app_behavior_spec.md` §5.1.

위 2건(4-7, §11.1)은 spec 정정이 아니라 **실제 기능 격차**라 코드 작업(라우팅 대상 구현 또는 spec에 미구현 명시) 필요 — 사용자 확인 후 진행. Msg Limit Err 건은 코드 수정 완료(위 ✅).
