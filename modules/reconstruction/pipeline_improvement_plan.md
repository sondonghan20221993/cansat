# 파이프라인 개선 계획 (2026-07-05 확정)

> 배경: 시뮬레이션 검증 완료 후, 전체 파이프라인(SfM → 학습 → 메시 → 평가) 개선이 다음 목표.
> 이 문서는 후보 검토 결과와 확정된 실행 순서를 기록한다.

---

## 검토 결과 요약

| # | 후보 | 판정 | 근거 |
|---|---|---|---|
| ① | MASt3R 포즈 → COLMAP BA 재정렬 | ✅ **채택** | STATUS.md에 "천장 돌파 레버 1위"로 기록, 미시도. 하류 전 모델이 이득 |
| ② | scene graph retrieval 전환 | ✅ **채택 (승격)** | MASt3R-SfM 논문 Table 4가 직접 입증 (아래 상세) |
| ③ | hloc+COLMAP 분기 | ❌ 제외 | 오래된 기술, baseline 소개용으로만 언급 |
| ④ | 프레임 조밀화 (55→110장) | ❌ 제외 | 효과 자명, 실험 가치 낮음 |
| ⑤ | 실외 조명 대응 (appearance modeling) | ✅ 채택 (스터디 선행) | WildGaussians/bilateral grid 계열 조사 후 2DGS 적용 |
| ⑥ | PGSR | ❌ 제외 — **GS-2M 유지** | GS-2M이 PGSR 기반(개선 버전)임을 확인 → PGSR 계열 대표는 GS-2M으로 (아래 상세) |
| ⑨ | GT-free 평가 체계 | ✅ **채택** | 이미지만으로 가능 확인 (아래 지표표) |
| ⑩ | 파이프라인 자동화 + /tmp 스크립트 repo 이관 | ✅ 채택 | ⑨와 묶어 진행 |

---

## ② 근거 — MASt3R-SfM 논문 Table 4 (T&T 200뷰)

| 그래프 방식 | ATE↓ | RTA@5↑ | 쌍 수 |
|---|---|---|---|
| Complete | 0.0126 | 75.9 | 39,800 |
| **Local window (= swin)** | **0.0251** | **33.1** | 2,744 |
| Random | 0.0156 | 55.2 | 2,754 |
| **Retrieval (ASMK)** | **0.0124** | **70.9** | 2,758 |

- 같은 쌍 수 예산에서 **swin이 최하위 (무작위보다도 나쁨)** → 입력 순서 의존성이 논문 수치로 입증됨
- 논문의 "unordered collections 지원" 주장의 근거가 retrieval 그래프 = **retrieval 전환은 논문 기본 구성으로의 복귀**
- 현재 진행 중인 3m+5m 인터리빙 실험 판정 기준 (별도 실험 불필요):
  1. pointcloud.ply에서 박스 정합 여부 (CloudCompare)
  2. cross-altitude 매칭 쌍 수 (SfM 캐시에서 3m↔5m 쌍 카운트)
  3. 그룹 간 scale 비율 (시뮬에서 11.6% mismatch 측정한 방식 재사용)
- **무작위 > swin인 이유 (논문엔 설명 없음, 그래프 이론 해석)**: swin은 사슬 그래프 → 두 프레임 간 제약이
  수십 홉을 거치며 오차가 곱셈적으로 누적(drift, RTA@5 33.1이 증거). 무작위는 같은 엣지 수로도
  장거리 엣지가 생겨 그래프 지름이 O(log N)으로 줄고 loop closure 역할 → 오차가 상쇄됨.
  retrieval은 여기에 "겹침 보장"까지 더한 것. **박스 분리 = 경계 엣지 1개짜리 사슬 실패의 전형**이며,
  인터리빙(수동)과 retrieval(자동)은 같은 원리로 cross-altitude 장거리 엣지를 만드는 방법.
