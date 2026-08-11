# uplink_app 완료 이력 모음

## uplink_app / lora_tdm_app 역할 정의 + self-config 현황


## 역할 분리

### lora_tdm_app = 링크 수신/중계 (ProcessUpFrame)

**입력**: LoRa serial (텍스트 프레임)
```
UP,version,class,seq,flags,payload_hex,crc
예: UP,1,2,100,0,DEADBEEF,A1B2
```

**처리**:
1. 전체 프레임 CRC-16/CCITT-FALSE 검증 (line 438-446)
   - "UP,..." 문자열에 대한 CRC 확인
   - 목적: LoRa 링크 무결성
2. hex payload 디코딩
3. LORA_TDM_APP_UplinkFwdCmd_t 구성
4. **Proxy Checksum 계산** (line 489-498)
   - Version+CommandClass+PayloadLength+Flags+Sequence(LE)+Payload에 대한 CRC-16/CCITT-FALSE
   - 목적: SB 메시지 변환 후 payload 손상 감지용
5. CFE_SB_TransmitMsg(UPLINK_APP_CMD_MID)

**출력**: UPLINK_APP_CMD_MID (Checksum 필드 포함)

**역할**: **링크 물리계층 → SB 메시지 변환** (포맷만 검증, 명령 유효성은 검증 안 함)

---

### uplink_app = 명령 검증/라우팅 (UPLINK_APP_ProcessUplink)

**입력**: UPLINK_APP_ProcessUplinkCmd_t (lora_tdm_app이 forward한 SB 메시지)

**처리**:
1. **Sequence 검증** (line 93-104)
   - 새 sequence > 마지막 수락된 sequence
   - 목적: replay attack 방지
2. **Proxy CRC 재검증** (line 106 → ValidateProxyCommand → line 156)
   - Version+CommandClass+PayloadLength+Flags+Sequence+Payload 재계산
   - 목적: SB 메시지 변환/전송 중 payload 손상 감지
3. **Authorization 검증** (line ~200)
   - CommandClass별 필요 auth level 확인
   - Flags의 auth bits 검증
   - Level 3 명령(RECOVERY/MODE)은 RequestToken 필수
4. **Health state 검증** (line 148-176)
   - FAILED/RECOVERY state: RECOVERY/DIAGNOSTIC만 허용
   - DEGRADED state: VIEWPOINT/CONFIG 차단
   - FORCE 플래그로 override 가능 (이벤트 기록)
5. **Route target 결정** (line 119-132)
   - CommandClass → CONFIG: cfs_core_app, ROUTE: cfs_core_app, MODE: cfs_core_app, RECOVERY: cfs_core_app, DIAGNOSTIC: cfs_core_app
   - 목적: 다음 명령 처리 앱 결정
6. SB 메시지로 대상 앱에 forward

**출력**: CONFIG_CMD_MID / MODE_CMD_MID / RECOVERY_CMD_MID / ROUTE_UPDATE_MID 등

**역할**: **SB 메시지 → 명령 검증 → 대상별 forward** (명령 유효성 검증 및 라우팅)

---

## 구조

```
LoRa serial
    ↓
[lora_tdm_app: 링크 수신]
  - 프레임 CRC 검증
  - payload 디코딩
  - Proxy Checksum 계산
  - → UPLINK_APP_CMD_MID
    ↓
[uplink_app: 명령 검증]
  - Sequence 검증
  - Proxy CRC 재검증
  - Authorization 검증
  - Health state 검증
  - Route target 결정
  - → CONFIG_CMD_MID / MODE_CMD_MID / ... (대상 앱별)
    ↓
[cfs_core_app / mavlink_bridge_app / ...]
  - 최종 명령 처리
```

## 중복 제거 여부

**결론: 중복이 아님**

