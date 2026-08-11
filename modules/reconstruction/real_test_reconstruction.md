# real_test 3D 복원 작업 노트 (2026-06-19)

> 새 세션에서 이 파일만 읽으면 real_test 복원 진행 상황 파악 가능.
> 전체 파이프라인 전략은 `pipeline_strategy_3branches.md`, 전체 현황은 `STATUS.md` 참고.

---

## 데이터셋 설명

| 항목 | 내용 |
|---|---|
| 이름 | `real_test` |
| 유형 | **시뮬레이션** (실제와 동일한 박스 크기 기준으로 촬영, AirSim) |
| 장수 | 34장 (실제 확인값) |
| 해상도 | 1920×1080 |
| 이미지 경로 | `~/Desktop/data/datasets/rgb/` |
| GT 포즈 경로 | `~/Desktop/data/datasets/meta/*.json` (position + quaternion) |
| 좌표계 | AirSim NED — OpenCV 좌표계와 축 관례 다름 (주의) |
| 비고 | 파이프라인 검증용 sandbox. GT 포즈 존재로 정량 비교 가능 |

---

## 실험 목표

**MASt3R-SfM vs VGGT 카메라 포즈 정확도 비교**

- GT(AirSim JSON 포즈) 기준으로 두 방법의 RPE/ATE 정량 비교
- 이후 DUSt3R, MASt3R-SLAM 등 추가 확장 가능

---

## 진행 현황 (2026-06-19 완료)

| 단계 | 상태 | 출력 경로 |
|---|---|---|
| real_test.zip 확인 | ✅ 이미 추출됨 | `~/Desktop/data/datasets/rgb/` |
| MASt3R-SfM | ✅ 완료 (34장, 149만 pts) | `experiments/real_test__mast3r/01_sfm/` |
| COLMAP 변환 | ✅ 완료 | `experiments/real_test__mast3r/02_colmap/` |
| VGGT 환경 구축 | ✅ `venv-vggt` (torch 2.3.1+cu121) | — |
| VGGT 포즈 추출 | ✅ 완료 | `experiments/real_test__vggt/poses_vggt.npy` |
| VGGT 포인트 클라우드 | ✅ 완료 (160만 pts) | `experiments/real_test__vggt/pointcloud_vggt.ply` |
| 포즈 비교 (RPE/ATE) | ✅ 완료 | `experiments/real_test__vggt/pose_comparison_result.md` |
| PLY 로컬 다운로드 | ✅ 완료 | `D:\3d_reconstruction\*.ply` |

---

## 비교 결과 요약

> 상세 결과: `D:\3d_reconstruction\pose_comparison_result.md`

### RPE (Relative Pose Error) ⭐ 핵심 지표

| 방법 | 회전 오차 평균 | 위치 오차 평균 |
|---|---|---|
| **MASt3R-SfM** | **21.9°** | 2.68 |
| VGGT | 29.3° | **2.57** |

→ **MASt3R 회전 우세, 위치는 동등 수준**

### ATE (위치만 유효)

| 방법 | 위치 오차 |
|---|---|
| **MASt3R-SfM** | **0.023** |
| VGGT | 6.17 |

> ⚠️ ATE 회전 오차(~120°)는 AirSim↔OpenCV 좌표계 고정 오프셋이라 무의미

### VGGT 성능 저하 원인

1. **이미지 비율 불일치** — 원본 1920×1080이 294×518로 전처리 (학습 기준 518×518 정사각형)
2. **장면 유형 불일치** — object-centric 드론 원형 촬영은 VGGT 학습 분포 밖
3. **Bundle Adjustment 없음** — 단일 feed-forward 추론, 오차 누적 보정 불가

---

## 스크립트

| 스크립트 | 서버 경로 | 용도 |
|---|---|---|
| `run_vggt_poses.py` | `~/Desktop/` | VGGT 포즈 + PLY 추출 |
| `compare_poses.py` | `~/Desktop/` | GT vs MASt3R vs VGGT 포즈 비교 → MD |

