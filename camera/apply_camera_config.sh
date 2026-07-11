#!/usr/bin/env bash
# WiFiLink V2(OpenIPC)에 프로토타입 설정 적용
# 사용법: ./apply_camera_config.sh [카메라IP]   (기본 192.168.1.10)
# 전제: 카메라 이더넷 접속 가능, ssh root 로그인 (OpenIPC 기본 암호 12345 — TODO(bench))
set -euo pipefail

CAM_IP="${1:-192.168.1.10}"
SSH="ssh -o StrictHostKeyChecking=no root@${CAM_IP}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "== [1/4] 접속 확인 =="
$SSH 'echo "firmware: $(cat /etc/os-release 2>/dev/null | head -1)"; ipcinfo -c 2>/dev/null || true'

echo "== [2/4] majestic 설정 (cli) =="
# majestic 내장 cli로 개별 키 설정 — user.ini 부팅 덮어쓰기와의 충돌은 README 주의 참조
$SSH 'cli -s .osd.enabled true'
$SSH 'cli -s .osd.template "%d.%m.%Y %H:%M:%S"'
$SSH 'cli -s .osd.posX 16'
$SSH 'cli -s .osd.posY 16'
$SSH 'cli -s .records.enabled true'
$SSH 'cli -s .records.split 60'
$SSH 'cli -s .records.maxUsage 90'
$SSH 'cli -s .rtsp.enabled true'
# TODO(bench): SD 마운트 경로 확인 후 records.path 설정
$SSH 'cli -s .records.path /mnt/mmcblk0p1/records || true'

echo "== [3/4] msposd 기동 스크립트 배포 =="
scp -o StrictHostKeyChecking=no "${SCRIPT_DIR}/msposd_air.sh" "root@${CAM_IP}:/usr/bin/msposd_air.sh"
$SSH 'chmod +x /usr/bin/msposd_air.sh'
# TODO(bench): RunCam 이미지의 기존 msposd 서비스 여부 확인 후 부팅 훅 결정
#   (예: /etc/rc.local 에 /usr/bin/msposd_air.sh 추가)

echo "== [4/4] majestic 재시작 =="
$SSH 'killall -1 majestic || true'

echo "완료. 다음: ./verify_camera.sh ${CAM_IP}"
echo "주의: WFB 채널(5.8GHz) 설정은 OpenIPC Configurator에서 별도 수행 (README §적용순서 1)"
