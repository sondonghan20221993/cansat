# lora_tdm_app 동작 명세

## 1. 목적

이 문서는 `lora_tdm_app`의 설계 및 동작을 정의한다.

`lora_tdm_app`은 기존 `lora_fc_downlink_app`(TX 전용)과 `bridge/lora_uplink_bridge.py`(RX 전용)를 단일 cFS C 앱으로 대체하여, 반이중 LoRa 채널의 TX/RX 타이밍을 cFS 내부에서 직접 제어한다.

Python 브리지(`lora_uplink_bridge.py`)는 레거시 경로로 유지되며 이 앱과 동시에 실행되지 않는다.

## 2. 배경 및 설계 동기

### 2.1 기존 구조의 문제

```
[기존]
lora_fc_downlink_app (C, cFS) → /dev/ttyUSB0  TX (연속 점유)
lora_uplink_bridge.py (Python) ← /dev/ttyUSB0  RX
```

- 동일 serial 포트 동시 접근
- TX 연속 점유로 인해 GS 명령 수신 불가
- Python이 cFS SB 외부에서 UDP 우회로를 통해 명령 주입 (cFS 설계 원칙 위반)

### 2.2 채택된 해결 방향

단일 cFS C 앱이 serial 포트를 **단독 소유**하고, TDM(Time Division Multiplexing) 방식으로 TX/RX 타이밍을 제어한다.

```
[변경 후]
lora_tdm_app (C, cFS) ↔ /dev/ttyUSB0
  ├─ TX: 다운링크 텔레메트리 송신
  ├─ RX window: ACK 또는 명령 수신 대기
  └─ SB publish: 수신 명령 → uplink_app 또는 직접 ROUTE_UPDATE_MID
```

## 3. 범위

이 명세는 다음을 다룬다.

- TDM 사이클 구조 (TX → RX window → 반복)
- ACK 패킷 프로토콜
- 업링크 명령 수신 및 SB 게시 경로
- serial 포트 소유 및 재연결 처리
- 링크 상태 모니터링 및 `LORA_LINK_STATUS_MID` 게시
- MID 계약 및 타이밍 파라미터

이 명세는 다음을 다루지 않는다.

- 다운링크 텔레메트리 패킷 포맷 (→ `lora_fc_downlink_app` 기존 포맷 재사용)
- 업링크 명령 의미 검증 (→ `uplink_app` 책임)
- GS(지상국) 측 소프트웨어 구현

## 4. 참조

- 기존 다운링크: `lora_fc_downlink_app/fsw/src/`
- 레거시 uplink 브리지: `bridge/lora_uplink_bridge.py`
- uplink 프레임 계약: `notes/lora_uplink_bridge_design.md`
- MID 계약 베이스라인: `notes/mission_app_runtime_spec.md` §5.1.1

## 5. 책임

`lora_tdm_app`의 책임:

- LoRa serial 포트 단독 소유 및 재연결 관리
- 구독한 FC 상태 MID 및 `SYSTEM_HEALTH_MID`로부터 다운링크 패킷 구성
- TDM 사이클에 따라 다운링크 전송 후 RX window 진입
- RX window에서 ACK 또는 UP 프레임 수신
- 수신된 UP 프레임을 cFS SB에 직접 게시 (`UPLINK_APP_CMD_MID` 또는 직접 MID)
- 링크 상태(`LORA_LINK_STATUS_MID`) 주기적 게시
- HK 요청 수신 시 HK 텔레메트리 게시

`lora_tdm_app`이 수행하지 않는 것:

- 업링크 명령의 의미 검증 (route geometry, CRC 등)
- FC MAVLink 직접 통신
- GS 측 타이밍 제어

## 6. MID 계약

### 6.1 구독 MID

