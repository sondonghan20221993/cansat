# lora_fc_downlink_app

cFS Software Bus에서 FC 상태 메시지와 시스템 헬스를 구독하고, LoRa 시리얼로 텔레메트리를 전송하며, 지상국 HB(heartbeat)를 수신하여 링크 상태를 관리하는 cFS 앱이다.

## MID 인터페이스

| 방향 | 심볼 | 값 | 설명 |
| --- | --- | --- | --- |
| CMD 수신 | `LORA_FC_DOWNLINK_APP_CMD_MID` | `0x18B0` (topic-id 기반) | NOOP, RESET_COUNTERS |
| CMD 수신 | `LORA_FC_DOWNLINK_APP_SEND_HK_MID` | `0x18B1` (topic-id 기반) | HK 요청 |
| SB 수신 | `FC_ATTITUDE_STATE_MID` | `0x1906` | attitude (roll/pitch/yaw + 각속도) |
| SB 수신 | `FC_EKF_LOCAL_STATE_MID` | `0x1905` | local position (x/y/z) + velocity (vx/vy/vz) |
| SB 수신 | `FC_GPS_RAW_STATE_MID` | `0x1907` | GPS raw (LatE7/LonE7/AltMm/FixType) |
| SB 수신 | `FC_EKF_STATUS_MID` | `0x1908` | EKF health flags |
| SB 수신 | `SYSTEM_HEALTH_MID` | `0x1904` | 시스템 헬스 (HealthState/FaultCode) |
| 게시 | `LORA_FC_DOWNLINK_APP_HK_TLM_MID` | topic-id 기반 | HK 텔레메트리 |

## 구현 기능

### 상태 캐시
SB 수신 시마다 다음 값을 내부 버퍼에 갱신한다:
- Attitude: RollRad, PitchRad, YawRad, Valid, TimestampMs
- EKF Local: X_m, Y_m, Z_m, Vx_mps, Vy_mps, Vz_mps, Valid, TimestampMs
- GPS Raw: LatE7, LonE7, AltMm, FixType, Valid, TimestampMs
- EKF Status: Valid, TimestampMs
- System Health: HealthState, FaultCode, TimestampMs

### LoRa 텔레메트리 송신 (ServiceLoRa)
SB 메시지 수신마다 `ServiceLoRa()`를 호출한다.

- LoRa serial 경로: `LORA_FC_DOWNLINK_APP_LORA_SERIAL_PATH` (config)
- Baud: `LORA_FC_DOWNLINK_APP_LORA_BAUDRATE` (기본 57600)
- open 모드: `O_RDWR | O_NOCTTY | O_NONBLOCK` → 열기 후 blocking으로 전환

패킷 선택:
- PacketType == SYSTEM_HEALTH → SH 패킷 전송
- AttitudeValid && LocalValid → FC 패킷 전송
- 그 외 → 전송 없음

**레이트 리미팅**: 마지막 TX로부터 500ms 미만이면 전송 스킵. (`LastLoRaTxMs` 기반)

> **⚠️ 포트 충돌 주의**: `uplink_app`도 동일한 CP2102 포트를 `O_RDONLY`로 열고 있음.
> Linux에서 두 프로세스가 같은 시리얼 포트를 열면 바이트를 서로 빼앗김.
> HB 바이트가 `uplink_app`으로, UP 바이트가 이 앱으로 분산될 수 있음.
> 근본 해결: 이 앱이 포트 독점 후 UP 프레임을 SB publish → `uplink_app`이 SB 구독.

write 오류:
- `EAGAIN/EWOULDBLOCK`: packet skip, 포트 유지
- 그 외: EVS 로그 후 포트 close + 재열기 대기

> **주의 — blocking write 지연**: LoRa FD는 open 후 `fcntl(F_SETFL, Flags & ~O_NONBLOCK)`으로 blocking 모드로 전환된다. `ServiceLoRa()`는 SB 메시지 처리 경로(`ProcessInputMessage`)에서 호출되므로, LoRa write가 장시간 block되면 앱의 SB 처리 루프 전체가 지연될 수 있다. 운용상 write timeout 정책이 필요하면 별도 요구사항으로 정의해야 한다.

