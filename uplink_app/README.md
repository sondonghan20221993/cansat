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

### 지속 상태 (SaveState/LoadState)
- 마지막으로 수락된 uplink sequence 번호를 파일로 저장 (atomic tmp+rename)
- 재시작 후 이전 sequence 기반 replay 방어 복원

## 입력 프레임 (transport 계층)

`bridge/lora_uplink_bridge.py`가 LoRa serial ASCII 프레임을 CCSDS UDP 패킷으로 변환해 UDP 1234로 전달한다.

```
UP,<version>,<command_class>,<sequence>,<flags>,<payload_hex>,<crc16_hex>
```

payload 최대 길이: **196바이트**

## 미구현

- runtime configuration 명령의 대상 앱 실제 전달 (수신·검증만 됨)
- viewpoint update downstream 처리

## 동작 명세 참조

- uplink 전체 계약: `notes/mission_app_runtime_spec.md` §18
- LoRa bridge 프로토콜: `notes/lora_uplink_bridge_design.md`