| 심볼 | 값 | 목적 |
|---|---|---|
| `LORA_TDM_APP_CMD_MID` | `0x18E0` | 명령 입력 |
| `LORA_TDM_APP_SEND_HK_MID` | `0x18E1` | HK 요청 |
| `CFS_CORE_APP_FC_ATTITUDE_STATE_MID_VALUE` | `0x1906` | 다운링크 데이터 |
| `CFS_CORE_APP_FC_EKF_LOCAL_STATE_MID_VALUE` | `0x1905` | 다운링크 데이터 |
| `CFS_CORE_APP_FC_GPS_RAW_STATE_MID_VALUE` | `0x1907` | 다운링크 데이터 |
| `CFS_CORE_APP_FC_EKF_STATUS_MID_VALUE` | `0x1908` | 다운링크 데이터 |
| `SYSTEM_HEALTH_MID` | `0x1904` | 다운링크 데이터 |

### 6.2 게시 MID

| 심볼 | 값 | 목적 |
|---|---|---|
| `LORA_TDM_APP_HK_TLM_MID` | `0x08E0` | HK 텔레메트리 |
| `LORA_LINK_STATUS_MID` | `0x190F` | LoRa 링크 상태 |
| `UPLINK_APP_CMD_MID` | `0x18D0` | 수신된 업링크 명령 → uplink_app 전달 |

> **참고**: 수신된 UP 프레임은 `uplink_app`의 기존 검증 경로를 재사용하기 위해 `UPLINK_APP_CMD_MID`로 SB publish한다. 직접 `ROUTE_UPDATE_MID`로 우회하지 않는다.

## 7. TDM 사이클 구조

```
[1 사이클]
┌─────────────────────────────────────────────────────┐
│ Phase 1: TX                                         │
│   다운링크 패킷 구성 + serial write                   │
│   소요시간: LoRa 전송 완료까지 (SF/BW 설정 의존)      │
├─────────────────────────────────────────────────────┤
│ Phase 2: RX window                                  │
│   serial read (timeout = LORA_TDM_RX_WINDOW_MS)     │
│   ├─ ACK 수신    → 링크 정상, 명령 없음              │
│   ├─ CMD 수신    → 링크 정상 + uplink_app에 전달     │
│   └─ 타임아웃    → 링크 손실 카운터 증가              │
├─────────────────────────────────────────────────────┤
│ Phase 3: 대기                                       │
│   다음 사이클까지 잔여 시간 대기                       │
│   총 사이클 주기 = LORA_TDM_CYCLE_PERIOD_MS         │
└─────────────────────────────────────────────────────┘
```

### 7.1 TX Phase

- 최신 캐시된 FC 상태 + SYSTEM_HEALTH 데이터로 다운링크 패킷 구성
- 기존 `lora_fc_downlink_app` 다운링크 패킷 포맷 재사용
- serial write 완료 후 즉시 RX window 진입

### 7.2 RX Window Phase

- serial read를 `LORA_TDM_RX_WINDOW_MS` 동안 수행
- GS는 Pi의 다운링크 패킷 수신 후 ACK 또는 CMD를 이 window 내에 전송해야 함
- 수신 프레임 타입에 따라 분기:

| 수신 프레임 | 처리 |
|---|---|
| `ACK,<seq_echo>` | 링크 정상 확인, `LastAckTimestampMs` 갱신, `NoAckCount` 리셋 |
| `UP,<version>,...` CRC 정상 | `UPLINK_APP_CMD_MID`로 SB publish, `PendingUplinkFeedback = 0x00` |
| `UP,...` CRC 실패 | `RxErrorCount` 증가, 폐기, `PendingUplinkFeedback = 0x01` 설정 → 다음 다운링크에 포함 |
| `UP,...` SEQ 실패 | `RxErrorCount` 증가, 폐기, `PendingUplinkFeedback = 0x02` 설정 → 다음 다운링크에 포함 |
| 타임아웃 (무응답) | `NoAckCount` 증가, 링크 손실 판단 임계 검사 |
| 파싱 오류 | `RxErrorCount` 증가, 폐기 |

## 8. ACK 프레임 프로토콜

### 8.1 ACK 프레임 형식 (GS → Pi)

명령이 없을 때 GS가 Pi에 보내는 응답:

```
ACK,<seq_echo>\n
```

