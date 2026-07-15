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

### 테스트 케이스 설계 (§16.4.2 브릿지 유틸리티, 2026-07-15)

브릿지는 Pi 호스트 프로세스(Python 예상, `bridge/` 관례) — cFS SB를 직접 못
붙으므로 실제 입력 경로 결정 필요(후보: mavlink_bridge가 쓰는 것과 별개로
FC UART을 이중 사용 불가 → **SB 구독 앱이 아니라, mavlink_bridge_app이
SYSTEM_TIME 수신 시 로컬 UDS/파일로 내보내는 tap을 추가**하거나, 기존
`SysTimeTlm` SB를 구독하는 초소형 cFS 앱. 구현 전 결정 항목).

단위 테스트 (Python, `tests/`):
- **TC-CHRONY-001 (SOCK 인코딩)**: 주어진 unix_usec 입력 → chrony SOCK
  refclock 바이너리 샘플(struct: tv_sec/tv_usec/offset/pulse/leap/magic
  0x534f434b) 필드/바이트 정확성.
- **TC-CHRONY-002 (offset 계산)**: offset = GPS시각 − 로컬 CLOCK_REALTIME,
  부호 포함 (로컬이 빠를 때/느릴 때 양쪽).
- **TC-CHRONY-003 (invalid/stale 거부)**: TimeValid=0 또는 수신 정체
  (마지막 샘플 후 N초 경과) 시 샘플 미주입.
- **TC-CHRONY-004 (소켓 부재 폴백)**: chrony SOCK 경로 없음/연결 거부 시
  크래시 없이 재시도.
- **TC-CHRONY-005 (지연 보정 한계 명시)**: FC→Pi 전달 지연(UART+파싱)만큼
  offset이 편향됨 — 보정 상수 적용 여부와 오차 예산(§16.5 수십 ms) 내
  판정 테스트.

런타임 (하드웨어, TEST_CASES.md 등재용):
- **RT-CHRONY-001**: Pi에서 `chronyc sources`에 SOCK refclock 표시 + reach>0.
- **RT-CHRONY-002**: Pi `date` vs 지상 CSV `sys_time_unix_usec` 오차 예산 내.
- **RT-CHRONY-003**: 카메라(`chronyc tracking`)가 Pi 경유로 GPS 시각 수렴.

### 대조(correlation) 방식 재검토 — 카메라 시계 규율 없이도 가능할 수 있음 (2026-07-15)

**질문 제기**: "다운링크 로그가 이미 GPS 기준값인데, 카메라 시계까지 GPS로
맞출 필요가 있나?" — 목적이 "다운링크 로그와 영상 프레임 사후 대조"라면
카메라 자체 시계 규율(chrony 브릿지)은 불필요할 수 있음.

- 다운링크 로그(`sys_time_unix_usec`)는 이미 GPS 기준 절대시각 — 정확.
- 카메라 녹화 파일은 카메라 로컬시계 기준 상대시각만 있고 GPS와 무관.
- **필요한 건 전면적 시계 규율이 아니라 "녹화 시작 시점 ↔ GPS 절대시각" 1회성
  앵커**: 녹화 시작 이벤트를 GPS 절대시각과 한 번만 매칭하면, 이후 프레임은
  `앵커 + 프레임순번×(1/fps)`로 GPS 기준 절대시각 역산 가능.
- `camera/README.md`에 이미 유사 패턴 존재: fpv4win 녹화(`<epoch_ms>.mp4`
  파일명)와 openMCT CSV(`timestamp` ISO)를 PC 공통시계 기준으로 사후 매칭
  (`correlate_video_telemetry.py`). 다만 이건 "PC 시계가 이미 맞다"는 전제.
- **msposd `--subtitle` sidecar 발견(같은 날, 미검증)**: SD카드 로컬 녹화도
  `.srt`/`.osd` 사이드카로 프레임별 타임스탬프 별도 기록 가능 — 이게 카메라
  로컬시계 기준이라도, 위 "1회성 앵커" 방식과 결합하면 chrony 없이 대조 가능.

**결론(잠정)**: OSD 화면 자체에 GPS 절대시각을 "보여줘야" 하는 요구가 아니라면
(§16.4.2 원래 목적이 그것이었음 — 재확인 필요), 사후 대조 목적으로는
chrony 브릿지 전체가 불필요하고 "녹화시작 앵커 1회 매칭" 방식으로 대체 가능.
→ 사용자에게 목적 재확인 후 §16.4.2 진행 여부/축소 결정.

