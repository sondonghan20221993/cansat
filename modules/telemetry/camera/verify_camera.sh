#!/usr/bin/env bash
# WiFiLink V2 설정 검증 — 적용 후 실행
# 사용법: ./verify_camera.sh [카메라IP] [Pi경유호스트(옵션)]
#   직결:    ./verify_camera.sh 192.168.1.10
#   Pi 경유: ./verify_camera.sh 192.168.1.10 sdh2983@192.168.50.65
# 주의: Pi 경유 시 ping/HTTP 스냅샷은 PC가 아니라 Pi에서 실행됨 (ssh -J는 TCP 세션만 프록시,
#       ICMP/HTTP는 직접 프록시 불가하므로 Pi를 통해 검사)
set -uo pipefail

CAM_IP="${1:-192.168.1.10}"
PROXY="${2:-}"
if [ -n "$PROXY" ]; then
    SSH="ssh -o StrictHostKeyChecking=no -J ${PROXY} root@${CAM_IP}"
    PING="ssh -o ConnectTimeout=5 ${PROXY} \"ping -c1 -W2 ${CAM_IP}\""
    CURL="ssh -o ConnectTimeout=5 ${PROXY} \"curl -fsS -o /tmp/cam_snapshot.jpg --max-time 5 http://${CAM_IP}/image.jpg\""
else
    SSH="ssh -o StrictHostKeyChecking=no root@${CAM_IP}"
    PING="ping -c1 -W2 ${CAM_IP}"
    CURL="curl -fsS -o /tmp/cam_snapshot.jpg --max-time 5 http://${CAM_IP}/image.jpg"
fi
PASS=0; FAIL=0
check() { if eval "$2" >/dev/null 2>&1; then echo "PASS: $1"; PASS=$((PASS+1)); else echo "FAIL: $1"; FAIL=$((FAIL+1)); fi }

check "ping"                "$PING"
check "ssh 접속"            "$SSH true"
check "majestic 프로세스"   "$SSH 'pidof majestic'"
check "osd.enabled=true"    "$SSH 'cli -g .osd.enabled' | grep -q true"
check "records.enabled"     "$SSH 'cli -g .records.enabled' | grep -q true"
check "HTTP 스냅샷"         "$CURL"
check "RTSP 응답(554)"      "timeout 3 bash -c 'exec 3<>/dev/tcp/${CAM_IP}/554'"
check "msposd 프로세스"     "$SSH 'pidof msposd'"          # FC 배선+기동 후에만 PASS
check "SD 마운트"           "$SSH 'mount | grep -q mmcblk'" # SD 삽입 후에만 PASS

echo "----------------------------------------"
echo "결과: PASS=${PASS} FAIL=${FAIL}"
echo "스냅샷 저장: /tmp/cam_snapshot.jpg (OSD 타임스탬프 번인 육안 확인)"
[ "$FAIL" -eq 0 ]
