# 통합 백로그 (2026-07-21 기준)

> temp/ 아래 문서들에 흩어져 있던 미완료 항목을 **중복 제거해서** 한 곳에
> 모은 것. 같은 일이 여러 문서에 다른 이름으로 적혀 있었고, 일부는 이미
> 해결됐는데 미해결로 남아 있었음.
>
> **착수 전 필독**: 각 항목의 "근거 문서"를 먼저 읽을 것. 이 백로그는
> *무엇이 남았는지*의 목록이지 *어떻게 할지*의 답이 아님. 특히 무선
> 프로토콜을 건드리는 항목은 반이중 TDM 슬롯 정렬(RX 윈도우 100ms),
> 4회 재전송 설계, 권한검증/request_token 규약과 충돌하면 안 됨.

---

## ✅ 요약 — 바로 진행 가능 vs 추후 결정 필요 (2026-07-21)

`BL-08`(실행결과 회신 채널) **완료(2026-07-22)** — 공용 `EXEC_RESULT_MID`로
CONFIG(3개 앱 전부)+RECOVERY(cfs_core_app) 배선. `BL-05`도 함께 해소.
`BL-11`(UFB 라디오 바이트 코드표)은 **별개 사안**으로 확인돼 BL-08 완료와
무관하게 여전히 미착수 상태로 남음.

### ▶ 바로 진행 가능 (모호성 없음, 착수만 하면 됨 — 2026-07-21 3차 확정)

```
✅BL-01  재전송 중복 오탐 차단           ← 완료(2026-07-21), 코드는 Pi 미배포
✅BL-04 링크 EID 3종 구현 — 완료(2026-07-21)
✅BL-06 stale TODO 주석 2건 정정 — 완료 확인(2026-07-22, 이미 제거돼 있었음)
✅BL-16 downlink_protocol 기체 엄격화(0/1만 수락) — 완료(2026-07-21)
✅BL-18 SaveState() fsync() — 완료(2026-07-21), 파일+부모디렉터리 둘 다
✅BL-19' 죽은 config 상수 안전 부분 삭제 — 완료(2026-07-21): SERIAL_REOPEN_DELAY_MS/
        PROTOCOL_VERSION(lora_tdm_app), IMAGE_ID_LEN/CAMERA_ID_LEN/ARTIFACT_REF_LEN
        (uplink_app+cfs_core_app, img_app 잔재) 삭제. MAX_PAYLOAD_LENGTH는 실사용 중이라
        존치. LORA_BAUDRATE·STREAM_REACQUIRE는 제외(아래 고민 목록)
✅BL-20 문서 stale 식별자(MID/앱이름) 정정 — 완료(2026-07-21)
✅BL-21 fcncode 정의 위치 중복 정리 — 완료(2026-07-21)
✅BL-03 다운링크에 seq+boot_count 동봉 — 완료(2026-07-22, v2/DL2, v1 제외)
  ├─ ✅BL-12 부트카운터 — 완료(2026-07-22)
  ├─ ✅BL-13 모듈러 seq 비교 — 완료(2026-07-22)
  └─ BL-14 재전송 인덱스(선택, 미착수 — RF 마진 진단용, 정확성에 필수 아님)
✅BL-09 cfs_core_app RECOVERY 명령 실제 연결 (+BL-25) — 부분 완료(2026-07-21)
✅BL-26/27/28  매직넘버 C↔Python 교차검증 테스트 — 완료(2026-07-21, openMCT)
```

**2026-07-22 기준**: 위 목록의 "바로 진행 가능" 항목 **전부 완료** (BL-14 제외,
명시적으로 선택사항). "고민 필요"(BL-17/19)도 전부 완료. 남은 것은
"추후 결정 필요"(BL-10/15) 뿐 — 전부 사용자 결정 또는 하드웨어(Pi) 필요.

### 🤔 고민 필요 (별도 기록, 결정 전까지 보류)

```
✅BL-17 LoadState() 실패 이벤트 구분 — 완료(2026-07-22)
✅BL-19 LORA_TDM_APP_LORA_BAUDRATE 실연결(shared_msgs/serial_baud.h)
        + STREAM_REACQUIRE_TIMEOUT_MS 삭제 — 완료(2026-07-22)
```

**목록에서 제거됨**: `BL-29` — 재확인 결과 **이미 완료 상태**
(`testcase_coverage_gap_2026-07-20.md` A-1~A-4/B-1 전부 `[x]`).

**순서 조정 권장**: `BL-07`은 `BL-09`(RECOVERY 실제 연결) **완료 후**로
이동 — 지금 갱신하면 BL-09 완료 후 다시 고쳐야 해서 이중작업.

### ⏸ 추후 결정 필요 (방향 자체가 미정이거나 후속 논의 필요)

```
✅BL-08 실행결과 회신 채널 — 완료(2026-07-22)
✅BL-05 BL-08과 함께 해소(2026-07-22)
✅BL-02 RunTx UFB 리셋 유지 확정 — 완료(2026-07-22)
✅BL-11 UFB 라디오 바이트 코드표 확정 — 완료(2026-07-22)
BL-10   VIEWPOINT 캐시 — "활용처 있음"만 정함, 뭘 할지는 미정
BL-15   5Hz 상향 — "필요하다"만 정함, 실측 전엔 상한 미정(Pi 필요)
```

### 🔧 하드웨어 있어야 착수 가능

```
BL-15(5Hz 실측), BL-22, BL-31~37
```

**추천 순서**: `BL-01`(지금 버그) → 나머지 "바로 진행 가능" 소규모 항목
(병렬 가능) → `BL-03` 프로토콜 개정 → `BL-09`.

---

## 🔴 긴급 — 현재 배포된 코드가 버그 상태

