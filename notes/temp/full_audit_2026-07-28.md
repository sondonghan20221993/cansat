# 전체 점검 감사 — 2026-07-28

> 저장소 전 범위 점검 결과. 5개 영역(문서 충돌 / mavlink_bridge / lora_tdm /
> cfs_core+uplink / 테스트·빌드)을 병렬 감사하고 통합한 것.
>
> **성격**: 발견 기록이며 처분 결정이 아님. 각 항목은 코드 근거(`파일:줄`)를
> 명시했고, 확신이 낮은 것은 "추정"으로 표시함. BL 번호는 부여하지 않았으며
> BACKLOG.md 편입 여부는 별도 판단.
>
> **미수정**: 이 감사에서 코드/문서를 고치지 않았음.

## 검증 방법

- 문서 충돌은 문서끼리 비교 후 **반드시 코드에서 정답을 확정**함.
- 테스트는 실제 실행함 (`python3 -m pytest tests -q` → `1 failed, 209 passed`).
- 정합이 확인된 항목도 오탐 방지를 위해 §7에 기록함.

---

## 0. 최우선 3건 (착수 순서)

| # | 항목 | 위치 | 비용 |
|---|---|---|---|
| 1 | UP2 flags 하드코딩 0 → v2 전 명령 인가 거부 | `lora_tdm_app_utils.c:662,675` | 2줄 |
| 2 | PX4 DO_SET_MODE 인코딩 + COMMAND_ACK 오프셋 | `mavlink_bridge_app_utils.c:1084,1494` | 소 |
| 3 | cFS 통합테스트 3파일 0개 수집 | `tests/test_*_e2e.py` 외 | 소 |

1은 영향 최대 대비 비용 최소. 2는 둘 다 무증상 실패이며 ACK 오프셋을 먼저
고쳐야 모드 명령 성공 여부를 관측할 수 있음. 3은 이후 모든 수정 검증의 전제.

---

## 1. 문서 충돌 (1순위)

### 근본 원인

다섯 개 결정이 코드에는 반영됐으나 문서 다수에 미반영. High 항목 대부분이
여기서 파생됨.

| 결정 | 내용 | 확정일 |
|---|---|---|
| BL-15 | TDM 100ms 확정 (200ms 원복 안 함) | 2026-07-27 |
| BL-38 | 앱 재시작 한도 폐지 (쿨다운만) | 2026-07-23 |
| BL-56 | waypoint 거리/영역 검증 폐지, ARMED 게이트 제거 | — |
| BL-70 | route waypoint 상한 16 → 37 | — |
| BL-71 | DL2 waypoint 12B → 13B (블록 28B → 30B) | 2026-07-28 |

### H-1. TDM 타이밍 상수 — 문서마다 4가지 값

**코드 정답**: `lora_tdm_app/config/default_lora_tdm_app_mission_cfg.h:9-12`
→ `CYCLE_PERIOD_MS=100`, `RX_WINDOW_MS=50`, `LINK_LOSS_THRESHOLD=50`,
`LINK_TIMEOUT_MS=5000`. 근거: `notes/temp/BACKLOG.md:129`.

| 문서 | 기재값 | 비고 |
|---|---|---|
| `notes/lora_protocol_v2_spec.md:203-207` | 200 / 100 / 15 | |
| `notes/lora_tdm_app_behavior_spec.md:155,166,197` | 200 / 100 | |
| `notes/lora_stage_measurement_runbook.md:132-133` | 200 / 100 / 15 | |
| `lora_tdm_app/README.md:51` | 150 / 70 / 33 | Stage 4a 임시값, 폐기됨 |
| `notes/spec_code_audit.md:70` | 1000 / 300 / 3 | v1 시절 |
| `notes/temp/BACKLOG.md:11` | RX 100ms | **착수 전 필독 경고문** |
| `README.md:54` | 200ms / 5Hz | |

**조치**: v2 spec §7 표를 `100/50/50/5000`으로 갱신하고 유일 진실원본으로
지정. 나머지는 참조로 축약. `lora_tdm_app/README.md:51` 경고문 삭제(BL-15 종결).

### H-2. RX 윈도우 300ms — 코드의 6배

**코드 정답**: 50ms.

- `README.md:23` "TDM 300ms 창"
- `uplink_app/README.md:60,71` "300ms RX 창", "downlink TX 후 300ms만 열린다"
- `notes/mission_app_runtime_spec.md:2231` "TDM RX 300ms 창"

v1 시절 값. **지상국 응답 예산이 이 수치에 걸려 있어 운용 오류로 직결됨.**

### H-3. 앱 자동 재시작 한도 — 한 파일 내 자기모순

**코드 정답**: 한도 없음. `cfs_core_app/fsw/src/cfs_core_app_utils.c:301`
`/* 쿨다운만으로 빈도 제한, MAX_RESTARTS 없음 */`. `*_MAX_RESTARTS` 매크로
자체가 부재.

- `notes/mission_app_runtime_spec.md:452,456-458` → "최대 3회"
- **같은 파일** `:487` → "무한 재시도, 포기 없음: `*_MAX_RESTARTS` 한도 제거"
- `notes/cfs_core_app_behavior_spec.md:60,61,250,430,515,533,577,748` → 8곳 "3회"
- 위 spec **§12 상수표(250행)에 존재하지 않는 매크로**
  `CFS_CORE_APP_BRIDGE_MAX_RESTARTS=3` 등 3개 등재
- `README.md:58,59`, `cfs_core_app/README.md:35`

**위험**: 유령 매크로가 상수표에 있어 신규 작업자가 이를 구현하려 들 수 있음.

### H-4. uplink/lora 자동 재시작 — 세 문서가 서로 다르고 셋 다 틀림

**코드 정답**: 있음. `cfs_core_app_utils.c:341-362`(uplink), `:373-391`(lora)
— `CFE_ES_RestartApp` + 5초 쿨다운, 한도 없음.

- `cfs_core_app/README.md:41` → "자동 재시작 **없음**"
- `README.md:59` → "각 최대 3회"
- `notes/cfs_core_app_behavior_spec.md:515,533` → "최대 3회"

### H-5. route waypoint 상한 16 vs 37 — BL-72 조사와 직결

**코드 정답**: 37. `uplink_app/config/default_uplink_app_mission_cfg.h:4`,
`cfs_core_app/config/default_cfs_core_app_mission_cfg.h:4`.

