# 전체 점검 기록 (2026-07-29) — 신규분만

> `notes/temp/full_audit_2026-07-28.md`와 중복 제거 후 남은 항목만 기록.
> CRLF(T-14), lora_tdm 타이밍값 불일치(H-1/H-2), mavlink README ARMED
> 정책(H-9), README MID표/fcncode 누락(M-6/M-7/L-6), build_check/build
> 산출물 커밋(530-538행)은 전부 07-28 문서에 이미 상세 기록돼 있으므로
> 여기서 제외.

## 1순위 — notes 충돌 (07-28 이후 신규분)

- **cfs_core_app**: `notes/cfs_core_app_command_execution_gap_completed.md`가
  BL-81(MODE_CMD EXEC_RESULT 회신, `cfs_core_app_utils.c:1165-1169`,
  2026-07-29 완료), BL-82(VIEWPOINT_CMD 짐벌 미탑재→FAILED 회신 확정,
  `:815-819`, 2026-07-29 완료)를 반영 못 함. VIEWPOINT 항목이 "방향 미정"으로
  남아있으나 실제는 확정됨. `cfs_core_app_behavior_spec.md` §17도 동일 갱신
  필요. (07-28 감사 시점엔 BL-81/82가 아직 미완료였어서 이 문서엔 없던 항목 —
  BL-81/82 완료로 새로 생긴 note 미반영 갭)

- **lora_tdm_app**: `lora_tdm_app_behavior_spec.md` §9, `lora_protocol_v2_spec.md`
  §11이 실제 구현된 v2 바이너리 상태머신(ACK2/UP2 다중 RX창 파싱,
  `lora_tdm_app.c:117-264`)을 "설계 확정, 코드 미착수"로 서술 — 실제로는
  구현+BL-78/79/86 감사까지 완료된 상태. 07-28 문서는 타이밍값 불일치만
  지적했고 이 구현상태 stale 서술은 다루지 않음.

- **uplink_app**: `_completed` 파일명인데 실제 미완 항목이 남은 note 3건
  (07-28 문서 L-1은 "20개 파일 중 대부분은 실제 구현완료, 체크박스만 미정리"
  라 결론지었으나 아래 3개는 구체적으로 특정되지 않았고, 확인 결과 실제
  미완 상태라 L-1의 "예외" 목록에 추가 필요):
  - `notes/uplink_seq_feedback_redesign_2026-07-21_completed.md`: T2/T7 판단
    보류, T5 업링크 태그 미착수, T9 미착수, T10(`LoadState()` 조용한 실패,
    `uplink_app_utils.c:480-513`) 미해결.
  - `notes/mission_item_int_frame_gap_completed.md:57-60`: "실물 FC로 INT
    경로 검증" 여전히 `[ ]`.
  - `notes/route_2pass_gps_correction_scoping_2026-07-22_completed.md`:
    "설계만 존재, 코드 0%"이나 제목이 완료로 오인되기 쉬움.

## 2순위 — 코드 문제 (신규, 경미)

- `tools/uplink_flight_mode_sender.py` — docstring상 "HOVER/LAND는
  waypoint-index 0 고정" 규약이 코드(`build_flight_mode_payload()`,
  `main()`, `uplink_app_cmds.c` HOVER/LAND 처리부)에 강제되지 않음. 임의
  인덱스로 실행 가능하고 테스트에도 검증 케이스 없음.
- `camera/correlate_video_telemetry.py:93-94` — `datetime.fromtimestamp(start_ms/1000)`에
  `tz=timezone.utc` 미지정. 매칭 로직(epoch ms 비교)은 정확하지만 콘솔 출력
  "구간: …" 문구가 로컬시간대로 나와 UTC로 오인 시 영상 검토 혼동 가능.

## 3순위 — 기타

- 07-28 문서 범위 밖 신규 발견 없음 (shared_msgs 필드 정합성, mission_defs,
  telemetry_app 전부 이상 없음 확인).

## 후속 조치 후보

1. `cfs_core_app_command_execution_gap_completed.md` / `cfs_core_app_behavior_spec.md`
   §17에 BL-81/82 반영
2. lora_tdm 두 spec 문서의 §9/§11 구현상태를 "구현완료"로 갱신
   (타이밍값 갱신은 07-28 문서 조치안에 이미 포함되어 있으므로 함께 처리)
3. uplink note 3건 실제 상태 재확인 후 갱신 또는 후속 작업 재개
4. camera 스크립트 UTC 명시, flight_mode_sender 인덱스 강제 로직(선택, 낮은 우선순위)
