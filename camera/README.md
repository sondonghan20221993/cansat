# camera/ — RunCam WiFiLink V2 (OpenIPC) 연동 프로토타입

상태: **프로토타입, 최소 동작 확인 완료** (2026-07-13). 기체 장착 상태에서 PC 이더넷 직결로
설정 적용 및 영상/OSD 확인 완료. 벤치 미확인 항목은 `TODO(bench)`로 표시.

## 최종 확정 설정값 (2026-07-13, 카메라 IP 192.168.1.10)

| 항목 | 값 |
| --- | --- |
| `.video0.size` | `1920x1080` |
| `.video0.fps` | `90` |
| `.video0.bitrate` | `8192` |
| `.video0.codec` | `h265` |
| `.isp.antiFlickerFreq` | `60` |
| `.osd.enabled` | `true` |
| `.osd.external` | `true` |
| `.osd.externalAddress` | `127.0.0.1:14551` |
| `.records.enabled` | `true` |
| `.rtsp.enabled` | `true` |
| WFB-ng 채널 | `161` (5.8GHz) |

**재부팅 후 영구성**:
- ✅ majestic 설정(위 표 전체)은 `/etc/majestic.yaml`에 저장되어 재부팅해도 유지됨
- ❌ **msposd 프로세스는 유지 안 됨** — 부팅 자동시작 훅 미등록(TODO), 재부팅마다 아래 재실행 필요:
  ```powershell
  Get-Content \\wsl$\Ubuntu\home\sdh2983\cfs-telemetry-app\camera\msposd_air.sh | ssh root@192.168.1.10 "cat > /usr/bin/msposd_air.sh"
  ssh root@192.168.1.10 "chmod +x /usr/bin/msposd_air.sh; /usr/bin/msposd_air.sh &"
  ```
  (majestic의 `.osd.external: true`는 남아있어도 msposd가 안 돌면 OSD 데이터 소스가 없어 라이브 화면에 타임스탬프 안 뜸)

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
| `correlate_video_telemetry.py` | fpv4win 녹화(mp4)와 openMCT 텔레메트리 CSV를 절대시각으로 매칭 | 지상 PC, 착륙 후 실행 |

## 페이즈별 목표 및 확인 코드

기체 장착 상태(PC 직결 불가) 기준. Pi(192.168.50.65)를 이더넷 경유(jump host)로 사용.

