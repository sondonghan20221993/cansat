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
| **BL-10** | 🔶 **결정(2026-07-21): 활용처 있음(A안) — 단 "무엇을 할지"는 추후 결정.** `VIEWPOINT_CMD_MID` 캐시를 실제로 소비할 로직(경로계획 연동 등) 설계가 남음. 지금은 방향만 확정, 착수는 보류 | `ground_plan` P2 / `cfs_core_app_command_execution_gap` | 후속설계 필요 |
| ~~**BL-11**~~ | ✅ **완료(2026-07-22)**. UFB 코드표를 4종→12종(`0x00~0x0B`)으로 확장 — uplink_app 내부 결과코드 번호를 그대로 복사하지 않고 UFB 전용 독립 번호로 순차 배정(무선 프로토콜을 SB 내부 enum과 분리). `lora_tdm_app_dispatch.c`에 8개 else-if 분기 추가(`FAILED`/`REJECT_VERSION`/`REJECT_CLASS`/`REJECT_LENGTH`/`ROUTE_MISS`/`REJECT_ROUTE`/`REJECT_CHECKSUM`/`REJECT_VIEWPOINT`). `DUPLICATE`(14)/`EXECUTED_OK`/`EXECUTED_FAILED`(15/16, BL-08)는 의도적으로 매핑 범위 밖(직전 값 유지). spec 3곳 갱신(`lora_tdm_app_behavior_spec.md` §9.2/§17, `lora_protocol_v2_spec.md`). 지상 디코더(openMCT, 별도 repo)는 이번 범위 밖 — 반영 작업 문서만 해당 repo `notes/temp/ufb_code_expansion_2026-07-22.md`에 기록 후 커밋/푸시(`f9e5a35`). 회귀 UT: lora_tdm_app 4종(64/14/145/59, dispatch에 신규 24케이스) 전부 PASS | T8 / `selfaudit` A-2 | 완료 |
| ~~**BL-CTR**~~ | ✅ **완료(2026-07-22)**. spec §18.4.6.7에 없던 counter management 명령 클래스(7)를 이번 세션 대화로 확정 후 구현. `uplink_app`이 `cfs_core_app` 경유 없이 대상 앱(mavlink_bridge/cfs_core/uplink 자신/lora_tdm, scope 1~4)에 직접 라우팅 — scope=uplink은 로컬 카운터 초기화, 나머지는 대상 앱의 기존 `CMD_MID`+`RESET_COUNTERS_CC=1`을 `CFE_MSG_SetFcnCode`로 재사용(신규 MID/CC 없음, P1-a 패턴 재사용). `lora_tdm_app` UFB `REJECT_COUNTER=12`(0x0C) 추가. 근거: `uplink_app`이 이미 `SYSTEM_HEALTH_MID`를 로컬 구독하므로 인가 판단에 cfs_core_app 왕복이 불필요(cFE SB 순수 비동기이기도 함). 회귀 UT: uplink_app_utils(120→131), uplink_app_cmds(+4), lora_tdm_app_dispatch(59→61) 전부 PASS | 2026-07-22 대화 확정 | 완료 |

---

## 🔵 설계 결정 필요 (구현 전 방향 확정)

| ID | 내용 | 근거 |
|---|---|---|
| ~~**BL-12**~~ | ✅ **완료(2026-07-22)**. 기체측 영속화(`uplink_app` 8비트 `BootCount`, `/cf/uplink_app_state.bin`) + BL-03에서 DL2 동봉/지상 `_BootCountTracker` anomaly 감지까지 완결 | T5 |
| ~~**BL-13**~~ | ✅ **완료(2026-07-22)**. `UPLINK_APP_CheckSequence()`를 모듈러 윈도우(`diff=(uint16)(seq-last); diff<0x8000`)로 변경 — 65535 wrap 해소. 회귀 UT 99/99(cmds) PASS | T6 / 문제3 |
| ~~**BL-14**~~ | ✅ **완료(2026-07-22)**. `Flags` `bits[2:1]=RETX_IDX`(0~3=슬롯-1, 0=최초 전송이라 구 프레임 하위호환) 확정 — spec §18.4.3.1 Flags 표에 기록(이때 bit[0] FORCE_FLAG 미반영 stale도 함께 정정). 기체(`uplink_app`): 수락/중복 EVS 이벤트에 `retx=` 표기만(검증 미사용, HK/DL2 변경 없음). 지상(openMCT `fc_serial_ws_server.py`): 큐를 프레임 문자열→구성요소 저장으로 바꿔 슬롯마다 flags+CRC 재조립. 회귀: uplink_app_cmds 110/110, openMCT pytest 56/56(신규 RetxIndexFlushTest 포함) | T9 | 완료 |
| **BL-15** | 🔶 **결정(2026-07-21): 상향 필요(A안) — 단 실측이 선행돼야 함.** 200ms는 검증됨(2026-07-14, 손실 0%), 200ms 미만은 미검증. Pi/LoRa 하드웨어로 단계적 실측(runbook Stage 2 방식 재사용) 후 상한 확정 → **BL-32로 이관** | `lora_downlink_5hz_cap` |
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
| **BL-22** | Pi `/boot/firmware/config.txt`의 `init_uart_clock=48000000` — **기각된 가설의 잔재**, 되돌릴지 결정(Pi 전원 필요) | `selfaudit` B-1 |

