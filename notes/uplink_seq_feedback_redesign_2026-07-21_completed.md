# 업링크 seq·피드백 규약 개정 — 문제 정리 및 설계 방향 (2026-07-21)

> **주의**: 이 문서는 "무엇이 문제인지"와 "어느 방향으로 갈지"까지만
> 정리한 것이며, 구현 착수 전 아래 관련 spec을 반드시 먼저 읽을 것.
> 특히 반이중 TDM 슬롯 정렬(RX 윈도우 100ms), 4회 재전송 설계,
> 권한검증/request_token 규약과 충돌하지 않아야 함.
>
> 필독: `notes/mission_app_runtime_spec.md` §18.4(프레임/Flags)·§18.10(health
> gate)·§18.11.1(권한검증), `notes/lora_protocol_v2_spec.md` §4(DL2 포맷)·§7(타이밍),
> `notes/lora_tdm_app_behavior_spec.md` §10(UFB), `notes/uplink_lora_test_status.md` §4

---

## 발단

오늘 UFB(UplinkFeedback) 관련 버그를 고치다가, 문제의 뿌리가 **개별
버그가 아니라 "지상이 명령의 처리 결과를 어떻게 확인하는가"라는 규약
자체의 미비**라는 게 드러남. 서로 얽힌 이슈 5개를 한 곳에 정리.

---

## 확인된 문제

### [긴급-1] 4회 재전송 중복이 "실패"로 보고됨 — 오늘 내 변경이 유발

지상은 좁은 RX 윈도우를 놓치지 않으려 **같은 명령을 4개 슬롯에
재전송**함(`_UPLINK_RETX = 4`, `fc_serial_ws_server.py:351`).
설계 의도된 동작이며 `uplink_lora_test_status.md` §4 "문제 A"에 근거 있음.

그런데 `uplink_app`의 중복 판정은:
```c
/* uplink_app_cmds.c */
static bool UPLINK_APP_IsSequenceAccepted(uint16 Sequence)
{
    if (UPLINK_APP_Data.AcceptedCount == 0U) return true;
    return (Sequence > UPLINK_APP_Data.LastAcceptedSequence);
}
```
→ 슬롯 1은 수락, **슬롯 2·3·4는 `REJECT_SEQUENCE(10)`**.

여기에 오늘 내가 넣은 두 변경이 겹침:
- `04d8f99` — `REJECT_SEQUENCE(10)` → `UFB_SEQ_FAIL(2)` 매핑
- `073a680` — `RunTx()`의 매 사이클 UFB 리셋 제거

**결과: 정상 성공한 명령이 오히려 `SEQ_FAIL(2)`을 영구 래치함.**

```
슬롯1  seq N 수락, 라우팅        → ufb=0
슬롯2  seq N 중복 → REJECT_SEQ   → ufb=2
슬롯3,4 동일                      → ufb=2
이후    (리셋 없음)               → ufb=2 무한 유지
```

오늘 실측에서 안 드러난 건 GUI가 명령 직후 **첫** 패킷만 보고
끝내서(`armPendingCommand` + 1초 타임아웃) 운 좋게 이전 OK를 읽었기 때문.

→ **"우리 재전송의 중복"은 실패가 아니므로 UFB로 새어나가면 안 됨.**

---

### [긴급-2] 지상국 재시작 시 명령권 상실 (신규 발견, 더 위험)

**기체는 기억하고 지상은 잊는 비대칭:**

| | 저장 위치 | 재시작 시 |
|---|---|---|
| 기체 `LastAcceptedSequence` | `/cf/uplink_app_state.bin` (`uplink_app.c:111` Load, `uplink_app_cmds.c:361` Save) | **유지됨** — 게다가 로드 시 `AcceptedCount=1` 강제라 "첫 명령 무조건 통과" 장치도 꺼짐 |
| 지상 `_SeqCounter._v` | 프로세스 메모리뿐 | **1로 리셋** |

시나리오:
```
1. 명령 50회 전송 → 기체가 LastAcceptedSequence=50 저장
2. 지상국 서버 재시작       ← 오늘 실제로 했던 동작(PARAM_BOUNDS 반영 때문)
3. 다음 명령 seq=1 → 1 > 50 거짓 → 거부
4. 기체를 재부팅해도 파일에서 50을 다시 읽음 → 여전히 거부
```