| 페이즈 | 목표 | 확인 코드 | 상태 |
| --- | --- | --- | --- |
| P0 영상 링크 | WFB-ng RF로 지상에서 영상 수신 | (fpv4win 육안 확인, 스크립트화 불필요) | ✅ 완료 |
| P1 대역분리 | WFB-ng 채널이 5.8GHz대(LoRa 2.4GHz와 비중첩) | `./check_band_separation.sh <카메라IP> [Pi경유]` | ✅ 완료 (2026-07-13 실측 — 채널 161/5.8GHz로 LoRa 2.4GHz와 비중첩 확인, §"실제 적용 결과" 참조) |
| P2 이더넷 접근 | 기체 장착 상태로 Pi 경유 SSH 접속 가능 | `./check_ethernet_access.sh [Pi호스트] [카메라IP]` | ⬜ 미확인 — **원인 파악도 필요**. 2026-07-13 검증은 PC 이더넷 **직결**로 수행(기체 미장착 상태), Pi jump host 경유는 아직 실측 안 됨. 안 되면 원인이 카메라측(이더넷 포트/스위치 설정)인지 Pi측(라우팅/방화벽)인지 구분 필요. Pi는 온라인 확인됨(2026-07-16) — 스크립트 실행 가능 |
| P3 설정 적용 | majestic OSD/녹화 설정 + msposd 배포 | `./apply_camera_config.sh <카메라IP> [Pi경유]` → `./verify_camera.sh <카메라IP> [Pi경유]` | ✅ 완료 (2026-07-13 실측 — OSD 라이브 활성화, 녹화 활성화, RTSP 활성화 확인. 단 PC 직결 기준, P2와 동일한 "Pi 경유" 재검증 여지 있음) |
| P4 SD 녹화 실물 확인 | 설정 적용 후 실제 녹화 파일 생성 | `./check_sd_recording.sh <카메라IP> [Pi경유] [분]` | ⬜ 미확인 — **원인 파악도 필요**. `.records.enabled=true`는 설정됐으나 카메라 SD카드에 실제 파일이 생성되는지 미실측(fpv4win PC측 녹화만 확인됨). 파일이 안 생기면 SD카드 마운트 문제인지, records 설정 누락인지, 카드 자체 이상인지 구분 필요 |
| P5 시각동기 | 카메라 OSD 타임스탬프를 절대시각으로 | (보류 — Pi GPS 동기 `mavlink_bridge_app_behavior_spec.md` §16.4 선행 필요) | 🟡 우회됨 (2026-07-16: 영상↔텔레메트리 매칭은 카메라 NTP 없이 §6 "시각 매칭 방식"으로 이미 해결 — fpv4win 파일명(`<epoch_ms>.mp4`)과 openMCT CSV가 같은 지상 PC 시계 기준이라 사후 매칭 가능. `correlate_video_telemetry.py` 참조. 카메라 자체 SD 녹화의 OSD 타임스탬프 절대화는 여전히 미착수이나, 운용 우선순위는 낮음 — 필요 재확인 시에만 착수) |

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
   (`notes/mavlink_bridge_app_behavior_spec.md` §16.4 — GPS 시각의 DL2 다운링크
   전달까지는 완료(2026-07-14), Pi OS 시계 반영(chrony)은 아직 미착수) 완료 후 의미 있음

## 참조 (기반 오픈소스)

- OpenIPC firmware: https://github.com/OpenIPC/firmware
- msposd (OSD 번인): https://github.com/OpenIPC/msposd — SSC338Q용 바이너리 `msposd_star6e`
- majestic 스트리머/엔드포인트: https://openipc.org/majestic-endpoints (`/image.jpg`, RTSP `stream0`)
- WiFiLink 설정 위키: https://github.com/OpenIPC/wiki/blob/master/en/fpv-runcam-wifilink-openipc.md
- 하드웨어 문서: https://docs.openipc.org/hardware/runcam/vtx/runcam-wifilink-v2/

## 실제 적용 결과 (2026-07-13)

### 환경
- 카메라: 기체 장착 상태, FC↔카메라 UART 배선 완료
- 지상국: Windows PC + fpv4win (채널 161, H264)
- 네트워크: PC 이더넷 직결 (192.168.1.x 고정 IP)

### 적용 단계 및 명령어

**1. PC 네트워크 설정**
```powershell
# Windows PowerShell (관리자)
New-NetIPAddress -InterfaceAlias "이더넷" -IPAddress 192.168.1.50 -PrefixLength 24
ping 192.168.1.10  # 카메라 핑 응답 확인
```

**2. 카메라 SSH 접속 및 설정**
```powershell
ssh root@192.168.1.10  # 비밀번호: 12345

# CLI로 설정 적용 (카메라 쉘에서)
cli -s .osd.enabled true
cli -s .osd.template "%d.%m.%Y %H:%M:%S"
cli -s .osd.posX 16
cli -s .osd.posY 16
cli -s .records.enabled true
cli -s .records.split 60
cli -s .records.maxUsage 90
cli -s .rtsp.enabled true
```

**3. msposd 배포 (SSH 파이프)**
```powershell
# PowerShell에서
Get-Content \\wsl$\Ubuntu\home\sdh2983\cfs-telemetry-app\camera\msposd_air.sh | ssh root@192.168.1.10 "cat > /usr/bin/msposd_air.sh"
ssh root@192.168.1.10 "chmod +x /usr/bin/msposd_air.sh && killall -1 majestic"
```

