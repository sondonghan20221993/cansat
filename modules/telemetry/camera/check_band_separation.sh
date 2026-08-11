#!/usr/bin/env bash
# Phase 1 목표: WFB-ng 영상 링크 채널이 LoRa(2.4GHz)와 비중첩(5.8GHz대)인지 확인
# 사용법: ./check_band_separation.sh [카메라IP] [Pi경유호스트(옵션)]
#   직결:     ./check_band_separation.sh 192.168.1.10
#   Pi 경유:  ./check_band_separation.sh 192.168.1.10 sdh2983@192.168.50.65
set -uo pipefail

CAM_IP="${1:-192.168.1.10}"
PROXY="${2:-}"

if [ -n "$PROXY" ]; then
    SSH="ssh -o StrictHostKeyChecking=no -J ${PROXY} root@${CAM_IP}"
else
    SSH="ssh -o StrictHostKeyChecking=no root@${CAM_IP}"
fi

echo "== WFB-ng 채널 설정 조회 =="
# 펌웨어 버전에 따라 wfb.conf 또는 wfb.yaml — 둘 다 시도
CHANNEL=$($SSH 'grep -E "^channel" /etc/wfb.conf 2>/dev/null || grep -E "channel:" /etc/wfb.yaml 2>/dev/null')

if [ -z "$CHANNEL" ]; then
    echo "FAIL: 채널 설정 파일을 찾지 못함 (/etc/wfb.conf, /etc/wfb.yaml 둘 다 없음)"
    exit 1
fi

echo "raw: $CHANNEL"
CH_NUM=$(echo "$CHANNEL" | grep -oE '[0-9]+' | head -1)

if [ -z "$CH_NUM" ]; then
    echo "FAIL: 채널 번호 파싱 실패"
    exit 1
fi

# 2.4GHz 채널: 1~14 근방 / 5.8GHz 채널: 36 이상 (802.11 채널 넘버링 기준, 대략치)
if [ "$CH_NUM" -lt 36 ]; then
    echo "FAIL: 채널 ${CH_NUM} → 2.4GHz 대역으로 추정, LoRa(2.4GHz)와 충돌 가능"
    exit 1
else
    echo "PASS: 채널 ${CH_NUM} → 5.8GHz 대역, LoRa(2.4GHz)와 비중첩"
    exit 0
fi
