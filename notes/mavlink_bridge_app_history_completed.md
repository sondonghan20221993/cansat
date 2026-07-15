# mavlink_bridge_app 완료 이력 모음

## FC 값 검증 갭 (finite/NaN 등)


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

## 결정 및 구현 (2026-07-13)

**A안(입구 차단) 채택.** `mavlink_bridge_app_utils.c`의 `PublishAttitude`/`PublishEkfLocal`에서
파싱 직후 `MAVLINK_BRIDGE_APP_ValuesFinite6()`로 6개 float 필드 전부 검증 —
하나라도 NaN/Inf면 `MAVLINK_BRIDGE_APP_RecordNonFiniteError()` 호출 후 즉시 return
(SB 게시 자체를 안 함, 기존 TLM 캐시도 그대로 보존).

- 새 카운터: `MAVLINK_BRIDGE_APP_Data.NonFiniteValueCount` (Data struct + HK TLM에 추가)
- 새 EID: `MAVLINK_BRIDGE_APP_NONFINITE_VALUE_ERR_EID` (13)
- `PublishGlobalPositionAsLocal`/`PublishGpsRaw`/`PublishEkfStatus`는 검증 대상에서 제외 —
  이 함수들의 float 필드는 int32/int16 raw 값을 상수로 나눈 결과라 구조적으로
  NaN/Inf가 나올 수 없음 (오버플로 없는 단순 나눗셈)

원래 검토했던 B/C안은 기각:
- B(Valid=0 마킹): 모든 구독자가 Valid를 확인해야 하는 부담이 있고, `lora_tdm_app`의
  다운링크 오염(§왜 문제인가 참조)은 여전히 발생 — 채택 안 함
- C(cfs_core_app에서만 검증): `lora_tdm_app` 다운링크 오염을 못 막음 — 채택 안 함

## 관련 항목

- `tests/TEST_CASES.md` "추가 런타임 시험 후보 — FC 장애/깨진 값" (RT-FC-007~009):
  테스트 후보에서는 이 갭을 **제외**하고 기록함 — 코드(설계) 사안이므로 본 노트로 분리
- EKF 발산 시나리오 자체는 RT-FC-009 (`fault=3 EKF_INVALID`)로 부분 커버되나,
  이는 FC가 스스로 EKF 불량을 보고하는 경우만 해당. 값이 깨졌는데 FC가 정상 보고하면 못 잡음

## 상태

- [x] 설계 방향 결정 (A) — 2026-07-13
- [x] 구현 + coveragetest 추가 (2026-07-13) — `Test_PublishAttitude_NaNRejected`(roll=NaN),
      `Test_PublishEkfLocal_InfRejected`(vz=+Inf), `Test_PublishAttitude_FiniteValuesAccepted`
      (정상값 통과 회귀 확인) 3건. mavlink_bridge_app UT 전체 회귀 없음
      (utils 136, main 14, cmds 4, dispatch 26 PASS).
- [x] E2E(B) PTY 삽입 테스트 — **불필요로 판명, 종결 (2026-07-14)**. 기존
      coveragetest의 `UT_FeedSerial()`(`coveragetest_mavlink_bridge_app_utils.c:687`)이
      이미 실제 OS `pipe()`에 실제 MAVLink 바이트를 write하고
      `MAVLINK_BRIDGE_APP_ServiceSerial()`을 직접 호출 — 이 함수 내부의 진짜
      `read(SerialFd, ...)` 시스템콜(`mavlink_bridge_app_utils.c:2021`)과
      `HandleReceivedBytes`→파싱→`PublishAttitude`/`PublishEkfLocal` 전 경로를
      실제로 통과한다. PTY와 pipe의 차이는 termios/tty 시맨틱스뿐이며, NaN/Inf
      검증은 순수 바이트스트림 파싱 로직이라 그 차이와 무관 — PTY로 바꿔도
      추가로 커버되는 코드 경로가 없음. `Test_PublishAttitude_NaNRejected`/
      `Test_PublishEkfLocal_InfRejected`가 이미 이 갭을 실질적으로 E2E 수준까지
      닫고 있음이 확인됨.

---

## FC MISSION 업로드 진단


배경: `dl2_systime_flag_length_crash` 수정 후 전체 `tests/` 재실행 중 `test_mission_upload_diag.py`
2건 실패 발견 (내 변경과 무관 — `git stash` 대조로 기존부터 존재하던 실패임을 확인).