**비행 중 지상 소프트웨어를 한 번 재시작하면 명령권을 잃음.**
복구 방법이 (a) 실패할 명령을 50번 태워 카운터를 넘기거나
(b) 기체에 SSH로 붙어 상태 파일 삭제 — 둘 다 비행 중 불가/부적절.

65535회 wrap보다 **훨씬 자주 발생할 조건**.

---

### [중] 문제 3. seq 65535 wraparound

지상은 `1..65535` 순환 후 **1로 되돌림**
(`_SeqCounter.next()`: `self._v = (self._v % 0xFFFF) + 1`, 전용 테스트도 있음).
기체는 엄격한 `>` 비교라 되돌아온 `1`이 영구 거부됨.

`mission_app_runtime_spec.md:1878`에 "wraparound 허용 여부와 허용 시간
창은 추가 확장 시에만 세분화한다"로 **의도적 유예** 상태이나,
seq 규칙을 손보는 김에 같이 확정하는 게 맞음.

**부수 발견 — 타입 불일치**:
- 무선/텔레메트리: `uint16` (`LastCommandSequence`, `LastRxSequence`)
- 내부/영속 상태: `uint32` (`uplink_app.h:28`, `uplink_app_utils.c:19`)

모듈러 비교를 도입할 때 캐스팅을 잘못하면 조용히 깨질 수 있는 지점.

---

### [중] 문제 4. UFB에 "어느 명령의 결과인지"가 없음 (A-1의 근본 원인)

DL2 프레임의 `ufb`는 **태그 없는 1바이트**
(`lora_protocol_v2_spec.md:47`). 지상은 "명령 보낸 뒤 처음 온 UFB =
내 명령의 결과"라고 추측할 수밖에 없음.

**그런데 기체 내부에는 이미 상관 정보가 있음**:
```c
/* uplink_app_utils.c:56-57 — UPLINK_STATUS_MID */
Tlm->LastCommandSequence = UPLINK_APP_Data.LastRxSequence;  /* 어느 명령 */
Tlm->LastCommandResult   = UPLINK_APP_Data.LastCommandResult; /* 결과 */
```
그리고 `lora_tdm_app`이 이미 이걸 구독 중(`lora_tdm_app_dispatch.c:101`).

→ **`lora_tdm_app`이 결과 1바이트만 고르고 seq는 버리는 게 유일한 병목.**
seq만 같이 내려보내면 지상이 대조 가능해지고, 오늘 씨름한
"래치 vs 리셋" 논쟁 자체가 소멸함(래치가 남아있든 말든 seq가 다르면
무시하면 되므로 TTL 같은 것도 불필요).

---

### [중] 문제 5. "라우팅 성공"과 "실제 적용 완료"가 구분 안 됨

`uplink_app`은 **프록시/라우터**이지 설정을 직접 적용하지 않음.
실제 적용은 대상 앱이 함:

| 단계 | 주체 | 현재 |
|---|---|---|
| 프레임 수신/CRC | `lora_tdm_app` | ✅ |
| 검증+라우팅 | `uplink_app` | ✅ (`ROUTED`) |
| **실제 적용** | 대상 앱 | ❌ **회신 경로 없음** |

대상 앱은 자기 결과를 갖고 있음 — 예:
`mavlink_bridge_app`의 `LastConfigResult`(`CONFIG_RESULT_OK` /
`CONFIG_RESULT_BAD_VALUE`, `mavlink_bridge_app_utils.c:1913,1928`).
그러나 **`uplink_app`으로 돌아오는 길이 없어** 지상까지 못 옴.

`mission_app_runtime_spec.md` §18.4.6.4가 "전달 vs 실행 구분 … 완료"라고
쓴 부분이 바로 이것이며, **실제로는 미구현**
(`command_dead_end_audit_2026-07-21.md` Finding 1과 동일 사안).

---

### [참고] 문제 6. 1개 명령이 최대 12회 전파