**4. 외부 OSD 활성화 (msposd↔majestic 연동)**
```powershell
ssh root@192.168.1.10 "cli -s .osd.external true"
ssh root@192.168.1.10 "cli -s .osd.externalAddress 127.0.0.1:14551"
ssh root@192.168.1.10 "killall -1 majestic"
```

**5. 코덱 설정 (H264 사용, HEVC 호환성 문제 회피)**
```powershell
ssh root@192.168.1.10 "cli -s .encoder.codec h264"
# fpv4win에서도 Codec → H264로 변경
```

**6. 카메라 재부팅 후 fpv4win 재연결**

### 결과
- ✅ **P0 영상 링크**: WFB-ng 채널 161(5.8GHz)에서 fpv4win 수신
- ✅ **P1 대역분리**: 채널 161로 LoRa(2.4GHz)와 비중첩 확인
- ✅ **P3 설정 적용**: OSD 활성화(라이브 화면), 녹화 활성화, RTSP 활성화
- ⚠️ **fpv4win 녹화(mp4) 자체 지원 확인**: GitHub 공식 문서엔 "Todo"로 나오지만 실제
  0.0.5-beta 바이너리에는 `startRecord`/`stopRecord` 구현돼 있음 (우측하단 버튼).
  저장 경로: `fpv4win.exe`와 같은 폴더 `mp4/<epoch_ms>.mp4`
- ❌ **OSD가 녹화 파일엔 안 찍힘**: 라이브 화면엔 타임스탬프 보이는데 저장된 mp4엔 없음.
  프레임 추출(ffprobe+ffmpeg)로 확인 — majestic의 `.osd.external` burn-in이 fpv4win이
  받는 WFB-ng RTP 스트림엔 반영 안 되고, 라이브 화면의 타임스탬프는 fpv4win 자체 UI
  오버레이로 추정. **채택한 대안(§아래 "시각 매칭" 참조)**: OSD 번인 포기, 착륙 후
  영상 파일명(PC epoch ms)과 텔레메트리 로그(openMCT CSV, `datetime.now().isoformat()`
  절대시각 이미 기록됨)를 매칭하는 사후 처리로 대체.
