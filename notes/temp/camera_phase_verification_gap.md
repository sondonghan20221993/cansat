# camera — P2/P4 실기체 확인 미완료 (P1/P3/P5는 해결됨)

## 배경

`camera/README.md` §페이즈별 목표. RunCam WiFiLink V2(OpenIPC) FPV 카메라 통합 작업.
초기엔 P0만 완료로 기록돼 있었으나, 실제로는 2026-07-13 벤치 검증에서 P1/P3도
확인됐던 것이 표에 반영이 안 돼 있었음(2026-07-16 표 정정).

## 현황 (2026-07-16 기준)

| 페이즈 | 목표 | 확인 스크립트 | 상태 |
| --- | --- | --- | --- |
| P0 영상 링크 | WFB-ng RF로 지상에서 영상 수신 | (육안 확인) | ✅ 완료 |
| P1 대역분리 | WFB-ng 채널 5.8GHz대(LoRa 2.4GHz와 비중첩) | `./check_band_separation.sh` | ✅ 완료 (2026-07-13 실측 — 채널 161) |
| P2 이더넷 접근 | 기체 장착 상태로 Pi 경유 SSH 접속 | `./check_ethernet_access.sh [Pi호스트] [카메라IP]` | ⬜ 미확인 — **원인 파악도 필요**. 2026-07-13은 PC 직결로 검증, Pi jump host 경유는 미실측. Pi 온라인 확인됨 — 실행 가능. 안 되면 카메라측(이더넷 포트/스위치)인지 Pi측(라우팅/방화벽)인지 원인 구분 필요 |
| P3 설정 적용 | majestic OSD/녹화 설정 + msposd 배포 | `./apply_camera_config.sh` → `./verify_camera.sh` | ✅ 완료 (2026-07-13 실측 — OSD 라이브, 녹화, RTSP 모두 활성화 확인. PC 직결 기준, Pi 경유 재검증은 P2와 같이 남음) |
| P4 SD 녹화 실물 확인 | 설정 적용 후 실제 녹화 파일 생성 | `./check_sd_recording.sh` | ⬜ 미확인 — **원인 파악도 필요**. `.records.enabled=true` 설정만 됨, 카메라 SD카드 실제 파일 생성은 미실측(fpv4win PC측 녹화만 확인됨). 파일이 안 생기면 SD 마운트 문제/설정 누락/카드 자체 이상 중 원인 구분 필요 |
| P5 시각동기 | 카메라 OSD 타임스탬프 절대시각화 | — | 🟡 우회됨 — 영상↔텔레메트리 매칭은 `correlate_video_telemetry.py`(지상 PC 시계 기준 사후 매칭)로 이미 해결. 카메라 자체 절대시각 OSD는 낮은 우선순위 |

## 남은 작업은 사실상 2개 — 둘 다 "확인 + 안 되면 원인 파악"까지 필요

1. **P2**: Pi를 jump host로 실제 SSH 접속되는지. 안 되면 카메라측/Pi측 중 원인 구분
2. **P4**: 카메라 SD카드에 실제 녹화 파일이 생성되는지. 안 되면 마운트/설정/카드 중 원인 구분

## 추가 미해결 항목 (TODO(bench), P1~P5와 별개)

- **user.ini 부팅시 덮어쓰기**: RunCam 공장 이미지 자체 `user.ini`가 부팅 시 `majestic.yaml` 직접수정을 되돌릴 가능성 — 실기체 확인 필요
- **msposd/OSD 설정 재부팅 유실**: 영구 적용(부팅 스크립트 등록) 미해결 — 매 재부팅마다 수동 재실행 필요
- **SD 녹화 파일에 OSD 미반영**: msposd는 라이브 스트림에만 OSD 합성, majestic이 SD에 저장하는 mp4는 센서 원본 그대로 저장 — OSD 안 박힘 (오디오도 동일 이유로 누락).
  해결책(미적용): msposd 기동 시 `--subtitle <path>` 옵션 → 사이드카 `.osd`/`.srt` 파일 생성, 비행 후 walksnail-osd-tool로 mp4+osd 합성. 실기체 미검증.

## 다음 단계

Pi(192.168.50.65)가 온라인이므로 P2부터 시작 가능:

```bash
cd camera/
./check_ethernet_access.sh sdh2983@192.168.50.65 <카메라IP>
./check_sd_recording.sh <카메라IP> sdh2983@192.168.50.65 [분]
```

## 참고

- `camera/README.md` §페이즈별 목표, §실제 적용 결과(2026-07-13) — P1/P3 실측 근거
- `camera/correlate_video_telemetry.py` — 시각 매칭 스크립트(P5 우회 경로)
