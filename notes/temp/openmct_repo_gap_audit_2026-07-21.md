# openMCT 저장소(별도 repo) 갭 점검 (2026-07-21)

> 저장소: `/mnt/c/Users/sdh97/Documents/GitHub/openMCT` (이 repo와 별개 git).
> cfs-telemetry-app에서 발견한 3개 버그 클래스(dead-end 핸들러 / 상태
> 리셋 레이스 / spec-코드 매직넘버 불일치)와 같은 패턴이 있는지 점검.

## ⚠️ TODO 사용 주의사항

아래 TODO는 **착수 순서/체크리스트일 뿐, 이것만 보고 바로 구현하면 안 됨.**
각 항목마다 명시된 **관련 spec 문서를 먼저 읽고 세부 요구사항·기존 결정
사항(예: request_token 규약, 권한 레벨, health 게이트 정책)을 확인한
뒤** 작업 범위를 정할 것. 특히:
- `notes/mission_app_runtime_spec.md` (uplink 프로토콜 전체 규약,
  §18.4~18.11 특히 CONFIG/RECOVERY/권한검증/request_token)
- `notes/lora_tdm_app_behavior_spec.md` §9~10 (UFB/판정 지속 정책)
- `notes/lora_protocol_v2_spec.md` (v2 바이너리 프레임 포맷 전체)
- `notes/uplink_lora_test_status.md` (기존 uplink 실측 결과/제약사항)

이 TODO는 "무엇을 봐야 하는지"의 포인터이지 "무엇을 어떻게 구현할지"의
답이 아님 — spec에 이미 정해진 제약(예: 반이중 TDM 300ms RX 윈도우,
재전송 4회 슬롯 정렬 등)과 충돌하지 않는 방식으로 구현해야 함.

## TODO

- [ ] **openMCT-1**: `_flush_pending_uplink()`가 타이머 없이 다운링크
      수신 시에만 트리거됨(`fc_serial_ws_server.py:361`, 호출부
      `serial_reader():779`) — 기체 링크가 끊기면 큐에 쌓인 명령이
      영원히 안 나가거나 조용히 유실될 수 있는데 HTTP 응답은 이미
      `{"ok": true, "queued": true}`로 성공 반환됨.
      → **관련 spec**: `mission_app_runtime_spec.md`의 uplink 재전송
      슬롯 규약(§18.4 근처)과 `notes/uplink_lora_test_status.md` §4
      "문제 A/B"(RX 윈도우 타이밍) 먼저 확인 — 반이중 TDM 구조상 무작정
      타이머 추가가 슬롯 정렬을 깨뜨릴 수 있음.
- [ ] **openMCT-2**: `plugin.js:156`의 UFB=1(CRC_FAIL) 자동 재전송이
      매번 `sendConfig()`/`resend()`로 **새 seq를 발급받아 다시 조립**함
      — 실패한 프레임의 진짜 재전송이 아니라 "같은 내용의 새 명령"이라
      3회 카운트가 원래 프레임 기준이 아님.
      → **관련 spec**: `lora_tdm_app_behavior_spec.md`의 시퀀스 검증
      로직(`IsSequenceAccepted`) 및 `uplink_lora_test_status.md` §4
      "문제 A"(4연속 재전송 설계) — seq 재사용이 그 중복거부 로직과
      상충하지 않는지 확인 필요.
- [x] 완료(2026-07-22, BL-25+BL-09). **openMCT-3**: `uplinkCLI` 도움말은 RECOVERY의
      `action/target/reason` 바이트를 "uplink_app이 무시함"이라고
      적어놨는데, 서버는 실제로 그 바이트들을 조립해서 전송하고
      성공으로 로그함(`fc_serial_ws_server.py:142` 부근) — cfs_core_app
      RECOVERY 갭과 같은 계열(문서-코드 불일치, 다만 이번엔 "무시된다"는
      쪽이 문서에 정직하게 써 있고 실제 동작이 그와 별개로 의미없이
      전송만 되는 케이스).
      → **관련 spec**: `mission_app_runtime_spec.md` §18.4.6.4(RECOVERY
      프로토콜) — `command_dead_end_audit_2026-07-21.md` Finding
      1/2(cfs_core_app 쪽 RECOVERY 미구현)와 **같이 묶어서** 처리
      방향 결정할 것(지상 코드만 고쳐봐야 기체 쪽이 무시하면 무의미).
- [x] 부분 완료(2026-07-21, BL-26/27/28 — 교차검증 테스트로 드리프트 감지는
      되지만 SSOT 파일 통합 자체는 안 함). **openMCT-4**: `lora_protocol_v2.py`의 `DL2_BASE_LEN=45`,
      UFB 0/1/2/3 값들이 기체측(`lora_tdm_app`) enum을 손으로 옮겨
      적은 매직넘버 — 이번에 `UFB_STATE_BLOCKED=3`을 기체측에 추가한
      것처럼, 앞으로 기체측 enum이 바뀌면 이 repo가 조용히 깨짐.
      공유 상수 파일(둘 다 참조하는) 또는 최소한 "반드시
      lora_tdm_app/config/default_lora_tdm_app_mission_cfg.h와
      동기화 유지" 주석/문서화 필요.
      → **관련 spec**: `lora_protocol_v2_spec.md` §7 타이밍표,
      `lora_tdm_app_behavior_spec.md` §10 UFB 코드표 — 두 문서를
      "단일 진실 공급원(SSOT)"으로 놓고 이 repo는 그걸 참조하는
      형태로 재구성하는 게 근본 해법.
- [x] 완료(2026-07-21, BL-26/27/28). **openMCT-5**: `tests/test_fc_serial_ws_server.py:79`의
      `test_dl2_base_len_includes_sats_field`가 상수를 자기 자신과만
      비교(`DL2_BASE_LEN == 45`) — cfs-telemetry-app 쪽 실제 프레임
      길이와의 교차검증이 없어서, lora_tdm_app 쪽 필드가 추가/삭제돼도
      이 테스트는 계속 통과함. lora_tdm_app 쪽엔 이미 Python↔C 교차검증
      벡터 테스트가 있음(`test_lora_downlink_decoder.py`의
      `test_cross_language_vector_matches_c_ut` 패턴) — 같은 방식을
      이 repo에도 적용 검토.
      → **관련 spec**: 없음(테스트 인프라 문제) — 대신
      `tests/test_lora_downlink_decoder.py`(cfs-telemetry-app repo)의
      기존 교차검증 패턴을 참고 구현.

## 점검했으나 문제없음으로 확인된 항목
- 상태 리셋 레이스(카테고리 2): `pendingCommand`/`ufbTimeoutHandle`/
  `downlinkSocket` 등에서 lora_tdm_app RunTx 같은 "매 틱 무조건 리셋"
  패턴 발견 안 됨.
- False-positive 성공 표시(카테고리 4): `UFB=0` 처리는 이미 이번
  세션에서 "오류 없음"과 "적용 확정"을 구분하는 문구로 수정 완료
  (`plugin.js:149`) — 추가 발견 없음.

## 관련
- `notes/temp/command_dead_end_audit_2026-07-21.md`
- `notes/temp/ground_controllable_capability_plan_2026-07-21.md`
- `notes/mission_app_runtime_spec.md`
- `notes/lora_tdm_app_behavior_spec.md`
- `notes/lora_protocol_v2_spec.md`
- `notes/uplink_lora_test_status.md`
