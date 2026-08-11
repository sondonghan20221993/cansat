#!/usr/bin/env bash
# Phase 4 목표: 실제 녹화 파일이 SD카드에 생성되는지 확인 (설정 적용만으로는 보장 안 됨)
# 사용법: ./check_sd_recording.sh [카메라IP] [Pi경유호스트(옵션)] [최근허용분(기본10)]
set -uo pipefail

CAM_IP="${1:-192.168.1.10}"
PROXY="${2:-}"
RECENT_MIN="${3:-10}"

if [ -n "$PROXY" ]; then
    SSH="ssh -o StrictHostKeyChecking=no -J ${PROXY} root@${CAM_IP}"
else
    SSH="ssh -o StrictHostKeyChecking=no root@${CAM_IP}"
fi

RECORDS_PATH="/mnt/mmcblk0p1/records"  # TODO(bench): 실제 마운트 경로 확인 후 조정

echo "== SD 마운트 확인 =="
$SSH 'mount | grep mmcblk' || { echo "FAIL: SD 미마운트"; exit 1; }

echo "== 녹화 경로 최근 파일 =="
$SSH "find ${RECORDS_PATH} -type f -mmin -${RECENT_MIN} -printf '%TY-%Tm-%Td %TH:%TM  %10s  %p\n' 2>/dev/null" \
    || { echo "FAIL: ${RECORDS_PATH} 접근 실패 (경로 TODO(bench) 확인 필요)"; exit 1; }

COUNT=$($SSH "find ${RECORDS_PATH} -type f -mmin -${RECENT_MIN} 2>/dev/null | wc -l")
if [ "${COUNT:-0}" -gt 0 ]; then
    echo "PASS: 최근 ${RECENT_MIN}분 내 녹화 파일 ${COUNT}개 확인"
    exit 0
else
    echo "FAIL: 최근 ${RECENT_MIN}분 내 녹화 파일 없음"
    exit 1
fi