지상 GUI는 `UFB=1(CRC_FAIL)`에 대해 최대 3회 자동 재전송하고,
각 재전송이 다시 4슬롯으로 나감 → **3 × 4 = 12 프레임**.
현재 `SEQ_FAIL(2)`에는 자동 재전송을 안 하므로(수동 안내) 긴급-1이
직접 증폭을 유발하진 않으나, 규약 개정 시 재전송 정책을 함께
점검할 것.

---

## 설계 방향

### 1단계 — 긴급 차단 (프로토콜 변경 없음)

- [ ] **재전송 중복을 실패로 보고하지 않기**
      `uplink_app`이 `Sequence == LastAcceptedSequence`(우리 재전송)와
      `Sequence < LastAcceptedSequence`(진짜 replay)를 구분해,
      전자는 `REJECT_SEQUENCE`가 아닌 별도 결과코드(예: `DUPLICATE`)로.
      → UFB로 새어나가지 않게 됨
- [ ] 그때까지 오늘 넣은 `073a680`(리셋 제거)을 유지할지 롤백할지 판단
      — 2단계가 곧 오면 유지, 지연되면 롤백 검토

### 2단계 — 핵심: 다운링크에 seq 동봉 (문제 2·3·4 동시 해결)

- [ ] DL2 프레임에 `ufb_seq`(2바이트) 추가,
      `lora_tdm_app`이 `UPLINK_STATUS_MID`의 `LastCommandSequence`를
      그대로 실음 (기체 내부엔 이미 있는 값이라 추가 수집 불필요)
- [ ] 지상 GUI가 **seq 대조 후** 판정 → 타이밍/래치 문제 원천 소멸
- [ ] **같은 필드로 지상 재시작 자가복구**(문제 2 해결):
      기체가 "나는 seq N까지 받았다"를 상시 내려보내므로, 지상은
      서버 기동 시 그 값을 보고 `N+1`부터 시작하면 됨
      → **지상이 파일로 기억할 필요 자체가 없어짐. 새 장비에서 켜도 자동 정합.**
- [x] **완료(2026-07-22, BL-13)**: 기체 비교를 모듈러 윈도우로 변경.
      `uplink_app_cmds.c UPLINK_APP_CheckSequence()` — DUPLICATE(==) 분리 후
      `uint16 diff = (uint16)(seq - last); NEW if diff < 0x8000 else REPLAY`.
      회귀 UT: uplink_app_cmds 99/99, 4종 합계 회귀 없음. 랩어라운드
      (65535→1) 정상 수락 회귀 테스트 추가
- [ ] 프레임 크기 47B → ~49B. `lora_protocol_v2_spec.md` §7 및
      Stage 2/3 실측 마진(`lora_stage_measurement_runbook.md`) 안에
      드는지 확인 필요

### 2단계 보강 — 부트 카운터(세션 번호) 필수

2단계의 "지상이 기체가 알려준 seq를 따라간다"만으로는 구멍이 있음:

- **ⓐ 보안 후퇴**: 공격자가 다운링크를 위조/재생해 낮은 seq를 보고하면
  지상 카운터가 뒤로 돌아가고, 이후 캡처해둔 예전 명령을 재생하면 수락됨
- **ⓑ 구분 불가**: 보고 seq가 50→0일 때 "재시작+상태유실"인지
  "깨진 프레임"인지 판별 불가
- **ⓒ wrap과 재시작 혼동**: 65535→1 과 재시작→1 이 동일하게 보임

**해법: 감소하지 않는 부트 카운터를 기체가 영속 보관 → 다운링크에 동봉**

```
다운링크: (boot_count, last_accepted_seq)

지상 판단:
  boot_count 증가            → 재시작 확인, seq 재동기 정당
  boot_count 동일 + seq 감소  → 비정상, 무시
```

- [x] **다운링크 방향 완료(2026-07-22, BL-03/BL-12)**: DL2에
      `(uplink_last_seq, boot_count)`를 동봉해 지상 자가복구(`_SeqCounter.
      resync_from_device()`, 앞으로만 당김)와 재부팅 감지
      (`_BootCountTracker`, 감소 시 `boot_count_anomaly` 플래그만) 구현.
      상세: `notes/temp/BACKLOG.md` BL-03/BL-12, 커밋
      `cfs-telemetry-app@2d91243` / `openMCT@8bdc969`.
