# 3D 복원 파이프라인 전략 — 3갈래 구도

---

## 빠른 참조 — 가상환경 × 작업 매핑

> 이 프로젝트는 **conda 없이 순수 venv**로 운영됩니다. 갈래마다 별도 venv를 사용합니다.

| 갈래 | 가상환경 | 활성화 | 주요 작업 | repo 경로 |
|------|----------|--------|-----------|-----------|
| 공통 prior (MASt3R-SfM) | `venv-mast3r` | `source ~/Desktop/venv-mast3r/bin/activate` | MASt3R-SfM 포즈/점군 추정 (`run_mast3r_sfm.py`) | `~/Desktop/MAST3R_2` |
| ① 점맵→메시 | `venv-pointmap` | `source ~/Desktop/venv-pointmap/bin/activate` | MASt3R pointmap → Poisson/BPA 메싱 | `~/Desktop/MAST3R_2` |
| ② **MeshSplatting→메시** ⭐ | `venv-meshsplat10` (Python 3.10) | `source ~/Desktop/venv-meshsplat10/bin/activate` | **MeshSplatting 학습** (Opaque Triangle + Differentiable Rendering, **CVPR 2026 SOTA**) | `~/Desktop/mesh-splatting` |
| ③ (폐기) | ❌ | — | ~~Neuralangelo 학습~~ (취소 2026-06-17, 학습 시간 너무 김) | — |
| ⭐ MILo→메시 (선택) | `venv-milo` | `source ~/Desktop/venv-milo/bin/activate` | MILo 학습 (비교 baseline용, SIGGRAPH Asia 2025) | `~/Desktop/milo` |

> **2026-06-17 전략 변경:**
> - `venv-meshsplat10` (2026-06-19 재구성, Python 3.10): **MeshSplatting (CVPR 2026 SOTA)** 환경 완성. 기존 `venv-meshsplat`(Python 3.8)은 effrdel Python>=3.9 요구로 폐기.
> - SDF 갈래 (Neuralangelo) 완전 폐기: 학습 시간 36배 차이 (29h vs 48m), 성능도 MeshSplatting이 우수 (F1 0.728 > 0.70)
> - MILo는 선택적 비교 baseline으로 변경 (필수 → 선택사항)

> **2026-06-19 실험 결과 추가:**
> - MeshSplatting blue_1 FHD 실행 결과 **메시 파편화 (결과 불량)**. PSNR 21.4dB.
> - 원인: 단색 박스에서 Photometric Loss ≈ 0 → 기하 gradient 없음 (→ 사례 A 참조)
> - **GS 기반 메시 추출은 이 데이터셋(단색 object-centric)에 구조적으로 부적합**으로 결론.

### 각 venv 활성화 후 필수 환경변수 (매번 설정)

```bash
# 공통 (venv-gof, venv-sdf, venv-pointmap, venv-mast3r 모두)
export CUDA_HOME=/usr/local/cuda-12.3
export CUDACXX=/usr/local/cuda-12.3/bin/nvcc
export CPATH=/usr/local/cuda-12.3/include

# venv-sdf 전용 추가
export PYTHONPATH=/home/sdh/Desktop/neuralangelo:/home/sdh/Desktop/neuralangelo/imaginaire

# venv-mast3r 전용 추가
export PYTHONPATH=$PYTHONPATH:~/Desktop/MAST3R_2:~/Desktop/MAST3R_2/dust3r
```

### 공통 입력 데이터

**기존 데이터셋**
- 이미지: `~/Desktop/data/datasets/orbit34/rgb/` (68장, 1920×1080)
- MASt3R-SfM 포즈: `~/Desktop/data/experiments/orbit34__mast3r__2dgs/01_sfm/`
  - `poses.npy` (68, 4, 4), `focals.npy` (68,), `pointcloud.ply` (2,366,187 pts)
- COLMAP 변환본: `~/Desktop/data/experiments/orbit34__mast3r__2dgs/02_colmap/`
- `real_test` (33장): SfM/COLMAP 결과 `~/Desktop/data/experiments/real_test__mast3r/{01_sfm,02_colmap}` — Neuralangelo 학습에 사용 중 (`~/Desktop/data/experiments/real_test__mast3r__neuralangelo/`)

