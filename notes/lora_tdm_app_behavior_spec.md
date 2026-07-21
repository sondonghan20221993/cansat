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
- 프로토콜 v2: `notes/lora_protocol_v2_spec.md` — 바이너리 프레임(DL2/UP2/ACK2). **구현·배포됨** (2026-07, `BuildDl2Frame`/`ParseUp2Frame`/`ParseAck2Frame`). CONFIG `PARAM_DOWNLINK_PROTOCOL`(0=v1 텍스트, 1=v2 바이너리)로 런타임 전환. TDM 주기도 Stage 3 값(200ms, §7)으로 전환 완료.

## 5. 인터페이스

### 5.1 구독 MID

`lora_tdm_app`은 초기화 중 다음 MID를 구독한다. 모든 구독은 단일 파이프 `LORA_TDM_PIPE` (깊이 50, `lora_tdm_app.c:268`)로 수신한다.

기본 원칙: 이 앱의 구독은 기존 앱들과 동일하게 **`CFE_SB_Subscribe()`(기본 limit=4)를 기준**으로 한다.
정말 저빈도인 MID(명령·HK 요청)는 이 기본값을 그대로 따른다. 다만 아래 표의 6개 MID는
**문서화된 예외**로 `CFE_SB_SubscribeEx()` + 커스텀 `MsgLim`을 쓴다 — 이유는 표 아래 노트 참고.

| 심볼 | 값 | 목적 | 구독 함수 |
| --- | --- | --- | --- |
| `LORA_TDM_APP_CMD_MID_VALUE` | `0x18E0` | 명령 입력 (NOOP, RESET_COUNTERS) | `CFE_SB_Subscribe()` (기본) |
| `LORA_TDM_APP_SEND_HK_MID_VALUE` | `0x18E1` | HK 게시 요청 | `CFE_SB_Subscribe()` (기본) |
| `LORA_TDM_APP_DIAGNOSTIC_CMD_MID_VALUE` | `0x1910` | uplink_app 라우팅 diagnostic 명령 — payload 미해석, 링크 상태 요약 EVS만 출력 (2026-06-17 추가) | `CFE_SB_Subscribe()` (기본) |
| `LORA_TDM_APP_SYSTEM_HEALTH_MID_VALUE` | `0x1904` | `cfs_core_app` 시스템 헬스 캐시 갱신 | `CFE_SB_SubscribeEx()`, MsgLim=20 **(예외)** |
| `LORA_TDM_APP_FC_EKF_LOCAL_STATE_MID_VALUE` | `0x1905` | FC local position/velocity 캐시 갱신 | `CFE_SB_SubscribeEx()`, MsgLim=10 **(예외)** |
| `LORA_TDM_APP_FC_ATTITUDE_STATE_MID_VALUE` | `0x1906` | FC attitude 캐시 갱신 | `CFE_SB_SubscribeEx()`, MsgLim=10 **(예외)** |
| `LORA_TDM_APP_FC_GPS_RAW_STATE_MID_VALUE` | `0x1907` | FC GPS 캐시 갱신 | `CFE_SB_SubscribeEx()`, MsgLim=10 **(예외)** |
| `LORA_TDM_APP_FC_EKF_STATUS_MID_VALUE` | `0x1908` | FC EKF status 캐시 갱신 | `CFE_SB_SubscribeEx()`, MsgLim=10 **(예외)** |
| `LORA_TDM_APP_FC_SYS_TIME_MID_VALUE` | `0x1909` | FC GPS SysTime 캐시 갱신 — DL2 SysTime 확장 블록용 (2026-07 추가) | `CFE_SB_SubscribeEx()`, MsgLim=10 **(예외)** |
| `LORA_TDM_APP_UPLINK_STATUS_MID_VALUE` | `0x190A` | uplink_app 명령 처리 결과 — `UFB_SEQ_FAIL` 피드백 판정 (2026-07 추가) | `CFE_SB_Subscribe()` (기본) |

