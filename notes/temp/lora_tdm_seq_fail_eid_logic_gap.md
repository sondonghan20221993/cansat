# lora_tdm_app — SEQ_FAIL_EID 로직 구현 완료 (2026-07-15)

## 배경

`lora_tdm_app_behavior_spec.md` §13, `spec_code_audit.md` 지적:

ACK 메시지 수신 시 `SeqEcho` 필드를 파싱하지만 **실제 검증 로직이 없었음**.

```c
// lora_tdm_app_utils.c:295 (구현 전) — 파싱만 있고 검증 없음
(void)SeqEcho;  // 무시됨
```

## 구현 내용

- `LORA_TDM_APP_Data_t`에 `SeqFailCount`(uint16), `LastSentSeq`(uint32) 필드 추가
- ACK(v1, ASCII)·ACK2(v2, 바이너리) 양쪽 프레임 처리 경로에 SeqEcho 검증 삽입
- 불일치 시 `SEQ_FAIL_EID` 이벤트 발생 + `SeqFailCount` 증가

## 1차 구현의 버그와 수정 경위

**1차 구현**은 `SeqEcho != AppData->DownlinkSeq`로 비교했으나, 이는 **항상 거짓 SEQ_FAIL을 유발하는 버그**였다.

원인: `RunTx()`에서 프레임 전송 시 프레임에 실리는 값은 전송 시점의 `DownlinkSeq`(N)이고,
전송 성공 직후 바로 `DownlinkSeq++`가 실행되어(`lora_tdm_app.c:261,296`) 값이 N+1로 증가한다.
지상국이 프레임 N에 대한 ACK(SeqEcho=N)를 보내오는 시점에는 이미 `AppData->DownlinkSeq`가
N+1이므로, `SeqEcho(N) != DownlinkSeq(N+1)`이 항상 참이 되어 **정상 ACK마다 SEQ_FAIL_EID가
오발생**하는 구조였다.

UT가 처음엔 통과했던 이유: 테스트가 `SeqEcho`와 `DownlinkSeq`를 수동으로 동일하게 맞춰
넣었기 때문에 실전 타이밍(전송→증가→ACK수신) 문제를 재현하지 못했음.

**수정**: 전송 성공 시점의 seq 값을 별도 필드 `LastSentSeq`에 저장하고,
ACK 비교는 `DownlinkSeq` 대신 `LastSentSeq` 기준으로 변경.

```c
/* RunTx() — 전송 성공 시 */
LORA_TDM_APP_Data.LastSentSeq = LORA_TDM_APP_Data.DownlinkSeq;
LORA_TDM_APP_Data.DownlinkSeq++;

/* ACK 수신 시 */
if (SeqEcho != AppData->LastSentSeq) { ... SEQ_FAIL ... }
```

## 회귀 방지 테스트 추가

버그를 실제로 재현하는 시나리오(TX 이미 증가된 상태에서 ACK 수신)를 UT로 추가:

- `Test_ProcessRxLine_Ack_SeqMatch_NoFalseFail` — LastSentSeq=7, DownlinkSeq=8(증가된 상태)에서
  ACK,7 수신 시 SeqFailCount가 증가하지 않아야 함
- `Test_ProcessRxLine_Ack_SeqMismatch` — 같은 조건에서 ACK,3(불일치) 수신 시 SeqFailCount++
- `Test_ProcessRxBinaryFrame_Ack2_SeqMatch_NoFalseFail` / `_SeqMismatch` — ACK2(v2 바이너리) 동일 검증

## 검증 결과

Pi(cFS_clean/build-ut) 빌드 및 회귀 테스트 실행:
- `coverage-lora_tdm_app-lora_tdm_app_utils-testrunner`: 49 그룹, **122/122 PASS**
- 기존 lora_tdm_app/lora_tdm_app_cmds/lora_tdm_app_dispatch 전체 회귀 통과

## 관련 파일

- `lora_tdm_app/fsw/src/lora_tdm_app.h` — `SeqFailCount`, `LastSentSeq` 필드
- `lora_tdm_app/fsw/src/lora_tdm_app.c` — `RunTx()` 두 경로(v1/v2)에서 `LastSentSeq` 저장
- `lora_tdm_app/fsw/src/lora_tdm_app_utils.c` — ACK/ACK2 검증 로직
- `lora_tdm_app/unit-test/coveragetest/coveragetest_lora_tdm_app_utils.c` — 회귀 테스트 4건
