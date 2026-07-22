# Pi 런타임 테스트 세션 (2026-07-22 저녁)

환경: Pi 192.168.50.65 (cfs.service, PID 갱신됨), LoRa 동글 양측 연결,
Windows 지상국(fc_serial_ws_server.py, 127.0.0.1:8082) 가동, GPS 없음(실내,
`fault_code=3 EKF_INVALID` 상시 → health DEGRADED).
빌드: 이번 세션 전체 변경분(counter mgmt/RETX_IDX/P1-a/BL-17 등) + BL-15
Stage 4b 타이밍(100ms/50ms/50).

## 완료된 실기 검증

| # | 항목 | 결과 |
|---|---|---|
| 1 | counter management(class 7) 4개 scope 전부 | ✅ PASS — 라우팅+대상앱 실행(`ResetCounters`/`RESET` EVS), 전부 retx=0 |
| 2 | BL-14 RETX_IDX | ✅ 동작 — `routed uplink ... retx=0` 로그 확인 |
| 3 | v2 전환 미반영 의심 | ✅ **해결** — 원인=health gate(DEGRADED)가 CONFIG 차단. `force:true` → v2(DL2) 전환+ACK2 양방향 확인 |
| 4 | BL-08 EXEC_RESULT | ✅ 실기 동작 — `exec result seq=6 generic=0` |
| 5 | BL-15 Stage 4b(100ms) | ✅ PASS — 5분 soak 손실 0.00%(2991/2991), 9.97pkt/s, 업링크 정상 |
| 6 | P1-a PARSER_RESET E2E | ✅ PASS — 지상→uplink→cfs_core→bridge 실제 파서 리셋 |
| 7 | RT-CORE-003 (uplink_app 자동 재시작) | ❌ **FAIL → BL-38 결함 발견** (아래) |

## RT-CORE-003 FAIL 상세 (BL-38)

- 준비 과정 이슈 2건(테스트 인프라, 수정 완료):
  - `runtime_app_restart_test.sh`가 바이너리명(`uplink_app`)을 보냈으나
    cFE 등록명은 `UPLINK_APP` — `GetAppIDByName failed RC=0xC4000002`.
    스크립트에 `CFE_APP_NAME` 매핑 추가(로컬 수정, 커밋 예정)
  - Pi startup.scr에 CI_LAB 미탑재 → cmd_send(UDP 1234) 수신 불가.
    벤치용으로 추가(`~/cfe_es_startup.scr.bak` 백업, STOP_APP류는 지상
    LoRa 명령셋에 의도적으로 없어 CI_LAB 경유가 유일한 주입 경로)
- 본 결함: STOP_APP으로 uplink_app 정지(20:58:39 Stop Completed) 후
  cfs_core_app 재시작 시도 이벤트(EID 15) **전무**, `Msg Limit Err
  (0x1904→UPLINK_CMD)` 지속. 원인은 fault 체인 종속(BL-38/spec §11.1).
  복구는 cfs.service 수동 재시작으로 수행.
- 파생 관찰: uplink_app 사망 중엔 지상 RECOVERY(RESTART_BRIDGE) 명령도
  무반응(업링크 명령 경로 자체가 uplink_app 경유) — 사용자가 GUI에서
  체감. "텔레메트리는 계속 내려옴"은 정상(다운링크는 lora_tdm 담당).

## 남은 순서 (사용자 지시: 기록 후 순서대로 전부 시도)

- [x] D-1 lora_tdm_app 재시작 실측(21:06) — **예상대로 FAIL**: STOP 후
      LORA_RESTART_EID(16) 미발동, BL-38 동일 결함 확증(EKF 분기가 Lora
      분기보다 상위). cfs.service 수동 복구
## 🔴 신규 결함 2호 — BL-39: uplink_app 영속 상태가 Pi에서 무동작

RT-DL2-SYSTIME-001 확인 중 CSV `uplink_boot_count`가 재시작 4회+ 후에도
`1`인 것으로 발견(2026-07-22 21:10).

- 원인: `UPLINK_APP_STATE_FILE_PATH = "/cf/uplink_app_state.bin"`을
  **POSIX `open()`에 리터럴로** 사용 — `/cf`는 OSAL 가상 경로 관행일 뿐
  Pi 실파일시스템에 `/cf` 디렉터리 없음(실제는
  `~/cFS_clean/build/exe/cpu1/cf/`). `SaveState()`는 open 실패 시
  이벤트 없이 조용히 return → **BootCount/LastAcceptedSequence 영속화
  전체가 실기에서 한 번도 동작한 적 없음** (BL-12/BL-03 지상 재부팅
  감지 실질 무효, BL-18 fsync도 무의미했음)
