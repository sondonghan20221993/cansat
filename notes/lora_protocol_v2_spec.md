# LoRa 링크 프로토콜 v2 명세 (바이너리) — 초안

작성: 2026-07-13. 상태: **설계 확정 전 초안 — 코드 미구현.**

## 1. 목적

현행 텍스트(CSV/hex) 프로토콜을 바이너리로 교체하여:

1. 다운링크 실효 갱신율 0.77Hz → **5Hz** (TDM 주기 1000ms → 200ms)
2. FC/SH 패킷의 last-wins 슬롯 경합 제거 (**통합 프레임**)
3. 업링크 긴급 명령(RECOVERY 등) 최악 대기 1.3s → 0.2s
4. 향후 필드 추가 여유 확보 (SysTime UTC 등 — `mavlink_bridge_app_behavior_spec.md` §16)

하드웨어 전제: MicoAir **LR24-F** (2.4GHz LoRa FHSS, air rate 기본 2.4KByte/s, UART 57600, 투명 전송).
에어타임 근거: v2 통합 프레임 46B ≈ 19ms, v1 FC 텍스트 ~130B ≈ 54ms @2.4KB/s.

## 2. 범위

- 다운링크 통합 프레임(DL2), 업링크 명령 프레임(UP2), ACK 프레임(ACK2)의 wire format
- TDM 타이밍 v2 및 링크 상태 임계값 재조정
- v1 텍스트 프로토콜과의 공존/이행 절차

다루지 않음: SB MID 계약(불변), uplink_app 내부 검증, LR24-F 모듈 설정 자체.

## 3. 공통 규칙

| 항목 | 값 |
| --- | --- |
| 바이트 오더 | Little-endian (기존 MAVLink 파싱 관례와 동일) |
| CRC | 기존 `LORA_TDM_APP_Crc16()` (CRC-16/CCITT, poly 0x1021) 재사용. 대상 = magic부터 payload 끝까지 (CRC 자신 제외) |
| 프레임 구분 | 첫 바이트 magic. v1 텍스트('F','S','A','U' = ASCII)와 겹치지 않는 값 사용 → 수신기가 v1/v2 즉시 판별 |
| 재동기화 | magic 스캔. len 필드로 프레임 경계 확정, CRC 실패 시 해당 바이트부터 재스캔 |
| 종단 문자 | 없음 (v1의 `\n` 종단 폐지 — 바이너리 페이로드에 0x0A 등장 가능) |

magic 할당: DL2=`0xD2`, UP2=`0xB2`, ACK2=`0xA2`.

## 4. DL2 — 다운링크 통합 프레임 (기체 → 지상)

FC 상태 + 시스템 헬스를 **하나의 프레임**으로 매 TDM 주기 전송. v1의 `PacketType` last-wins 스케줄링은 폐지한다.

| offset | 필드 | 형식 | 단위/의미 |
| --- | --- | --- | --- |
| 0 | magic | u8 | `0xD2` |
| 1 | len | u8 | magic부터 CRC 직전까지 길이 (확장 블록 포함) |
| 2 | seq | u16 | DownlinkSeq (성공마다 +1, wrap 허용) |
| 4 | flags | u8 | bit0=SysTime 블록 첨부, bit1=위치 saturate 발생, bit2~7 예약(0) |
| 5 | ufb | u8 | UplinkFeedback (0x00 OK / 0x01 CRC_FAIL / 0x02 SEQ_FAIL) |
| 6 | ts | u32 | FC 상태 캐시 TimestampMs (FC boot ms) |
| 10 | roll, pitch, yaw | i16 ×3 | rad ×10⁴ (±3.2767 rad → ±π 커버) |
| 16 | x, y, z | i16 ×3 | cm (±327.67m, §4.1 saturation) |
| 22 | vx, vy, vz | i16 ×3 | cm/s (±327.67m/s) |
| 28 | lat, lon | i32 ×2 | deg ×10⁷ (v1과 동일) |
| 36 | alt_mm | i32 | mm |
| 40 | fix | u8 | GPS fix type |
| 41 | sats | u8 | SatellitesVisible (2026-07-13 추가 — v1과 동일하게 fix 옆에 배치) |
| 42 | health | u8 | SystemHealthState |
| 43 | fault | u8 | FaultCode |
| 44 | linkstate | u8 | LoRa LinkState |
| 45 | crc | u16 | CRC-16/CCITT |