## 1. `CrcTest::test_crc_acc_known_value` — 테스트가 틀림 (코드는 정상)

**주장**: `_crc_acc(0x00, 0xFFFF)`가 `0`을 반환해야 한다는 테스트, 실제론 `3975` 반환.

**검증**: MAVLink 공식 C 레퍼런스(`checksum.h::crc_accumulate`)를 동일 조건(`data=0, crcAccum=0xFFFF`)으로
수동 트레이스:
```
tmp = 0 ^ (0xFFFF & 0xFF) = 0xFF
tmp ^= (tmp << 4) truncated to uint8 → 0x0F
crc = (0xFFFF>>8) ^ (0x0F<<8) ^ (0x0F<<3) ^ (0x0F>>4)
    = 0xFF ^ 0x0F00 ^ 0x78 ^ 0 = 0x0F87 = 3975
```
→ `tools/mission_upload_diag.py::_crc_acc()`의 결과(3975)가 **MAVLink 표준과 일치**.
테스트 주석("tmp = 0xFF ^ 0xFF = 0")은 실제 알고리즘의 `tmp` 계산식과 무관한 잘못된 추론 —
`tmp`는 `byte ^ (crc&0xFF)`이지 `crc_low ^ crc_low`가 아님.

**결정**: 코드는 손대지 않음. 테스트 기대값을 `3975`로 정정 (또는 실제 계산식 기반 값으로 교체).

## 2. `ParserRoundTripTest::test_roundtrip_mission_item_int` — 실제 파서 버그

**증상**: `build_mission_item_int()`로 만든 프레임을 `_Parser`로 왕복 파싱하면 `None` 반환
(프레임을 못 찾음).

**재현/원인**:
```python
frame = build_mission_item_int(1, (2.0, -10.0, 3.0), 1, 1)
# frame[29] == 0xFE  (z=-3.0 → struct.pack('<f', 3.0)에서 우연히 발생한 페이로드 바이트)
```
`_Parser.feed()`:
```python
def feed(self, byte):
    if byte in (STX_V1, STX_V2):   # 0xFE, 0xFD
        self._reset()
        ...
```
이 체크가 **현재 상태(`self.state`)와 무관하게 모든 바이트에 대해** 실행된다.
MAVLink는 페이로드를 이스케이프하지 않으므로(그게 표준 설계), 페이로드나 CRC 바이트가
우연히 `0xFD`/`0xFE` 값을 가지면 파서가 프레임 중간에 리싱크해버려 진행 중이던 프레임을
통째로 잃는다.

실측: `frame[29]=0xFE`에서 `_reset()` 발동 → 이후 `frame[30]=0xFF`를 새 LEN(255)으로
오인 → 남은 바이트로는 255바이트 페이로드를 채울 수 없어 스트림 끝까지 `PAYLOAD` 상태에
머무름 → `msg=None`.