> **수정 완료 (2026-06-16, 실 Pi 런타임 2차 검증에서 발견 후 코드 수정)**: 위 MID들이 원래
> 다른 MID와 동일하게 `CFE_SB_Subscribe()`(기본 함수)를 썼는데, 이 경우 cFE 기본값
> `CFE_PLATFORM_SB_DEFAULT_MSG_LIMIT = 4`가 적용된다 — **MsgId별로 미처리 메시지 4개까지만
> 허용**하며, 이는 파이프 깊이(50)와는 별개의 제한이다(파이프 깊이를 늘려도 해결되지 않음).
>
> (당시 Stage 1 타이밍 기준 서술) `lora_tdm_app`의 cycle 주기는 약 1.3초(`OS_TaskDelay(1000ms)` + 최대 `RX_WINDOW_MS(300)`)인데, FC가
> `FC_ATTITUDE_STATE_MID`/`FC_EKF_LOCAL_STATE_MID`를 5 Hz(200ms)로 보내면 한 cycle 사이에 6~7개가 쌓여
> limit 4를 넘었다. 1차 수정(이 4개 MID만 SubscribeEx 적용) 후 재검증한 로그에서 그 4개는 해결됐지만,
> **`SYSTEM_HEALTH_MID`에서 동일 에러가 16회 연속(부팅 직후 5ms 안에 몰림) 추가로 확인됐다.** 원인을
> 코드에서 확인한 결과 — `cfs_core_app`은 `SYSTEM_HEALTH_MID`를 1Hz 주기가 아니라 **FC 상태 메시지가
> 들어올 때마다(ATTITUDE/EKF_LOCAL/GPS_RAW/EKF_STATUS/ROUTE_UPDATE 처리 직후) 매번 강제 발행**한다
> (`cfs_core_app_utils.c:193`, `CFS_CORE_APP_UpdateHealth(NowMs, true)` — `ForcePublish=true`는
> 1Hz 주기 제한을 우회함; 일치하는 서술: `cfs_core_app_behavior_spec.md` §11.1 "즉시 헬스 게시").
> 즉 실제로는 FC 입력과 같은 속도(최대 14Hz대)로 발행되어, "1Hz라 기본값 충분"은 잘못된 가정이었다.
> 실제 Pi 실행 로그(1차):
> ```
> CFE_SB 17: Msg Limit Err, MsgId 0x1904, pipe LORA_TDM_PIPE, sender CFS_CORE_APP
> CFE_SB 17: Msg Limit Err, MsgId 0x1906, pipe LORA_TDM_PIPE, sender MAVLINK_BRIDGE_APP
> ```
> 캐시는 최신값으로 덮어쓰는 구조라 치명적 데이터 손실은 아니었으나(드롭돼도 다음 메시지가 최신값 갱신),
> 불필요한 에러 이벤트가 계속 쌓였다. **조치 1**: `SYSTEM_HEALTH_MID` 포함 5개 MID를
> `CFE_SB_SubscribeEx(..., CFE_SB_DEFAULT_QOS, MsgLim)`로 변경(`lora_tdm_app.c`) — `SYSTEM_HEALTH_MID`는
> 합산 이벤트 빈도가 더 높을 수 있어 MsgLim=20, FC_* 4개는 MsgLim=10. `CMD`/`SEND_HK`만 진짜 저빈도라
> 기존 `CFE_SB_Subscribe()` 유지 — 앱 전체 구독 방식을 바꾼 게 아니라, 실측 근거가 있는 5개 MID에
> 한정한 예외 처리다.
>
> **2차 재검증(2026-06-16)**: 위 수정 후 재실행했더니 `0x1904` 에러는 줄었지만 같은 부팅 시점에
> `0x1905`/`0x1906`에서도 다시 발생. 전체 로그를 확인한 결과 **모든 Msg Limit Err(16건)가 부팅 후
> 130ms 안에만 몰려 있고 이후 60초+ 동안 0건** — 지속 문제가 아니라 **1회성 부팅 버스트**임을 확인.
> `rx_ms`(Pi 수신 시각)가 여러 메시지에서 동일하게 찍히는 패턴으로 보아, `mavlink_bridge_app`이
> `/dev/serial0`를 여는 시점에 **cFS가 안 떠 있던 동안 FC가 계속 보내서 커널 시리얼 버퍼에 쌓여있던
> 데이터를 한 번에 드레인**하는 것으로 판단됨 — 이 경우 MsgLim을 더 올려도 다운타임이 길면 버스트도
> 커지므로 근본 해결이 안 됨. **조치 2(근본 원인)**: `mavlink_bridge_app_utils.c`의
> `MAVLINK_BRIDGE_APP_OpenSerial()`에서 `tcsetattr()` 성공 직후 `tcflush(Fd, TCIFLUSH)` 추가 —
> 포트를 열 때 묵은 입력 버퍼를 비워서 버스트 자체를 없앰.

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
CYCLE_PERIOD_MS = 200 ms   (Stage 3, 5 Hz — 2026-07 전환. Stage 1은 1000 ms)
```

각 주기(`RunCycle`) 실행 순서:

1. **SB pipe drain** (`CFE_SB_POLL`): 대기 중인 모든 메시지를 처리한다.
   - FC 상태 MID → `UpdateCacheFromMsg()` 호출로 내부 캐시 갱신
   - SEND_HK → `ReportHousekeeping()` + `ReportLinkStatus()` 호출
   - CMD MID → dispatch (NOOP, RESET_COUNTERS)
2. **serial open**: `LoRaFd < 0`이면 `OpenSerial()` 시도.
3. **TX** (`RunTx`): FC 또는 SH downlink 패킷 1건 전송.
4. **RX 창** (`RunRxWindow`): `RX_WINDOW_MS(100)` 동안 serial 읽기.
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

`RunRxWindow()`는 `GetTimeMs() + RX_WINDOW_MS(100)`을 deadline으로 설정하고 반복한다.

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

### 9.2 UFB (Uplink Feedback Byte) 코드 (Phase 3.3)

downlink 패킷의 피드백 바이트(UFB)는 가장 최근 uplink 명령의 처리 결과를 지상국에 알린다 (§18.11.1 SEQ_FAIL).

| UFB 코드 | 값 | 의미 | 발생 조건 |
|---|---|---|---|
| `UFB_OK` | 0 | 정상 수신 | uplink frame CRC/길이 검증 통과 |
| `UFB_CRC_FAIL` | 1 | CRC 오류 | raw frame CRC mismatch, 길이 오류, framing 오류 |
| `UFB_SEQ_FAIL` | 2 | 시퀀스 거부 | uplink_app의 `UPLINK_STATUS_MID`에서 `LastCommandResult == REJECT_SEQUENCE` (§18.10.1) |

**구현 정책**:

1. **기본 상태**: `PendingUplinkFeedback = UFB_OK` (boot 또는 성공 시)
2. **CRC 오류 감지**: frame 검증 실패 시 `UFB_CRC_FAIL` 설정 (기존 구현)
3. **시퀀스 거부 감지** (Phase 3.3 추가):
   - lora_tdm_app이 `UPLINK_STATUS_MID` 구독
   - `LastCommandResult == 10 (REJECT_SEQUENCE)`이면 `UFB_SEQ_FAIL` 설정
     (2026-07-21 정정: 원문은 "3"이었으나 `default_uplink_app_msgdefs.h` 기준
     실제 값은 10 — 코드가 매직넘버 3을 그대로 써서 ROUTED(성공)를 오탐하던
     버그를 유발함, `lora_tdm_app_dispatch.c` 수정 및 회귀 테스트 추가 완료)
4. **downlink 게시**: TDM slot에서 피드백 바이트 포함하여 전송

**신호 흐름**:
```
uplink raw frame (lora_tdm_app RX)
  ↓ [CRC검증]
  ├─ CRC fail → PendingUplinkFeedback = UFB_CRC_FAIL
  └─ CRC pass → uplink_app으로 forward
           ↓
      uplink_app (ProcessUplinkCommand)
      [시퀀스검증] → UPLINK_STATUS_MID 발행
           ↓ (lora_tdm_app 구독)
      LastCommandResult = REJECT_SEQUENCE
           ↓
      PendingUplinkFeedback = UFB_SEQ_FAIL
           ↓
      downlink TDM slot에서 포함
