# LoRa 링크 프로토콜 v2 명세 (바이너리) — 초안

작성: 2026-07-13. 상태: **구현·배포됨 (2026-07)** — `lora_tdm_app`에 DL2(`BuildDl2Frame`, SysTime 확장 블록 포함)/UP2(`ParseUp2Frame`)/ACK2(`ParseAck2Frame`) 구현, CONFIG `PARAM_DOWNLINK_PROTOCOL`(0=v1/1=v2)로 런타임 전환. Stage 3 타이밍(CYCLE 200ms) 적용 완료. (구 상태: 설계 확정 전 초안)

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
| 5 | ufb | u8 | UplinkFeedback — 0x00 OK / 0x01 CRC_FAIL / 0x02 SEQ_FAIL / 0x03 STATE_BLOCKED(health gate 차단, 2026-07-21) / 0x04 FAILED / 0x05 REJECT_VERSION / 0x06 REJECT_CLASS / 0x07 REJECT_LENGTH / 0x08 ROUTE_MISS / 0x09 REJECT_ROUTE / 0x0A REJECT_CHECKSUM / 0x0B REJECT_VIEWPOINT (0x04~0x0B는 BL-11, 2026-07-22 추가 — 상세: `lora_tdm_app_behavior_spec.md` §9.2) |
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
| 45 | uplink_last_seq | u16 | 기체가 마지막 수락한 uplink seq (BL-03, 2026-07-22) — 지상 재시작 시 이 값+1부터 재개(자가복구) |
| 47 | uplink_boot_count | u8 | 기체 부팅 카운터, uint8 wrap (BL-12/BL-03) — 재부팅 감지, 감소 시 지상은 자동거부 대신 "운영자 확인 필요" 플래그만 |
| 48 | crc | u16 | CRC-16/CCITT |

기본 길이 **50B** (에어타임 ~21ms @2.4KB/s). SysTime 확장 포함 시 58B.
uplink_last_seq/uplink_boot_count는 SysTime 블록(있으면) 뒤, CRC 앞에 항상 붙는다
("기존 끝에 추가" 결정, 2026-07-22) — SysTime 유무와 무관하게 CRC 직전 3바이트.

### 4.1 위치 saturation 정책

로컬 x/y/z가 ±327.67m를 초과하면 ±32767로 clamp하고 `flags` bit1을 세운다.
운용 반경이 상시 327m를 초과하는 미션이 확정되면 x/y를 i32로 승격하는 v2.1을 정의한다 (len 필드로 하위 호환).

### 4.2 SysTime 확장 블록 (선택)

`flags` bit0 = 1이면 offset 45(=sats 추가로 44→45로 밀림)에 8바이트 블록을 삽입하고
`uplink_last_seq`/`uplink_boot_count`/CRC가 그만큼 뒤로 밀린다(offset 53/55/56):

| 필드 | 형식 | 의미 |
| --- | --- | --- |
| sys_time_unix_usec | u64 | FC SYSTEM_TIME 기반 GPS UNIX epoch (µs). `mavlink_bridge` §16.2의 `LastSysTimeUnixUsec` |

**구현 완료 (2026-07-14)** — `notes/temp/gps_time_sync_164_implementation.md`.
당초 "1Hz(5주기마다 1회) 첨부"로 설계했으나, 실제로는 **캐시에 유효값이 있으면
매 다운링크 사이클(5Hz)마다 첨부**하도록 단순화 — 8바이트 추가 에어타임이
100ms RX창 대비 무시 가능한 수준(~1.4ms @57600baud)이라 주기 제한의 실익이
없고, 매 사이클 최신 캐시값을 실어 보내는 게 구현이 더 단순하고 지연도 낮음.
유효 시각 미확보(`TimeValid == 0`, mavlink_bridge의 `LastSysTimeUnixUsec == 0`과
동일 조건) 시 미첨부(47B), 버퍼 부족 시에도 미첨부 폴백.
전제였던 `FC_SYS_TIME_MID(0x1909)` SB 발행(§16.3)은 이미 구현 완료 상태였고,
lora_tdm이 구독하는 부분만 남아있었음 — 이번에 구독 추가로 완결.

### 4.3 waypoint readback 확장 블록 (선택, 2026-07-23 신설)