| 필드 | 형식 | 설명 |
|---|---|---|
| `ACK` | 고정 토큰 | 프레임 타입 |
| `seq_echo` | uint8 hex | Pi가 마지막 전송한 다운링크 패킷 seq의 하위 8비트 |

예시: `ACK,2A\n`

### 8.1.1 업링크 오류 복구 (UplinkFeedback)

CRC 실패를 감지하는 쪽은 Pi(수신 측)이므로, Pi → GS 방향 피드백이 필요하다.
Pi는 다운링크 패킷 헤더에 `UplinkFeedback` 필드를 포함하여 GS에 통보한다.

**`UplinkFeedback` 필드 (다운링크 패킷 내 포함):**

| 값 | 심볼 | 의미 | GS 처리 |
|---|---|---|---|
| `0x00` | `UPLINK_FB_OK` | 최근 업링크 없음 또는 정상 수신 | 정상 |
| `0x01` | `UPLINK_FB_CRC_FAIL` | 수신한 UP 프레임 CRC 불일치 | 마지막 명령 재전송 |
| `0x02` | `UPLINK_FB_SEQ_FAIL` | sequence 오류 감지 | seq 재조정 후 재전송 |

**Pi 측 동작:**
- `PendingUplinkFeedback` 내부 필드를 유지 (기본값 `0x00`)
- UP 프레임 CRC 실패 시 `PendingUplinkFeedback = 0x01` 설정
- UP 프레임 SEQ 실패 시 `PendingUplinkFeedback = 0x02` 설정
- UP 프레임 정상 수신 시 `PendingUplinkFeedback = 0x00` 리셋
- 매 다운링크 TX 시 `PendingUplinkFeedback`를 패킷에 포함 후 `0x00`으로 리셋

**GS 재전송 규칙:**
- GS는 다운링크 수신 후 `UplinkFeedback` 필드 확인
- `UPLINK_FB_CRC_FAIL` 또는 `UPLINK_FB_SEQ_FAIL` → 다음 RX window에 마지막 명령 재전송
- 재전송 최대 횟수: `GS_MAX_RETRANSMIT` (권장값: 3회)
- 재전송 횟수 초과 시 해당 명령 폐기 및 운용자 알림
- 재전송 시 sequence는 동일 값 유지 (증가 없음)

### 8.2 명령 프레임 (GS → Pi)

명령이 있을 때는 기존 `UP,...` 형식 그대로 사용:

```
UP,<version>,<command_class>,<sequence>,<flags>,<payload_hex>,<crc16_hex>\n
```

기존 `lora_uplink_bridge.py`의 프레임 계약과 동일 (`notes/lora_uplink_bridge_design.md` §Input Frame Contract 참조).

### 8.3 GS 응답 타이밍 요구사항

GS는 Pi의 다운링크 패킷 수신 완료 후 `LORA_TDM_GS_RESPONSE_BUDGET_MS` 이내에 ACK 또는 CMD 전송을 시작해야 한다.

```
Pi TX 완료
  └─ GS 처리 시간 (~30ms)
  └─ GS LoRa TX 시간 (~50-100ms, SF/BW 의존)
  └─ 마진 (~70ms)
  = 총 RX window ≈ 200~300ms (LORA_TDM_RX_WINDOW_MS)
```

## 9. 링크 상태 (`LORA_LINK_STATUS_MID`)

### 9.1 게시 조건

- TDM 사이클마다 갱신 (또는 주기적 게시)
- 링크 상태 전이 발생 시 즉시 게시

### 9.2 페이로드 필드

| 필드 | 형식 | 설명 |
|---|---|---|
| `Seq` | `uint32` | 단조 게시 카운터 |
| `TimestampMs` | `uint32` | 게시 시각 |
| `LinkState` | `uint8` | `0=DISCONNECTED`, `1=CONNECTED`, `2=DEGRADED` |
| `LastAckTimestampMs` | `uint32` | 마지막 ACK 수신 시각 |
| `NoAckCount` | `uint16` | 연속 ACK 미수신 횟수 |
| `TxCount` | `uint32` | 누적 다운링크 전송 횟수 |
| `RxAckCount` | `uint32` | 누적 ACK 수신 횟수 |
| `RxCmdCount` | `uint32` | 누적 명령 수신 횟수 |
| `RxErrorCount` | `uint16` | 누적 수신 오류(파싱 실패 등) |