| ID | 내용 | 근거 | 규모 |
|---|---|---|---|
| ~~**BL-01**~~ | ✅ **완료(2026-07-21)**. `Sequence == LastAccepted`→`UPLINK_APP_RESULT_DUPLICATE(14)`, `< LastAccepted`→기존 `REJECT_SEQUENCE` 유지로 분리. lora_tdm_app은 DUPLICATE를 SEQ_FAIL로 오귀속하지 않음. 로컬 UT 477/477 PASS. 상세: `bl01_duplicate_retransmit_completed_2026-07-21.md` | `uplink_seq_feedback_redesign` T1 | 완료 |
| ~~**BL-02**~~ | ✅ **완료(2026-07-22, 유지 확정)**. `073a680`(RunTx UFB 리셋을 매 TX→명령 포워딩 시점으로 이동)은 실측으로 확인된 진짜 레이스 버그의 수정이라 롤백 사유 없음. BL-03(seq+boot_count 동봉)으로 지상 쪽 상관까지 보강돼 이 설계가 더 힘을 받음. UFB 바이트 자체에 seq가 안 붙는 한계는 BL-02가 아니라 BL-11 영역으로 분리 | 동 T2 / `inferred_decisions_selfaudit` A-1 | 완료 |
| ~~**BL-03**~~ | ✅ **완료(2026-07-22)**. DL2 프레임에 `uplink_last_seq(u16)`+`uplink_boot_count(u8)` 3바이트 동봉(기존 47B/55B→50B/58B, SysTime 뒤·CRC 앞). `uplink_app`→`UPLINK_STATUS_MID`(`LastAcceptedSequence`/`BootCount` 신규 필드)→`lora_tdm_app` 캐시→DL2 인코딩. 지상 `_SeqCounter.resync_from_device()`(앞으로만 당김, 자가복구)와 `_BootCountTracker`(모듈러 감소 감지 시 `boot_count_anomaly` 플래그만, 자동거부 없음) 구현. `lora_protocol_v2.py`+`bridge/lora_downlink_decoder.py`(포크본) 동시 갱신. 회귀: C 4종 UT 전부 PASS, Python 190/190 PASS(cfs-telemetry-app) + 55/55(openMCT) | 동 T3+T4 | 완료 |

> BL-03은 BL-04\~BL-06(프로토콜 확장)의 **공통 선행**. 무선 포맷을 여러 번
> 나눠 바꾸면 지상/기체 버전 호환이 계속 깨지므로 **한 번 열 때 필요한
> 필드를 모두 넣을 것**.

---

## 🟠 spec이 사실과 다름 (문서가 거짓 주장)

| ID | 내용 | 근거 |
|---|---|---|
| ~~**BL-04**~~ | ✅ **부분 완료(2026-07-21)**. `LINK_LOST/DEGRADED/RESTORED` 3종은 `UpdateLinkState()`에 `LinkStateInitialized` 플래그로 첫 관측 제외 + 엣지 트리거로 구현(추가 히스테리시스는 기존 다중사이클 임계값으로 충분해 불필요 판단). `PIPE_ERR/SUB_ERR/SB_SEND_ERR` 3종은 **범위 밖으로 미룸** — spec 표기를 ❌ 미구현으로 정정. 회귀 UT 135/135 PASS(신규 5건) | `system_wide_reaudit` F-1 |
| ~~**BL-05**~~ | ✅ **완료(2026-07-22, BL-08과 함께)** — `UPLINK_APP_RESULT_EXECUTED_OK/FAILED`(15/16) 추가, `mission_app_runtime_spec.md` §18.4.6.4의 "전달 vs 실행 구분" 표기가 실제로 사실이 됨 | `command_dead_end_audit` F1 |
| ~~**BL-06**~~ | ✅ **완료 확인(2026-07-22)**. 재확인 결과 해당 TODO 주석 2건은 이미 제거되어 있음(`lora_tdm_app.h`/`lora_tdm_app_utils.h`에 "미배선"/"미지원" 문자열 없음) — 별도 작업 불필요 | `system_wide_reaudit` F-2/F-3 |
| ~~**BL-07**~~ | ✅ **완료(2026-07-22)**. `cfs_core_app_command_execution_gap.md` 갱신 — MODE 상태 전이(`cfs_core_app_utils.c:883`, ENTER/EXIT 허용 조합 검증)와 RECOVERY 필드 구분 처리(BL-09) 둘 다 구현 완료로 정정. 남은 미구현은 VIEWPOINT 실행 로직(BL-10)과 타임스탐프 time base 검증뿐 | 오늘 확인 |

---

## 🟡 미구현 기능 (spec엔 있는데 코드가 없음)