기본 길이 **47B** (에어타임 ~20ms @2.4KB/s, sats 1바이트 추가로 46B→47B).

### 4.1 위치 saturation 정책

로컬 x/y/z가 ±327.67m를 초과하면 ±32767로 clamp하고 `flags` bit1을 세운다.
운용 반경이 상시 327m를 초과하는 미션이 확정되면 x/y를 i32로 승격하는 v2.1을 정의한다 (len 필드로 하위 호환).

### 4.2 SysTime 확장 블록 (선택)

`flags` bit0 = 1이면 offset 45(=sats 추가로 44→45로 밀림)에 8바이트 블록을 삽입하고 CRC가 그 뒤로 밀린다:

| 필드 | 형식 | 의미 |
| --- | --- | --- |
| sys_time_unix_usec | u64 | FC SYSTEM_TIME 기반 GPS UNIX epoch (µs). `mavlink_bridge` §16.2의 `LastSysTimeUnixUsec` |

첨부 주기: 1Hz (5주기마다 1회). 유효 시각 미확보(`LastSysTimeUnixUsec == 0`) 시 첨부하지 않는다.
전제: `FC_SYS_TIME_MID(0x1909)` SB 발행 구현 (§16.3, 미구현) 후 lora_tdm이 구독.

## 5. UP2 — 업링크 명령 프레임 (지상 → 기체)

v1 `UP,<version>,<class>,<seq>,<flags>,<payload_hex>,<crc16>\n`의 바이너리 대체. hex 인코딩 폐지로 payload가 원본 크기 그대로 실린다 (v1 대비 payload 구간 50% 절감).

| offset | 필드 | 형식 | 의미 |
| --- | --- | --- | --- |
| 0 | magic | u8 | `0xB2` |
| 1 | plen | u8 | payload 길이 (0 허용 — payload 없는 명령 유효, v1 규칙 승계) |
| 2 | version | u8 | 프로토콜 버전 = 2 |
| 3 | command_class | u8 | `UPLINK_APP_CommandClass_t` (1=CONFIG … 6=DIAGNOSTIC) |
| 4 | seq | u16 | 업링크 시퀀스 |
| 6 | flags | u8 | 예약 (0) |
| 7 | payload | u8 ×plen | 명령 페이로드 (raw) |
| 7+plen | crc | u16 | CRC-16/CCITT |

수신 처리는 v1과 동일하게 CRC 검증 → `UPLINK_APP_CMD_MID`로 SB 전달. CRC 실패 → `PendingUplinkFeedback = CRC_FAIL`.

## 6. ACK2 — 지상 ACK 프레임 (지상 → 기체)

| offset | 필드 | 형식 |
| --- | --- | --- |
| 0 | magic | u8 = `0xA2` |
| 1 | seq_echo | u16 (마지막 수신 DL2 seq) |
| 3 | crc | u16 |

5B. v1 `ACK,<seq>\n` 대비 기능 동일 + CRC 보호 추가. seq_echo 검증(SEQ_FAIL 판정)은 기존 §18.11.1 갭 그대로 — 본 spec 범위 외.

## 7. TDM 타이밍 v2

| 파라미터 | v1 | v2 | 근거 |
| --- | --- | --- | --- |
| `CYCLE_PERIOD_MS` | 1000 | **200** | DL2 19ms + RX창 + 마진 |
| `RX_WINDOW_MS` | 300 | **100** | UP2 최대(payload 255B → 262B ≈ 110ms)는 초과 — §7.1 |
| 실효 다운링크 | ~0.77Hz | **5Hz** | |
| `LINK_TIMEOUT_MS` | 5000 | 5000 (유지) | 절대시간 기준이라 불변 |
| `LINK_LOSS_THRESHOLD` (NoAck 연속) | 3 (≈3.9s) | **15** (≈3s) | 주기 단축 보정 — 3 유지 시 0.6s만에 DEGRADED로 과민 |

