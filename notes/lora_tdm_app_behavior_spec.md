# lora_tdm_app 동작 명세

## 1. 목적

이 문서는 현재 이 저장소에서 구현된 `lora_tdm_app`의 동작을 정의한다.
코드와 정합된 명세서로, 리뷰, 통합, 테스트 수행에 활용한다.

코드와 이 문서가 서로 다르면, 코드를 조사의 원본으로 취급해야 한다.

## 2. 설계 배경

이전 구조:
- `lora_fc_downlink_app` — LoRa serial TX 전용. serial port를 점유하여 downlink만 송신.
- `bridge/lora_uplink_bridge.py` — Python 프로세스, LoRa serial RX 전용. UP frame을 읽어 UDP 1234로 uplink_app에 전달.

문제: 두 컴포넌트가 동일한 serial port를 동시에 접근하여 LoRa half-duplex 충돌 발생.

해결: `lora_tdm_app` 단일 cFS 앱이 serial port를 독점 소유하고 TDM(Time Division Multiplexing) 방식으로 TX → RX를 교대 수행한다.

## 3. 범위

이 명세는 다음을 다룬다.

- `lora_tdm_app` 메시지 구독 및 게시
- TDM 주기 구조 (TX → RX 창)
- FC downlink 및 시스템 헬스 downlink 패킷 형식
- ACK 및 UP frame 수신 처리
- UP frame → uplink_app SB 전달 경로
- 링크 상태 관리 (CONNECTED/DEGRADED/DISCONNECTED)
- HK 및 링크 상태 텔레메트리 게시
- serial 재열기 정책
- 설정 상수

이 명세는 다음을 다루지 않는다.

- `uplink_app` 내부 payload 검증 및 라우팅
- `mavlink_bridge_app` 내부 MAVLink 파싱
- `cfs_core_app` 헬스 분류 로직

## 4. 참조

- Source: `lora_tdm_app/fsw/src/lora_tdm_app.c` — 초기화, TDM 주기, TX, RX 창
- Source: `lora_tdm_app/fsw/src/lora_tdm_app_utils.c` — CRC, frame build/parse, 링크 상태, 캐시 갱신
- Source: `lora_tdm_app/fsw/src/lora_tdm_app.h` — Data 구조체 정의
- Source: `lora_tdm_app/fsw/src/lora_tdm_app_dispatch.c` — 명령 dispatch
- Config: `lora_tdm_app/config/default_lora_tdm_app_mission_cfg.h` — 타이밍 상수, 링크 임계값
- Config: `lora_tdm_app/config/default_lora_tdm_app_topicid_values.h` — MID 값
- Config: `lora_tdm_app/config/default_lora_tdm_app_msgstruct.h` — 메시지 구조체

## 5. 인터페이스

### 5.1 구독 MID

`lora_tdm_app`은 초기화 중 다음 MID를 구독한다. 모든 구독은 단일 파이프 `LORA_TDM_PIPE` (깊이 50, `lora_tdm_app.c:268`)로 수신한다.

| 심볼 | 값 | 목적 |
| --- | --- | --- |
| `LORA_TDM_APP_CMD_MID_VALUE` | `0x18E0` | 명령 입력 (NOOP, RESET_COUNTERS) |
| `LORA_TDM_APP_SEND_HK_MID_VALUE` | `0x18E1` | HK 게시 요청 |
| `LORA_TDM_APP_SYSTEM_HEALTH_MID_VALUE` | `0x1904` | `cfs_core_app` 시스템 헬스 캐시 갱신 |
| `LORA_TDM_APP_FC_EKF_LOCAL_STATE_MID_VALUE` | `0x1905` | FC local position/velocity 캐시 갱신 |
| `LORA_TDM_APP_FC_ATTITUDE_STATE_MID_VALUE` | `0x1906` | FC attitude 캐시 갱신 |
| `LORA_TDM_APP_FC_GPS_RAW_STATE_MID_VALUE` | `0x1907` | FC GPS 캐시 갱신 |
| `LORA_TDM_APP_FC_EKF_STATUS_MID_VALUE` | `0x1908` | FC EKF status 캐시 갱신 |

### 5.2 게시 MID

