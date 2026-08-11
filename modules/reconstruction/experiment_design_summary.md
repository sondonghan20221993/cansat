# 3D Mesh 복원 실험 설계 요약

## 실험 목표

실외 건물 이미지 200~300장 → **시각적 품질 최우선 3D mesh 복원**

---

## 실험 구조

### 공통 후반 파이프라인 (A/B 동일)

```
카메라 pose + point cloud
  ↓
2DGS 학습
  ↓
mesh 추출 (2DGS 자체 or SuGaR)
  ↓
최종 mesh (텍스처 포함)
```

### 실험 A (메인) — hloc + COLMAP

```
이미지 200~300장
  ↓
hloc (SuperPoint+LightGlue) → COLMAP
  ↓
카메라 pose + sparse point cloud
  ↓
공통 후반 파이프라인
```

### 실험 B (비교) — MASt3R-SfM ✅ 진행 중

```
이미지 200~300장 (동일 이미지셋)
  ↓
MASt3R-SfM ✅ 완료 (2026-06-14)
  ↓
카메라 pose + dense point cloud ✅ 완료
  - poses.npy (68, 4, 4)
  - focals.npy (68,) — shared focal = 261.83px
  - pointcloud.ply (2,366,187 포인트)
  ↓
COLMAP 형식 변환 ✅ 완료
  - colmap_input/sparse/0/{cameras,images,points3D}.txt
  ↓
2DGS 학습 ✅ 완료 (30,000 iter)
  - 출력: ~/Desktop/data/experiment_B/2dgs_output/
  ↓
mesh 추출 ← 현재 단계
  - [ ] 방법 1: 2DGS 자체 extract_mesh.py
  - [ ] 방법 2: SuGaR (텍스처 포함)
  ↓
최종 mesh 시각적 검토
```

### 비교 변수

- **유일한 차이**: pose + point cloud 생성 방법만 다름
- **비교 기준**: 최종 mesh 시각적 품질 (텍스처, 노이즈, artifact)
- 수치적 정확도(Chamfer distance 등)는 평가 기준 아님

---

## 서버 환경

| 항목 | 내용 |
|------|------|
| GPU | NVIDIA RTX A6000 (VRAM 48GB) |
| Driver / CUDA | 545.23.08 / CUDA 12.3 |
| OS | Ubuntu (20.04 추정) |
| 사용자 | sdh |
| conda | `/home/sdh/miniforge3` (miniforge3 기준) |
| 가상환경 | `recon3d` (Python 3.10) |

---

## 설치된 패키지 및 경로

| 패키지 | 경로 | 비고 |
|--------|------|------|
| PyTorch | conda env recon3d | 2.1.0+cu118 |
| nvcc | conda env recon3d | 11.8 |
| COLMAP | conda env recon3d | 3.11.1, CUDA 지원 |
| hloc | `~/Desktop/Hierarchical-Localization` | pip -e 설치 |
| 2DGS | `~/Desktop/2d-gaussian-splatting` | setup.py로 빌드 |
| SuGaR | `~/Desktop/SuGaR` | nvdiffrast 포함 |
| MASt3R | `~/Desktop/MAST3R_2` | PYTHONPATH 등록 |
| MASt3R checkpoint | `~/Desktop/MAST3R_2/checkpoints/` | MASt3R_ViTLarge_BaseDecoder_512_catmlpdpt_metric.pth |

---

## 환경 변수 (.bashrc 등록됨)

```bash
# conda 활성화 (매번 필요)
source /home/sdh/miniforge3/etc/profile.d/conda.sh
conda activate recon3d

# MASt3R PYTHONPATH (.bashrc에 영구 등록됨)
export PYTHONPATH=$PYTHONPATH:~/Desktop/MAST3R_2:~/Desktop/MAST3R_2/dust3r
```

---

## 주의사항

| 항목 | 내용 |
|------|------|
| .bashrc 문법 오류 | 3번째 줄 오류로 `source ~/.bashrc` 실패 → 매 셸 시작 시 `source /home/sdh/miniforge3/etc/profile.d/conda.sh` 수동 실행 필요 |
| numpy 버전 | `<2`로 고정 (2DGS 빌드 시 다운그레이드) → opencv-python / plyfile 충돌 경고 있으나 무시 |
| MASt3R 설치 | setup.py/pyproject.toml 없는 구버전 → PYTHONPATH로 해결 |
| nano 붙여넣기 | 들여쓰기 자동 추가 버그 → heredoc 후 `sed -i 's/^  //' 파일명` 으로 해결 |

---

## 디렉토리 구조 (정리 후)

### 명명 규칙
```
experiments/{데이터셋}__{SfM방법}__{렌더러}/
```

### 전체 구조
```
~/Desktop/data/
├── datasets/
│   └── orbit34/                              # 원본 데이터셋
│       ├── rgb/                              # 이미지 68장 (1920x1080)
│       ├── meta/
│       └── manifest.json
└── experiments/
    └── orbit34__mast3r__2dgs/               # 실험 B
        ├── 01_sfm/                          # MASt3R-SfM 결과
        │   ├── poses.npy
        │   ├── focals.npy
        │   ├── pointcloud.ply
        │   └── cache/
        ├── 02_colmap/                       # COLMAP 변환 결과
        │   ├── sparse/0/
        │   │   ├── cameras.txt
        │   │   ├── images.txt
        │   │   └── points3D.txt
        │   └── images/                      # 심볼릭 링크
        ├── 03_gaussian/                     # 2DGS 학습 결과
        └── 04_mesh/                         # mesh 추출 결과
            ├── 2dgs_mesh.ply                # 2DGS TSDF mesh
            └── sugar_mesh/                  # SuGaR mesh (예정)

~/Desktop/MAST3R_2/
├── run_mast3r_sfm.py                        # MASt3R-SfM 실행 스크립트
└── mast3r_to_colmap.py                      # COLMAP 변환 스크립트
```

### 실험 추가 시 예시
```
experiments/
├── orbit34__mast3r__2dgs/                   # 실험 B (현재)
└── orbit34__hloc_colmap__2dgs/              # 실험 A (예정)
```

---

## 다음 단계

```
Step 1 (완료) MASt3R-SfM → pose + point cloud
Step 2 (완료) COLMAP 형식 변환
Step 3 (완료) 2DGS 학습 (30,000 iter)
Step 4 (진행 중) mesh 추출
        방법 1: 2DGS extract_mesh.py
        방법 2: SuGaR (coarse → refine → 텍스처)
        → 두 결과 시각적 비교

Step 5 (예정) 실험 A (hloc+COLMAP) 동일 파이프라인 실행
Step 6 (예정) A vs B 최종 결과 비교
```