### 7.1 대형 업링크(ROUTE_UPDATE) 분할 수신

RX창 100ms에 들어가는 UP2 최대 크기는 ~240B(에어타임 100ms). payload 255B 프레임은 창을 초과할 수 있으므로, RX 파서는 **줄 단위가 아니라 바이트 스트림 상태머신**으로 구현하여 프레임이 여러 RX창에 걸쳐 수신되는 것을 허용한다 (수신 중간 상태를 주기 간 유지). 이것이 v1 `\n` 줄버퍼 방식과의 가장 큰 구현 차이다.

### 7.2 지상국 타이밍

지상국(bridge)은 DL2 수신 직후 RX창이 열려 있는 동안 ACK2/UP2를 송신해야 한다. DL2 수신 완료 시점부터 ~100ms 이내 응답 요구 — 지상 브리지의 처리 지연 예산에 명시.

## 8. v1 공존 및 이행 절차

수신 파서(기체 RX / 지상 bridge 공통)는 첫 바이트로 분기한다:

| 첫 바이트 | 처리 |
| --- | --- |
| `0xD2` / `0xB2` / `0xA2` | v2 바이너리 상태머신 |
| ASCII (`A`,`U`,`F`,`S` 등) | v1 텍스트 파서 (기존 경로 유지) |

이행 순서 (링크 양단 동시 교체 불가 전제):

1. **지상 bridge에 v2 수신 지원 추가** (v1 송신 유지) — 배포
2. 기체 lora_tdm에 v2 송신(DL2) + v2 수신(UP2/ACK2) 추가, **송신 포맷은 CONFIG 파라미터로 v1/v2 선택** (기본 v1)
3. 지상에서 CONFIG 명령으로 v2 전환 → 검증 → 기본값 v2로 릴리스
4. 안정화 후 v1 송신 경로 제거 (수신 파서의 v1 분기는 진단용으로 존치)

## 9. 검증 요구사항

| 항목 | 방법 |
| --- | --- |
| DL2 인코딩/디코딩 왕복 | 기체 인코더 ↔ 지상 디코더 단위테스트 (saturation, SysTime 블록 유/무 포함) |
| UP2 다중 RX창 분할 수신 | 프레임을 임의 지점에서 쪼개 주입하는 UT |
| CRC/재동기화 | 프레임 중간 바이트 손상 주입 → 재스캔으로 후속 프레임 정상 수신 |
| v1/v2 공존 | 혼합 스트림 주입 UT |
| 실링크 5Hz 유지율 | Pi + LR24-F 실물, 1시간 soak — TxCount/RxAckCount 비율, NoAckCount 추이 |
| 타이밍 | RX창 내 ACK 왕복 실측 (지상 bridge 응답 지연 포함) |

## 10. 미결정/후속 항목

- x/y i32 승격(v2.1) 트리거가 되는 운용 반경 기준 — 미션 요구 확정 대기
- FC 스트림 상향(ATTITUDE 10Hz) 시 `CYCLE_PERIOD_MS` 100ms 재검토 — 본 spec 범위 외
- LR24-F 모듈 설정(air rate/채널/패킷화 지연)의 공식 문서화 — `notes/test_environment.md`에 추가 필요
- 2.4GHz 대역 간섭: WFB-ng 영상 링크는 5.8GHz 채널 사용 필수 (LR24-F와 대역 분리) — 시스템 통합 노트에 명시 필요
- **참조 구현 미동기화 (2026-07-13)**: `bridge/lora_downlink_decoder.py`의 `DL2_BASE_LEN`(44)과
  `decode_dl2`/`encode_dl2`의 오프셋 언패킹이 아직 §4의 sats 추가(46B→47B, offset 41)를
  반영하지 못했다. v1(`lora_tdm_app`)에는 이미 sats를 반영했지만 v2는 설계만 갱신된 상태 —
  실제 v2 구현(기체 C 인코더 + 참조 디코더) 착수 시 이 offset을 최신 표대로 맞춰야 한다.
