#!/usr/bin/env bash
# Phase 2 목표: 카메라가 기체 장착 상태(PC 직결 불가)에서도 Pi 경유로 접근 가능한지 확인
# 전제: Pi(192.168.50.65) 이더넷 포트가 카메라(192.168.1.10)와 물려있고,
#       Pi에 192.168.1.x 대역 서브인터페이스가 설정되어 있음 (TODO(bench): 실제 배선/설정 확인)
# 사용법: ./check_ethernet_access.sh [Pi호스트] [카메라IP]
set -uo pipefail

PI_HOST="${1:-sdh2983@192.168.50.65}"
CAM_IP="${2:-192.168.1.10}"
PASS=0; FAIL=0
check() { if eval "$2" >/dev/null 2>&1; then echo "PASS: $1"; PASS=$((PASS+1)); else echo "FAIL: $1"; FAIL=$((FAIL+1)); fi }

check "Pi SSH 접속"                "ssh -o ConnectTimeout=5 ${PI_HOST} true"
check "Pi에 192.168.1.x 인터페이스" "ssh -o ConnectTimeout=5 ${PI_HOST} \"ip addr show | grep -q '192.168.1.'\""
check "Pi → 카메라 ping"           "ssh -o ConnectTimeout=5 ${PI_HOST} \"ping -c1 -W2 ${CAM_IP}\""
check "PC → Pi → 카메라 SSH (jump)" "ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 -J ${PI_HOST} root@${CAM_IP} true"

echo "----------------------------------------"
echo "결과: PASS=${PASS} FAIL=${FAIL}"
echo "PASS 시 이후 단계는: ./apply_camera_config.sh ${CAM_IP} ${PI_HOST}"
echo "                      ./verify_camera.sh ${CAM_IP} ${PI_HOST}"
[ "$FAIL" -eq 0 ]
