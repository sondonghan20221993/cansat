# BL-41(CONFIG 부분) 설계 확정 — 운영 중 조정값 재부팅 지속 (2026-07-23)

## 배경
`persistent_state_gap_audit_2026-07-23.md`에서 발견된 8범주 중 route는
`bl41_route_buffer_design_2026-07-23.md`에서 별도 확정. 여기서는
**CONFIG 조정값**(운영 중 CONFIG_CMD_MID로 바꾼 timeout/interval 등)
쪽만 다룸.

## 대상 확인

| 앱 | CONFIG 값 | 필드 수 | 기존 상태파일 |
| --- | --- | --- | --- |
| `cfs_core_app` | `ActiveConfig`(`AttitudeTimeoutMs`/`LocalTimeoutMs`/`GpsTimeoutMs`/`EkfTimeoutMs`/`BridgeTimeoutMs`/`PublishPeriodMs`) | 6 | 있음(`cf/cfs_core_app_state.bin`, BL-39로 상대경로 전환 완료) → **필드 추가** |
| `mavlink_bridge_app` | `ActiveConfig`(`AttitudeIntervalUs`/`LocalPositionIntervalUs`/`GlobalPositionIntervalUs`/`GpsRawIntervalUs`/`EkfStatusIntervalUs`/`ReconnectIntervalMs`/`HeartbeatIntervalMs`) | 7 | **없음** → 신규 생성 |
| `lora_tdm_app` | `UseV2Downlink`(다운링크 프로토콜, 0/1) | 1 | **없음** → 신규 생성 |

(최초 조사 때 mavlink_bridge_app도 기존 파일이 있다고 잘못 판단 —
grep 재확인 결과 SaveState/LoadState 자체가 없음, 정정함.)

## 저장 위치 확정

기존 파일에 필드 추가(cfs_core_app) + 없는 앱은 신규 파일(2026-07-23
확정, "한번에 다 넣자"). 3개 앱 전부 **BL-17/18/19 표준 패턴 재사용**:
- 매직 넘버 + 체크섬으로 파일 손상 감지
- 임시파일 write → fsync → rename (원자적 교체, 쓰다 중단돼도 기존
  파일 안전)
- 파일 없음(첫 부팅/ENOENT) → 조용히 기본값. 매직/체크섬 불일치(손상)
  → ERROR EID 남기고 기본값

## 저장 시점 확정

**CONFIG 적용 성공할 때마다 즉시** (PendingConfig → ActiveConfig 승격
직후, `ConfigGeneration++` 하는 지점과 동일한 곳 — mavlink_bridge_app
`mavlink_bridge_app_utils.c:1925` 부근, cfs_core_app
`cfs_core_app_utils.c:687` 부근, lora_tdm_app
`lora_tdm_app_utils.c` `ProcessConfigCommand`의 `ConfigOk = true` 분기).
주기적 저장(HK 틱마다 등) 안 씀 — 변경 시에만 write.

**추가 발견(2026-07-23, 테스트 작성 중)**: lora_tdm_app의 `UseV2Downlink`는
CONFIG_CMD_MID 외에 **전용 지상 명령 `SET_DL_PROTO_CC`**
(`lora_tdm_app_cmds.c` `LORA_TDM_APP_SetDownlinkProtocol()`)로도 바뀜 —
이 두 번째 변이 지점에도 성공 분기에서 `SaveState()` 호출 필요.
(원래 설계에서 누락돼 있었음, 테스트
`Test_SetDownlinkProtocol_PersistsOnSuccess`가 이 배선을 요구.)

## 추가 검증 여부 — 불필요로 결론

**논의 경과**: 로드한 값에 range/일관성 재검증을 추가할지 고민했으나,
CONFIG_CMD 적용 시점에 이미 `PendingConfig` 후보 검증(range·상호
일관성)을 통과해야만 `ActiveConfig`로 승격됨 — 저장되는 값은 항상
그 검증을 통과한 값이므로, 로드 시 같은 검증을 반복하는 건 중복
(사용자 지적, 2026-07-23). 파일 손상은 체크섬이 이미 잡음.

**최종안**: 표준 패턴(매직+체크섬)에 **`ConfigVersion` 필드 하나만
추가**. CONFIG_CMD_MID 프로토콜에 이미 있는 `ConfigVersion` 상수를
파일에도 같이 저장 — 펌웨어 업데이트로 CONFIG 파라미터 구조(필드
추가/삭제)가 바뀌는 경우에만 의미 있는 방어. 불일치 시 range 재검증이
아니라 **파일 전체를 구버전으로 간주해 기본값 폴백**(바이트를 잘못된
필드로 오해석 방지). 그 외 추가 검증 없음.

## 남은 구현 작업 (미착수)

- [ ] `cfs_core_app_state.bin` 파일 포맷에 `ActiveConfig`(6필드) +
      `ConfigVersion` 필드 추가, `SaveState`/`LoadState` 갱신
- [ ] `mavlink_bridge_app` 신규 상태파일 생성 — BL-17/18/19 패턴
      복제(magic/checksum/fsync+rename/corrupt EID), `ActiveConfig`(7필드)
      + `ConfigVersion` 저장
- [ ] `lora_tdm_app` 신규 상태파일 생성 — 동일 패턴, `UseV2Downlink`(1필드)
      + `ConfigVersion` 저장 (이 앱의 첫 영속 상태)
- [ ] 3개 앱 전부: CONFIG 적용 성공 지점에 `SaveState()` 호출 추가
- [ ] lora_tdm_app: `SET_DL_PROTO_CC` 성공 분기에도 `SaveState()` 호출
      (위 "추가 발견" 참조 — 두 번째 변이 지점)
- [ ] 3개 앱 전부: `Init()`에서 `LoadState()` 호출해 `ActiveConfig` 복원
      (파일 없거나 손상 시 컴파일타임 기본값 유지)
- [ ] 3개 앱 전부: SaveState에 BL-18 패턴(파일 fsync + 부모 디렉터리
      fsync) 적용 — uplink_app과 동일 수준
- [ ] spec 갱신: 각 앱 behavior spec에 CONFIG 지속 동작 명시
- [x] UT 추가 완료(2026-07-23, TDD red — 구현보다 먼저 작성):
      3개 앱 x 13~14케이스, 총 44개. 왕복/truncated/bad-magic/checksum/
      ConfigVersion 불일치/파일 없음/open-error-not-ENOENT/write실패
      (RLIMIT_FSIZE)/rename실패(EISDIR)/CONFIG 배선/Init 복원 배선/
      dir-fsync 2종 + lora_tdm `SET_DL_PROTO_CC` 배선.
      현재 전부 컴파일 실패 상태가 정상(미구현 인터페이스 참조).

## 미결정 (없음)

route/CONFIG 둘 다 이 두 문서로 설계 확정 완료. 구현 착수 시 두
문서의 체크리스트를 그대로 작업 목록으로 사용.