| ID | 내용 | 근거 | 선행 |
|---|---|---|---|
| ~~**BL-08**~~ | ✅ **완료(2026-07-22)** — 공용 `EXEC_RESULT_MID`(0x1912, `shared_msgs/exec_result_msg.h`) 신설. `SourceSequence`(원본 seq 반사, 상관키)+`GenericResult`(OK/FAILED, uplink_app이 실제 사용)+`DetailCode`(대상앱 원시코드, 진단용)로 3개 앱의 서로 다른 스키마를 통일하지 않고 대분류로 흡수. `uplink_app`은 공용 MID 1개만 구독(사용자 결정), 타임아웃 없이 최신 수락 seq와 일치할 때만 반영(다음 명령이 오면 자연 무시, 사용자 결정). CONFIG는 3개 앱 전부, RECOVERY는 cfs_core_app만 배선 — ROUTE_UPDATE/MODE/VIEWPOINT/DIAGNOSTIC은 범위 밖. **주의**: CONFIG_CMD_MID는 3앱 공유 브로드캐스트라 scope 불일치(다른 앱 대상) 시 EXEC_RESULT 발행 안 함(발행하면 실제 대상 앱 응답과 경합해 오염). 회귀 UT: uplink_app 4종(10/102/35/108), cfs_core_app_utils(269), mavlink_bridge_app_utils(167), lora_tdm_app_utils(145) 전부 PASS, 총 1069/1069 | `ground_plan` P0 / `command_dead_end` F1 / T7 | 완료 |
| ~~**BL-09**~~ | ✅ **완료(2026-07-22)**. `RESTART_BRIDGE`/`RESTART_UPLINK`/`RESTART_LORA`는 2026-07-21에 실제 `CFE_ES_RestartApp()` 연결 완료. **`PARSER_RESET`/`SERIAL_RECONNECT`도 2026-07-22에 실제 연결됨**(P1-a) — 신규 MID 대신 `mavlink_bridge_app`의 기존 `CMD_MID`(`0x18A0`)를 FcnCode로 재사용(`PARSER_RESET_CC=3`/`SERIAL_RECONNECT_CC=4`), `CFS_CORE_APP_SendBridgeCtrlCmd()`로 발행 → `mavlink_bridge_app`이 실제 `ResetParser()`/`CloseSerial()+OpenSerial()` 호출. 회귀 UT: cfs_core_app 4종(19/7/277/35), mavlink_bridge_app 4종(14/4/170/34) 전부 PASS. BL-25(uplinkCLI 도움말)도 같이 정정 | `ground_plan` P1-a~d / `command_dead_end` F2 | 완료 |
| **BL-10** | 🔶 **재확인(2026-07-23): FC 실전송은 이번에도 보류로 재확정.** README상 "viewpoint 수신 후 FC MAVLink 명령 실행"은 명시적 범위 제외(2026-06-08) 상태 그대로 유지 — MAVLink 표준상 `DO_SET_ROI`/`CONDITION_YAW`/`DO_MOUNT_CONTROL`로 기술적으로 가능은 하나(짐벌 있으면 마운트 제어, 없으면 기체 자세만), **실제 짐벌 하드웨어 탑재 여부 미확인** + 이번 세션에서 "추후 고려로 제외" 결정. 대안으로 route와 동일한 "스냅샷 발행"(캐시→`VIEWPOINT_SNAPSHOT_MID` 신규→lora_tdm_app 다운링크, ROUTE_SNAPSHOT_MID 0x1913과 동일 패턴)도 제시됐으나 **미확정** — 다음 논의 필요 대상은 ① 짐벌 유무 확인 ② 확인 후 FC 실행 구현 vs 스냅샷만 발행 선택 | `ground_plan` P2 / `cfs_core_app_command_execution_gap` | 후속설계 필요, 하드웨어(짐벌) 확인 선행 |
| ~~**BL-11**~~ | ✅ **완료(2026-07-22)**. UFB 코드표를 4종→12종(`0x00~0x0B`)으로 확장 — uplink_app 내부 결과코드 번호를 그대로 복사하지 않고 UFB 전용 독립 번호로 순차 배정(무선 프로토콜을 SB 내부 enum과 분리). `lora_tdm_app_dispatch.c`에 8개 else-if 분기 추가(`FAILED`/`REJECT_VERSION`/`REJECT_CLASS`/`REJECT_LENGTH`/`ROUTE_MISS`/`REJECT_ROUTE`/`REJECT_CHECKSUM`/`REJECT_VIEWPOINT`). `DUPLICATE`(14)/`EXECUTED_OK`/`EXECUTED_FAILED`(15/16, BL-08)는 의도적으로 매핑 범위 밖(직전 값 유지). spec 3곳 갱신(`lora_tdm_app_behavior_spec.md` §9.2/§17, `lora_protocol_v2_spec.md`). 지상 디코더(openMCT, 별도 repo)는 이번 범위 밖 — 반영 작업 문서만 해당 repo `notes/temp/ufb_code_expansion_2026-07-22.md`에 기록 후 커밋/푸시(`f9e5a35`). 회귀 UT: lora_tdm_app 4종(64/14/145/59, dispatch에 신규 24케이스) 전부 PASS | T8 / `selfaudit` A-2 | 완료 |
| ~~**BL-CTR**~~ | ✅ **완료(2026-07-22)**. spec §18.4.6.7에 없던 counter management 명령 클래스(7)를 이번 세션 대화로 확정 후 구현. `uplink_app`이 `cfs_core_app` 경유 없이 대상 앱(mavlink_bridge/cfs_core/uplink 자신/lora_tdm, scope 1~4)에 직접 라우팅 — scope=uplink은 로컬 카운터 초기화, 나머지는 대상 앱의 기존 `CMD_MID`+`RESET_COUNTERS_CC=1`을 `CFE_MSG_SetFcnCode`로 재사용(신규 MID/CC 없음, P1-a 패턴 재사용). `lora_tdm_app` UFB `REJECT_COUNTER=12`(0x0C) 추가. 근거: `uplink_app`이 이미 `SYSTEM_HEALTH_MID`를 로컬 구독하므로 인가 판단에 cfs_core_app 왕복이 불필요(cFE SB 순수 비동기이기도 함). 회귀 UT: uplink_app_utils(120→131), uplink_app_cmds(+4), lora_tdm_app_dispatch(59→61) 전부 PASS | 2026-07-22 대화 확정 | 완료 |

---

## 🔵 설계 결정 필요 (구현 전 방향 확정)