- [x] **업링크 방향 — 의도적으로 하지 않기로 결정(2026-07-29)**: 지상이 아는
      `boot_count`를 UP2 프레임에도 실어 기체가 이전 세션 번호를 단 명령을
      거부하는 것(예전 세션 캡처 명령의 재생 차단)은 **ⓐ가 방어하려던
      "공격자가 재생 공격"이라는 위협 모델 자체를 프로젝트 범위에서
      제외하기로 하면서 불필요해짐**. ⓑⓒ(구분 불가/wrap 혼동)는 이미
      다운링크 방향만으로 해소됨(아래). 남기는 비용(UP2 프레임 크기 증가,
      RX창 마진 잠식, 파서 재작업, 지상측 boot_count 유실 시 가용성 저하
      위험)이 실익보다 커서 착수하지 않음.
- [x] `(boot_count, seq)` 복합 식별자로 seq wrap 문제(문제 3)는
      BL-13(모듈러 비교, 2026-07-22)로 별도 해소 완료. boot_count는
      wrap 자체를 막진 않지만 재부팅 시 지상 재동기 근거를 제공
- [x] 저장 위치: `UPLINK_APP_PersistentState_t`에 `BootCount` 추가,
      `Checksum = Magic + LastAcceptedSequence + BootCount`로 변경
      완료(2026-07-22) — 기존 파일은 체크섬 불일치로 자동 폐기되고
      신규 상태(`BootCount=0`)로 시작, 의도된 무해한 마이그레이션
- [ ] ~~**대안 확인**: cFE 프로세서 리셋 카운터로 대체~~
      → **근거 약함(2026-07-21 정정)**. Pi에서는 cFE 리셋 카운터도 결국
      같은 SD카드에 저장되므로 앱 상태 파일과 동일한 유실 위험을 가짐.
      "더 안전한 저장소"가 아님

#### 상태 파일 유실 시 거동 — "자동 거부" 금지

`SaveState()`(`uplink_app_utils.c:518-543`)는
`tmp 쓰기 → close → rename()` 패턴이라 **부분 기록 파일은 생기지
않음**(rename은 원자적). 다만 **`fsync()`가 없어** rename 후 데이터가
SD에 내려가기 전 전원이 끊기면 내용이 깨질 수 있음(ext4 휴리스틱이
실무상 대개 보정하나 보장은 아님).

- 노출 구간은 좁음: `SaveState()`는 **명령 수락 시에만** 호출
  (`uplink_app_cmds.c:361`) → 상시 기록이 아님
- 깨져도 로드 시 체크섬으로 **감지·폐기**됨 → `AcceptedCount=0`
  (아무 seq나 수락)으로 기동 → 복구 가능, 먹통 아님

**따라서 부트 카운터를 "감소 = 공격 = 자동 거부"로 만들면 안 됨.**
진짜 파일 유실 시 운영자가 복구할 수단이 사라짐.

```
boot_count 증가  → 자동 수락 (정상 재시작)
boot_count 감소  → 자동 거부가 아니라 "운영자 확인 필요" 상태로 표시
                   (위조는 자동으로 통과하지 않고, 진짜 유실은 승인으로 복구)
```

- [x] `SaveState()`에 `fsync()` 추가 완료(2026-07-21, BL-18) — 파일 fd +
      부모 디렉터리(`/cf`) fd 둘 다

**부수 발견 — `LoadState()`의 조용한 실패**
`uplink_app_utils.c:480-513` — 파일 열기/크기/Magic/체크섬 실패 시
전부 `return`만 하고 **이벤트를 내지 않음**(성공 시에만
`UPLINK_APP_STARTUP_EID` 발생). 상태 파일이 깨져 `AcceptedCount=0`
(= 아무 seq나 수락) 상태로 기동했는지 지상에서 알 방법이 없음.
→ 실패 경로에도 이벤트 추가 필요. `system_wide_reaudit`의 F-1
(SUB_ERR_EID 등 미발생)과 같은 계열.