### VGGT 실행 커맨드
```bash
source ~/Desktop/venv-vggt/bin/activate
unset LD_LIBRARY_PATH
export CUDA_HOME=/usr/local/cuda-12.3
cd ~/Desktop

python3 run_vggt_poses.py \
    --image_dir ~/Desktop/data/datasets/rgb/ \
    --output_dir ~/Desktop/data/experiments/real_test__vggt/
```
- 모델: `~/.cache/torch/hub/checkpoints/model.pt` (로컬 캐시, 재다운로드 불필요)

### 포즈 비교 커맨드
```bash
python3 compare_poses.py \
    --meta_dir ~/Desktop/data/datasets/meta/ \
    --mast3r_dir ~/Desktop/data/experiments/real_test__mast3r/01_sfm/ \
    --vggt_dir ~/Desktop/data/experiments/real_test__vggt/ \
    --output ~/Desktop/data/experiments/real_test__vggt/pose_comparison_result.md
```

---

## 주의사항

- `LD_LIBRARY_PATH` 트랩 → 환경 전환 시 반드시 `unset LD_LIBRARY_PATH`
- AirSim NED 좌표계 ↔ OpenCV 좌표계 차이 → ATE 회전 오차 무의미, RPE 기준 사용
- VGGT numpy 버전 경고 있으나 동작에 문제 없음 (`numpy 2.2.6`, vggt 요구사항 `<2`)

---

## 출력 파일 목록

### real_test (서버)
| 파일 | 경로 | 형식 | 설명 |
|---|---|---|---|
| `pointcloud.ply` | `experiments/real_test__mast3r/01_sfm/` | PLY | MASt3R 포인트 클라우드 (149만 pts) |
| `poses.npy` | `experiments/real_test__mast3r/01_sfm/` | numpy (34,4,4) | MASt3R 카메라 포즈 (c2w) |
| `focals.npy` | `experiments/real_test__mast3r/01_sfm/` | numpy (34,) | MASt3R 초점거리 |
| `poses_vggt.npy` | `experiments/real_test__vggt/` | numpy (34,4,4) | VGGT 카메라 포즈 (c2w) |
| `pointcloud_vggt.ply` | `experiments/real_test__vggt/` | PLY | VGGT 포인트 클라우드 (160만 pts) |
| `image_list.txt` | `experiments/real_test__vggt/` | txt | VGGT 처리 이미지 순서 |
| `pose_comparison_result.md` | `experiments/real_test__vggt/` | MD | RPE/ATE 비교 결과 |

### 로컬 다운로드 (`D:\3d_reconstruction\`)
| 파일 | 설명 |
|---|---|
| `pointcloud.ply` | MASt3R PLY (real_test) |
| `pointcloud_vggt.ply` | VGGT PLY (real_test) |
| `pose_comparison_result.md` | 포즈 비교 결과 |

---

## VGGT 평가: ❌ 결과 불량

> **VGGT는 이 프로젝트 데이터에 적합하지 않음으로 결론.**

| 지표 | MASt3R | VGGT | 판정 |
|---|---|---|---|
| RPE 회전 | **21.9°** | 29.3° | MASt3R 우세 |
| RPE 위치 | 2.68 | **2.57** | 동등 (무의미한 차이) |
| ATE 위치 | **0.023** | 6.17 | MASt3R 압도적 우세 |

**실패 원인:**
1. 이미지 비율 불일치 (1920×1080 → 294×518, 학습 기준 518×518)
2. object-centric 드론 원형 촬영 → 학습 분포 밖
3. Bundle Adjustment 없음

→ **이후 포즈 추정은 MASt3R-SfM 기준으로 진행**

---

## 다음 실험 후보

| 모델 | 방식 | 서버 상태 | 추가 비교 가치 |
|---|---|---|---|
| DUSt3R | feed-forward (MASt3R 전신) | `venv-dust3r` 있음 | 세대 비교 |
| MASt3R-SLAM | SLAM (실시간) | `MASt3R-SLAM` 있음 | batch vs SLAM |
