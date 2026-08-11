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
→ **`telemetry_app`·`img_app`은 startup 미등록(미배포·코드 보존)**. `lora_fc_downlink_app`은 저장소에서 삭제됨(commit `7c080f1`, 2026-06-30). (`lora_tdm_app`은 2026-06-16부터 startup 등록·배포됨 — 구 문장의 `lora_tdm_app` 표기는 오기, 2026-07-05 정정)

**uplink_app 명령 라우팅 MID (uplink_app/config)**: `UPLINK_STATUS 0x190A`, `ROUTE_UPDATE 0x190B`, `RECOVERY_CMD 0x190C`, `VIEWPOINT_CMD 0x190D`, `CONFIG_CMD 0x190E`, `MODE_CMD 0x190F`, `DIAGNOSTIC_CMD 0x1910`, HK `0x08D0`. (`UPLINK_APP_LORA_RAW 0x1909`는 2026-07-14 코드에서 제거됨 — 아래 4-8 참조)

| # | 카테고리 | spec 위치 | 코드 위치 | spec | 코드 | 판정 | 권고 |
|---|---|---|---|---|---|---|---|
| 4-1 | **MID 충돌** | — | uplink_app:12 ↔ lora_tdm topicid:21 | — | `MODE_CMD_MID 0x190F`(uplink) = `LORA_TDM_APP_LINK_STATUS_MID 0x190F`(tdm) | ✅ **해결** | lora_tdm `LINK_STATUS_MID_VALUE` `0x190F`→**`0x1911`** 재할당(미배포 측 이동). uplink `MODE_CMD 0x190F` 유지. 문서(behavior spec/README) 동기화 완료 |
| 4-2 | MID 인벤토리 | §5.1.1,§17.1 | uplink_app msgid | (누락) | `RECOVERY_CMD 0x190C`, `VIEWPOINT_CMD 0x190D`, `CONFIG_CMD 0x190E`, `MODE_CMD 0x190F`, `DIAGNOSTIC_CMD 0x1910`, `UPLINK_APP_LORA_RAW 0x1909`(2026-07-14 제거, 4-8 참조) | ✅ **해결** | §17.1에 FC 상태 MID·라우팅 명령 MID(0x190C~0x1910)·0x190F·0x1911 추가 (2026-06-16) |
| 4-3 (=1-6) | 게시율 | §5.1.1 | mavlink internal_cfg | ATTITUDE `~20Hz`, EKF `~10Hz`, GPS `~5Hz` | stream req ATTITUDE 5Hz(200ms), EKF 2Hz(500ms), GPS 2Hz(500ms) | ✅ **해결** | §5.1.1 게시율을 코드 stream 요청 간격으로 정정 + "FC 송신율 의존" 명시 (2026-06-16) |
| 4-4 | 앱 집합 | §4 | startup.scr | downlink 역할 = `lora_fc_downlink_app` | **변경**: startup.scr에서 `lora_fc_dl_app` 제거, `lora_tdm_app` 등록(prio 58) (2026-06-16) | ✅ **해결** | `mission_app_runtime_spec.md` §2/§4/§5/§11/§13/§16/§17/§18 전반에서 `lora_fc_downlink_app` → `lora_tdm_app` 갱신, 잔존 언급은 전부 "삭제됨/역사 참고용"으로 명시 (commit `faf30ef`, 2026-07-14 확인) |
| 4-5 | 배포 상태 | §2(현황) | startup.scr | — | `lora_tdm_app` baseline 등록됨(2026-06-16), `lora_fc_downlink_app`은 저장소에서 삭제됨(commit `7c080f1`, 2026-06-30). `telemetry_app`/`img_app` 미배포·코드 보존 유지 | ✅ **해결** | §2 현황을 위 내용으로 갱신 (commit `faf30ef`) |