각 단계의 CRC/Checksum 검증은 서로 다른 목적:
- **lora_tdm_app CRC**: LoRa 링크 무결성 (serial I/O 손상 감지)
- **uplink_app Proxy CRC**: SB 메시지 변환/전송 무결성 (SB 버스 손상 감지)

분리 이유:
1. 물리계층(LoRa) ↔ SB 계층의 경계에서 손상 감지 필요
2. 각 계층의 독립적 오류 처리
3. uplink_app이 다른 uplink 소스(USB-C 등)로 확장될 때 재사용 가능

**실제 중복 제거 대상**: config payload checksum 검증만
- lora_tdm_app: ConfigChecksum 검증 O
- uplink_app: ConfigChecksum 검증 X (추가 필요)
- mavlink_bridge_app: ConfigChecksum 검증 X (추가 필요)
- cfs_core_app: ConfigChecksum 검증 ? (확인 필요)

---

# uplink_app 자신의 CONFIG 처리 현황 (2026-07-15)

## 설계 (spec 13.1)

```
uplink_app: 제한적
- 자신의 파라미터(MAX_PAYLOAD, PROTOCOL_VERSION 등) 필요 시만 로컬 버퍼 유지
- 다른 앱 대상 config는 해당 앱 MID로 forward만 수행 (자신이 소유하지 않음)
```

## 현황

### 구현됨 (Forward only)
- ✅ CONFIG_CMD_MID 수신 후 checksum 검증
- ✅ 대상 앱(cfs_core_app/mavlink_bridge_app)으로 forward
- ✅ ConfigPendingState 추적 (forward 상태)

### 미구현 (Self-processing)
- ❌ UPLINK_APP_CONFIG_SCOPE 정의 없음
- ❌ ProcessConfigCommand 없음
- ❌ 자신의 파라미터 런타임 변경 불가

### 런타임 고정 파라미터
```c
#define UPLINK_APP_MAX_PAYLOAD_LENGTH     196
#define UPLINK_APP_PROTOCOL_VERSION       1
#define UPLINK_APP_ROUTE_FLYABLE_X_MIN_M  -50.0f
// ... (모두 컴파일 타임 값)
```

## 설계 의도 해석

**두 가지 해석 가능:**

### 해석 A: 의도된 제한 (현재 상태 유지)
- uplink_app은 자신의 config를 수신하지 않음
- 이유: 비행 중 uplink 프로토콜 변경 위험
- cfs_core_app/mavlink_bridge_app만 런타임 재구성 가능

### 해석 B: 미완성 기능 (구현 필요)
- uplink_app도 자신의 config를 처리해야 함
- 예: MAX_PAYLOAD를 지상국에서 조정
- 예: PROTOCOL_VERSION 호환성 런타임 변경

## 위험 평가

**현재 상태로 문제:**
- 중: uplink 프로토콜 변경 불가 (fw 업데이트만 가능)
- 낮: MAX_PAYLOAD 고정 (보통 충분)

**구현 시 위험:**
- 높: uplink_app 재구성 중 명령 유실
- 해결: ConfigPendingState, active/pending 이중 버퍼 필요

## 다음 단계 (확인 필요)

- [ ] spec 의도 명확화: "제한적"의 범위?
- [ ] 지상국 요구사항: uplink 파라미터 변경 필요?
- [ ] UT 확인: 자신의 config 처리 UT 있는가?
- [ ] 추가 구현: 필요 시 ProcessConfigCommand 추가

## 관련 파일
- uplink_app/fsw/src/uplink_app_utils.c:350 (ForwardConfigCommand)
- uplink_app/config/default_uplink_app_msgstruct.h (파라미터 정의)
- notes/mission_app_runtime_spec.md:§13.1 (설계)

---

## LORA_RAW 죽은 경로 제거


## 문제

`uplink_app`이 구독하는 `UPLINK_APP_LORA_RAW_MID_VALUE = 0x1909`가
`mavlink_bridge_app`의 `FC_SYS_TIME_MID_VALUE = 0x1909`(commit `38c2f22`,
2026-07-13)와 번호가 겹친다 — 이전에 고친 4-1(`0x190F`)과 같은 급의
MID 충돌.