- `uplink_app/README.md:29` → "1..16"
- `notes/mission_app_runtime_spec.md:176` → "min(N,16)"
- `notes/lora_protocol_v2_spec.md:98` "상한 16개" ↔ **같은 파일 :100** "37개로 확장 확정"
- `notes/temp/BACKLOG.md:202`(BL-57) → "최대 16개" 3회 반복
- `notes/mirror_struct_layout_refactor_complete.md:222` → 16
- `notes/mavlink_bridge_app_behavior_spec.md:323` → "min(N,37)" ✅ 정확

**미해결 버그 BL-72(readback 개수 17→16→15 불안정) 조사 범위와 겹침.**
문서 정정이 디버깅에 실질 기여함.

### H-6. DL2 waypoint 블록 28B(12B/wp) vs 30B(13B/wp)

**코드 정답**: 30B, waypoint당 13B. `lora_tdm_app_utils.c:284-295`가
`CmdType(1)+LatE7(4)+LonE7(4)+Z(4)`를 offset +4, +17에 기록. 지상 디코더
`bridge/lora_downlink_decoder.py:40,142`도 13/30으로 일치 (BL-71).

- `notes/lora_protocol_v2_spec.md:138,144` → 13B / 28→30 ✅
- **같은 파일** `:100` "37개×12바이트", `:156` "waypoint 1개 12바이트…
  페이지당 2개/28바이트 구조는 불변", `:161` "총 28바이트/사이클"
  ← BL-61 문단이 BL-71 갱신 시 방치됨. **한 섹션 안에 12/13, 28/30 공존.**
- **코드 주석도 stale**: `lora_tdm_app/fsw/inc/lora_tdm_app_interface_cfg.h:36-37`
  `/* … waypoint[2]×12 = 28B */` 바로 아래 `#define …WAYPOINT_BLOCK_LEN 30u`

### H-7. UP2 flags "예약(0)" — 실제로는 3개 비트필드 사용 중

**코드 정답**: `uplink_app/fsw/src/uplink_app_cmds.c`
— bit0 = `FORCE_FLAG`(health gate 우회, :230), bits[2:1] = RETX_IDX(:129,478),
bits[7:6] = auth_level(:62,291).

- `notes/lora_protocol_v2_spec.md:181` → "| 6 | flags | u8 | **예약 (0)** |"

**§2 치명 버그와 정확히 같은 필드.** spec이 "예약"이라 적혀 있어 구현이 0으로
채운 것으로 보임 — 문서 오류가 코드 결함을 유발한 사례. BL-68이 DL2 flags 표는
정정했으나 UP2 flags는 누락.

### H-8. UP2 command_class 6종 vs 8종

**코드 정답**: 8종. `uplink_app/config/default_uplink_app_msgdefs.h:37-45`
— `COUNTER_MGMT=7`, `FLIGHT_MODE=8`(BL-44).

- `notes/lora_protocol_v2_spec.md:180` → "(1=CONFIG … 6=DIAGNOSTIC)"
- **같은 파일** `:172` → "command_class 8종별로 펼친 시각화"
- `notes/uplink_payload_map.html` → 8종 전부 반영 ✅

### H-9. ARMED 시 미션 업로드 차단 — 폐지됐는데 README 잔존 (안전 오판)

**코드 정답**: 차단 없음. `mavlink_bridge_app_utils.c:2510`에서 `IsArmed`를
세팅만 하고 게이트로 사용하지 않음. BL-56이 "`IsArmed` 체크 완전 제거" 명시.

- `mavlink_bridge_app/README.md:49` → "ARMED면 mission upload 차단 +
  `ARMED_WARN_EID(12)` 경고"

2-pass 보정(호버 중 REPLACE)이 이 차단과 충돌해 폐지된 것이므로, README를 믿고
"ARMED면 안 올라간다"고 가정하면 **운용 안전 판단 오류**.

### Medium

| ID | 항목 | 코드 정답 | 문서 오류 |
|---|---|---|---|
| M-1 | route 세그먼트 거리 검증 | 없음 (BL-56 폐지, `uplink_app_utils.c:196`에 고도+finite만) | `uplink_app/README.md:29` "2m..2m 검증" (하한=상한, 표기 자체 무의미) |
| M-2 | RestartApp 앱 이름 | 대문자 `"UPLINK_APP"` 등 (startup.scr과 일치) | `cfs_core_app_behavior_spec.md:515,533,577` 소문자 — **BL-40이 고친 그 버그가 spec에 잔존** |
| M-3 | state 파일 경로 | 상대 `"cf/cfs_core_app_state.bin"` (절대경로는 ENOENT 실측 확인) | `cfs_core_app/README.md:36` `/cf/...` (앞슬래시 = 실패했던 경로) |
| M-4 | stale timeout | 1000ms (`_utils.c:2489`) | `mavlink_bridge_app/README.md:28` "3초" |
| M-5 | UFB 코드 수 | 15종 (0x00~0x0E) | `lora_tdm_app/README.md:28,47` "12종" (behavior spec·v2 spec은 15종 ✅) |
| M-6 | mavlink_bridge fcncode | 0~5 (PARSER_RESET=3, SERIAL_RECONNECT=4, **SET_FLIGHT_MODE=5**) | `mavlink_bridge_app/README.md:6,58` 0~2만 — BL-44 핵심 진입점 누락 |
| M-7 | uplink HK MID | `0x08D0` (`default_uplink_app_msgid_values.h:6`) | `README.md:37` "HK `0x18D1`" (0x18D1은 SEND_HK 명령 MID). 같은 표 타 3개 앱은 정확 |
| M-8 | mavlink_bridge baud | **921600** (2026-07-16 확정) | `notes/integration_steps.md:426,436` "57600" — **재통합 절차서라 따라하면 검증 단계 오판** |
| M-9 | lora_tdm baud | `SERIAL_BAUD_GetConstant` 사용 (BL-19) | `notes/uplink_lora_test_status.md:21` "`B57600` 고정" (값은 우연히 같으나 서술 오류) |
| M-10 | FC 펌웨어 | **PX4** (2026-07-14 사용자 확인) | `fc_telemetry_rate_1_2hz_duplicate_completed.md:17` "ArduPilot" ↔ **같은 파일 :37,:44는 PX4**. `camera/README.md`의 `ardupilot_msp_osd.param` 파일명 |

### Low

- **L-1. `_completed`인데 미체크 항목 잔존** — 20개 파일. 확인 결과 **대부분은
  코드가 이미 구현했고 체크박스만 안 지운 것**(작업 미완이 아님). 예외:
  `runtime_test_session_2026-07-22_completed.md`(13개)는 실기 테스트 잔여로
  실제 미완 가능성 높음. → `_designed` / `_implemented` 접미사 분리 권장.
