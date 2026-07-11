#!/usr/bin/env bash
# WiFiLink V2 설정 검증 — 적용 후 벤치에서 실행
# 사용법: ./verify_camera.sh [카메라IP]
set -uo pipefail

CAM_IP="${1:-192.168.1.10}"
SSH="ssh -o StrictHostKeyChecking=no root@${CAM_IP}"
PASS=0; FAIL=0
check() { if eval "$2" >/dev/null 2>&1; then echo "PASS: $1"; PASS=$((PASS+1)); else echo "FAIL: $1"; FAIL=$((FAIL+1)); fi }

check "ping"                "ping -c1 -W2 ${CAM_IP}"
check "ssh 접속"            "$SSH true"
check "majestic 프로세스"   "$SSH 'pidof majestic'"
check "osd.enabled=true"    "$SSH 'cli -g .osd.enabled' | grep -q true"
check "records.enabled"     "$SSH 'cli -g .records.enabled' | grep -q true"
check "HTTP 스냅샷"         "curl -fsS -o /tmp/cam_snapshot.jpg --max-time 5 http://${CAM_IP}/image.jpg"
check "RTSP 응답(554)"      "timeout 3 bash -c 'exec 3<>/dev/tcp/${CAM_IP}/554'"
check "msposd 프로세스"     "$SSH 'pidof msposd'"          # FC 배선+기동 후에만 PASS
check "SD 마운트"           "$SSH 'mount | grep -q mmcblk'" # SD 삽입 후에만 PASS

echo "----------------------------------------"
echo "결과: PASS=${PASS} FAIL=${FAIL}"
echo "스냅샷 저장: /tmp/cam_snapshot.jpg (OSD 타임스탬프 번인 육안 확인)"
[ "$FAIL" -eq 0 ]
