# 지속 상태(persistent state) 갭 감사 (2026-07-23)

## 발견 경위

Pi가 원인 불명으로 재부팅됨(16:36 KST, 커널 로그상 USB/WiFi/GPU 드라이버
재초기화 확인 — 하드웨어 리부팅). `cfs_core_app`의 `MissionRoute` 캐시(RAM
only)가 날아가 waypoint readback 실기 검증 도중 `wp_count=0`으로 확인.
재부팅 원인은 journald가 비영구 저장(RAM only)이라 **추적 불가** —
`/var/log/journal` 생성 + `systemctl restart systemd-journald`로 영구
저장 전환 완료(2026-07-23), 다음 재발 시엔 원인 추적 가능.

## CDS(Critical Data Store) 사용 불가 확인

cFE의 CDS는 소프트 리셋에서 보존되도록 설계됐으나, 이번 재부팅 로그에서
`CFE_PSP: Starting the cFE with a POWER ON reset.` 직후
`CFE_PSP: Clearing out CFE CDS Shared memory segment.`가 명시적으로
찍힘 — **POWER ON RESET(전원 재인가로 간주되는 케이스)에서는 CDS도
같이 초기화됨.** 이번처럼 Pi가 재부팅되는 시나리오에서는 CDS를 써도
똑같이 날아갔을 것 — CDS는 이번 문제의 해법이 아님.

## spec §12 재확인 — 파일 기반이 맞는 설계

spec §12(699~714행)는 "Pi/cFS 호스트 하드 부팅 또는 전원 주기"에서도
"검증된 지속 상태 및 기본 안전 구성만 복원"돼야 한다고 명시하고,
저장소 백엔드로 **"Raspberry Pi 파일 시스템에 record 단위로 저장"**을
지정한다(현재 BL-39로 uplink_app/cfs_core_app이 쓰는 방식과 일치,
CDS 아님). 즉 설계 방향은 맞고, **실제로 무엇을 저장 대상에 넣을지가
비어있는 상태**.

## 갭 감사 — spec 후보 값 8범주 vs 현재 구현

| 범주 | spec 후보 값(§12) | 현재 구현 |
| --- | --- | --- |
| 부팅/오류 | 마지막 재설정 이유, 부팅 횟수, Pi/cFS 호스트 재설정 횟수, watchdog 재설정 횟수, watchdog 표시, 재부팅 루프 창 시작 타임스탬프 | `uplink_app`: `BootCount`만(`uplink_app_state.bin`). 나머지 전부 미구현 |
| 앱 상태 | 앱 상태, 재시작 횟수, 마지막 오류 코드 | **미구현** — BL-38(2026-07-23)에서 재시작 카운터를 "HK 노출만, RAM"으로 설계해 파일 저장 안 함(재부팅 시 0으로 리셋되지만 무한 재시도 구조라 치명적이진 않음) |
| 하드웨어 상태 | 센서 상태, 마지막 하드웨어 오류 코드, 복구 시도 횟수 | 미구현 |
| 임무 상태 | 임무 단계, 활성 cFS 상태, 성능저하/복구/최소보고 항목 | `cfs_core_app`: `LastHealthState`만(`cfs_core_app_state.bin`) |
| 항해 | 마지막 유효 GPS, 마지막 유효 EKF, 마지막 유효 타임스탬프 | 미구현 |
| 텔레메트리 | 마지막 링크 상태, 마지막 양호 접촉 시간, 활성 전송 ID | 미구현 |
| **구성** | 운영자가 수정한 구성 버전 및 검증된 테이블 버전 | ✅ **구현 완료(2026-07-23, BL-41)** — cfs_core(6필드)/mavlink_bridge(7필드)/lora_tdm(UseV2Downlink) 전부 CONFIG 적용 성공 시 영속화+Init 복원, UT 16/16 green (runtime spec §12.2) |
| 회복 | 보류 중인 복구 작업, 마지막 복구 결과 | 미구현 |
| (표에 없음) | **mission route(waypoint) 데이터** — "임무 상태"에 가장 가까우나 spec 표에 명시적 항목 없음 | 미구현(오늘 발견의 직접 원인) |