- **L-2. `lora_downlink_5hz_cap_2026-07-21_completed.md` 결론이 뒤집힘** —
  파일 전체가 "5Hz 고정" 전제이고 :46,:52-53이 후속 질문을 나열하는데, BL-15가
  그 질문들에 전부 답하고 100ms/RX50으로 확정. **`_completed`가 붙어 유효
  문서로 읽힘** → "BL-15로 대체됨" 배너 필요.
- **L-3. UP2 최대 payload 255B는 구현 불가** — `lora_tdm_app.c:220`
  `RxFrameTargetLen = 7+Plen+2`, 버퍼 상한 `LINE_BUF_LEN-1 = 255`.
  Plen>246이면 도달 불가로 조용한 재동기화 루프. spec이 참조하는 심볼
  `LORA_TDM_APP_RX_MAX_FRAME`은 **코드에 존재하지 않음**(grep 0건).
  실질 위험은 낮음(`MAX_PAYLOAD_LENGTH 196` → 최대 205B). spec을 196B 기준으로
  정정하거나 버퍼를 264B로 확대.
- **L-4. `spec_code_audit.md`(2026-06-16) 전체가 v1 스냅샷** — 다른 노트가 이를
  근거로 링크 중. "현행값 아님" 배너 권장.
- **L-5. `BACKLOG.md` 요약 절 ↔ 상세표 충돌** — BL-11/BL-14/BL-15가 요약 절에선
  "미착수", 상세표에선 완료. 요약 절 상단에 시점 명기 필요.
- **L-6. `README.md` 기능표에 구현 완료 5종 누락** — FLIGHT_MODE, COUNTER_MGMT,
  waypoint readback, route op 4종, 권한검증 §18.11. 반대로
  `uplink_app/README.md:75`는 viewpoint를 "미구현"으로 표기하나 BL-10이 "범위
  제외(짐벌 미탑재)"로 종결됨.
- **L-7. 연대기 모순 없음** — 파일명 날짜와 본문 갱신일 차이는 명명 관례 문제.

### 참조 기준으로 신뢰 가능 (코드와 완전 일치, 2026-07-28 갱신분)

`notes/uplink_payload_map.html`, `notes/tdm_timing_map.html`,
`notes/lora_frame_map.html`, `bridge/lora_downlink_decoder.py`

---

## 2. 코드 문제 (2순위)

### 치명 — UP2 업링크 경로에서 전 명령 인가 거부

**위치**: `lora_tdm_app/fsw/src/lora_tdm_app_utils.c:662, 675`

`LORA_TDM_APP_ParseUp2Frame()`(:369)은 `Out->Flags = Buf[6]`으로 UP2 flags를
정상 디코드하지만, `ForwardUp2ToUplinkApp()`이 `FwdCmd.Flags = 0;`으로
하드코딩함 (CRC 입력 `CrcBuf[3]`도 0). **v1 ASCII 경로(:575, :589)는 Flags를
제대로 실음 — v2 경로만 누락.**

**영향**: `uplink_app_cmds.c:297`의 `auth_level = (Flags >> 6) & 0x3`이 항상 0.
`GetClassRequiredLevel()`은 모든 클래스에 최소 1 이상을 요구(DIAGNOSTIC=1,
CONFIG/ROUTE/VIEWPOINT=2, RECOVERY/MODE/COUNTER/FLIGHT_MODE=3)하므로
`IsAuthorized()`가 항상 false → **전 명령이 `UPLINK_APP_AUTHZ_BLOCK_EID`로 거부.**
FORCE 플래그·재전송 인덱스 진단정보도 유실.

**재현**: v2 설정에서 지상국이 auth=3, token≠0인 FLIGHT_MODE HOVER 전송 →
lora_tdm은 `RxCmdCount++`/ACK OK로 정상 처리, uplink_app은 "command blocked
(insufficient auth) auth=0 required=3" 거부. **링크는 살아있는데 명령이 전혀
안 먹는 형태.**

**수정**: `FwdCmd.Flags = Decoded->Flags;`, `CrcBuf[3] = Decoded->Flags;`

**연관**: §1 H-7 — spec이 이 필드를 "예약(0)"이라 기재하고 있음.

### High

#### C-1. PX4 DO_SET_MODE 파라미터 인코딩 불일치 (추정, 확신 높음)

`mavlink_bridge_app/fsw/src/mavlink_bridge_app_utils.c:1084-1090`

`CustomMode = (MAIN_MODE_AUTO << 16) | (SubMode << 24)`로 **HEARTBEAT.custom_mode
패킹 형식**을 만들어 COMMAND_LONG의 param2에 넣고 param3(offset 8)은 0으로 둠.
PX4 Commander의 `MAV_CMD_DO_SET_MODE` 핸들러는 param2 = `custom_main_mode`(작은
enum, AUTO=4), param3 = `custom_sub_mode`(AUTO_MISSION=4 등)를 기대함. QGC도 그렇게 보냄.

PX4가 param2=0x04040000(67371008)을 main_mode로 해석 → `MAV_RESULT_DENIED`.
HOVER/WAYPOINT/LAND 3종이 전부 무시됨. 그런데 전송 성공만으로
`EXEC_RESULT_GENERIC_OK`를 회신(:1126)하므로 **지상국에는 성공으로 보이는
무증상 실패**. C-4(COMMAND_ACK 오프셋 오류)와 겹쳐 ACK로도 검출 불가.

수정: `WriteFloatLE(&Payload[4], (float)MAVLINK_PX4_MAIN_MODE_AUTO)`,
`WriteFloatLE(&Payload[8], (float)SubMode)`. 실기 COMMAND_ACK 검증 필요.

#### C-2. MAVLink v2 trailing-zero truncation 미처리 → 정상 프레임 폐기

`mavlink_bridge_app_utils.c:1149`(ATTITUDE), `:1209`(LOCAL_POSITION_NED),
`:1267`(GLOBAL_POSITION_INT)

`PayloadLen != EXPECTED_LEN`이면 무조건 length error. MAVLink v2는 payload 끝
0바이트를 잘라 보내는 게 **정상 규격**(`_mav_trim_payload`, PX4 기본 동작).

ATTITUDE 마지막 필드 `yawspeed`가 정확히 0.0f(지상 정치, 호버 트림에서 흔함)
→ FC가 24B 전송 → `!= 28`로 폐기, `ParseErrorCount` 증가, 자세 텔레메트리 구멍.
`vz==0`, `hdg==0`도 동일. **CRC는 통과하므로 원인 추적이 어려움.**

**같은 파일 `HandleSysTime()`(:1410-1428)에는 이미 zero-extend 처리가 있음**
— SYS_TIME만 고치고 나머지 3개는 누락.