### 3단계 — 실행 결과(EXECUTED) 회신 (문제 5)

- [ ] 대상 앱 → `uplink_app` 실행결과 회신 MID/메커니즘 신규 설계
      (현재 `UPLINK_STATUS_MID`는 `uplink_app`→`lora_tdm_app` 단방향)
- [ ] `UPLINK_APP_RESULT_EXECUTED` 계열 결과코드 추가
- [ ] `mission_app_runtime_spec.md` §18.4.6.4의 "완료" 표기를 실제와 정합화
- [ ] UFB 코드표 전체를 이때 한 번에 확정
      — 현재 `REJECT_STATE`만 매핑돼 있고 나머지 7종
      (`FAILED`, `REJECT_CLASS`, `REJECT_LENGTH`, `ROUTE_MISS`,
      `REJECT_ROUTE`, `REJECT_CHECKSUM`, `REJECT_VIEWPOINT`)은
      지상에서 OK와 구분 불가
      (`inferred_decisions_selfaudit_2026-07-21.md` A-2)

### 선택 — 재전송 인덱스 명시 (2단계와 함께라면 저비용)

`Flags` 바이트 여유 비트 활용:
```
bit:  7  6  5  4  3  2  1  0
     └AUTH┘  └── 여유 5비트 ──┘ FORCE(0x01)
```
`bits[5:1]`이 비어 있어 재전송 인덱스(1~4)는 2비트면 충분,
**프레임 크기·air time 증가 없음**.

- 이득: 4번 중 몇 번째가 통과했는지 → **RF 링크 마진 진단 지표**
- 비용: 지상이 4개 프레임을 각각 다르게 생성(현재는 동일 문자열 4회)
- 정확성 자체는 1단계(ⓑ 기체 추론)로 이미 확보되므로 **급하지 않음**.
  2단계에서 어차피 양쪽을 손대므로 그때 같이 넣는 것을 권장.

---

## 미결정 사항

- [ ] 1단계만 먼저 할지, 2단계까지 묶어서 설계 후 한 번에 갈지
- [ ] `073a680`(RunTx 리셋 제거) 유지 vs 롤백
- [ ] 모듈러 윈도우 크기(half-range `0x8000` vs 더 좁게) —
      좁을수록 replay 방어는 강하나 정상 desync 복구가 어려워짐
- [ ] 재전송 인덱스(ⓐ)를 2단계에 포함할지
- [ ] 부트 카운터를 커스텀 필드로 넣을지, cFE 리셋 카운터를 쓸지
- [ ] "인증된 seq 재동기 명령"(모든 어긋남의 최종 탈출구)이 필요한지 —
      2단계+부트카운터로 대부분 해소되므로 우선순위 낮음.
      도입 시 seq 검사를 우회해야 하므로 `request_token` 기반 인증 선행 필요

---

## Task 요약

