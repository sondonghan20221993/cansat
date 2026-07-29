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
| 게시 | `CONFIG_CMD_MID` | `0x190E` | runtime configuration relay → cfs_core/mavlink_bridge/lora_tdm |
| 게시 | `RECOVERY_CMD_MID` | `0x190C` | 복구 명령 relay → cfs_core_app (레벨3 인증 필요) |
| 게시 | `MODE_CMD_MID` | `0x190F` | 모드 명령 relay → cfs_core_app (레벨3 인증 필요) |
| 게시 | `DIAGNOSTIC_CMD_MID` | `0x1910` | 진단 명령 relay → lora_tdm_app (레벨1 인증) |

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

### 권한 검증 (§18.11.1) + health gate
- health 미수신(boot 직후) 시 모든 명령 차단(fail-safe), health 상태별로도 명령 클래스 차단
  (DEGRADED/FAILED에서 CONFIG류 차단, RECOVERY/DIAGNOSTIC은 예외적으로 허용)
- Flags 필드 비트[7:6]로 실린 요청 인증레벨을 명령 클래스별 요구레벨과 대조(`GetClassRequiredLevel`) —
  CONFIG/ROUTE_UPDATE=레벨2, RECOVERY/MODE=레벨3(0이 아닌 request_token 추가 요구), DIAGNOSTIC=레벨1
- `UPLINK_FORCE_FLAG`(bit0)로 health gate만 벤치 테스트 목적으로 우회 가능(권한 검증은 우회 안 됨)
- 미달 시 `AUTHZ_BLOCK_EID` EVS 발생 + 거부

### 지속 상태 (SaveState/LoadState)
- 마지막으로 수락된 uplink sequence 번호를 파일로 저장 (atomic tmp+rename)
- 재시작 후 이전 sequence 기반 replay 방어 복원

## 입력 경로

### UDP 경로 (CI_LAB)
`bridge/lora_uplink_bridge.py` 또는 `tools/uplink_route_update_sender.py --transport udp`가 CCSDS UDP 패킷을 UDP 1234로 전달한다.

### LoRa SB 경로 (`lora_tdm_app` → `0x18D0`) — ✅ 현행 (2026-06-16~)

LoRa serial 포트는 **`lora_tdm_app`이 단독 소유**(TDM: downlink TX + 300ms RX 창)한다.
uplink_app은 serial을 직접 열지 않는다(포트 충돌 제거).

흐름:
1. `lora_tdm_app`이 TDM RX 창에서 "UP,..." 라인을 읽음
2. CRC16 검증 + hex 디코딩 후 `LORA_TDM_APP_UplinkFwdCmd_t` 구성
3. `UPLINK_APP_CMD_MID`(0x18D0), FcnCode=`PROCESS_UPLINK_CC(2)`로 SB 전송
4. `uplink_app`이 일반 PROCESS_UPLINK 명령으로 수신 → `UPLINK_APP_ProcessUplink()`

frame CRC/framing은 lora_tdm_app(transport), 버전/클래스/시퀀스/payload semantic 검증은 uplink_app 소유(spec §18.4.4).

> **TDM 제약(반이중)**: lora RX 윈도우는 downlink TX 후 300ms만 열린다.
> 지상국은 downlink 수신 직후 그 슬롯 안에 UP 프레임을 송신해야 수신된다.

UP 프레임 형식:
```
UP,<version>,<command_class>,<sequence>,<flags>,<payload_hex>,<crc16_hex>
```

검증 항목:
- CRC16-CCITT (`UP,version,class,seq,flags,payload_hex` 부분에 대한 CRC)
- hex 디코딩 유효성
- payload 길이 ≤ 196바이트

## 미구현

- **viewpoint FC 실행**: `cfs_core_app`이 viewpoint 명령을 캐시하지만, FC에 MAVLink 명령으로 전달하는 로직이 없음 — 짐벌 미탑재로 범위 제외 확정(BL-10, 2026-07-24). 대신 `EXEC_RESULT_MID`로 uplink_app에 명시적 FAILED 회신(BL-82, 2026-07-29)해 무한 ROUTED 대기는 해소됨

## Python bridge 대체 현황

| 구 Python 프로세스 | 현 cFS 구현 | 상태 |
| --- | --- | --- |
| `bridge/lora_uplink_bridge.py` | `lora_tdm_app` TDM RX → `0x18D0` PROCESS_UPLINK 직접 전달 | ✓ 대체 완료 (2026-06-16) |
| — | UDP 경로 (CI_LAB) 유지 | ✓ 병행 가능 (테스트용) |

## 동작 명세 참조

- uplink 전체 계약: `notes/mission_app_runtime_spec.md` §18
- LoRa bridge 프로토콜(참고): `notes/lora_uplink_bridge_design.md`