#### C-3. HEARTBEAT 경로에 CRC 검증이 아예 없음

`mavlink_bridge_app_utils.c:1471-1490`

다른 모든 msgid는 `ComputeFrameCrc` 후 비교하는데 HEARTBEAT 분기만 CRC를
계산조차 하지 않고 `TargetSystemId/ComponentId` 락온, `SetLinkState(CONNECTED)`,
`UpdateFromHeartbeat()`(→ `IsArmed`)까지 수행.

노이즈/보율 미스매치로 파서 재동기화 중 msgid=0은 확률적으로 가장 잘 나오는
값이고 `PayloadLen>=9`만 만족하면 통과. 결과: (a) 엉뚱한 SysId 락온 → 이후 진짜
FC 하트비트를 무시, (b) 링크가 죽었는데 CONNECTED 유지, (c) `IsArmed`가 쓰레기
값(BaseMode bit7)으로 뒤집힘.

#### C-4. NoAckCount가 영원히 0 → DEGRADED 판정 사망

`lora_tdm_app/fsw/src/lora_tdm_app.c:242`

```c
if (LORA_TDM_APP_Data.RxAckCount == 0 && LORA_TDM_APP_Data.NoAckCount < 0xFFFF)
```

`RxAckCount`는 사이클별이 아니라 **부팅 이후 누적**이며 사이클 시작 시 리셋되지
않음. 첫 ACK를 한 번이라도 받으면 이후 링크가 완전히 끊겨도 `NoAckCount`가
증가하지 않음.

재현: 부팅 → ACK 1회 → 안테나 분리 → NoAckCount 0 고정 → `UpdateLinkState()`의
DEGRADED 분기 절대 미성립. HK/LinkStatus에도 항상 0 보고. (DISCONNECTED만 5초
타임아웃으로 생존.) 부작용: `ResetCounters`가 `RxAckCount=0`으로 만들면 그때부터
다시 증가 → 카운터 의미가 명령 이력에 따라 달라짐.

수정: 사이클 진입 시 `AckReceivedThisCycle` 플래그로 판정.

#### C-5. RX 창이 실제로 열리지 않음 — 첫 빈 read에서 즉시 종료

`lora_tdm_app.c:148-152` (+ `OpenSerial` :85-89)

`OpenSerial()`이 `VMIN=0/VTIME=0` 설정 후 `O_NONBLOCK`을 **해제**함. 이 조합의
`read()`는 데이터가 없으면 즉시 `0` 반환. 그런데 `RunRxWindow()`가 `Rc == 0`에서
`break`. `RX_WINDOW_MS(50ms)` 데드라인 루프가 사실상 "1회 폴링"이 되어, DL2 송신
직후 아직 응답이 도착하지 않은 정상 상황에서 창이 t≈0에 닫힘.

재현: DL2 전송 후 지상국이 10ms 뒤 ACK2 → 이미 창 종료 → 다음 사이클(≈100ms 후)
수신. half-duplex "TX 슬롯 → RX 창" 가드 구조가 무력화되고 ACK가 항상 1사이클 지연.

**방증**: `notes/tdm_timing_map.html:275`의 "100+TX+50≈150ms여야 하는데 실측
100ms" 미해결 질문 — 원인이 이것.

수정: `Rc == 0`일 때 break 대신 `OS_TaskDelay(1)` 후 continue(데드라인까지 유지),
또는 `VMIN=0/VTIME=1` + 블로킹 read.

#### C-6. DL2 seq 16비트 절단 vs LastSentSeq 32비트

`lora_tdm_app_utils.c:232` (`PutU16LE(&Buf[2], (uint16)AppData->DownlinkSeq)`),
`:707`, `lora_tdm_app.h:54`

프레임에는 `(uint16)DownlinkSeq`가 실리고 ACK2의 `SeqEcho`도 0~65535인데, 비교
대상 `LastSentSeq`는 uint32 무절단 값.

재현: 100ms 주기(10Hz)로 약 1시간 49분 운용 → `DownlinkSeq` 65536 도달 → 지상이
정상 ACK(SeqEcho=0)를 보내도 `0 != 65536` → 이후 **모든 ACK마다** `SEQ_FAIL_EID`
+ `SeqFailCount++`. EVS 이벤트 10Hz 폭주.

spec §4가 "seq wrap 허용"을 명시하므로 코드가 spec 위반. v1 경로는 무절단이라
영향 없음. 수정: `if ((uint16)SeqEcho != (uint16)AppData->LastSentSeq)`.

#### C-7. MODE 명령이 완전한 dead-end

`cfs_core_app/fsw/src/cfs_core_app_utils.c:1114-1157`

1. `CurrentModeState`는 이 함수 안에서만 읽고 쓰이며 다른 어떤 코드도 참조하지
   않음 (전 저장소 grep 확인) — **상태 전이가 아무 동작도 바꾸지 않음.**
2. RECOVERY/CONFIG와 달리 `CFS_CORE_APP_PublishExecResult()`를 **호출하지 않음**
   → uplink_app이 영원히 `ROUTED`에 머물고 EXECUTED_OK/FAILED를 못 받음.
3. 거부돼도 `ErrCounter`가 안 오르고 `CmdCounter`는 이미 증가(:1118).

재현: 지상에서 `ENTER RECOVERY`(auth 3, token≠0) → uplink는 ROUTED, cfs_core는
"TRANSITION" 이벤트만, `SYSTEM_HEALTH_MID`의 HealthState 불변. **지상국이 모드
진입 성공/실패를 구분할 수단이 없음.**

#### C-8. VIEWPOINT 명령도 dead-end

`cfs_core_app_utils.c:795-814` — `ViewpointCmd.*`에 캐시만 하고 어디서도 소비
안 함(쓰기 11곳, 읽기 0). EXEC_RESULT 미발행, `CmdCounter` 증가도 없음.

### 공통 구조 결함 — SB 메시지 길이 미검증 (4개 앱 전부)

| 앱 | 검증 적용 범위 | 위치 |
|---|---|---|
| mavlink_bridge | `MissionQuery` 1개뿐 | `mavlink_bridge_app_dispatch.c:22-70` |
| lora_tdm | GROUND_CMD 3종만 | `lora_tdm_app_dispatch.c` |
| cfs_core | 자체 CMD_MID만, cross-app 7종 무검증 | `cfs_core_app_dispatch.c:246-290` |
| uplink | 3개 함수에 else 누락 | `uplink_app_utils.c:366,579,603` |

`memcpy(..., 37 × sizeof(ROUTE_WAYPOINT_t))` 형태 over-read가 세 앱에 존재:

- `mavlink_bridge`: ROUTE_UPDATE_MID 짧은 메시지 → `Msg->Waypoints[i]`(37×32B)
  버퍼 밖 read → 쓰레기 waypoint가 FC로 업로드
- `lora_tdm`: `ProcessRouteSnapshot`(`utils.c:989`) → 최대 37×32=1184B over-read
- `cfs_core`: `SetRouteCacheWaypoints()`의 `memcpy(..., sizeof(Cache->Waypoints))`

**uplink는 실제 도달 가능**: `if (Cmd->PayloadLength >= N)`이 거짓일 때 else가
없어 필드 0인 채로 발행하고 `true` 반환 → ACCEPTED/ROUTED 집계. DIAGNOSTIC은
요구 auth가 1이고 `ValidateProxyCommand`가 ROUTE/VIEWPOINT에만
`PayloadLength==0` 거부(:170-176)를 걸므로 **payload 0바이트 DIAGNOSTIC이
통과 → `DiagAction=0` 발행 → lora_tdm의 `DIAG_ACTION_LINK_STATUS`가 실행됨
(요청하지 않은 동작)**. RECOVERY/MODE는 레벨 3 토큰 게이트 덕에 우연히 막히는
것이지 방어가 아님.

→ 개별 수정보다 **공통 매크로로 일괄 처리** 권장.

### Medium

| ID | 항목 | 위치 | 증상 |
|---|---|---|---|
| C-9 | COMMAND_ACK 필드 오프셋 오류 | `mavlink_bridge_app_utils.c:1494-1497` | `command`@0-1/`result`@2가 정답인데 `Payload[8]`/`Payload[0]` 사용. `StreamRequestAckCount` 영구 0. `>=10U` 조건 때문에 base-only(3B) ACK는 전부 `RecordParseError` |
| C-10 | 아무 바이트나 받으면 CONNECTED 승격 | 동 `:1994-1997` | CRC 무관하게 `SetLinkState(CONNECTED)` → 노이즈만 들어와도 STALE 판정 미발동. "링크 살아있는데 텔레메트리 없음"을 정상 보고. `StartMissionUpload`의 링크 가드도 무력화 |
| C-11 | `ServiceSerial`이 SB 타임아웃 때만 호출 | `mavlink_bridge_app.c:29-40` | 파이프(깊이 32)에 메시지가 차 있으면 200ms간 시리얼 미독. 921600 baud에서 tty 버퍼 오버런 |
| C-12 | uint8 오버플로 → 조용한 미션 절단 | `mavlink_bridge_app_utils.c:518` | `NewCount = ActiveCount + IndexOrCount` (검증 없는 wire 값). Active=16+요청250 → 266 → `NewCount=10`. **MAX 이하라 절단 경고 분기도 안 탐** |
| C-13 | 부분 write / EINTR 미처리 | 동 `:366-373` | fd가 `O_NONBLOCK`(:968)인데 `write != FrameLen`으로만 판정. 미션 업로드 연속 전송 시 절반만 나간 프레임이 회선에 남고 보정 없음 → FC 파서 desync |
| C-14 | read() EINTR을 링크 다운으로 오인 | 동 `:2487-2503` | 시그널 수신 시 멀쩡한 링크를 끊고 파서·스트림요청·락온 전부 리셋. 또 `while (read > 0)`가 무제한 루프라 SB 처리·하트비트 기아 |
| C-15 | 업로드/readback 상태머신 상호 배제 없음 | 동 `:488`, `:701` | readback 진행 중 ROUTE_UPDATE 유입 시 MISSION_ACK/COUNT 응답을 서로 오인. 업로드는 타임아웃 3회 후 실패 |
| C-16 | UP2 `plen` 상한 검증 없음 | `lora_tdm_app.c:173-183` | 잡음 plen=0xFF → 목표 264B인데 저장 상한 255B → 완성 불가. 그 사이 **정상 프레임들이 유령 프레임 본문으로 흡수**(최대 255B 소실). spec §11.2가 요구하는 magic 재스캔도 미구현 |
| C-17 | 시리얼 열기 실패 시 EVS 폭주 | 동 `:412-415`, `:55` | 매 사이클 재오픈 시도 + `CFE_EVS_Register(NULL,0,0)`(필터 0개) → LoRa 미연결 부팅 시 ERROR 이벤트 10Hz 지속 |
| C-18 | DEGRADED 임계값 산술 충돌 | `default_lora_tdm_app_mission_cfg.h:10-11` | `50 × 100ms = 5000ms` = `LINK_TIMEOUT_MS`. DISCONNECTED를 먼저 검사하므로 DEGRADED 관측 창 없음. spec §7 의도는 3s vs 5s |
| C-19 | RouteType 값이 route_op enum과 의미 충돌 | `cfs_core_app_utils.c:1188` | 스냅샷이 `ROUTE_SEGMENT_MISSION_EXTENSION`(=1)을 넣는데 BL-61 이후 같은 오프셋은 route_op(REPLACE=1). 수신측이 "REPLACE 연산"으로 읽음. 코드 주석 :110-118이 경고한 충돌의 반대 방향 |
| C-20 | ResetCounters가 replay 방어 무력화 | `uplink_app_cmds.c:321-335` | `AcceptedCount=0` + `LastAcceptedSequence=0` → `CheckSequence()` 부트스트랩 분기(:252)가 열려 **임의의 오래된 seq 1개가 무조건 수락**. `SaveState()` 미호출로 파일/메모리 괴리 |
| C-21 | uplink 자기 카운터 리셋 직후 부활 | `uplink_app_utils.c:406-409` + `cmds.c:705` | COUNTER_MGMT scope=UPLINK이 0으로 만든 직후 `ProcessUplink`이 `CmdCounter++` → 항상 1. Accepted/Rejected/Duplicate는 미처리로 로컬 CC 경로와 비대칭 |
| C-22 | 지상 readback 조립기가 이전 세션과 혼합 | `bridge/lora_downlink_decoder.py:256-262` | 세션 경계를 `total_pages` 변화로만 판정. 같은 페이지 수 경로를 연속 조회하면 **이전 경로 waypoint가 남은 채 완성 처리** |

### Low

- **C-23** `mavlink_bridge_app_utils.c:1902` — `if (Byte > MAVLINK_MAX_PAYLOAD_LEN)`
  에서 `Byte`는 uint8, 상수는 255U → **항상 거짓인 죽은 검사**. (버퍼가 정확히
  255B라 실 오버플로는 없으나 가드가 있다고 착각하기 쉬움.)