**신규 테스트 데이터셋 (2026-06-16, SYMA Z3 드론 촬영, Google Drive에서 다운로드)**
- 원본 zip/추출본: `~/Desktop/data/raw_videos/test1/extracted/` (각 객체별 `{이름}_frame_NNN.png` 형식으로 섞여 있음)
- 추출 방식: 영상에서 1fps 추출 (영상 길이 1분 초중반 → 장당 60~100장대)
- 객체별로 분리해 옮길 위치 (폴더 생성 완료, 이미지 이동은 진행 중):

| 객체 | 원본 장수 | 목표 폴더 | 비고 |
|---|---|---|---|
| `blue_1` | 125 | `~/Desktop/data/datasets/blue_1/rgb/` | 우선 진행 대상 |
| `blue_2` | 106 | `~/Desktop/data/datasets/blue_2/rgb/` | blue_1과 동일 객체 가능성, 중복 확인 필요 |
| `streetlight_low` | 75 | `~/Desktop/data/datasets/streetlight_low/rgb/` | |
| `blue_person` | 38 | `~/Desktop/data/datasets/blue_person/rgb/` | ⚠️ 목표(60~100장) 미달, 참고용으로 함께 진행 |

---

## 환경 구축 현황 (2026-06-16 기준)

### 서버 환경
| 항목 | 내용 |
|------|------|
| GPU | NVIDIA RTX A6000 (48GB) |
| CUDA | `/usr/local/cuda-12.3` (V12.3.107) |
| Python | 3.10 (venv 기반) |
| 방식 | conda 대신 **순수 venv** (CUDA_HOME=/usr/local/cuda-12.3 지정) |

### 공통 주의사항
- `numpy<2` 고정 필수 (torch 2.1.2가 numpy 1.x로 컴파일됨)
- `_lzma.so` 누락 → conda `recon3d` env에서 복사 필요 (모든 venv 공통)
- CUDA 확장 빌드 시 `--no-build-isolation` + `CUDA_HOME`/`CUDACXX`/`CPATH` 설정 필요
- CGAL/GMP: `sudo apt install libcgal-dev libgmp-dev libmpfr-dev` 완료 (시스템 전역)
- **`LD_LIBRARY_PATH` 트랩 (2026-06-17 발견):** 한 conda env(GLIBCXX 문제 해결용으로 export)에서 다른 venv로 전환해도 `LD_LIBRARY_PATH`는 shell에 그대로 남아있어, torch가 엉뚱한 CUDA 라이브러리를 링크해 `torch._C`에 일부 속성(`_OutOfMemoryError` 등)이 누락되는 기묘한 에러 발생. **환경 전환 시 반드시 `unset LD_LIBRARY_PATH`** 하거나 새 셸에서 시작할 것.
- `huggingface-hub[torch]`, `gradio` 등 일부 패키지가 설치 중 torch를 임의 최신 버전으로 강제 업그레이드함 → 항상 마지막에 `pip install torch==2.1.2 torchvision==0.16.2 --index-url https://download.pytorch.org/whl/cu121`로 재확인/재고정

### venv 구축 상태

| venv | 갈래 | 상태 | 비고 |
|------|------|------|------|
| `venv-mast3r` | 공통 prior (MASt3R-SfM) | ✅ **완료** | torch/numpy/lzma/mast3r/dust3r ALL OK 검증 완료 |
| `venv-meshsplat` | ② **MeshSplatting→메시** | 🔄 **구축 예정** (2026-06-17 신규 추가) | ⭐ **우선순위 1위**, CVPR 2026 SOTA, blue_1부터 진행 |
| `venv-pointmap` | ① 점맵→메시 | ✅ **완료** | ALL OK 검증 완료 |
| `venv-milo` | ⭐ MILo→메시 (선택) | ⏸️ **선택적** | 비교 baseline용 (SIGGRAPH Asia 2025) |
| `venv-sdf` | ③ (폐기) | ❌ **취소** (2026-06-17, 학습 시간 너무 김) | Neuralangelo 삭제 |
| `venv-gof` | (폐기) | ❌ **삭제** | 2026-06-17 MeshSplatting으로 대체 |

