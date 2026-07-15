# mission_upload_diag 테스트 실패 2건 원인 분석 (2026-07-14 도출)

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