## 결론 — 추후 작업목록으로만 등재(구현은 보류, 사용자 지시)

거의 모든 범주가 비어있어 route 하나만 추가하는 게 아니라 **"무엇을
살릴지" 우선순위 정의가 먼저** 필요하다는 사용자 판단. 이번엔 기록만
하고 구현은 다음 세션으로 이월.

**다음 세션 시작 시 논의할 것**: 위 8범주 중 이 프로젝트 현재 단계에서
실질적으로 중요한 것부터 우선순위 선정 → 단일 통합 구조(범용
persistent state 스키마)로 갈지, 앱별로 필요한 것만 개별 추가할지 결정
→ 구현.

**후보 추천(제 의견, 확정 아님)**: CONFIG(운영 중 조정한 파라미터)와
mission route가 체감 영향이 제일 큼 — CONFIG는 재부팅마다 튜닝값이
날아가는 게 반복 작업 부담이고, route는 오늘 실측으로 이미 실패
사례가 나옴.

## 후속 (2026-07-23 같은 날 진행)

위 추천 2건이 그대로 채택되어 **구현까지 완료**:
- CONFIG → ✅ `bl41_config_persistence_design_2026-07-23_completed.md` (UT 44개, 커밋 `c33357c`)
- route → ✅ `bl41_route_buffer_design_2026-07-23.md` (FC 진실원본 + RAM 버퍼 +
  0x1914 readback, UT 15개, 커밋 `07c622d`)

## 나머지 6범주 우선순위 결정 (2026-07-23 사용자 확정)

| 범주 | 결정 | 사유 |
| --- | --- | --- |
| 1. 부팅/오류 | ✅ **구현** | 부팅 카운터 + 재부팅 루프 감지 창(WindowStartMs/BootsInWindow) + LastResetReason. **보고만** — 루프 감지 시 기체 동작 변경 없이 HK/다운링크 플래그 노출, 대응은 지상국 판단. watchdog 카운터는 watchdog 설정 실체 확인 후 |
| 2. 앱 상태 | ✅ **구현** | 재시작 카운터 3종(bridge/uplink/lora) + LastFaultCode 영속화 — BL-38 RAM 카운터에 저장/복원 배선만 추가. Init LoadState로 자동 복원(누적 지속), RESET_COUNTER 명령 시에도 SaveState 동기화 |
| 3. 하드웨어 | ❌ 제외 | 저장할 원천(HW 오류 분류/복구 로직)이 코드에 없음 — 빈 필드만 생김. SD카드 감시 논의 있었으나("고장난 SD에 SD 고장을 기록"하는 자기모순) 함께 제외 |
| 4. 항해 | ❌ 제외 | FC를 진실원본으로 신뢰(route와 동일 논리) — FC 생존 시 몇 초 내 실데이터 재수신, 지상국도 다운링크 로그로 마지막 위치 보유 |
| 5. 텔레메트리 | ❌ 제외 | 정보 중복 — 링크 단절 시각은 지상국 로그(상대편이 이미 앎) + journald 영구화(EVS 전이 이벤트 잔존)로 복원 가능. 1·2번과 달리 소실돼도 새로 알게 되는 것이 없음 |
| 6. 회복 | ❌ 제외 | 현재 복구 동작이 전부 원자적(RestartApp/PARSER_RESET/SERIAL_RECONNECT — 호출=완료, 중간 상태 없음) — "이어할 작업"이 존재하지 않고, 실패 시 BL-38 무한 재시도가 재발동. 다단계 복구가 생기면 재검토 |

→ 다음 작업: 1·2번 SDD(spec 정의) → TDD → 구현. 이 문서의 갭 감사는
이것으로 **전 범주 처분 완료** — 1·2번 구현 끝나면 이 문서도 completed 이관.