### 지상국 HB 수신 (ServiceLoRaRead)
SB 메시지 수신마다 `ServiceLoRaRead()`를 호출하여 LoRa serial에서 1바이트씩 읽어 줄 단위로 누적한다.

지원 HB 프레임 형식:
- 단순: `HB` 또는 `HB,<seq>`
- canonical: `HB,<node_id>,<seq>,<tx_ms>,<sensor_ok>,<crc16_hex>` — sensor_ok != 0 필수

HB 수신 시 `HbLastRxMs`(CFE_TIME 기반 ms), `HbLinkValid = 1` 갱신.

## 패킷 포맷 (LoRa ASCII)

### FC State 패킷 (PacketType 1)
```
FC,<count>,<ts_ms>,<roll_rad>,<pitch_rad>,<yaw_rad>,<x_m>,<y_m>,<z_m>,<vx_mps>,<vy_mps>,<vz_mps>,<lat_e7>,<lon_e7>,<alt_mm>,<fix_type>,0\n
```

총 17필드 (FC 포함). 마지막 `0`은 `uplink_fb` 자리이며 이 앱에서는 항상 0.

| 필드 | 형식 | 출처 |
| --- | --- | --- |
| count | uint (`LoRaTxCount++`) | LoRa ASCII 패킷 생성 시마다 증가 (실제 write 시도 횟수) |
| ts_ms | uint | AttitudeTlm.TimestampMs |
| roll/pitch/yaw_rad | %.6f | AttitudeTlm |
| x/y/z_m | %.3f | EkfLocalTlm |
| vx/vy/vz_mps | %.3f | EkfLocalTlm |
| lat_e7/lon_e7 | %ld (1e-7 도) | GpsRawTlm |
| alt_mm | %ld | GpsRawTlm |
| fix_type | %u | GpsRawTlm |
| uplink_fb | 항상 0 | 미구현 (lora_tdm_app 호환용 자리) |

> **주의**: Python 파서(`fc_serial_ws_server.py`)는 17필드를 기대함. `,0` 누락 시 파싱 실패.

### System Health 패킷 (PacketType 2)
```
SH,<count>,<ts_ms>,<health_state>,<fault_code>,0,0\n
```

총 7필드 (SH 포함). 마지막 `0,0`은 `link_state`, `uplink_fb` 자리이며 이 앱에서는 항상 0.

| 필드 | 형식 | 출처 |
| --- | --- | --- |
| count | uint (`LoRaTxCount++`) | LoRa ASCII 패킷 생성 시마다 증가 |
| ts_ms | uint | SystemHealthMirror.TimestampMs |
| health_state | uint (0=NOMINAL, 1=DEGRADED, 2=RECOVERY) | SystemHealthMirror |
| fault_code | uint | SystemHealthMirror.FaultCode |
| link_state | 항상 0 | 미구현 (lora_tdm_app 호환용 자리) |
| uplink_fb | 항상 0 | 미구현 (lora_tdm_app 호환용 자리) |

> **주의**: Python 파서(`fc_serial_ws_server.py`)는 7필드를 기대함. `,0,0` 누락 시 파싱 실패.

## 카운터 필드 의미

| 필드 | 증가 시점 | 의미 |
| --- | --- | --- |
| `LoRaTxCount` | `ServiceLoRa()`에서 패킷 문자열 생성 시 | LoRa ASCII 패킷 생성 횟수 (write 성공 여부 무관) |
| `DownlinkCount` | `ProcessInputMessage()` 진입 시마다 | SB 입력 메시지 처리 횟수 (LoRa 전송 여부 무관) |

`LoRaTxCount ≤ DownlinkCount`: AttitudeValid/LocalValid 조건 불충족 시 패킷이 생성되지 않아 LoRaTxCount가 더 작을 수 있다.

## Python bridge 대체 현황

| 구 Python 프로세스 | 현 cFS 구현 |
| --- | --- |
| `bridge/mavlink_uart_bridge.py` | `mavlink_bridge_app` (FC UART 직접 처리) |
| `bridge/lora_telemetry_bridge.py` | 이 앱 (HB read + LoRa write) |

## 동작 명세 참조

- 시스템 MID 계약: `notes/mission_app_runtime_spec.md` §5.1.1
- LoRa 송신 이관 배경: `notes/mavlink_bridge_app_behavior_spec.md` §2.1
