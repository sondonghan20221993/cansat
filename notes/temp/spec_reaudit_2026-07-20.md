# spec ↔ 코드 재감사 (2026-07-20) — 결과 요약 및 정정 진행

상세 표는 `notes/spec_code_audit.md` "재감사 (2026-07-20)" 패스 1~4 참조.
기준: 코드 = 진실. 값 계약(MID/CC/상수값)은 전 앱 정합 — 불일치 12건 전부
"코드가 spec보다 앞선" 스테일(기능 추가 시 spec 동반 갱신 누락).

## 정정 체크리스트 (app별)

- [x] 1. `mavlink_bridge_app_behavior_spec.md`
  - R1-1: EID 13 `NONFINITE_VALUE_ERR_EID` + HK `NonFiniteValueCount` 문서화
  - R1-2: §16.5 "UART 115200" → 57600
  - R1-3(⚠️): FC 상태 구조체 shared_msgs 병합 한 줄 언급
  - R1-4(⚠️): internal_cfg 타이밍/CONFIG 한도 상수 표 추가
- [x] 2. `cfs_core_app_behavior_spec.md`
  - R2-1: uplink/lora "자동 재시작 없음" → bridge 동일 패턴 재시작(5s/3회, EID 15/16)
  - R2-2: §17 RECOVERY 4-action/MODE 전이검증 "미구현" → 구현 반영
  - R2-3(⚠️): §7.2 shared_msgs 단일 진실 + NonFiniteValueCount
- [x] 3. `lora_tdm_app_behavior_spec.md` + `lora_protocol_v2_spec.md`
  - R3-1: Stage 3 타이밍 200/100/15 전 구간 반영
  - R3-2: SEQ_FAIL/UFB_SEQ_FAIL "미구현" → 구현 반영
  - R3-3: §4 구독 표에 0x1909/0x190A 행 추가
  - R3-4: protocol v2 헤더 "코드 미구현" → 구현됨
- [x] 4. `mission_app_runtime_spec.md`
  - R4-1: §5.1.1에 `FC_SYS_TIME_MID 0x1909` 행 추가 (+ mavlink spec 540행 주석 해소)
  - R4-2: 0x08A0·SYSTEM_HEALTH 행 구독자에 uplink_app 추가
  - R4-3: §4 lora_tdm 구독 목록에 0x1909/0x190A 추가
  - R4-4: cfs_core의 UPLINK/LORA HK 생존감시·자동 재시작 반영 (§5.1.1, §11)
  - R4-5(⚠️): lora_fc_downlink_app 현재형 서술 → 삭제됨 표기

## 테스트 정합 패스 (2026-07-20 추가)

실행 검증: cFS UT 16/16 PASS(mavlink 4종 포함, `build-ut` 재빌드), pytest 186/186 PASS.
신규 기능(재시작/RECOVERY/MODE/SEQ_FAIL/DL2) 단위테스트는 코드와 동행 — 문서만 스테일.

`tests/TEST_CASES.md` 정정:
- [x] TDM-RX-004 SEQ_FAIL "미구현" → ✓ (A3 C.1/C.2)
- [x] "mavlink_bridge_app unit-test 디렉터리 미구성" → 해소됨 (4종 등록·PASS)
- [x] e2e 3파일 "미구현" → 구현됨(--cfs 게이트/skip, 4+10+5건)
- [x] RT-LORA-003의 "EID 12 미구현 갭" → 구현 완료로 정정
- [x] TDM-RT-001/006 타이밍: 1초→200ms, RX 300ms→100ms (Stage 3)
- [x] lora_fc_downlink_app 테스트 "이력 참고용 유지" → 파일 삭제됨(7c080f1) 명시

잔여(하드웨어 필요, 문서상 정확): RT-CORE-003/004 재시작 런타임 실측,
TDM-RT-* 실물 LoRa, B그룹 e2e는 cFS 빌드+--cfs로 실행 가능(미실행).

## 통합 패스 (패스 5, 2026-07-20 추가)

cross-app 와이어 레이아웃 전수 대조 (shared_msgs 병합 제외분) — **어긋난 레이아웃 0건**.
상세는 spec_code_audit.md 패스 5 표.

후속 후보 (코드 작업, 미착수):
- [ ] 라우팅 명령 5종(RECOVERY/MODE/VIEWPOINT/DIAGNOSTIC/UplinkFwd) 중복 정의 →
      shared_msgs 병합 (BridgeHkMirror 버그와 동일 패턴의 잠재 리스크 제거)
- [ ] uplink `SysHealthMirror_t`(prefix 미러) → shared_msgs/system_health_msg.h typedef 교체
