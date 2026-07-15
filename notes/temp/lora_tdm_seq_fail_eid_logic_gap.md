# lora_tdm_app — SEQ_FAIL_EID 파싱만 있고 로직 미구현

## 배경

`lora_tdm_app_behavior_spec.md` §13, `spec_code_audit.md` 지적:

ACK 메시지 수신 시 `SeqEcho` 필드를 파싱하지만 **실제 검증 로직이 없음**.

## 현황

```c
// lora_tdm_app_utils.c:295 — 파싱만 있고 검증 없음
(void)SeqEcho;  // 무시됨
```

## 해야 할 일

- [ ] `DownlinkSeq`와 `SeqEcho` 비교 로직 구현
- [ ] 불일치 시 `SEQ_FAIL_EID` 이벤트 발생
- [ ] `SeqFailCount` 증가
- [ ] HK 텔레메트리 보고

## 참고

- `lora_tdm_app/fsw/src/lora_tdm_app_utils.c:290~310` (ACK 파싱)
- `lora_tdm_app/fsw/inc/lora_tdm_app_eventids.h` (`SEQ_FAIL_EID` 정의)
