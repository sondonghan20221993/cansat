#!/usr/bin/env python3
"""CRC16/CCITT-FALSE 교차검증 UT — lora_protocol_v2_spec.md §11.3.

3개 Python crc16_ccitt 구현(lora_downlink_decoder.py, lora_uplink_bridge.py,
lora_telemetry_bridge.py)과 C LORA_TDM_APP_Crc16(lora_tdm_app_utils.c)이
동일 알고리즘(init 0xFFFF, poly 0x1021, no reflection)인지 표준 벡터로 검증.

C측 대응 테스트: lora_tdm_app/unit-test/coveragetest/coveragetest_lora_tdm_app_utils.c
Test_Crc16_KnownVector — 반드시 같은 벡터를 써야 교차검증 의미가 있음.

사용법: python3 -m pytest bridge/test_crc16_cross_validation.py -v
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from lora_downlink_decoder import crc16_ccitt as crc16_downlink
from lora_uplink_bridge import crc16_ccitt as crc16_uplink
from lora_telemetry_bridge import crc16_ccitt as crc16_telemetry

# C Test_Crc16_KnownVector와 동일 벡터 — 표준 CRC-16/CCITT-FALSE 테스트 벡터.
STANDARD_VECTOR = b"123456789"
STANDARD_EXPECTED = 0x29B1

IMPLS = {
    "lora_downlink_decoder": crc16_downlink,
    "lora_uplink_bridge": crc16_uplink,
    "lora_telemetry_bridge": crc16_telemetry,
}


def test_standard_vector_all_impls():
    for name, fn in IMPLS.items():
        result = fn(STANDARD_VECTOR)
        assert result == STANDARD_EXPECTED, (
            f"{name}: crc16_ccitt(b'123456789') = 0x{result:04X}, "
            f"expected 0x{STANDARD_EXPECTED:04X} (C 구현과 불일치 — §11.3 위반)"
        )


def test_impls_agree_with_each_other():
    vectors = [b"", b"\x00", b"\xff", b"A", b"hello world", bytes(range(256))]
    for v in vectors:
        results = {name: fn(v) for name, fn in IMPLS.items()}
        assert len(set(results.values())) == 1, (
            f"입력 {v!r}에 대해 구현별 결과 불일치: {results}"
        )


def test_empty_input():
    for name, fn in IMPLS.items():
        assert fn(b"") == 0xFFFF, f"{name}: 빈 입력의 CRC는 init값(0xFFFF)이어야 함"


if __name__ == "__main__":
    test_standard_vector_all_impls()
    test_impls_agree_with_each_other()
    test_empty_input()
    print("PASS: 3개 Python 구현 모두 표준 벡터 일치, 상호 일치, C 구현과 일치")
