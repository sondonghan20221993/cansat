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

## 검토할 설계 선택지 (미결정)

| 안 | 내용 | 장단점 |
|---|---|---|
| A. mavlink_bridge에서 게시 전 검증 | 파싱 직후 `isfinite()` 실패 시 해당 TLM 미게시 + `ParseErrorCount` 또는 별도 카운터 증가 | 입구 차단(하류 전부 보호) / FC 상태 파악 정보 손실 |
| B. 게시하되 `Valid=0` 마킹 | TLM의 기존 `Valid`/`ErrorCode` 필드 활용 | 정보 보존, 구독자가 판단 / 모든 구독자가 Valid 확인해야 함 |
| C. cfs_core_app에서만 검증 | 헬스 판단 입력에서 거부 (미래 타임스탬프 거부와 같은 위치) | 변경 최소 / lora_tdm 다운링크는 여전히 오염 |

## 관련 항목

- `tests/TEST_CASES.md` "추가 런타임 시험 후보 — FC 장애/깨진 값" (RT-FC-007~009):
  테스트 후보에서는 이 갭을 **제외**하고 기록함 — 코드(설계) 사안이므로 본 노트로 분리
- EKF 발산 시나리오 자체는 RT-FC-009 (`fault=3 EKF_INVALID`)로 부분 커버되나,
  이는 FC가 스스로 EKF 불량을 보고하는 경우만 해당. 값이 깨졌는데 FC가 정상 보고하면 못 잡음

## 상태

- [ ] 설계 방향 결정 (A/B/C)
- [ ] 결정 후 구현 + coveragetest 추가 (NaN/Inf 주입 케이스)
- [ ] E2E(B)에 PTY로 NaN attitude 주입 테스트 추가
