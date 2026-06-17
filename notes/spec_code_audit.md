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
| 1-4 | Pub/Sub | spec §4,§5 | `mavlink_bridge_app.c:142` | (미언급) | `CONFIG_CMD_MID 0x190E` 구독 | ✅ **해결** | `mavlink_bridge_app_behavior_spec.md` §4.1에 SB 구독 목록 표 추가 (2026-06-17) |
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
| 3-6 | 이벤트 | (EID 표 없음) | eventids.h | — | `SEQ_FAIL_EID 12` 정의·로직 미구현 | ✅ **해결** | `lora_tdm_app_behavior_spec.md` §13에 EID 1~19 전체 표 추가; `SEQ_FAIL_EID 12`는 ⚪ 미구현(`lora_tdm_app_utils.c:295` `(void)SeqEcho`)으로 명시 (2026-06-17) |

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
| `0x190C` | `RECOVERY_CMD_MID` | uplink_app → **cfs_core_app** (2026-06-17 구독 추가, §4-7 해소) | ✅ |
| `0x190D` | `VIEWPOINT_CMD_MID` | uplink_app→cfs_core_app | ✅ |
| `0x190E` | `CONFIG_CMD_MID` | uplink_app→cfs_core/mavlink_bridge | ✅ |
| `0x190F` | `MODE_CMD_MID` | uplink_app → **cfs_core_app** (2026-06-17 구독 추가, §4-7 해소) | ✅ |
| `0x1910` | `DIAGNOSTIC_CMD_MID` | uplink_app → **lora_tdm_app** (2026-06-17 구독 추가, §4-7 해소) | ✅ |
| `0x1911` | `LORA_TDM_APP_LINK_STATUS_MID` (구 `0x190F`, 충돌 해소 재할당) | lora_tdm_app | 미배포 |

> `0x190C`~`0x1910` 라우팅 명령 MID와 `0x1909`는 `mission_app_runtime_spec.md` MID 표에 미수록(§4-2). `0x190F` 이중 할당은 §4-1.

---

## 종합 요약 (2026-06-17 갱신)

**❌ 항목 — 2026-06-17 추가 해결:**
- ✅ **4-7 RECOVERY/MODE/DIAGNOSTIC dead-end** → `cfs_core_app`에 RECOVERY(0x190C)/MODE(0x190F) 구독·핸들러 추가, `lora_tdm_app`에 DIAGNOSTIC(0x1910) 구독·핸들러 추가 (18파일, commit `e9957e9`).
- ✅ **§11.1 recovery authority vs 코드** → `mission_app_runtime_spec.md` §11.1 표를 코드 기준으로 재작성: cfs_core_app이 mavlink_bridge_app만 재시작(CFE_ES_RestartApp 1곳); 다른 앱 미구현 명시, 구현 상태 컬럼 추가 (commit `faf30ef`).
- ✅ **4-4/4-5 앱 집합 갱신** → `mission_app_runtime_spec.md` §2/§4/§5/§11/§13/§16/§17/§18 전반에서 `lora_fc_downlink_app` → `lora_tdm_app` 갱신 (commit `faf30ef`).
- ✅ **1-4 CONFIG_CMD_MID 미문서화** → `mavlink_bridge_app_behavior_spec.md` §4.1에 SB 구독 목록 표 추가 (commit 진행 중).

부록 A MID 인벤토리 갱신 (2026-06-17): `0x190C`/`0x190F`/`0x1910` — dead-end 해소 (cfs_core_app/lora_tdm_app 구독 추가).

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

**신규 발견 (2026-06-17):**
- ✅ **해결 — `cfs_core_app` health FAILED 고착 (실 Pi 런타임에서 발견, 배포 설정 누락)** — 부팅 30초 후 `health 2->3 fault=1`(BRIDGE_TIMEOUT)로 빠진 뒤 다시는 회복되지 않음을 매 테스트 실행마다 관찰. 코드 확인: `BridgeTimedOut = !BridgeState.Received || ...`(`cfs_core_app_utils.c:233`)이고 `BridgeState.Received`는 `mavlink_bridge_app`의 `BRIDGE_HK`(0x08A0)를 한 번이라도 받아야 `true`가 됨. HK는 `SEND_HK` 명령(보통 `SCH_LAB` 스케줄러가 트리거)이 있어야 발행되는데, 실제 빌드의 `~/cFS_clean/apps/sch_lab/fsw/tables/sch_lab_table.c`(NASA 표준 sch_lab 기본 테이블)를 확인한 결과 **빈 placeholder이고 cFE 코어 서비스 HK 예시조차 전부 주석 처리**되어 있었음. `mission_defs/`에 이 테이블의 override가 없어 우리 커스텀 앱 4개 전부 SEND_HK가 스케줄된 적이 없었음 — `mavlink_bridge_app`은 실제로 FC 텔레메트리를 정상 디코드 중이었으나(로그로 확인), `cfs_core_app`이 그걸 확인할 방법이 없어 영원히 타임아웃으로 오판한 것. 코드 버그 아니라 배포 설정 누락. **조치**: `mission_defs/tables/cpu1_sch_lab_table.c` 신규 추가 — `mavlink_bridge_app`/`cfs_core_app`/`uplink_app`/`lora_tdm_app` 4개 앱의 `SEND_HK` MID를 ~1Hz로 스케줄링(`cpu1_cfe_es_startup.scr`와 동일한 `cpu1_<filename>` override 명명 규칙 사용).

