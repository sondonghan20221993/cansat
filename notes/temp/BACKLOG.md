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

## 🔴 긴급 — 현재 배포된 코드가 버그 상태

| ID | 내용 | 근거 | 규모 |
|---|---|---|---|
| **BL-01** | **재전송 중복이 실패로 보고됨.** 지상이 명령을 4슬롯 재전송하는데 2~4번째가 `REJECT_SEQUENCE`가 되고, 오늘 넣은 `04d8f99`+`073a680`이 겹쳐 **성공한 명령이 SEQ_FAIL을 영구 래치**. `Sequence == LastAccepted`(우리 재전송)와 `< LastAccepted`(replay)를 구분해 전자를 별도 코드로 | `uplink_seq_feedback_redesign` T1 | 소, 기체만 |
| **BL-02** | `073a680`(RunTx UFB 리셋 제거) **유지 vs 롤백 판단**. BL-03이 곧 오면 유지, 지연되면 롤백 | 동 T2 / `inferred_decisions_selfaudit` A-1 | 판단 |
| **BL-03** | **지상국 재시작 시 명령권 상실.** 기체는 `LastAcceptedSequence`를 파일에 영속화하는데 지상 `_SeqCounter`는 재시작마다 1로 리셋 → 이후 모든 명령 거부. 해법: 다운링크에 `ufb_seq` 동봉 → 지상이 그 값 보고 이어받기(자가복구) | 동 T3+T4 | 중, 양쪽 + DL2 +2B |

> BL-03은 BL-04\~BL-06(프로토콜 확장)의 **공통 선행**. 무선 포맷을 여러 번
> 나눠 바꾸면 지상/기체 버전 호환이 계속 깨지므로 **한 번 열 때 필요한
> 필드를 모두 넣을 것**.

---

## 🟠 spec이 사실과 다름 (문서가 거짓 주장)

| ID | 내용 | 근거 |
|---|---|---|
| **BL-04** | `lora_tdm_app_behavior_spec.md:381-386`이 **✅(구현됨)로 표기한 EID 6종이 코드에 없음** — `LINK_LOST/DEGRADED/RESTORED`, `PIPE_ERR`, `SUB_ERR`, `SB_SEND_ERR`. 링크 3종은 `UpdateLinkState()`가 전이 감지 없이 매 사이클 무조건 대입해 이벤트 낼 지점 자체가 없음 → **구현하거나 spec 표기를 정정하거나 택일** | `system_wide_reaudit` F-1 |
| **BL-05** | `mission_app_runtime_spec.md` §18.4.6.4가 "전달 vs 실행 구분 … 완료"라 하지만 `EXECUTED` 결과코드 자체가 없음 → **BL-08과 함께 처리** | `command_dead_end_audit` F1 |
| **BL-06** | stale TODO 주석 2건 — `lora_tdm_app.h:68`("CONFIG 커맨드 미배선"), `lora_tdm_app_utils.h:72`("SysTime 미지원") 둘 다 **실제로는 구현 완료**. 주석만 정정 | `system_wide_reaudit` F-2/F-3 |
| **BL-07** | `cfs_core_app_command_execution_gap.md` **문서 자체가 낡음** — "MODE 상태 전이 미구현"이라 적혀 있으나 실제로는 구현돼 있음(`cfs_core_app_utils.c:786-829`, 전이 검증 포함). RECOVERY 항목도 부분 진전됨(switch 추가). 문서 갱신 후 이 백로그로 흡수 | 오늘 확인 |

---

## 🟡 미구현 기능 (spec엔 있는데 코드가 없음)

| ID | 내용 | 근거 | 선행 |
|---|---|---|---|
| **BL-08** | **실행결과(EXECUTED) 회신 채널.** `uplink_app`은 "라우팅 성공"까지만 앎. 대상 앱은 자기 결과를 갖고 있으나(`mavlink_bridge_app`의 `LastConfigResult`) **돌아오는 길이 없음**. 대상앱→`uplink_app` 회신 MID 신규 설계 필요 | `ground_plan` P0 / `command_dead_end` F1 / T7 | 설계 필요, 범위 큼 |
| **BL-09** | **`cfs_core_app` RECOVERY 명령 실제 연결.** `RESTART_BRIDGE`/`PARSER_RESET`/`SERIAL_RECONNECT`가 로그만 찍음. 대상 기능은 `mavlink_bridge_app`에 이미 존재(`ResetParser()`, `OpenSerial()`/`CloseSerial()`)하나 크로스앱 명령 채널이 없음. **추가로 `RESTART_UPLINK`/`RESTART_LORA`는 enum 자체가 없음**(자동 재시작 로직은 있음) | `ground_plan` P1-a~d / `command_dead_end` F2 | 중 |
| **BL-10** | `VIEWPOINT_CMD_MID` 캐시의 **활용처 결정** — 실제 쓸 데가 있는지, 없으면 spec에 "저장만, 활용 미정" 명시로 종결 | `ground_plan` P2 / `cfs_core_app_command_execution_gap` | 소(결정) |
| **BL-11** | **UFB 코드표 전체 확정.** 현재 `REJECT_STATE`만 매핑돼 나머지 7종(`FAILED`, `REJECT_CLASS`, `REJECT_LENGTH`, `ROUTE_MISS`, `REJECT_ROUTE`, `REJECT_CHECKSUM`, `REJECT_VIEWPOINT`)은 지상에서 OK와 구분 불가 | T8 / `selfaudit` A-2 | **BL-08** |