---

## 🟣 지상국(openMCT, 별도 repo)

| ID | 내용 |
|---|---|
| ~~**BL-23**~~ | ✅ **완료(2026-07-22, openMCT)**. 타이머 기반 강제 송신은 넣지 않음(반이중 슬롯 정렬 유지, 실제 송신은 여전히 다운링크 수신 시에만). 대신 큐 크기 상한(16, 사용자 결정)을 둬 링크 단절 중 계속 쌓이는 걸 방지 — 초과 시 HTTP 에러가 아니라 가장 오래된 항목을 버리고 경고 로그만 남김, 새 명령은 그대로 accept. pytest 57/57 PASS(신규 `UplinkQueueCapTest`) |
| **BL-24** | UFB=1 자동 재전송이 **새 seq로 재조립** — 진짜 재전송이 아니라 "같은 내용의 새 명령". 3회 카운트가 원 프레임 기준이 아님. BL-01/BL-13과 함께 볼 것 |
| ~~**BL-25**~~ | ✅ **완료(2026-07-21, BL-09와 함께)**. `uplinkCLI/plugin.js` 도움말을 실제 동작(byte[0]=action이 cfs_core_app RESTART 3종을 실제로 트리거함)으로 정정 |
| ~~**BL-26/27/28**~~ | ✅ **완료(2026-07-21, openMCT `c561aa3`)**. `tests/test_fc_serial_ws_server.py`에 형제 디렉터리 C 헤더를 정규식 파싱해 비교하는 교차검증 테스트 4건 추가(`DL2_BASE_LEN`↔`LORA_TDM_APP_DL2_LEN_FIELD`, `PARAM_BOUNDS[cfs_core/mavlink_bridge]`↔C 상수). repo 없는 환경은 skip. 35/35 PASS |

---

## 🧪 테스트

| ID | 내용 | 근거 |
|---|---|---|
| **BL-29** | A/B 그룹 단위테스트 (하드웨어 불필요, 지금 작성 가능) | `testcase_coverage_gap_2026-07-20` |
| **BL-30** | C-1: WSL x86 cFS 빌드 + pytest PTY fixture + CI_LAB 주입/EVS 관측 헬퍼 (E2E 하네스) | 동 |

---

## 🔧 실물 하드웨어 필요 (Pi/LoRa/FC/카메라)

| ID | 내용 | 근거 |
|---|---|---|
| **BL-31** | D-1: 앱 재시작 실측 (`tools/runtime_app_restart_test.sh`) | `testcase_coverage_gap` |
| **BL-32** | D-2: TDM/LoRa/DL2-SYSTIME 실물 테스트군 | 동 |
| **BL-33** | D-3: 통합 순차 세션 7단계 | 동 |
| **BL-34** | `MISSION_ITEM_INT` 경로 실물 FC 검증 (ArduPilot 거부 위험) | `mission_item_int_frame_gap` |
| **BL-35** | ACK seq 불일치 실측 (SF/BW/CR 확인 또는 Pi↔GS 타임스탬프 대조) | `pi_flight_build_missing_2026-07-16` |
| **BL-36** | camera P2(Pi 경유 SSH)/P4(카메라 SD 녹화) 확인 | `camera_phase_verification_gap` |
| **BL-37** | 실외 GPS 확보 후 `fault_code=3`(EKF_INVALID) 해소 확인 | `uplink_lora_test_status` §8.4 |

---

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

- `command_dead_end_audit_2026-07-21.md`
- `ground_controllable_capability_plan_2026-07-21.md`
- `openmct_repo_gap_audit_2026-07-21.md`
- `system_wide_reaudit_2026-07-21.md`
- `uplink_seq_feedback_redesign_2026-07-21.md` ← T1~T11 상세 설계는 여기 유지
- `inferred_decisions_selfaudit_2026-07-21.md` ← AI 추론 재검토 근거
- `cfs_core_app_command_execution_gap.md` ← **BL-07로 갱신 필요**
- `testcase_coverage_gap_2026-07-20.md`
- `lora_downlink_5hz_cap_2026-07-21.md`
- `mission_item_int_frame_gap.md`
- `camera_phase_verification_gap.md`
- `pi_flight_build_missing_2026-07-16.md`
