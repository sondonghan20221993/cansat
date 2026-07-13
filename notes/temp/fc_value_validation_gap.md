# FC 수신 값 finite 검증 갭 — 설계 검토 필요 (2026-07-09 도출)

## 문제

`mavlink_bridge_app`에 FC 수신 값의 **finite(NaN/Inf) 검증이 없다**.
MAVLink CRC를 통과한 attitude/position 값은 내용 검증 없이 그대로 SB에 게시된다.

- 확인: `mavlink_bridge_app/fsw/src/*.c`에 `isfinite`/`isnan` 사용처 0곳 (2026-07-09 기준)
- 대비: uplink 쪽(viewpoint/route payload)은 범위·finite·버전 검증이 이미 구현되어 있음

## 왜 문제인가

- CRC는 **전송 오류**만 잡는다. FC 자체가 깨진 값을 계산해서 보내면
  (EKF 발산, 센서 고장 등) CRC는 정상 통과한다.
- NaN/Inf attitude가 SB에 게시되면 하류 전파:
  - `cfs_core_app` — 헬스 판단 입력 오염
  - `lora_tdm_app` — `snprintf %.6f`로 `nan`/`inf` 문자열이 다운링크 라인에 실려
    지상국 파서까지 도달
- NaN 특성상 비교 연산이 전부 false → 범위 검사 없는 상태 머신은 조용히 오동작 가능

## 결정 및 구현 (2026-07-13)

**A안(입구 차단) 채택.** `mavlink_bridge_app_utils.c`의 `PublishAttitude`/`PublishEkfLocal`에서
파싱 직후 `MAVLINK_BRIDGE_APP_ValuesFinite6()`로 6개 float 필드 전부 검증 —
하나라도 NaN/Inf면 `MAVLINK_BRIDGE_APP_RecordNonFiniteError()` 호출 후 즉시 return
(SB 게시 자체를 안 함, 기존 TLM 캐시도 그대로 보존).

- 새 카운터: `MAVLINK_BRIDGE_APP_Data.NonFiniteValueCount` (Data struct + HK TLM에 추가)
- 새 EID: `MAVLINK_BRIDGE_APP_NONFINITE_VALUE_ERR_EID` (13)
- `PublishGlobalPositionAsLocal`/`PublishGpsRaw`/`PublishEkfStatus`는 검증 대상에서 제외 —
  이 함수들의 float 필드는 int32/int16 raw 값을 상수로 나눈 결과라 구조적으로
  NaN/Inf가 나올 수 없음 (오버플로 없는 단순 나눗셈)

원래 검토했던 B/C안은 기각:
- B(Valid=0 마킹): 모든 구독자가 Valid를 확인해야 하는 부담이 있고, `lora_tdm_app`의
  다운링크 오염(§왜 문제인가 참조)은 여전히 발생 — 채택 안 함
- C(cfs_core_app에서만 검증): `lora_tdm_app` 다운링크 오염을 못 막음 — 채택 안 함

## 관련 항목

- `tests/TEST_CASES.md` "추가 런타임 시험 후보 — FC 장애/깨진 값" (RT-FC-007~009):
  테스트 후보에서는 이 갭을 **제외**하고 기록함 — 코드(설계) 사안이므로 본 노트로 분리
- EKF 발산 시나리오 자체는 RT-FC-009 (`fault=3 EKF_INVALID`)로 부분 커버되나,
  이는 FC가 스스로 EKF 불량을 보고하는 경우만 해당. 값이 깨졌는데 FC가 정상 보고하면 못 잡음

## 상태

- [x] 설계 방향 결정 (A) — 2026-07-13
- [x] 구현 + coveragetest 추가 (2026-07-13) — `Test_PublishAttitude_NaNRejected`(roll=NaN),
      `Test_PublishEkfLocal_InfRejected`(vz=+Inf), `Test_PublishAttitude_FiniteValuesAccepted`
      (정상값 통과 회귀 확인) 3건. mavlink_bridge_app UT 전체 회귀 없음
      (utils 136, main 14, cmds 4, dispatch 26 PASS).
- [ ] E2E(B)에 PTY로 NaN attitude 주입 테스트 추가 (선택사항 — Unit으로 이미 핵심 로직 커버,
      우선순위 낮음)