### venv-mast3r 설치 목록
- torch 2.1.2+cu121, torchvision 0.16.2+cu121
- numpy 1.26.4 (고정)
- roma, gradio, matplotlib, tqdm, opencv-python, scipy, einops, trimesh, tensorboard, pyglet<2, huggingface-hub, scikit-learn
- _lzma.so (conda recon3d에서 복사)
- repo: `~/Desktop/MAST3R_2` (PYTHONPATH로 등록: `~/Desktop/MAST3R_2:~/Desktop/MAST3R_2/dust3r`)
- 기존 conda `recon3d`는 torch/numpy/torchvision 파일 손상으로 복구 단념, 그대로 보존(미사용)

### venv-meshsplat10 설치 목록 **[2026-06-19 재구성, Python 3.10]**
- **Python 3.10** 필수 (effrdel이 Python>=3.9 요구, 기존 venv-meshsplat Python 3.8 폐기)
- torch 2.1.2+cu121, torchvision 0.16.2
- numpy 1.26.4 고정 (`<2` 필수, torch 2.1.2가 numpy 1.x로 컴파일됨)
- CUDA 확장 (`--no-build-isolation`, `unset LD_LIBRARY_PATH` 후):
  - `diff-triangle-mesh-rasterization` (submodules/)
  - `simple-knn` (submodules/)
  - `effrdel` (submodules/) — `pip install .`
- ⚠️ effrdel의 `tetutils.py`, `intersect.py`는 `find_packages`에서 누락됨 → site-packages에 수동 복사 필요:
  ```bash
  cp submodules/effrdel/src/tetutils.py ~/Desktop/venv-meshsplat10/lib/python3.10/site-packages/
  cp submodules/effrdel/src/intersect.py ~/Desktop/venv-meshsplat10/lib/python3.10/site-packages/
  ```
- requirements.txt: lpips, plyfile, mediapy, opencv-python, matplotlib, tqdm, trimesh, mmengine, timm, pybind11, open3d, scipy
- repo: `~/Desktop/mesh-splatting`

**실행 커맨드 (blue_1 FHD 기준)**
```bash
source ~/Desktop/venv-meshsplat10/bin/activate
unset LD_LIBRARY_PATH
export CUDA_HOME=/usr/local/cuda-12.3
cd ~/Desktop/mesh-splatting

# 학습 (~18분, 30000 iter)
python3 train.py \
    -s ~/Desktop/data/experiments/blue_1__mast3r_fhd/02_colmap \
    -m ~/Desktop/data/experiments/blue_1__meshsplat_fhd \
    --iterations 30000

# 메시 추출 (~3분, TSDF fusion)
python3 mesh.py \
    -s ~/Desktop/data/experiments/blue_1__mast3r_fhd/02_colmap \
    -m ~/Desktop/data/experiments/blue_1__meshsplat_fhd \
    --iteration 30000
```
출력: `experiments/blue_1__meshsplat_fhd/train/ours_30000/fuse_post.ply`

### venv-milo 설치 목록 **[완료, 2026-06-18]**
- **Python 3.10** 필수 (tetra_triangulation .so가 cp310으로 빌드됨, Python 3.8 불가)
- torch 2.1.2+cu121, torchvision 0.16.2
- numpy 1.24.4 고정 (open3d 등이 numpy 2.x로 올리려 함 → 설치 후 재고정 필요)
- CUDA 확장 (모두 `--no-build-isolation`):
  - `diff-gaussian-rasterization`
  - `diff-gaussian-rasterization_ms`
  - `diff-gaussian-rasterization_gof`
  - `fused-ssim`
  - `simple-knn`
  - `nvdiffrast` (submodule 비어있음 → `git clone https://github.com/NVlabs/nvdiffrast.git`으로 직접 클론)
  - `tetra_triangulation` (setup.py만으로는 .so 미설치 → PYTHONPATH 우회 필요)
- _lzma.so: `/home/sdh/miniforge3/envs/recon3d/lib/python3.10/lib-dynload/` → `/usr/local/lib/python3.10/lib-dynload/` (sudo 필요)
- tetranerf 경로 우회: train.py, mesh_extract_sdf.py 등에 sys.path 패치 적용 (2026-06-18)
- repo: `~/Desktop/milo` (MILo 공식 구현, Anttwo)