## 경위 (git log로 확인)

- **2026-06-15 (`2fcaf80`, Task B)**: 당시 `lora_fc_downlink_app`(transport)이
  raw "UP,..." 텍스트를 `UPLINK_RAW_MID(0x1909)`로 SB publish하고,
  `uplink_app`(app)이 구독해 `ParseLoRaFrame()`으로 파싱하는 "분리형(A) 설계"
  채택 — CP2102 시리얼 포트를 한 앱만 열도록 하기 위한 의도적 설계였음.
- **2026-06-11 실제로는 이보다 먼저(`5712a0f`) `lora_tdm_app` 도입** —
  `lora_fc_downlink_app`을 대체하며 설계가 바뀜: TDM은 1초 주기
  TX→300ms RX창의 촉박한 타이밍이라, UP 프레임 CRC 오류를 **바로 다음
  다운링크 슬롯**에 UFB(Uplink Feedback Byte)=CRC_FAIL로 실어 지상국에
  알려야 함(`notes/lora_tdm_app_behavior_spec.md` §9.2). raw forward 후
  `uplink_app`과 비동기 SB 왕복으로 파싱 결과를 기다리면 같은 슬롯 안에
  못 돌아올 수 있음 — 그래서 `lora_tdm_app`이 **CRC 검증 자체를 자기
  안으로 가져와** 동기 처리하도록 재설계(`ProcessUpFrame`), 파싱 완료된
  구조체(`LORA_TDM_APP_UplinkFwdCmd_t`)를 별도 MID(`0x18D0`)로 직접 전달.
  (참고: SEQ_FAIL은 즉시성 불필요라 여전히 비동기 — `uplink_app`의
  `UPLINK_STATUS_MID` 폴링으로 처리)
- 이 전환 과정에서 `uplink_app`의 구 raw-forward 경로(구독/dispatch/
  `ParseLoRaFrame`)는 **정리되지 않고 그대로 남음** — 발행자(`lora_fc_downlink_app`)는
  이미 commit `7c080f1`(2026-06-30)로 저장소에서 삭제됐는데, 구독 쪽만 남아
  죽은 코드가 됨.
- **2026-07-13 (`38c2f22`)**: `mavlink_bridge_app`이 `0x1909`를 SysTime
  발행용으로 새로 할당 — 죽어있던 `uplink_app`의 구독과 번호가 겹치는 걸
  아무도 몰랐음.

## 영향

`uplink_app`과 `mavlink_bridge_app`이 둘 다 떠 있으면 `uplink_app`의
CommandPipe가 SysTime 텔레메트리를 `0x1909`로 수신하고,
`UPLINK_APP_TaskPipe`가 이를 raw LoRa 프레임으로 오인해
`ParseLoRaFrame()`에 넘긴다. 파싱 자체는 CRC 불일치로 안전하게
거부되어(크래시 없음) `ErrCounter`만 증가하지만, 의미 없는 오류
로그·카운터 증가가 계속 발생.

## 결정

`ParseLoRaFrame` 경로는 발행자가 없는 완전한 죽은 코드이므로 **전체
삭제**. `lora_tdm_app`의 `ProcessUpFrame`(→`UPLINK_APP_CMD_MID_VALUE
0x18D0`)이 유일한 실사용 경로이며 이걸로 충분 — 향후 raw 전달이
필요해지면 그때 재설계.

## 삭제 대상

- `uplink_app/fsw/src/uplink_app.c` — `UPLINK_APP_LORA_RAW_MID_VALUE` 구독
- `uplink_app/fsw/src/uplink_app_dispatch.c` — `UPLINK_APP_LORA_RAW_MID_VALUE`
  분기 블록 전체