- **C-24** 동 `:734` — `PublishFcMissionReadback()`이 매 게시마다 `CFE_MSG_Init()`
  호출 → TLM 시퀀스 카운터 리셋 → 지상에서 패킷 유실 검출 불가. Init은 1회만.
- **C-25** 동 `:741-744` — `Count` 초과분 `Waypoints[]` 미소거로 이전 잔여값 전송.
- **C-26** 동 `:1129-1137` `ProcessSerialReconnectCmd` — `OpenSerial()` 반환값을
  `(void)`로 버리고 실패해도 `CmdCounter++` 및 성공 톤 INFORMATION 이벤트.
- **C-27** `mavlink_bridge_app.h:26` `ReconnectIntervalMs` — Init에서 세팅만 되고
  실제로는 `ActiveConfig.ReconnectIntervalMs`만 사용. 죽은 필드.
- **C-28** 동 `:427` `SendMissionItemInt` — `ActiveResumeIndex >= NewCount`면
  (REPLACE로 수가 줄어든 경우) `current=1` 항목이 없어 FC가 seq 0부터 재개. *(추정)*
- **C-29** `lora_tdm_app.c:280-291` — v1 모드에서 waypoint readback이 영구 pending.
  `RoutePageIndex` 전진이 v2 분기에만 있어, v2 전환 시 stale route 재송출.
- **C-30** `lora_tdm_app_utils.c:502` — `sscanf("%[^,]")` 폭 미지정. 현재는
  입력원이 256B라 무해하나 `LINE_BUF_LEN` 상향 시 즉시 스택 오버플로.
- **C-31** 동 `:414-421` — `NowMs < LastAckTimestampMs`(uint32 ms 롤오버 ≈49.7일)면
  `Elapsed=0` 처리로 링크 끊김 누락. 모듈러 뺄셈이 정답.
- **C-32** `uplink_app_utils.c:547,562-563` — `ConfigPendingState = PENDING` 직후
  같은 함수에서 덮어써 PENDING이 텔레메트리에 나타날 수 없음.
  `LastConfigResult = (uint8)(!Ok)`는 성공=0/실패=1로 `ConfigResult_t`와 의미 반대.
- **C-33** `cfs_core_app_utils.c:228-246` — `UpdateStateCache()`가 중복/역행 seq로
  조기 return 해도 `AttitudeFieldsFinite()` 실패 시 `Valid=false`가 **거부된
  메시지 때문에** 유효 캐시에 적용됨.
- **C-34** `shared_msgs/route_msg.h:29` — `SourceSequence`만 uint32(타 5개 메시지는
  uint16). EXEC_RESULT에서 절단되므로 실피해 없으나 wire 감사 시 함정.
- **C-35** `default_cfs_core_app_internal_cfg_values.h:4` — 파이프 깊이 16에 고레이트
  FC 스트림 4종 + 전 명령 채널 집중. 명령 메시지가 조용히 drop될 수 있음
  (mavlink_bridge는 32).
- **C-36** `bridge/lora_downlink_decoder.py:125` — SYSTIME 비트가 있는데 길이가
  모자라면 조용히 `sys_time=None` + tail 오프셋 유지 → 에러 없이 오디코딩.
  `DecodeError`가 맞음.
- **C-37** 동 `:192` `encode_dl2` — `wp_waypoints` 3개 이상이면 초과분이 조용히
  버려지고 `wp_in_page`에는 실제 개수 기록. 테스트/시뮬 전용이라 Low.

---

## 3. 테스트 / 빌드 / 기타 (3순위)

### 실행 결과 (실측)

```
$ python3 -m pytest tests -q
FAILED tests/test_camera_prototype.py::ConsistencyTest::test_bench_todos_tracked
1 failed, 209 passed in 2.00s

$ python3 -m pytest tests --cfs -q
1 failed, 209 passed in 1.10s        # --cfs 를 줘도 테스트 수 동일
```

### T-1 [P0]. cFS 통합테스트 3파일이 0개 수집

`tests/test_uplink_e2e.py:76,114`, `tests/test_lora_fc_downlink_e2e.py:53`,
`tests/test_rec_serial.py:38`

클래스명이 `UplinkE2ETest`, `LoraFcDownlinkE2ETest`, `SerialRecoveryTest` —
pytest 기본 `python_classes = Test*`(접두사) 규칙에 안 맞고 `unittest.TestCase`도
상속하지 않음. 세 파일 전부 `no tests ran`. `@pytest.mark.cfs_required`와
conftest의 `--cfs` 스킵 로직 전체가 붙을 대상이 없어 무의미.

**cFS 실행 경로 통합검증이 존재하는 것처럼 보이지만 실행된 적이 없음.**
`--cfs`로 실기 검증했다는 근거가 성립하지 않음. 통과하는 `ConsistencyTest`류는
TestCase를 상속해 수집되므로 이 차이가 드러나지 않았음.

추가로 `tests/test_uplink_e2e.py:88,101,113`(및 :124 부근)에
`assert True  # placeholder` 4곳 — 수집되더라도 검증 없음.

### T-2 [P0]. `test_lora_fc_downlink_packet.py` 26개가 실제 코드와 다른 스펙 검증

자기참조 구조: `build_fc_packet()`가 C 포맷을 Python으로 재구현하고 같은 파일의
`parse_fc_packet()`로 되파싱해 assert. **C 소스를 전혀 안 탐.** 게다가 스펙 값이
실제와 불일치:

| | 필드 수 | 각도 | 위치 |
|---|---|---|---|
| 실제 (`lora_tdm_app_utils.c:85`) | **18** | `%.4f` | `%.4f` |
| 테스트 (`:56-62,71`) | **17** | `%.6f` | `%.3f` |

C 주석(`utils.c:75-78`)에 "sats를 18번째 필드로 추가, 구 파서(17필드)와 호환"이
명시돼 있는데 테스트는 그 구 파서에 멈춰 있음. 앱 이름도 이미 없어진
`lora_fc_downlink_app` 기준.

→ C의 snprintf 포맷을 단일 소스로 삼아 갱신하거나, 이 파일을 폐기하고 C UT
(`coveragetest_lora_tdm_app_utils.c`)로 일원화.

### T-3 [P1]. 유일한 실패는 실제 버그

`tests/test_camera_prototype.py:117`
```
IsADirectoryError: [Errno 21] Is a directory: '.../camera/__pycache__'
```
`for path in CAMERA_DIR.iterdir(): path.read_text()` — 디렉터리 필터 없음.
`camera/correlate_video_telemetry.py`가 한 번이라도 import되면 영구 실패.
수정: `if path.is_file()` 가드.