---

## 🔵 설계 결정 필요 (구현 전 방향 확정)

| ID | 내용 | 근거 |
|---|---|---|
| **BL-12** | **부트 카운터(세션 번호)** 도입 여부. 다운링크 seq 보고만으로는 위조/구분불가/wrap혼동 문제가 남음. 단 **"감소=공격=자동거부"로 만들면 안 됨**(상태파일 유실 시 복구 불가) → "운영자 확인 필요" 상태로 | T5 |
| **BL-13** | seq 비교를 **모듈러 윈도우**로: `diff=(uint16)(seq-last); 0<diff<0x8000`. 65535 wrap 해소. **uint16/uint32 혼용 주의**(무선은 uint16, 내부는 uint32) | T6 / 문제3 |
| **BL-14** | **재전송 인덱스**를 `Flags` 여유비트(`bits[5:1]`)에 실을지 — 프레임 크기 증가 없음. 정확성보다 **RF 링크 마진 진단** 목적 | T9 |
| **BL-15** | **LoRa 다운링크 5Hz 상향 여부.** 200ms는 이미 실측 검증됨(2026-07-14, 손실 0%). **200ms 미만은 미검증** — 운용 요구가 실제 있는지부터 | `lora_downlink_5hz_cap` |
| **BL-16** | `downlink_protocol` 범위 불일치 — 기체는 `!=0`이면 다 수락, 지상은 오늘 내가 0/1로 제한(**기존 되던 동작을 막음**). 기체를 엄격화할지 지상을 완화할지 | `selfaudit` A-4 |

---

## ⚪ 위생 / 소규모 (독립 착수 가능, 프로토콜 무관)

| ID | 내용 | 근거 |
|---|---|---|
| **BL-17** | `LoadState()` 실패 경로가 **조용히 `return`만** 함 → 상태파일 손상으로 "아무 seq나 수락" 상태 기동을 지상이 알 수 없음. 이벤트 추가 | T10 |
| **BL-18** | `SaveState()`에 **`fsync()` 없음** — rename은 원자적이나 전원차단 시 내용 손상 가능 | T11 |
| **BL-19** | **죽은 config 상수** — `LORA_TDM_APP_LORA_BAUDRATE`(코드가 `B57600` 하드코딩해 무시), `SERIAL_REOPEN_DELAY_MS`(미사용), `PROTOCOL_VERSION`, `MAX_PAYLOAD_LENGTH`, `STREAM_REACQUIRE_TIMEOUT_MS`, img_app 잔재 상수들 | `system_wide_reaudit` F-4/F-5/F-6 |
| **BL-20** | 문서 stale 식별자 — `uplink_lora_test_status.md`의 `UPLINK_RAW=0x1909`(실제 `0x18D0`, 0x1909는 FC_SYS_TIME), 존재하지 않는 `lora_fc_downlink_app` 참조 4곳 | 동 F-7 |
| **BL-21** | `lora_tdm_app` fcncode 정의 위치 중복(`fsw/inc` vs `config/`) — 기능 문제 아님, 정리만 | 동 F-6 |
| **BL-22** | Pi `/boot/firmware/config.txt`의 `init_uart_clock=48000000` — **기각된 가설의 잔재**, 되돌릴지 결정(Pi 전원 필요) | `selfaudit` B-1 |

---

## 🟣 지상국(openMCT, 별도 repo)

| ID | 내용 |
|---|---|
| **BL-23** | `_flush_pending_uplink()`가 타이머 없이 **다운링크 수신 시에만** 동작 → 링크 끊기면 큐 명령이 유실되는데 HTTP는 이미 성공 반환. **단, 반이중 슬롯 정렬을 깨지 않아야 함** |
| **BL-24** | UFB=1 자동 재전송이 **새 seq로 재조립** — 진짜 재전송이 아니라 "같은 내용의 새 명령". 3회 카운트가 원 프레임 기준이 아님. BL-01/BL-13과 함께 볼 것 |
| **BL-25** | `uplinkCLI` 도움말은 RECOVERY의 `action/target/reason`을 "무시됨"이라 하는데 서버는 실제로 조립·전송·성공로그 → **BL-09와 묶어서** 처리 |
| **BL-26** | `DL2_BASE_LEN=45`, UFB 값들이 기체 enum을 손으로 복제한 매직넘버 → 동기화 강제 장치 없음 |
| **BL-27** | `PARAM_BOUNDS`도 기체 상수를 손으로 복제(오늘 내가 추가) → **BL-26과 함께 C헤더 교차검증 테스트**로 해결 권장 |
| **BL-28** | `test_dl2_base_len_includes_sats_field`가 상수를 **자기 자신과만 비교** → 크로스 repo 드리프트를 못 잡음. `test_lora_downlink_decoder.py`의 C↔Python 교차검증 패턴 참고 |

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
   BL-04(구현 vs spec정정), BL-10, BL-15, BL-16

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