| ID | 내용 | 근거 |
|---|---|---|
| ~~**BL-12**~~ | ✅ **완료(2026-07-22)**. 기체측 영속화(`uplink_app` 8비트 `BootCount`, `/cf/uplink_app_state.bin`) + BL-03에서 DL2 동봉/지상 `_BootCountTracker` anomaly 감지까지 완결 | T5 |
| ~~**BL-13**~~ | ✅ **완료(2026-07-22)**. `UPLINK_APP_CheckSequence()`를 모듈러 윈도우(`diff=(uint16)(seq-last); diff<0x8000`)로 변경 — 65535 wrap 해소. 회귀 UT 99/99(cmds) PASS | T6 / 문제3 |
| ~~**BL-14**~~ | ✅ **완료(2026-07-22)**. `Flags` `bits[2:1]=RETX_IDX`(0~3=슬롯-1, 0=최초 전송이라 구 프레임 하위호환) 확정 — spec §18.4.3.1 Flags 표에 기록(이때 bit[0] FORCE_FLAG 미반영 stale도 함께 정정). 기체(`uplink_app`): 수락/중복 EVS 이벤트에 `retx=` 표기만(검증 미사용, HK/DL2 변경 없음). 지상(openMCT `fc_serial_ws_server.py`): 큐를 프레임 문자열→구성요소 저장으로 바꿔 슬롯마다 flags+CRC 재조립. 회귀: uplink_app_cmds 110/110, openMCT pytest 56/56(신규 RetxIndexFlushTest 포함) | T9 | 완료 |
| **BL-15** | 🔶 **실측 완료(2026-07-22): Stage 4a(150ms)·4b(100ms) 둘 다 PASS** (100ms 손실 0.00%, 9.97pkt/s). **⚠️ 남은 결정: 최종 주기 확정** — `mission_cfg.h`가 실험값 100ms/RX 50/임계 50 **그대로 커밋돼 현재 배포본이 10Hz로 운용 중**(2026-07-23 확인). ① 100ms 확정 승인 or ② 200ms(5Hz) 원복 결정 필요. 부속 검토: 사이클=RX윈도우×2 구조의 마진(100ms 사이클에 RX 50ms — 실측상 문제 없었으나 마진 0 아님 재확인), CONFIG 파라미터화는 운용 요구 생기면(죽은 설정 방지). 실측 상세: `../bl15_stage4_5hz_cap_progress_2026-07-22_completed.md`, `../lora_downlink_5hz_cap_2026-07-21_completed.md` | 이관됨(2026-07-23) |
| ~~**BL-16**~~ | ✅ **구현 완료(2026-07-21)**. `LORA_TDM_APP_SetDownlinkProtocol`/`ProcessConfigCommand` 둘 다 `Value==0 or 1`만 수락, 그 외는 `ErrCounter++`+신규 EID(`SET_DL_PROTO_ERR_EID=21`)로 거부. 지상 `PARAM_BOUNDS(0,1)`과 대칭 일치. 회귀 UT 4종(64/14/36/125, 신규 2건 포함) PASS | `selfaudit` A-4 |

---

## ⚪ 위생 / 소규모 (독립 착수 가능, 프로토콜 무관)

| ID | 내용 | 근거 |
|---|---|---|
| ~~**BL-17**~~ | ✅ **완료(2026-07-22, 커버리지 갭도 해소)**. `UPLINK_APP_LoadState()`에 신규 `UPLINK_APP_STATE_CORRUPT_EID`(9) 추가 — `open()` 실패가 `errno==ENOENT`(파일 없음=첫 부팅)면 기존과 동일하게 조용히 반환, 그 외 open 실패/read truncated/bad magic/checksum mismatch는 전부 ERROR 이벤트로 보고 후 기본값 사용. 구현 직후엔 `/cf`가 테스트 환경에 없어 corrupt 4개 분기가 gcov상 0회(미커버) 상태였음 — `lora_tdm_app`/`mavlink_bridge_app`의 시리얼 경로 주입과 동일한 `UPLINK_APP_STATE_FILE_PATH` env var 패턴을 상태파일에도 적용해 `/tmp` 임시파일로 4개 분기(open 실패 non-ENOENT/truncated/bad magic/checksum mismatch) 전부 실행 확인. 회귀 4종(10/102/120/35) PASS, gcov로 4개 분기 전부 커버 재확인 | T10 |
| ~~**BL-18**~~ | ✅ **완료(2026-07-21)**. 파일 fd fsync 후 rename, rename 후 부모 디렉터리(`/cf`) fd도 fsync — POSIX 표준상 rename 자체 유실 방지에 필요. `/cf`가 없는 테스트 환경에선 open 단계에서 조용히 return(기존 동작 유지), 104/104 PASS | T11 |
| ~~**BL-19**~~ | ✅ **완료(2026-07-22)**. `git log -S`로 원 의도 추적: `LORA_TDM_APP_LORA_BAUDRATE`는 mavlink_bridge_app이 이미 갖고 있던 `GetBaudConstant()`(int→`speed_t` lookup) 패턴을 `shared_msgs/serial_baud.h`(공용, 두 앱이 함께 사용)로 이관해 실제 연결 — `lora_tdm_app.c`의 `B57600` 하드코딩 제거. `STREAM_REACQUIRE_TIMEOUT_MS`는 커밋 `8558793`에서 도입돼 실제 구현(`StreamRefreshNeeded()`)까지 됐다가 5분 뒤 `7c4aac3`에서 저자 본인이 되돌리며 상수만 지우는 걸 빠뜨린 죽은 잔재로 확인 → 삭제. 회귀 UT: mavlink_bridge_app 4종(14/4/167/26), lora_tdm_app 4종(64/14/145/39) 전부 PASS | `system_wide_reaudit` F-4/F-5/F-6 |
| ~~**BL-20**~~ | ✅ **완료(2026-07-21)**. `uplink_lora_test_status.md` 상단에 정정 안내 블록 추가 — `lora_fc_downlink_app`은 옛 이름, `UPLINK_RAW_MID=0x1909`는 실제 `FC_SYS_TIME_MID` 값임을 명시 | 동 F-7 |
| ~~**BL-21**~~ | ✅ **완료(2026-07-21)**. `config/default_lora_tdm_app_fcncode_values.h`는 어디서도 include 안 되는 죽은 중복본(SET_DOWNLINK_PROTO_CC도 누락)이라 삭제. 실사용은 `fsw/inc/lora_tdm_app_fcncodes.h`(dispatch.c가 이걸 include). 회귀 4종 234/234 PASS | 동 F-6 |
| ~~**BL-22**~~ | ✅ **유지 결론(2026-07-22 실측 근거)** — FC가 `/dev/serial0`(PL011 ttyAMA0) **921600** 보레이트 사용 중, PL011 고속 보레이트는 UART 클럭 48MHz 상향이 필요한 구성이라 "기각된 가설의 잔재"가 아니라 현 FC 링크 의존 설정일 가능성 높음. 제거 이득 없음+회귀 위험 → 유지 | `selfaudit` B-1 / 실측 |