**신규 발견 (2026-06-16):**
- ❌ **4-7 RECOVERY/MODE/DIAGNOSTIC 명령 dead-end** — `uplink_app`이 검증·라우팅까지는 하지만, `RECOVERY_CMD_MID`/`MODE_CMD_MID`/`DIAGNOSTIC_CMD_MID`를 구독하는 앱이 코드베이스 전체에 없음. 지상국이 이 3개 클래스 명령을 보내도 실제로 처리되지 않음.
- ❌ **`mission_app_runtime_spec.md` §11.1 vs 코드** — spec은 `cfs_core_app`을 "모든 앱의 복구 권한(recovery authority)"로 설계(반복 앱 오류 시 60초/앱당 3회로 재시작)했으나, 코드의 `CFE_ES_RestartApp` 호출은 전체에서 1곳뿐이고 대상이 `mavlink_bridge_app`으로 하드코딩됨(5초/3회). `uplink_app`/`lora_fc_downlink_app`에 대한 감시·재시작·에스컬레이션 수신 메커니즘 없음. §11.1 표를 코드 기준으로 정정 필요.
- ✅ **해결(3차 검증까지 완료) — `lora_tdm_app` SB Msg Limit Err (실 Pi 런타임에서 발견)** — `LORA_TDM_PIPE` 구독이 전부 `CFE_SB_Subscribe()`(기본 limit=4)라서, 1차로 FC_* 4개 MID(5Hz)에서 발생 확인 → `CFE_SB_SubscribeEx(MsgLim=10)`으로 수정 → 재검증 결과 그 4개는 해결됐으나 `SYSTEM_HEALTH_MID`에서 동일 에러 16회 추가 발견 → `SYSTEM_HEALTH_MID`도 `MsgLim=20`으로 수정(`cfs_core_app`이 1Hz가 아니라 FC 입력마다 강제 발행함을 코드로 확인, `cfs_core_app_utils.c:193`) → 3차 재검증 결과 에러가 줄었으나 부팅 시점에 `0x1905`/`0x1906`까지 재발. 전체 로그 분석 결과 **에러 16건 전부 부팅 후 130ms 안에만 발생, 이후 60초+ 0건** — 지속 문제 아니라 1회성 부팅 버스트로 확인. 근본 원인: `mavlink_bridge_app`이 `/dev/serial0`를 열 때 cFS가 꺼져있던 동안 FC가 보낸 누적 데이터를 한 번에 드레인(MsgLim을 올려도 다운타임이 길면 버스트가 커져 근본 해결 안 됨). **최종 조치**: `mavlink_bridge_app_utils.c`의 `OpenSerial()`에 `tcflush(Fd, TCIFLUSH)` 추가해 포트 open 시 묵은 입력 버퍼를 비움(버스트 자체를 제거). 상세는 `lora_tdm_app_behavior_spec.md` §5.1.

위 2건(4-7, §11.1)은 spec 정정이 아니라 **실제 기능 격차**라 코드 작업(라우팅 대상 구현 또는 spec에 미구현 명시) 필요 — 사용자 확인 후 진행. Msg Limit Err 건은 코드 수정 완료(위 ✅).

---

## 배포 런타임 이슈 (2026-06-17)

### 🔴 R-1: cFS 시작 불가 — `OS_API_Init()` failure (RT 스케줄링 권한 없음)

**발견**: 2026-06-17 Pi 재부팅 후 `./core-cpu1` 즉시 abort.

**증상**:
```
CFE_PSP: OS_API_Init() failure
```
`OSAL_CONFIG_DEBUG_PRINTF=TRUE` 재빌드(`native/default_cpu1` 서브cmake) 후 상세 출력:
```
OS_Posix_TaskAPI_Impl_Init():412: Could not setschedparam in main thread: Operation not permitted (1)
OS_API_Init():146: OS_API_Impl_Init(0x1) failed to initialize: -1
```

**근본 원인**: OSAL이 `SCHED_RR`(Policy 2) 실시간 우선순위(`setschedparam`)를 요구하는데, 일반 사용자는 `CAP_SYS_NICE` capability 없이 RT policy 설정 불가. Pi 재부팅으로 `cap_sys_nice` capability가 초기화되었거나, 이전 실행 시에는 `sudo`나 capability 설정이 있었던 것으로 추정.

