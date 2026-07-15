# uplink_app_cmds UT 44개 실패 — 3중 원인 규명 및 수정 (2026-07-14 도출/해소)

## 문제

`uplink_app_cmds` 단위테스트(spec에 "미실행"으로 기록돼 있던 것)를 처음 로컬에서
돌려본 결과 86개 중 44개 FAIL. 프로덕션 코드는 실기체에서 정상 동작 중이었으므로
테스트 문제로 추정하고 조사.

## 원인 (3가지 중첩)

1. **`CfsHealthReceived` fail-closed 정책 미반영 (주범)** — `uplink_app_cmds.c`의
   health-received 게이트가 커밋 `1112351`에서 `if (CfsHealthReceived) {...}`
   (미수신 시 통과, fail-open)에서 `if (!CfsHealthReceived) { REJECT_STATE }`
   (미수신 시 항상 차단, fail-safe boot)로 **의도적으로** 극성이 뒤집혔으나, 그
   이후 이 UT 스위트가 한 번도 실행되지 않아 대부분의 "성공(ROUTED) 기대" 테스트가
   §18.11.1 auth 체크에 도달하기도 전에 이 게이트에서부터 막히고 있었음.
2. **§18.11.1 인증레벨 `Flags` 미설정** — 거의 모든 테스트가 `TestMsg.Flags`를
   세팅하지 않아 `auth_level=0`으로 고정, `IsAuthorized()`가 요구레벨(2 또는 3)에
   항상 미달.
3. **`GetClassRequiredLevel()`의 DIAGNOSTIC 영구 인증 불가 (실제 코드 버그)** —
   switch case 값이 `UPLINK_APP_CLASS_*` enum과 라벨이 어긋나(MODE=5/DIAGNOSTIC=6
   자리 뒤바뀜) DIAGNOSTIC이 case 6(요구레벨 3)에 걸림. 레벨 3은 0이 아닌
   `request_token` 필수인데 토큰 파싱 분기는 RECOVERY/MODE 클래스에만 존재 —
   DIAGNOSTIC은 `Flags`를 뭘로 채워도 영구 인증 불가. 스펙상 DIAGNOSTIC은
   RECOVERY/FAILED 상태에서 RECOVERY와 함께 유일하게 허용되는 "항상 통하는 개입
   경로"인데 실제로는 이 경로 자체가 막혀 있었음.

부수 발견: 두 테스트가 실제 정책과 반대로 작성돼 있었음(우연히 위 버그들과
상쇄되어 PASS로 위장):
- `BlockedFailed`: DIAGNOSTIC class로 작성돼 있었으나 DIAGNOSTIC은 FAILED에서도
  허용돼야 함 — 테스트 의도(차단 검증)와 반대
- `FailOpenBeforeHealth`: 옛 fail-open 정책 기준 이름/기대값 그대로 방치, 커밋
  `1112351`의 fail-closed 정책 반영 안 됨

## 수정 (커밋 `740521d`)

- 모든 "성공 기대" 테스트에 `CfsHealthReceived=1U` + 클래스별 `Flags` 인증레벨 추가
- RECOVERY/MODE(레벨3) 테스트에 request_token 페이로드 바이트 추가
- `GetClassRequiredLevel`을 `UPLINK_APP_CLASS_*` named enum으로 재작성,
  DIAGNOSTIC↔MODE 요구레벨 스왑(DIAGNOSTIC: 3→1, MODE: 1→3) — DIAGNOSTIC 인증 가능해짐
- `BlockedFailed`: CommandClass를 DIAGNOSTIC→CONFIG로 정정(`BlockedRecovery`와 동일 패턴)
- `FailOpenBeforeHealth` → `BlockedBeforeHealth`로 개명, 기대값을 현재 정책(REJECT_STATE)에 맞게 정정
- 하네스 정상화로 제외돼있던 `ForceFlagBypassesDegradedBlock`/`ForceFlagNoOpWhenNotBlocked`
  양성 테스트 재추가