---

## 🟣 지상국(openMCT, 별도 repo)

| ID | 내용 |
|---|---|
| ~~**BL-23**~~ | ✅ **완료(2026-07-22, openMCT)**. 타이머 기반 강제 송신은 넣지 않음(반이중 슬롯 정렬 유지, 실제 송신은 여전히 다운링크 수신 시에만). 대신 큐 크기 상한(16, 사용자 결정)을 둬 링크 단절 중 계속 쌓이는 걸 방지 — 초과 시 HTTP 에러가 아니라 가장 오래된 항목을 버리고 경고 로그만 남김, 새 명령은 그대로 accept. pytest 57/57 PASS(신규 `UplinkQueueCapTest`) |
| ~~**BL-24**~~ | ✅ **완료(2026-07-22, openMCT)**. 설계 논의로 실제 위험 확인: 새 seq 재조립은 큐에 남은 원본 사본이 뒤늦게 수락되면 **같은 명령이 이중 실행**되는 경합이 있었음(특히 RECOVERY/ROUTE append에 위험). A안(사용자 결정): 서버가 최근 전송 명령을 seq별 캐시(32개)하고 `/api/uplink/resend`가 같은 seq 그대로 재큐잉 — 원본이 이미 수락됐어도 기체 DUPLICATE(BL-01) 방어로 이중 실행 구조적 불가. uplinkGUI 3종(config/route/recovery) resend 클로저를 새 명령 재호출→resend 엔드포인트 호출로 교체, UFB=1 로그도 "같은 seq=N로 재전송"으로 정정. pytest 60/60 PASS(신규 `ResendEndpointTest` 3건) |
| ~~**BL-25**~~ | ✅ **완료(2026-07-21, BL-09와 함께)**. `uplinkCLI/plugin.js` 도움말을 실제 동작(byte[0]=action이 cfs_core_app RESTART 3종을 실제로 트리거함)으로 정정 |
| ~~**BL-26/27/28**~~ | ✅ **완료(2026-07-21, openMCT `c561aa3`)**. `tests/test_fc_serial_ws_server.py`에 형제 디렉터리 C 헤더를 정규식 파싱해 비교하는 교차검증 테스트 4건 추가(`DL2_BASE_LEN`↔`LORA_TDM_APP_DL2_LEN_FIELD`, `PARAM_BOUNDS[cfs_core/mavlink_bridge]`↔C 상수). repo 없는 환경은 skip. 35/35 PASS |

---

## 🧪 테스트

| ID | 내용 | 근거 |
|---|---|---|
| ~~**BL-29**~~ | ✅ **완료 확인(2026-07-22)**. A/B 그룹(A-1~A-4/B-1) 전부 `[x]` — 상단 요약(2026-07-22)에서 이미 제거 표기됐으나 이 표에 갱신 누락돼 있던 것 정정(2026-07-23) | `testcase_coverage_gap_2026-07-20` |
| **BL-30** | C-1: ~~WSL x86 cFS 빌드~~(✅ 해소 2026-07-23 — `~/cFS_clean/build-ut` ctest 검증됨, BL-41 작업 중 확인) + pytest PTY fixture + CI_LAB 주입/EVS 관측 헬퍼 (E2E 하네스) | 동 |

---

## 🔧 실물 하드웨어 필요 (Pi/LoRa/FC/카메라)