**MILo 실행 (blue_1 FHD 기준)**
```bash
source ~/Desktop/venv-milo/bin/activate
unset LD_LIBRARY_PATH
export CUDA_HOME=/usr/local/cuda-12.3
cd ~/Desktop/milo/milo

# 학습
python3 train.py -s ~/Desktop/data/experiments/blue_1__mast3r_fhd/02_colmap \
    -m ~/Desktop/data/experiments/blue_1__milo_fhd \
    --imp_metric outdoor --rasterizer radegs

# 메시 추출
python3 mesh_extract_sdf.py -s ~/Desktop/data/experiments/blue_1__mast3r_fhd/02_colmap \
    -m ~/Desktop/data/experiments/blue_1__milo_fhd --rasterizer radegs
```
출력: `~/Desktop/data/experiments/blue_1__milo_fhd/mesh_learnable_sdf.ply`

### venv-sdf 설치 목록
- torch 2.1.2+cu121, torchvision 0.16.2
- numpy 1.26.4 (고정)
- tinycudann 2.0 (CUDA 빌드, --no-build-isolation)
- addict, gdown, kornia, lpips, open3d, opencv-python-headless, PyMCubes, scikit-image, scipy, trimesh, wandb, tensorboard 등
- imaginaire: PYTHONPATH 등록 방식 (`pip install -e` 불가, setup.py 없음)
- _lzma.so (conda recon3d에서 복사)
- repo: `~/Desktop/neuralangelo`
- PYTHONPATH: `/home/sdh/Desktop/neuralangelo:/home/sdh/Desktop/neuralangelo/imaginaire`

### venv 활성화 시 필수 환경변수 (매번 설정)
```bash
export CUDA_HOME=/usr/local/cuda-12.3
export CUDACXX=/usr/local/cuda-12.3/bin/nvcc
export CPATH=/usr/local/cuda-12.3/include
# venv-sdf 전용 추가
export PYTHONPATH=/home/sdh/Desktop/neuralangelo:/home/sdh/Desktop/neuralangelo/imaginaire
```

### venv-pointmap 설치 목록
- torch 2.1.2+cu121, torchvision 0.16.2
- numpy 1.26.4 (고정, opencv/plyfile 버전 충돌 경고 있으나 실제 동작 확인)
- open3d 0.19.0, trimesh, plyfile, scipy, scikit-learn, opencv-python, matplotlib, tqdm
- _lzma.so (conda recon3d에서 복사)

---

> 목표: **object-centric(단일 물체)**, 입력은 **원형 2바퀴 캡처**, 최종 용도는 **렌더링/시각화**, 우선순위는 **복원 품질(표면 정확도) 최우선**(경량성 제외)

---

## -1. 이미지 전처리 — 업스케일 (2026-06-18 추가)

> **배경:** blue_1 실측 이미지가 720p로 촬영됨. 시뮬레이션(FHD)에서는 동일 객체 복원 성공 → 해상도가 주요 병목으로 확인.

| 원인 | 설명 |
|---|---|
| 해상도 낮음 (720p) | MASt3R 특징점 수 감소 → 포즈 오차 증가 → 점군 노이즈 |
| 실제 조명 변화 | specular 반사가 뷰마다 달라 GS photometric loss 혼란 |
| 배경 노이즈 | 실측 배경이 시뮬레이션보다 복잡 |

**해결책: Real-ESRGAN으로 720p → FHD 업스케일 후 전체 파이프라인 재시도**

```bash
# 설치 (최초 1회)
cd ~/Desktop
git clone https://github.com/xinntao/Real-ESRGAN.git
cd Real-ESRGAN
python3 -m venv venv-esrgan
source venv-esrgan/bin/activate
pip install basicsr facexlib gfpgan
pip install -r requirements.txt
python3 setup.py develop
wget https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.1/RealESRGAN_x2plus.pth -P weights/

# 업스케일 실행
python3 inference_realesrgan.py -n RealESRGAN_x2plus \
    -i ~/Desktop/data/datasets/blue_1/rgb/ \
    -o ~/Desktop/data/datasets/blue_1/rgb_fhd/ \
    --outscale 1.5
```

