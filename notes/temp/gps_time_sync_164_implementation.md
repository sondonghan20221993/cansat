# §16.4 GPS 시각 반영 — 방향 전환: cFE 코어 대신 DL2 다운링크로 실어보내기 (2026-07-14)

## 배경 (앞선 시도 경위)

당초 §16.4.1(`CFE_TIME_ExternalGPS` 호출)로 cFS 내부 시각을 GPS로 규율하려
했으나 조사 중 두 가지 확인:

1. **cFE 코어 설정 3개 변경 필요** — `CFE_PLATFORM_TIME_CFG_SOURCE`/`SRC_GPS`를
   `true`로, `CFE_MISSION_TIME_CFG_FAKE_TONE`을 `false`로. 앱 코드가 아니라
   cFE Time 서브시스템 전체의 동작 모드 변경.
2. **더 심각한 위험 발견**: 4개 앱(`cfs_core_app`/`mavlink_bridge_app`/
   `uplink_app`/`lora_tdm_app`) 전부의 `GetTimeMs()`가 `CFE_TIME_GetTime()`
   (MET+STCF)을 그대로 씀 — spec이 가정했던 "PSP MONOTONIC이라 STCF에 안
   영향받음"이 실제 코드에선 성립하지 않음. `ExternalGPS` 호출 시 STCF가
   갱신되는 순간 4개 앱의 내부 "지금 시각"이 전부 한 번에 점프 →
   health/timeout 비교식 오작동 → 최악의 경우 GPS 락 잡히는 순간
   `cfs_core_app`이 엉뚱하게 앱을 자동 재시작시킬 위험.

## 결정 (2026-07-14, 사용자 채택) — 방향 전환

cFE 코어 시각 규율은 전면 보류. 대신 **이미 받고 있는 GPS 시각을 DL2 다운링크
프레임에 실어서 지상으로 그�낭 내려보낸다.** cFE 코어 변경 0건, 앱 레벨
배선만 필요 — 리스크 대부분 제거.