| ID | 내용 | 근거 |
|---|---|---|
| **BL-38** | 🔶 **수정 완료(2026-07-23), Pi 실기 재검증 대기**. 원 결함: RT-CORE-003 실기 FAIL로 발견(2026-07-22) — uplink/lora 자동 재시작이 fault 우선순위 else-if 체인 내부에 있어 상위 fault(EKF invalid 등) 지속 시 도달 불가 + 카운터 리셋됨. RT-CORE-004(lora)도 동일 확증. **구현(A안+세부설계 6항목 확정, spec §11.1)**: `CFS_CORE_APP_CheckAppRestarts()` 신설 — 체인과 독립, 사이클당 1건(우선순위 bridge>uplink>lora, 실제 발행 시에만 하위 스킵 — 최초 구현에서 기아 버그를 UT가 잡아내 수정), 무한 재시도(MAX_RESTARTS 제거), 카운터는 관측용 보존. 보고 체인은 순수화(FaultCode 의미 불변). UT 9개 추가(BL38-UT-1 등), coverage 전체 99/99 PASS. ExceptionAction 관련은 당초 대화 중 오설명(0=프로세서리셋)이 정반대(실제 0=앱만 재시작, cFE 정의 재확인) — 이미 4개 앱 전부 적용 중이라 수정 불필요, spec 정정 반영. Pi 재배포+RT-CORE-003~011 재시험은 최종 검증 때 일괄 | RT-CORE-003/004 실측 |
| **BL-39** | 🔶 **원인 확정(2026-07-23, Pi 재연결 직접 실측)**. 실기 발견(2026-07-22): uplink_app 영속 상태(`/cf/uplink_app_state.bin`)를 POSIX open 리터럴 경로로 사용. 당초 "root 권한 부족" 가설은 **오류로 정정**(`systemctl show cfs.service`: `User=root` 확인). 실제 원인: `/cf`가 파일시스템 루트에 부재해 `ENOENT`. `WorkingDirectory=~/cFS_clean/build/exe/cpu1`의 `cf/`(EEPROM.DAT 등 실사용 중)가 진짜 경로 — 확인 결과 `uplink_app_state.bin` 전무, SaveState 무동작 실물 확증. **범위 확대**: `cfs_core_app_state.bin`도 동일 패턴("/cf/cfs_core_app_state.bin")이라 **cfs_core_app도 동일 결함**. `Environment=` 미주입 확인(ⓐ 미적용 상태). **결론: ⓑ(상대경로 `cf/...`)가 기존 실경로와 정확히 일치** — **수정 완료(2026-07-23)**: uplink_app/cfs_core_app 둘 다 절대경로→상대경로 변경 + SaveState open/write/rename 3개 실패지점 전부 ERROR EVS(errno) 추가(uplink EID 10, cfs_core EID 17 신규). UT 8/8 PASS. Pi 재배포+boot_count 실측 재검증은 최종 검증 때 일괄. `runtime_test_session_2026-07-22.md` 참조 | RT-DL2-SYSTIME-001 실측 중 발견 |
| **BL-40** | 🔴 **GUI 실기 시험 FAIL로 발견(2026-07-22 21:20)**: `cfs_core_app`의 앱 이름 상수 3종(`CFS_CORE_APP_BRIDGE/UPLINK/LORA_APP_NAME`)이 소문자 바이너리명(`"mavlink_bridge_app"` 등) — cFE 등록명(`MAVLINK_BRIDGE_APP` 등, startup.scr 3번째 필드)과 불일치, `CFE_ES_GetAppIDByName` 대소문자 구분이라 **항상 실패**. 영향: ① 지상 RESTART_BRIDGE/UPLINK/LORA 명령 실기 전부 무동작(exec result FAILED detail=1, seq=12/13 실측) ② BL-38 체인 결함을 고쳐도 자동 재시작이 같은 상수라 어차피 실패(2중 결함). 단위테스트는 GetAppIDByName 스텁이 SUCCESS라 못 잡음. **수정 완료(2026-07-23): 상수 3개 등록명 대문자화**(`default_cfs_core_app_internal_cfg_values.h`), 동기화 후 UT 4/4 PASS. ✅ **실기 검증 완료(2026-07-23)**: RESTART_UPLINK/BRIDGE/LORA 3종 전부 Pi 실측 PASS(`CFE_ES_RestartApp ... Completed` 확인). PARSER_RESET/SERIAL_RECONNECT는 앱명 안 쓰는 FcnCode 경로라 정상. `runtime_test_session_2026-07-23.md` 참조 — **BL-40 완전 종결** | GUI 실기 시험 6번 |
| **BL-31** | D-1: 앱 재시작 실측 (`tools/runtime_app_restart_test.sh`) — 2026-07-22 1차 시도: 스크립트 앱명 버그(소문자→`UPLINK_APP` 등록명) 수정, CI_LAB 미탑재 발견·추가. uplink_app FAIL → BL-38 결함 발견. 결함 수정 후 재시험 필요 | `testcase_coverage_gap` |
| **BL-32** | D-2: TDM/LoRa/DL2-SYSTIME 실물 테스트군 — 🔶 **대부분 완료(2026-07-22)**: TDM-RT-001~006·RT-LORA-004 ✅(로그 증거), RT-DL2-SYSTIME-001 부분(실외 GPS 필요), 잔여=TDM-RT-007(CRC 변조 주입 도구 필요)/008(타이밍 주입 곤란)/009·RT-LORA-001(물리 조작). `runtime_test_session_2026-07-22.md` | 동 |
| **BL-33** | D-3: 통합 순차 세션 7단계 — ⏸ **현 환경 실행 불가 판정(2026-07-22)**: 사전조건 `health=NOMINAL`이 실내(GPS 없음, fault=3 상시)에서 달성 불가 + 단계 3/4 판정(fault=6/7 전이)은 BL-38 결함으로 관측 불가. **실외 GPS + BL-38 수정 선행** | 동 |
| ~~**BL-34**~~ | ✅ **완료(2026-07-23 실기)** — route 업로드 실측: FC가 MISSION_ITEM_INT(GLOBAL_RELATIVE_ALT+degE7) 업로드에 `mission upload success` ACCEPTED, INT 다운로드 방향은 BL-41 readback으로 검증(`runtime_test_session_2026-07-23.md` 3차) | `mission_item_int_frame_gap` |
| ~~**BL-35**~~ | ✅ **정량 실측 완료(2026-07-22)** — mismatch 18,237건 분석: lag 3~6프레임 집중(평균 4.36), 100ms 주기 기준 **왕복 ~440ms 체계적 파이프라인 지연**(LoRa 양방향 airtime+모듈 버퍼+호스트 처리). 데싱크/유실 아님, 링크 판정(NoAckCount)에 영향 없음 — 이벤트는 사실상 로그 노이즈. 후속(선택): 허용 윈도우/이벤트 집계 | `pi_flight_build_missing` / 실측 |
| **BL-36** | camera P2(Pi 경유 SSH)/P4(카메라 SD 녹화) 확인 | `camera_phase_verification_gap` |
| **BL-37** | 실외 GPS 확보 후 `fault_code=3`(EKF_INVALID) 해소 확인 | `uplink_lora_test_status` §8.4 |

---