**전제 확인 사항**:
- `fs.mqueue.msg_max=10`(기본값)이 너무 낮아 보였으나 실제 원인 아님 (256으로 올렸어도 동일 실패).
- `OSAL_CONFIG_DEBUG_PRINTF`는 top-level cmake(`~/cFS_clean/build/`)가 아닌 **`~/cFS_clean/build/native/default_cpu1/`** 에서 설정해야 적용됨.

**해결 방법 (둘 중 하나 선택)**:

| 방법 | 명령 | 장단점 |
|---|---|---|
| **A: `cap_sys_nice` capability 부여 (권장)** | `sudo setcap cap_sys_nice+eip ~/cFS_clean/build/exe/cpu1/core-cpu1` | RT 스케줄링 그대로 유지. `make install` 후 매번 재설정 필요. |
| **B: OSAL Permissive Mode** | `cd ~/cFS_clean/build/native/default_cpu1 && cmake -DOSAL_CONFIG_DEBUG_PERMISSIVE_MODE=TRUE . && make -j4 && make install DESTDIR=~/cFS_clean/build` | RT 실패를 무시하고 계속 실행. RT 스케줄링 비활성화 — 앱 타이밍 정밀도 저하 가능. |

**추가 조치**:
- `sch_lab_table.tbl`: `make tabletool-execute` + 수동 복사로 `~/cFS_clean/build/exe/cpu1/cf/`에 배포 완료 (2026-06-17).
- `fs.mqueue.msg_max=256`, `fs.mqueue.queues_max=512`: `/etc/sysctl.conf`에 영구 추가 완료 (2026-06-17).

**✅ 해결 완료 (2026-06-17)**: `sudo ./core-cpu1`으로 실행. `setcap`은 `make install` 후 소멸되어 채택하지 않음.

---

### 🔴 R-2: `TargetSystemId` 덮어쓰기 버그 — 다중 MAVLink 장치 환경에서 스트림 요청 불안정

**발견**: 2026-06-17 FC 연결 후 로그에서 복수의 sysid 관찰.

**증상**:
```
requesting telemetry streams from sys=105 comp=0
requesting telemetry streams from sys=1 comp=1
requesting telemetry streams from sys=4 comp=0
```
스트림 요청 대상이 매 하트비트마다 바뀜. `health` BRIDGE_TIMEOUT 미해소.

**근본 원인** (`mavlink_bridge_app_utils.c:1066-1070`):
```c
if (MAVLINK_BRIDGE_APP_Parser.MsgId == MAVLINK_MSG_ID_HEARTBEAT)
{
    MAVLINK_BRIDGE_APP_Data.TargetSystemId    = MAVLINK_BRIDGE_APP_Parser.SysId;  // 무조건 덮어씀
    MAVLINK_BRIDGE_APP_Data.TargetComponentId = MAVLINK_BRIDGE_APP_Parser.CompId;
```
MAVLink 버스에 여러 장치가 하트비트를 보내면 (sys=1 FC, sys=105/sys=4 주변기기) `TargetSystemId`가 매번 마지막 하트비트 발신자로 교체됨. 결과적으로 스트림 요청이 실제 FC(sys=1)가 아닌 다른 장치에도 전송되고, FC의 텔레메트리 스트림이 중단·재개를 반복.

**연쇄 효과**:
- `LastRxTimestampMs`는 하트비트 수신만으로도 업데이트되므로 BRIDGE_TIMEOUT은 발생하지 않음
- 그러나 ATTITUDE/EKF/GPS 스트림이 끊어져 `cfs_core_app`의 `AttitudeState.Received`/`GpsState.Received` 미갱신
- `health` BRIDGE_TIMEOUT과는 별도로 FC 텔레메트리 기반 기능(웨이포인트, EKF 판단 등) 동작 불가

**✅ 해결 완료 (2026-06-17)** (`mavlink_bridge_app_utils.c:1065-1084`):
- HEARTBEAT 수신 시 `autopilot` 필드(payload[5])로 FC 식별: `3`(ArduPilot) 또는 `12`(PX4)인 경우만 `TargetSystemId` lock-in
- 이후 하트비트는 `SysId == TargetSystemId`인 경우만 처리 (주변기기 완전 무시)

**검증 결과** (Pi 실 동작):
- 수정 전: `sys=1/105/4` 혼재, FC 텔레메트리 끊김, `health 2` 지속
- 수정 후: `sys=1`로만 스트림 요청, `LOCAL_POSITION_NED`·`ATTITUDE`·`GPS_RAW_INT` 수신, `health 2->1` NOMINAL 복귀 확인