> **2026-06-16 배포 전환 후속 작업 (코드/문서 외부, 운영 단계):**
> - Pi에서 `bridge/lora_uplink_bridge.py`, `bridge/lora_telemetry_bridge.py` 프로세스 종료 필요 (둘 다 `lora_tdm_app`과 같은 LoRa serial을 점유하면 충돌).
> - Pi 크로스컴파일용 cFS 프레임워크의 앱 목록(`targets.cmake` 등, 이 저장소 밖)에 `lora_tdm_app` 추가 필요.
> - `mission_app_runtime_spec.md`는 `lora_fc_downlink_app`을 downlink 역할 구현체로 광범위하게 서술 중(§4,§6,§11,§17~18 등) — 전면 갱신은 별도 작업으로 분리.
| 4-6 | uplink 라우팅 | §18.4.x | dispatch 체인 | 명령 클래스 문서화됨 | `uplink_app`→(0x190C~0x1910)→`cfs_core_app`(viewpoint/config 구독) 실재 | ✅ | 클래스→MID 값 매핑만 보강(§4-2와 연계) |
| 4-7 | **dead-end 라우팅** | §18.4.x (명령 클래스 분류) | `uplink_app_utils.c` `ForwardRecoveryCommand`/`ForwardModeCommand`/`ForwardDiagnosticCommand` | spec: RECOVERY/MODE/DIAGNOSTIC 명령 클래스가 대상 앱으로 라우팅됨을 전제 | `RECOVERY_CMD_MID`(0x190C)/`MODE_CMD_MID`(0x190F)/`DIAGNOSTIC_CMD_MID`(0x1910) — `grep -rl` 결과 코드베이스 전체에서 **publish하는 uplink_app 자신 외 구독자가 0개**. `cfs_core_app`/`mavlink_bridge_app`/`lora_fc_downlink_app` 어디도 구독 안 함 | ✅ **해결** | `cfs_core_app`에 RECOVERY(0x190C)/MODE(0x190F) 구독·핸들러 추가, `lora_tdm_app`에 DIAGNOSTIC(0x1910) 구독·핸들러 추가 (commit `e9957e9`). `cfs_core_app_dispatch.c:53,59`, `lora_tdm_app.c:463`/`lora_tdm_app_dispatch.c:93`에서 구독 확인 (2026-07-14) |
| 4-8 | **MID 충돌(신규)** | — | uplink_app msgid_values:18 ↔ mavlink_bridge_app interface_cfg_values:11 | — | `UPLINK_APP_LORA_RAW_MID_VALUE 0x1909`(uplink_app 구독, 발행자 없음) = `FC_SYS_TIME_MID_VALUE 0x1909`(mavlink_bridge_app, commit `38c2f22` 2026-07-13 신규 발행 시작) — 4-1과 같은 급의 MID 충돌 | ✅ **해결** | git log 확인 결과 `UPLINK_APP_LORA_RAW_MID`/`ParseLoRaFrame`은 2026-06-11 `lora_tdm_app` 도입(TDM 슬롯 타이밍상 CRC 검증을 동기화해야 해서 raw-forward 대신 자체 파싱+직접 SB 전달로 재설계, `ProcessUpFrame`→`0x18D0`) 이후 발행자 없는 죽은 코드였음. 구독·dispatch 분기·`ParseLoRaFrame()`·관련 config/coveragetest 전체 삭제로 충돌 원천 해소 (2026-07-14). UT 회귀 없음(`uplink_app` 9/9, `_cmds` 91/91, `_dispatch` 29/29, `_utils` 88/88) |

> 종합(2026-07-14 갱신): 4-1(0x190F MID 충돌)·4-2(라우팅 MID 인벤토리)·4-3(게시율)·4-4/4-5(앱 집합/배포 현황)·4-7(RECOVERY/MODE/DIAGNOSTIC dead-end)·4-8(0x1909 MID 충돌, 신규 발견·해결) **전부 해결**. §4 앱 집합·라우팅 체인 전 클래스(CONFIG/VIEWPOINT/ROUTE_UPDATE/RECOVERY/MODE/DIAGNOSTIC)가 코드와 정합.

---

## 재감사 (2026-07-20) — 패스 1: `mavlink_bridge_app`

2026-07-14 이후 변경분(shared_msgs 병합, DL2 SysTime, NaN/Inf 검증, CONFIG checksum 등) 중심 재점검. 모드: 보고 우선.