| 심볼 | 값 | 목적 |
| --- | --- | --- |
| `LORA_TDM_APP_HK_TLM_MID_VALUE` | `0x08E0` | HK 텔레메트리 |
| `LORA_TDM_APP_LINK_STATUS_MID_VALUE` | `0x1911` | LoRa 링크 상태 텔레메트리 (구 `0x190F` → `uplink_app MODE_CMD_MID`와 충돌하여 재할당) |
| `UPLINK_APP_CMD_MID_VALUE` | `0x18D0` | UP frame forward (수신된 uplink를 uplink_app에 전달) |

## 6. 내부 상태 캐시

### 6.1 FC 상태 캐시 (`LORA_TDM_APP_FcStateCache_t`)

| 필드 | 갱신 MID | 의미 |
| --- | --- | --- |
| RollRad, PitchRad, YawRad | 0x1906 | FC 자세 (rad) |
| AttitudeValid | 0x1906 | 수신 여부 플래그 |
| PosX, PosY, PosZ | 0x1905 | NED 위치 (m) |
| VelX, VelY, VelZ | 0x1905 | NED 속도 (m/s) |
| LocalValid | 0x1905 | 수신 여부 플래그 |
| LatE7, LonE7, AltMm | 0x1907 | GPS 절대 위치 |
| GpsFix | 0x1907 | fix type |
| GpsValid | 0x1907 | 수신 여부 플래그 |
| EkfValid | 0x1908 | EKF 상태 플래그 유효성 |
| TimestampMs | 최신 갱신 MID | CFE_TIME 기반 mission elapsed ms |

### 6.2 시스템 헬스 캐시 (`LORA_TDM_APP_SystemHealthCache_t`)

| 필드 | 갱신 MID | 의미 |
| --- | --- | --- |
| SystemHealthState | 0x1904 | cfs_core_app 헬스 상태 |
| FaultCode | 0x1904 | 현재 fault code |
| TimestampMs | 0x1904 | CFE_TIME 기반 mission elapsed ms |

## 7. TDM 주기 구조

`LORA_TDM_APP_Main()`은 `OS_TaskDelay(CYCLE_PERIOD_MS)` 후 `RunCycle()`을 반복한다.

```
CYCLE_PERIOD_MS = 1000 ms
```

각 주기(`RunCycle`) 실행 순서:

1. **SB pipe drain** (`CFE_SB_POLL`): 대기 중인 모든 메시지를 처리한다.
   - FC 상태 MID → `UpdateCacheFromMsg()` 호출로 내부 캐시 갱신
   - SEND_HK → `ReportHousekeeping()` + `ReportLinkStatus()` 호출
   - CMD MID → dispatch (NOOP, RESET_COUNTERS)
2. **serial open**: `LoRaFd < 0`이면 `OpenSerial()` 시도.
3. **TX** (`RunTx`): FC 또는 SH downlink 패킷 1건 전송.
4. **RX 창** (`RunRxWindow`): 300 ms 동안 serial 읽기.
5. **링크 상태 갱신** (`UpdateLinkState`): 현재 시각 기준으로 상태 재계산.

## 8. TX 동작

`RunTx()`는 `PacketType` 필드에 따라 두 가지 중 하나를 전송한다.

| PacketType | 형식 |
| --- | --- |
| FC State (`LORA_TDM_APP_FC_STATE_PACKET_TYPE` = 1) | `FC,<seq>,<ts>,<roll>,<pitch>,<yaw>,<x>,<y>,<z>,<vx>,<vy>,<vz>,<lat_e7>,<lon_e7>,<alt_mm>,<fix>,<ufb>\n` |
| SYSTEM_HEALTH (`LORA_TDM_APP_SYSTEM_HEALTH_PACKET_TYPE` = 2) | `SH,<seq>,<ts>,<state>,<fault>,<linkstate>,<ufb>\n` |

