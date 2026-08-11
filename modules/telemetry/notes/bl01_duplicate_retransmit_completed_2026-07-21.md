# BL-01 완료 — 재전송 중복 오탐 차단 (2026-07-21)

## 문제
지상국이 반이중 타이밍 지터를 견디려고 같은 업링크 명령을 4회 연속(같은 seq)
전송하는 설계(§4x 재전송)와, `073a680`에서 제거한 UFB 리셋(성공 명령까지
래치)이 상호작용해 **성공한 명령의 2~4번째 재전송 슬롯이 `REJECT_SEQUENCE`로
거부되며 SEQ_FAIL을 영구 래치**시키던 버그.

## 결정 (착수 전 명확화, 임의 추론 아님으로 확정)
- `Sequence == LastAcceptedSequence` → **새 결과코드 `UPLINK_APP_RESULT_DUPLICATE = 14`**
  (재전송 슬롯, 에러 아님 — `ErrCounter`/`RejectedCount`/`LinkState` 불변,
  별도 `DuplicateCount`만 증가)
- `Sequence < LastAcceptedSequence` → 기존과 동일하게 `REJECT_SEQUENCE`
  (진짜 replay/desync)
- lora_tdm_app 쪽: DUPLICATE(14)는 SEQ_FAIL로 오귀속하지 않고 무시
  (`PendingUplinkFeedback` 직전값 유지 — dispatch.c의 기존 else 분기가
  이미 이 동작이라 코드 변경 불필요, 주석만 추가)
- 신규 EID `UPLINK_APP_DUPLICATE_EID = 8` (기존 NOOP_EID 재사용 대신 전용 EID)

## 남은 구멍 (문서화만, 이번 범위 밖)
- 부트스트랩 예외(`AcceptedCount==0`)와의 상호작용은 BL-03(부트카운터)
  선행 필요 — `ambiguity_audit_by_task_2026-07-21.md` 참고, 미해결 유지

## 테스트 (test-first: 테스트 추가 → 구현 → 통과 확인)
- `uplink_app_cmds` coveragetest: 기존 `RejectSequenceReplay`를 seq<last로 정정,
  신규 `DuplicateRetransmit` 추가
- `lora_tdm_app_dispatch` coveragetest: 신규 `Duplicate_NoLatchOverride` 추가
  (DUPLICATE 수신 시 UFB 직전값 유지 확인)

## 검증 결과 (로컬 WSL, `verify-build/cFS_verify/build-ut`)
- `coverage-uplink_app-uplink_app_cmds-testrunner`: **96/96 PASS**
- `coverage-uplink_app-uplink_app-testrunner`: 10/10 PASS
- `coverage-uplink_app-uplink_app_dispatch-testrunner`: 33/33 PASS
- `coverage-uplink_app-uplink_app_utils-testrunner`: 104/104 PASS
- `coverage-lora_tdm_app-lora_tdm_app-testrunner`: 64/64 PASS
- `coverage-lora_tdm_app-lora_tdm_app_cmds-testrunner`: 12/12 PASS
- `coverage-lora_tdm_app-lora_tdm_app_dispatch-testrunner`: **36/36 PASS** (34→36, 신규 2건)
- `coverage-lora_tdm_app-lora_tdm_app_utils-testrunner`: 122/122 PASS
- 총 477/477 PASS, 회귀 없음

## 참고
- `verify-build/cFS_verify`는 오래되어 `cfs-telemetry-app` 신규 파일
  (route_msg.h 등 shared_msgs)이 누락돼 있었음 — 전체 rsync로 동기화 후 빌드.
  실제 배포는 Pi(cFS_clean)에도 동일 rsync 필요 (Pi 전원 off, 추후 진행).

## 관련
- `notes/temp/BACKLOG.md` (BL-01)
- `notes/temp/uplink_seq_feedback_redesign_2026-07-21.md`