### T-4 [P1]. CRC 교차검증이 CI 기본 경로에서 누락

`bridge/test_crc16_cross_validation.py`가 `tests/` 밖이라 `pytest tests`에
미포함(단독 실행 시 3 passed). 게다가 `legacy/bridge/` 두 모듈을 import — 사실상
legacy 코드를 검증 대상으로 붙잡아 둠.

### T-5. 커버리지 쏠림

- `tests/test_mavlink_uart_bridge.py`, `test_hb_parse.py`,
  `test_lora_uplink_bridge.py`, `test_uplink_lora_frame.py`,
  `test_mission_upload_diag.py` — **총 85개 테스트가 `legacy/` 모듈 대상.**
  현역 `bridge/lora_downlink_decoder.py`(439줄) 검증은 33개뿐.
- `tools/analyze_downlink_csv.py`(154줄): 대응 테스트 전무.
- C UT assert 밀도: cfs_core 406/1557줄, uplink 332/1701, lora_tdm 287/1990,
  **mavlink_bridge 274/2783** ← 코드 최다·assert 최소. **§2에서 High 결함이 가장
  많이 나온 앱과 일치.**

### T-6 [P1]. Perf ID 91 중복

`cfs_core_app/config/default_cfs_core_app_perfids.h:4`,
`mavlink_bridge_app/.../perfids.h:4`, `uplink_app/.../perfids.h:4`,
`telemetry_app/.../perfids.h:26` — 전부 91. lora_tdm만 93.

동시 구동 3개 앱이 같은 perf ID를 써서 perf 로그 판독 불가. 92는 비어 있음.

### T-7 [P1]. EDS 빌드에서 unit-test configure 실패 (추정)

`mavlink_bridge_app/fsw/src/mavlink_bridge_app_eds_dispatch.c`는 존재하는데
`coveragetest_mavlink_bridge_app_eds_dispatch.c`가 없음. unit-test CMakeLists는
`APP_SRC_FILES` 각각에 대응 테스트를 요구하므로 `CFE_EDS_ENABLED=ON` +
`ENABLE_UNIT_TESTS=ON`에서 실패. (타 3개 앱은 보유.) 현재 non-EDS 빌드만 써서 잠복.

### T-8 [P2]. 빌드 산출물이 커밋되어 있음 — Pi 배포 시 빌드 파손

`.gitignore`에 `__pycache__/`, `.venv/`는 있으나 빌드 산출물 20여 개가 트래킹 중:
`*/build_check/CMakeCache.txt`, `mavlink_bridge_app/build/CMakeCache.txt`,
`Testing/Temporary/*`, `legacy/img_app/build_check/*`.

내용에 Windows 절대경로가 박혀 있음:
```
CMAKE_HOME_DIRECTORY:INTERNAL=C:/Users/sdh97/Documents/GitHub/cfs-telemetry-app/mavlink_bridge_app
```
→ Pi에서 클론 시 stale 캐시가 cmake configure를 깨뜨림.
조치: `git rm -r --cached` + `.gitignore`에 `build/ build_check/ Testing/`.

### T-9 [P2]. CI_LAB 부재 → 지상 툴 전량 무동작

`mission_defs/targets.cmake:105`의 `cpu1_APPLIST`에는 `ci_lab`이 있으나
`mission_defs/cpu1_cfe_es_startup.scr`에는 제거돼 있음(테스트용 scr에만 존재).
**`tools/query_fc_mission.py`·`uplink_*_sender.py`가 전부 CI_LAB UDP 1234를
전제**하므로 프로덕션 scr로 배포하면 지상 툴이 전부 무응답.

### T-10 [P2]. 빌드에 안 들어가는 앱

`telemetry_app/`, `legacy/img_app/` 둘 다 CMakeLists·소스 완비이나 targets.cmake
APPLIST/SEARCH_PATH 어디에도 없음. `telemetry_app`은 legacy/ 밖에 남아 현역처럼
보이면서 MsgId/PerfId 네임스페이스만 점유. (NASA 샘플 앱 원본 그대로이며 명령
경로에 관여하지 않음.)

### T-11 [P2]. legacy 삭제 불가 상태로 고착

`legacy/`는 빌드에 안 들어가지만 Python 테스트 5개 파일 +
`bridge/test_crc16_cross_validation.py:20-21`이 import하므로 삭제 불가.

### T-12 [P1]. `runtime_app_restart_test.sh` 판정이 항상 PASS

`tools/runtime_app_restart_test.sh:99-101`
```bash
if journalctl -u cfs.service --since "$LOG_MARK_TIME" ... | grep -qi "$APP"; then
```
[4/4] 판정이 앱 이름 단순 grep — 1단계에서 보낸 STOP_APP 로그 자체가 매칭되므로
**재기동이 실패해도 PASS**. 또 위에서 계산한 `RESTART_EID_NAME`/`RESTART_EID_NUM`
(15/16)은 선언만 되고 미사용([3/4]는 `grep -q "restart attempt"` 문자열로 대체).

### T-13. 하드코딩 / 실기 배포 리스크

| 심각도 | 위치 | 내용 |
|---|---|---|
| P1 | `default_lora_tdm_app_mission_cfg.h:47` | LoRa 시리얼이 CP2102 by-id 고정. 시리얼번호 미기록 CP2102는 by-id가 unstable — 동일 칩 어댑터 2개면 `_0001-if00`이 다른 장치에 붙을 수 있음. udev 규칙/환경변수 오버라이드 필요 |
| P1 | `tools/query_fc_mission.py:24-25` 외 senders | 기본 타깃 `127.0.0.1:1234` = CI_LAB (T-9 참조) |
| P2 | `camera/*.sh` 5개 | `192.168.1.10`(카메라) / `sdh2983@192.168.50.65`(Pi) 하드코딩. 인자로 덮을 수 있으나 문서 예시가 실 IP |
| P2 | `camera/msposd_air.sh:11` | `FC_UART=/dev/ttyS2` 고정 + 바로 위 주석 "후보… 배선 후 확인" — 미검증 값이 기본값 |
| P2 | `camera/check_sd_recording.sh:16` | `RECORDS_PATH`에 `TODO(bench): 실제 마운트 경로 확인` 주석 잔존 |
| P2 | `bridge/lora_downlink_decoder.py:23` + `legacy/bridge/*.py` | 동일 by-id 경로가 4곳 중복(단일 소스 없음) |
| P3 | `coveragetest_cfs_core_app_utils.c:1364~1806` | UT가 `/tmp/`에 15개 이상 실파일 생성. 병렬 실행/샌드박스 충돌 가능 *(추정)* |