**핵심 사실**: DL2 프레임 포맷 자체는 이미 SysTime 확장 블록을 지원하도록
설계돼 있음(spec §4.2, `flags bit0`, `DL2_SYSTIME_BLOCK_LEN=8`). 그런데:
- **C 인코더(`LORA_TDM_APP_BuildDl2Frame`)는 항상 고정 47바이트만 씀** —
  SysTime 블록을 채우는 코드 자체가 없음(주석: "SysTime 확장 블록 미포함,
  flags bit0 항상 0, 기본 47B 고정 길이")
- **`lora_tdm_app`은 `FC_SYS_TIME_MID`(0x1909)를 구독조차 안 함** — GPS 시각이
  `mavlink_bridge_app`에서 발행은 되고 있지만(§16.2/16.3 완료) `lora_tdm_app`이
  받지를 못해서 DL2에 실을 값 자체가 없었음
- 지상측(openMCT `dl2_frame_to_data`)은 `Dl2Frame.sys_time_unix_usec`를
  파싱은 하지만 WS/CSV로 내보내지는 않음(오늘 통합 시 "이번 범위 밖"으로
  명시해둔 부분)

## 구현 범위

### 1. `lora_tdm_app` — FC_SYS_TIME_MID 구독 + 캐시
- `LORA_TDM_APP_FcStateCache_t`(lora_tdm_app.h)에 `uint64 TimeUnixUsec`,
  `uint8 TimeValid` 추가
- `lora_tdm_app.c` Init에 `CFE_SB_SubscribeEx(FC_SYS_TIME_MID_VALUE, ...)` 추가
  (기존 FC 상태 4종 구독과 동일 패턴)
- `LORA_TDM_APP_UpdateCacheFromMsg`에 SYS_TIME 분기 추가 — mavlink_bridge의
  `SysTimeTlm_t`(TimestampMs/Seq/Valid/Stale/ErrorCode/Reserved/TimeUnixUsec)와
  동일 레이아웃의 로컬 struct로 캐스팅해 읽음(기존 4종과 동일 관례)

### 2. `LORA_TDM_APP_BuildDl2Frame` — 가변 길이로 변경
- `FcState.TimeValid`가 참이면: `flags |= DL2_FLAG_SYSTIME`(0x01), 47바이트
  뒤에 `TimeUnixUsec`(u64 LE) 8바이트 추가 → 총 55바이트, 아니면 기존 47바이트
- `LORA_TDM_APP_DL2_FRAME_LEN`을 고정 상수가 아니라 base/max 두 개로 분리하고
  반환값을 실제 쓴 길이로. 호출측(`lora_tdm_app.c`의 RunTx)이 버퍼 크기를
  base+block 기준으로 잡아뒀는지 확인 필요.

### 3. 지상측(openMCT) — 이미 파싱은 되니 노출만
- `dl2_frame_to_data()`에 `sys_time_unix_usec` 필드 추가(있을 때만)
- `_csv_fields`에 `sys_time_unix_usec` 컬럼 추가

## 상태

- [x] 방향 전환 결정 기록 (cFE 코어 보류, DL2 다운링크 방식 채택)
- [x] `LORA_TDM_APP_FcStateCache_t`에 TimeUnixUsec/TimeValid 추가
- [x] `FC_SYS_TIME_MID_VALUE` lora_tdm_app config에 정의 추가(0x1909, mavlink_bridge와 동일값)
- [x] lora_tdm_app.c 구독 추가
- [x] UpdateCacheFromMsg SYS_TIME 분기 추가
- [x] BuildDl2Frame 가변 길이 처리 (base 47B / SysTime 포함 55B) + 호출측
      버퍼(`Dl2Buf`)를 `DL2_MAX_FRAME_LEN`으로 확장 + 버퍼 부족 시 SysTime
      생략 폴백
- [x] C 단위테스트 4건 추가 (SysTime 포함/미포함/폴백/캐시반영) —
      `lora_tdm_app_utils` 114/114 PASS
- [x] 로컬 build-ut 회귀 확인 — lora_tdm_app 전체(40/114/30/12) 전부 PASS
- [x] 지상측(openMCT) `_csv_fields`에 `sys_time_unix_usec` 추가,
      `dl2_frame_to_data()`가 있을 때만 채우도록 반영
- [x] 지상측 테스트 2건 추가(포함/미포함) — 전체 52/52 PASS, 회귀 없음
- [x] `tests/TEST_CASES.md` 카탈로그 반영 — TDM-CACHE-006, TDM-DL2-001~006(신규
      섹션, 기존 3건도 소급 카탈로그화), 런타임 후보 RT-DL2-SYSTIME-001
- [ ] (선택, 실기체 필요) Pi 배포 후 실측 — SysTime 블록 도착 확인 (RT-DL2-SYSTIME-001)

## §16.4.2 — 카메라 GPS 동기 (2026-07-14 결정, 착수 전)

### 결정 — chrony 브릿지 + 검증 방법

**구현**: `SysTime`(DL2로 지상까지 온 것과 별개로, Pi 로컬에서 cFS SB의
`FC_SYS_TIME_MID`를 직접 구독)을 chrony SOCK refclock으로 주입하는 작은
Pi 호스트 유틸리티. `pi_chrony_camera.conf`(카메라→Pi NTP)는 이미 있음 —
Pi 자신의 시계가 GPS 규율된 뒤에야 그게 의미를 가짐.

**검증 방법 — 녹화 영상 대조는 안 됨**: `camera/README.md`에 이미 기록된
별개 버그("OSD가 녹화 파일엔 안 찍힘")때문에, 녹화 mp4로는 시계 동기
여부를 확인할 수 없음. 대신:
- **1차(정확)**: 카메라에 SSH로 `date`/`chronyc tracking` 직접 조회 →
  같은 순간 다운링크 로그(`sys_time_unix_usec`)와 비교. §16.5 오차
  예산(~수십 ms) 안에 드는지 확인.
- **2차(보조, 오차 큼)**: 라이브 화면(영상 파일 아님)에 찍히는 OSD
  타임스탬프를 육안으로 다운링크 로그와 대조. **사람이 화면을 보고
  초 단위로 대조하는 방식이라 오차가 수백ms~초 단위로 클 수 있음** —
  "대략 맞는지"만 확인하는 정성적 체크로 취급, 1차 방법의 대체가 아님.

## 상태 (§16.4.2)

- [x] 방식 결정 — chrony SOCK refclock 브릿지 + 2단계 검증(date/chronyc 1차,
      육안 라이브화면 2차·오차 큼 명시)
- [ ] Pi 호스트 브릿지 유틸리티 구현 (cFS SB 구독 → SOCK refclock 인코딩)
- [ ] chrony 자체 설정(upstream `refclock SOCK`)에 추가 — `pi_chrony_camera.conf`와는
      별개(그건 카메라→Pi 방향)
- [ ] Pi+FC+카메라 실기체 검증 (date/chronyc 대조)
