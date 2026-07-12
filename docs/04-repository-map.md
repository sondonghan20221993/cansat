# 04. Repository Map — 논리 모듈 ↔ 구현 리포 대응

작성: 2026-07-13. 이 문서는 02-system-architecture.md의 논리 모듈이 실제로 어느
저장소·파일에 구현되어 있고, 상세 명세(단일 원본)가 어디 있는지를 기록한다.

## 1. 리포 목록

| 리포 | 위치 | 역할 |
| --- | --- | --- |
| `cfs-telemetry-app` | github.com/sondonghan20221993/cfs-telemetry-app | 기체(CM=Raspberry Pi) 측 cFS 앱 + 프로토콜 참조 구현 + 카메라 설정 |
| `openMCT` | 로컬 `Documents/GitHub/openMCT` | 지상국 — Open MCT 대시보드 + 지상 LoRa 브리지 |
| `cansat_2` (본 리포) | 로컬 `Documents/GitHub/cansat_2` | 시스템 상위 명세 (요구사항/아키텍처/인터페이스/검증) |
| `3d_reconstruction` 등 | Documents/GitHub | Reconstruction/Alignment 모듈 (module-ownership-guide.md 참조) |

## 2. 논리 모듈 → 구현 대응

| 논리 모듈 (02 §3) | 리포 | 구현 | 상세 spec (단일 원본) |
| --- | --- | --- | --- |
| Telemetry Interface (LoRa) | cfs-telemetry-app | `lora_tdm_app/` | `notes/lora_tdm_app_behavior_spec.md` (v1), `notes/lora_protocol_v2_spec.md` (v2 초안) |
| MAVLink Bridge | cfs-telemetry-app | `mavlink_bridge_app/` | `notes/mavlink_bridge_app_behavior_spec.md` (§16 = 시각 동기) |
| cFS Integration Layer | cfs-telemetry-app | `cfs_core_app/`, `uplink_app/` | `notes/cfs_core_app_behavior_spec.md`, `notes/mission_app_runtime_spec.md` |
| Image / Video Path (기체) | cfs-telemetry-app | `camera/` (WiFiLink V2 = OpenIPC 설정 프로토타입) | `camera/README.md` |
| Image / Video Path (지상) | — | Windows + RTL8812AU + fpv4win (WFB-ng 수신, 코드 없음) | `camera/README.md` §목표 구성 |
| **Ground Station** (02에 미등재 — §4 참조) | openMCT | `fc_serial_ws_server.py` (v1 디코드·CSV·업링크 빌더·WS), `my_openmct_app/` (대시보드·uplinkCLI) | `openmct_bridge_notes.md` |
| LoRa v2 프로토콜 라이브러리 | cfs-telemetry-app | `bridge/lora_downlink_decoder.py` (참조 구현 + 진단 도구) | `notes/lora_protocol_v2_spec.md` |
| 시각 동기 체인 | cfs-telemetry-app | mavlink_bridge(SYSTEM_TIME 파싱) + `camera/pi_chrony_camera.conf` | 03-interface §6.1 (상위), mavlink_bridge spec §16 (상세) |

## 3. 명세 위임 규칙 (drift 방지)

같은 사실을 여러 문서에 중복 기술하지 않기 위해, 소유 계층을 다음과 같이 나눈다.

| 계층 | 소유 문서 | 소유 내용 |
| --- | --- | --- |
| 시스템 계약 | cansat_2 `03-interface-specification.md` | 링크 역할(LORA/IMG_VID), 상관관계 필드(frame_id/job_id/seq), 시각 기준(UTC, §6.1 체인), 대역 분리 제약 |
| Wire format | cfs-telemetry-app `notes/lora_protocol_v2_spec.md` (v2) / `notes/lora_tdm_app_behavior_spec.md` §8 (v1) | 프레임 바이트 레이아웃, CRC, TDM 타이밍 |
| 앱 동작 | cfs-telemetry-app `notes/*_behavior_spec.md` | 각 cFS 앱의 상태머신·EVS·HK |
| 지상 구현 | openMCT `openmct_bridge_notes.md` | ws 서버·대시보드 동작 (wire format은 위를 참조만) |

규칙: 하위 계층 문서가 상위 계약과 충돌하면 상위(cansat_2)를 갱신하거나 하위를 고친다.
wire format을 openMCT나 cansat_2에 복사 기술하지 않는다 — 링크로 참조한다.

## 4. 인터페이스 변경 체크리스트

LoRa wire format(또는 링크 계약)이 바뀔 때 함께 갱신해야 하는 것:

1. `cfs-telemetry-app/notes/lora_protocol_v2_spec.md` — wire format 단일 원본
2. 기체 구현: `lora_tdm_app/` (C) + 단위테스트
3. 참조 구현: `bridge/lora_downlink_decoder.py` + `tests/test_lora_downlink_decoder.py`
4. 지상 구현: `openMCT/fc_serial_ws_server.py` (+ 대시보드 필드 변경 시 `my_openmct_app/`)
5. 본 리포: 03-interface (계약 변경 시에만), 08-verification (크로스 리포 TC)

## 5. 알려진 크로스 리포 갭 (2026-07-13 기준)

| 갭 | 위치 | 영향 |
| --- | --- | --- |
| 지상국이 ACK 프레임을 송신하지 않음 | openMCT `fc_serial_ws_server.py` | 기체 `LinkState`가 CONNECTED로 전이 불가 (lora_tdm spec §11) — v2 통합 시 ACK2 회신으로 해소 예정 |
| ws 서버 수신 루프가 `readline()` 기반 | openMCT | v2 바이너리(종단 문자 없음) 수신 불가 — 바이트 스트림 상태머신으로 교체 필요 |
| `FC_SYS_TIME_MID` SB 발행 미구현 | cfs-telemetry-app | 시각 동기 체인(03-interface §6.1) 미가동 |
| mavlink_bridge 파서 STX 이스케이프 결함 | cfs-telemetry-app | 페이로드 내 0xFD/0xFE에서 프레임 유실 (~20%/28B 프레임) — P1 수정 대기 |
