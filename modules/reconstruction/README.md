# 3D Reconstruction — 실외 이미지 기반 메시 복원 실험

드론/실외 건물 이미지로부터 **시각적 품질을 최우선**으로 한 3D 메시 복원을 연구하는 실험 저장소다.
여러 SfM(포즈/점군) 방법과 가우시안/메시 렌더러를 동일 데이터셋에서 비교하고, 그 진행 상황과
실패 사례·교훈을 문서로 축적한다.

> 이 저장소는 **실험 스크립트 + 진행 문서**가 본체다. 학습 산출물(`*.ply`, `outputs/`)과
> 대용량 데이터셋은 git에 포함하지 않으며(원격 GPU 서버에 존재), `.gitignore`로 제외한다.

---

## 목표

- 입력: 실외 객체/건물 이미지 수십~수백 장
- 출력: 텍스처 포함 3D 메시 (노이즈·artifact 최소)
- **1차 평가 기준: 시각적 품질** (Chamfer 등 수치 정확도는 부차적)
- GT가 있는 데이터셋(시뮬레이션)에 한해 포즈 정확도(ATE/RPE) 정량 비교

---

## 방법 구도

### SfM (포즈 + 점군)
| 방법 | 비고 |
|---|---|
| **MASt3R-SfM** | 메인 prior. 포즈 정확도 우수 (ATE 0.023m) |
| hloc (SuperPoint+LightGlue) → COLMAP | 비교 분기 |
| VGGT | 포즈 비교용 (ATE 6.17m로 MASt3R 대비 열위) |

### 렌더러 / 메시 추출
| 방법 | 상태 |
|---|---|
| **2DGS** (2d-gaussian-splatting) | 메인 |
| GS-2M | 큐브 세밀도 비교 |
| MiLo (SIGGRAPH Asia 2025) | 선택적 baseline |
| 3DGS / SuGaR / MeshSplatting | 비교·실험 |
| ~~Neuralangelo(SDF)~~ | 폐기 (학습 시간 과다) |

---

## 데이터셋

| 이름 | 유형 | 장수 | GT | 용도 |
|---|---|---|---|---|
| `orbit34` | 렌더/실측 혼합 | 68 | 일부 | 메인 파이프라인 |
| `real_test` | 시뮬(AirSim) | 34 | ✅ pose | 파이프라인 검증·정량비교 |
| 실제 드론(`3m_1/5m_1/7m_1`) | DJI 야외 | 55~57/고도 | ❌ | 실데이터 복원 |
| `rocket_eval_captures` | 합성 렌더 (PX4 하강 로그 재현) | 120 (stable 80) | ✅ pose | 하강 궤적 SfM/SLAM 평가 (STATUS.md PART F) |

---

## 서버 환경

| 항목 | 값 |
|---|---|
| 호스트 | `sysai3` (원격 GPU 서버) |
| GPU | NVIDIA RTX A6000 (VRAM 48GB) |
| CUDA | 12.3 (서브모듈 빌드용 11.8 혼용) |
| 데이터 | `~/Desktop/data/` (입력·실험결과, git 제외) |
| 모델 repo | `~/Desktop/models/<repo>` |
| 가상환경 | `~/Desktop/venvs/venv-<X>` (+ conda `~/miniforge3`) |

> **명명 규칙**: 실험 결과는 `experiments/{데이터셋}__{SfM}__{렌더러}` 또는
> `{데이터셋}__{방법}` 형식으로 저장한다.

---

## 저장소 구조

```
3d_reconstruction/
├── README.md                       # (이 문서)
├── STATUS.md                       # ⭐ 현재 작업 현황 — 새 세션은 이것부터
├── experiment_design_summary.md    # 실험 설계 요약 (A/B 분기, 환경)
├── pipeline_strategy_3branches.md  # 3갈래 전략 + venv×작업 매핑
├── pose_comparison_result.md       # MASt3R vs VGGT 포즈 정확도
├── real_test_reconstruction.md     # real_test(시뮬) 복원 노트
├── 2dgs_results/                   # 2DGS 결과 메타(대용량 ply 제외)
├── tools/                          # viewpoint 프로토타입 + python 유틸
├── run_mast3r_sfm.py               # MASt3R-SfM 실행
├── run_vggt_poses.py               # VGGT 포즈 추정
├── run_milo_batch.sh               # MiLo ×4 강도 순차 실행
├── make_milo_priors_batch.py       # MiLo prior 배치 생성
├── run_poisson_compare.py          # Poisson 메시 비교
├── preprocess.py / preprocess_multi.py   # 이미지 전처리
├── visualize_reconstruction.py     # 복원 결과 시각화
├── setup_2dgs_env.sh               # 2DGS venv 셋업
└── gpu_monitor_auto_run.sh         # GPU 여유 감시 → 자동 학습 트리거
```

---

## 문서 읽는 순서

1. **`STATUS.md`** — 지금 무엇을 하고 있는지(현재 상태 한 파일로 파악)
2. `experiment_design_summary.md` — 전체 실험 설계와 서버/패키지 환경
3. `pipeline_strategy_3branches.md` — 분기별 전략과 가상환경 매핑
4. `pose_comparison_result.md` / `real_test_reconstruction.md` — 세부 결과·노트

---

## 핵심 교훈 (현재까지)

- **포즈**: MASt3R-SfM ≫ VGGT (ATE 0.023m vs 6.17m). RPE를 기준 지표로 사용.
- **단색 object-centric 데이터**: GS 기반 메시 추출이 구조적으로 부적합
  (단색 박스 → Photometric Loss ≈ 0 → 기하 gradient 소실 → 메시 파편화).
- **다고도 합산 복원**: 순차 정렬 시 inter-orbit scale mismatch로 **박스 2개 분리** 발생
  (3m+7m, 3m+5m 동일). 인터리빙 정렬로 해결 시도 중.
- **하강 단일 통과 궤적(PART F)**: SfM은 전 케이스 프레임 100% 등록에 성공하지만 SLAM은
  16건 중 11건이 키프레임 1개로 실패. 원인은 ① 궤적 초반 텀블링 구간(잘라내면 회복),
  ② 반복 타일 텍스처 지형(교체 전엔 회복 불가), ③ 어안 렌즈 모델 불일치로 갈린다.
  **SfM 성공 ≠ SLAM 성립** — 두 백엔드를 같은 지표로 판단하지 말 것.

---

## 비고

- 일부 레거시 문서는 서버 재정리 이전 경로(`~/Desktop/<repo>`, `~/Desktop/venv-<X>`)를
  참조할 수 있다. 현재 서버 레이아웃은 `~/Desktop/models/`, `~/Desktop/venvs/` 기준이다.
- 학습 산출물·데이터셋은 git에 포함하지 않는다 (`.gitignore`: `*.ply`, `outputs/`).