## 관련 항목

- `mission_app_runtime_spec.md` §18.10.2~§18.10.4 (FORCE_FLAG 설계, §18.11.1 권한
  레벨 미전송 발견, 이번 UT 조사 전체 기록)
- `uplink_app/fsw/src/uplink_app_cmds.c`, `uplink_app/unit-test/coveragetest/coveragetest_uplink_app_cmds.c`

## 상태

- [x] 원인 규명 (요인 A/B/C 3중 원인 분리)
- [x] 스펙 문서화 (§18.10.4, 프로덕션 코드 수정 전에 선행)
- [x] 테스트 픽스처 수정 + `GetClassRequiredLevel` 프로덕션 코드 수정
- [x] 로컬 UT 검증 — `uplink_app_cmds` 91/91 PASS, `uplink_app`(8/8)·`uplink_app_dispatch`(13/13) 회귀 없음
- [x] 커밋 + push (`740521d`)
- [x] Pi 실기체 배포 — **완료 (2026-07-14)**. Pi를 origin/main(`b8763b0` 이후)으로
      재동기화 + 전체 재빌드(`cfs_core_app`/`uplink_app`/`mavlink_bridge_app`/
      `lora_tdm_app`)·`cfs.service` 재시작으로 반영 확인
      (`uplink_app_cmds.c`: `CLASS_DIAGNOSTIC→1`, `CLASS_MODE→3` 실측 확인).
- [x] §18.10.3에서 이미 식별된 별도 항목 — 지상(`fc_serial_ws_server.py`)의
      §18.11.1 인증레벨 bit[7:6] 반영, 2026-07-14 완결·커밋·push
      (`openMCT` repo commit `f65b295`). 최초 diff는 CONFIG 핸들러에만
      적용돼 있었고 ROUTE_UPDATE/RECOVERY는 누락된 미완결 상태였음 —
      두 핸들러에도 동일 적용해 완결. 상세는 그 repo의
      `openmct_bridge_notes.md` §18.11.1 절 참조.
      **Pi 배포도 완료** — `GetClassRequiredLevel` 스왑과 함께 2026-07-14
      재동기화로 반영됨. (openMCT `fc_serial_ws_server.py`의 §18.11.1 플래그
      반영은 지상 PC에서 최신 코드 실행 중인지 별도 확인 필요 — 이건 openMCT
      레포 배포 확인 문제라 이 항목과는 무관)
- [x] `uplink_app_utils` UT의 무관한 사전 결함 4건(`ParseLoRaFrame`) — 별도 조사
      (2026-07-14) 및 수정 완료. 원인: `sscanf("%[^,]", ...)`는 0글자 매칭을
      허용하지 않아, payload가 없는(길이 0) **모든 정상 v1 ASCII uplink 명령이
      실제 운영 경로(`uplink_app_dispatch.c:43`)에서 항상 파싱 실패**하고 있었음
      (조용히 `ErrCounter`만 증가, `ProcessUplink` 자체가 호출 안 됨). 흥미롭게도
      동일 로직이 중복 구현된 `lora_tdm_app_utils.c::ProcessUpFrame`에는 이미
      과거에 동일 버그가 발견되어 retry-fallback으로 수정돼 있었으나
      (`uplink_app_utils.c::ParseLoRaFrame`으로는 전파 안 됨) — 두 함수가 같은
      파싱 로직을 독립적으로 중복 구현하고 있다는 것 자체도 향후 통합 검토 대상.
      수정은 sscanf 기반 파싱을 수동 콤마-분리 방식으로 교체(모든 필드의 0글자
      케이스를 일반적으로 처리). `uplink_app_utils` UT 102/102 PASS(`ParseLoRaFrame`
      14/14 포함), `uplink_app`(8/8)·`uplink_app_cmds`(91/91)·`uplink_app_dispatch`
      (13/13) 회귀 없음. 아직 커밋/Pi 배포 전.
