#!/bin/bash
# 2DGS 환경 설정 (Python 3.8 + torch 2.1.2 + CUDA 서브모듈)

set -e

BASE="$HOME/Desktop"
VENV_DIR="${BASE}/venv-2dgs"
REPO_DIR="${BASE}/2d-gaussian-splatting"

echo "=========================================="
echo "2DGS Environment Setup"
echo "=========================================="
echo ""

# Step 1: venv 생성
if [ -d "$VENV_DIR" ]; then
    echo "[!] venv 이미 존재: $VENV_DIR"
    echo "    삭제 후 재구성..."
    rm -rf "$VENV_DIR"
fi

echo "[1/5] venv 생성 (Python 3.8)..."
python3.8 -m venv "$VENV_DIR"
source "${VENV_DIR}/bin/activate"

# Step 2: pip 업그레이드
echo "[2/5] pip 업그레이드..."
pip install --upgrade pip wheel setuptools

# Step 3: PyTorch + 기본 패키지
echo "[3/5] PyTorch 2.1.2 + CUDA 12.1 설치..."
pip install torch==2.1.2 torchvision==0.16.2 torchaudio==2.1.2 --index-url https://download.pytorch.org/whl/cu121

# Step 4: pip 패키지 (2DGS requirements)
echo "[4/5] 추가 패키지 설치..."
pip install open3d==0.18.0 mediapy==1.1.2 lpips==0.1.4 scikit-image==0.21.0 tqdm==4.66.2 trimesh==4.3.2 plyfile opencv-python

# numpy 고정 (torch 호환성)
echo "     numpy 고정..."
pip install 'numpy<2'

# Step 5: CUDA 서브모듈 빌드
echo "[5/5] CUDA 서브모듈 빌드..."
cd "$REPO_DIR"

# 환경변수 설정
export CUDA_HOME=/usr/local/cuda-12.3
export CUDACXX=/usr/local/cuda-12.3/bin/nvcc
export CPATH=/usr/local/cuda-12.3/include
unset LD_LIBRARY_PATH

echo "     Building diff-surfel-rasterization..."
cd submodules/diff-surfel-rasterization
pip install . --no-build-isolation 2>&1 | tail -5
cd ../../

echo "     Building simple-knn..."
cd submodules/simple-knn
pip install . --no-build-isolation 2>&1 | tail -5
cd ../../

echo ""
echo "=========================================="
echo "[✓] 2DGS 환경 설정 완료"
echo "=========================================="
echo ""
echo "활성화:"
echo "  source ${VENV_DIR}/bin/activate"
echo ""
echo "실행 (예):"
echo "  source ${VENV_DIR}/bin/activate"
echo "  unset LD_LIBRARY_PATH"
echo "  export CUDA_HOME=/usr/local/cuda-12.3"
echo "  cd ${REPO_DIR}"
echo "  python3 train.py -s <COLMAP_DIR> -m <OUTPUT_DIR>"
echo ""
