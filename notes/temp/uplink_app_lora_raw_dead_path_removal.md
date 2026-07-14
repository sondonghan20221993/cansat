# uplink_app LORA_RAW 죽은 경로 삭제 — 0x1909 MID 충돌 해소 (2026-07-14 도출)

## 문제

`uplink_app`이 구독하는 `UPLINK_APP_LORA_RAW_MID_VALUE = 0x1909`가
`mavlink_bridge_app`의 `FC_SYS_TIME_MID_VALUE = 0x1909`(commit `38c2f22`,
2026-07-13)와 번호가 겹친다 — 이전에 고친 4-1(`0x190F`)과 같은 급의
MID 충돌.

## 경위 (git log로 확인)

- **2026-06-15 (`2fcaf80`, Task B)**: 당시 `lora_fc_downlink_app`(transport)이
  raw "UP,..." 텍스트를 `UPLINK_RAW_MID(0x1909)`로 SB publish하고,
  `uplink_app`(app)이 구독해 `ParseLoRaFrame()`으로 파싱하는 "분리형(A) 설계"
  채택 — CP2102 시리얼 포트를 한 앱만 열도록 하기 위한 의도적 설계였음.
- **2026-06-11 실제로는 이보다 먼저(`5712a0f`) `lora_tdm_app` 도입** —
  `lora_fc_downlink_app`을 대체하며 설계가 바뀜: TDM은 1초 주기
  TX→300ms RX창의 촉박한 타이밍이라, UP 프레임 CRC 오류를 **바로 다음
  다운링크 슬롯**에 UFB(Uplink Feedback Byte)=CRC_FAIL로 실어 지상국에
  알려야 함(`notes/lora_tdm_app_behavior_spec.md` §9.2). raw forward 후
  `uplink_app`과 비동기 SB 왕복으로 파싱 결과를 기다리면 같은 슬롯 안에
  못 돌아올 수 있음 — 그래서 `lora_tdm_app`이 **CRC 검증 자체를 자기
  안으로 가져와** 동기 처리하도록 재설계(`ProcessUpFrame`), 파싱 완료된
  구조체(`LORA_TDM_APP_UplinkFwdCmd_t`)를 별도 MID(`0x18D0`)로 직접 전달.
  (참고: SEQ_FAIL은 즉시성 불필요라 여전히 비동기 — `uplink_app`의
  `UPLINK_STATUS_MID` 폴링으로 처리)
- 이 전환 과정에서 `uplink_app`의 구 raw-forward 경로(구독/dispatch/
  `ParseLoRaFrame`)는 **정리되지 않고 그대로 남음** — 발행자(`lora_fc_downlink_app`)는
  이미 commit `7c080f1`(2026-06-30)로 저장소에서 삭제됐는데, 구독 쪽만 남아
  죽은 코드가 됨.
- **2026-07-13 (`38c2f22`)**: `mavlink_bridge_app`이 `0x1909`를 SysTime
  발행용으로 새로 할당 — 죽어있던 `uplink_app`의 구독과 번호가 겹치는 걸
  아무도 몰랐음.

## 영향

`uplink_app`과 `mavlink_bridge_app`이 둘 다 떠 있으면 `uplink_app`의
CommandPipe가 SysTime 텔레메트리를 `0x1909`로 수신하고,
`UPLINK_APP_TaskPipe`가 이를 raw LoRa 프레임으로 오인해
`ParseLoRaFrame()`에 넘긴다. 파싱 자체는 CRC 불일치로 안전하게
거부되어(크래시 없음) `ErrCounter`만 증가하지만, 의미 없는 오류
로그·카운터 증가가 계속 발생.

## 결정

`ParseLoRaFrame` 경로는 발행자가 없는 완전한 죽은 코드이므로 **전체
삭제**. `lora_tdm_app`의 `ProcessUpFrame`(→`UPLINK_APP_CMD_MID_VALUE
0x18D0`)이 유일한 실사용 경로이며 이걸로 충분 — 향후 raw 전달이
필요해지면 그때 재설계.

## 삭제 대상

- `uplink_app/fsw/src/uplink_app.c` — `UPLINK_APP_LORA_RAW_MID_VALUE` 구독
- `uplink_app/fsw/src/uplink_app_dispatch.c` — `UPLINK_APP_LORA_RAW_MID_VALUE`
  분기 블록 전체
- `uplink_app/fsw/src/uplink_app_utils.c` — `UPLINK_APP_ParseLoRaFrame()` 함수 전체
- `uplink_app/fsw/src/uplink_app.h` — `UPLINK_APP_ParseLoRaFrame` 프로토타입
- `uplink_app/config/default_uplink_app_msgid_values.h` — `UPLINK_APP_LORA_RAW_MID_VALUE` 정의
- `uplink_app/config/default_uplink_app_msgstruct.h` — `UPLINK_APP_LoRaRawMsg_t` 정의
- `uplink_app/unit-test/coveragetest/coveragetest_uplink_app_dispatch.c` —
  `Test_UPLINK_APP_TaskPipe_LoRaRaw` / `Test_UPLINK_APP_TaskPipe_LoRaRaw_BadFrame`
  + `ADD_TEST` 2건
- `uplink_app/unit-test/coveragetest/coveragetest_uplink_app_utils.c` —
  `Test_UPLINK_APP_ParseLoRaFrame` + `ADD_TEST` 1건
- `uplink_app/unit-test/stubs/uplink_app_utils_stubs.c` — 관련 stub

## 상태

- [x] 원인·경위 규명 (git log 대조)
- [x] 결정: 전체 삭제 (사용자 확인)
- [x] 코드 삭제 실행 — 구독(`uplink_app.c`)/dispatch 분기/`ParseLoRaFrame()`+선언/
      config(`msgid_values.h`/`msgstruct.h`)/coveragetest 2건/stub 전부 제거,
      `uplink_app/README.md` 레거시 언급 정리
- [x] UT 빌드/회귀 확인 — `uplink_app` 9/9, `_cmds` 91/91, `_dispatch` 29/29,
      `_utils` 88/88 전부 PASS, 회귀 없음
- [x] `mission_app_runtime_spec.md`(MID 표·§18.4.4 구현상태 노트)/`TEST_CASES.md`
      (coveragetest 카탈로그 3건)/`spec_code_audit.md`(4-8 신규 행 + 부록 A
      MID 인벤토리) 갱신 완료