| # | Task | 해결 대상 | 수정 범위 | 프로토콜 변경 | 선행조건 | 상태 |
|---|---|---|---|---|---|---|
| **T1** | 재전송 중복을 실패로 보고하지 않기<br>(`seq == last` → `DUPLICATE`, `seq < last` → replay) | 긴급-1 | 기체(`uplink_app`) | 없음 | 없음 | ✅ 완료(BL-01) |
| **T2** | `073a680`(RunTx UFB 리셋 제거) 유지/롤백 판단 | 긴급-1 파생 | 기체(`lora_tdm_app`) | 없음 | T1 판단과 연동 | ✅ 완료(BL-02, 2026-07-22) — 실측 확인된 레이스 수정이라 유지 확정, 롤백 사유 없음 |
| **T3** | 다운링크에 `ufb_seq` 동봉<br>(`UPLINK_STATUS_MID`의 `LastCommandSequence`를 그대로) | 문제 4 (+A-1 소멸) | 기체+지상 | **DL2 +2B** | air time 마진 확인 | ✅ 완료(BL-03, 실제론 +3B — seq u16+boot_count u8) |
| **T4** | 지상이 다운링크 seq 보고 자가복구 | 긴급-2 | 지상 | 없음(T3 재사용) | **T3** | ✅ 완료(BL-03, `_SeqCounter.resync_from_device()`) |
| **T5** | 부트 카운터 도입<br>(영속 저장 + 다운링크 + 업링크 태그) | ⓐⓑⓒ 보안·구분 | 기체+지상 | **DL2/UP +2B** | **T3**, cFE 리셋카운터 대체 가능성 확인 | ⛔ **범위 축소·보류(2026-07-29 결정)**: 다운링크 방향(BL-12/BL-03, 재시작 자가복구 ⓑⓒ)만 완료. **업링크 태그(재생공격 차단, ⓐ)는 프로젝트가 악의적 공격자 시나리오를 위협 모델에서 제외하기로 결정해 불필요 — 의도적 미착수로 종결**. 프레임 크기·에어타임·파서 재작업 비용 대비 실익 없음 |
| **T6** | seq 비교를 모듈러 윈도우로<br>`diff=(uint16)(seq-last); 0<diff<0x8000` | 문제 3 (wrap) | 기체 | 없음 | uint16/uint32 타입 정리 | ✅ 완료(BL-13) |
| **T7** | 대상앱→`uplink_app` 실행결과 회신 채널 + `EXECUTED` 코드 | 문제 5 | 기체 전반 | 내부 MID 신설 | 설계 필요(범위 큼) | 🟡 채널 자체는 BL-08(2026-07-22)로 완료, CONFIG(3앱)·RECOVERY(cfs_core)만 배선. 이후 BL-81/82(2026-07-29)로 MODE/VIEWPOINT도 배선 완료. **ROUTE_UPDATE/DIAGNOSTIC은 여전히 범위 밖** — 완전 완료 아님 |
| **T8** | UFB 코드표 전체 확정<br>(현재 미매핑 7종 포함) | A-2 | 기체+지상 | 코드값 확정 | **T7** | ⏸ BL-11, T7 종속 |
| **T9** | 재전송 인덱스를 `Flags` 여유비트에 | RF 진단 | 기체+지상 | 없음(여유비트) | 선택, T3와 동시 권장 | ✅ 완료(BL-14, 2026-07-22) — `bits[2:1]=RETX_IDX`, 기체 EVS 이벤트 표기 + 지상(openMCT) 슬롯별 재조립 |
| **T10** | `LoadState()` 실패 경로 이벤트 추가 | 조용한 실패 | 기체 | 없음 | 없음 | ✅ 완료(BL-17, 2026-07-22) — ENOENT(첫 부팅)만 조용히 처리, 그 외(open 실패/truncated/bad magic/checksum mismatch) 전부 `UPLINK_APP_STATE_CORRUPT_EID` 발생 |
| **T11** | `SaveState()`에 `fsync()` 추가 | 전원차단 시 내용 손상 | 기체 | 없음 | 없음 | ✅ 완료(BL-18) |

### 착수 순서 제안

```
즉시(독립):        T1 ─┬─ T2
                       └─ T10, T11, T6  (각각 독립, 작음)

핵심 묶음:         T3 ─┬─ T4
                       ├─ T5
                       └─ T9 (선택)

이후(범위 큼):     T7 ─── T8
```

- **T1/T6/T10/T11은 서로 독립이고 프로토콜 변경이 없어** 언제든 개별 착수 가능
- **T3는 T4·T5·T9의 공통 선행조건** — 다운링크 포맷을 한 번만 열고
  필요한 필드를 모두 넣는 게 유리(무선 포맷을 여러 번 나눠 바꾸면
  지상/기체 버전 호환이 계속 깨짐)
- **T7/T8은 범위가 커서 별도 설계 단계 필요** — 급하지 않음

---

## 관련
- `notes/temp/inferred_decisions_selfaudit_2026-07-21.md` (A-1/A-2 — 오늘 내 변경의 한계)
- `notes/temp/command_dead_end_audit_2026-07-21.md` (Finding 1 = 문제 5)
- `notes/temp/ground_controllable_capability_plan_2026-07-21.md` (P0 = 문제 5)
- `notes/temp/openmct_repo_gap_audit_2026-07-21.md` (openMCT-2 = 재전송 seq 재사용)
- `notes/temp/system_wide_reaudit_2026-07-21.md`
