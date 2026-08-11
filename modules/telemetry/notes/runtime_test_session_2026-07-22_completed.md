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

**직접 원인 재확인(2026-07-23, Pi 재연결 후 실측)**: 당초 "권한 부족"
가설은 **틀림** — `systemctl show cfs.service`로 확인한 결과
`User=root`로 실행 중. 실제 원인은 단순히 `/cf`가 파일시스템 루트에
**존재하지 않아 `ENOENT`** (부모 디렉터리 자체가 없어 open() 실패,
권한과 무관). `WorkingDirectory=/home/sdh2983/cFS_clean/build/exe/cpu1`
아래 `cf/` 디렉터리가 이미 존재하며(`sdh2983` 소유) `EEPROM.DAT`·
`cfe_es_startup.scr` 등 실제 cFS 런타임 파일이 거기 있음 — 이것이
진짜 작동 중인 "cf" 위치. 그 안에 `uplink_app_state.bin`(또는 `.tmp`)
**전무** 확인 — SaveState 무동작 실물 확증. **범위 확대**:
`cfs_core_app_state.bin`도 동일 패턴(`CFS_CORE_APP_STATE_FILE_PATH
"/cf/cfs_core_app_state.bin"`)이라 **cfs_core_app도 동일 결함**
(uplink 단독이 아님). `systemctl show -p Environment`도 확인 →
비어있음(ⓐ env var 미주입 상태). **ⓑ(상대경로 `cf/...`)가 이미
사용 중인 실경로와 정확히 일치** — ⓑ + SaveState 실패 시 EVS 경고로
확정.

**구현 완료(2026-07-23)**: uplink_app/cfs_core_app 둘 다
`STATE_FILE_PATH`를 절대경로→상대경로(`cf/...`)로 변경. SaveState의
open/write/rename 3개 실패 지점 전부에 ERROR EVS(errno 포함) 추가 —
uplink `UPLINK_APP_STATE_SAVE_FAIL_EID`(10), cfs_core
`CFS_CORE_APP_STATE_SAVE_FAIL_EID`(17) 신규. spec §12.1에 BL-39
결함/수정 주석 반영. UT 8/8(uplink_app 4개 스위트+cfs_core_app 2개
스위트) PASS. **미완**: Pi 재배포 후 boot_count 실측 증가 확인은
최종 검증 때 일괄(사용자 지시).

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
## GUI 유도 시험 (2026-07-22 21:17~21:24, 사용자 실조작 + 실시간 모니터)

| # | 시험 | 결과 |
|---|---|---|
| 1 | CONFIG force 없이 (mavlink_bridge heartbeat) | ✅ 예상대로 health gate 차단 (seq=6, UFB=3) |
| 2 | CONFIG force 켜고 | ✅ FORCED THROUGH → routed → exec result OK (seq=7) |
| 3 | CONFIG lora_tdm downlink_protocol=0 (force) | ✅ "protocol set to v1(text)" + exec OK (seq=9) — force 빼먹은 seq=8 차단도 정상 |
| 4 | COUNTER scope=lora_tdm (GUI 패널) | ✅ class=7 routed + ResetCounters 실행 (seq=10) |
| 5 | RECOVERY 드롭다운 PARSER_RESET | ✅ payload 자동구성 → cfs_core → bridge 파서리셋 + exec OK (seq=11) |
| 6 | RECOVERY RESTART_BRIDGE | ❌ **FAIL → BL-40 발견** (seq=12/13, exec FAILED detail=1) |
| + | RECOVERY SERIAL_RECONNECT | ✅ bridge 시리얼 재연결 + exec OK (seq=14) — FcnCode 경로라 BL-40 무관 |
| 7 | ROUTE mission 1wp | ✅ route updated + HK mission_wp=1 (seq=15) |

**전 시험에서 retx=0** — 100ms 타이밍에서도 업링크 첫 슬롯 적중 일관.

## 🔴 신규 결함 3호 — BL-40: 앱 이름 상수 소문자 (RESTART 3종 무동작)

`CFS_CORE_APP_BRIDGE/UPLINK/LORA_APP_NAME`이 소문자 바이너리명 —
cFE 등록명(대문자, startup.scr)과 불일치 → `GetAppIDByName` 항상 실패
→ 지상 RESTART_BRIDGE/UPLINK/LORA 전부 exec FAILED. **BL-38을 고쳐도
자동 재시작이 같은 상수를 써서 어차피 실패했을 2중 결함.** UT는
GetAppIDByName 스텁 SUCCESS라 못 잡음. 상세 BACKLOG BL-40.

## 내일 할 일 (2026-07-23, 사용자 지시로 이월)