| **BL-41** | 🔶 **CONFIG·route 양쪽 구현 완료(2026-07-23), Pi 실기 검증만 잔여**. **route 부분 ✅**: SDD(spec 선정의 — mavlink spec §10 재정의, runtime spec §5.1.1/§17.1 0x1914, cfs_core spec §16 2채널) → TDD(15개 red→green). `FC_MISSION_READBACK_MID(0x1914)` 신설, 트리거 3종(CONNECTED 전이/업로드 완료/MISSION_QUERY) 공통 다운로드 상태머신, lat/lon→로컬 역변환 버퍼링, 완료 시 게시→cfs_core `MissionRoute` 캐시 갱신(RAM 전용), timeout 지수 백오프(1→2→4→5s) 무한 재시도. dispatch 화이트리스트 누락 갭 발견·수정+테스트. 전체 회귀 16/16 PASS. 상세: `bl41_route_buffer_design_2026-07-23.md`. **CONFIG 부분 ✅**: TDD(테스트 44개 선작성 red → green 구현) 완료 — cfs_core(기존 파일에 ConfigVersion+ActiveConfig 6필드 추가, STATE_CORRUPT_EID 19, BL-18 부모 dir fsync 추가), mavlink_bridge(신규, Magic 0x3AB51DE0, 7필드, EID 14·15), lora_tdm(신규, Magic 0x10A7D3B0, UseV2Downlink, EID 23·24, CONFIG_CMD_MID+SET_DL_PROTO_CC 양쪽 배선). coverage 16/16 PASS(`~/cFS_clean/build-ut`), spec 정합(runtime spec §12.2 신설, cfs_core spec §14.5 본문화). 커밋 `c33357c`. **Pi 실기 검증 잔여**(CONFIG 전송→재부팅→값 유지). 상세: `bl41_config_persistence_design_2026-07-23.md`, `bl41_config_tdd_session_state_2026-07-23.md`. **route 부분(미착수)**: `cfs_core_app`의 `MissionRoute`를 **파일 지속 대신 FC를 진실원본으로 하는 RAM 전용 버퍼**로 전환 — FC(ArduPilot/PX4)가 미션을 자체 플래시에 영속하므로 Pi 파일 저장은 "사본의 사본"이 돼 이중 원본 문제만 생김. ① FC 링크 CONNECTED 전이 ② ROUTE_UPDATE 업로드 완료 후 ③ MISSION_QUERY_CC 수신 시, 3개 트리거에서 FC 재조회해 캐시 채움(무한 재시도+지수 백오프 1s→5s 상한). MID `0x1914`(가칭 FC_MISSION_READBACK_MID)로 지상국발 ROUTE_UPDATE_MID(0x190B)와 분리. 상세: `bl41_route_buffer_design_2026-07-23.md`. 부가: Pi journald 영구 저장 전환 완료(2026-07-23, 재부팅 원인 추적용) | waypoint readback 실기 검증 중 발견 |
| **BL-43** | 🔶 **구현 완료(2026-07-23, SDD→TDD), Pi 실기 검증 잔여**. UT 13개(uplink 8 + cfs_core 5) red 선작성 → green, 전체 회귀 16/16 PASS. uplink 상태파일 확장(LastResetReason/SurvivedMark/ShortBootStreak, 세션당 쓰기 3회), STATUS tlm 3필드 추가; cfs_core 상태파일 확장(재시작 카운터 3종+LastFaultCode), HK 4필드 추가, 저장 배선 3종(RestartApp 직후/RESET_COUNTER/health 전이). 기존 파일 마이그레이션: 크기 불일치→corrupt 폴백(1회 카운터 리셋 수용). 설계(원안): 영속화 6범주 처분에서 채택된 2건: **① uplink_app 부팅/오류** — LastResetReason + 재부팅 루프 감지("생존 마커" 방식: Init 시 SurvivedMark=0 저장, 120s 생존 시 1 저장, 직전 마커 0이면 ShortBootStreak++, ≥5면 HK BootLoopSuspect=1 — 시계 불필요, 세션당 쓰기 2회, 보고만/대응은 지상국). **② cfs_core_app 앱 상태** — 재시작 카운터 3종+LastFaultCode 영속화 + HK 신규 노출(BL-38 당시 "HK 노출"은 미구현이었음 확인). 제외 4범주(HW/항해/텔레메트리/회복) 사유는 `persistent_state_gap_audit_2026-07-23.md` 결정 표 | 영속화 갭 감사 후속 |
| **BL-44** | **route update 2-pass GPS 능동 보정 — 착수 여부 결정 대기** (2026-07-23 등재). 설계는 `mission_app_runtime_spec.md` §18.4.6.2.1에 확정돼 있음(2랩 상태머신, 최소자승 원 피팅, LOITER_UNLIM 호버링, 실패 시 원본 폴백). 코드 0%. 착수 전 결정: ① 담당 앱(원 피팅·상태머신을 cfs_core vs uplink), ② 착수 순서(제안: 원 피팅 수학→lap1 수집→상태머신→재업로드). 스코핑: `../route_2pass_gps_correction_scoping_2026-07-22_completed.md` | 스코핑 문서 이관됨 |
| ~~**BL-45**~~ | ✅ **완료(2026-07-24)**. `LORA_TDM_APP_Init()`에서 memset(=v1) 직후 `UseV2Downlink=1U` 명시 세팅 → LoadState가 저장값 있으면 덮어씀. 첫 부팅/상태파일 부재 시 v2(DL2)로 송신. spec 2곳 갱신(`lora_protocol_v2_spec.md` 이행순서 3, `lora_tdm_app_behavior_spec.md` §프로토콜 v2). 회귀 lora_tdm 4종 PASS. 기존 테스트는 LoadState 동작만 검증(Init 기본값 미단정)이라 무영향 | `runtime_test_session_2026-07-22` |
| ~~**BL-46**~~ | ✅ **재검토 완료 — 유지 확정(2026-07-24)**. 조사: CCSDS SDLS(키기반 MAC+AES-GCM+anti-replay), MAVLink 2 signing(HMAC-SHA256 6B+timestamp+link_id) 대조. 진단: 현 `Flags[7:6]` 자기신고는 위조 방어 아님(제3자가 RF로 명령 주입 가능), 실효는 운영자 실수 방지+Level3 request_token≠0뿐. 결정: **현 운용은 연구/취미 단계로 근거리 적대적 RF 공격자를 위협모델에 미포함(사용자 확정)** → 코드 변경 없이 유지, spec §18.11.1에 보안 범위 정직화 블록 추가(위조 방어 아님 명시 + 위협모델 변경 시 강화안=MAVLink signing-lite 경량 MAC 기록). 강화는 위협모델 변경 시 재개 | 동 |
| ~~**BL-47**~~ | ✅ **완료(2026-07-24)**. 인터랙티브 바이트 맵 `notes/lora_frame_map.html`(자립형 단일 HTML — DL2/UP2/ACK2/v1 4종, 필드 그룹 색상, 호버 판독창, SysTime·waypoint 확장 토글, 라이트/다크). `lora_protocol_v2_spec.md` §4에서 링크. Claude 아티팩트로도 게시(비공개). 근거 §4~§6 표 그대로 렌더 | 동 |
| **BL-48** | **최종 전수 실기 검증** (사용자 지시 2026-07-22): 전 CONFIG 파라미터(mavlink 7종+lora+cfs_core 6종) + RECOVERY 6종 + COUNTER 4종 + ROUTE + DIAGNOSTIC 전 명령, Pi EVS와 지상 UFB 양쪽 대조. 2026-07-23 부분 수행(CONFIG 1종/ROUTE/readback 체인) — 전수는 잔여. 부속: TEST_CASES.md RT-LORA-001 제외 문구 stale 정정(재편입), 0x1913 readback 파이프라인(DIAGNOSTIC→DL2 페이지→지상 재조립) 실기 왕복, uplinkGUI DIAGNOSTIC 버튼 | 동 / `runtime_test_session_2026-07-23` |
| ~~**BL-42**~~ | ✅ **완료(2026-07-24, SDD→TDD)**. 근본 원인 확정: `TimestampMs`는 FC `time_boot_ms`(FC 부팅 기준), `NowMs`는 Pi cFE 미션시각(Pi 기준)으로 **두 시계 기준이 다른데** 만료 판정이 직접 빼고 있어 FC 재부팅 시 오프셋 급변으로 판정이 무너지는 잠재 결함. **수정(B+A 조합)**: ① 만료 판정을 Pi 도착시각 기준으로 전환 — `StateCache_t.ArrivalMs`(수신 시 NowMs 기록) 추가, `StateExpired`가 `(NowMs-ArrivalMs)` 사용(단조 Pi 시계, 링크 두절도 정상 감지). `TimestampMs`는 순서·재부팅 감지용으로만 유지. ② FC 재부팅 감지 — `Msg->TimestampMs + TIMEBASE_SHIFT_MS(10000) < 직전 TimestampMs`면 `TIMEBASE_SHIFT_EID(20)`+`TimebaseShiftCount`(HK 노출), 메시지는 거부 안 하고 새 기준으로 갱신. spec §7 재정의. TDD: 신규 3종(ArrivalMs 만료/역방향/재부팅 감지) + 기존 만료 테스트 145곳 ArrivalMs 미러 마이그레이션. cfs_core coverage 4종 PASS(`build-ut` 동기화 후) | `cfs_core_app_command_execution_gap` §4 |