### 대안 B — FC→MSP DisplayPort→카메라 직결 경로에 GPS 시각 직접 주입 (2026-07-15 제안, 미결정)

**핵심 아이디어**: 사후 대조(앵커/트리거)도, 카메라 시계 규율(chrony)도 필요
없이, **각 비디오 프레임에 GPS 시각 값 자체를 실어버림**. FC↔카메라는
이미 UART(MSP DisplayPort) 직결(Pi/cFS 경유 안 함, `camera/README.md`
목표구성 참조) — 이 스트림에 GPS 시각 필드를 얹으면 msposd가 OSD로
찍거나 `--subtitle` sidecar(`.srt`)에 프레임별 기록 가능.

**아직 결정 아님 — 대안 A(chrony 브릿지)와 경합 중, 조사 후 택일.**

#### 구현 시 필요 항목 (조사 우선, 미착수)

1. **[조사, 최우선] MSP DisplayPort 프로토콜에 GPS 시각(epoch) 필드 존재
   여부** — ArduPilot/INAV MSP OSD 스펙(`MSP_RAW_GPS` 등)엔 GPS lat/lon/fix는
   있어도 GPS 절대시각 필드는 표준에 없을 가능성 높음.
2. **[조사, 최우선] FC가 실제 PX4(MicoAir743v2)인데 현재 설정
   (`ardupilot_msp_osd.param`)은 ArduPilot 기준** — PX4가 MSP DisplayPort
   OSD 자체를 지원하는지, 지원 시 커스텀 필드 삽입 여지가 있는지 재검증
   필요(기존 미해결 항목, `README.md`에 이미 명시).
3. **[조사] msposd 소스에서 수신 필드 중 뭘 sidecar/OSD로 노출하는지 확인.**
4. **분기 A (표준 필드에 시각 없음 — 가능성 높음)**:
   - FC 펌웨어(PX4) MSP 응답 커스텀 확장 필요 → 이 저장소 범위 밖(펌웨어
     소스) 작업일 가능성 큼
   - 또는 Pi가 기존 external OSD 포트(`127.0.0.1:14551`)를 통해 카메라에
     GPS 값을 주입하는 방식으로 재설계 → 결국 Pi가 다시 개입(대안 A와
     유사 위치로 회귀, chrony 대신 MSP 프레임 주입이라는 차이만 남음)
   - 이 경우 Pi 입력 경로는 §16.4.2 대안 A와 동일 문제(SB 구독 방법 결정)
     여전히 남음
5. **분기 B (표준 필드에 시각 있음 — 가능성 낮음)**:
   - FC측 설정만으로 msposd 수신 시각 필드 활성화
   - `--subtitle` 옵션 적용 + 재부팅 자동시작 훅(기존 미해결 TODO, msposd
     프로세스 영속화 문제와 동일 카테고리)
   - 하드웨어 검증: sidecar `.srt` 시각 vs 다운링크 로그 대조
6. **공통**: 1~3번 조사 결과로 분기 A/B 중 확정 후 세부 구현.

**다음 액션**: 1~2번(MSP 프로토콜/PX4 지원 여부) 웹 리서치부터 — 아직 미착수.

#### 조사 결과 (2026-07-15) — 분기 확정, 둘 다 저장소 범위 밖

- **PX4는 MSP DisplayPort OSD 자체는 지원**(`msp_osd.cpp` 드라이버 존재,
  `osd_rtc_datetime_pos`라는 RTC/시각 OSD 요소 파라미터도 이미 존재) — 그러나
  이 요소가 뜨려면 **`MSP_SET_RTC` 명령을 GCS/컴패니언이 보내 PX4의 RTC를
  세팅**해줘야 함. PX4 쪽에 `MSP_SET_RTC` **수신 처리가 구현 안 되어 있음**
  (드라이버가 그 메시지 타입을 처리하는 코드가 없음) → 실제로 쓰려면
  **PX4-Autopilot 펌웨어 소스를 패치(msp_osd.cpp에 MSP_SET_RTC 수신 구현)
  + 포크·빌드·플래시 필요** — `cfs-telemetry-app` 저장소 범위 밖.