```

**지상국 해석**:
- UFB=OK: 명령이 수락되었거나, 현재 pending 명령이 없음
- UFB=CRC_FAIL: 패킷 손상/framing 문제 → 재전송 권장
- UFB=SEQ_FAIL: 명령의 sequence가 거부됨 → sequence 재설정 또는 recovery 필요

## 11. 링크 상태 관리

`UpdateLinkState(AppData, NowMs)`:

```
elapsed = NowMs - LastAckTimestampMs

if elapsed > LINK_TIMEOUT_MS (5000):
    LinkState = DISCONNECTED (0)
elif NoAckCount >= LINK_LOSS_THRESHOLD (15):
    LinkState = DEGRADED (2)
else:
    LinkState = CONNECTED (1)
```

초기 상태: `LastAckTimestampMs = 0`, `NoAckCount = 0` → 앱 시작 직후 `elapsed > 5000`이므로 DISCONNECTED.

## 12. HK 텔레메트리 (`LORA_TDM_APP_HkPayload_t`)

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

## 17. 링크 상태 텔레메트리 (`LORA_TDM_APP_LinkStatusTlm_t`)

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

## 17. 이벤트 ID 목록

`lora_tdm_app/fsw/inc/lora_tdm_app_eventids.h` 기준 (코드 권위):

| EID | 심볼 | 유형 | 트리거 | 구현 |
| --- | --- | --- | --- | --- |
| 1 | `INIT_INF_EID` | INFO | 앱 초기화 완료 | ✅ |
| 2 | `NOOP_INF_EID` | INFO | NOOP 명령 수신 | ✅ |
| 3 | `RESET_INF_EID` | INFO | RESET_COUNTERS 수신 | ✅ |
| 4 | `MID_ERR_EID` | ERROR | 알 수 없는 MID 수신 | ✅ |
| 5 | `CMD_LEN_ERR_EID` | ERROR | 명령 길이 불일치 | ✅ |
| 6 | `CC_ERR_EID` | ERROR | 알 수 없는 CC 수신 | ✅ |
| 7 | `SERIAL_OPEN_ERR_EID` | ERROR | `/dev/serial0` 열기 실패 | ✅ |
| 8 | `SERIAL_WRITE_ERR_EID` | ERROR | TX write 실패 | ✅ |
| 9 | `SERIAL_READ_ERR_EID` | ERROR | RX read 실패 | ✅ |
| 10 | `ACK_PARSE_ERR_EID` | ERROR | "ACK," 프레임 파싱 실패 | ✅ |
| 11 | `CRC_FAIL_EID` | ERROR | UP frame CRC 불일치 | ✅ |
| 12 | `SEQ_FAIL_EID` | ERROR | ACK sequence echo 불일치 (`SeqEcho != LastSentSeq`, ACK/ACK2 공통 — `lora_tdm_app_utils.c:520-526`) | ✅ 구현 (2026-07, 타이밍 버그 수정 commit `48c8d12`) |
| 13 | `LINK_LOST_EID` | ERROR | LinkState → DISCONNECTED 전이 | ✅ |
| 14 | `LINK_DEGRADED_EID` | WARNING | LinkState → DEGRADED 전이 | ✅ |
| 15 | `LINK_RESTORED_EID` | INFO | LinkState → CONNECTED 복구 | ✅ |
| 16 | `PIPE_ERR_EID` | ERROR | SB 파이프 수신 오류 | ✅ |
| 17 | `SUB_ERR_EID` | ERROR | SB 구독 실패 | ✅ |
| 18 | `SB_SEND_ERR_EID` | ERROR | SB 메시지 송신 실패 | ✅ |
| 19 | `DIAGNOSTIC_CMD_EID` | INFO | `DIAGNOSTIC_CMD_MID` 수신 (2026-06-17 추가) | ✅ |

## 17. Serial 재열기 정책 <!-- 구 §13 -->

- `LoRaFd`는 초기화 시 `-1`로 설정한다.
- `RunCycle()` 진입마다 `LoRaFd < 0`이면 `OpenSerial()` 시도.
- `OpenSerial()` 실패 시 EVS ERROR 이벤트 발행, `LoRaFd = -1` 유지.
- 성공 시 O_RDWR, 57600 baud, 8N1, no flow control, blocking 모드로 설정.
- `RunTx()`와 `RunRxWindow()`는 `LoRaFd < 0`이면 즉시 반환한다.

## 17. 설정 상수 <!-- 구 §14 -->

| 상수 | 값 | 의미 |
| --- | --- | --- |
| `LORA_TDM_APP_CYCLE_PERIOD_MS` | `200` | TDM 주기 (ms) — Stage 3 (구 1000) |
| `LORA_TDM_APP_RX_WINDOW_MS` | `100` | RX 창 길이 (ms) — Stage 3 (구 300) |
| `LORA_TDM_APP_LINK_LOSS_THRESHOLD` | `15` | DEGRADED 전이 NoAckCount 임계값 — Stage 3 (구 3) |
| `LORA_TDM_APP_LINK_TIMEOUT_MS` | `5000` | DISCONNECTED 전이 elapsed 임계값 (ms) |
| `LORA_TDM_APP_UPLINK_FB_OK` | `0` | UplinkFeedback 정상 |
| `LORA_TDM_APP_UPLINK_FB_CRC_FAIL` | `1` | UplinkFeedback CRC 실패 |
| `LORA_TDM_APP_UPLINK_FB_SEQ_FAIL` | `2` | UplinkFeedback 시퀀스 실패 — 구현됨 (`UPLINK_STATUS_MID` 구독, `LastCommandResult==10(REJECT_SEQUENCE)` 시 설정, `lora_tdm_app_dispatch.c:105-108`; 2026-07-21 매직넘버 3→10 오류 수정) |
| `LORA_TDM_APP_LINK_DISCONNECTED` | `0` | 링크 상태: 단절 |
| `LORA_TDM_APP_LINK_CONNECTED` | `1` | 링크 상태: 정상 |
| `LORA_TDM_APP_LINK_DEGRADED` | `2` | 링크 상태: 저하 |

## 17. 미구현 항목 <!-- 구 §15 -->

- ~~`SEQ_FAIL` 경로~~: 구현 완료 (2026-07) — ACK SeqEcho 검증(`SEQ_FAIL_EID`)과 `UFB_SEQ_FAIL` 피드백(`UPLINK_STATUS_MID` 경유) 모두 동작. §13 EID 표, §12 UFB 참조.
- `PacketType` 전환 명령: 현재 외부 명령으로 `PacketType`을 전환하는 command code가 없다.

> 수정 완료(2026-06-16): `ReportLinkStatus()`가 `dispatch.c`의 SEND_HK 처리에서 호출되지 않던 dead code 문제를 발견 — `LORA_TDM_APP_ProcessCommandPacket()`의 SEND_HK 분기에 `LORA_TDM_APP_ReportLinkStatus()` 호출을 추가해 해소함.
