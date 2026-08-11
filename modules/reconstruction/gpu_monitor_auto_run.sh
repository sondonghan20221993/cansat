#!/bin/bash
# 2DGS 설정 완료 대기 → GPU VRAM 여유 확인 → MILo/2DGS 병렬/순차 자동 결정

set -e

BASE="$HOME/Desktop"
VENV_2DGS="${BASE}/venv-2dgs"
LOG_2DGS="${BASE}/2dgs_setup.log"
TMUX_SESSION_MILO="milo_batch"

# GPU VRAM 설정 (RTX A6000: 48GB)
TOTAL_VRAM_GB=48
REQUIRED_VRAM_MARGIN_PERCENT=20
REQUIRED_VRAM_GB=$(echo "$TOTAL_VRAM_GB * (1 - $REQUIRED_VRAM_MARGIN_PERCENT / 100)" | bc)

echo "=========================================="
echo "GPU VRAM 자동 감시 및 병렬/순차 결정"
echo "=========================================="
echo "총 VRAM: ${TOTAL_VRAM_GB}GB"
echo "권장 사용량 (20% 마진): ${REQUIRED_VRAM_GB}GB"
echo ""

# Step 1: 2DGS 설정 완료 대기
echo "[*] 2DGS 설정 완료 대기 중..."
while ! grep -q "2DGS 환경 설정 완료" "$LOG_2DGS" 2>/dev/null; do
    if grep -q "error\|Error\|failed\|Failed" "$LOG_2DGS" 2>/dev/null; then
        echo "[!] 2DGS 설정 실패. 로그 확인:"
        tail -20 "$LOG_2DGS"
        exit 1
    fi
    sleep 30
    echo "  $(date '+%H:%M:%S') - 계속 대기 중..."
done

echo "[✓] 2DGS 설정 완료! ($(date))"
echo ""

# Step 2: GPU VRAM 여유 확인
echo "[*] GPU VRAM 여유 확인 중..."
AVAILABLE_VRAM=$(nvidia-smi --query-gpu=memory.free --format=csv,noheader,nounits | head -1)
AVAILABLE_VRAM_GB=$(echo "scale=2; $AVAILABLE_VRAM / 1024" | bc)

echo "  현재 가용 VRAM: ${AVAILABLE_VRAM_GB}GB"
echo "  권장 사용량: ${REQUIRED_VRAM_GB}GB"

# Step 3: 병렬/순차 결정
if (( $(echo "$AVAILABLE_VRAM_GB >= $REQUIRED_VRAM_GB" | bc -l) )); then
    echo ""
    echo "[✓] VRAM 여유 충분 → MILo와 2DGS 병렬 실행"
    echo ""

    # MILo tmux 세션이 아직 실행 중인지 확인
    if tmux list-sessions | grep -q "^$TMUX_SESSION_MILO"; then
        echo "[*] MILo tmux 세션 활성 (계속 실행)"
    else
        echo "[!] MILo tmux 세션 없음"
        exit 1
    fi

    # 2DGS 병렬 실행 (새 tmux 윈도우)
    echo "[*] 2DGS 실행 시작..."
    tmux new-window -t $TMUX_SESSION_MILO -n 2dgs -c "$BASE/2d-gaussian-splatting" \
        -c "$BASE" \
        "source ${VENV_2DGS}/bin/activate && unset LD_LIBRARY_PATH && export CUDA_HOME=/usr/local/cuda-12.3 && \
         python3 train.py -s ${BASE}/data/experiments/blue_1__mast3r_fhd/02_colmap -m ${BASE}/data/experiments/blue_1__2dgs_fhd"

    echo "[✓] 2DGS 병렬 실행 시작됨 (tmux ${TMUX_SESSION_MILO}:2dgs)"

else
    echo ""
    echo "[!] VRAM 여유 부족 (필요: ${REQUIRED_VRAM_GB}GB, 가용: ${AVAILABLE_VRAM_GB}GB)"
    echo "[*] MILo 완료 후 2DGS 순차 실행"
    echo ""

    # MILo 완료 대기
    echo "[*] MILo 완료 대기 중..."
    MILO_START=$(date +%s)
    while tmux list-sessions | grep -q "^$TMUX_SESSION_MILO"; do
        # 간단히: tmux 세션이 남아있으면 실행 중
        ELAPSED=$(($(date +%s) - MILO_START))
        HOURS=$((ELAPSED / 3600))
        MINS=$(((ELAPSED % 3600) / 60))
        echo "  경과: ${HOURS}시간 ${MINS}분"
        sleep 300  # 5분마다 확인
    done

    echo "[✓] MILo 완료! ($(date))"
    echo ""

    # 2DGS 순차 실행
    echo "[*] 2DGS 순차 실행 시작..."
    tmux new-session -d -s 2dgs_seq -c "$BASE/2d-gaussian-splatting" \
        "source ${VENV_2DGS}/bin/activate && unset LD_LIBRARY_PATH && export CUDA_HOME=/usr/local/cuda-12.3 && \
         python3 train.py -s ${BASE}/data/experiments/blue_1__mast3r_fhd/02_colmap -m ${BASE}/data/experiments/blue_1__2dgs_fhd && \
         echo '[✓] 2DGS 완료' >> ${BASE}/2dgs_seq.log"

    echo "[✓] 2DGS 순차 실행 시작됨 (tmux 2dgs_seq)"
fi

echo ""
echo "=========================================="
echo "[✓] 모니터링 완료. 후속 작업 시작됨"
echo "=========================================="