> **주의:** AI 업스케일은 뷰마다 일관성 있어야 함. Real-ESRGAN은 실사 이미지 학습 기반으로 프레임 간 일관성 양호. SD 업스케일러(디테일 창작)는 멀티뷰 일관성 깨질 수 있어 **비추천**.

---

## 0. 공통 전제 — 포즈 인프라는 하나로 공유

세 갈래 모두 **MASt3R-SfM 포즈**를 입력으로 공유한다. 포즈 파이프라인은 한 번만 구축한다.

- MASt3R-SfM은 자유 촬영 / 소~중규모 뷰에서 COLMAP보다 안정적으로 포즈를 잡음 → 현재 셋업(원형 2바퀴, object-centric)에 적합
- 점맵(pointmap)은 **메시의 최종 산출 경로가 아니라 포즈·초기 기하(앞단) 역할**로 쓸 때 가장 강함
- SDF 축은 COLMAP 포맷 변환 + **단위 구(unit sphere) 정규화**가 추가로 필요 (SDF는 정규화에 민감)

핵심 원칙: **"점맵을 메시의 끝단이 아니라 시작단으로 쓴다."**

---

## 갈래 ①: 점맵(pointmap) → 메시

| 항목 | 내용 |
|---|---|
| 표현 | pointmap → point cloud → mesh |
| 역할 | **빠른 baseline / 비교군** |
| 메싱 방법 | Screened Poisson (법선 필요, watertight, 안정적) / BPA (디테일 보존, 점 깨끗할 때) / 학습 기반(Shape As Points, POCO) — 노이즈·구멍 많을 때 |
| 강점 | feed-forward, 빠름, 포즈·depth·intrinsic 동시 추정 |
| 한계 | 이산화 + 뷰별 불일치 누적 → **표면 정확도 상한이 구조적으로 낮음** |

**결론:** raw 점맵을 그냥 메싱하는 경로는 품질 상한이 낮다. 빼지 말고 **비교 baseline으로 유지** (같은 포즈로 세 방법 비교 자체가 의미 있는 결과물).

---

## 갈래 ②: MeshSplatting (Differentiable Mesh Rendering) → 메시 **[2026-06-17 업데이트]**

| 항목 | 내용 |
|---|---|
| 표현 | **Opaque Triangle + Differentiable Rendering** → 최종 mesh |
| 역할 | **정밀 메시 + 실시간 렌더링, 새로운 SOTA (CVPR 2026)** |
| 선택 방법 | **MeshSplatting** (Held et al., CVPR 2026 Oral) |
| 강점 | 기하 직접 최적화, MILo 대비 +5.8% F1, 2.2배 빠른 학습, 2.5배 메모리 절감, 220 FPS 실시간 렌더링 |
| 한계 | 최신 논문으로 생태계 성숙도 아직 미흡 |

### 성능 근거 (CVPR 2026 SOTA)

**Mip-NeRF360 & Tanks & Temples 정량 비교 (모두 mesh-based)**

| 방법 | PSNR ↑ | Chamfer ↓ | F1 ↑ | 메시 크기 | 학습 시간 | FPS (HD) | 메모리 |
|------|--------|---------|------|---------|---------|---------|--------|
| GOF | 20.78 | 0.465 | 0.573 | 33M | **74m** | ❌ OOM | 1.5GB |
| RaDe-GS | 23.56 | 0.361 | 0.668 | 31M | 84m | ❌ OOM | 1.1GB |
| **MILo** | 24.09 | 0.323 | 0.688 | 7M | 106m | 170 | 253MB |
| **MeshSplatting** | **24.78** ✅ | **0.310** ✅ | **0.728** ✅ | **3M** ✅ | **48m** ✅ | **220** ✅ | **100MB** ✅ |

**개선도 (MILo 대비)**
- PSNR: +0.69 dB (+2.9%)
- Chamfer Distance: -4.0% (더 정확)
- **F1 Score: +5.8%** ⭐ (가장 중요한 지표)
- 메시 크기: -57% (더 경량)
- **학습 시간: 2.2배 빠름** (106m → 48m)
- **렌더링: 1.3배 빠름** (170→220 FPS)
- **메모리: 2.5배 절감** (253MB → 100MB)

