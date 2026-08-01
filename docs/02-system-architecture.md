# 시스템 아키텍처 (QGC 기반, 2026-08-02 개정)

## 배경

Raspberry Pi를 컴패니언 컴퓨터로 사용할 수 없게 됨에 따라 cFS(cFE+커스텀 앱 4종:
`mavlink_bridge_app`/`cfs_core_app`/`uplink_app`/`lora_tdm_app`) 기반 아키텍처를
포기한다. cFS 관련 명세(구 01/03/04/07/08번 문서, `openMCT` 레포)는 폐기 대상이며
`cfs-telemetry-app` 레포는 참고용으로만 보존한다.

## 신규 아키텍처

```
FC (PX4)
    ↕ UART (MAVLink, 투명 전송)
LR24-F (기체측, 2.4GHz LoRa FHSS, 2.4KB/s, UART 57600)
    ↕ 무선(LoRa)
LR24-F (지상측) ─ UART/USB 시리얼 ─▶ 지상 PC
                                        ├─ QGroundControl     (MAVLink 직결, 조종/미션)
                                        └─ Open MCT (선택)     (MAVLink 브리지 → WebSocket, 커스텀 시각화)

RunCam WiFiLink V2 (기체측, 영상)
    ↕ 무선(WiFi, 독립 링크)
지상 PC
    └─ fpv4win                          (FPV 영상 뷰어, 위 텔레메트리 경로와 무관)
```

- FC가 뿌리는 MAVLink 스트림을 LR24-F가 **투명 전송(transparent UART)** 모드로
  무선 중계 — 커스텀 프레이밍/프로토콜 계층 불필요.
- QGC가 지상측 LR24-F를 시리얼 포트로 잡아 표준 MAVLink 연결로 인식.
- 미션 업로드/다운로드, 텔레메트리 표시, RC/배터리 모니터링 전부 QGC 내장 기능으로 처리.
- Open MCT 병행 시 QGC와 시리얼 포트를 동시에 못 열므로 `mavlink-router`/`mavproxy` 등으로
  포트를 앞단에서 나눠 QGC·openMCT 브리지 각각 별도 UDP 엔드포인트로 공급해야 함.
- FPV 영상(RunCam WiFiLink V2 + `fpv4win`)은 LoRa/MAVLink 경로와 별개의 WiFi 링크 —
  통합 작업 불필요, 지상 PC에서 병행 실행만 하면 됨.

## 폐기되는 구성요소

| 구성요소 | 사유 |
| --- | --- |
| `cfs-telemetry-app` (4개 cFS 앱 전체) | 컴패니언 컴퓨터(Pi) 전제, Pi 사용 불가로 무의미 |
| `openMCT` 레포의 기존 코드 (`fc_serial_ws_server.py`, `lora_protocol_v2.py`) | DL2/UP2/ACK2 커스텀 프레임 파싱 목적, 더 이상 불필요 — Open MCT를 커스텀 시각화용으로 병행할 경우 표준 MAVLink 디코딩 브리지로 재작성 필요 (아래 "신규 아키텍처" 참조) |
| DL2 다운링크 커스텀 프로토콜 | MAVLink 표준 메시지로 대체 (`docs/payload-spec.md` 참조) |
| TDM 슬롯 타이밍(CYCLE/RX WINDOW) | 양방향 스트림인 MAVLink에는 불필요 |

## 유지/이관이 필요한 기능

| 구 기능 | 담당 (cFS) | 신규 대응 |
| --- | --- | --- |
| 자세/위치/GPS/시각 텔레메트리 | `mavlink_bridge_app` → DL2 | QGC 표준 MAVLink 수신 (`docs/payload-spec.md`) |
| 미션 업로드/재조회 | `mavlink_bridge_app` + `uplink_app` | QGC "Plan" 뷰에서 직접 업/다운로드 |
| 시스템 헬스 판정 + 자동 재시작 | `cfs_core_app` | **대응 없음** — 컴패니언 컴퓨터 전제 기능, 이관 불가. PX4 자체 failsafe로 일부만 대체 |
| 링크 상태(CONNECTED/DEGRADED/DISCONNECTED) | `lora_tdm_app` | MAVLink heartbeat 타임아웃 + QGC 자체 링크 상태 표시로 대체 |

## 확인 필요 사항

- LR24-F 투명 모드에서 2.4KB/s 대역폭이 QGC 기본 스트림 레이트(자세/위치 등 5Hz 유지 시)를
  감당하는지 실측 필요 — `docs/payload-spec.md`의 메시지별 레이트 참조.
- LR24-F가 RADIO_STATUS(RSSI 등) MAVLink 라디오 상태 메시지를 자체 주입하는지 확인 필요.