### 9.3 링크 상태 분류

| 상태 | 조건 |
|---|---|
| `CONNECTED` | `NoAckCount < LORA_TDM_LINK_LOSS_THRESHOLD` |
| `DEGRADED` | `NoAckCount >= LORA_TDM_LINK_LOSS_THRESHOLD` (임계 미도달, 단순 손실 증가) |
| `DISCONNECTED` | `NowMs - LastAckTimestampMs > LORA_TDM_LINK_TIMEOUT_MS` |

## 10. 타이밍 설정

| 상수 | 기본값 | 설명 |
|---|---|---|
| `LORA_TDM_CYCLE_PERIOD_MS` | `1000` | 전체 TDM 사이클 주기 |
| `LORA_TDM_RX_WINDOW_MS` | `300` | RX window 길이 (GS 응답 대기 시간) |
| `LORA_TDM_LINK_LOSS_THRESHOLD` | `3` | DEGRADED 판단 연속 ACK 미수신 횟수 |
| `LORA_TDM_LINK_TIMEOUT_MS` | `5000` | DISCONNECTED 판단 절대 타임아웃 |

> **참고**: `LORA_TDM_RX_WINDOW_MS`는 LoRa 모듈의 SF/BW 설정에 따라 조정이 필요하다. SF7/BW125kHz 기준 ACK 패킷 전송 시간 ~50ms를 포함하여 최소 200ms 이상이어야 한다.

## 11. serial 포트 관리

- 초기화 시 `O_RDWR | O_NOCTTY` 로 serial 포트 오픈
- TX phase: write()
- RX window phase: select() + read() (timeout = `LORA_TDM_RX_WINDOW_MS`)
- write() 또는 read() 오류 시: 포트 닫기 → 재열기 재시도 (`LORA_TDM_SERIAL_REOPEN_DELAY_MS` 간격)
- 재열기 중 TDM 사이클 일시 중단

## 12. HK 동작

HK 요청 시 보고 항목:

- 명령 카운터 / 명령 오류 카운터
- TxCount, RxAckCount, RxCmdCount, RxErrorCount
- NoAckCount, LastAckTimestampMs
- 현재 LinkState
- serial 포트 상태
- 재열기 시도 횟수

## 13. 명령 처리

| CC | 명령 | 동작 |
|---|---|---|
| 0 | NOOP | 이벤트 발생, 카운터 갱신 |
| 1 | 카운터 리셋 | 모든 카운터 0으로 리셋 |

## 14. 기존 컴포넌트와의 관계

| 컴포넌트 | 상태 | 비고 |
|---|---|---|
| `lora_fc_downlink_app` | **대체됨** | `lora_tdm_app`이 TX 기능 포함. 레거시 빌드용으로 소스 보관 |
| `bridge/lora_uplink_bridge.py` | **레거시 유지** | UDP 경로 테스트/디버깅용. Pi에서 `lora_tdm_app`과 동시 실행 불가 (serial 포트 충돌) |
| `uplink_app` | **변경 없음** | SB 경로로 명령 수신, 기존 검증 로직 재사용 |
| `cfs_core_app` | **변경 없음** | `LORA_LINK_STATUS_MID` 구독 추가 검토 가능 (선택) |

## 15. 미구현 사항

- CONFIG_CMD를 통한 `LORA_TDM_CYCLE_PERIOD_MS`, `LORA_TDM_RX_WINDOW_MS` 런타임 변경
- 다운링크 패킷 포맷 변경 (현재 `lora_fc_downlink_app` 포맷 재사용)
- ACK 프레임에 RSSI/SNR 피드백 필드 추가
- GS 측 소프트웨어 (ACK 전송, 명령 큐잉) 구현