→ **MeshSplatting은 정확도 + 효율성 + 실시간 성능을 모두 달성한 새로운 SOTA.**

### 갈래② 확정 파이프라인 — MeshSplatting 중심 메시 재구축

> 결정(2026-06-17 재심의): 갈래②는 **MeshSplatting** (CVPR 2026 새 방법)으로 전환. MILo는 비교 baseline용 선택적 실행.

**MeshSplatting 핵심 방법론**
- **입력:** COLMAP 포즈 + 이미지
- **핵심 기술:** Opaque Triangle을 end-to-end 미분 가능하게 렌더링
  - 제한된 Delaunay triangulation → 삼각형 한 번만 생성
  - 각 삼각형을 불투명 Opaque object로 처리 (Splat 기반 아님)
  - Gradient: 3D 좌표, color, albedo, normal 등에 대해 완전 미분 가능
- **메시 추출:** 학습 중 자동 생성 + 최종 `mesh_optimized.ply`

**MASt3R prior 통합 (선택사항)**
- MASt3R 점군 초기화 + depth/normal supervision 가능 (MILo처럼)
- 다만 MeshSplatting 공식 구현은 이미 SOTA 성능이므로, 먼저 baseline으로 실행 후 평가

#### 실행 순서
1. **blue_1로 MeshSplatting baseline** 실행 (예상 ~48분 학습)
2. 메시 정량 평가 (Chamfer, F1, PSNR)
3. (선택) MILo 또는 다른 방법과 시각적 비교
4. 나머지 객체 (blue_2, streetlight_low 등) 순차 진행

#### 참고: MILo와의 보완 관계
- **MeshSplatting:** 빠르고 가벼운 고정밀 메시 (프로덕션 용도)
- **MILo:** GS-메시 양방향 일관성 강조 (연구 논문용)

---

## 갈래 ③: SDF (Neuralangelo) — **폐기 (2026-06-17)** ❌

> **결정:** SDF 갈래는 취소. 이유: Neuralangelo 학습 시간이 너무 김 (~29시간 real_test 33장 기준). MeshSplatting (48분)이 정확도도 우수하고 빠르므로 우선순위 변경.

| 항목 | 상태 |
|---|---|
| 갈래③ 완전 폐기 | ❌ 취소 (2026-06-17) |
| 진행 중인 실험 | Terminal A (real_test 학습 중) 중단 |
| venv-sdf | 선택사항 (나중에 필요시만 유지) |

### 참고 (폐기 사유)
- **학습 시간:** ~29시간 (real_test 33장) vs MeshSplatting ~48분 **→ 36배 차이**
- **성능:** Neuralangelo F1 0.70 vs MeshSplatting F1 0.728 **→ MeshSplatting이 더 우수**
- **결론:** MeshSplatting으로 충분. SDF는 제외.

> 결정(2026-06-16): SDF 갈래는 두 역할을 분리한다. 한 갈래에 뭉치면 꼬인다.

- **역할 A — 정확도 상한 기준점 (vanilla):** prior 없이 순수 Neuralangelo. 가우시안이 못 따라오는 표면 정확도의 천장 레퍼런스. *현재 서버에서 학습 진행 중(Epoch 로그 확인됨)*. 끝까지 완주해 기준점 확보.
- **역할 B — MASt3R prior 통합 (기여):**
  - 갈래②와 **동일한 prior 자산(점군 + depth/normal/confidence 맵)** 을 SDF에 공급
  - 점군 → 단위 구 정규화 기준 + SDF 초기화
  - 학습 중 depth/normal supervision(confidence 가중)

#### 베이스 선택 (중요)
- Neuralangelo는 prior 손실이 **기본 미탑재**(순수 RGB + hash grid + numerical gradient). prior 이식이 필요.
- **순서:** prior 내장된 **MonoSDF**에서 Omnidata depth/normal 자리에 MASt3R 맵을 먼저 끼워 효과 검증 → 먹히면 그 손실만 **Neuralangelo로 이식**해 최종 천장. (검증 전 Neuralangelo 직접 이식은 엔지니어링 늪)