- **msposd(카메라)를 포크해 Pi가 별도 채널(기존 external OSD UDP 포트
  14551)로 GPS 시각을 직접 주입하는 경로**도 가능하지만, 이러면 **Pi가
  다시 개입**하는 구조가 되어 대안 A(chrony 브릿지)와 **입력 경로 문제가
  사실상 동일한 위치로 회귀**(SB 구독 방법 결정 필요, 이번엔 chrony 대신
  msposd 포크가 얹힘 — 오히려 카메라 쪽 커스텀 펌웨어까지 추가로 필요해
  범위가 더 커짐).

**결론**: 대안 B의 두 분기 모두 저장소 범위 밖(펌웨어 포크·빌드·플래시,
또는 카메라 펌웨어(msposd) 포크)이라 **대안 A보다 나은 선택지가 아님**.
오히려 대안 A보다 범위가 큼. → 대안 B는 폐기 권장.

**남은 선택지 정리**:
1. 대안 A(chrony 브릿지, §16.4.2 원안) 그대로 진행 — Pi 입력 경로만 결정하면 됨
2. §16.4.2 전체 폐기, **기존에 이미 동작하는 fpv4win 지상측 사후 매칭**
   (`camera/README.md` "시각 매칭 방식 채택", `correlate_video_telemetry.py`)
   만으로 충분하다고 보고 종료 — 추가 구현 0, 단 카메라 SD카드 자체 녹화
   (fpv4win 수신 화질보다 나음)는 대조 불가한 채로 남음

**미결정** — 사용자 판단 필요.

### 최종 정리 (2026-07-15) — 실제 목적 확인 결과: 이미 해결됨

**사용자 목적 확정**: "카메라 Wi-Fi로 수신한 영상에 타임스탬프를 붙여
텔레메트리와 매칭" — 즉 **Wi-Fi(WFB-ng)→지상 fpv4win 수신 영상**의 대조가
목적이지, 카메라 SD카드 자체 녹화나 OSD 실시간 표시가 아님.

**이 목적은 이미 해결되어 있음** (2026-07-13, `camera/README.md` "시각 매칭
방식 채택" 절):
- 영상: fpv4win 녹화 파일명 = `<epoch_ms>.mp4` (지상 PC 시계 기준)
- 텔레메트리: openMCT CSV `timestamp`(같은 PC 시계) + GPS 기준
  `sys_time_unix_usec`(§16.4 메인, DL2)
- 매칭 도구: `camera/correlate_video_telemetry.py` (이미 존재)
- 같은 PC 시계에서 양쪽이 찍히므로 앵커/chrony/cFS 시각 규율 전부 불필요

**전제 (사용자 확인함)**: fpv4win과 지상국 서버(`fc_serial_ws_server.py`)가
**같은 PC**에서 실행되어야 함. 분리 운용 시 두 PC 간 NTP 동기 필요
(README TODO(bench)).

**§16.4.2 처리**: 대안 A(chrony 브릿지)·대안 C(앵커+역산)는 "SD카드 자체
녹화 대조"나 "OSD 실시간 절대시각 표시"가 필요해질 때만 유효한 보류
옵션으로 남김. 현재 목적(Wi-Fi 수신 영상 매칭)에는 **추가 구현 0건** —
§16.4.2 착수 안 함.

## 상태 (§16.4.2)

- [x] 방식 결정 — chrony SOCK refclock 브릿지 + 2단계 검증(date/chronyc 1차,
      육안 라이브화면 2차·오차 큼 명시)
- [x] 목적 재확인(2026-07-15) → **착수 불필요로 종결** — 실제 목적(Wi-Fi 수신
      영상↔텔레메트리 매칭)은 기존 fpv4win 동일-PC 매칭으로 이미 해결.
      아래 항목은 SD카드 녹화 대조/OSD 절대시각 표시가 필요해질 때만 재개:
- [ ] (보류) Pi 호스트 브릿지 유틸리티 구현 (cFS SB 구독 → SOCK refclock 인코딩)
- [ ] (보류) chrony 자체 설정(upstream `refclock SOCK`)에 추가 — `pi_chrony_camera.conf`와는
      별개(그건 카메라→Pi 방향)
- [ ] (보류) Pi+FC+카메라 실기체 검증 (date/chronyc 대조)