- 가림 요인: LoadState ENOENT 침묵(BL-17 결정) + SaveState 무이벤트
- 수정 후보(미결정): ⓐ cfs.service에 `Environment=UPLINK_APP_STATE_FILE_PATH=...`
  (기존 env 주입 경로 재사용, 코드 무변경) ⓑ 기본 매크로를 상대경로
  `cf/uplink_app_state.bin`으로(WorkingDirectory 기준) ⓒ OS_OpenCreate
  (OSAL 가상 경로) 전환 — 제일 정석이나 수정 범위 큼.
  + SaveState open/write 실패 시 1회성 EVS 경고 추가 검토

## D-2 진행 (2026-07-22)

- [x] TDM-RT-001 (다운링크 주기): ✅ Stage 4b soak가 상회 충족(100ms,
      5분, 손실 0, seq 단조)
- [x] TDM-RT-002 (ACK→CONNECTED): ✅ `link restored (→1)` 관측, ACK 흐름 확인
- [x] TDM-RT-003 (no-ACK→DEGRADED): ✅ 20:33:09 `link degraded (1 -> 2)` 관측
- [x] TDM-RT-004 (→DISCONNECTED): ✅ 20:58:49 `link lost (1 -> 0)` 관측
- [x] TDM-RT-005 (ACK 재개→CONNECTED): ✅ 20:59:05 `link restored (0 -> 1)` 관측
- [x] TDM-RT-006 (UP 프레임→라우팅): ✅ counter/config/recovery 다수
      `routed uplink` 관측
- [ ] TDM-RT-007 (CRC 변조→UFB=1): 지상 서버가 유효 프레임만 생성 —
      변조 주입 도구 필요, 미실행
- [ ] TDM-RT-008 (RX 윈도우 외 무시): 수동 타이밍 주입 곤란, 미실행
- [ ] TDM-RT-009 / RT-LORA-001 (USB 분리/미연결 시작): 물리 조작 필요
      (사용자 손)
- [x] RT-LORA-004 (링크 전이 EVS): ✅ 위 003~005와 동일 증거로 충족
- [x] RT-DL2-SYSTIME-001: 🔶 **부분** — v2 전환 후 DL2 수신·디코딩·ACK2
      정상, `uplink_last_seq=5` 에코(BL-03) 확인. `sys_time_unix_usec`은
      GPS 없어 빈 값(설계상 정상) — **완전 판정은 실외 GPS 락 필요**.
      단, 이 과정에서 BL-39 발견
- [ ] D-3: 통합 순차 세션 7단계 (TEST_CASES.md 1008행)
- [x] D-3: **현 환경 실행 불가 판정** — 사전조건 `health=NOMINAL`이
      실내(GPS 없음, fault=3 상시)에서 달성 불가 + 단계 3/4 판정
      기준(fault=6/7 전이)은 BL-38 결함으로 관측 불가. **실외 GPS +
      BL-38 수정 선행 필요**
- [x] BL-22: **유지 결론(권고)** — FC가 `/dev/serial0`(PL011 ttyAMA0)
      **921600** 보레이트 사용 중. PL011 고속 보레이트는 UART 클럭
      상향(48MHz)이 필요한 구성이라 "기각된 가설의 잔재"가 아니라 현
      FC 링크가 의존할 가능성 높음. 제거 이득 없음 + 회귀 위험만 존재
- [x] BL-35: ✅ **정량 실측 완료** — 20:40 이후 mismatch 18,237건 분석
      (재시작 경계 오염 제거, 0<lag<30): **lag 3~6 프레임 집중, 평균
      4.36** (분포: 3=4272, 4=5196, 5=4838, 6=3085). 100ms 주기 기준
      왕복 ~440ms의 **체계적 파이프라인 지연**(LoRa 양방향 airtime
      57600 + 모듈 버퍼링 + 호스트 처리)이며 데싱크/유실 아님. ACK는
      매 사이클 도착하므로 링크 판정(NoAckCount)에는 영향 없음 —
      mismatch 이벤트는 사실상 로그 노이즈. 후속 검토(선택): 허용
      윈도우 도입 또는 이벤트 억제/집계
- [ ] (결함 수정 후) BL-38 A안 구현 → RT-CORE-003/004 재시험
- [ ] BL-39 수정 방향 결정(ⓐ/ⓑ/ⓒ) 후 구현 → BootCount 영속 재검증