#### 참고 SOTA (출처)
- **MonoSDF**: depth+normal prior로 SDF supervision (Omnidata) — 기준점
- **SuperNormal**: normal 중심, 적은 뷰에서 고정밀
- DUSt3R/VGGT를 SDF 기하 prior·init으로 쓰는 2025~ 흐름
- 주의: "MASt3R prior + SDF"의 정립 벤치마크는 아직 없음 → 기여 포인트이자 실증 필요

---

## 갈래 역할 분담 요약 — 실질 비교 항목은 4개

> 갈래②는 같은 MASt3R prior를 공유하는 베이스라인(2-1)과 기여(2-2)로 쪼개져 있어, 실제 비교표는 ①②③ 3갈래가 아니라 **1, 2-1, 2-2, 3의 4항목**이 된다. (갈래③의 역할B 〈MASt3R prior 통합〉은 MonoSDF 검증이 선행돼야 해서 아직 별도 future task — 지금은 갈래③=vanilla 1개로만 카운트)

| # | 항목 | 표현 | prior | 역할 |
|---|------|------|-------|------|
| **1** | 점맵 메싱 | pointmap→mesh (Poisson/BPA) | MASt3R 점군 직접 메싱 | baseline / 비교군 |
| **2-1** | MILo 베이스라인 | GS→mesh (MILo, 매 iter 추출) | COLMAP sparse init, prior 없음 | 순수 SOTA 성능 기준 |
| **2-2** | MILo + MASt3R | GS→mesh (MILo, 매 iter 추출) | MASt3R dense init + depth/normal/confidence supervision | 핵심 기여 |
| **3** | SDF (vanilla) | SDF→mesh (Neuralangelo) | 없음 | 정밀 표면 상한 기준점 |

**한 줄 요약:** 1은 *포즈·초기화 앞단*, 2-1→2-2는 *MASt3R prior가 가우시안 표면에 주는 순수 효과*, 3은 *정밀 표면 상한*. 1과 2-1/2-2는 같은 MASt3R-SfM 포즈+점군을 공유하므로 "raw 메싱 vs 가우시안 최적화"와 "prior 유무에 따른 가우시안 품질 차"를 동시에 분리해서 볼 수 있다.

---

## 다음 액션 아이템

### 갈래① — 점맵 Screened Poisson
- [x] ~~blue_1 720p~~ → ❌ 울퉁불퉁
- [x] blue_1 FHD → ✅ 박스 형태 식별 가능 (`mesh_poisson_fhd.ply`)
- [ ] blue_2, streetlight_low FHD 업스케일 + Poisson

### 갈래② — MeshSplatting
- [x] venv-meshsplat10 (Python 3.10) 환경 구축 ✅
- [x] blue_1 FHD 학습 + 메시 추출 → ❌ **파편화 (Photometric Loss Failure)**
- [ ] 다른 데이터셋 시도는 보류 (근본 원인 미해결)

### MILo (선택적 baseline)
- [x] venv-milo 환경 구축 ✅
- [x] blue_1 FHD 학습 + 메시 추출 ✅ (`blue_1_fhd_milo.ply` 18MB)
- [ ] 육안 품질 확인 (Screened Poisson과 비교)
- [ ] blue_2, streetlight_low FHD 진행 여부 판단 후

### 갈래③ — SDF (폐기)
- [x] Neuralangelo 폐기 완료 ❌ (학습 시간 36배)

### 비교
- [ ] blue_1 FHD: MILo vs Screened Poisson 육안 비교
- [ ] 2DGS 시도 검토 (표면 정렬 특화, GS 계열 중 유일하게 미시도)

---

## 검증 필요 / 미확정 항목

- **MeshSplatting**의 정확한 T&T F1 / DTU Chamfer 수치는 본문 미확보 → self-report(Mip-NeRF360에서 MILo 대비 PSNR +0.69dB, 학습 2배 빠름, 메모리 2배 절약)만 확인됨
- 물체 표면이 **광택·반사 재질**이면 SDF·가우시안 공통으로 specular 영역 표면 붕괴 → Ref-NeuS 등 반사 처리 변형 별도 검토 필요
- 위 벤치마크 수치는 모두 COLMAP 포즈 기준 → MASt3R-SfM 포즈 사용 시 실제 이득은 직접 비교 실험으로 확인 권장
