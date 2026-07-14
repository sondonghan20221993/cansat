# DL2 SYSTIME 플래그·길이 불일치 크래시 (2026-07-14 도출)

## 문제

`bridge/lora_downlink_decoder.py::decode_dl2()`가 `flags & DL2_FLAG_SYSTIME`만 보고
SysTime 8바이트 블록을 `struct.unpack_from("<Q", frame, DL2_BASE_LEN)`으로 읽는다.
`body_len`(frame[1])이 `DL2_BASE_LEN`(SysTime 블록 없음)인데 `flags` bit0만 켜진
프레임이 들어오면 `frame` 길이(47)가 unpack이 요구하는 최소 길이(53)보다 짧아
`struct.error`가 발생한다.

- `_try_parse_dl2()`는 `body_len`이 `DL2_BASE_LEN`~`DL2_BASE_LEN+DL2_SYSTIME_BLOCK_LEN`
  범위인지만 검증하고, `flags`와 `body_len`의 정합성은 검증하지 않음
- CRC16은 실제 수신 바이트의 무결성만 보장하지, `flags` 비트와 `body_len`이
  서로 논리적으로 맞는지는 보장하지 않음 — 두 필드가 서로 독립적으로 손상돼도
  우연히(1/65536) CRC를 통과할 수 있고, FC측 인코더 버그·프로토콜 버전 불일치로도
  이론상 발생 가능

## 왜 문제인가

- `decode_dl2()`의 예외가 `_try_parse_dl2()` → `DownlinkStream.feed()`까지 전파되고,
  `main()`은 `KeyboardInterrupt`만 catch — 예외 발생 시 지상국 다운링크 디코더
  프로세스 자체가 죽는다 (재시작 전까지 텔레메트리 수신·ACK 전송·CSV 로깅 전부 중단)
- 재현 확인 (2026-07-14):
  ```python
  body[1] = DL2_BASE_LEN   # SysTime 블록 없음
  body[4] = 0x01           # flags bit0 (SYSTIME) 켬
  # CRC는 정상 계산 → CRC 검증 통과 → decode_dl2()에서 struct.error
  ```

## 결정 및 구현

**입구에서 길이 재검증.** `flags & DL2_FLAG_SYSTIME`뿐 아니라
`len(frame) >= DL2_BASE_LEN + DL2_SYSTIME_BLOCK_LEN + 2`(CRC 포함 전체 프레임 길이)도
함께 확인한 뒤에만 SysTime 필드를 읽는다. 조건 불충족 시 크래시 대신 `sys_time_unix_usec=None`
(SYSTIME 없음과 동일하게 안전 처리) — C 코드 쪽에서 이미 쓰던 "clamp 후 사용" 패턴과 동일한
방어 원칙.

- 수정 파일: `bridge/lora_downlink_decoder.py::decode_dl2()`
- 대안(기각): `DecodeError` 반환 — CRC까지 통과한 프레임을 통째로 버리는 것보다,
  나머지 40바이트(자세/위치 등 핵심 텔레메트리)는 신뢰 가능하므로 SysTime 필드만
  누락 처리하는 편이 정보 손실이 적음

## 상태

- [x] 문제 확인 + 재현 스크립트 (2026-07-14)
- [x] 수정 적용 (2026-07-14)
- [x] `tests/TEST_CASES.md`에 회귀 케이스 명세 선기록 (2026-07-14)
- [x] 회귀 테스트 구현 — `test_lora_downlink_decoder.py::StreamingTest::
      test_systime_flag_set_but_block_missing_returns_none_not_crash`
- [x] 전체 pytest 재실행 — 175 passed, 0 failed
