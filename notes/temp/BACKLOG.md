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
| ~~**BL-10**~~ | ❌ **제외 확정(2026-07-24, 사용자 확인) — 짐벌 하드웨어 없음.** 재확인(2026-07-23) 당시 미확인이었던 짐벌 탑재 여부가 "없음"으로 확정됨 — `DO_MOUNT_CONTROL`(마운트 제어) 경로는 원천 불가, `DO_SET_ROI`/`CONDITION_YAW`(기체 자세만으로 응시)도 실질 효용 낮아 범위에서 제외. VIEWPOINT 클래스 관련 작업(FC 실행/스냅샷 발행 둘 다) 전면 보류 종료 — 재개 안 함 | `ground_plan` P2 / `cfs_core_app_command_execution_gap` | 제외 — 짐벌 미탑재 |
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

| ~~**BL-41**~~ | ✅ **CONFIG·route 양쪽 구현+실기 검증 완료(2026-07-24, RT-CONFIG-001/RT-ROUTE-001 PASS)**. **route 부분 ✅**: SDD(spec 선정의 — mavlink spec §10 재정의, runtime spec §5.1.1/§17.1 0x1914, cfs_core spec §16 2채널) → TDD(15개 red→green). `FC_MISSION_READBACK_MID(0x1914)` 신설, 트리거 3종(CONNECTED 전이/업로드 완료/MISSION_QUERY) 공통 다운로드 상태머신, lat/lon→로컬 역변환 버퍼링, 완료 시 게시→cfs_core `MissionRoute` 캐시 갱신(RAM 전용), timeout 지수 백오프(1→2→4→5s) 무한 재시도. dispatch 화이트리스트 누락 갭 발견·수정+테스트. 전체 회귀 16/16 PASS. 상세: `bl41_route_buffer_design_2026-07-23.md`. **CONFIG 부분 ✅**: TDD(테스트 44개 선작성 red → green 구현) 완료 — cfs_core(기존 파일에 ConfigVersion+ActiveConfig 6필드 추가, STATE_CORRUPT_EID 19, BL-18 부모 dir fsync 추가), mavlink_bridge(신규, Magic 0x3AB51DE0, 7필드, EID 14·15), lora_tdm(신규, Magic 0x10A7D3B0, UseV2Downlink, EID 23·24, CONFIG_CMD_MID+SET_DL_PROTO_CC 양쪽 배선). coverage 16/16 PASS(`~/cFS_clean/build-ut`), spec 정합(runtime spec §12.2 신설, cfs_core spec §14.5 본문화). 커밋 `c33357c`. **Pi 실기 검증 잔여**(CONFIG 전송→재부팅→값 유지). 상세: `bl41_config_persistence_design_2026-07-23.md`, `bl41_config_tdd_session_state_2026-07-23.md`. **route 부분(미착수)**: `cfs_core_app`의 `MissionRoute`를 **파일 지속 대신 FC를 진실원본으로 하는 RAM 전용 버퍼**로 전환 — FC(ArduPilot/PX4)가 미션을 자체 플래시에 영속하므로 Pi 파일 저장은 "사본의 사본"이 돼 이중 원본 문제만 생김. ① FC 링크 CONNECTED 전이 ② ROUTE_UPDATE 업로드 완료 후 ③ MISSION_QUERY_CC 수신 시, 3개 트리거에서 FC 재조회해 캐시 채움(무한 재시도+지수 백오프 1s→5s 상한). MID `0x1914`(가칭 FC_MISSION_READBACK_MID)로 지상국발 ROUTE_UPDATE_MID(0x190B)와 분리. 상세: `bl41_route_buffer_design_2026-07-23.md`. 부가: Pi journald 영구 저장 전환 완료(2026-07-23, 재부팅 원인 추적용) | waypoint readback 실기 검증 중 발견 |
| ~~**BL-43**~~ | ✅ **구현+실기 검증 완료(2026-07-24, SDD→TDD, RT-BOOTLOOP-001/002 PASS)**. UT 13개(uplink 8 + cfs_core 5) red 선작성 → green, 전체 회귀 16/16 PASS. uplink 상태파일 확장(LastResetReason/SurvivedMark/ShortBootStreak, 세션당 쓰기 3회), STATUS tlm 3필드 추가; cfs_core 상태파일 확장(재시작 카운터 3종+LastFaultCode), HK 4필드 추가, 저장 배선 3종(RestartApp 직후/RESET_COUNTER/health 전이). 기존 파일 마이그레이션: 크기 불일치→corrupt 폴백(1회 카운터 리셋 수용). 설계(원안): 영속화 6범주 처분에서 채택된 2건: **① uplink_app 부팅/오류** — LastResetReason + 재부팅 루프 감지("생존 마커" 방식: Init 시 SurvivedMark=0 저장, 120s 생존 시 1 저장, 직전 마커 0이면 ShortBootStreak++, ≥5면 HK BootLoopSuspect=1 — 시계 불필요, 세션당 쓰기 2회, 보고만/대응은 지상국). **② cfs_core_app 앱 상태** — 재시작 카운터 3종+LastFaultCode 영속화 + HK 신규 노출(BL-38 당시 "HK 노출"은 미구현이었음 확인). 제외 4범주(HW/항해/텔레메트리/회복) 사유는 `persistent_state_gap_audit_2026-07-23.md` 결정 표 | 영속화 갭 감사 후속 |
| ~~**BL-44**~~ | ✅ **base 명령(uplink+mavlink_bridge) 구현 완료(2026-07-24), 실외 실기 검증만 잔여**. 결정: 원 피팅·상태머신·1500샘플 버퍼를 온보드에서 **지상국(openMCT/Python, 별개 레포)으로 이관**(사용자 결정). 근거: 최소자승/상태머신은 C 비행SW에서 크고 테스트난이+안전민감, 지상 Python은 수 줄+사람 검토 가능, route/텔레메트리 경로 이미 존재. spec §18.4.6.2.1 개정 완료(책임 분담표, 링크 의존 폴백). 원 피팅·상태머신은 지상 레포 담당. **설계 재정의(2026-07-24, 사용자): 3-모드 base + 보정경로=평범한 waypoint**. LOITER를 미션에 심거나 2-pass 플래그를 두지 않는다 — 지상이 보정 계산 후 나온 점(a,b,c)을 **그냥 REPLACE waypoint로 재업로드**하고, 호버/재개/착륙은 **명시 비행모드 명령**으로 오케스트레이션. 앞서 착수했던 ⓐ(reserved 2-pass 플래그 배선)는 이 모델에서 불필요라 **되돌림(revert, 미커밋)**.
**base 명령 설계 확정(2026-07-24) — spec §18.4.6.8 신설**: class code 8(`FLIGHT_MODE`), payload={flight_mode(HOVER=0/WAYPOINT=1/LAND=2), waypoint_start_index, request_token}. PX4 매핑: HOVER→AUTO(4)/LOITER(3), WAYPOINT→AUTO(4)/MISSION(4)+`MISSION_SET_CURRENT`, LAND→AUTO(4)/LAND(6) — 기존 `COMMAND_LONG`/`DO_SET_MODE`(176) 송신 인프라 재사용. auth Level 3(request_token≠0). **health gate**: HOVER·LAND=위험축소 명령이라 헬스 상태 무관 항상 허용, WAYPOINT=위험증가(새 경로 신뢰) 명령이라 §18.10.1 게이트 정상 적용(대화로 확정, LAND만 예외였던 1차안에서 HOVER도 포함해 재확정). 라우팅은 counter mgmt(§18.4.6.7)와 동일 패턴(cfs_core 경유 없이 uplink→mavlink_bridge CMD_MID 신규 FcnCode). 2-pass는 지상이 이 base 명령 조합(waypoint→끝감지→HOVER→보정 재업로드→waypoint→…→LAND).
**구현(2026-07-24, TDD)** — **uplink_app 슬라이스 ✅ 완료**: `UPLINK_APP_CLASS_FLIGHT_MODE=8` 신설, `ParseFlightModePayload`(flight_mode 0~2 검증, waypoint_start_index는 WAYPOINT 외 0 강제)+`ForwardFlightModeCommand`(mavlink_bridge CMD_MID 0x18A0에 신규 FcnCode `SET_FLIGHT_MODE_CC=5` 얹어 직접 전송, P1-a 패턴) 구현. `GetClassRequiredLevel`에 Level 3 추가. 헬스게이트: DEGRADED에 FLIGHT_MODE(WAYPOINT만) 차단 추가 + HOVER/LAND는 모든 상태(DEGRADED/RECOVERY/FAILED)에서 예외 허용 로직 신설. `ValidateProxyCommand`/`ResolveRouteTarget`(신규 `UPLINK_APP_ROUTE_FLIGHT_MODE=4`)에도 배선. 신규 테스트 10종(파싱 2/전달 1 in utils + 디스패치 7종: accept/parse실패/forward실패/auth거부/HOVER예외/LAND예외/WAYPOINT차단×2) 전부 green, uplink coverage 4종 회귀 없음.
**mavlink_bridge_app 슬라이스 ✅ 완료(2026-07-24, TDD)**: spec §18.4.6.8.1대로 구현. `MAVLINK_BRIDGE_APP_SetFlightModeCmd_t`(CommandHeader+SourceSequence+FlightMode+WaypointStartIndex) 신설, uplink_app쪽 `UPLINK_APP_FlightModeCtrlCmd_t`에도 누락됐던 `SourceSequence` 추가(양쪽 동시 정정). `MAVLINK_BRIDGE_APP_ProcessSetFlightModeCmd()`가 PX4 custom_mode 인코딩(`(AUTO<<16)|(sub<<24)`)으로 `COMMAND_LONG`/`DO_SET_MODE`(176) 전송(HOVER→LOITER/3, WAYPOINT→MISSION/4, LAND→LAND/6), WAYPOINT는 추가로 `MISSION_SET_CURRENT`(msg 41, CRC_EXTRA=28)까지 전송. 신규 FcnCode `SET_FLIGHT_MODE_CC=5` 디스패치 배선, `EXEC_RESULT_MID` 회신(`PublishExecResult`, class=8) 연결. 신규 테스트 5종(HOVER/WAYPOINT/LAND wire 인코딩 socketpair 캡처+디코딩, invalid mode, send 실패) 전부 green, mavlink_bridge coverage 4종 회귀 없음. **BL-44 uplink_app+mavlink_bridge_app 양쪽 슬라이스 모두 완료**. **✅ 실기 검증 완료(2026-07-24, Pi+FC보드 연결, 프로펠러 미장착)**: RT-FLIGHTMODE-001~004(TEST_CASES.md) 전부 PASS — HOVER/LAND는 실내 DEGRADED(fault=3) 상태에서도 예외 허용돼 `mavlink_bridge: flight mode set` + `exec result generic=0 OK`까지 전체 체인 확인, WAYPOINT는 동일 상태에서 정확히 차단(`blocked by health state=1 class=8`), auth Level 3 미달도 정상 거부. 잔여는 RT-FLIGHTMODE-005(WAYPOINT의 실제 FC 모드 전환 확인, NOMINAL 상태 필요 — 실외 GPS 대기)뿐. 스코핑: `../route_2pass_gps_correction_scoping_2026-07-22_completed.md` | 지상 연산 + 3-모드 + PX4 매핑 확정, 실기 게이트 검증 완료 |
| ~~**BL-45**~~ | ✅ **완료(2026-07-24)**. `LORA_TDM_APP_Init()`에서 memset(=v1) 직후 `UseV2Downlink=1U` 명시 세팅 → LoadState가 저장값 있으면 덮어씀. 첫 부팅/상태파일 부재 시 v2(DL2)로 송신. spec 2곳 갱신(`lora_protocol_v2_spec.md` 이행순서 3, `lora_tdm_app_behavior_spec.md` §프로토콜 v2). 회귀 lora_tdm 4종 PASS. 기존 테스트는 LoadState 동작만 검증(Init 기본값 미단정)이라 무영향. **실기 확인(2026-07-24, RT-DL2-DEFAULT-001 PASS, 부수)**: 별도 CONFIG 미전송 상태에서 `ACK2` 이벤트 관측(v2 전용 명칭) — 기본 v2 송신 확인 | `runtime_test_session_2026-07-22` |
| ~~**BL-46**~~ | ✅ **재검토 완료 — 유지 확정(2026-07-24)**. 조사: CCSDS SDLS(키기반 MAC+AES-GCM+anti-replay), MAVLink 2 signing(HMAC-SHA256 6B+timestamp+link_id) 대조. 진단: 현 `Flags[7:6]` 자기신고는 위조 방어 아님(제3자가 RF로 명령 주입 가능), 실효는 운영자 실수 방지+Level3 request_token≠0뿐. 결정: **현 운용은 연구/취미 단계로 근거리 적대적 RF 공격자를 위협모델에 미포함(사용자 확정)** → 코드 변경 없이 유지, spec §18.11.1에 보안 범위 정직화 블록 추가(위조 방어 아님 명시 + 위협모델 변경 시 강화안=MAVLink signing-lite 경량 MAC 기록). 강화는 위협모델 변경 시 재개 | 동 |
| ~~**BL-47**~~ | ✅ **완료(2026-07-24)**. 인터랙티브 바이트 맵 `notes/lora_frame_map.html`(자립형 단일 HTML — DL2/UP2/ACK2/v1 4종, 필드 그룹 색상, 호버 판독창, SysTime·waypoint 확장 토글, 라이트/다크). `lora_protocol_v2_spec.md` §4에서 링크. Claude 아티팩트로도 게시(비공개). 근거 §4~§6 표 그대로 렌더 | 동 |
| **BL-48** | **최종 전수 실기 검증** (사용자 지시 2026-07-22): 전 CONFIG 파라미터(mavlink 7종+lora+cfs_core 6종) + RECOVERY 6종 + COUNTER 4종 + ROUTE + DIAGNOSTIC 전 명령, Pi EVS와 지상 UFB 양쪽 대조. 2026-07-23 부분 수행(CONFIG 1종/ROUTE/readback 체인) — 전수는 잔여. 부속: TEST_CASES.md RT-LORA-001 제외 문구 stale 정정(재편입). **2026-07-24 추가**: 단위테스트만 있고 RT 케이스 미등재였던 BL-41/42/43/45를 `TEST_CASES.md`에 신규 등재(RT-CONFIG-001~003, RT-ROUTE-001~004, RT-TIMEBASE-001~002, RT-BOOTLOOP-001~003, RT-DL2-DEFAULT-001~002, 전부 ⬜ 미실행) — 전수 검증 시 이 목록도 포함. **✅ 2026-07-24 완료**: `uplinkGUI DIAGNOSTIC 버튼`, `0x1913 readback 파이프라인 실기 왕복` — openMCT 레포(`fc_serial_ws_server.py`/`plugin.js`, 커밋 `2d455a0`)에 Diagnostic 섹션 신설 + `RouteReadbackAssembler`(기존엔 콘솔 로그만) 상태를 `GET /api/uplink/route_readback`으로 노출 + GUI 자동 폴링 표시까지 구현. 실기 왕복 확인: route_readback 요청→cfs_core 발행→DL2 waypoint 페이지→지상 재조립→GUI `status=complete, waypoints=[업로드값과 일치]`. TDD 신규 10종(FlightMode 7+RouteReadback 3) green, openMCT 전체 38/38 PASS | 동 / `runtime_test_session_2026-07-23` |
| ~~**BL-49**~~ | ✅ **PX4 실기 재검증 완료(2026-07-24)**. `tools/uplink_route_update_sender.py route-good-no-gps`로 실제 PX4에 REPLACE 2-waypoint 업로드 → **3가지 ArduPilot 유래 quirk 전부 PX4에서도 유효 확인**: ① `MISSION_CLEAR_ALL` 선행 시퀀스로 `mission upload success wp_count=2` 정상 수락 ② `MAV_FRAME_LOCAL_NED`→`GLOBAL_RELATIVE_ALT` 변환 후 readback 왕복 값 일치(`FC mission readback applied wp_count=2`) — frame 변환이 데이터 왜곡 없이 정상 동작 ③ `sysid=255` 요구도 FC 응답 수신으로 유효성 확인. 전체 체인(업로드→success→readback started→published→cfs_core applied, `route_updates` 1→2→3 증가) 실측 확인. HEARTBEAT autopilot 이중 인식은 기존 결론(정상 로직, 유지)대로. 코드 변경 불필요 — 현 구현이 PX4에서도 이미 올바르게 동작. 카메라 `ardupilot_msp_osd.param`은 BL-36에서 별도 확인 | PX4 실기 재검증 완료 |
| **BL-50** | 🔵 **관찰 기록 — LORA_TDM_PIPE Msg Limit Err (2026-07-24, 낮은 우선순위)**. FLIGHT_MODE/DIAGNOSTIC 연속 실기 테스트 중 `CFE_SB: Msg Limit Err MsgId 0x190a pipe LORA_TDM_PIPE sender UPLINK_APP` 1회 관측. 파이프 depth=50(`lora_tdm_app.c:446`)로 평상시 운용 페이스엔 충분 — 짧은 시간에 명령을 연속 여러 번 쏜 테스트 부하 아티팩트로 판단, 오늘 변경사항과 무관(오늘은 publish 빈도/파이프 설정 미변경). 실제 운용(정상 명령 빈도)에서 재현되면 재조사 필요, 아니면 조치 불요 | 실기 테스트 중 관찰 |
| ~~**BL-51**~~ | ✅ **완료(2026-07-24) — LoadState/EVS_Register 순서 버그 수정**. RT-CONFIG-003(3앱 동시 영속화) 실기 검증 중 발견: `mavlink_bridge_app`/`lora_tdm_app` 둘 다 `Init()`에서 `LoadState()`를 `CFE_EVS_Register()`보다 먼저 호출 — LoadState 내부 EVS 이벤트(복원/손상 로그)가 등록 전이라 `EVS_NotRegistered: App ... not registered`로 조용히 유실되고 있었음(`cfs_core_app`/`uplink_app`은 이미 순서 올바름). 데이터 자체는 정상 복원 중이었으나(v1 다운링크 프로토콜 유지 등 실측 확인) 확인 로그 부재로 디버깅에 지장. Register를 LoadState보다 먼저로 이동. 회귀 테스트 4종×2앱 PASS(호출횟수 기반이라 순서 무관). Pi 재배포 후 재검증: 4개 앱 전부 `restored` 로그 정상 출력, `EVS_NotRegistered` 미발생 확인 | RT-CONFIG-003 실기 검증 중 발견 |
| **BL-52** | 🔵 **재검토 필요 — STOP_APP 후 mavlink_bridge_app 자동 재시작 미관측(2026-07-24)**. RT-TIMEBASE-002 검증 중 `CFE_ES_STOP_APP_CC`로 mavlink_bridge_app 정지 → stale 감지(health fault=1)는 3초 내 정상 확인됐으나, 이후 ~50초간 BL-38 자동 재시작(`bridge restart attempt=`)이 발생하지 않음(`CFE_ES_DeleteApp`/`ExitApp` 로그로 볼 때 완전 삭제 → 이후 `GetAppIDByName` 실패 가능성). **BL-40 기존 기록과 상충**: 동일 STOP_APP 메커니즘으로 uplink_app/lora_tdm_app은 자동 재시작 실측 PASS 기록됨(`runtime_test_session_2026-07-23`). 대기 시간 부족이었을 가능성도 있어 재검증 필요 — mavlink_bridge STOP_APP 후 재시작 인터벌(5s)을 확실히 넘겨(15초 이상) 재확인. 수동 `systemctl restart cfs.service`로 즉시 복구함(시스템 정상 상태로 되돌림) | RT-TIMEBASE-002 실기 검증 중 발견 |
| **BL-53** | 🔵 **RT-ROUTE-004 재현 방법 재검토 필요(2026-07-24)**. Pi↔FC 시리얼 케이블 물리적 분리로 "FC 무응답→백오프" 재현 시도 → 의도와 다르게 `OpenSerial` 실패로 BL-38 자동 앱 재시작 루프만 유발(`MissionReadbackBackoffMs` 백오프 로직과 무관한 별개 코드 경로). 케이블 분리는 "링크 완전 단절"이지 "FC 연결 유지하며 특정 명령만 무응답"이 아니라서 부적합 — 실제 백오프(1→2→4→5s) 검증에는 FC가 heartbeat는 계속 보내되 `MISSION_REQUEST_INT`에만 응답하지 않는 상황이 필요. FC측 스크립트 개입(예: 특정 메시지만 필터링하는 프록시)이나 다른 재현 방법 검토 필요 | RT-ROUTE-004 실기 시도 중 발견 |
| **BL-54** | 🔵 **관찰 기록 — health-gate 차단 명령의 "선차단 후force" 2클릭 흐름(2026-07-24)**. RT-FLIGHTMODE 실기 GUI 테스트 중 WAYPOINT가 DEGRADED에서 막힘(UFB=3) → force 체크 후 재전송해야 통과. 원인: GUI가 사전에 현재 health_state를 표시하지 않아 유저가 막힐지 미리 알 수 없음(같은 패턴이 CONFIG 테스트에서도 나타났었음). 버그 아님(force 자체는 정상 동작, `_handle_flight_mode`에 force 파라미터 배선 완료 — 이 이슈 발견 계기로 같이 추가됨) — 개선하려면 GUI에 health_state 상시 표시 UI 추가 필요. 낮은 우선순위, 조치 보류 | RT-FLIGHTMODE-006 실기 GUI 테스트 중 발견(openMCT) |
| **BL-55** | 🟢 **설계 확정(2026-07-25) — 진짜 원인은 GUI 버그가 아니라 프로토콜 레벨 UFB_OK 모호성**. RT-ROUTE 실기 GUI 테스트 중 2wp REPLACE(seq=63)가 세그먼트거리 위반(BL-56에서 이 제약 자체는 폐지됨)으로 REJECT_ROUTE 4회 거부됐는데 GUI엔 `[✅ UFB=0]`으로 표시된 사례. 원래 가설("onUFBReceived가 다른 class UFB를 잘못 매칭")은 **틀림** — 실제로는 `lora_tdm_app_behavior_spec.md` §10에 이미 문서화돼 있던 기존 한계: `UFB_OK(0x00)`가 "성공"과 "보고할 pending 결과 없음(default/idle)"을 구분 못 함(`ROUTED` 시 명시적으로 세팅하는 코드가 없어 실패분기 안 탄 이전값이 그대로 유지) + 200ms 다운링크 주기 내 비동기 처리결과 도착 레이스. **해결**: `UFB_APPLIED(0x0C)` 신설 — `LastCommandResult==ROUTED(3)` 감지 시 세팅, ROUTE뿐 아니라 UFB 메커니즘을 공유하는 전체 명령 클래스에 공통 적용, 기존 코드와 동일한 감지방식이라 신규 메커니즘 불필요(`lora_tdm_app_behavior_spec.md`에 표+한계 정정 반영 완료). **잔존 한계(감수)**: `ROUTED`="라우팅 성공"이지 "FC 실제 반영 성공" 보장은 아님 — 확실한 반영 확인은 `route_readback`(BL-41) 별도 확인 필요. **다음**: `lora_tdm_app`(UFB_APPLIED 세팅 로직) + GUI(`onUFBReceived`에 `ufb===0x0C` 분기 추가) 양쪽 구현 필요 | RT-ROUTE 실기 GUI 테스트 중 발견(openMCT) |
| **BL-56** | ✅ **설계+구현+테스트 완료(2026-07-25)** — uplink_app 350/350, mavlink_bridge_app 422/422 통과. Pi 실기 재검증만 남음(아래 "재검증 대기" 참조). route op을 REPLACE/APPEND/DELETE 3종에서 **REPLACE/ADD(index없음,끝추가)/DELETE(index)/MODIFY(index)** 4종으로 재정의, **ARMED 차단 전면 폐지(2026-07-25 최종 확정)** — REPLACE도 `current=1` 플래그로 인덱스리셋 문제가 해결되므로 4종 전부 ARMED 허용, `mavlink_bridge_app_utils.c:507`의 `IsArmed` 체크 완전 제거. REPLACE의 "실수 파급 범위 큼" 위험은 기체측 가드 대신 **GUI 재확인 다이얼로그**(전송 전 항상 표시)로 완화 — 이 발견은 2-pass 보정 재업로드(호버링=ARMED 상태에서 REPLACE 필요)가 기존 ARMED 차단과 정면 모순됨을 확인하며 나옴. 세그먼트거리 2.0m 제약 전면 폐지(flyable area/고도 범위만 검증, BL-55 원인도 같이 해소). `ROUTE_WAYPOINT_t`를 X/Y/Z만 있던 구조에서 `CmdType+Param1~4+UseGlobal+LatE7/LonE7+X/Y/Z`(38바이트)로 확장 — LOITER 등 명령 파라미터 표현 가능, `UseGlobal=1`이면 절대좌표(LatE7/LonE7) 그대로 사용(로컬 변환 스킵, BL-57용). 비행 중 재개 지점 문제는 별도 `MISSION_SET_CURRENT` 없이 PX4가 업로드 트랜잭션 내 `current=1` 플래그를 그대로 채택하는 걸 활용(`ActiveResumeIndex` 관리, §18.4.6.8 base 배선 문단도 정정 완료). DELETE(index==ActiveResumeIndex)는 거부 정책. **모호점 확정(2026-07-25)**: ADD는 끝 추가만 지원(중간 삽입 없음, 필요 시 MODIFY/DELETE 조합), MODIFY(index)는 좌표뿐 아니라 CmdType/Param까지 해당 인덱스 전체를 통째 교체. **파급 효과**: waypoint 구조체가 38바이트로 커져 LoRa 프레임(196B) 1개당 5개까지만 담김(기존 16개서 축소) — 16개 채우려면 ADD를 여러 프레임(예 4×4개)로 나눠 전송, 세션/누적대기 없이 매 ADD가 즉시 캐시반영+즉시 PX4 재업로드(HOVER는 배열 슬롯 안 씀, §18.4.6.2.1 "알려진 제약" 갱신). **추가 모호점 확정(2026-07-25 2차)**: UseGlobal=1 waypoint의 flyable area 검증은 2-pass와 동일 계획경로 기준점 재사용(±50m 여유 커서 기준점 차이 무의미), CmdType 필드는 검증 없음(지상국 신뢰), MODIFY(index==ActiveResumeIndex)로 인한 급각도 진로변경도 허용(사용자 책임). **신규 구현 필요 확인**: `ActiveResumeIndex` 갱신용 `MISSION_CURRENT`(MAVLink msg #42) 파싱이 현재 코드에 전혀 없음 — 기존 기능 재사용 아니라 새로 추가해야 함. **3차 확정(2026-07-25) — 항상 절대좌표로 단순화**: `UseGlobal`/로컬 `X`/`Y` 필드를 아예 제거, waypoint는 항상 `CmdType+Param1~4+LatE7+LonE7+Z`(29바이트, 38→29로 축소, LoRa 프레임당 5→6개)만 사용 — `mavlink_bridge_app`의 `RefLatE7`/`RefLonE7` 기반 로컬↔전역 변환 로직·추적 자체가 업로드/다운로드(readback) 양쪽에서 전부 불필요해져 제거 대상. **flyable area(±50m) 기체측 검증도 폐지** — 로컬 변환 기준점을 새로 안 두고, 대신 GUI 재확인 다이얼로그로 사용자 책임 위임(고도 2~8m 검증만 기체측 유지). **4차 확정(2026-07-25) — 잘못된 개념 정정**: `mavlink_bridge_app_utils.c:715` 주석으로 확인 — **별도 "landing route 캐시" 자체가 존재한 적 없음**("RouteType=1 MISSION 고정, FC에 landing 세그먼트 개념 없음"), GUI의 "mission"/"landing" 라디오 버튼은 구 REPLACE/APPEND op의 잘못된 이름표였을 뿐 — 새 GUI 구현 시 이 라디오 버튼은 완전 폐기, REPLACE/ADD/DELETE/MODIFY op 선택 UI로 교체. route_version은 v1→**v2로 증가**(payload 포맷 완전히 바뀜), API는 기존 관례대로 **단일 엔드포인트**(`/api/uplink/route`) + op 필드 유지(REST 분리 안 함). **route_version v1/v2 API 구조/landing 오개념 정리는 §18.4.6.2·§18.4.6.2.1에 반영 완료(commit c1764c5, 이후)**. **다음**: 회귀 테스트 재작성(`coveragetest_mavlink_bridge_app_utils.c:70~`) → uplink_app/mavlink_bridge_app(RefLatE7 관련 코드 삭제 포함)/GUI 3단 구현(+MISSION_CURRENT 파싱 신규, mission/landing 라디오 폐기) → Pi 재검증 | 사용자 요청(openMCT GUI 워크스루 중 경로수정 흐름 논의) |
| **BL-57** | ✅ **설계+구현+테스트 완료(2026-07-25)** — Leaflet 지도 플러그인(`mapRouteGUI`) 신규 추가, openMCT 93/93 통과. 타일 실제 확보(수동, OSM 정책 준수)만 남음. openMCT GUI에 Leaflet(2D 평면, CesiumJS 3D는 과함) 지도 플러그인 추가(기존 `my_openmct_app/src/plugins/uplinkGUI/plugin.js` 패턴) — 지도 클릭으로 웨이포인트 입력. **타일**: 사전에 활동구역 반경 지역 타일 다운로드해 정적 파일로 앱 내 보관, 완전 오프라인 동작(별도 타일 서버 불필요). **좌표 변환 문제 해소**: `RefLatE7`/`RefLonE7`은 고정 홈이 아니라 매 `GLOBAL_POSITION_INT`마다 갱신되는 기체 현재위치라, 지도 클릭 lat/lon을 지상에서 로컬 X/Y로 미리 변환해 보내면 업로드 시점 기준점 드리프트로 오차 발생 — 대신 BL-56에서 **항상 절대좌표(`LatE7`/`LonE7`)만 쓰는 것으로 3차 확정**(로컬 모드 자체를 없앰)해 기체측 재변환 로직 통째로 제거. **op 매핑**: 지도 클릭=ADD, 기존 점 드래그/우클릭 삭제=MODIFY(index)/DELETE(index). QGroundControl 등 기존 GCS 연동안은 기각(3안 중 Leaflet+오프라인타일 확정). **타일 출처(2026-07-25 확정)**: OSM 공식 타일 다운로드 정책 준수해 비행구역 반경 소규모 수동 저장(대량 자동 스크래핑 금지 정책 위반 안 하는 범위). **GUI 경로 생성 방식(2026-07-25 추가 확정)**: 사람이 점을 하나하나 좌표로 입력하는 방식은 폐기 — 원형/그리드 등 파라미터(중심점 지도 클릭+반경+개수 등)만 입력하면 **GUI가 직접 경로를 계산**해 지도에 실시간 표시, 확인 후 ADD로 전송. 계산 결과는 openMCT 레포 기존 `telemetry_logs/`(현재 미추적 런타임 로그 폴더) 하위에 세분화한 서브폴더(예: `telemetry_logs/routes/`)에 CSV로 저장(감사/재사용용, 별도 외부 도구 불필요). **3차 확정(2026-07-25)**: 자유 클릭(점 하나=웨이포인트 하나)과 파라미터 자동생성(원형/그리드 등) 둘 다 지원 — 둘 다 결국 "웨이포인트 목록(최대 16개)을 ADD로 전송"하는 동일 메커니즘이고, 자동생성은 16개를 사람이 일일이 타이핑하기 번거로워서 만드는 편의 기능일 뿐 프로토콜상 특별 취급 없음. 경로 마지막 지점은 "LANDING POINT"로 별도 표시해 §18.4.6.8 FLIGHT_MODE(LAND)와 연결. 지도 플러그인은 **새 별도 파일/플러그인으로 분리**(`my_openmct_app/src/plugins/` 하위 신규, uplinkGUI와 독립) 확정 — Leaflet 의존성 격리 목적, route 명령 전송은 기존 API 재사용. **구현 참고(2026-07-25)**: `my_openmct_app`에 정적 자산(타일 등) 서빙 관례가 아직 없음(`public/`·`static/` 디렉터리 부재, `src/`만 존재) — 구현 시 빌드 설정(webpack 등) 확인해 새로 만들어야 함, 별도 설계 결정 아니라 구현 중 확인 사항. CSV 스키마(제안, 구현 중 조정 가능): `timestamp,seq,cmd_type,param1,param2,param3,param4,lat_e7,lon_e7,z_m` 컬럼. **다음**: 지도 플러그인 UI 구현(경로 생성 로직 포함) → BL-56 GUI 배선과 통합 — 구현 진행하며 세부 설계가 더 나올 수 있음(예: 파라미터 UI 형태, 그리드 생성 알고리즘 등), 그때그때 이어서 설계 확정 | 사용자 요청(openMCT Route Update GUI 사용성 논의) |
| **BL-58** | 🟡 **[openMCT 레포 종합, ①의 전제 자체가 미확정으로 재조정 2026-07-25] 지상국(ground station) 쪽 미착수 작업 총정리**. BL-44 완료 시 "원 피팅·상태머신은 지상국(openMCT/Python)으로 이관"이라 결정만 해두고, **openMCT 레포에 2-pass GPS 능동보정 로직 자체가 코드로 하나도 없음**(circle_fit/correction grep 0건). **① 2-pass 오케스트레이션 — ⚠️ 핵심 전제 미해결(2026-07-25 발견)**: §18.4.6.2.1의 원 피팅·보정 로직 전체가 "이전 초안이었지 사용자가 직접 결정한 게 아니었다"고 확인됨 — 특히 **보정 대상 오차가 (A) GPS 측정 노이즈(무작위, 다표본 평균으로 상쇄됨)인지 (B) 기체 실제 궤적의 지속적/체계적 편향(바람·제어루프 추종오차·GPS 항법 바이어스 등, 진짜 보정 대상)인지 구분이 안 된 채 설계됨**. 지상국이 갖는 다운링크 위치값만으로는 이 둘을 구분할 수 없어, 실제로 (A)인데 (B)로 오인해 보정을 적용하면 오히려 멀쩡한 비행을 왜곡시킬 위험 — **2-pass 기능을 애초에 필요하다고 느낀 실제 계기/증상**부터 다시 논의해 이 전제를 확정해야 알고리즘(원피팅 방식 Kasa/Taubin, 피팅품질 검증 여부 등)도 결정 가능. 나머지 세부(좌표계=계획경로 고정원점, 전송=UseGlobal, 검증실패=clamp, 상태머신=백엔드, 진행중 수동개입=재확인)는 정해뒀지만 **이 근본 전제가 안 풀리면 전체가 재검토 대상**. 원피팅·편차계산은 **로컬 접평면 미터 좌표**로(위경도 직접 피팅 시 축척 왜곡), 원점은 **라이브 GPS 스냅샷이 아니라 사전 계획된 원형 경로(lap1 계획 waypoint) 자체의 값**(계획 waypoint[0] 또는 계획 원피팅 중심)을 고정 원점으로 사용(2026-07-25 정정 — 라이브 GPS 스냅샷은 그 순간 노이즈/fix quality에 좌표계 전체가 좌우되는 위험 때문에 기각, 계획 경로는 설계 시점 확정값이라 안전). 드리프트하는 `RefLatE7` 사용 금지는 유지. 보정 결과 전송 시 로컬좌표를 그대로 REPLACE하면 업로드시점 기준점과 lap1 기준점이 달라 오차 발생 → **지상국이 lap1 고정원점으로 직접 절대좌표 변환 후 전송**(BL-56에서 항상 절대좌표만 쓰는 것으로 확정돼 기체측 재변환 자체가 없어짐). flyable area 기체측 검증 자체가 폐지됐으므로(BL-56), 보정치가 크게 벗어나도 기체가 거부하지 않음 — **지상국이 스스로 안전 경계 내로 clamp해서 전송**(2026-07-25 확정). **상태머신은 브라우저 GUI가 아니라 백엔드(`fc_serial_ws_server.py`) 보관**(탭 새로고침에도 진행상황 유지), 진행 중 수동 route 명령은 차단 대신 재확인 다이얼로그로 완화(2026-07-25 확정). ② **BL-55**: ROUTE 세그먼트거리 위반 GUI UFB 오표시 — BL-56에서 세그먼트거리 제약 자체를 폐지해 **원인 해소**(재현 조건 자체가 없어짐, 별도 코드 수정 불요 가능성 높음 — 구현 시 재확인만 필요). ③ **BL-56 GUI 절반**: route op 4종(REPLACE/ADD/DELETE/MODIFY) 반영해 `plugin.js` Route Update 패널 재작업(기체측은 cfs-telemetry-app 별도). ④ **BL-57**: 지도 입력(위 참조). 순서: ③→④→①, ②는 ③ 구현 시 자연히 해소. **①의 부분 진전(2026-07-25, 여전히 보류)**: "한 번의 비행 기회 안에서 다 고려해서 수정해야 한다"는 운용 제약이 확인됨 — 이는 보정 대상이 여러 날에 걸친 장기 편향이 아니라 **그 비행 세션(랩 1~2분) 동안 유지되는 편향**(바람·그 순간 GPS 위성기하 등)이면 됨을 의미하고, lap1 다표본 최소자승 원피팅이 정확히 이런 "세션 내 지속 편향은 유지, 무작위 노이즈는 상쇄"를 구분하는 도구라 원래 초안 설계가 이 시나리오엔 맞는 접근이었다는 결론까지는 도달함. 다만 **랩 도중 일시적 돌풍처럼 "세션 내내도 아니고 무작위도 아닌 국소 교란"**이 있으면 원피팅 잔차(적합도)가 나빠지는데, 이 잔차를 이상치 판정에 쓸지(레퍼런스: MAD 기반 임계값 — 중앙값 잔차 3배 초과 시 이상 판정, arXiv 2508.03720/PMC5191017 등 표준 기법 확인됨) — 이 세부는 아직 미결(사용자가 "일단 이 부분 제외하고 개발"로 보류 지시, 2026-07-25). **다음**: ③④(GUI route op 재작업, 지도입력)는 설계 종료 → TDD/구현 착수 가능. **①(2-pass)은 잔차/적합도 임계값 정책만 남기고 나머지는 상당히 진전됐으나, 사용자 지시로 이 세부 결정 없이 ③④부터 먼저 구현 진행** — "2-pass가 왜 필요하다고 느꼈는지" 논의는 사실상 해소(위 참조), 남은 건 잔차 임계값 채택 여부뿐 | 사용자 요청(openMCT 잔여작업 총정리) |
| **BL-59** | 🔵 **FC 수신값 finite(NaN/Inf) 검증 부재 — 설계 선택 미결정(2026-07-13 우선순위 문서 발굴)**. `mavlink_bridge_app_utils.c`가 FC로부터 받는 attitude/position/GPS 값에 CRC만 검증하고 `isfinite()` 등 유한성 검증이 없음 — NaN/Inf가 그대로 TLM 발행/하류(cfs_core_app 헬스 판단)로 전파될 위험. **설계 확정(2026-07-25) — C안**: `mavlink_bridge_app`은 지금처럼 그대로 TLM 발행(수정 없음). `cfs_core_app`의 헬스 판단 로직(`cfs_core_app_utils.c:410~416`, `GpsState`/`EkfState`/`LocalState`/`AttitudeState` 각각의 `!Valid || Stale` 체크)에 **`isfinite()` 검증을 추가**해, 해당 값이 NaN/Inf면 `Valid=true`여도 "Fresh하지 않음/무효"로 취급해 헬스 판단에서 걸러낸다. 근거: 확인 결과 `.Valid`는 값의 유한성과 무관하게 `mavlink_bridge_app`이 무조건 `true`로 세팅하고 있어(원 갭), NaN이 그대로 헬스 판단 체인을 통과하는 게 실제로 미결 상태였음 — 재시작/복원력(BL-38 등)은 fault 발생 "이후" 대응이라 NaN 자체를 fault로 못 잡는 문제와 별개. 독립적 개선. **필드 레벨 확정(2026-07-25)**: `CFS_CORE_APP_StateCache_t`는 prefix(Timestamp/Seq/Valid/Stale/ErrorCode)만 있고 실제 수치 필드가 없음을 확인 — `isfinite()`는 `UpdateStateCache()` 내부가 아니라 **디스패치 지점(`MsgPtr`을 각 타입 전체 구조체로 캐스팅 가능한 곳)에서 타입별로** 삽입해야 함. `shared_msgs/fc_state_msg.h` 4종 확인 결과 대상은 **`FC_ATTITUDE_TLM_t`(RollRad/PitchRad/YawRad/RollspeedRps/PitchspeedRps/YawspeedRps, float 6개)와 `FC_EKF_LOCAL_TLM_t`(X_m/Y_m/Z_m/Vx_mps/Vy_mps/Vz_mps, float 6개) 2종뿐** — `FC_GPS_RAW_TLM_t`(LatE7/LonE7/AltMm, 전부 int32 고정소수점이라 NaN/Inf 불가)와 `FC_EKF_STATUS_TLM_t`(Flags뿐, float 없음)는 체크 대상 아님(GPS는 기존에도 헬스 미반영 정책과 일치, §6.5). 이 메시지들은 wire payload가 아니라 `mavlink_bridge_app`이 이미 파싱해 cFE 내부 버스(SB)로 발행하는 구조체라 별도 디코딩 계층 없이 필드 직접 접근 가능. **ErrorCode 신설 안 함(2026-07-25 확정)**: NaN/Inf 감지 시 새 `ErrorCode` 값을 추가하는 방안도 검토했으나 기각 — `ErrorCode` 필드는 이미 `mavlink_bridge_app`이 쓰는 필드(`MAVLINK_BRIDGE_ERROR_NONE/INVALID_VALUE/GPS_NO_FIX/EKF_UNHEALTHY` 등)라, `cfs_core_app`이 여기 새 값을 써넣으면 ① 필드를 쓰는 주체가 둘로 늘어 향후 충돌 위험(방금 발견한 `ROUTE_SEGMENT_LANDING`/`route_op` 충돌과 동일 유형) ② `mavlink_bridge_app`이 이미 정당하게 세팅한 값을 덮어써 정보 유실 위험. **`Valid`만 `false`로 덮고, 진단은 EVS 이벤트 로그로 남기는 걸로 확정**. **EVS 이벤트 ID 확정(2026-07-25)**: `cfs_core_app_eventids.h` 확인, 다음 사용 가능 번호는 21 — `CFS_CORE_APP_NONFINITE_VALUE_EID = 21`. **다음**: TDD(NaN/Inf 주입 테스트, Attitude/EkfLocal 2종) → 디스패치 지점에 isfinite 삽입(Valid=false + `CFS_CORE_APP_NONFINITE_VALUE_EID` EVS 이벤트) → 구현 | `fc_value_validation_gap` (2026-07-13 우선순위 문서) |
| **BL-60** | 🔵 **lora_tdm_app 시리얼 재오픈 로직 UT 커버리지 부족(2026-07-13 우선순위 문서 발굴)**. write/read 오류 시 `close(fd)+fd=-1` 처리 자체는 구현 완료·정상 동작하나, 해당 로직이 있는 `RunTx`/`RunRxWindow`가 static이라 UT에서 직접 호출 불가 — 실패 경로(write -1, read -1+errno=EIO) 커버 안 됨. **설계 확정(2026-07-25)**: 실기로는 write/read syscall 레벨 실패를 의도적으로 재현하기 어려움(BL-53과 동일한 한계 — 케이블 분리는 "완전 단절"이지 "syscall이 특정 순간 -1 반환"을 골라 만들 수 없음) 확인, 유닛테스트 mock 경로가 유일한 실질적 검증 수단. `RunTx`/`RunRxWindow`를 **`_internal.h`로 이동해 노출**(wrapper 함수 추가보다 코드 중복 적음, 기존 프로젝트 패턴과 일관). 독립적, 낮은 우선순위 | `fc_value_validation_gap` 인접 문서 (2026-07-13 우선순위 문서) |
| **BL-61** | ✅ **설계+구현+테스트 완료(2026-07-25)** — cfs_core_app 430/430, lora_tdm_app 329/329, openMCT 93/93 통과. Pi 실기 재검증만 남음. BL-56 firmware 구현(uplink_app/mavlink_bridge_app) 완료 후 `cfs_core_app`는 범위 밖이라 방치됐는데, 공유 구조체(`ROUTE_WAYPOINT_t`) 변경으로 **컴파일 자체가 깨진 상태**로 확인됨. 원인 조사 결과 production 소스(`cfs_core_app_utils.c`)는 `memcpy` 기반이라 실제로는 멀쩡, **깨진 건 유닛테스트 파일(`coveragetest_cfs_core_app_utils.c`) 하나뿐**(옛 `.X/.Y/.Z` 필드 직접 세팅 코드 20곳). 다만 조사 중 **더 근본적인 로직 버그**를 발견: `cfs_core_app`가 `uplink_app`으로부터 `ROUTE_UPDATE_MID`를 `mavlink_bridge_app`과 병렬로 독립 수신하는데, 지금은 REPLACE/ADD/DELETE/MODIFY 구분 없이 무조건 `memcpy`로 캐시(`MissionRoute`) 덮어쓰기 — ADD/DELETE/MODIFY payload는 전체 목록이 아니라 신규분/인덱스만 담고 있어 그대로 덮으면 캐시가 완전히 틀어짐(예: ADD 2개 payload를 memcpy하면 기존 캐시가 "2개짜리 미션"으로 오염). **해결 설계**: 이미 존재하는 `FC_MISSION_READBACK_MID`(BL-41, "FC가 유일 진실원본")가 미션 업로드 완료마다(op 무관) 자동 트리거되는 걸 활용 — REPLACE는 기존처럼 `ROUTE_UPDATE_MID` 수신 즉시 memcpy 유지, **ADD/DELETE/MODIFY는 `Waypoints`/`WaypointCount`를 안 건드리고 뒤이은 자동 readback이 정확한 최종상태로 갱신할 때까지 대기**(신규 동기화 로직 불필요, 업로드 실패 시에도 캐시가 손상 없이 이전 상태 유지되는 부수 이점 확인). **HK 카운터(`RouteUpdateCount`/`LastRouteUpdateTimestampMs`)는 원본 op 수신 즉시가 아니라 readback 확정 시점에만 갱신**(사용자 결정 — "실패해도 카운터 올라가는" 불안정한 의미 방지). `LandingRoute` 캐시(HK에 개수만 보고, 실제 어디에도 안 쓰이는 죽은 데이터 확인됨)는 당장은 유지, 삭제는 보류. **다음**: 유닛테스트 파일 필드 갱신 → `CFS_CORE_APP_UpdateRouteCache` 호출부를 op별 분기(REPLACE=즉시 반영, ADD/DELETE/MODIFY=readback 대기)로 재작성 → HK 카운터 갱신 시점 이동 → 회귀 테스트. **추가 발견+설계 확정(2026-07-25, openMCT 레포)**: `RouteReadbackAssembler`(`lora_protocol_v2.py:230`) 자체는 페이지 재조립기라 문제없지만, 그 안에 담기는 DL2 다운링크 waypoint 페이지 디코딩(`lora_protocol_v2.py:130~137`)이 옛 `(x,y,z)` 3-float×2개/페이지 포맷으로 하드코딩돼 있어 재설계 필요 — **좌표(LatE7/LonE7/Z, 12바이트)만 담고 CmdType/Param1~4는 생략(readback 시 기본값 NAV_WAYPOINT/0으로 복원)하는 걸로 확정**, 페이지당 2개 유지(페이지 크기·재조립 로직 변경 최소화). **BL-61 범위에 `lora_tdm_app` 추가(2026-07-25)**: `lora_tdm_app_utils.c:280-285`가 바로 이 DL2 waypoint 페이지를 만드는 기체측 인코더(openMCT `RouteReadbackAssembler`의 반대편)인데, `.X/.Y/.Z` 필드를 직접 읽어 채우고 있어 **production 코드 자체가 컴파일 깨짐**(`cfs_core_app`은 memcpy라 안 깨졌지만 여긴 필드 직접 접근이라 진짜로 깨짐). 위에서 정한 DL2 결정(좌표 LatE7/LonE7/Z만, 페이지당 2개, CmdType/Param 생략)을 인코더에 그대로 적용 — 신규 설계 아니라 반영. 관련 유닛테스트(`coveragetest_lora_tdm_app_utils.c`, `.X/.Y/.Z` 직접 세팅 다수)도 같이 갱신 필요. **✅ 구현 완료(2026-07-25)**: `CFS_CORE_APP_UpdateRouteCache`를 `CFS_CORE_APP_SetRouteCacheWaypoints`(캐시 반영)와 `CFS_CORE_APP_BumpRouteCacheCounters`(HK 카운터)로 분리 — REPLACE는 즉시 둘 다, ADD/DELETE/MODIFY는 `FC_MISSION_READBACK_MID` 확정 시에만 둘 다 실행. **구현 중 실제 버그 발견 및 수정**: `CFS_CORE_APP_ROUTE_SEGMENT_LANDING == 2`가 새 `route_op ADD == 2`와 값이 충돌 — 옛 세그먼트 enum으로 분기하던 코드가 그대로 있었으면 ADD 명령이 죽은 `LandingRoute` 캐시로 잘못 라우팅될 뻔했음. 명시적 `CFS_CORE_APP_ROUTE_OP_{REPLACE,ADD,DELETE,MODIFY}` 상수 신설해 분기 전환으로 해결(`LandingRoute` 구조체는 유지, 도달 불가 상태로만 남음). `lora_tdm_app`은 `PutI32LE` 신규 헬퍼로 DL2 페이지 인코더 수정. openMCT `lora_protocol_v2.py`도 대칭으로 `<iif` 디코딩/인코딩 전환, `mapRouteGUI` 플러그인의 E7→도(度) 변환 누락도 같이 발견·수정. **테스트**: cfs_core_app 430/430, lora_tdm_app 329/329, openMCT 93/93 전부 통과 — 3개 저장소 순서(cfs_core_app→lora_tdm_app→openMCT) 구현 완료 | BL-56 구현 후속, 사용자 질문("cfs_core_app 설계 다 됐나")으로 발견 |
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