**배경**: 지상에서 mission route를 업로드(`ROUTE_UPDATE` class)할 수는 있었으나
기체에 실제로 뭐가 올라가 있는지 회수(readback)할 방법이 없었음(설계 결정
2026-07-23, `notes/temp/runtime_test_session_2026-07-22.md` "waypoint조회는
왜 없지" 참조). mission route 상한 16개×12바이트(X/Y/Z float)=192바이트로
현재 air rate(2.4KB/s)에서 <1초 — 별도 대역폭 상향 불요, 여러 사이클에
나눠 보내는 페이징만으로 충분.

**트리거**: 기존 `DIAGNOSTIC` 클래스(class=6) 재사용, 신규 MID 없음.
`DIAGNOSTIC_CMD_TLM_t.DiagTarget`으로 대상 앱 구분(기존엔 미사용 필드,
lora_tdm_app 단독 구독이라 불필요했으나 cfs_core_app도 구독 대상에
추가되며 처음 도입):

| DiagTarget | 대상 |
| --- | --- |
| 0 (기본값, 하위호환) | `lora_tdm_app` (기존 LINK_STATUS/RX_STATS/TX_STATS) |
| 1 | `cfs_core_app` (신규) |

`cfs_core_app`의 `DiagAction`(자체 네임스페이스, `CFS_CORE_APP_DiagAction_t`):
`ROUTE_READBACK_REQUEST = 3`(기존 LOG_LEVEL/LINK_STATUS/CAPTURE_TOGGLE는
미구현 예약값). 페이로드는 route 종류 지정용 1바이트(현재 mission만
지원, 값 무시하고 항상 mission 처리 — landing은 향후 확장).

**데이터 흐름**: `cfs_core_app`이 요청 수신 → `MissionRoute` 캐시를
`ROUTE_SNAPSHOT_MID`(신규, 0x1913, `ROUTE_UPDATE_TLM_t`와 동일 레이아웃
재사용)로 `lora_tdm_app`에 SB 발행(SB 내부 전송이라 크기 제약 없음,
192바이트 그대로 1메시지) → `lora_tdm_app`이 로컬 캐시에 저장하고
페이지 상태(`PageIndex=0`, `TotalPages=ceil(WaypointCount/2)`) 진입 →
이후 `BuildDl2Frame()` 매 사이클마다 완료 전까지 확장 블록 첨부.

**DL2 확장 블록** (`flags` bit2 = "waypoint 페이지 첨부", 기존 bit0
SysTime/bit1 saturation과 독립적으로 동시 첨부 가능 — 위치는 **꼬리
필드(`uplink_last_seq`/`uplink_boot_count`) 뒤, CRC 직전 고정**.
BL-03에서 확립한 "새 필드는 항상 끝에 추가"(tail 오프셋 불변) 원칙을
따름 — 최초 설계 시 SysTime과 tail 사이로 잘못 기술했던 것을 구현 중
정정, 2026-07-23):

| 필드 | 형식 | 의미 |
| --- | --- | --- |
| route_type | u8 | `CFS_CORE_APP_ROUTE_SEGMENT_*` (현재 MISSION_EXTENSION=1 고정) |
| page_index | u8 | 0-base 현재 페이지 |
| total_pages | u8 | 전체 페이지 수 |
| waypoints_in_page | u8 | 이 페이지에 실제 담긴 waypoint 수(1 또는 2 — 마지막 페이지 홀수 개수 대응) |
| waypoint[0] | float×3 | X/Y/Z (12바이트) |
| waypoint[1] | float×3 | X/Y/Z (12바이트, `waypoints_in_page==1`이면 0으로 패딩) |

총 28바이트/사이클. `page_index`가 `total_pages-1`에 도달하는 사이클을
마지막으로 자동 종료(기체 쪽 `ReadbackPending=false`). 지상은
`page_index`로 순서 재조립, `total_pages`로 완료 판정(누락 페이지는
다음 판정 갱신까지 불완전 상태 유지 — 재시도는 지상이 DIAGNOSTIC 요청
재전송으로 처리, 별도 재전송 프로토콜 없음 — 단순화).

**미완(후속 검토)**: landing route 지원, 페이지 유실 시 지상 쪽
불완전 상태 UI 표시, ground 측 GUI 패널(현재는 로그/디코더 레벨만).

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
   — BL-41(2026-07-23)로 전환값이 영속화되어 재부팅 후 재전송 불필요
   (runtime spec §12.2). 남은 결정은 컴파일타임 기본값의 v1→v2 변경뿐.
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
- ~~참조 구현 미동기화 (2026-07-13)~~ — **해소 (2026-07-13)**: `bridge/lora_downlink_decoder.py`
  `DL2_BASE_LEN` 44→45, `decode_dl2`/`encode_dl2`에 sats(offset 41) 반영 완료.
  기체측 C `LORA_TDM_APP_BuildDl2Frame()`(`lora_tdm_app_utils.c`, §4 §11.1/§11.2 게이트와
  함께 구현)과 오프셋 동일 확인. 남은 것: UP2 인코더/ACK2 파서는 아직 지상 Python에 없음
  (지상은 UP2 송신/ACK2 수신 쪽), CONFIG로 v1/v2 런타임 전환, 실기체 5Hz 검증.
- ~~`decode_dl2()` SYSTIME 플래그·길이 불일치 크래시~~ — **해소 (2026-07-14)**:
  `flags & DL2_FLAG_SYSTIME`은 켜져 있는데 `body_len`이 SysTime 블록 없는 길이인
  프레임(CRC는 통과)이 들어오면 `struct.unpack_from`이 `struct.error`를 던져 지상
  다운링크 디코더 프로세스 자체가 죽던 버그. 길이 재검증(`len(frame) >=
  DL2_BASE_LEN + DL2_SYSTIME_BLOCK_LEN + 2`) 후에만 SysTime 필드를 읽도록 수정 —
  조건 불충족 시 크래시 대신 `sys_time_unix_usec=None`으로 안전 처리. 회귀테스트:
  `test_lora_downlink_decoder.py::StreamingTest::
  test_systime_flag_set_but_block_missing_returns_none_not_crash`.
  (근거: `notes/temp/dl2_systime_flag_length_crash.md`, 커밋 `c1ec450`)

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
