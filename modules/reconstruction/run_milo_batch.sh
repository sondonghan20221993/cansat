#!/bin/bash
# MILo × 4 강도 순차 실행 (tmux)

BASE="/home/sdh/Desktop"
VENV="${BASE}/venv-milo"

# 4개 작업 정의
JOBS=(
    "blue_1__milo_fhd_강도2_엄청빡셈_vs0003"
    "blue_1__milo_fhd_강도1_엄청빡셈_vs0005"
    "blue_1__milo_fhd_강도1-2_일부압축_vs0008"
    "blue_1__milo_fhd_강도1-1_일부압축_vs0010"
)

SESSION="milo_batch"

# tmux 세션 생성
tmux new-session -d -s $SESSION -c $BASE

echo "=========================================="
echo "MILo × 4 강도 순차 실행 시작"
echo "=========================================="
echo "Session: $SESSION"
echo ""

for i in "${!JOBS[@]}"; do
    JOB="${JOBS[$i]}"
    EXP_DIR="${BASE}/data/experiments/${JOB}"
    JOB_NUM=$((i + 1))

    echo "[${JOB_NUM}/4] $JOB"
    echo "  경로: $EXP_DIR"

    TRAIN_CMD="source ${VENV}/bin/activate && unset LD_LIBRARY_PATH && export CUDA_HOME=/usr/local/cuda-12.3 && cd ${BASE}/milo/milo && python3 train.py -s ${EXP_DIR}/02_colmap_dense -m ${EXP_DIR} --imp_metric outdoor --rasterizer radegs"

    MESH_CMD="source ${VENV}/bin/activate && unset LD_LIBRARY_PATH && export CUDA_HOME=/usr/local/cuda-12.3 && cd ${BASE}/milo/milo && python3 mesh_extract_sdf.py -s ${EXP_DIR}/02_colmap_dense -m ${EXP_DIR} --rasterizer radegs"

    # 첫 번째: 새 윈도우로, 이후: append 명령
    if [ $i -eq 0 ]; then
        tmux send-keys -t $SESSION "echo '[$JOB_NUM/4] TRAIN: $JOB 시작' && $TRAIN_CMD && echo '[$JOB_NUM/4] MESH: $JOB 시작' && $MESH_CMD && echo '[$JOB_NUM/4] 완료'" C-m
    else
        tmux send-keys -t $SESSION "echo '' && echo '[$JOB_NUM/4] TRAIN: $JOB 시작' && $TRAIN_CMD && echo '[$JOB_NUM/4] MESH: $JOB 시작' && $MESH_CMD && echo '[$JOB_NUM/4] 완료'" C-m
    fi

    echo ""
done

# 모든 작업 완료 후 세션 자동 종료 (좀비 세션 방지)
tmux send-keys -t $SESSION "echo '[전체 완료] tmux 세션 자동 종료' && tmux kill-session -t $SESSION" C-m

echo "=========================================="
echo "모든 작업이 tmux '$SESSION'에서 순차 실행 중"
echo ""
echo "모니터링 명령:"
echo "  tmux attach-session -t $SESSION"
echo "  또는"
echo "  tmux capture-pane -t $SESSION -p"
echo "=========================================="
