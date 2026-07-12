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

## 11. 기체 C 수신 구현 세부 (Stage 3 착수 게이트)

`lora_stage_measurement_runbook.md` Stage 3의 3개 선행 게이트를 구현 수준으로 확정한다. 모두 실측과 무관한 로컬 코드/UT 작업이며, 아래 순서대로 진행한다.

### 11.1 `RunRxWindow` 버퍼 static/전역화

**현재 결함**: `lora_tdm_app.c:106` `RunRxWindow()`가 `char Buf[LINE_BUF_LEN]`를 호출마다 스택에 재선언 → RX창(현 300ms) 경계를 넘어가는 프레임의 중간 수신 상태가 유실. v2는 payload 255B 프레임이 RX창(100ms)을 초과할 수 있으므로(§7.1) **주기 간 수신 상태 유지가 필수**다.

**설계**: 파서 상태를 앱 데이터(또는 파일 static)로 승격한 수신 컨텍스트 구조체로 분리한다.

```c
typedef struct
{
    uint8  State;                     /* WAIT_MAGIC / GOT_MAGIC / GOT_LEN / READING_BODY / GOT_CRC1 */
    uint8  Magic;                     /* 0xD2/0xB2/0xA2 — v1 분기는 별도(§8) */
    uint16 BodyLen;                   /* magic·len 확정 후 남은 본문 길이 */
    uint16 BodyIndex;                 /* 지금까지 채운 본문 바이트 수 */
    uint16 Crc;                       /* 누적 CRC (본문 소비하며 갱신) */
    uint8  Body[LORA_TDM_APP_RX_MAX_FRAME];  /* 정적 최대 프레임 버퍼 */
} LORA_TDM_APP_RxParser_t;
```

`RunRxWindow()`는 이 구조체를 **리셋하지 않고** 창마다 이어서 채운다. 프레임 완료(CRC 일치) 또는 리셋 조건에서만 `State=WAIT_MAGIC`, `BodyIndex=0`로 되돌린다. `LORA_TDM_APP_RX_MAX_FRAME`은 UP2 최대(262B, §7.1)를 수용.

### 11.2 길이 기반 상태머신 (magic-collision 금지)

mavlink STX 결함(`mavlink_bridge_app_behavior_spec.md` §17)의 재답습을 막는다. **magic 바이트는 `WAIT_MAGIC` 상태에서만 프레임 시작으로 인식**하고, 본문 소비 중(`READING_BODY`)에는 magic 값과 무관하게 위치 기반으로 `BodyLen`바이트를 채운다.

- 프레임 경계는 `len` 필드로 확정(§3 재동기화 규칙과 동일).
- CRC 불일치 → 해당 프레임 폐기 후 `WAIT_MAGIC`로 복귀, **버려진 바이트 스트림에서 다음 magic부터 재스캔**(§8 첫 바이트 분기 재적용). 이때 `bridge/lora_downlink_decoder.py`의 `DownlinkStream`(참조 구현)과 **동일한 재동기 규칙**을 따른다. 주의: C는 UP2/ACK2(수신)을, DownlinkStream은 DL2(수신)를 파싱하므로 다른 프레임을 본다 — **공유되는 건 프레이밍 규율(magic 위치, len 기반 경계, CRC 방식, 재동기 알고리즘)이지, 같은 프레임을 양쪽이 파싱하는 게 아니다**.
- v1 공존: 첫 바이트가 ASCII면 기존 `\n` 줄버퍼 경로로 분기(§8). 즉 이 상태머신은 magic(0xD2/0xB2/0xA2) 진입 시에만 동작.

### 11.3 CRC16 C ↔ Python 교차검증 UT

**현재 결함**: C `LORA_TDM_APP_Crc16`(`lora_tdm_app_utils.c:21`)와 Python `crc16_ccitt`(`bridge/` 3개 파일에 각각 구현)가 같은 표준 테스트 벡터로 맞대본 적이 없다.

**요구**:
- 표준 벡터 고정: CRC-16/CCITT-FALSE `"123456789"` → `0x29B1`을 C UT와 Python 테스트 양쪽에 assert.
- 추가 공유 벡터(빈 입력, 1바이트, 실제 DL2/UP2 본문 샘플 몇 개)를 `tests/`에 공용 픽스처로 두고 C·Python이 동일 기대값을 검증.
- `bridge/`의 CRC 구현 3중복은 이 기회에 단일 모듈로 통합 검토(§10 참조 구현 정리와 함께).

> 세 게이트 모두 통과 후 §9 검증 요구사항(왕복·분할 수신·재동기·공존)으로 진행한다. 본 절은 설계 확정(2026-07-13)이며 코드는 미착수.