### T-14 [P2]. 저장소 위생 — CRLF 오염

Windows 클론(`C:\Users\sdh97\Documents\GitHub\cfs-telemetry-app`)에서 74개 파일이
modified로 표시되나 `git diff --ignore-cr-at-eol`이 완전히 비어 있음. **내용 변경
0, 줄바꿈(LF→CRLF)만 변경** (11,706줄 추가 / 11,706줄 삭제).

이대로 커밋하면 전체 히스토리가 한 번 끊겨 이후 blame/diff가 무의미해짐.
```bash
git config core.autocrlf input      # 또는 .gitattributes 에 * text=auto eol=lf
git checkout -- .
```
※ WSL(`/home/sdh2983/cfs-telemetry-app`)에도 같은 레포 클론이 있어 병행 시 재발.
주 작업본 통일 권장.

---

## 4. 정합 확인됨 (오탐 방지 기록)

재감사 시 중복 조사를 피하기 위해 **문제가 없다고 확인된 것**을 남김.

### 프로토콜 / 인코딩
- **CRC16**: `bridge/lora_downlink_decoder.py:48-57`, `legacy/bridge/*.py`,
  `lora_tdm_app_utils.c:26-44` 모두 init 0xFFFF / poly 0x1021 / no-reflect 동일.
  C UT 벡터(`"123456789"→0x29B1`)와 Python 벡터 일치. uplink/lora/Python 3자 일치.
- **DL2 프레임 상수**: Python ↔ `lora_tdm_app_interface_cfg.h:19-42` 전 항목 일치
  (BASE_LEN 45, SYSTIME 8, TAIL 3, WAYPOINT_BLOCK 30, WP/PAGE 2, MAGIC D2/B2/A2,
  FLAG SYSTIME 0x01 / WAYPOINT 0x04). DL2 바이트 맵도 spec §4/§4.2/§4.3 및
  `lora_frame_map.html`과 offset 0~44, tail(45/53) 전부 일치.
  ※ 단 `DL2_FLAG_POS_SATURATED=0x02`는 Python에만 명명 상수로 존재하고 C는
  `utils.c:294`에서 리터럴 `0x02u` — drift 위험(경미).
- **MAVLink CRC_EXTRA 17종** 전부 규약과 일치. accumulate 구현도 표준 X.25 동일.
- **MISSION_ITEM_INT / MISSION_ITEM / COMMAND_LONG / MISSION_COUNT /
  MISSION_SET_CURRENT** wire 오프셋 전부 정확 (size-descending 정렬 준수).
  MISSION_ITEM_INT 38B 레이아웃·frame(GLOBAL_RELATIVE_ALT)·mission_type 정확.
  ※ `:406` 주석이 ArduPilot을 근거로 들고 있어 PX4 기준 갱신 권장.
- **PX4 custom_mode 인코딩**(HEARTBEAT 방향) 및 float 표현 정상.
  ※ COMMAND_LONG 방향은 C-1 참조 — 별개 문제.
- **UP 프레임 CRC** `ComputeProxyCrc` 버퍼 클램프 정상.
- **route waypoint wire 파싱**(29B 필드별 역직렬화): 오프셋·경계 정확.
  `4 + count*29 ≤ 196` 제약이 자동으로 `count ≤ 6`을 강제해 배열(37) 오버플로 불가.
- **seq uint16 wraparound 모듈러 비교**(BL-13) 및 중복/replay 3분기 정확.

### 구조 / 자원
- **MsgId·EventID·FcnCode 충돌 없음.** 앱 간 중복 정의는 전부 의도된 공유값으로
  일치: 0x08A0(BRIDGE_HK, 3곳), 0x08D0(UPLINK_HK, 2곳), 0x08E0(LORA_HK, 2곳),
  0x190B/0x190E/0x1912/0x1913/0x1914 등. counter mgmt CC=1, SET_FLIGHT_MODE CC=5
  모두 대상 앱과 일치. lora_tdm EID 1~24 유일.
- **`EKF_STATUS_MIN_PAYLOAD_LEN 21`**은 오프바이원처럼 보이나 `ResetParser()`가
  매 프레임 Payload를 memset하므로 v2 trim된 flags 상위바이트가 0으로 읽혀 정확.
- **타임아웃 비교**가 전부 `(int32)(Now - Deadline) >= 0` 형태로 32비트 랩어라운드
  안전 (mavlink_bridge). ※ lora_tdm은 C-31 참조.
- **`OpenSerial`** 실패 경로 fd 누수 없음, termios raw 설정 정상,
  `tcflush(TCIFLUSH)` + `ResetParser()`로 재연결 시 백로그 폐기 정상.
- **`SaveState`** tmp→fsync→rename→디렉터리 fsync 패턴 정상 (mavlink_bridge,
  lora_tdm 양쪽). config 체크섬 magic+version+checksum 구조 정상
  (단순 덧셈 3필드라 검출력이 약한 건 설계 의도).
- **미러 구조체**: `lora_tdm_app_utils.c:909`의 인라인 `SysTimeMsg_t` ↔
  `MAVLINK_BRIDGE_APP_SysTimeTlm_t` 필드 순서/타입 일치
  (`mirror_struct_layout_refactor_complete.md` 결론 유지). 다만 이 인라인 복제는
  해당 노트가 지적한 "삼중 진실" 잔존분 그대로.
- **`telemetry_app/`**은 NASA 샘플 앱 원본이며 이 시스템 명령 경로에 무관여.

### 미발견
- 문서 간 **명시적 날짜/연대기 모순 없음**.
- **stale 배너가 제대로 붙은 문서**: `lora_telemetry_bridge_design.md`,
  `lora_uplink_bridge_design.md` (`[폐기됨]` 표기 ✅).

---

## 5. 교차 참조 — 문서 오류가 코드 결함을 유발한 사례

| 문서 오류 | 유발된 코드 결함 |
|---|---|
| H-7 (spec이 UP2 flags를 "예약(0)"으로 기재) | §2 치명 — `FwdCmd.Flags = 0` 하드코딩 |
| H-6 (헤더 주석 "×12 = 28B" vs define 30) | BL-72 readback 개수 불안정 조사 혼선 |
| H-5 (16 vs 37 혼재) | 동 |

역방향으로, **코드 결함이 문서의 미해결 질문으로 기록된 사례**:

| 문서의 미해결 기록 | 실제 원인 |
|---|---|
| `tdm_timing_map.html:275` "150ms여야 하는데 실측 100ms" | C-5 (RX 창 `Rc==0` break) |
| `tdm_timing_map.html:277` DEGRADED 문서 드리프트 | C-18 + C-4 |