- [x] **BL-40 수정 (코드, 2026-07-23)**: 앱 이름 상수 3개 → 등록명 대문자
      완료, UT 4/4 PASS. Pi 재배포 + RESTART 3종 GUI 재시험은 최종 검증
      때 일괄(Pi 미연결, 사용자 지시)
- [ ] (결함 수정 후) BL-38 구현 → RT-CORE-003/004 재시험.
      **설계 확정(2026-07-23 대화)**: A안(재시작 로직을 fault 체인에서
      분리, 각 *TimedOut 독립 검사) + ⓑ ExceptionAction 병행(startup.scr
      7번째 필드 RESTART — 크래시는 ES가 즉시, 소프트 장애는 cfs_core가).
      대안 비교 결과: ⓐ 체인 재정렬=가림 대상만 이동(기각),
      ⓒ HS 앱 도입=4-app 규모에 과함(기각), ⓓ systemd 전체 재시작=과잉
      복구(기각). **세부 설계 최종 확정(2026-07-23, 정본은 spec §11.1)**:
      1) 구조 = a-1 별도 함수 `CheckAppRestarts()` 분리, else-if 체인은
         FaultCode/HealthState 보고 전용으로 순수화
      2) 사이클당 재시작 1건, 고정 우선순위 bridge > uplink > lora
      3) **무한 재시도(MAX_RESTARTS 제거)** — 운용 ≤5분이라 3회 소진 후
         방치 = 실질 기능 상실(특히 uplink는 지상 수동 복구 경로도 자기
         경유). 고정 쿨다운 5초가 빈도 상한. 지수 백오프(짧은 운용에서
         복구만 지연)·우선순위 스왑·타이브레이크(복잡도 대비 이득 없음,
         사이클 1초≪쿨다운 5초라 기아 불가)는 논의 후 기각
      4) 재시도 횟수는 HK 카운터 노출만(관측용) — 카운터 리셋 개념 소멸
      5) ⓑ ExceptionAction — **정정(2026-07-23)**: cFE 정의 재확인 결과
         `0=앱만 재시작`/`Non-Zero=프로세서 전체 리셋`으로 대화 중 설명이
         반대였음. 4개 앱 전부 이미 `0` → **수정 불필요, 이미 적용 중**
         (cfs_core 자신도 포함)
      6) FaultCode 보고 불변 — 최상위 1개 의미 유지, 동시 fault는 기존
         개별 상태 필드로 관측 가능(비트마스크 전환 기각)
      → **설계 전 항목 확정 완료, 구현 착수 가능**

      **구현 완료(2026-07-23)**: `cfs_core_app_utils.c`에
      `CFS_CORE_APP_CheckAppRestarts()` 신설 — fault 체인과 독립 호출,
      사이클당 1건, 고정 우선순위 bridge>uplink>lora, `RestartIssued`
      플래그로 "실제 발행했을 때만" 하위를 건너뛰게 구현(쿨다운 중인
      상위 fault가 계속되면 하위로 폴스루 — UT가 최초 구현에서 이
      기아 버그를 실제로 잡아냄, 수정함). `PublishSystemHealth`
      else-if 체인은 보고 전용으로 순수화. `MAX_RESTARTS` 제거(무한
      재시도), 재시도 카운터는 복구 후에도 보존(관측용, 리셋 없음).
      UT 9개 추가/수정: BL38-UT-1(EKF fault 중 uplink 재시작 회귀 재현,
      어제 실측 FAIL의 정확한 재현) 등. `coverage-` 전체 99/99 PASS.
      **정정 1건**: startup.scr `ExceptionAction` 필드는 실제로
      `0=앱만 재시작`(cFE 공식 정의, 대화 중 반대로 설명한 것은 오류)
      — 4개 앱 전부 이미 `0`이라 **수정 불필요**, spec §11.1에 정정
      반영.
      **미완**: Pi 재배포 + RT-CORE-003/004/005~011 실기 재시험은
      최종 검증 때 일괄(사용자 지시).
- [x] **BL-39 원인 확정 + 수정 완료(2026-07-23)**: Pi 재연결 직접 실측 —
      권한 가설 오류 정정(User=root), 실제 원인 `/cf` 부재로 ENOENT.
      ⓑ(상대경로) 채택, cfs_core_app도 동일 결함 확인해 함께 수정.
      SaveState 실패 EVS 3종 추가(uplink EID10/cfs_core EID17). UT 8/8
      PASS. Pi 재배포 후 BootCount 실측 재검증은 최종 검증 때 일괄
- [ ] **waypoint 조회 기능** — **방향 확정(2026-07-23): a-2(전체
      readback)**. 대역폭 검토 결과 최대 16 waypoint×12바이트=192바이트,
      현재 air rate(2.4KB/s) 기준 <1초로 충분(air rate 상향 불필요,
      최종검증 이후로 연기). DL2 확장 블록(SysTime 블록과 동일 패턴)으로
      여러 사이클에 분할 다운링크하는 방식 — **세부 프레임 설계 착수 필요**