- 출처: [MASt3R-SfM (arXiv 2409.19152)](https://arxiv.org/abs/2409.19152)

## ② 실행 결과 (2026-07-05, 서버 sysai3에서 실제 실행 완료)

> ⚠️ **이 프로젝트는 지금까지 단 한 번도 retrieval을 쓴 적이 없었다** (swin-5만 사용, 2026-07-05 서버 로그/bash_history/배포 스크립트 전수 확인으로 검증됨).
> 아래는 2026-07-05에 처음으로 붙인 것이며, 실행 시각/경로/파라미터를 명확히 구분해 기록한다. 헷갈리지 말 것.

### 설치 (venv-mast3r, 서버 sysai3)
- `asmk` (github.com/jenicek/asmk, PyPI에 없어 소스 clone 후 `pip install . --no-build-isolation`) 설치 완료
- `faiss-cpu` 설치 완료
- retrieval 체크포인트: `~/Desktop/models/MASt3R-SLAM/checkpoints/MASt3R_ViTLarge_BaseDecoder_512_catmlpdpt_metric_retrieval_{trainingfree.pth,codebook.pkl}` → `~/Desktop/models/MAST3R_2/checkpoints/`로 복사 (동일 backbone, 파일명 규칙 일치 확인)
- `run_mast3r_sfm.py` 패치: `dust3r.image_pairs.make_pairs` → `mast3r.image_pairs.make_pairs`로 교체(retrieval 지원), `--scene_graph retrieval`, `--retrieval_model`, `--retrieval_na`(기본 20), `--retrieval_k`(기본 1) 인자 추가. 원본은 `run_mast3r_sfm.py.bak_before_retrieval`로 서버에 백업됨

### 실험 매트릭스 (서버 경로 기준, 절대 헷갈리지 말 것)

| 데이터셋 | scene_graph | 서버 출력 경로 | 상태 |
|---|---|---|---|
| 시뮬(3m+7m uniform, 34장) | swin-5 (기존) | `data/experiments/real_test_combined_uniform__mast3r/` | 기존 |
| 시뮬(3m+7m uniform, 34장) | retrieval-20-**1** | `data/experiments/real_test_combined_uniform__mast3r_retrieval/` | ❌ 7m 그룹 파탄 |
| 시뮬(3m+7m uniform, 34장) | retrieval-20-**5** | `data/experiments/real_test_combined_uniform__mast3r_retrieval_k5/` | ✅ **채택 확정** |
| 시뮬(4m uniform, 17장) | swin-5 | `data/experiments/real_test_4m_old__mast3r/` | 기존 |
| 시뮬(4m uniform, 17장) | retrieval-20-**5** | `data/experiments/real_test_4m_old__mast3r_retrieval_k5/` | ✅ 검증 완료 |
| 5m_1 (실제 드론, 55장) | swin-5 (기존) | `data/drone_real_sfm/5m_1/` | 기존 |
| 5m_1 (실제 드론, 55장) | retrieval-20-**1** | `data/drone_real_sfm_retrieval/5m_1/` | ✅ 문제 없음(궤도 매끄러움 확인) |
| 5m_1 (실제 드론, 55장) | retrieval-20-**5** | `data/drone_real_sfm_retrieval_k5/5m_1/` | ✅ 검증 완료 |

### 시뮬레이션 정량 결과 — per-group Umeyama (`tools/python/eval_retrieval_group_scale.py`)

| 지표 | swin-5 | retrieval-20-1 | **retrieval-20-5** |
|---|---|---|---|
| cross-altitude 쌍 비율 | 17.6% (30/170) | 36.8% (75/204) | 35.3% (94/266) |
| 3m ATE RMSE | 0.76cm | 0.63cm | 0.53cm |
| 7m ATE RMSE | 27.6cm | 384.33cm (파탄, 이상치 2개: 1317cm/767cm) | **2.56cm** |
| 7m ATE mean | (미측정) | 215.39cm | **2.15cm** |
| 그룹 간 scale mismatch | ~11.6% | 2.61% | **0.19%** |

**결론**: `retrieval-20-1`(k=1)은 그룹 간 scale mismatch는 고치지만 그룹 내부 로컬 밀도 부족으로 개별 프레임이 파탄남.
`retrieval-20-5`(k=5, swin의 winsize와 동일 밀도)가 두 문제를 모두 해결 — **swin 대비 7m ATE 10배 이상 개선, scale mismatch 사실상 해소**.
→ **retrieval-20-5를 앞으로 기본값으로 채택**.

### 5m_1(실제 드론) 결과 — 궤도 매끄러움 체크 (`tools/python/eval_trajectory_smoothness.py`, GT 없음)

5m_1은 55장 단일 궤도라 3m+7m 같은 그룹 분할이 없어 k=1의 약점이 애초에 안 드러남. k=5 결과 확인 완료:

| 지표 | swin-5 | retrieval-20-1 | retrieval-20-5 |
|---|---|---|---|
| step 평균 | 0.1901 | 0.1945 | 0.2048 |
| step 표준편차 | 0.0844 | 0.0860 | 0.0820 |
| step 최대 | 0.3840 | 0.3923 | 0.3998 |
| 중앙값 5배 초과 튐 | 0개 | 0개 | 0개 |

**결론**: 모든 방법 동등한 성능. 5m_1 단일 궤도에서는 k=1/k=5 차이 미미, 파라미터 통일 목적으로 retrieval-20-5 채택 확정.

### 4m(시뮬) 결과 — 궤도 매끄러움 체크 (2026-07-05, 17장)

단일 고도에서도 retrieval-20-5의 개선 여부 검증:

| 지표 | swin-5 | retrieval-20-5 |
|---|---|---|
| step 평균 | 0.6803 | 0.6671 (-1.9%) |
| step 표준편차 | 0.2026 | 0.1988 |
| step 최대 | 1.0321 | 1.0076 |
| 중앙값 5배 초과 이상치 | 0개 | 0개 |
| 포인트 개수 | 1,278,606 | 1,292,854 (+14,248) |

**결론**: 단일 고도에서도 일관된 개선 (+포인트, -궤도 불안정성). retrieval-20-5는 모든 테스트에서 안정적이거나 개선.

## ✅ retrieval-20-5 최종 채택 (2026-07-05 확정)

**종합 검증 결과:**
- **멀티 고도 (3m+7m)**: 7m ATE 27.6cm → 2.56cm (10배 개선)
- **단일 고도 (4m)**: step 0.6803 → 0.6671 (1.9% 개선)
- **실제 드론 (5m_1)**: 동등 유지, 궤도 안정

→ **모든 상황에서 안정적/개선. 기본값으로 채택**.

## ⑥ 근거 — GS-2M은 PGSR 기반 → GS-2M으로 유지 (2026-07-05 확정)

- GS-2M 논문 원문: *"we construct GS-2M from PGSR to maintain SoTA reconstruction performance"*
- PGSR의 unbiased depth rendering + multi-view constraint를 그대로 상속, material(albedo/roughness) 분해를 추가한 것이 GS-2M
- **결정**: PGSR 계열 대표는 개선 버전인 GS-2M으로 유지, PGSR 별도 실행은 하지 않음
- (참고) 굳이 돌린다면 "GS-2M − material 분해" ablation 의미 — "BRDF 분해가 저텍스처 씬에서 ill-posed" 가설 직접 검증용
- 출처: [GS-2M (arXiv 2509.22276)](https://arxiv.org/abs/2509.22276), [PGSR (arXiv 2406.06521)](https://arxiv.org/abs/2406.06521)

## GS-2M 파이프라인 개선 목록 (2026-07-06 추가)

> 실제 드론 데이터(저텍스처 박스 스캔)의 메시 품질 개선을 위한 후보 작업.
> 우선순위 기준: 포즈 정밀도 개선 + 입력 PLY 품질 + depth 신호 재활용.

| # | 작업 | 설명 | 우선순위 | 효과 기대 |
|---|---|---|---|---|
| **2** | **Depth Supervision** | MASt3R depth map → GS-2M 학습에 loss로 추가. textureless 영역(흰 벽, 단색 표면) 기하 개선 | 높음 | 저텍스처 영역 품질 ↑↑ |
| **3** | **Confidence 필터링** | MASt3R per-pixel confidence로 PLY 노이즈 제거 (중복/이중 표면 감소) | 높음 | floater ↓, 학습 속도 ↑ |
| **4** | ~~**메시 추출 파라미터 정규화**~~ ✅ 완료(2026-07-06, 이미 구현돼있었음) | TSDF voxel size/truncation을 씬 bounding box 기준으로 자동 계산. 스케일 정규화. | 중간 | 메시 추출 일관성 ↑ |

### 2번 조사 결과 (2026-07-06) — GS-2M 코드 확인: **미지원, 직접 구현 필요**

`~/Desktop/models/GS-2M` 코드 추적 결과:
- `--depths` 인자와 `dataset_readers.py`의 depth PNG 로딩 경로는 **존재하지만 죽은 배선**.
- `scene/cameras.py`의 `Camera.__init__`이 depth에서 **파일 경로 문자열(`self.depth_path`)만 저장**하고 실제 픽셀 텐서(`self.gt_depth` 등)는 만들지 않음.
- `depth_path`를 소비하는 코드가 train/utils/renderer/pbr 전체에서 0건 (grep 확인).
- `loss_utils.py`의 `depth_normal_loss`, `multi_view_loss` 등은 전부 `render_pkg['depth_map']` (렌더된 depth)에 대한 **self-supervised** 항 — 외부 GT depth와 무관.
- **결론**: 2번은 "인자 켜기"가 아니라 **학습 코드 구현**이 필요함. 구현 시 필수 고려사항:
  1. `Camera` 클래스에 실제 depth 텐서 로드 추가
  2. `train.py`에 depth loss 항 추가 (렌더 depth vs GT depth)
  3. **스케일 정렬 필수** — MASt3R는 up-to-scale이라 GT depth를 그대로 빼면 안 됨 (per-view affine 정렬 또는 gradient/normal 도메인 loss 필요)
  4. confidence 낮은 픽셀 마스킹 (3번과 시너지)
- **우선순위 재조정**: 구현 비용이 3번보다 훨씬 큼 → 3번을 먼저 진행.

### 3번 조사 결과 (2026-07-06) — MASt3R-SfM 확인: **기본 필터링은 이미 적용 중**

`run_mast3r_sfm.py` 확인 결과:
```python
pts3d_list, _, confs_list = scene.get_dense_pts3d(clean_depth=True)
masks = [c.cpu().numpy().flatten() > args.min_conf_thr for c in confs_list]  # min_conf_thr=1.5
```
- MASt3R confidence 기반 1차 필터링(`min_conf_thr=1.5`)이 **이미 PLY 저장 전에 적용됨**.
- **남은 작업** (여기가 실제 3번의 스코프):
  - voxel downsampling 없음 → 17장 시뮬에서 장당 ~7.5만 점 (과밀)
  - statistical outlier removal(SOR) 없음 → 중복/이중 표면(floater) 잔존 가능
  - confidence 임계값(1.5) 튜닝/ablation 안 해봄
- **비용**: 학습 코드 불필요, 이미 나온 PLY에 open3d 후처리 스크립트만 추가하면 됨 → **즉시 A/B 가능**. 2번 대비 압도적으로 저비용.

### 3번 실행 & 정량 검증 (2026-07-06) — ✅ **의미 있는 개선 확인**

`tools/python/filter_pointcloud.py` 작성 (voxel_down_sample + remove_statistical_outlier), 4m(swin-5/retrieval-20-5) pointcloud.ply에 적용:

| 지표 | swin-5 원본 | swin-5 필터 | retrieval-20-5 원본 | retrieval-20-5 필터 |
|---|---|---|---|---|
| 전체 점 개수 | 1,278,606 | 283,292 (22.2%) | 1,292,854 | 261,509 (20.2%) |

**단순 개수 감소가 노이즈 제거인지 검증** — GT 큐브(Cube8-12)와 Umeyama Sim3 정렬 후 박스 영역 crop, CD/F-score 비교 (`eval_pointcloud_filter_quality.py`, 서버 `/tmp/eval_mesh_4m_old.py` 로직 재사용):

| 지표 | swin-5 원본 | swin-5 필터 | retrieval 원본 | retrieval 필터 |
|---|---|---|---|---|
| CD (박스 영역) | 4.336cm | **3.783cm** (-12.7%) | 2.678cm | **2.321cm** (-13.3%) |
| F@1cm | 0.1855 | **0.2800** (+51%) | 0.2948 | **0.3977** (+35%) |
| F@5cm | 0.4196 | **0.5304** (+26%) | 0.6445 | **0.7301** (+13%) |
| F@10cm | 0.7642 | **0.8231** | 0.9843 | **0.9894** |
| 점-GT 평균거리 | 8.164cm | **7.036cm** | 4.863cm | **4.125cm** |

**결론**: 점 개수가 1/5로 줄었는데도 CD/F-score가 두 방법 모두에서 일관되게 개선됨 → 제거된 점들이 실제로 GT에서 먼 노이즈/중복 표면이었고, 남은 점들이 GT에 더 근접. **밀도 감소가 아니라 품질 개선**임을 정량 확인.

### 3번 파라미터 설계 재검토 (2026-07-06) — GT-free 원칙 반영

> **문제 제기(사용자)**: voxel=0.01은 MASt3R 좌표 단위(up-to-scale)라 씬마다 실제 물리적 크기가 다름. Umeyama scale(s=3.87~3.95)로 환산하면 voxel=0.01은 실제 **1cm가 아니라 약 3.87~3.95cm**였음. 이 scale은 GT 정렬에서만 나오므로, **GT 없는 실전 드론 데이터에는 애초에 적용 불가** — 정규화를 Umeyama scale에 의존하면 안 됨.

**해결**: `filter_pointcloud.py`에 `--voxel_multiplier` 옵션 추가 — voxel 크기를 점군 자체의 **median nearest-neighbor distance 배수**로 결정 (GT/scale 정보 불필요, 실전 데이터에도 바로 적용 가능). Umeyama scale은 결과를 실측 cm로 "해석"하는 검증 용도로만 사용.

- 4m 데이터 median NN distance: swin-5 0.001836 unit(≈0.710cm 실측), retrieval-20-5 0.001773 unit(≈0.701cm 실측) — 둘 다 거의 동일
- 기존 voxel=0.01 절대값은 배수로 환산 시 k≈5.5~5.6에 해당 (검증됨: k=5.6 필터링 시 원본 결과와 거의 일치)

### 3번 voxel size ablation (2026-07-06) — std_ratio ablation보다 우선 진행

> **근거(사용자)**: SOR이 지운 건 필터링 후 기준 0.9~1.6%뿐, 감소분의 78%p는 voxel dedup → std_ratio를 흔들어도 CD/F-score 변화가 미미할 것으로 예상됨. 같은 실험 예산이면 voxel size ablation이 기대 이득이 훨씬 큼.

`k ∈ {1.4, 2.8, 5.6}` (실측 환산 ≈1cm/2cm/4cm) 로 CD/F-score 비교:

| k (실측) | swin-5 CD | swin-5 F@1cm | retrieval CD | retrieval F@1cm |
|---|---|---|---|---|
| 원본(필터 없음) | 4.336cm | 0.1855 | 2.678cm | 0.2948 |
| k=1.4 (~1cm) | 4.234cm | 0.2037 | 2.605cm | 0.3160 |
| k=2.8 (~2cm) | 4.008cm | 0.2459 | 2.464cm | 0.3592 |
| k=5.6 (~4cm) | **3.768cm** | **0.2778** | **2.336cm** | **0.3933** |

**결과**: 예상(voxel을 줄이면 1cm 정밀도가 더 좋아질 것)과 반대로, **voxel이 클수록 CD/F-score가 계속 개선되는 단조 증가** 패턴. k=1.4~5.6 구간에서 아직 꺾이는 지점(최적점) 미도달 — 이 씬의 중복/이중표면 노이즈가 4cm 스케일 병합 손실보다 훨씬 심각하다는 뜻. **k=5.6이 3개 후보 중 최선이나 상한 미확인** → 더 큰 k(예: 8, 11)로 추가 탐색 필요.

**다음 단계:**
- ⏳ voxel k 상한 탐색 (k=8, 11 등으로 개선이 멈추거나 역전되는 지점 확인)
- 최적 k 후보 확정 후 GS-2M 학습까지 실행해 최종 메시 CD/F-score로 재검증 (점군 단계 proxy ≠ 최종 메시 최적치일 수 있음)
- 2번(depth supervision)은 우선순위 낮춤 (구현 비용 大, 3번으로 상당 부분 목적 달성)
- 4번(메시 추출 파라미터 정규화): 별도 진행

### 3번 최종 메시 단계 검증 (2026-07-06) — ⚠️ **점군 단계 결론이 뒤집힘**

점군 CD/F-score만으로는 최적 voxel을 확정할 수 없다는 우려([[feedback_gtfree_param_design]] 참고: "proxy 최적 ≠ 최종 최적")가 실제로 발생. `real_test_4m_old` (retrieval-20-5, 17장) 기준 raw/k1.4/k2.8/k5.6 4개 점군을 각각 COLMAP `points3D`로 주입해 GS-2M 30,000 iter 학습 → `render.py --extract_mesh --skip_test` → `tsdf_post.ply`를 동일 Umeyama(포즈 기반, retrieval poses.npy)로 GT 큐브와 비교:

| variant | 점군단계 CD | 점군단계 F@1cm | **메시단계 CD** | **메시단계 F@1cm** | F@5cm | F@10cm |
|---|---|---|---|---|---|---|
| 원본(raw, conf필터만) | 2.678cm | 0.2948 | 2.46cm | 0.3545 | 0.7068 | 0.9894 |
| k=1.4 (~1cm) | 2.605cm | 0.3160 | **2.43cm** | **0.3767** | 0.7102 | 0.9901 |
| k=2.8 (~2cm) | 2.464cm | 0.3592 | 2.52cm | 0.3369 | 0.6968 | 0.9901 |
| k=5.6 (~4cm) | **2.336cm** | **0.3933** | 2.56cm | 0.3223 | 0.6904 | 0.9898 |

**핵심 발견**: 점군 단계에서는 k가 클수록(더 많이 뭉갤수록) 계속 좋아지는 단조 증가였는데, GS-2M 메시 단계에서는 **정반대로 k가 작을수록 좋음** (k1.4 > raw > k2.8 > k5.6, CD/F@1cm 둘 다 동일한 순서). 즉 점군 단계의 "최선" 후보(k=5.6)가 메시 단계에서는 4개 중 **최악**.

**원인 추정**: GS-2M 초기화 점군은 densification의 seed 역할만 하고 학습 중 위치/개수가 계속 갱신됨. voxel을 크게 걸면(k=5.6) 초기 점 개수가 적어져(중복 표면이 줄어드는 대신) 학습 초반 densification이 커버해야 할 빈 영역이 늘어나고, 결과적으로 30k iter 안에 얇은 구조(큐브 모서리 등) 커버리지가 raw/k1.4 대비 나빠지는 것으로 보임. 반대로 raw/k1.4는 초기 점이 조밀해 densification 부담이 적어 최종 표면 커버리지가 더 좋음. (가설, 추가 검증 안 됨 — 필요시 densification 통계 로그로 확인 가능)

**결론 및 조치**:
- ✅ voxel k 상한 탐색(k=8, 11)은 **중단** — 메시 단계에서 이미 역전 확인되어 점군 단계 확장 탐색은 무의미
- ✅ **최종 채택 파라미터: k=1.4 (~1cm, 가장 보수적인 downsample)** — raw보다도 소폭 우수(CD -1.2%, F@1cm +6.3%), k=2.8/5.6은 raw보다도 나쁨
- 개선 폭 자체는 점군 단계(CD -12~13%, F@1cm +35~51%)보다 메시 단계(CD -1.2%, F@1cm +6.3%)에서 훨씬 작음 — GS-2M densification이 초기 점군 노이즈의 상당 부분을 학습 과정에서 스스로 흡수/보정한다는 뜻. **3번 항목의 실사용 기대 효과는 애초 예상보다 작게(그러나 확실히 양의 방향으로) 재조정**
- 방법론 교훈: 점군/전처리 단계 ablation은 반드시 **최종 소비 단계(GS-2M 학습 후 메시)까지 가서** 파라미터를 확정해야 함 — 중간 proxy 지표 단독 최적화는 최종 성능과 반대 방향일 수 있음 ([[feedback_gtfree_param_design]] 원칙 실증 사례로 기록)

### 4번 조사 결과 (2026-07-06) — ✅ **이미 구현되어 있음, 추가 작업 불필요**

GS-2M `render.py` 코드 확인 결과, `--voxel_size`/`--depth_trunc`/`--sdf_trunc`를 지정하지 않으면(기본값 -1) 이미 씬 자체 스케일로 자동 계산됨:

```python
max_depth  = args.max_depth if args.max_depth > 0 else 2.0 * scene.cameras_extent
voxel_size = args.voxel_size if args.voxel_size > 0 else max_depth / 1024.0
sdf_trunc  = args.sdf_trunc if args.sdf_trunc > 0 else 4.0 * voxel_size
```

`cameras_extent`(=`nerf_normalization["radius"]`)는 재구성된 카메라 중심들의 평균 위치로부터의 최대거리 × 1.1 — **순수 카메라 포즈 기반, GT 불필요**. 2DGS도 동일 패턴(`radius` = 카메라 중심 최소거리 기반, `voxel_size = depth_trunc/mesh_res`)으로 이미 자동 스케일링 지원.

실측 확인: 이번 4개 mesh 추출(raw/k1.4/k2.8/k5.6) 모두 `--dtu`/`--tnt`/`--blender` 프리셋 없이 기본 인자로 돌렸고, 4개 전부 동일하게 `voxel_size=0.003806` (COLMAP 단위)로 자동 계산됨 (`mesh/config.json` 확인) — 씬마다 다른 MASt3R 스케일에 자동으로 맞춰지는 것 확인.

**결론**: 4번 항목은 새로 구현할 게 없음. 오히려 STATUS.md에 남아있던 2DGS 메시 추출 명령이 `--voxel_size 0.01 --depth_trunc 6.0 --sdf_trunc 0.04`로 절대값을 강제 지정해 이 자동 스케일링을 **무력화하는 안티패턴**이었음 — STATUS.md에서 해당 부분 수정 완료 (인자 생략하고 자동 계산 쓰도록). 앞으로 메시 추출 시 voxel_size류 인자는 절대 수동 지정하지 말 것.

## 표면 지터(surface jitter) 노이즈 및 딥러닝 디노이징 검토 (2026-07-06)

3번(voxel+SOR)이 잡는 노이즈는 "명백한 outlier/중복 표면"이고, SOR은 **표면을 따라 넓게 퍼진 가우시안성 지터**(여러 뷰가 같은 표면을 각각 살짝 다른 깊이로 추정해 생기는 노이즈)는 통계적으로 이웃과의 거리가 정상 범위라 못 잡는다는 점이 논의됨. 이 유형은 딥러닝 디노이저(StraightPCF, IterativePFN, DPCD 등)의 주 타겟이지만:
- 대부분 합성 가우시안 노이즈 + CAD형 모델로 학습 → 실제 드론 포토그래메트리(MASt3R up-to-scale MVS) 데이터로의 일반화가 검증 안 됨
- 점을 이동시키는 방식이라 3번에서 확인한 "prior를 세게 건드릴수록 GS-2M densification 부담 증가 → 메시 단계 역전" 함정에 똑같이 노출될 수 있음

**대안(사전학습 디노이저보다 가벼움)**: MASt3R가 이미 뷰별로 내놓는 confidence map을 이용한 가중 병합 — voxel centroid 병합을 confidence-weighted average로 바꾸거나 저신뢰 점을 하향 가중. 노이즈의 생성 원인(깊이 추정 불확실성)에 직접 대응하고 학습 분포 괴리 문제가 원천적으로 없음. 단, confidence-실제오차 상관관계는 데이터에서 별도 확인 필요.

**합의된 다음 순서**: 디노이저 본실험(사전학습 체크포인트 도입, GPU 비용, 일반화 검증 등 비용이 큼) 전에 **오라클 실험으로 이득 상한부터 측정**하기로 함.

### 오라클 스냅 실험 계획

방법: GT 메시가 있는 씬(`real_test_4m_old`)에서, 평가 crop 영역(GT bbox + margin, 기존 eval 스크립트와 동일 기준) 안에 있는 점만 GT 표면의 최근접점으로 스냅(그 외 점은 그대로 유지). 이것이 "완벽한 디노이저"의 이론적 상한. 이 오라클 점군을 COLMAP points3D로 주입해 GS-2M 학습(30k iter) → 메시 추출 → CD/F-score를 raw/k1.4와 동일 기준으로 비교.

판단 기준:
- 오라클 개선폭이 큼(수 % 이상) → surface jitter가 실제 병목 → 디노이저 시험 정당화, 오라클 수치가 목표 상한이 됨
- 오라클 개선폭이 작음(3번의 -1.2% 수준) → 완벽한 디노이저도 메시 단계에서 무의미 → 디노이저/confidence-weighted merging 모두 접고 다른 병목(GS-2M 최적화, TSDF 융합 등) 탐색으로 전환

### 오라클 1차 시도 (2026-07-06) — ⚠️ **실험 구현 버그로 무효, 재실행**

1차 결과: CD 3.07cm, F@1cm 0.1663 — **raw(2.46cm/0.3545)와 k1.4(2.43cm/0.3767)보다 오히려 나쁨**. 표면 지터가 병목이 아니라는 결론으로 성급히 가지 않고 원인부터 진단.

**원인 확인**: crop 영역 내 스냅 대상 56,053개 점 중 고유 위치는 8,789개(15.7%)뿐 — 84% 이상이 같은 GT 표면 샘플점으로 뭉침(many-to-one). 전체 점군 기준 최근접 이웃 거리가 사실상 0(<1e-6)인 점이 86%. 3DGS/GS-2M의 `create_from_pcd`는 초기 Gaussian scale을 이웃점 거리(distCUDA2)로 계산하므로, 거리 0인 점들은 scale이 퇴화(collapse)해 해당 영역 표현이 오히려 망가짐. "완벽한 표면 위치"를 만들려다 실전에 없는 병리적 중복 초기화를 만든 것 — voxel dedup을 안 거친 게 원인.

**수정**: 스냅 후 중복 위치 제거(voxel dedup과 동일 원리, unsnapped 점은 그대로 유지) → `oracle_pointcloud_dedup.ply` (1,245,590개, 스냅점 56,053→8,789개로 축소) 생성, 재학습 진행 중.

**방법론 메모**: 오라클/합성 개입 실험도 "점을 이동시키는" 방식이면 실제 파이프라인이 겪지 않는 병리적 초기화(중복 좌표 등)를 만들 수 있음 — 결과가 직관과 크게 어긋나면 우선 구현 아티팩트부터 의심하고 진단할 것 ([[feedback_gtfree_param_design]] 원칙과 같은 결의 교훈: 결과를 있는 그대로 받아들이기 전에 파이프라인 각 단계를 검증).

### 오라클 dedup 재실행 (2026-07-06) — ✅ **결론 도출: 병목은 정확도가 아니라 밀도**

중복 제거 후에도 여전히 raw/k1.4보다 나쁨: **CD 3.15cm, F@1cm 0.1811** (dedup 전 1차: 3.07cm/0.1663과 거의 동일 — collapse 버그 수정이 핵심이 아니었다는 뜻). 단순 버그가 아니라는 걸 확인하기 위해 crop 영역(GT 큐브 주변) 내 점 개수를 5개 variant 전부에서 직접 세어 비교:

| variant | crop 영역 점 개수 | 메시 CD | 메시 F@1cm |
|---|---|---|---|
| raw(conf필터만) | 56,092 | 2.46cm | 0.3545 |
| k1.4 | 49,610 | **2.43cm**(최선) | **0.3767**(최선) |
| k2.8 | 35,414 | 2.52cm | 0.3369 |
| k5.6(filtered) | 13,933 | 2.56cm | 0.3223 |
| **oracle_dedup** | **8,828**(최소) | 3.15cm(최악) | 0.1811(최악) |

**crop 영역 점 개수와 메시 품질이 거의 정확히 같은 순서로 움직임** — 오라클은 점 위치가 GT 표면에 완벽히 일치하는데도, crop 영역 점 개수가 5개 중 최소라서 결과가 가장 나쁨. 즉 **이 씬(17장, 저뷰수, 작은 스캔 객체)에서 메시 품질을 지배하는 건 점의 "정확도"가 아니라 관심 영역의 "점 밀도"**이고, 노이즈 제거/스냅이 그 영역 점 개수를 줄이면 정확도 이득을 밀도 손실이 압도함.

**잠정 결론(교란변수 발견으로 아래에서 재검토)**:
- 표면 지터 노이즈가 메시 품질을 제한한다는 원래 가설은 이 데이터에서 기각되는 듯 보임. 병목은 점군 정확도가 아니라 GS-2M이 30k iteration의 고정 예산 안에서 (17장짜리 저뷰수 제약 아래) 작은 객체 영역의 점 밀도를 densification만으로 복구하지 못하는 것으로 추정.
- 3번(confidence/voxel/SOR 필터링) 항목은 k=1.4로 최종 확정 (이 결정 자체는 아래 재검토와 무관하게 유효).
- 향후 이 병목을 다루려면 방향을 바꿔야 함: 점을 "줄이거나 정확하게 만들기"가 아니라 **관심 영역의 점을 늘리는 방향**(예: 추가 뷰/이미지 확보, 특정 영역 depth map 초해상도 샘플링, 또는 GS-2M densification 파라미터 자체 튜닝)이 더 유망한 후보.

### 오라클 실험 교란변수 재검토 (2026-07-07) — oracle_raw로 정확도/밀도 분리

**문제 제기(사용자)**: oracle_dedup은 두 변수를 동시에 바꿨음 — (1) 위치를 GT로 스냅, (2) dedup으로 점 개수를 8,828개까지 감소. 이 실험이 확립한 건 "밀도 손실이 정확도 이득을 압도한다"이지 "정확도 개선 자체가 무익하다"까지는 아님. 딥러닝 디노이저 중 병합(merge) 방식(confidence-weighted merging)은 밀도 함정에 걸리지만, **변위(displacement) 방식**(StraightPCF/IterativePFN 등, 입출력 점 개수 동일)은 원리적으로 밀도를 줄이지 않으므로 같은 결론이 적용된다고 볼 근거가 없음. → "둘 다 이동/병합 방식이라 같은 함정" 추론은 병합 쪽에만 성립.

추가로 재확인해보니, 1차 오라클 시도(dedup 안 한 버전)도 사실 밀도를 보존한 게 아니었음 — **이산 GT 샘플점(30만 개)에 대한 최근접 스냅** 자체가 여러 입력점을 같은 이산 타겟으로 뭉치게 만들어(count는 56,053 그대로였지만 고유 위치는 이미 8,789개로 붕괴, 15.7%). 즉 지금까지의 두 오라클 시도 모두 실질적으로 밀도를 낮춘 상태였고, "밀도 고정 + 정확도만 개선"은 아직 검증 안 됨.

**oracle_raw 실험**: 이산 점군이 아니라 **연속 메시 표면**에 직접 투영(open3d `RaycastingScene.compute_closest_points`, `tools/python`으로 옮길 예정인 `oracle_project_pointcloud.py`)해 진짜 밀도를 보존. crop 영역 56,092개 전부를 GT 표면에 개별 투영(dedup 없음, count 불변). 결과: 투영 후 고유 위치 26,379/56,092(47.0%, 나머지는 박스 모서리/꼭짓점 밖으로 벗어난 점들이 같은 edge로 수렴하는 자연스러운 기하학적 효과 — 인위적 collapse 아님). GS-2M 학습(30k) 진행 중.

**판단 기준**:
- oracle_raw ≈ raw(2.46cm) 수준 → 밀도를 고정해도 정확도 이득 없음 → 디노이저 전면 폐기 결론 확정
- oracle_raw가 유의미하게 좋음(예: 2.2cm 이하) → 밀도 보존형(변위) 디노이저에는 회수 가능한 이득이 남아있음 → "병합형만 폐기, 변위형(StraightPCF 등)은 보류 후보로 재고"

어느 쪽이든 "점을 늘리는 방향이 우선"이라는 결론과 k=1.4 채택은 유효.

**결과 (2026-07-07)**: CD 2.79cm, F@1cm 0.2456.

| variant | CD | F@1cm |
|---|---|---|
| raw | 2.46cm | 0.3545 |
| k1.4 | **2.43cm**(최선) | **0.3767**(최선) |
| oracle_raw (밀도 보존, 완벽 위치) | 2.79cm | 0.2456 |
| oracle_dedup (밀도 붕괴) | 3.15cm(최악) | 0.1811(최악) |

oracle_dedup보다는 나아졌음(밀도 보존이 실제로 도움이 됐다는 증거) — 하지만 **raw/k1.4보다는 뚜렷하게 나쁨**. 즉 밀도를 count 기준 완전히 보존해도(56,092개 그대로, dedup 없음) 위치를 GT 표면에 완벽히 맞추는 것 자체가 raw의 노이즈 있는 위치보다 나쁜 결과를 냄.

**최종 결론**: "밀도 손실이 정확도 이득을 압도한다"는 가설을 넘어, **"정확도 개선 자체가 (이 씬에서는) 순이득이 아니다"**로 확장 확인됨. 변위형(displacement) 디노이저처럼 점 개수를 유지하며 위치만 정교하게 개선하는 방식도 oracle_raw와 원리적으로 동일한 개입(점을 표면쪽으로 이동)이므로 같은 결과가 예상됨 — **병합형(voxel/confidence-weighted merging)과 변위형(StraightPCF/IterativePFN 등) 디노이저 모두 폐기**. 추정 원인(미검증 가설, 확정 아님): ① 색상-위치 불일치(원래 점의 색을 유지한 채 위치만 이동시켜, 새 위치의 실제 색상과 어긋남 — photometric 최적화에 나쁜 초기조건) ② 박스가 단순 평면들로 구성돼 있어 노이즈 있는 원본 점들의 표면 주변 자연 산포(thickness)가 오히려 Gaussian 국소 형상(orientation/scale) 추정에 필요한 다양성을 제공했는데, 완벽히 평면에 붙인 점은 이를 제거해 국소 추정이 오히려 나빠짐.
- **3번(필터링) 및 표면 지터 디노이징 라인 전체 종료** — k=1.4 최종 채택 확정, 디노이저 계열(병합형/변위형 모두) 투자 안 함.
- 다음 우선순위는 기존 결론 그대로 유지: 점을 "정교하게" 대신 관심 영역의 점을 "늘리는" 방향(추가 뷰 확보, GS-2M densification 파라미터 튜닝) 또는 색상-위치 결합 문제 자체를 별도 조사.

## ⑨ GT-free 평가 지표 세트 (이미지+메시만으로 산출)

| 지표 | 입력 | 측정 대상 |
|---|---|---|
| held-out NVS PSNR/SSIM/LPIPS | 이미지 (llffhold=8) | 렌더링 품질 (시뮬에서 쓰던 방식 그대로) |
| 지면 planarity RMS | 메시 | 지면 RANSAC 평면 피팅 잔차 — "지면은 평평" 사전지식이 GT 역할 |
| floater 비율 | 메시 + 추정 포즈 | 카메라 가시영역 밖 vertex 비율 |
| 박스 분리 감지 | 점군/메시 | connected component 클러스터링 — 다고도 실험 성패 자동 판정 |
| 모델 간 상호 CD | 메시 2개+ | 방법 간 합의도 — outlier 방법 탐지 |

> ⚠️ NVS PSNR 단독은 기하를 못 잡음 (3DGS: PSNR 양호·메시 붕괴 사례) → **NVS + planarity + floater 세트**로 판정.
> 전부 open3d로 구현 가능.

---

## 실행 순서

1. **인터리빙 결과 판정** (진행 중, 위 ② 판정 기준 3개 적용)
2. **② `scene_graph=retrieval` 실행** — 3m+5m 동일 데이터로 swin-5 인터리빙과 비교 (논문 근거 확보됨, 인터리빙 결과와 무관하게 진행)
3. **① COLMAP BA 재정렬** — MASt3R 포즈/점군 → COLMAP DB → `point_triangulator` + `bundle_adjuster`. 효과 검증은 시뮬 데이터(GT)로 ATE 전후 비교
4. **⑨+⑩ 평가 스크립트 + 자동화** — `tools/eval_*` 로 repo에 정리, `/tmp` 산발 스크립트 이관, 원커맨드 파이프라인
5. **⑤ appearance modeling** — 스터디(WildGaussians, bilateral grid, GLO embedding) 후 2DGS 적용 판단

---

## ⑪ 점군 필터링 방식 비교 — SOR vs ROR, voxel 강도 스윕 (2026-07-07, 서버 sysai3)

> 배경: `4m_old` 실제 드론 데이터(retrieval-k5 SfM, 17장)에서 raw/filtered(SOR)/oracle 계열 GS-2M 결과 비교 중,
> "완벽한 위치(oracle)도 실제 SOR 필터링을 못 이긴다"는 결과가 나와 SOR의 어떤 요소가 효과적인지 분석하고 ROR과 비교.

### 중요 정정 — "k" 파라미터의 정체

기존 실험 파일명(`pointcloud_filtered_k1.4.ply` 등)의 `k`를 SOR의 `std_ratio`로 오인했으나,
`filter_pointcloud.py`를 원본 파라미터로 재실행해 역추적한 결과 **`k`는 실제로 `--voxel_multiplier`였음을 확인**
(SOR의 `nb=20, std=2.0`은 전 실험에서 고정값). 즉 지금까지의 PSNR 차이는 SOR 임계값이 아니라
**voxel 다운샘플링 강도**가 지배적으로 만든 것이었다.

| voxel_multiplier (="k") | voxel 후 점수 | SOR 후 최종 점수 | 최종 비율 |
|---|---|---|---|
| 1.4 | (미기록) | 1,012,047 | 78.3% |
| 2.8 (스크립트 기본값) | 615,815 | 594,607 | 46.0% |
| 5.6 | 268,108 | 263,749 | 20.4% |
| "filtered"(대표, 5.6과 사실상 동일 설정으로 추정) | — | 261,509 | 20.2% |

**결론**: voxel_multiplier가 클수록(더 성기게 다운샘플할수록) PSNR이 높다 (raw 1.0×상당 30.60 → 5.6× 31.38).
SOR 자체가 voxel 후 추가로 지우는 비율은 1.6~3.4%에 불과 — **SOR의 기여는 미미하고 voxel 다운샘플링이 핵심 레버**.

### ROR(Radius Outlier Removal) 비교 실험

voxel_multiplier=5.6(최고 성능 baseline과 동일)으로 고정하고, 그 위의 outlier 제거 단계만 SOR→ROR로 교체.
점 개수를 SOR과 맞춰 "같은 밀도에서 어떤 점을 지우는지"만 비교되도록 설계.

| 버전 | 파라미터 | 점 개수 | PSNR (iter 30k, train, **풀프레임·비crop**) |
|---|---|---|---|
| raw | 필터 없음 | 1,292,854 | 30.60 |
| filtered (SOR) | voxel 5.6× + SOR(nb=20,std=2.0) | 263,749 | **31.38** (최고) |
| oracle (GT 스냅) | — | — | 31.12 |
| k2.8 | voxel 2.8× + SOR | 594,607 | 31.05 |
| k1.4 | voxel 1.4× + SOR | 1,012,047 | 30.97 |
| oracle_raw | — | — | 30.89 |
| oracle_dedup | — | — | 30.86 |
| **ror_matched** | voxel 5.6× + ROR(min_pts=4, radius=3.5×medianNN) | 260,816 | **28.47** |
| **ror_aggressive** | voxel 5.6× + ROR(min_pts=6, radius=3.0×medianNN) | 234,234 | **28.50** |

**결론**: ROR은 SOR과 거의 같은 점 개수를 유지했음에도 PSNR이 raw보다도 낮게 나옴(약 -2.9dB vs SOR).
가설: ROR은 고정 반경 내 이웃 개수만 보는 절대 밀도 기준이라, voxel로 이미 균일화된 점군에서
국소 밀도 차이(예: 박스 표면 vs 성긴 배경)를 SOR(상대적/통계적 기준)만큼 잘 구분하지 못하고
유효한 성긴 영역까지 오제거했을 가능성. **이 데이터셋에서는 ROR을 SOR 대체재로 채택하지 않음.**
PCA/MLS 등 로컬 표면 피팅 계열은 위치 정제 축이라 오라클 결과(위치 정밀도 한계효용 낮음)상 기대 이득이
낮다고 판단했던 이전 결론과 함께, **다음 우선순위는 voxel_multiplier를 5.6 이상으로 더 밀어보는 스윕**과
**PSNR 대신 박스 크롭 CD/floater 비율로 재평가**하는 쪽.

> ⚠️ 위 PSNR은 전부 GS-2M의 기본 리포트(`training_utils.py`, alpha_mask 없음)로 **이미지 전체 기준**이며
> `eval_mesh_4m_old_v2.py`의 GT bbox 크롭 방식과는 다른 지표.

## ⑫ swin-5 vs retrieval-20-5 × raw vs filtered(k1.4) 4개 조합 최종 메시 검증 (2026-07-09)

> 배경: 지금까지 filtered(k1.4) 메시 단계 검증은 retrieval-20-5에서만 수행됨 (§3 "최종 메시 단계 검증"). swin-5+filtered는 누락돼 있었음 → 4개 조합(2×2)을 전부 채워 비교.

### 실행
- swin-5 `pointcloud_filtered_k1.4.ply`는 이미 서버에 존재(§3 필터링 당시 swin-5/retrieval 둘 다 생성해둠) → COLMAP `points3D.txt`만 새로 변환(`cameras.txt`/`images.txt`는 raw swin-5 dir와 동일 재사용, 카메라 포즈는 필터링과 무관하므로 안전).
- `real_test_4m_old__colmap_filtered_k1.4` → GS-2M 30k 학습(1시간 20분, train PSNR 31.20) → `render.py --extract_mesh --skip_test`(voxel_size 등 자동 계산, §4번 원칙 그대로) → `tsdf_post.ply`(7,838,253 V).
- 4개 전부 `eval_mesh_4m_old_v2.py <mesh> <poses.npy> <label>`로 동일 기준 재평가(GT 카메라 위치 기반 Umeyama, GT 큐브 5개 bbox+margin crop, CD/F@1·5·10cm).

### 결과

| | raw | filtered(k1.4) |
|---|---|---|
| **swin-5** | CD 18.05cm / F@1,5,10cm = 0, 0, 0 (crop 내 355/200,000점=0.2%) | **CD 4.20cm** / F@1cm 0.1809 / F@5cm 0.4652 / F@10cm 0.8085 (crop 내 7,843개=3.9%) |
| **retrieval-20-5** | CD 2.46cm / F@1cm 0.3545 / F@5cm 0.7068 / F@10cm 0.9894 | CD 2.43cm / F@1cm 0.3767 / F@5cm 0.7102 / F@10cm 0.9901 |

### 해석

- **swin-5 raw는 사실상 박스 복원 실패** — F-score 전부 0, crop 영역에 점이 거의 없음(0.2%). 필터링 적용 시 CD가 18.05→4.20cm로 극적으로 개선되고 F-score도 정상 범위로 회복(3.9%).
- **retrieval-20-5는 필터링 효과가 미미**(§3 기존 결론 그대로, CD -1.2%) — raw 자체가 이미 잘 복원돼 있었기 때문.
- **결론**: "필터링 효과가 작다"는 §3/§9의 기존 결론은 **retrieval-20-5 한정**이었다. SfM 품질이 나쁜(swin-5처럼 사슬형 그래프로 국소 밀도 왜곡이 있는) 데이터에서는 필터링이 복원 성패 자체를 좌우하는 핵심 변수가 될 수 있음. retrieval-20-5가 swin-5보다 전반적으로 우월하다는 기존 결론(§②)은 유지되며, 오히려 이번 결과로 더 강화됨(swin-5는 필터링 없이는 아예 실패).
- **원인 미확인 (후속 조사 후보)**: 왜 swin-5 raw만 박스 영역에 점이 거의 없었는지(과밀 중복 표면이 GS-2M densification을 방해했는지, 혹은 다른 병리적 초기화 문제인지)는 미조사. voxel dedup+SOR이 우연히 이를 해소한 것으로 보이나 근본 원인은 불명.

### 박스 크롭 CD/F-score 재평가 — PSNR 순위가 완전히 뒤집힘 (2026-07-07)

`eval_mesh_4m_old_v2.py`(GT 박스 5개 메시(Cube8~12)에 crop margin 0.3 적용, pred mesh를 그 bbox로 크롭 후 CD/F-score)로
7개 버전(oracle/oracle_dedup/oracle_raw/k1.4/k2.8/filtered/raw)을 재평가. **ROR 2개는 크롭 후 점이 0개라 평가 불가.**

| 버전 | CD(cm) | F@1cm | F@5cm | F@10cm | (참고) 풀프레임 PSNR |
|---|---|---|---|---|---|
| **k1.4** (voxel 1.4×, 최소 다운샘플) | **2.43** (최고) | 0.3767 | 0.7102 | 0.9901 | 30.97 |
| raw (필터 없음) | 2.46 | 0.3545 | 0.7068 | 0.9894 | 30.60 |
| k2.8 (voxel 2.8×) | 2.52 | 0.3369 | 0.6968 | 0.9901 | 31.05 |
| filtered (voxel 5.6×+SOR, PSNR 1위) | 2.56 | 0.3223 | 0.6904 | 0.9898 | 31.38 |
| oracle_raw | 2.79 | 0.2456 | 0.6937 | 0.9883 | 30.89 |
| oracle (GT 스냅) | 3.07 | 0.1663 | 0.6784 | 0.9870 | 31.12 |
| oracle_dedup | 3.15 (최악) | 0.1811 | 0.6313 | 0.9848 | 30.86 |
| ror_matched | 크롭 후 0/200,000 (0.0%) — **평가 불가** | — | — | — | 28.47 |
| ror_aggressive | 크롭 후 0/200,000 (0.0%) — **평가 불가** | — | — | — | 28.50 |

**PSNR 순위와 완전히 반대**: voxel_multiplier를 키울수록(다운샘플 강할수록) 풀프레임 PSNR은 오르지만
박스(작은 전경 물체) CD는 오히려 나빠짐(k1.4 2.43cm 최고 → filtered 2.56cm → oracle 3.07cm → oracle_dedup 3.15cm 최악).
**해석**: voxel 다운샘플이 배경(프레임 대부분 차지)의 노이즈는 줄여 PSNR을 올리지만, 동시에 박스 표면의 세밀한
점 밀도까지 성기게 만들어 기하 정밀도를 깎는다. **풀프레임 PSNR은 박스처럼 작은 전경 물체의 재구성 품질을
대변하지 못하는 지표**임이 실측으로 확인됨 (이전 문서의 "NVS PSNR 단독은 기하를 못 잡음" 우려가 실증됨).

**ROR의 치명적 실패가 명확해짐**: box crop 후 점이 0개 — ROR이 박스 영역의 점을 통째로 제거했다는 뜻.
이전 가설("ROR이 성긴 유효 영역을 오제거했을 것")이 최댓값으로 확인됨: 단순히 정밀도가 낮은 게 아니라
**박스 자체가 재구성 결과에서 사라짐**. ROR은 이 데이터셋 성격(작고 국소적인 전경 물체 + 넓고 조밀한 배경)에
근본적으로 부적합 — 반경 기준 절대 밀도 판정이 배경 대비 상대적으로 희소한 전경 구조를 outlier로 오판.

**다음 우선순위 수정**: voxel_multiplier는 낮을수록(1.4 근처) 박스 CD가 좋아지는 경향이 보이므로,
1.4 미만(1.0, 0.7 등) 추가 스윕 + 박스 크롭 CD를 1차 지표로 승격. PSNR은 배경 품질 참고용으로만 병기.
ROR은 이 프로젝트에서 폐기.

관련 파일(서버, 아직 저장소 미반영): `filter_pointcloud_v2.py`(SOR/ROR 겸용, `/home/sdh/Desktop/filter_pointcloud_v2.py`),
`real_test_4m_old__colmap_retrieval_k5_ror_{matched,aggressive}[__gs2m]`, `logs/4m_ror_{matched,aggressive}_gs2m*.log`,
`logs/4m_old_box_crop_eval_all.log`(7개 버전 전체 CD/F-score raw 출력).
