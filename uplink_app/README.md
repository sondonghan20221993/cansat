# uplink_app

지상국 uplink 명령을 수신·검증하고 route update를 cFS Software Bus로 전달하는 cFS 앱이다.

## MID 인터페이스

| 방향 | 심볼 | 값 | 설명 |
| --- | --- | --- | --- |
| CMD 수신 | `UPLINK_APP_CMD_MID` | `0x18D0` | NOOP, RESET_COUNTERS, PROCESS_UPLINK (CC=2) |
| CMD 수신 | `UPLINK_APP_SEND_HK_MID` | `0x18D1` | HK 요청 |
| 게시 | `UPLINK_APP_HK_TLM_MID` | HK MID | HK 텔레메트리 |
| 게시 | `UPLINK_STATUS_MID` | `0x190A` | uplink 처리 상태 |
| 게시 | `ROUTE_UPDATE_MID` | `0x190B` | 검증된 route update → cfs_core_app + mavlink_bridge_app |
| 게시 | `VIEWPOINT_CMD_MID` | `0x190D` | viewpoint 명령 relay → cfs_core_app 캐시 |
| 게시 | `CONFIG_CMD_MID` | `0x190E` | runtime configuration relay → cfs_core_app 검증·적용 |

## 구현 기능

### uplink 패킷 처리 (PROCESS_UPLINK_CC)
- 지상국 또는 `bridge/lora_uplink_bridge.py`에서 CCSDS 래핑된 uplink 패킷 수신
- 검증 항목: 프로토콜 버전, CRC-16, payload 길이(최대 196바이트), sequence 단조 증가
- 명령 클래스 분류: route update, viewpoint update, runtime configuration, recovery command 등
- 검증 통과 시 대상 MID로 라우팅

### route update 처리
- waypoint 개수(1..16), x/y/z finite 검사, 고도(2m..8m), 인접 waypoint 3D 거리(2m..2m) 검증
- 검증 통과 시 `ROUTE_UPDATE_MID`로 publish → `cfs_core_app` 캐시 + `mavlink_bridge_app` FC 업로드
- Z 좌표: 양수 = 고도(AGL), 단위 meters

### viewpoint 처리
- payload 크기(== `sizeof(UPLINK_APP_ViewpointPayload_t)`)·버전(`UPLINK_APP_VIEWPOINT_VERSION`)·타입(0–2)·frame(0=LOCAL_NED) 검증
- X/Y ∈ [−50, 50]m, Z ∈ [2, 8]m, Yaw ∈ [−π, π], Pitch ∈ [−π/2, π/2], HoldTimeMs ≤ 30 000 범위 검사 및 finite 검사
- 검증 통과 시 typed `UPLINK_APP_ViewpointCmdTlm_t`으로 변환 후 `VIEWPOINT_CMD_MID`로 publish → `cfs_core_app` 캐시

### runtime configuration 처리
- raw payload를 `CONFIG_CMD_MID`로 relay → `cfs_core_app`이 scope/version/param ID/checksum 검증 후 `ActiveConfig` 적용

### 지속 상태 (SaveState/LoadState)
- 마지막으로 수락된 uplink sequence 번호를 파일로 저장 (atomic tmp+rename)
- 재시작 후 이전 sequence 기반 replay 방어 복원

## 입력 경로

### UDP 경로 (CI_LAB)
`bridge/lora_uplink_bridge.py` 또는 `tools/uplink_route_update_sender.py --transport udp`가 CCSDS UDP 패킷을 UDP 1234로 전달한다.

### LoRa serial 직접 경로 (ServiceLoRa) — ⚠️ 비활성화 권고 (lora_tdm_app 잔재물)

> **포트 충돌**: `lora_fc_downlink_app`이 같은 CP2102 포트를 `O_RDWR`로 독점 오픈한다.
> 두 앱이 동시에 같은 포트를 열면 Linux 시리얼 드라이버의 수신 버퍼를 서로 빼앗아 가므로,
> 이 경로를 활성화하면 `lora_fc_downlink_app`의 HB 수신과 `uplink_app`의 UP 수신 모두 신뢰할 수 없다.
>
> **올바른 경로**: `lora_fc_downlink_app`이 RX 윈도우에서 UP 프레임을 읽고 SB로 publish →
> `uplink_app`이 `UPLINK_APP_CMD_MID`를 구독하는 방식 (PDF 설계 원칙).
> 현재 `lora_fc_downlink_app`에 UP 파싱이 미구현이므로 UDP 경로를 사용할 것.

`ServicePrototype()` 호출마다 `ServiceLoRa()`가 LoRa serial에서 직접 UP 프레임을 읽어 `ProcessUplink()`를 내부 호출한다.

- serial 경로: `UPLINK_APP_LORA_SERIAL_PATH` (config, 기본값: CP2102 USB-UART)
- Baud: `UPLINK_APP_LORA_BAUDRATE` (기본 57600)
- open 모드: `O_RDONLY | O_NOCTTY | O_NONBLOCK` → 열기 후 blocking 전환

UP 프레임 형식:
```
UP,<version>,<command_class>,<sequence>,<flags>,<payload_hex>,<crc16_hex>
```

검증 항목:
- CRC16-CCITT (`UP,version,class,seq,flags,payload_hex` 부분에 대한 CRC)
- hex 디코딩 유효성
- payload 길이 ≤ 196바이트
- sequence 단조 증가 (regression 거부)

## 미구현

- **viewpoint FC 실행**: `cfs_core_app`이 viewpoint 명령을 캐시하지만, FC에 MAVLink 명령으로 전달하는 로직이 없음

## Python bridge 대체 현황

| 구 Python 프로세스 | 현 cFS 구현 | 상태 |
| --- | --- | --- |
| `bridge/lora_uplink_bridge.py` | ~~이 앱 `ServiceLoRa()` 직접 처리~~ | ⚠️ CP2102 포트 충돌로 사용 불가 |
| — | UDP 경로 (CI_LAB) 유지 | ✓ 현재 권장 경로 |

## 동작 명세 참조

- uplink 전체 계약: `notes/mission_app_runtime_spec.md` §18
- LoRa bridge 프로토콜(참고): `notes/lora_uplink_bridge_design.md`