## 착수 순서 제안

```
지금 (독립, 프로토콜 무관)
   BL-01 ← 배포된 코드가 버그
   BL-06, BL-07, BL-17, BL-18, BL-19, BL-20, BL-21   (전부 소규모)

프로토콜 한 번 열 때 묶어서
   BL-03 ─┬─ BL-12 (부트카운터)
          ├─ BL-13 (모듈러 seq)
          └─ BL-14 (재전송 인덱스, 선택)
          → BL-02 자동 해소, BL-24 재검토

결정만 하면 되는 것
   BL-04(구현 vs spec정정), BL-10, BL-15 — BL-16은 결정 완료(기체 엄격화)

범위 큼 (별도 설계)
   BL-08 → BL-05, BL-11, BL-25 연쇄 해소
   BL-09
   BL-26/27/28 (교차검증 테스트로 묶기)

하드웨어 확보 후
   BL-22, BL-31~37
```

---

## 이 백로그로 대체되는 문서

아래 문서들의 체크리스트는 여기로 흡수됨. **배경·근거는 원문에 있으므로
삭제하지 말고 참조용으로 유지**할 것.

**완료 항목만 있어 `notes/`(temp 밖)로 이관됨(2026-07-23)**:
- `../command_dead_end_audit_2026-07-21_completed.md`
- `../openmct_repo_gap_audit_2026-07-21_completed.md`
- `../system_wide_reaudit_2026-07-21_completed.md`
- `../uplink_seq_feedback_redesign_2026-07-21_completed.md` ← T1~T11 상세 설계는 여기 유지
- `../inferred_decisions_selfaudit_2026-07-21_completed.md` ← AI 추론 재검토 근거
- `../pi_flight_build_missing_2026-07-16_completed.md`
- `../ambiguity_audit_by_task_2026-07-21_completed.md`
- `../ambiguity_recheck_immediate_list_2026-07-21_completed.md`

**미해결 항목이 남아 `temp/`에 유지**:
- `testcase_coverage_gap_2026-07-20.md` ← BL-30 참조 중
- `camera_phase_verification_gap.md` ← BL-36 참조 중
- `runtime_test_session_2026-07-23.md` ← BL-48(0x1913 왕복 등) 참조 중

**2026-07-23 이관됨** (잔여 항목은 BACKLOG가 계속 추적):
- `../bl41_route_buffer_design_2026-07-23_completed.md` ← 구현+실기 PASS
- `../persistent_state_gap_audit_2026-07-23_completed.md` ← 전 범주 처분 완료(채택분 BL-41/43 구현·실기 완료)
- `../bl15_stage4_5hz_cap_progress_2026-07-22_completed.md` ← 잔여 결정은 BL-15
- `../lora_downlink_5hz_cap_2026-07-21_completed.md` ← 동
- `../route_2pass_gps_correction_scoping_2026-07-22_completed.md` ← 잔여는 BL-44
- `../ground_controllable_capability_plan_2026-07-21_completed.md` ← 잔여 P2는 BL-10
- `../runtime_test_session_2026-07-22_completed.md` ← 잔여는 BL-45~48
- `../cfs_core_app_command_execution_gap_completed.md` ← 잔여는 BL-10/BL-42
- `../mission_item_int_frame_gap_completed.md` ← 잔여는 BL-34(실물 FC 검증)
- `../parser_reset_serial_reconnect_progress_2026-07-22_completed.md`
- `../bl41_config_persistence_design_2026-07-23_completed.md` ← 잔여는 BL-41 Pi 실기 검증
- `../bl41_config_tdd_session_state_2026-07-23_completed.md`
