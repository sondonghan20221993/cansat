# camera/ — RunCam WiFiLink V2 (OpenIPC) 연동 프로토타입

상태: **프로토타입** (2026-07-13). 벤치에서 실기기 확인하며 `TODO(bench)` 항목을 채운다.

## 대상 하드웨어

- RunCam WiFiLink V2 — OpenIPC 펌웨어(`ssc338q_fpv_openipc_urllc_aio_nor`), SSC338Q + IMX415
- 접속: 이더넷 직결, 카메라 기본 IP `192.168.1.10` (PC를 `192.168.1.x` 고정 IP로)
- SSH: `root@192.168.1.10` (OpenIPC FPV 기본 암호 `12345` — TODO(bench): 실제 확인)

## 목표 구성

```
FC ──UART(MSP DisplayPort)──▶ WiFiLink(msposd 번인 OSD + majestic 타임스탬프)
                                  │ WFB-ng 5.8GHz (LR24-F 2.4GHz와 대역 분리)
                                  │ SD카드 자동 녹화 (OSD 포함)
Pi ──이더넷──▶ WiFiLink (chrony NTP: Pi 시각 → 카메라 시계)
```

## 파일 구성

| 파일 | 용도 | 적용 위치 |
| --- | --- | --- |
| `majestic_fragment.yaml` | OSD 타임스탬프 + SD 녹화 + RTSP 설정 조각 | 카메라 `/etc/majestic.yaml`에 병합 |
| `msposd_air.sh` | msposd 공중부 번인 모드 기동 라인 | 카메라 부팅 스크립트 |
| `ardupilot_msp_osd.param` | FC측 MSP DisplayPort OSD 파라미터 | Mission Planner로 FC에 적용 |
| `apply_camera_config.sh` | 위 설정을 ssh/scp로 카메라에 적용 | 벤치 PC/Pi에서 실행 |
| `verify_camera.sh` | 적용 후 검증 (ping, 스냅샷, RTSP, msposd 프로세스) | 벤치 PC/Pi에서 실행 |
| `pi_chrony_camera.conf` | Pi를 카메라의 NTP 서버로 (§시각 동기) | Pi `/etc/chrony/conf.d/` |

## 페이즈별 목표 및 확인 코드

기체 장착 상태(PC 직결 불가) 기준. Pi(192.168.50.65)를 이더넷 경유(jump host)로 사용.

| 페이즈 | 목표 | 확인 코드 | 상태 |
| --- | --- | --- | --- |
| P0 영상 링크 | WFB-ng RF로 지상에서 영상 수신 | (fpv4win 육안 확인, 스크립트화 불필요) | ✅ 완료 |
| P1 대역분리 | WFB-ng 채널이 5.8GHz대(LoRa 2.4GHz와 비중첩) | `./check_band_separation.sh <카메라IP> [Pi경유]` | ⬜ 미확인 |
| P2 이더넷 접근 | 기체 장착 상태로 Pi 경유 SSH 접속 가능 | `./check_ethernet_access.sh [Pi호스트] [카메라IP]` | ⬜ 미확인 (Pi 오프라인 확인됨) |
| P3 설정 적용 | majestic OSD/녹화 설정 + msposd 배포 | `./apply_camera_config.sh <카메라IP> [Pi경유]` → `./verify_camera.sh <카메라IP> [Pi경유]` | ⬜ P2 선행 필요 |
| P4 SD 녹화 실물 확인 | 설정 적용 후 실제 녹화 파일 생성 | `./check_sd_recording.sh <카메라IP> [Pi경유] [분]` | ⬜ P3 선행 필요 |
| P5 시각동기 | 카메라 OSD 타임스탬프를 절대시각으로 | (보류 — Pi GPS 동기 `mavlink_bridge_app_behavior_spec.md` §16.4 선행 필요) | ⬜ 차단 |

각 스크립트는 `<Pi경유>` 인자를 생략하면 PC 직결을 시도한다 (예: `sdh2983@192.168.50.65` 형식으로 지정 시 `ssh -J`로 Pi를 경유).

## 적용 순서

1. **대역 분리 (최우선)**: WFB-ng 채널을 5.8GHz로. OpenIPC Configurator 또는 카메라에서
   `/etc/wfb.conf`(또는 `wfb.yaml` — 펌웨어 버전에 따라 다름, TODO(bench))의 `channel`을
   5.8GHz 채널(예: 161)로 설정. 지상 수신측(fpv4win/PixelPilot)도 동일 채널로.
2. `./apply_camera_config.sh 192.168.1.10` — majestic 설정 + msposd 기동 스크립트 배포
3. FC에 `ardupilot_msp_osd.param` 적용 + FC↔카메라 UART 배선 (rx↔tx 크로스)
4. `./verify_camera.sh 192.168.1.10` — 스냅샷/RTSP/OSD 확인
5. SD카드 삽입 → 재부팅 → `/mnt/mmcblk0p1`(TODO(bench): 실제 마운트 경로)에 녹화 파일 생성 확인
6. (시각 동기, 선택) Pi에 `pi_chrony_camera.conf` 적용 — 전제: Pi 시계의 GPS 동기 체인
   (`notes/mavlink_bridge_app_behavior_spec.md` §16.4, 미구현) 완료 후 의미 있음

## 참조 (기반 오픈소스)

- OpenIPC firmware: https://github.com/OpenIPC/firmware
- msposd (OSD 번인): https://github.com/OpenIPC/msposd — SSC338Q용 바이너리 `msposd_star6e`
- majestic 스트리머/엔드포인트: https://openipc.org/majestic-endpoints (`/image.jpg`, RTSP `stream0`)
- WiFiLink 설정 위키: https://github.com/OpenIPC/wiki/blob/master/en/fpv-runcam-wifilink-openipc.md
- 하드웨어 문서: https://docs.openipc.org/hardware/runcam/vtx/runcam-wifilink-v2/

## 주의

- RunCam 공장 이미지에는 자체 `user.ini`(부팅 시 설정 덮어쓰기)가 있음 — majestic.yaml 직접
  수정이 부팅 후 되돌아가면 `user.ini`를 먼저 확인할 것 (TODO(bench)).
- MSP와 MAVLink를 같은 UART에 동시 설정 금지. 이 프로토타입은 MSP DisplayPort 단독.
- 카메라 시계는 NTP 동기 전까지 부정확 — OSD의 majestic 타임스탬프는 §6 완료 전에는
  상대시간으로만 신뢰. FC GPS 시각 OSD 요소는 그와 무관하게 정확.