- `uplink_app/fsw/src/uplink_app_utils.c` — `UPLINK_APP_ParseLoRaFrame()` 함수 전체
- `uplink_app/fsw/src/uplink_app.h` — `UPLINK_APP_ParseLoRaFrame` 프로토타입
- `uplink_app/config/default_uplink_app_msgid_values.h` — `UPLINK_APP_LORA_RAW_MID_VALUE` 정의
- `uplink_app/config/default_uplink_app_msgstruct.h` — `UPLINK_APP_LoRaRawMsg_t` 정의
- `uplink_app/unit-test/coveragetest/coveragetest_uplink_app_dispatch.c` —
  `Test_UPLINK_APP_TaskPipe_LoRaRaw` / `Test_UPLINK_APP_TaskPipe_LoRaRaw_BadFrame`
  + `ADD_TEST` 2건
- `uplink_app/unit-test/coveragetest/coveragetest_uplink_app_utils.c` —
  `Test_UPLINK_APP_ParseLoRaFrame` + `ADD_TEST` 1건
- `uplink_app/unit-test/stubs/uplink_app_utils_stubs.c` — 관련 stub

## 상태

- [x] 원인·경위 규명 (git log 대조)
- [x] 결정: 전체 삭제 (사용자 확인)
- [x] 코드 삭제 실행 — 구독(`uplink_app.c`)/dispatch 분기/`ParseLoRaFrame()`+선언/
      config(`msgid_values.h`/`msgstruct.h`)/coveragetest 2건/stub 전부 제거,
      `uplink_app/README.md` 레거시 언급 정리
- [x] UT 빌드/회귀 확인 — `uplink_app` 9/9, `_cmds` 91/91, `_dispatch` 29/29,
      `_utils` 88/88 전부 PASS, 회귀 없음
- [x] `mission_app_runtime_spec.md`(MID 표·§18.4.4 구현상태 노트)/`TEST_CASES.md`
      (coveragetest 카탈로그 3건)/`spec_code_audit.md`(4-8 신규 행 + 부록 A
      MID 인벤토리) 갱신 완료

---

## uplink_app_cmds UT 인증 게이트 실패 케이스 보강


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

---

## FC MISSION_ACK 응답 피드백 (openMCT 노출)


## 문제

openMCT에서 uplink 명령 송신 시:
- 현재: "수신만 되면 적용됨"으로 표시 (uplink_app 검증 결과만 반영)
- 실제: FC가 MISSION_ACK 응답으로 accept/reject 알려주고 있음 → 지상국이 못 받음

**발견 경로**: 
1. openMCT 명령 → uplink_app 검증 ✓
2. uplink_app → lora_tdm_app → FC (LoRa 송신) ✓
3. FC → mavlink_bridge_app (MISSION_ACK 응답) ✓ 수신 완료
4. mavlink_bridge_app HkTlm.LastUploadResult ✓ FC 응답 저장
5. **uplink_app StatusTlm → openMCT ✗ FC 응답 미포함 → 지상국이 못 봄**

## 근본 원인

### 현재 UPLINK_STATUS_MID 필드 (spec 18.7)

✓ 있음:
- LastCommandResult (uplink_app 검증: 수락/거부/라우팅실패)
- LastConfigResult (구성 활성화 결과)

✗ 없음:
- FC의 MISSION_ACK 응답 결과
- 비행체 업로드 상태

## 해결 방안

### 1. spec 수정 (mission_app_runtime_spec.md 18.7)

UPLINK_STATUS_MID에 추가 필드:

```
- FcMissionResult: FC MISSION_ACK 응답
  값: MISSION_ACCEPTED(0) | MISSION_UNSUPPORTED_FRAME(2) | MISSION_DENIED(3) | TIMEOUT(4)
- FcMissionUploadState: 현재 FC 업로드 상태
  값: IDLE(0) | ACTIVE(1)
- FcMissionUploadSuccessCount: 누적 성공 업로드 수 (FC 응답 기준)
```