필드 상세:
- `<seq>`: `DownlinkSeq` (전송 성공마다 1 증가)
- `<ts>`: FC 상태 캐시의 `TimestampMs` (FC 패킷) 또는 SH 캐시의 `TimestampMs` (SH 패킷)
- `<roll>/<pitch>/<yaw>`: float, 소수점 4자리 (rad)
- `<x>/<y>/<z>/<vx>/<vy>/<vz>`: float, 소수점 4자리 (m, m/s)
- `<lat_e7>/<lon_e7>`: 정수 ×10⁷
- `<alt_mm>`: 정수 (mm)
- `<fix>`: GPS fix type (unsigned)
- `<ufb>`: UplinkFeedback — 0x00=OK, 0x01=CRC_FAIL, 0x02=SEQ_FAIL
- `<state>/<fault>/<linkstate>`: unsigned 정수
- `\n`: 줄바꿈 종단

전송 성공 시: `TxCount++`, `DownlinkSeq++`, `PendingUplinkFeedback = UPLINK_FB_OK`.
전송 실패 시: EVS ERROR 이벤트, 카운터 갱신 없음.

## 9. RX 창 동작

`RunRxWindow()`는 `GetTimeMs() + RX_WINDOW_MS(300)`을 deadline으로 설정하고 반복한다.

- deadline 초과 또는 `read()` ≤ 0 시 즉시 종료.
- 1바이트씩 읽어 줄 버퍼 누적 (최대 `LORA_TDM_APP_LINE_BUF_LEN - 1`).
- `'\n'` 수신 시 `ProcessRxLine(line, AppData)` 호출.

`ProcessRxLine()`은 line 접두사로 분기한다.

| 접두사 | 처리 |
| --- | --- |
| `ACK,` | `ParseAckFrame()` → `LastAckTimestampMs = UtilsGetTimeMs()`, `NoAckCount = 0`, `RxAckCount++` |
| `UP,` | `ProcessUpFrame()` → CRC 검증 → hex 디코딩 → `CFE_SB_TransmitMsg` to `UPLINK_APP_CMD_MID` |
| 기타 | 무시 (카운터 갱신 없음) |

RX 창 종료 시 `RxAckCount == 0`이었으면 `NoAckCount++` (최대 0xFFFF).

### 9.1 UP frame 처리 세부

UP 프레임 형식:
```
UP,<version>,<command_class>,<sequence>,<flags>,<payload_hex>,<crc16_hex>
```

CRC 검증 대상: `UP,<version>,<command_class>,<sequence>,<flags>,<payload_hex>` (첫 6개 필드 comma 결합 문자열).
알고리즘: CRC-16/CCITT-FALSE (init=0xFFFF, poly=0x1021).

`<payload_hex>`가 빈 문자열인 프레임(payload 없는 명령)도 유효하다. 파싱은 1차로 `%[^,]` 기반 sscanf를 시도하고, 빈 payload로 인해 실패하면 `,,` 리터럴을 포함한 대체 포맷으로 재시도한다 (`lora_tdm_app_utils.c` `ProcessUpFrame`, 2026-06-16 수정 — 이전에는 빈 payload 프레임이 전부 CRC_FAIL로 오판되던 버그가 있었음, `Test_ProcessRxLine_ValidUp`로 검증).

CRC 불일치 시: `PendingUplinkFeedback = UPLINK_FB_CRC_FAIL`, `RxErrorCount++`, 전달 안 함.
CRC 통과 시: `LORA_TDM_APP_UplinkFwdCmd_t` 구성 → `CFE_MSG_Init()` + `CFE_SB_TransmitMsg()` to `UPLINK_APP_CMD_MID_VALUE (0x18D0)`.

`LORA_TDM_APP_UplinkFwdCmd_t` 필드 매핑:
- `Version = version`
- `CommandClass = command_class`
- `Sequence = sequence`
- `Flags = flags`
- `PayloadLength = decoded_len`
- `Payload[0..decoded_len-1] = decoded_bytes`

## 10. 링크 상태 관리

`UpdateLinkState(AppData, NowMs)`:

```
elapsed = NowMs - LastAckTimestampMs

if elapsed > LINK_TIMEOUT_MS (5000):
    LinkState = DISCONNECTED (0)
elif NoAckCount >= LINK_LOSS_THRESHOLD (3):
    LinkState = DEGRADED (2)
else:
    LinkState = CONNECTED (1)
```