**코드 권위 요약 (변경분)**
- MID: 기존 + `FC_SYS_TIME_MID 0x1909` (interface_cfg_values.h:11). 구독 4종(CMD/SEND_HK/ROUTE_UPDATE/CONFIG_CMD) 변동 없음 (`mavlink_bridge_app.c:132-150`)
- EID: **13 `NONFINITE_VALUE_ERR_EID` 신설** (`eventids.h:16`), HK에 `NonFiniteValueCount` 필드 추가 (`utils.c:291-292,1931`)
- internal_cfg 신규/현행: `RECONNECT 1000`/`STALE_TIMEOUT 1000`/`HEARTBEAT_INTERVAL 1000`/`STREAM_REQUEST_RETRY 2000`/`TARGET_DISCOVERY_TIMEOUT 10000`/`STREAM_REACQUIRE_TIMEOUT 5000`/`SYS_TIME_INTERVAL_US 1000000`/`CONFIG_VERSION 1`/`CONFIG_SCOPE 2`/`PARAM_INTERVAL 10000~10000000us`/`PARAM_MS 100~60000`
- platform_cfg: `SERIAL_BAUDRATE 57600`, `PIPE_DEPTH 32`
- FC 상태 구조체 4종: `shared_msgs/fc_state_msg.h`의 `FC_*_TLM_t`를 msgstruct.h에서 typedef (Task #4 병합). `SysTimeTlm_t`는 msgstruct.h:44에 직접 정의

| # | 카테고리 | spec 위치 | 코드 위치 | spec | 코드 | 판정 | 권고 |
|---|---|---|---|---|---|---|---|
| R1-1 | 이벤트 | (EID 표/§ 어디에도 없음) | eventids.h:16, utils.c:291 | NONFINITE/NaN 검증 언급 0건 | `NONFINITE_VALUE_ERR_EID 13` + `NonFiniteValueCount` HK 필드 실사용 | ❌ | spec에 NaN/Inf 게이트 동작·EID 13·HK 필드 추가 |
| R1-2 | 설정·한도 | §16.5 표 | platform_cfg.h:8 | "UART **115200**, 1 Hz 폴링" | `SERIAL_BAUDRATE **57600**` (§13.4 예시·코드 일치) | ❌ | §16.5 표 115200→57600 정정 |
| R1-3 | 페이로드 구조체 | §16(507,515행) | msgstruct.h:8,29 / shared_msgs/fc_state_msg.h | "msgstruct.h의 `EkfLocalTlm_t`…" 서술 | 실정의는 `shared_msgs/fc_state_msg.h`(typedef 경유라 호환) | ⚠️ | shared_msgs 병합 사실 한 줄 언급 권장 (기능적 불일치 아님) |
| R1-4 | 설정·한도 | (미기재) | internal_cfg_values.h:4-9,19-26 | 재연결/stale/하트비트/디스커버리/재획득 타이밍, CONFIG version/scope/param 한도 미문서화 | 좌기 상수 11종 실사용 | ⚠️ | spec에 타이밍·CONFIG 한도 표 추가 (값 자체 불일치는 없음) |
| R1-5 | 교차 참조 | spec 540행 | mission spec §5.1.1 | "`FC_SYS_TIME_MID` 행 추가 필요 (미반영)" | mission spec §5.1.1 표에 여전히 0x1909 행 없음 (부록 인벤토리에만 있음) | 🕒 | 패스 4에서 처리: §5.1.1에 0x1909 행 추가 후 본 spec 540행 문구 해소 |
| R1-6 | 파서 결함 | §17 | utils.c:1631-1646 | "STX 이스케이프 결함, 코드 미적용" | 코드 확인: 여전히 무조건 STX 선점 — spec 기술과 일치 | ✅ | — (P1 미적용 상태 정확히 문서화됨) |
| R1-7 | MID/CC/구독 | §4.1,§10 | config/*, app.c | MID·CC·구독 목록 | 동일 | ✅ | — |

> 종합: 핵심 계약(MID/CC/구독/타이밍 상수값)은 정합 유지. 실 불일치는 ❌R1-1(EID 13 미문서화), ❌R1-2(baud 115200 오기) 2건. R1-5는 패스 4 이관.

## 재감사 (2026-07-20) — 패스 2: `cfs_core_app`

**코드 권위 요약 (변경분)**
- EID: **13 `RECOVERY_CMD`/14 `MODE_CMD`/15 `UPLINK_RESTART`/16 `LORA_RESTART` 신설** (eventids.h:16-19)
- 구독 13종: CMD/SEND_HK/BRIDGE_HK/**UPLINK_HK(0x08D0)/LORA_HK(0x08E0)**/FC_* 4종/ROUTE_UPDATE/CONFIG/VIEWPOINT/**RECOVERY/MODE** (`cfs_core_app.c:95-173`)
- internal_cfg 신규: `UPLINK/LORA_TIMEOUT 5000`, `UPLINK/LORA_RESTART_INTERVAL 5000`/`MAX_RESTARTS 3`, `CONFIG_VERSION 1`/`SCOPE 1`/`PARAM 100~60000ms`
- **uplink_app·lora_tdm_app 자동 재시작 구현됨** (`cfs_core_app_utils.c:353-404`, bridge와 동일 패턴, EID 15/16)
- RECOVERY 명령: `RecoveryAction` 4종(RESET_COUNTER/RESTART_BRIDGE/PARSER_RESET/SERIAL_RECONNECT) switch 분기 + unknown 거부 (utils.c:740-784)
- MODE 명령: `ModeAction` ENTER/EXIT + NORMAL↔RECOVERY 전이 허용 검증, 불허 시 REJECTED ERROR 이벤트 (utils.c:786-829)
- 미러 구조체: `shared_msgs/`(bridge_hk/system_health/route/config/fc_state) 단일 진실, 앱측은 typedef. `BRIDGE_HK_TLM_t`에 `NonFiniteValueCount` 포함

| # | 카테고리 | spec 위치 | 코드 위치 | spec | 코드 | 판정 | 권고 |
|---|---|---|---|---|---|---|---|
| R2-1 | 복구/재시작 | §1(61행), §14.x(507,525행) | utils.c:353-404, internal_cfg:19-26 | uplink/lora 타임아웃 "**자동 재시작 없음 (보고 전용)**" | 둘 다 자동 재시작 구현(5s 간격, 최대 3회, EID 15/16) | ❌ | §1/§14.x를 bridge와 동일 패턴 재시작으로 정정, EID 15/16 문서화 |
| R2-2 | 명령 처리 | §17(626-627행) | utils.c:740-829 | RECOVERY "action/target 구분 검증 **미구현**", MODE "전이 검증 **미구현**" | RecoveryAction 4종 분기+거부, MODE NORMAL↔RECOVERY 전이 검증+REJECTED 구현됨 | ❌ | §17 두 항목을 구현 상태로 정정 (RESET_COUNTER 외 action은 로그 전용임도 명시) |
| R2-3 | 페이로드 구조체 | §7.2(145행) | shared_msgs/bridge_hk_msg.h, utils.h:10 | BridgeHkMirror 소비 필드 3개 표, shared_msgs 미언급 | 실정의는 `BRIDGE_HK_TLM_t`(shared_msgs, `NonFiniteValueCount` 등 16필드), typedef 경유 | ⚠️ | §7.2에 shared_msgs 단일 진실 + NonFiniteValueCount 언급 추가 |
| R2-4 | MID/구독 | §4(96-107행) | msgid_values.h, app.c | 구독 13종·UPLINK/LORA_HK·RECOVERY/MODE 전부 표에 존재 | 동일 | ✅ | — |
| R2-5 | 설정·한도/FAULT | §12.x,§21.5(806-807행) | internal_cfg, utils.c | 타임아웃 일습, FAULT_UPLINK(6)/LORA(7) | 동일 | ✅ | — |

> 종합: MID/구독/타임아웃/FAULT 코드는 정합. 실 불일치 ❌2건 — R2-1(uplink/lora 자동 재시작을 spec이 "없음"으로 부정), R2-2(RECOVERY/MODE 검증 로직 "미구현" 스테일). 둘 다 코드가 spec보다 앞선 패턴의 재발.

## 재감사 (2026-07-20) — 패스 3: `lora_tdm_app`

**코드 권위 요약 (변경분)**
- **Stage 3 타이밍**: `CYCLE_PERIOD_MS 200`(5Hz)/`RX_WINDOW_MS 100`/`LINK_LOSS_THRESHOLD 15` (mission_cfg.h:7-9, commit `454f8b4`)
- 구독 10종: CMD/SEND_HK + SubscribeEx(SYSTEM_HEALTH MsgLim20, FC_* 4종 MsgLim10, **FC_SYS_TIME 0x1909**) + DIAGNOSTIC(0x1910) + **UPLINK_STATUS(0x190A)** (`lora_tdm_app.c:411-479`), 파이프 깊이 50 유지
- **프로토콜 v2 구현됨**: `BuildDl2Frame`(SysTime 확장 블록 포함)/`ParseAck2Frame`/`ParseUp2Frame`+`ForwardUp2ToUplinkApp` (utils.c:108-300,552+), CONFIG `PARAM_DOWNLINK_PROTOCOL`(0=v1/1=v2) 런타임 전환
- **SEQ_FAIL 구현됨**: ACK `SeqEcho != LastSentSeq` 비교 → `SEQ_FAIL_EID 12` 발생 (utils.c:520-526, 타이밍 버그 수정 commit `48c8d12`)
- **UFB_SEQ_FAIL 구현됨**: `UPLINK_STATUS_MID` 구독, `LastCommandResult==3(REJECT_SEQUENCE)`이면 `PendingUplinkFeedback=UFB_SEQ_FAIL` (dispatch.c:105-108)

| # | 카테고리 | spec 위치 | 코드 위치 | spec | 코드 | 판정 | 권고 |
|---|---|---|---|---|---|---|---|
| R3-1 | TDM 상수 | §1(49행),§4노트(77행),§7(143-146,186행),§14(275행),설정표(353-355행) | mission_cfg.h:7-9 | CYCLE **1000**/RX **300**/THRESHOLD **3**, "200ms 설계는 미구현 초안" | CYCLE **200**/RX **100**/THRESHOLD **15** 배포 중 | ❌ | 전 구간 Stage 3 값으로 정정 (구값은 이력 주석으로) |
| R3-2 | 이벤트/UFB | EID표(332행), 설정표(359행), §15(366행) | utils.c:520-526, dispatch.c:105-108 | `SEQ_FAIL_EID 12` "⚪ 미구현", `UFB_SEQ_FAIL` "미구현" | 둘 다 구현·실동작(Pi 로그에서 SEQ_FAIL 관측됨) | ❌ | EID표 ✅ 전환, §15 미구현 목록에서 제거 |
| R3-3 | Pub/Sub | §4 표(63-70행) | lora_tdm_app.c:465,479 | `FC_SYS_TIME(0x1909)`·`UPLINK_STATUS(0x190A)` 행 누락 (본문 240행엔 언급) | 둘 다 구독 중 (0x1909는 SubscribeEx) | ❌ | §4 표에 2행 추가 |
| R3-4 | 프로토콜 문서 | lora_protocol_v2_spec.md 헤더(3행) | utils.c DL2/UP2/ACK2 전부 | "설계 확정 전 초안 — **코드 미구현**" | DL2/ACK2/UP2 구현 + CONFIG 런타임 전환 배포 | ❌ | 헤더를 "구현됨(2026-07-13~)"으로 갱신, 미구현 잔여 있으면 명시 |
| R3-5 | MID/CC/깊이 | §4,§5 | topicid_values.h, app.c:405 | MID 값·CC·깊이 50·MsgLim 예외 5종 | 동일 | ✅ | — |

> 종합: MID/CC/MsgLim 정합. 그러나 **Stage 3 전환(5Hz)과 프로토콜 v2 구현이 behavior spec에 전혀 반영 안 됨** — 패스 1·2보다 스테일 폭이 큼 (❌4건).

## 재감사 (2026-07-20) — 패스 4: 교차 통합 ↔ `mission_app_runtime_spec.md` (+ uplink_app)

**확인된 정합 (✅)**
- CONFIG checksum 검증: spec §13.4 "3개 앱 모두 검증" ↔ 코드 3앱 전부 `*_ConfigChecksum` 구현 확인 (uplink_utils.c:353, cfs_core_utils.c:504, mavlink_utils.c:1758)
- FC MISSION_ACK 피드백: §18.7(1648-1655행) `FcMissionResult`/`FcMissionUploadState`/`FcMissionUploadSuccessCount` ↔ `uplink_app_dispatch.c:40` BRIDGE_HK 캐시 — 일치 (2026-07-15 반영분)
- MID 인벤토리·0x190F/0x1909 충돌 해소 상태 유지, startup.scr 앱 집합 변동 없음

| # | 카테고리 | spec 위치 | 코드 위치 | spec | 코드 | 판정 | 권고 |
|---|---|---|---|---|---|---|---|
| R4-1 (=R1-5) | MID 인벤토리 | §5.1.1 표 | mavlink interface_cfg:11 | `FC_SYS_TIME_MID 0x1909` 행 없음 (부록/§17.1엔 있음) | 발행 중(1Hz), lora_tdm 구독 | ❌ | §5.1.1에 0x1909 행 추가 → mavlink spec 540행 주석도 해소 |
| R4-2 | Pub/Sub 표 | §5.1.1(169-170행) | uplink_app.c:98,104 | `0x08A0` 구독자 "cfs_core_app"만, `SYSTEM_HEALTH` 구독자에 uplink_app 없음 | **uplink_app도 BRIDGE_HK·SYSTEM_HEALTH 구독** (MISSION_ACK 캐시·헬스 게이트) | ❌ | 두 행 구독자 목록에 uplink_app 추가 |
| R4-3 | 앱 집합 표 | §4(103행) | lora_tdm_app.c:465,479 | lora_tdm 구독 목록에 `FC_SYS_TIME(0x1909)`/`UPLINK_STATUS(0x190A)` 누락 | 둘 다 구독 | ❌ | §4 표 갱신 (R3-3과 동일 원인) |
| R4-4 | 감시 입력 | §5.1.1 | cfs_core_app.c:113,119 | `UPLINK_APP_HK(0x08D0)`/`LORA_TDM_APP_HK(0x08E0)`의 cfs_core 구독(생존 감시) 행/언급 없음 | 구독·타임아웃 감시·자동 재시작 구현 | ❌ | §5.1.1에 HK 2행(또는 기존 행 구독자 갱신) + §11 복구 표에 uplink/lora 재시작 반영 (R2-1 연동) |
| R4-5 | 폐기 앱 서술 | §5.1.1 주(174행), §6.6(285행) | — | `lora_fc_downlink_app`이 "…HK만 publish**한다**" 현재형 서술 | 앱은 저장소에서 삭제됨(`7c080f1`) | ⚠️ | 과거형/"(삭제됨)" 표기로 정정 |

> 종합: 신규 기능 자체(checksum, MISSION_ACK 피드백)는 mission spec에 이미 반영돼 있으나, **구독 관계 표들이 코드 변화(uplink의 BRIDGE_HK/SYSTEM_HEALTH 구독, cfs_core의 HK 생존감시, lora_tdm의 0x1909/0x190A)를 못 따라감** — ❌4건 전부 표 갱신 성격.

### 재감사 종합 (2026-07-20)

- 패스1 ❌2: EID 13 미문서화, baud 115200 오기
- 패스2 ❌2: uplink/lora 자동 재시작 "없음" 부정, RECOVERY/MODE 검증 "미구현" 스테일
- 패스3 ❌4: Stage 3 타이밍(200/100/15) 미반영, SEQ_FAIL/UFB_SEQ_FAIL "미구현" 스테일, 구독 표 0x1909/0x190A 누락, protocol v2 "코드 미구현" 헤더 스테일
- 패스4 ❌4: §5.1.1 0x1909 행, 구독자 표 3건, (⚠️ 폐기 앱 현재형 서술)
- 공통 패턴: **값 계약(MID/CC/상수)은 전부 정합, 스테일은 전부 "코드가 spec보다 앞섬"** — 기능 추가 시 behavior spec 동반 갱신 누락이 원인. spec 정정은 승인 후 일괄 진행.

## 재감사 (2026-07-20) — 패스 5: 통합 (cross-app 와이어 레이아웃 + 배포 산출물)

shared_msgs 병합(2026-07-15)에서 **제외된** cross-app 메시지 전수 레이아웃 대조. 앱별 audit이 못 잡는 "양쪽 정의가 어긋나는" 축 점검.

| # | 계약 | 발행측 정의 | 구독측 정의 | 대조 결과 | 판정 |
|---|---|---|---|---|---|
| R5-1 | RECOVERY_CMD 0x190C | `UPLINK_APP_RecoveryCmdTlm_t` | `CFS_CORE_APP_RecoveryCmdTlm_t` | `shared_msgs/recovery_cmd_msg.h`(`RECOVERY_CMD_TLM_t`) typedef로 병합 완료 | ✅ |
| R5-2 | MODE_CMD 0x190F | `UPLINK_APP_ModeCmdTlm_t` | `CFS_CORE_APP_ModeCmdTlm_t` | `shared_msgs/mode_cmd_msg.h`(`MODE_CMD_TLM_t`) typedef로 병합 완료 | ✅ |
| R5-3 | VIEWPOINT_CMD 0x190D | `UPLINK_APP_ViewpointCmdTlm_t` | `CFS_CORE_APP_ViewpointCmdTlm_t` | `shared_msgs/viewpoint_cmd_msg.h`(`VIEWPOINT_CMD_TLM_t`) typedef로 병합 완료 | ✅ |
| R5-4 | DIAGNOSTIC_CMD 0x1910 | `UPLINK_APP_DiagnosticCmdTlm_t` | `LORA_TDM_APP_DiagnosticCmdTlm_t` | `shared_msgs/diagnostic_cmd_msg.h`(`DIAGNOSTIC_CMD_TLM_t`) typedef로 병합 완료 | ✅ |
| R5-5 | UPLINK_APP_CMD 0x18D0 (UP forward) | `LORA_TDM_APP_UplinkFwdCmd_t` | `UPLINK_APP_ProcessUplinkCmd_t` | `shared_msgs/uplink_fwd_cmd_msg.h`(`UPLINK_FWD_CMD_TLM_t`) typedef로 병합 완료 | ✅ |
| R5-6 | UPLINK_STATUS 0x190A | `UPLINK_APP_StatusTlm_t` | lora_tdm이 `uplink_app_msg.h` **직접 include** (dispatch.c:7) | 단일 정의 — 미러 없음 | ✅ |
| R5-7 | SYSTEM_HEALTH 0x1904 → uplink | `SYSTEM_HEALTH_TLM_t` (shared_msgs) | `UPLINK_APP_SysHealthMirror_t` | prefix 재정의 제거, `SYSTEM_HEALTH_TLM_t` 직접 typedef로 교체 완료 | ✅ |
| R5-8 | SCH_LAB SEND_HK 스케줄 | `mission_defs/tables/cpu1_sch_lab_table.c:44-47` | 각 앱 SEND_HK MID | 0x18A1/0x18C1/0x18D1/0x18E1 — 4앱 전부 일치 | ✅ |
| R5-9 | CONFIG scope 분담 | GS→0x190E 공용 발행 | mavlink SCOPE=2 / cfs_core=1 / lora_tdm=3 | 중복 없음, 앱별 자기 scope만 처리 | ✅ |

> 종합 (2026-07-20 후속 조치 완료): 발견 당시 "현재 어긋난 레이아웃 0건이나 중복/prefix 미러 6건이 BridgeHkMirror 사고와 같은 잠재 리스크"였던 것을 코드 작업으로 전부 해소. `shared_msgs/`에 헤더 5종(`recovery_cmd_msg.h`/`mode_cmd_msg.h`/`viewpoint_cmd_msg.h`/`diagnostic_cmd_msg.h`/`uplink_fwd_cmd_msg.h`) 신설, 관련 4개 앱(`uplink_app`/`cfs_core_app`/`lora_tdm_app`)의 로컬 중복 정의를 typedef로 교체. uplink의 `SysHealthMirror_t` prefix 재정의도 `SYSTEM_HEALTH_TLM_t` 직접 typedef로 교체. 순수 리팩터(레이아웃 불변) — cFS UT 16/16 PASS 회귀 없음 확인, spec 본문 수정 불필요(동작 변경 없음).


| 값 | 심볼 | 소유 앱 | 배포 |
|---|---|---|---|
| `0x08A0` | `MAVLINK_BRIDGE_APP_HK_TLM` | mavlink_bridge_app | ✅ |
| `0x08C0` | `CFS_CORE_APP_HK_TLM` | cfs_core_app | ✅ |
| `0x08D0` | `UPLINK_APP_HK_TLM` | uplink_app | ✅ |
| `0x08E0` | `LORA_TDM_APP_HK_TLM` | lora_tdm_app | ✅ (2026-06-16 배포) |
| `0x18A0/A1` | `MAVLINK_BRIDGE_APP_CMD/SEND_HK` | mavlink_bridge_app | ✅ |
| `0x18B0/B1` | `LORA_FC_DOWNLINK_APP_CMD/SEND_HK` (topic-id) | lora_fc_downlink_app | 미배포 (2026-06-16 startup 제거) |
| `0x18C0/C1` | `CFS_CORE_APP_CMD/SEND_HK` | cfs_core_app | ✅ |
| `0x18D0/D1` | `UPLINK_APP_CMD/SEND_HK` | uplink_app | ✅ |
| `0x18E0/E1` | `LORA_TDM_APP_CMD/SEND_HK` | lora_tdm_app | ✅ (2026-06-16 배포) |
| `0x1904` | `SYSTEM_HEALTH_MID` | cfs_core_app | ✅ |
| `0x1905` | `FC_EKF_LOCAL_STATE_MID` | mavlink_bridge_app | ✅ |
| `0x1906` | `FC_ATTITUDE_STATE_MID` | mavlink_bridge_app | ✅ |
| `0x1907` | `FC_GPS_RAW_STATE_MID` | mavlink_bridge_app | ✅ |
| `0x1908` | `FC_EKF_STATUS_MID` | mavlink_bridge_app | ✅ |
| `0x1909` | `FC_SYS_TIME_MID` | mavlink_bridge_app | ✅ (2026-07-13 발행 시작. 구 `UPLINK_APP_LORA_RAW_MID`가 동일 번호로 죽은 채 구독 중이던 것과 충돌 — 2026-07-14 죽은 구독 삭제로 해소, §4-8) |
| `0x190A` | `UPLINK_STATUS_MID` | uplink_app | ✅ |
| `0x190B` | `ROUTE_UPDATE_MID` | uplink_app→cfs_core/mavlink_bridge | ✅ |
| `0x190C` | `RECOVERY_CMD_MID` | uplink_app → **cfs_core_app** (2026-06-17 구독 추가, §4-7 해소) | ✅ |
| `0x190D` | `VIEWPOINT_CMD_MID` | uplink_app→cfs_core_app | ✅ |
| `0x190E` | `CONFIG_CMD_MID` | uplink_app→cfs_core/mavlink_bridge | ✅ |
| `0x190F` | `MODE_CMD_MID` | uplink_app → **cfs_core_app** (2026-06-17 구독 추가, §4-7 해소) | ✅ |
| `0x1910` | `DIAGNOSTIC_CMD_MID` | uplink_app → **lora_tdm_app** (2026-06-17 구독 추가, §4-7 해소) | ✅ |
| `0x1911` | `LORA_TDM_APP_LINK_STATUS_MID` (구 `0x190F`, 충돌 해소 재할당) | lora_tdm_app | ✅ (2026-06-16 배포) |

> `0x190C`~`0x1910` 라우팅 명령 MID와 `0x1909`는 `mission_app_runtime_spec.md` MID 표에 미수록(§4-2). `0x190F` 이중 할당은 §4-1.

> **MID 충돌 전수 재감사 (2026-07-14)**: 배포 4개 앱 전체 `*_MID_VALUE` define을
> 값별 그룹핑해 재확인. **충돌 0건** — 모든 MID가 정확히 1개 논리 메시지에 대응하고,
> 중복 심볼(0x1904~0x1908, 0x18D0, 0x08E0 등)은 전부 "발행앱 1 + 구독앱 N"의 정상
> 공유. 과거 충돌 2건(`0x190F` §4-1, `0x1909` §4-8) 모두 해소 유지, 폐기된
> `lora_fc_downlink_app`의 `0x08B0`/`0x18B0` 잔재도 배포 앱에 없음. 위 부록 A
> 인벤토리가 코드와 완전 정합.

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
- ✅ **해결(2026-06-17) — 4-7 RECOVERY/MODE/DIAGNOSTIC 명령 dead-end** — `uplink_app`이 검증·라우팅까지는 하지만, `RECOVERY_CMD_MID`/`MODE_CMD_MID`/`DIAGNOSTIC_CMD_MID`를 구독하는 앱이 코드베이스 전체에 없었음. `cfs_core_app`에 RECOVERY/MODE 구독·핸들러, `lora_tdm_app`에 DIAGNOSTIC 구독·핸들러 추가로 해소 (commit `e9957e9`, 상세는 위 "종합 요약" 참조).
- ✅ **해결(2026-06-17) — `mission_app_runtime_spec.md` §11.1 vs 코드** — spec은 `cfs_core_app`을 "모든 앱의 복구 권한"으로 설계했으나 코드는 `mavlink_bridge_app`만 재시작(하드코딩). §11.1 표를 코드 기준(구현 상태 컬럼 포함)으로 재작성 (commit `faf30ef`).
- ✅ **해결(3차 검증까지 완료) — `lora_tdm_app` SB Msg Limit Err (실 Pi 런타임에서 발견)** — `LORA_TDM_PIPE` 구독이 전부 `CFE_SB_Subscribe()`(기본 limit=4)라서, 1차로 FC_* 4개 MID(5Hz)에서 발생 확인 → `CFE_SB_SubscribeEx(MsgLim=10)`으로 수정 → 재검증 결과 그 4개는 해결됐으나 `SYSTEM_HEALTH_MID`에서 동일 에러 16회 추가 발견 → `SYSTEM_HEALTH_MID`도 `MsgLim=20`으로 수정(`cfs_core_app`이 1Hz가 아니라 FC 입력마다 강제 발행함을 코드로 확인, `cfs_core_app_utils.c:193`) → 3차 재검증 결과 에러가 줄었으나 부팅 시점에 `0x1905`/`0x1906`까지 재발. 전체 로그 분석 결과 **에러 16건 전부 부팅 후 130ms 안에만 발생, 이후 60초+ 0건** — 지속 문제 아니라 1회성 부팅 버스트로 확인. 근본 원인: `mavlink_bridge_app`이 `/dev/serial0`를 열 때 cFS가 꺼져있던 동안 FC가 보낸 누적 데이터를 한 번에 드레인(MsgLim을 올려도 다운타임이 길면 버스트가 커져 근본 해결 안 됨). **최종 조치**: `mavlink_bridge_app_utils.c`의 `OpenSerial()`에 `tcflush(Fd, TCIFLUSH)` 추가해 포트 open 시 묵은 입력 버퍼를 비움(버스트 자체를 제거). 상세는 `lora_tdm_app_behavior_spec.md` §5.1.

위 3건 전부 코드 수정 완료 (2026-07-14, 표·요약 정합 확인).

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