- [ ] v2 표준화: 부팅 기본값 v1→v2 변경 여부(`UseV2Downlink` 초기값,
      코드+spec) — 사용자 "v2로 사용하기로" 언급(2026-07-22), 테스트 종료
      시 v2 복귀 CONFIG 전송도 잊지 말 것
      → **전제 변경(2026-07-23, BL-41)**: `UseV2Downlink` 영속화가
      설계·테스트 확정(`bl41_config_persistence_design_2026-07-23.md`).
      구현되면 지상 1회 v2 전환 후 재부팅에도 유지되므로 "재부팅 시
      v2 복귀 CONFIG 재전송" 불필요해짐. 컴파일타임 기본값 v1→v2 변경
      여부만 별도 결정 사안으로 남음
- [ ] BL-15 최종값 확정(100ms uncommitted) / CI_LAB startup.scr 원복 여부
- [ ] **최종 검증(전수)**: 위 수정들이 끝나면 지상국에서 **모든 CONFIG
      파라미터를 전부 실제로 변경**(mavlink_bridge 7종 + lora_tdm +
      cfs_core는 CLI/HTTP 경유)하고, RECOVERY 6종·COUNTER 4종·ROUTE·
      DIAGNOSTIC까지 **전 명령 전수 실기 검증**을 거친다 — 오늘처럼
      Pi EVS(routed/exec result)와 지상 UFB 양쪽 대조로 판정 (사용자
      지시, 2026-07-22)

## 추가 검토 항목 (2026-07-22 밤, 사용자 제기 — 내일 이후)

- [ ] **권한 검증(§18.11) 설계 재검토**: 지상국이 사실상 master인데
      지상국 스스로 auth level 비트를 채워 보내는 현 구조가 실질 보안
      효과가 있는지 애매함(자기 신고식 — 위조 방어 아님, 실수 방지
      수준). **실제 우주/드론 지상시스템(CCSDS 등)은 어떻게 하는지
      조사** 후 유지/단순화/강화 방향 논의
- [ ] **TDM 패킷 구조 시각화**: v1 ASCII 라인 / v2 DL2 바이너리 프레임
      (seq/boot_count/SysTime 확장블록/UFB 1바이트 포함) 바이트 레이아웃
      다이어그램화 — 가능하면 지상국(openMCT) 또는 문서에 시각 자료로
- [x] **하드웨어 전제 기술 수정 — 재검토 결과 정정 불필요(2026-07-23)**:
      사용자가 제조사(micoair.com) 공식 상품설명 제공 — "air rate
      2.4KB/s(2400 bytes/s)"는 매뉴얼의 "2.4K Byte/s(기본값)"와 정확히
      일치, 단위 혼동 아니었음(원래 표기가 맞음). 상세는
      `lr24f_module_spec_2026-07-23.md` 참조.
      **신규 발견**: air rate가 실제로 **2.4K/4K/8K/20K Byte/s 중
      선택 가능**(부품 교체 불요, 모듈 설정) — 최대 8.3배 상향 여지.
      UART도 921600까지 지원(현재 57600).
      **결정(2026-07-23)**: waypoint readback(a-2) 대역폭 계산 결과
      16개 waypoint(mission route 상한, `CFS_CORE_APP_ROUTE_MAX_WAYPOINTS`)
      × 12바이트(X/Y/Z float) = 192바이트, 현재 air rate 기준 <1초 —
      **a-2에 air rate 상향이 불필요함이 확인됨**. 단점(도달거리/안정성
      저하, BL-15 Stage 4b 타이밍 재튜닝 필요, 2.4GHz 간섭 여지,
      양단 동시설정 실패 시 무선 복구 불가, 최종검증 직전 시점 리스크)
      대비 지금 이득 없어 **최종 전수 검증 이후로 연기**. a-2는 현재
      air rate 기준으로 설계 진행
- [ ] **TEST_CASES.md 통합 세션 메모 stale 정정**: "RT-LORA-001(USB
      런타임 분리)은 serial_reopen_gap 해소 전까지 제외" 문구 —
      **갭은 이미 해소됨**(2026-07-10 구현, fd close→재오픈 경로 +
      2026-07-13 UT). 제외 사유 소멸했으므로 통합 세션 단계에 RT-LORA-001
      재편입하도록 문서 갱신
- [ ] (참고) mavlink_bridge 기존 기능 확인: FC MISSION 조회
      (MISSION_QUERY_CC), FC ARMED 상태 감지 — waypoint 조회 설계 시
      MISSION_QUERY 경로 재사용 가능성 검토
