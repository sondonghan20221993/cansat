# 4개 앱 spec-vs-code 전수 검증 결과 — 2026-07-27

> mavlink_bridge_app / cfs_core_app / uplink_app / lora_tdm_app 각각에 대해
> notes/*_behavior_spec.md 등 스펙 문서와 fsw 소스 전체, 관련 python 툴/테스트를 대조.
> 코드 수정 없음 — 순수 조사 결과만 기록.

## 요약

| 앱 | 실사용 영향 | 핵심 발견 |
|---|---|---|
| lora_tdm_app | **버그(실비행 영향)** | waypoint readback DL2 프레임이 지상 Python 디코더에서 폐기됨 |
| uplink_app | **버그(실비행 영향)** | LoRa 경로 CLI 툴에서 auth flag 유실 → CONFIG/ROUTE_UPDATE 실비행 중 항상 거부 |
| cfs_core_app | 결함 후보 + 문서 드리프트 | RECOVERY RESET_COUNTER 비대칭, spec 미기재 기능 존재 |
| mavlink_bridge_app | 문서 드리프트만 (코드 자체는 정합) | spec이 BL-56 리팩터링(ARMED 정책, 좌표계) 미반영 |

---

## 1. lora_tdm_app — waypoint readback DL2 확장 블록 미구현 (지상 Python)

- 기체 C 인코더(`lora_tdm_app/fsw/src/lora_tdm_app_utils.c:195-300`, `LORA_TDM_APP_BuildDl2Frame`)는
  `DL2_FLAG_WAYPOINT` bit2(`lora_tdm_app/fsw/inc/lora_tdm_app_interface_cfg.h:38`)가 서면
  tail 뒤에 28바이트 waypoint 페이지를 추가, `BodyLen`이 최대 81바이트(45+8+28)까지 커짐.
- 지상 `bridge/lora_downlink_decoder.py`의 `_try_parse_dl2()` (97-253행, 특히 241-244행)는
  `max_len = DL2_BASE_LEN + DL2_SYSTIME_BLOCK_LEN + DL2_TAIL_LEN = 56`으로 고정 —
  waypoint 블록을 전혀 고려 안 함. `DL2_FLAG_WAYPOINT` 상수 자체가 Python 쪽에 정의돼 있지 않음.
- **결과**: 기체가 route readback(waypoint) 프레임(body_len > 56)을 보내면 지상 파서가 CRC 검증도 못 해보고
  `"bad DL2 len"`으로 즉시 폐기, 1바이트만 버리고 재동기화 → **waypoint readback이 지상에서 사실상 완전히 동작 안 함**.
- `tests/test_lora_downlink_decoder.py`, `test_lora_fc_downlink_e2e.py`, `test_lora_fc_downlink_packet.py`에
  waypoint/route 관련 테스트 전무.

### 부차 (문서만의 문제, 코드는 일치)
- `notes/lora_protocol_v2_spec.md` §4 DL2 표(49행)가 UFB 0x0C~0x0E(REJECT_COUNTER/REJECT_FLIGHT_MODE/APPLIED)를
  안 실었음. `notes/lora_tdm_app_behavior_spec.md` §9.2는 최신 반영됨 — 두 spec 문서 간 불일치일 뿐,
  C(`default_lora_tdm_app_mission_cfg.h:19-36`)/uplink_app enum 값 자체는 일치.
- CRC16, DL2/ACK2/UP2 프레임 오프셋, DiagnosticCmd 3개 action(LINK_STATUS/RX_STATS/TX_STATS)은
  C/Python/spec 모두 정합 확인됨.

**권고**: `bridge/lora_downlink_decoder.py`의 `_try_parse_dl2()` max_len 계산에 waypoint 블록(28B) 반영,
`DL2_FLAG_WAYPOINT` 상수 추가, waypoint 페이로드 파싱 로직 구현 + 회귀 테스트 추가.

---

## 2. uplink_app — LoRa 전송 경로에서 인증 레벨(Flags) 유실

C 프로덕션 코드(`uplink_app_cmds.c`/`uplink_app_utils.c`)는 spec(§17.5, §18.4.7, §18.11)과
라인 단위로 완전 일치 — 권한 레벨 매핑, health-gate→auth 순서, request_token echo, fail-safe boot 전부 정상.
버그는 전부 **지상국 파이썬 CLI 툴**에서 발견.

- **`tools/uplink_config_sender.py:208`** (lora-serial 분기): `build_lora_frame(args.sequence, config_payload)` —
  `flags` 인자를 안 넘겨 기본값 0 적용. 같은 함수 내 lora-text/udp 분기는
  `(1 if args.force else 0) | (args.auth << 6)`를 정확히 넘김 — lora-serial만 누락.
  → `--auth 2`를 지정해도 실제 프레임엔 반영 안 됨 → CONFIG(Level 2 요구) 명령이 **실기체에서 항상 거부**.

- **`tools/uplink_route_update_sender.py`**: lora-text, lora-serial **두 분기 모두** 동일 버그.
  CLI에서 `--auth`/`--force`로 `flags` 변수는 정확히 계산해놓고, LoRa 출력 두 경로 모두에서 그 변수를 안 씀.
  UDP 분기(`build_process_uplink_payload(..., flags=flags)`)만 정상.
  → **route update(Level 2 요구)가 LoRa 무선으로는 항상 auth=0으로 나가 실비행 중 거부됨.**

- **`tools/uplink_flight_mode_sender.py`**: LoRa transport 자체가 없고 UDP 전용(§18.13과 일관).
  auth flag 처리는 정상이나, BL-44(Level 3, request_token 필수)가 UDP(CI_LAB) 전용 경로로만
  검증돼 실비행 LoRa 경로 커버리지 공백 있음.

### 왜 안 잡혔나
- `tests/test_uplink_config_sender.py`, `test_uplink_route_update_sender.py`는 저수준 빌더 함수만 직접 호출 —
  버그가 있는 `main()`의 CLI 분기(lora-serial/lora-text)는 전혀 거치지 않음.
- `tests/test_uplink_e2e.py:118-133` `test_valid_route_update_routed`도 flags 인자 없이 호출(기본값 0)하고,
  검증부가 `assert True # placeholder`라 실질적으로 아무것도 검증 안 함.

**권고**:
1. `uplink_config_sender.py` lora-serial 분기, `uplink_route_update_sender.py` lora-text/lora-serial 두 분기에
   계산된 `flags`(auth+force) 전달하도록 수정.
2. `test_uplink_e2e.py::test_valid_route_update_routed`에 `flags=(2<<6)` 지정 + placeholder 제거,
   실제 EVS/텔레메트리 검증으로 교체.
3. CLI `main()`의 transport 분기별 프레임 생성 로직에 대한 유닛테스트 추가
   (현재 "계산은 맞는데 배선이 빠짐" 유형 버그를 구조적으로 못 잡음).

---

## 3. cfs_core_app

1. **spec에 없는 기능이 코드에 존재**: `DIAGNOSTIC_CMD_MID`(0x1910, `cfs_core_app.c:206-211`),
   `ROUTE_SNAPSHOT_MID`(0x1913, `cfs_core_app_utils.c:1157-1193`, waypoint readback 스냅샷 발행) —
   `notes/cfs_core_app_behavior_spec.md` §6.1/§6.2/§16/§17 어디에도 미기재.

2. **RECOVERY RESET_COUNTER 비대칭 (결함 후보)**: `cfs_core_app_utils.c:1002-1010` —
   `RESET_COUNTER` 처리가 `RecoveryStartMs`/`BridgeRestartCount`만 리셋하고
   `UplinkRestartCount`/`LoraRestartCount`/`NextUplinkRestartMs`/`NextLoraRestartMs`는 그대로 둠.
   BL-43에서 세 재시작 카운터를 대칭적으로 영속화하도록 설계했음에도, 지상 RESET_COUNTER 명령은
   bridge 것만 지움 — 운영자 기대와 다른 비대칭 동작.

3. **checksum 공식 문서 낡음**: spec §14.5(584행)는 6필드 합만 언급하나, 실제
   `CFS_CORE_APP_ComputeStateChecksum`(`cfs_core_app_utils.c:829-836`)은 BL-43 추가 필드
   (BridgeRestartCount/UplinkRestartCount/LoraRestartCount/LastFaultCode)까지 포함 — 코드가 맞고 문서가 낡음.

4. **"PARSER_RESET/SERIAL_RECONNECT는 항상 OK" spec 서술 부정확**: 실제로는
   `CFS_CORE_APP_SendBridgeCtrlCmd()`(`cfs_core_app_utils.c:645-654`) 반환값에 따라 FAILED 회신 가능한데
   spec은 이 실패 경로를 다루지 않음.

5. **PublishPeriodMs 확장 시 재시작 감시/에스컬레이션 타이밍도 지연되는 부작용, spec 미기재**:
   `CFS_CORE_APP_UpdateHealth`(`utils.c:412-415`)에서 강제 게시 트리거 없이는 `CheckAppRestarts()`가
   `ActiveConfig.PublishPeriodMs` 주기로만 호출됨(`utils.c:455`). PublishPeriodMs를 60000ms로 올리면
   `BRIDGE_RESTART_INTERVAL_MS(5000)`/`FAILED_ESCALATION_MS(30000)`가 사실상 무의미해질 수 있음.

일치 확인(불일치 없음): 헬스 분류 우선순위, NOMINAL_STABILITY_MS/FAILED_ESCALATION_MS, 시퀀스/타임스탬프
검증(BL-42), CONFIG 이중버퍼+checksum/range 검증, MODE 전이, RECOVERY 6-action 분기, 상태 파일 원자적 저장.

---

## 4. mavlink_bridge_app — spec이 BL-56(2026-07-25) 리팩터링 미반영

코드/coveragetest 자체는 정합. `notes/mavlink_bridge_app_behavior_spec.md`(§17까지만 존재)가
최신 BL-56 내용을 전혀 반영 못함.

1. **ARMED 차단 정책 정반대**: spec §5/§11.1 "ARMED 시 업로드 차단, EID 12 경고"는
   BL-56에서 전면 폐지됨(`mavlink_bridge_app_utils.c:508-510` 주석 확인).
   `MAVLINK_BRIDGE_APP_ARMED_WARN_EID`는 정의만 남은 죽은 코드.
   coveragetest에 `Test_StartMissionUpload_AllowedWhenArmed`로 회귀 방지 명시 — 의도된 변경, spec만 미갱신.

2. **RouteOpType 체계 다름**: spec은 REPLACE/APPEND(count)/DELETE(count) 3종.
   실제 코드(`mavlink_bridge_app_utils.c:6-9`)는 REPLACE/ADD(count)/DELETE(**index 기반**)/MODIFY(신규) 4종.
   DELETE 의미론이 완전히 다름 — spec만 보고 uplink_app 페이로드 구성 시 실제 FC 동작과 어긋남.

3. **좌표 표현 체계 변경**: spec §5/§7/§13.1은 local NED→global 변환(RefLat/RefLon 기준)을 서술하나,
   실제 코드는 이미 절대좌표(LatE7/LonE7)를 그대로 저장·전송(BL-56, `utils.c:404-433` 주석) —
   변환 로직 자체가 삭제됨. §7 필드 매핑 표는 현재 사실과 다름.

4. spec §17 제목 "코드 미적용"은 오기 — 실제로는 이미 적용됨(`utils.c:1875-1893`,
   `ProcessReceivedByte`에서 STX 체크가 WAIT_STX 상태에서만 수행되도록 가드됨).
   관련 완료 노트(`mavlink_stx_reentry_parser_bug_completed.md` 등)가 맞고 spec §17 표시가 틀림.

5. spec §15("INT 경로 GLOBAL_RELATIVE_ALT 변환 미구현")가 같은 문서 §7/§13.1.1과 자기모순.
   코드 확인(`utils.c:426`, `SendMissionItemInt`) 결과 §7/§13.1.1이 맞고 §15가 낡은 잔재 문구.

코드 자체 버그(오프바이원/널체크/오버플로우/race condition)는 발견되지 않음.

---

## 우선순위 제안

1. **[긴급] uplink_app LoRa auth flag 유실** — 실비행에서 CONFIG/ROUTE_UPDATE가 조용히 거부되는 상태.
2. **[긴급] lora_tdm waypoint readback 디코더 미구현** — 기능 자체가 지상에서 동작 안 함.
3. **[중] cfs_core RECOVERY RESET_COUNTER 비대칭** — 운영자 기대와 다른 동작, 의도적인지 확인 필요.
4. **[문서] mavlink_bridge_app_behavior_spec.md 전면 재작성** (§5, §5.1, §7, §13.1, §15, §17).
5. **[문서] cfs_core_app_behavior_spec.md 보강** (§6.1/§6.2/§16/§17 DIAGNOSTIC/ROUTE_SNAPSHOT 추가, §14.5/§17 갱신).
6. **[문서] lora_protocol_v2_spec.md §4 UFB 표 갱신** (0x0C~0x0E 추가).