- ✅ **1080p 화질 저하 원인 확인 및 해결**: 최초엔 `.video0.size`만 바꾸고 `.video0.fps`가
  120에 고정된 채라 발생한 문제였음 — IMX415는 **1080p@90fps 또는 720p@120fps**만 지원
  ([OpenIPC/firmware#1179](https://github.com/OpenIPC/firmware/issues/1179)와 동일 증상).
  `fps 90` 먼저 맞추고 `size 1920x1080` 적용하니 해상도 정상화. 이후 나타난 깜빡임은
  실내 형광등(60Hz)과 센서 안티플리커 주파수 불일치였고 `.isp.antiFlickerFreq 60` 설정으로 해소.
  화면 가장자리가 휘어 보이는 것은 렌즈 FOV 160°(광각) 특성상 정상(왜곡 아님).

### 트러블슈팅

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| HEVC(H265) 디코딩 에러 | fpv4win FFmpeg 미지원 | 코덱을 H264로 변경 |
| H264 data partitioning 에러 | 카메라의 비표준 H264 변형 | (허용하고 진행, 영상 정상) |
| OSD 미표시(라이브) | `.osd.external` 비활성화 | `cli -s .osd.external true` 및 externalAddress 설정 |
| msposd 미실행 | `/usr/bin/msposd_air.sh` 배포 실패 | SCP 대신 SSH stdin 파이프 사용 |
| 재부팅 후 OSD/msposd 재설정 필요 | `cli -s`가 즉시 적용되나 재부팅 시 msposd 자동시작 훅 없음(TODO) | 재부팅마다 §4 재실행 필요, 부팅 스크립트 등록은 미해결 |
| 녹화 파일에 OSD 안 찍힘 | fpv4win 녹화가 burn-in 이전 스트림을 저장하는 것으로 추정 | OSD 번인 포기, 로그 매칭 방식 채택 |
| 1080p 화질 저하/손상 | fps(120)와 해상도(1080p) 조합 불가 (IMX415는 1080p@90fps/720p@120fps만 지원) | fps부터 90으로 낮춘 후 size 1920x1080 적용 |
| 1080p90 화면 깜빡임 | 실내 형광등(60Hz)과 안티플리커 주파수 불일치 | `cli -s .isp.antiFlickerFreq 60` |
| 화면 가장자리 휘어 보임 | 렌즈 FOV 160° 광각 특성 (정상) | 조치 불필요 |
| 영상 미표시, `[udp @ ...] bind failed: -10013` (되다가 재부팅 후 갑자기 안 됨) | winnat(Hyper-V/WSL) 동적 포트 예약이 fpv4win 로컬 RTP 포트(52356)를 점유 — 예약 범위가 부팅마다 달라져 간헐 발생 | 관리자 PowerShell에서 `net stop winnat` 후 fpv4win 실행 (해결 확인 2026-07-16). 상세: 아래 "fpv4win 영상 미표시" 참조 |
| USB 어댑터 12MBit/s 저속 폴백 + `rtw_read: iostream stream error` | Zadig(WinUSB) 드라이버 바인딩 손상 | 장치관리자에서 드라이버 제거 → 재삽입 → Zadig로 WinUSB 재설치 (480MBit/s 복구 확인) |

### 주요 발견

- **SCP 미지원**: OpenIPC sftp-server 없음 → SSH stdin 파이프로 파일 전달 필수
- **코덱 호환성**: H265는 fpv4win FFmpeg에서 호환성 문제, H264 권장 (단, H264도 완전하진 않음 — data partitioning 경고 존재)
- **외부 OSD는 라이브 화면 전용**: 녹화 파일엔 반영 안 됨 — 녹화용 타임스탬프가 필요하면 카메라 SD카드 자체 녹화(진짜 burn-in 경로) 사용해야 함
- **OSD-미녹화 근본원인 재확인 (2026-07-15, 미검증 정보)**: msposd는 OSD를 wfb-ng
  라이브 스트림 위에만 합성하고, majestic이 SD카드에 저장하는 로컬 mp4는 센서
  원본 스트림을 그대로 저장해 OSD 합성 단계를 거치지 않음 — 라이브 경로와 로컬
  녹화 경로가 애초에 분리된 구조(오디오도 같은 이유로 mp4에 안 들어감). fpv4win
  녹화(위 실측)뿐 아니라 카메라 SD카드 자체 녹화도 동일 제약일 가능성 높음.
  **해결책(미적용, TODO(bench))**: msposd 기동 시 `--subtitle <path>` 옵션 추가 →
  mp4 녹화 시작마다 동일 이름의 `.osd`(walksnail-osd-tool 호환)/`.srt` 사이드카
  파일을 별도 생성, 녹화 종료 시 같이 닫힘. air측에서 recording 폴더 감시하려면
  majestic을 flat 구조로: `cli -i /etc/majestic.yaml -s .records.path
  /mnt/mmcblk0p1/%F/..`. 비행 후 mp4+osd를 walksnail-osd-tool로 합성하면 OSD
  박힌 결과물 획득 가능. 출처: msposd README(OpenIPC/msposd), 직접 실기체 검증 안 됨.
- **카메라 재부팅**: 설정 적용 후 전원 껐다 켜야 안정화되지만, msposd/OSD 설정은 재부팅마다 유실됨 (영구 적용 미해결)
- **시각 매칭 방식 채택**: 영상의 시작 절대시각과 openMCT 텔레메트리 CSV의
  `timestamp`(ISO, PC 수신시각)가 동일한 시간축이라는 전제로 사후 매칭. 전제:
  fpv4win과 `fc_serial_ws_server.py`가 같은 PC(같은 시계)에서 실행됨(다른 PC면
  두 PC 간 NTP 동기화 필요, TODO(bench) 확인).
  ```bash
  python3 camera/correlate_video_telemetry.py <video.mp4> <telemetry.csv> [--out matched.csv]
  ```
  영상 구간에 속하는 텔레메트리 행만 추출, 각 행에 `video_offset_sec`(영상 재생 시점) 추가.
  **영상 시작 시각 소스는 2단계**(2026-07-16 개선, `mp4` 로컬 테스트로 양쪽 경로 검증):
  1. mp4 컨테이너의 `creation_time` 메타데이터(ffprobe) — 녹화 버튼→실제 인코딩 시작
     사이 지연 없이 컨테이너가 기록한 실제 시각. fpv4win이 이 태그를 실제로 남기는지는
     `TODO(bench)` 확인 필요.
  2. 없으면 파일명(`<epoch_ms>.mp4`) 파싱으로 폴백 — 녹화 버튼 누른 시점의 PC 시계
     (버튼→실제 녹화 시작 지연만큼 오차 가능, 기존 방식).

  실행 시 어느 소스를 썼는지 콘솔에 출력됨(`시작 시각 소스: ...`).

- **fpv4win 영상 미표시 트러블슈팅 전말 (2026-07-16, 해결)**: 재부팅 후
  갑자기 영상이 안 뜨는 문제. 최종 원인은 **winnat(Hyper-V/WSL) 동적 포트
  예약**이 fpv4win의 로컬 RTP 포트(52356)를 점유한 것 — 예약 범위
  (`netsh interface ipv4 show excludedportrange protocol=udp`)가 부팅마다
  달라져서, 예전엔 되던 게 갑자기 `bind failed: -10013`으로 깨짐.
  **해결: 관리자 PowerShell에서 `net stop winnat` 후 fpv4win 실행** (필요 시
  이후 `net start winnat`으로 복구 — WSL/Docker 네트워크에 필요).
  - 진단 과정에서 배제된 가설들 (기록용):
    - ~~키 불일치~~: 카메라 `/etc/drone.key`와 fpv4win `gs.key` md5 완전 동일
      (`24767056...`). fpv4win은 키 없으면 `Unable to open gs.key`를 명시적으로
      출력하므로 로드 실패도 아님.
    - ~~채널/대역~~: 카메라 `iw dev`로 5805MHz(채널 161) 실측 정상, 카메라측
      wfb_tx/majestic/msposd 전 프로세스 정상 동작 확인.
    - ~~어댑터 5.8GHz 미지원~~: AC1200(RTL8812AU) 듀얼밴드. `SwChnl false`
      로그는 뜨지만 실제 수신엔 지장 없음.
  - 함께 발견/해결된 별개 문제: USB 12MBit/s 저속 폴백(→ Zadig 드라이버
    재설치로 480MBit/s 복구), fpv4win 2개 중복 실행(USB/포트 점유 충돌 유발).
  - 잔여 관찰: `Unable to decrypt packet` 로그가 진단 중 반복 출력됐으나
    winnat 해결 후 영상 정상 — 중복 실행 인스턴스 간 USB 점유 충돌로 인한
    수신 데이터 손상이었을 가능성. 재발 시에만 재조사.

## 주의

- RunCam 공장 이미지에는 자체 `user.ini`(부팅 시 설정 덮어쓰기)가 있음 — majestic.yaml 직접
  수정이 부팅 후 되돌아가면 `user.ini`를 먼저 확인할 것 (TODO(bench)).
- MSP와 MAVLink를 같은 UART에 동시 설정 금지. 이 프로토타입은 MSP DisplayPort 단독.
- 카메라 시계는 NTP 동기 전까지 부정확 — OSD의 majestic 타임스탬프는 §6 완료 전에는
  상대시간으로만 신뢰. FC GPS 시각 OSD 요소는 그와 무관하게 정확.