**왜 문제인가**: 이 도구는 "기체 없이" cFS 없이 직접 시리얼로 미션 업로드를 검증하는
디버그 툴(`tools/mission_upload_diag.py` 최상단 docstring: "no cFS, logging every
TX/RX frame for comparison with the cFS bridge"). 페이로드에 우연히 `0xFD`/`0xFE` 바이트가
섞이는 경우(부동소수점 좌표값 등, 드물지 않음) FC가 정상 응답해도 파서가 놓쳐서
"응답 없음"으로 오진단할 수 있음 — 향후 기체 디버깅 시 오탐 유발.

**정정 (2026-07-14)**: 처음엔 "`state=='STX'`일 때만 리싱크"로 고치려 했으나,
기존 테스트 `ParserRoundTripTest::test_v1_stx_resets_parser`가 **"프레임 파싱 도중에도
STX 바이트가 나오면 즉시 리셋해야 한다"**를 명시적으로 검증하고 있음을 확인 —
즉 현재 동작은 실수가 아니라 의도된 설계(동기화 유실 시 빠른 복구 우선)임.
상태 가드를 추가하면 이 테스트가 깨짐 (실제로 깨짐 확인 후 되돌림).

**결론**: 이건 "버그"가 아니라 **설계 트레이드오프**:
- 현재(항상 리싱크): 시리얼 노이즈로 동기화가 깨졌을 때 빠르게 복구 가능. 대신
  페이로드/CRC 바이트가 우연히 `0xFD`/`0xFE`와 같으면(부동소수점 좌표 인코딩 시
  드물지 않음) 정상 프레임도 놓친다 (`test_roundtrip_mission_item_int`가 이 케이스를 실증).
- 대안(상태 가드 추가): 정상 프레임은 절대 안 놓치지만, 진짜 동기화 유실 시
  다음 진짜 STX까지 자동 복구가 안 되고 CRC1/CRC2까지 다 채워야 재동기화됨.

두 테스트가 서로 반대 요구를 명시하고 있어 코드만으로는 둘 다 만족 불가 —
사용자 확인 후 방향 결정 필요.

## 3. 최종 결정 — 상태 가드 채택 (2026-07-14)

**결정**: "상태 가드 추가" 방향 채택. STX 리싱크는 유휴 상태(`state == 'STX'`, 즉
직전 프레임을 완결했거나 아직 아무 바이트도 안 받은 상태)에서만 허용. 프레임
파싱 도중(`LEN`~`CRC2`)에는 바이트 값이 `0xFD`/`0xFE`와 우연히 같아도 정상
페이로드/CRC 데이터로 소비한다.

**근거**:
- 실제 MAVLink 표준 파서(pymavlink 등)의 계약과 일치 — 페이로드는 이스케이프되지
  않으므로, LEN으로 명시된 만큼 소비 후 CRC로만 무결성을 판단하는 게 정석
- "항상 리싱크"의 실피해가 이미 실증됨(§2): 좌표 float 인코딩에 0xFE가 우연히
  섞이면 정상 응답을 놓쳐 "FC 무응답"으로 오진단 — 진단 툴의 존재 목적을 훼손
- "빠른 동기화 복구" 이점은 이 툴 사용 맥락에서 미미: 동기화가 실제로 깨져도
  잘못 읽은 프레임은 CRC 검증에서 걸러지고 다음 진짜 STX에서 재동기화됨.
  최대 지연은 가짜 LEN(최대 255+2바이트) 소비 시간 — 57600bps 기준 ~44ms,
  5초 타임아웃 단위로 동작하는 이 CLI 디버그 툴에서는 무시 가능

**계약 변경**:
- 기존: `feed()`는 상태 무관 모든 바이트에서 STX 값이면 리셋
- 신규: `feed()`는 `self.state == 'STX'`일 때만 STX 값에서 리셋(및 새 프레임 시작).
  파싱 도중 우연히 STX 값과 같은 바이트는 그대로 LEN/SEQ/PAYLOAD/CRC 데이터로 소비.
  진짜 동기화 유실 시엔 CRC 불일치로 걸러지고, 다음 유휴 상태 진입(=다음 정상
  프레임 완결 또는 스트림 재시작) 후에야 재동기화 — 이번 변경 범위에서 CRC 실패
  시 즉시 리셋하는 로직은 추가하지 않음(별도 개선 후보로 남김, 아래 상태 참조)

**구현 범위**:
- 수정: `tools/mission_upload_diag.py::_Parser.feed()` — 조건에 `and self.state == 'STX'` 추가
- 재작성: `tests/test_mission_upload_diag.py::test_v1_stx_resets_parser` — 새 계약에 맞춰
  "유휴 상태에서 STX 수신 시 새 프레임 시작"을 검증하도록 변경 (프레임 도중 STX는
  이제 데이터로 소비되어야 하므로 이름/내용 갱신)
- 회귀 대상: `ParserRoundTripTest::test_roundtrip_mission_item_int`가 이제 통과해야 함

## 상태

- [x] 원인 분석 완료 (2026-07-14)
- [x] 테스트 기대값 수정 (`test_crc_acc_known_value` → `0x0F87`), 통과 확인
- [x] 방향 결정 완료 (2026-07-14) — 상태 가드 채택, 근거·계약 변경 §3에 명시
- [x] `_Parser.feed()` 상태 가드 구현 (`and self.state == 'STX'`)
- [x] 테스트 재작성 — `test_v1_stx_resets_parser` → `test_v1_stx_resets_parser_when_idle`
      (유휴 상태 리셋 검증) + `test_stx_byte_mid_frame_is_consumed_as_data` 신규 추가
      (프레임 도중 0xFE가 데이터로 소비됨을 검증)
- [x] 전체 pytest 재실행 — 174 passed, 0 failed (known-failure 해소됨)