초기 상태: `LastAckTimestampMs = 0`, `NoAckCount = 0` → 앱 시작 직후 `elapsed > 5000`이므로 DISCONNECTED.

## 11. HK 텔레메트리 (`LORA_TDM_APP_HkPayload_t`)

`ReportHousekeeping()`이 게시하는 필드:

| 필드 | 소스 |
| --- | --- |
| CommandCounter | `CmdCounter` |
| CommandErrorCounter | `ErrCounter` |
| LinkState | `LinkState` |
| PacketType | `PacketType` |
| AttitudeValid, LocalValid, GpsValid, EkfValid | FcState 캐시 valid 플래그 |
| SystemHealthState | SystemHealth 캐시 |
| PendingUplinkFeedback | `PendingUplinkFeedback` |
| TxCount, RxAckCount, RxCmdCount, RxErrorCount, NoAckCount | 카운터 |
| LastAckTimestampMs | `LastAckTimestampMs` |

## 12. 링크 상태 텔레메트리 (`LORA_TDM_APP_LinkStatusTlm_t`)

`ReportLinkStatus()`가 게시하는 필드:

| 필드 | 소스 |
| --- | --- |
| Seq | `DownlinkSeq` |
| TimestampMs | `GetTimeMs()` |
| LinkState | `LinkState` |
| LastAckTimestampMs | `LastAckTimestampMs` |
| NoAckCount | `NoAckCount` |
| RxErrorCount | `RxErrorCount` |
| TxCount | `TxCount` |
| RxAckCount | `RxAckCount` |
| RxCmdCount | `RxCmdCount` |

## 13. Serial 재열기 정책

- `LoRaFd`는 초기화 시 `-1`로 설정한다.
- `RunCycle()` 진입마다 `LoRaFd < 0`이면 `OpenSerial()` 시도.
- `OpenSerial()` 실패 시 EVS ERROR 이벤트 발행, `LoRaFd = -1` 유지.
- 성공 시 O_RDWR, 57600 baud, 8N1, no flow control, blocking 모드로 설정.
- `RunTx()`와 `RunRxWindow()`는 `LoRaFd < 0`이면 즉시 반환한다.

## 14. 설정 상수

| 상수 | 값 | 의미 |
| --- | --- | --- |
| `LORA_TDM_APP_CYCLE_PERIOD_MS` | `1000` | TDM 주기 (ms) |
| `LORA_TDM_APP_RX_WINDOW_MS` | `300` | RX 창 길이 (ms) |
| `LORA_TDM_APP_LINK_LOSS_THRESHOLD` | `3` | DEGRADED 전이 NoAckCount 임계값 |
| `LORA_TDM_APP_LINK_TIMEOUT_MS` | `5000` | DISCONNECTED 전이 elapsed 임계값 (ms) |
| `LORA_TDM_APP_UPLINK_FB_OK` | `0` | UplinkFeedback 정상 |
| `LORA_TDM_APP_UPLINK_FB_CRC_FAIL` | `1` | UplinkFeedback CRC 실패 |
| `LORA_TDM_APP_UPLINK_FB_SEQ_FAIL` | `2` | UplinkFeedback 시퀀스 실패 (미구현) |
| `LORA_TDM_APP_LINK_DISCONNECTED` | `0` | 링크 상태: 단절 |
| `LORA_TDM_APP_LINK_CONNECTED` | `1` | 링크 상태: 정상 |
| `LORA_TDM_APP_LINK_DEGRADED` | `2` | 링크 상태: 저하 |

## 15. 미구현 항목

- `SEQ_FAIL` 경로: `UPLINK_FB_SEQ_FAIL` 상수가 정의되어 있으나, sequence 단조 증가 검증 및 피드백 전송 로직이 구현되지 않았다.
- `PacketType` 전환 명령: 현재 외부 명령으로 `PacketType`을 전환하는 command code가 없다.

> 수정 완료(2026-06-16): `ReportLinkStatus()`가 `dispatch.c`의 SEND_HK 처리에서 호출되지 않던 dead code 문제를 발견 — `LORA_TDM_APP_ProcessCommandPacket()`의 SEND_HK 분기에 `LORA_TDM_APP_ReportLinkStatus()` 호출을 추가해 해소함.