### 2. 코드 수정

#### 2.1 uplink_app_utils.c

- mavlink_bridge_app의 HK 구독 추가 (BRIDGE_HK_MID)
- 수신 시 LastUploadResult 캐시
- UPLINK_APP_UpdateStatusTelemetry()에서 FC 응답 필드 채우기

#### 2.2 default_uplink_app_msgstruct.h

UPLINK_APP_StatusTlm_t에 필드 추가:
```c
uint8  FcMissionResult;           // MISSION_ACK result
uint8  FcMissionUploadState;      // IDLE/ACTIVE
uint32 FcMissionUploadSuccessCount; // 누적 성공
```

#### 2.3 UT (coveragetest_uplink_app_utils.c)

- mavlink_bridge HK 구독 모킹
- FcMissionResult 업데이트 시나리오 추가
- StatusTlm 필드 검증

### 3. openMCT 표시 (지상국, 별도 저장소)

- fc_serial_ws_server.py에서 UPLINK_STATUS_MID 파싱 시 FC 응답 필드 노출
- UI에서 "FC 응답" 상태 표시 (ACCEPTED/DENIED/TIMEOUT)

## 상태 (완료, 2026-07-15)

- [x] spec 18.7 업데이트 (필드 정의 추가)
- [x] uplink_app HK 구독 구현 (BRIDGE_HK_MID Subscribe #4 추가)
- [x] StatusTlm 필드 추가 (FcMissionResult/FcMissionUploadState/FcMissionUploadSuccessCount)
- [x] UT 추가 — Init_Subscribe4Error, TaskPipe_BridgeHk, UpdateStatusTelemetry FC 필드 검증
- [x] 빌드/회귀 검증 — uplink_app 4개 러너 전량 PASS(235 TOTAL), 타 앱 회귀 없음
- [ ] 지상국(openMCT) 파싱 업데이트 (별도 저장소, fc_serial_ws_server.py)

## 구현 요약

```
shared_msgs/bridge_hk_msg.h (BRIDGE_HK_TLM_t)
  ├─ LastUploadResult, MissionUploadSuccessCount 이미 존재 (재사용)

uplink_app/config/default_uplink_app_msgid_values.h
  └─ BRIDGE_HK_MID_VALUE (0x08A0) 추가

uplink_app/config/default_uplink_app_msgstruct.h
  ├─ #include "bridge_hk_msg.h"
  ├─ typedef BRIDGE_HK_TLM_t UPLINK_APP_BridgeHkMirror_t
  └─ UPLINK_APP_StatusTlm_t 끝에 FcMissionResult/FcMissionUploadState/
     FcMissionUploadSuccessCount 필드 append (mirror 레이아웃 컨벤션 준수)

uplink_app/fsw/src/uplink_app.c
  └─ Init()에 BRIDGE_HK_MID Subscribe 추가 (#4)

uplink_app/fsw/src/uplink_app_dispatch.c
  └─ TaskPipe()에 BRIDGE_HK_MID 처리 분기 추가
     (LastUploadResult/MissionUploadSuccessCount 캐시, UploadState=ACTIVE)

uplink_app/fsw/src/uplink_app_utils.c
  └─ UPLINK_APP_UpdateStatusTelemetry()에서 캐시값을 StatusTlm에 반영
```

## 다음 단계 (별도 저장소, openMCT/지상국)

fc_serial_ws_server.py에서 UPLINK_STATUS_MID 파싱 시 FcMissionResult 등
3개 필드 파싱 추가 필요. 이 저장소 범위 밖.

## 노트

- 기존 `LastCommandResult` 필드는 유지 (uplink 검증 결과)
- FC 응답은 별도 필드로 추가 (명확한 구분)
- mavlink_bridge_app HK는 이미 MISSION_ACK를 추적 중 — 재사용만 하면 됨
- 대역폭: StatusTlm에 4바이트 추가 (uint8×2 + padding + uint32)
