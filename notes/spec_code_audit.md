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
| 1-5 | 설정·한도 | spec §8,§13.0 | config 헤더에 없음 | timeout 2000/retry 3/CLEAR_DELAY 300, sysid 255/compid 190 | (fsw 상수로 추정, 미확인) | 🕒 | 해당 상수 정의 위치 확인 후 값 일치 검증 (config 헤더 미수록) |
| 1-6 | 게시율 | (mission spec §5.1.1) | internal_cfg_values.h | ATTITUDE ~20Hz, GPS ~5Hz, EKF ~10Hz | 요청 interval 5Hz/2Hz/2Hz | ❌ | **pass 4에서 처리**: 5.1.1 게시율을 코드 stream interval과 정합 |
| 1-7 | 책임 분리 | spec §2.1 | (src 미확인) | `LoRaFd`/`LoRaTxCount`/`ServiceLoRa` 제거됨 | 미검증 | 🕒 | src에서 LoRa 잔재 부재 확인 권장 |
| 1-8 | spec 내부 | spec §13.1.1/§332/§341 vs §15 | — | INT 경로 global frame "미구현" ↔ §15 "구현 완료" | — | ⚠️ | 문구 모호: legacy(구현)/INT(미구현) 경로 구분 명확화 |

> 종합: MID/CC/EID 핵심 계약은 코드와 **일치**. 실 불일치는 ❌1-4(CONFIG_CMD_MID 미문서화), ❌1-6(게시율, pass 4), ⚠️1-8(spec 내부 문구). 1-5/1-7은 확인 권장.

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
| 3-5 | spec 내부 | §7.1 vs §15 | — | §7.1 "SEND_HK→HK+LinkStatus" ↔ §15 "SEND_HK→HK만" | — | ⚠️ | dispatch.c 확인 후 §7.1/§15 통일 |
| 3-6 | 이벤트 | (EID 표 없음) | eventids.h | — | `SEQ_FAIL_EID 12` 정의·로직 미구현 | ⚠️ | §15와 일관(경미). EID 표 추가 권장 |

> 종합: MID·TDM 상수 **완전 일치**. 실 불일치는 ❌3-3(PacketType 기본값), ❌3-4(파이프 깊이 10 vs 50). ⚠️3-5는 spec 내부 모순.

## 4. 교차 통합 패스 ↔ `mission_app_runtime_spec.md`

**배포 baseline (`mission_defs/cpu1_cfe_es_startup.scr`)**: `mav_bridge_app`(prio50), `cfs_core_app`(55), `uplink_app`(57), `lora_fc_dl_app`(58) + lab apps(ci/to/sch).
→ **`lora_tdm_app`·`telemetry_app`·`img_app`은 startup 미등록(미배포)**.

**uplink_app 명령 라우팅 MID (uplink_app/config)**: `UPLINK_STATUS 0x190A`, `ROUTE_UPDATE 0x190B`, `RECOVERY_CMD 0x190C`, `VIEWPOINT_CMD 0x190D`, `CONFIG_CMD 0x190E`, `MODE_CMD 0x190F`, `DIAGNOSTIC_CMD 0x1910`, `UPLINK_APP_LORA_RAW 0x1909`, HK `0x08D0`.

| # | 카테고리 | spec 위치 | 코드 위치 | spec | 코드 | 판정 | 권고 |
|---|---|---|---|---|---|---|---|
| 4-1 | **MID 충돌** | — | uplink_app:12 ↔ lora_tdm topicid:21 | — | `MODE_CMD_MID 0x190F`(uplink) = `LORA_TDM_APP_LINK_STATUS_MID 0x190F`(tdm) | ✅ **해결** | lora_tdm `LINK_STATUS_MID_VALUE` `0x190F`→**`0x1911`** 재할당(미배포 측 이동). uplink `MODE_CMD 0x190F` 유지. 문서(behavior spec/README) 동기화 완료 |
| 4-2 | MID 인벤토리 | §5.1.1,§17.1 | uplink_app msgid | (누락) | `RECOVERY_CMD 0x190C`, `VIEWPOINT_CMD 0x190D`, `CONFIG_CMD 0x190E`, `MODE_CMD 0x190F`, `DIAGNOSTIC_CMD 0x1910`, `UPLINK_APP_LORA_RAW 0x1909` | ✅ **해결** | §17.1에 FC 상태 MID·라우팅 명령 MID(0x190C~0x1910)·0x1909·0x1911 추가 (2026-06-16) |
| 4-3 (=1-6) | 게시율 | §5.1.1 | mavlink internal_cfg | ATTITUDE `~20Hz`, EKF `~10Hz`, GPS `~5Hz` | stream req ATTITUDE 5Hz(200ms), EKF 2Hz(500ms), GPS 2Hz(500ms) | ✅ **해결** | §5.1.1 게시율을 코드 stream 요청 간격으로 정정 + "FC 송신율 의존" 명시 (2026-06-16) |
| 4-4 | 앱 집합 | §4 | startup.scr | downlink 역할 = `lora_fc_downlink_app` | startup에 `lora_fc_dl_app` 등록 | ✅ | 일치 (이전 pass에서 정정 완료) |
| 4-5 | 배포 상태 | §2(현황) | startup.scr | — | `lora_tdm_app` 코드 존재·미배포, `telemetry_app`/`img_app` 미배포 | 📝 | §2 범위/현황에 "lora_tdm_app 코드 완료·미배포(향후 lora_fc_downlink 대체)" 명시 |
| 4-6 | uplink 라우팅 | §18.4.x | dispatch 체인 | 명령 클래스 문서화됨 | `uplink_app`→(0x190C~0x1910)→`cfs_core_app`(viewpoint/config 구독) 실재 | ✅ | 클래스→MID 값 매핑만 보강(§4-2와 연계) |

> 종합: 핵심은 ❌4-1(**0x190F MID 충돌** — 최우선), ❌4-2(라우팅 MID 인벤토리 누락), ❌4-3(게시율). §4 앱 집합·라우팅 체인은 코드와 정합.

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
| `0x190C` | `RECOVERY_CMD_MID` | uplink_app 라우팅 | ✅ |
| `0x190D` | `VIEWPOINT_CMD_MID` | uplink_app→cfs_core_app | ✅ |
| `0x190E` | `CONFIG_CMD_MID` | uplink_app→cfs_core/mavlink_bridge | ✅ |
| `0x190F` | `MODE_CMD_MID` | uplink_app 라우팅 | ✅ |
| `0x1910` | `DIAGNOSTIC_CMD_MID` | uplink_app 라우팅 | ✅ |
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

**spec 내부 staleness (⚠️) — cfs_core 분은 4d66241에서 해소:** 2-6/2-7/2-8/2-9 정정 완료. 남음: 3-5(lora_tdm §7.1 vs §15 SEND_HK 문구), 1-8(mavlink INT frame 미구현 문구).

**확인 권장 (🕒, 미해결):** 1-5(mavlink sysid/timeout 상수 위치 검증), 1-7(mavlink src LoRa 잔재 부재 확인).

**일치 확인 (✅):** 전 앱 MID 수치값, cfs_core timing, lora_tdm TDM 상수, 앱 집합/라우팅 체인.

**잔여 작업**: ⚠️3-5·1-8(문구 정리, 경미), 🕒1-5·1-7(코드 확인). 핵심 ❌는 전부 해결됨.
