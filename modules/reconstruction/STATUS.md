# 3d_reconstruction 작업 현황

이 문서는 실험 로그이자 재현 매뉴얼. 시간순 나열 대신 **PART(트랙)** 별로 묶어서 정리(2026-07-24 재구성). 각 PART 내부는 시간순.

## 현재 상태 요약 (2026-07-24 기준)

**메인 트랙**: `real_test_4m_old` (17장 시뮬레이션, GT 있음) 기준 GS-2M prior/전처리 비교.

- **최고 결과**: normfix + voxelcoarse(TSDF 파라미터 튜닝) — 전체 CD 3.29cm, F@10cm 0.8827, notch CD 6.15cm
- **정정된 결론**: dense MVS+ICP(denseicp768, CD 2.70cm)가 전체 표면 정확도에서 여전히 최선. native prior(normfix)는 색 로딩 버그만 고치면 더 이상 완전히 실패하지 않지만, dense+ICP보다 낫진 않음 — notch 영역만 근소 우위. ("dense+ICP 불필요"는 과장된 결론이었음, 상세는 PART A 하단)
- **사용자가 시각 확인한 핵심 병목 두 가지**: ① 표면 반사가 심한 부분의 잘못된 렌더링, ② 요철(notch) 부분의 렌더링 — 수치로 원인을 재해석하지 말 것
- **완료 (2026-07-25)**: native+SOR(task1, full CD 3.29cm/notch 6.06cm), clahe+native(task3, full CD 4.07cm/notch 6.37cm) — 둘 다 최초 실행은 환경/스케일 버그로 실패했다가 원인 수정 후 재실행 성공. 결과 다운로드+사용자 시각평가 완료(`4m_old_results/task1_task3_new/`, `전체_재평가_2026-07-24.md` §6): task1 "반사부분 실패, 요철 실패" / task3 "반사광 노이즈, 요철 초초미세 노이즈(성공상태 가까움)" — **요철은 task3(clahe단독)가 이 시점까지 최고** — (2026-07-28 정정) 더 이전 실험(§10-②, `4m_old_prep_mesh_clahe.ply`, full CD 4.08cm — `전체_재평가_2026-07-24.md`에 3.05cm로 잘못 옮겨적혀 있던 것 정정함)을 육안 재비교한 결과 오히려 더 좋음 — 요철 최고 후보 갱신. 둘 다 TSDF 표면 메시(가우시안 렌더링 원본 아님), 상세는 `전체_재평가_2026-07-24.md` §4
- **완료 (2026-07-26)**: "반사 문제 없던 전처리 + 요철 좋았던 clahe" 조합 4종 실험 — case1(retinex+clahe), case2(1D gamma+clahe), case3(z-gamma+clahe, depth 불연속 기반), case4(2D-gamma+clahe, 국소 조명지도 기반). 결과+시각평가 완료(`4m_old_results/case1234_new/`, `전체_재평가_2026-07-24.md` §7):
  - case1(retinex+clahe): post "초미세 노이즈, 성공 가까움"(task3의 "초초미세"와 거의 동급) + 반사 지적 없음(case2/4는 여전히 "반사부분 노이즈" 남음) — 이 시점 기준 최고였으나 **2026-07-28 재정정됨(아래 "정정·통합" 항목 참고, clahe 단독이 더 나음으로 갱신)**
  - case2(gamma+clahe): 요철 적은 노이즈, 반사 노이즈 있음. full CD 2.85cm/F@10cm 0.9537로 수치는 최고지만 시각은 case1보다 못함(§6번 교훈 재확인 — 수치 1위가 시각 1위는 아님)
  - **case3(z-gamma+clahe) — 폐기**: ATE 47.11cm(다른 케이스 대비 25배)로 정합 실패. post 메시가 CloudCompare에서 로드는 되나 화면에 아무것도 안 보임, raw도 사용자가 "정합실패"로 판정. z-gamma 아이디어(prior point cloud를 카메라로 재투영해 depth map 생성 → depth 불연속으로 요철 검출해 gamma 세게)는 최초 시도 시 focal 스케일백 버그(1452px, 이 프로젝트에서 여러 번 재발한 그 버그)로 한 번 실패해 수정 후 재실행했음에도 이번엔 SfM 매칭 자체가 이 전처리로 인해 흔들린 것으로 보임 — depth-aware 전처리 자체가 이 데이터셋엔 안 맞을 가능성
  - case4(2D-gamma+clahe): 요철 적은 노이즈, 반사 노이즈 있음. case2와 시각적으로 비슷한 수준
  - **다음 후보로 거론(미착수)**: 2D depth-aware shadow removal(homomorphic filtering, DSM+태양각 기반) — case3 실패로 depth 활용은 신중히 재검토 필요
- **정정·통합 (2026-07-28) — 요철+반사 최종 순위**: line 12/14의 "task3/case1이 최고"는 모두 갱신됨. `4m_old_prep_mesh_clahe.ply`(§10-②, **clahe 단독**, full CD 4.08cm, 버그 무관 확인됨)를 육안 재비교한 결과, retinex를 추가한 case1(retinex+clahe)보다 요철·반사 둘 다 더 좋음. **정정된 순위: clahe 단독(prep_mesh_clahe) > case1(retinex+clahe) > task3(clahe단독, normfix 파이프라인) > case2/case4(반사 노이즈 있음)**. 가설(미검증): CLAHE는 국소 대비를 방향성 없이 정규화해 그림자(저대비)와 반사(고대비) 양쪽을 다 눌러주는 반면, retinex(MSR)는 반사처럼 고주파·국소적인 밝은 반점에서 log-domain 연산이 불안정해져 오히려 색 왜곡을 더할 수 있음 — **"전처리는 최대한 단순한 게 유리하다"는 기존 교훈(edge/강한 clahe 실패)과 같은 방향.** 단, prep_mesh_clahe(§10-②)와 case1(§07-27, normfix 기반)은 서로 다른 시기의 다른 prior 파이프라인이라 완전한 단일변수 비교는 아님 — 필요시 동일 파이프라인에서 clahe단독 vs retinex+clahe 재검증 권장.
- **완료**: TSDF 파라미터 스윕(voxelcoarse 승, PART A 최하단), clahe 파라미터 스윕 + retinex prior 정량 비교(둘 다 native 원본보다 prior 자체 정확도는 낮음 — PART A 최하단)
- **완료**: 로컬 다운로드 57개 ply 전체 재평가(수치 + 사용자 시각평가 병기, PART A "57개 ply 전체 재평가" 절 및 `4m_old_results/전체_재평가_2026-07-24.md`)

**결과 파일**: 서버 실험 디렉토리는 `sysai3:~/Desktop/data/experiments/real_test_4m_old__*`. 로컬 다운로드본은 `C:\Users\sdh97\Desktop\4m_old_results\1_final\`(메시), `\5_prior_점구름\`(prior 점군).

**보조 트랙**: 실제 드론 데이터(PART B), 초기 시뮬레이션 real_test/blue_1(PART C, 아카이브).

**신규 (2026-07-27 착수)**: 시뮬레이션에서 검증된 개선안(전처리/prior/필터)을 실제 5m_1 드론 데이터에 하나씩 단일 변수로 적용 — 계획 및 진행 로그는 `real_data_5m1_improvement_plan.md` 참고. GT 없어 육안 비교만 가능.

**신규 트랙 (2026-08-06~08-10, PART F)**: 실제 PX4 비행 로그의 하강 궤적을 합성 환경에서 재현해 MASt3R-SfM/SLAM 복원 정확도 평가. **궤도 촬영이 아니라 하강 단일 통과 궤적**이라 PART A~C와 촬영 구도가 근본적으로 다름 — 교훈 혼용 금지. 현재까지: SfM은 10개 데이터셋 전부 프레임 100% 등록 성공, SLAM은 16건 중 11건이 키프레임 1개로 실패(원인 3종 분리 완료, PART F.3). **ATE 절대값은 두 평가 경로가 상충해 보류 중**(PART F.4) — 평가 스크립트 일원화가 선행 과제.

---

# PART A. GS-2M 4m 시뮬레이션 (메인 트랙, 최신)

## GS-2M 다음 실험 계획 (2026-07-20 작성, 착수 대기 중)

전제: 서버(sdh@210.110.250.34:8522) GPU가 타 사용자(cbchoi, `river_data` 스크립트)로 90%+ 점유 중이라 착수 보류. 모두 원본 GS-2M(`~/Desktop/models/GS-2M`) 코드는 건드리지 않고 데이터/인자만 바꿔서 실행(공정 비교 원칙 유지).

### 우선순위 순서

1. **native prior + SOR 필터 재학습** [최우선] — ✅ 완료 (2026-07-25). full CD 3.29cm / boxcrop 3.52cm / notch CD 6.06cm / F@10cm 0.8818 — normfix(3.34/6.60)보다 소폭 우세, denseicp768(2.70cm)엔 못 미침. 결과: `4m_old_results/task1_task3_new/`, 상세는 `전체_재평가_2026-07-24.md` §6. 사용자 시각평가 대기 중.
   - (최초 시도 실패 원인: conda `gs2m` 환경의 open3d/PIL이 `GLIBCXX_3.4.29` 충돌로 필터 스크립트가 죽고, 그 상태로 point cloud 0개인 채 학습이 강행돼 CUDA 에러로 즉시 크래시. `venv-gs2m`(open3d 정상 동작 확인됨)으로 전체 스크립트 환경을 바꿔서 재실행해 해결.)
   - native prior(`mast3r_retr_res768/pointcloud.ply`)에서 GT 표면 20cm+ 벗어난 floater(7.2%, ~16만점) SOR 필터로 제거
   - 학습: 동일 플래그(`--iterations 30000 --use_opacity_reduce`)

2. **TSDF 후처리 파라미터 튜닝** [가장 쌈, 학습 불필요] — ✅ 완료. voxelcoarse(voxel_size=0.008)가 신규 최고 기록(CD 3.29cm, notch 6.15cm)
   - 기존 normfix 학습 결과(`point_cloud/iteration_30000`) 재사용
   - `render.py --extract_mesh` 재실행하되 num_clusters, voxel_size 등 파라미터 변형 3~5종
   - 목적: post-cluster 시 raw의 9.6만 클러스터 중 본체 외 잡음을 더 깔끔히 제거 가능한지
   - 예상 소요: 30~60분(추출만, 학습 없음)

3. **clahe 전처리 + native prior 조합** — ✅ 완료 (2026-07-25). full CD 4.07cm / boxcrop 4.43cm / notch CD 6.37cm / F@10cm 0.7995 — full CD는 normfix보다 열세지만 notch CD는 소폭 우세(6.37 vs 6.60). §6 교훈(clahe는 요철 개선 효과가 full CD에 안 잡힘)을 감안하면 수치만으로 기각 금지 — 사용자 시각평가 필요. 결과: `4m_old_results/task1_task3_new/`, 상세는 `전체_재평가_2026-07-24.md` §6.
   - (최초 시도 실패 원인: `build_colmap_4mold_resbase.py`를 BASE=768로 호출했으나 clahe prior는 실제 512 해상도 산출물이라 focal이 651px로 잘못 계산돼 `assert focal>700`에서 즉시 죽음. BASE=512로 수정해 해결 — 7/5 문서화된 것과 같은 유형의 focal 스케일백 버그.)
   - 기존 clahe 검증(요철 형상 최고)과 native prior 승자를 조합

4. **res1024 + native prior 재평가**
   - 기존 res1024 기각은 denseicp 파이프라인 기준이었음 — native prior 주인공인 지금 재검토 가치
   - MASt3R res1024로 재실행 → GS-2M 학습
   - 예상 소요: SfM 30분~1시간 + 학습 4~6시간 + 추출/평가 20분

5. **clahe 파라미터 스윕** [전처리 추가 실험, 학습 불필요 1차 검증] — ✅ 완료. c35g8/c20g16 모두 원본 clahe·native보다 prior 자체 CD 열위(4.5~5.1cm vs native 3.83cm) — 채택 보류
   - 현재 clahe는 clipLimit 2.0 / 8×8 타일 한 세트만 검증됨
   - clipLimit 3~4, 타일 16×16 등 2~3종 변형으로 이미지 재생성 → MASt3R SfM만 돌려 prior 육안/정량 비교
   - 승자만 3번(clahe+native 학습)에 반영
   - 예상 소요: 변형당 SfM 30분~1시간(학습 없음)

6. **Retinex(MSRCR/SSR) 전처리 시험** [전처리 추가 실험] — ✅ 완료. prior CD 4.00cm, native(3.83cm)보다 열위 — 채택 보류
   - 조명/반사 성분 분리로 그늘 영역을 들어올림 — 전역 조명 불균형에 CLAHE보다 강할 수 있음
   - OpenCV+numpy로 전처리 스크립트 작성 → MASt3R SfM → prior 비교(1차는 학습 없이 판별)
   - 유망하면 GS-2M 학습까지 진행
   - 예상 소요: 스크립트 30분 + SfM 30분~1시간

(보류 후보, 5·6번 결과 보고 결정: clahe+bilateral/NLM denoise 조합, shadow-region 국소 보정, 약한 unsharp mask)

### 전체 예상 시간(순차, GPU 단독 기준)
- 1~4번: 약 20~25시간(학습만 16~24시간 차지)
- 5~6번: 1차 검증(SfM만)은 각 1시간 내외 추가

### 착수 조건
서버 GPU 점유 상황 재확인 후 시작. 확인 명령: `nvidia-smi --query-gpu=name,memory.used,memory.total,utilization.gpu --format=csv` / `nvidia-smi --query-compute-apps=pid,used_memory,process_name --format=csv`

## normfix 결론 정정 + 핵심 요지 (2026-07-24)

### 정정: "dense MVS+ICP 불필요" 결론은 과장이었음
| | 전체 CD | notch CD |
|---|---|---|
| denseicp768 (dense MVS+ICP) | **2.70cm** ✅ 더 좋음 | 8.0cm |
| normfix (native, 색로딩 버그 수정) | 3.29~3.34cm | **6.15~6.60cm** ✅ 더 좋음 |
| normfix + voxelcoarse (TSDF 파라미터 튜닝) | 3.29cm | 6.15cm |

normfix가 실제로 확인한 건 "native prior가 (색 로딩 버그만 고치면) 더 이상 완전히 실패(5.68M 가우시안 폭증·파편화)하지는 않는다"는 것뿐. **전체 표면 정확도는 dense MVS+ICP가 여전히 확실히 우위**(약 20% 더 좋음). normfix가 이긴 건 notch 영역 하나뿐이고 차이도 크지 않음. "dense+ICP 불필요, native로 충분" 식의 결론은 과장이었고 정확히는 "쓸 수는 있지만 dense보다 낫지 않은 대안"으로 정정.

### 핵심 요지 (사용자 지정, 시각 확인 기반 — 수치로는 원인 재해석하지 말 것)
사용자가 실제 렌더링/메시를 시각적으로 확인한 결과, 남은 오차의 가장 큰 두 요인은:
1. **표면 반사가 심한 부분의 잘못된 렌더링**
2. **요철(notch) 부분의 렌더링**

수치 참고(해석 없이 사실만): notch CD는 모든 실험에서 전체 CD보다 항상 나쁨(6~8cm대 vs 2.7~3.3cm대). 반사 관련 정량 지표는 아직 별도로 측정한 바 없음.

앞으로의 실험은 이 두 요인을 사용자가 시각적으로 확인해 우선순위를 판단.

## 57개 ply 전체 재평가 — 수치 + 사용자 시각평가 (2026-07-24)

로컬 다운로드본 57개(`C:\Users\sdh97\Desktop\4m_old_results\`) 전체를 대상으로, 수치(CD/notch CD/F-score, 재사용 39개 + 신규평가 7개 + 평가불가 11개)와 **사용자가 파일을 직접 열어 확인한 시각평가**를 나란히 정리. 전체 표: `4m_old_results/전체_재평가_2026-07-24.md`.

**수치 요약**: 재사용 39개(기존 README/STATUS 기록), 신규평가 7개(5_prior_점구름의 point cloud, `eval_pointcloud_filter_quality.py`로 GT crop CD 측정 — retr_raw 2.678cm, retr_k1.4 2.605cm, swin_raw 4.006cm, swin_k1.4 3.917cm, prep_prior_gamma 2.780cm, prep_prior_edge 11.055cm, prior_res1024는 정합 실패), 평가불가 11개(raw 파편화 메시, boxcrop 파생본, poses 부재).

**사용자 시각평가 원문 그대로 (해석 없이 기록만 — 반복 등장한 표현)**:
- 메시 결과 다수에서 "요철부분 실패/노이즈", "모서리 울퉁불퉁/노이즈" 반복 지적
- `denseicp768_D2`: "요철부분 실패, 그림자부분에 파란색조각, 물체 면 부분에 점같이 렌더링 안된게 있음"
- `mast3r_res768_pointcloud`(=native prior): "요철부분 그림자부분 구분어렵고 복원물체 선명도가 매우 떨어짐"
- retrieval/swin 4콤보 메시 다수: "물체소실" → "지면 재질로 렌더링"(재질감으로 대체 표현)
- prior 점구름 계열: retinex/gamma "복원물체 선명도가 떨어지고 요철 그림자 노이즈 적어짐, 밝기(명도) 증가", clahe 계열 "물체의 노이즈가 전체적으로 뒤덮힘", edge "검은색화면에 엣지복원 실패, 뭉쳐짐"
- `mast3rprior_fixed`/`nopior768`/`unionicp` 계열: 사용자가 "정합실패"로 일괄 표시

세부 파일별 코멘트는 `전체_재평가_2026-07-24.md`의 "시각평가" 열 참고. 이 항목들은 사용자가 직접 관찰한 사실이며, 원인(왜 그런지)에 대한 해석은 추가하지 않음.

### 수치-시각 교차분석 (수치와 관찰의 일치/불일치만 기록, 원인 역산 없음)

1. **full CD는 시각평가와 거의 무관, notch CD가 관찰과 일치.** notch CD 측정 항목은 전부 6.15~8.0cm로 full CD(2.7~3.4cm)의 2~3배이고, 이 항목들에서 사용자는 예외 없이 "요철부분 실패". `denseicp768_D2`는 full CD 2.70cm(최상위권)인데 notch CD 8.0cm(normfix 그룹 최악)이고 시각평가도 가장 상세한 실패 서술. → notch CD가 지각 추종 지표, full CD는 오도.
2. **최저 full CD 3개가 전부 시각 불합격 — 수치 오탐.** `unionicp`(2.26~2.36cm, 전체 최저)→정합실패 / `4_retrieval20-5_filtered_k1.4`(2.72cm)→"지면 재질로 렌더링" / `denseicp768`(2.70cm)→요철실패. 반대로 CD가 더 나쁜 `1_swin5_raw`(3.73cm)만 물체 형상 유지. 물체가 지면으로 붕괴/병합되면 점이 GT 지면 근처라 CD가 낮아지는 구조 → ATE·notch·육안 검증 없는 낮은 CD는 거짓 양성.
3. **edge 전처리는 두 파이프라인 모두 파탄.** mesh `prep_mesh_edge` 11.1cm(메시 최악)·prior `prep_prior_edge` 11.055cm(점군 최악), 시각도 각각 "요철 실패"/"엣지복원 실패, 뭉쳐짐" → 폐기 대상.
4. **retr vs swin prior 수치 우열 역전 가능.** `retr_k1.4` CD 2.605cm(prior 최상)·F@10 0.985인데 poses ATE 9.10cm·시각 "요철 노이즈"; `swin_k1.4` CD 3.917cm·ATE 1.86cm·시각 "요철 적은 노이즈". retr의 낮은 CD는 5배 느슨한 정합 위 값 → 헤드라인 CD는 retr 우위지만 정합+육안은 swin 우위.
5. **native/retinex 역설.** `native(res768)` CD 3.83cm(그룹 최저)인데 "선명도 매우 떨어짐", `retinex` CD 4.00cm(더 나쁨)인데 "요철 그림자 노이즈 적어짐+밝기 증가" — CD +0.17cm 손해로 관심영역 지각 개선.
6. **clahe = 요철 노이즈를 사실상 지운 유일한 전처리(사용자 직접 관찰, 수치엔 안 잡힘).** 전체 케이스 중 clahe만 요철 노이즈를 확실히 제거. 강도 구분 존재 — 약한 clahe(`prep_prior_clahe`·`prep_mesh_clahe`)가 요철 제거 케이스, 강한 감마 변형(`clahe_c35g8`·`clahe_c20g16`, CD 4.51→5.12)은 "물체 노이즈 전체 뒤덮힘"으로 과함. 이 요철 제거 효과는 full CD(clahe가 native보다 나쁨)에 안 잡힘 → clahe를 수치상 최악으로 본 판단은 full CD만 본 오판으로 정정. task3(clahe+native)은 유지가 맞음.

**핵심**: 1·2·6번이 "수치만으로 못 보는 것"의 정체 — (a) full CD가 놓치는 요철 실패는 notch CD에, (b) 최저 CD 항목이 물체 붕괴/정합실패, (c) clahe의 요철 노이즈 제거는 full CD에 안 잡힘. full CD 단일 지표로는 순위가 거꾸로 나옴.

## 최종 시뮬레이션 환경 검증 실험 계획 (2026-06-28)

> **위치 부여**: 이 실험은 **시뮬레이션(AirSim/Unreal) 환경에서의 마지막 검증**이다.
> 여기서 MASt3R-SfM 기반 pose-free 파이프라인(포즈 추정 → ply 초기화 → 3D 복원)이
> 시뮬레이션 GT 대비 정량적으로 검증되면, 이후 실제 드론 데이터로 넘어간다.
> 실제 드론에는 GT가 없으므로, **GT로 정량 검증이 가능한 것은 이 시뮬레이션 단계가 마지막**이다.

### 0. 데이터셋 / 환경

| 항목 | 값 |
|---|---|
| **데이터셋** | `real_test_3m_uniform`(17장) + `real_test_7m_uniform`(17장) = **34장** ← **최종 확정** |
| Google Drive | `1-RAGc9v-JDg6tcDdHrCDLmn3zTxm0smA` (`real_test_3-7m_uniform.zip`, 135MB) |
| 서버 경로 | `/home/sdh/Desktop/data/datasets/real_test_3m_uniform/`, `real_test_7m_uniform/` |
| 해상도 | 1920×1080 |
| 출처 | AirSim/Unreal 시뮬레이션 (orbit, **균일 촬영**) |
| 서버 | sysai3, RTX A6000 48GB |
| 포즈 | ✅ MASt3R-SfM 완료 → `real_test_combined_uniform__mast3r/poses.npy` (34,4,4) |
| 초기 점군 | ✅ `pointcloud.ply` 1,804,901 pts → 500k 다운샘플 → COLMAP points3D.txt |

> ⚠️ **데이터셋 교체 사유 (2026-06-28)**: 이전 `real_test_3m/7m`은 웨이포인트 기반 비행으로
> 각도 간격 std=9.4° (14°~43°), 타임스탬프 간격 3.8~7.9s로 **비균일 촬영**이었음.
> 신규 데이터셋은 각도 간격 std=1.5~2.5° (18°~31°)로 **거의 균일**하게 개선.
> 이전 실험 결과 전체 폐기, 신규 데이터로 처음부터 재시작.

### 포즈 품질 (per-group Umeyama Sim(3) 정렬, 2026-06-28 측정)

| 항목 | 3m (17장) | 7m (17장) | 전체 |
|---|---|---|---|
| ATE RMSE | **0.76 cm** | **27.6 cm** | 19.52 cm |
| RPE 회전 | 0.23° | 0.67° | — |
| focal (1920px 기준) | 969.40 px | 969.40 px | (shared_intrinsics) |

> ⚠️ 7m ATE 27.6cm는 swin-5 그래프 경계효과: 3m↔7m 간 cross-altitude 연결로
> 7m frame 0 (103cm 이상치) 발생. 7m 단독 실행 시 ATE=2.03cm였으나 합산 후 악화.
> 근본 원인: 고도 차이로 feature matching 불충분 → inter-orbit scale mismatch ~11.6%.
> **3m+7m 합산 복원 유지 이유**: 지형 커버리지 확보 목적.

### 1. 대원칙: GT는 평가에만, 학습엔 절대 안 씀

| 단계 | GT 사용? |
|---|---|
| 포즈 추정 / ply 초기화 / 모델 학습 / 메시 추출 | ❌ **GT 0% (100% MASt3R 추정값)** |
| 메시 정렬 + CD/F-score 평가 | ✅ GT 메시를 **정답지로만** (DTU/TNT 표준 관행과 동일) |
| 메시 정렬용 Sim(3) | ✅ GT 카메라 포즈를 **평가 시점 정렬에만** (학습 leakage 아님) |

> GT를 학습에 넣으면 leakage(부정행위)지만, 평가 정답지로 쓰는 것은 모든 surface
> reconstruction 논문(DTU Chamfer, TNT F-score)의 표준이다.

### 2. prior 정의 (혼동 방지 — 명확히 분리)

prior는 두 가지가 있으며 **서로 다른 것**이다:

| prior | 정체 | 역할 |
|---|---|---|
| **① ply 초기화** | MASt3R-SfM `pointcloud.ply` (1.8M점, 500k 다운샘플 후 COLMAP 투입) | 가우시안 시작 위치 (input). **모든 모델 공통** |
| **② depth supervision** | MASt3R 렌더 depth → invdepth L1 loss (`--depths`) | 학습 중 외부 깊이 정규화 |

- **"외부 depth"**(②)와 **"모델 내부가 자체 렌더링하는 depth"**(2DGS depth-distortion, TSDF fusion용 depth 등)는 다름. 후자는 알고리즘 본질이라 끌 수 없고 당연히 씀.
- 우리가 통제하는 변수는 ②(외부 MASt3R depth supervision)뿐.

### 3. 실험 트랙 (2개)

### 트랙 ① 메시 복원 (CD/F-score) — surface method 비교

| 설정 | 값 |
|---|---|
| 학습 | `eval=False`, **34장 전체** (held-out 없음 — 복원은 전량 투입이 정석) |
| 통일 prior | **① ply 초기화만** (외부 depth supervision ② 없음 → 공정) |
| 정규화 | 각 모델 **native 방식** (끌 수 없는 본연의 것) |
| 평가 | 추출 메시 vs **언리얼 export GT 메시**, CD↓ / F-score↑ |

| 모델 | 메시 추출 방식 | ply-init | 외부 depth |
|---|---|---|---|
| **3DGS + TSDF** | 별도 후처리 (open3d TSDF) | ✅ | ❌ |
| **2DGS** | native (surfel→TSDF) | ✅ | ❌ |
| **GS-2M** | native (TSDF) | ✅ | ❌ |
| **MILo** | native (learnable SDF→marching cubes) | ✅ | ❌ |

> 3DGS는 surface method가 아님(볼류메트릭 타원체) → TSDF 돌려도 noisy.
> baseline으로 포함해 "왜 2DGS/MILo가 필요한가"를 정량으로 보임. 표에 "별도 TSDF" 명시.

### 트랙 ② NVS (test PSNR) — depth supervision ablation

| 설정 | 값 |
|---|---|
| 학습 | `eval=True`, llffhold=8 → **29 train / 5 test** |
| test 5장 | `3m_000000, 3m_000008, 3m_000016, 7m_000007, 7m_000015` (자동 선택, llffhold=8) |
| 평가 | **held-out test 뷰** PSNR↑/SSIM↑/LPIPS↓ |

> 헤드라인 메시지: "② depth supervision이 **안 본 각도(test)** 품질을 얼마나 올리나".
> ⚠️ train-view PSNR은 암기 점수이므로 **성능으로 제시 금지**. 보여야 하면 "training-view
> reconstruction fidelity"로 라벨 명시.

**모델별 외부 depth supervision 지원 여부 (2026-06-29 코드 확인):**

| 모델 | 외부 depth 지원 | 방식 | 비고 |
|---|---|---|---|
| **3DGS** | ✅ | `--depths` 플래그, invdepth L1 loss (`depth_l1_weight` 지수감소) | 메인 ablation 대상 |
| **GS-2M** | ❌ | `depths` 인수 존재하나 train.py에 미사용. `depth_normal_loss`는 내부 일관성용 | 외부 depth 불가 |
| **2DGS** | ❌ | `depth_ratio`는 내부 depth-normal 일관성용. 외부 depth 파이프라인 없음 | 외부 depth 불가 |
| **MILo** | ✅ | `mast3r_depth_dir` config. depth ordering supervision (rank-based, metric 아님) | 3m_1에서 구현 완료 |

> **결론**: 외부 MASt3R depth supervision은 **3DGS와 MILo에만 적용 가능**.
> 2DGS/GS-2M은 각 모델 고유의 내부 supervision 방식을 사용하므로 depth ablation 대상 외.

**진행 상황 (2026-06-29 01:14 KST):**

| 모델 | prior | 7k test PSNR | 30k test PSNR | 상태 |
|---|---|---|---|---|
| **3DGS** (eval=True) | ① ply-init만 | **20.54** | 19.61 | ✅ 완료 |
| **3DGS + depth** (eval=True) | ① + ② MASt3R depth | 20.15 | 19.05 | ✅ 완료 |
| **MILo** (eval=True, 선택) | ① ply-init만 | — | — | ⏳ 낮은 우선순위 |

> **결과 해석**: depth supervision이 오히려 **−0.4dB 악화** (7k 기준).
> 두 모델 모두 7k에서 peak → 30k에서 과적합 (3DGS 과적합 gap ~1dB).
> 3m_1 결과(depth +0.09dB 미미)보다도 나쁜 결과 → MASt3R depth와 COLMAP이 동일 metric 스케일임에도
> 이 씬에서 depth prior는 도움이 되지 않음.
> **근본 원인 추정**: 34장 중 29장으로만 학습(eval=True) 시 지형 커버리지 부족 + 3m/7m 고도 차이
> scale mismatch가 depth loss 방향을 혼란시킴.

> ⚠️ 기존 `real_test_combined_uniform__3dgs`는 **eval=False**로 학습됨 (train PSNR=30.80dB).
> Track ②용: `real_test_combined_uniform__3dgs_eval` (eval=True 재학습).

**depth map 생성 완료 (2026-06-29):**
- `/tmp/gen_depth_combined.py`: MASt3R pointcloud → 카메라별 투영 → 34장 depth.npy
  - `R_w2c = R_c2w.T`, `t_w2c = -R_c2w.T @ t_c2w`, focal=969.40px (1920px 기준)
  - 유효 픽셀: 690k~890k/frame, 깊이 범위 [0.92, 13.2]m ✅
- `/tmp/convert_depth_combined.py`: .npy → uint16 invdepth PNG + depth_params.json
  - COLMAP/MASt3R depth 스케일 비율=**0.9899** (동일 metric ✅)
  - 출력: `real_test_combined_uniform__colmap/depths_png/` (34 PNG)
  - `sparse/0/depth_params.json` (키: `3m_000000` 형식, 확장자 없음)
- 3DGS `--depths depths_png` → `source_path/depths_png/` 자동 로드 ✅

**test 프레임** (llffhold=8, 34장 정렬 기준):
`3m_000000, 3m_000008, 3m_000016, 7m_000007, 7m_000015` (5장)

### 4. 메시 평가 절차 (트랙 ①, 언리얼 GT 받은 후)

```
0단계  handedness 일치: 언리얼(left-handed) → 한 축 flip(예: Y→−Y) → right-handed
        ※ Sim(3)는 det(R)=+1만 허용 → 거울상은 ICP로도 안 겹침. flip이 최우선.
1단계  Sim(3) 정렬: GT 카메라 1:1 대응으로 결정(s≈3.49) + ICP 미세정렬
2단계  cropping 통일: 복원 메시를 GT bbox(+margin)로 crop, 4개 모델 동일 적용
        ※ CD 양방향 → 복원 메시의 GT-외 floater가 accuracy 부풀림 방지 (TNT foreground 논리)
3단계  CD(양방향) + F-score @1cm/5cm/10cm 산출
```

### 4-1. GT 메시 (언리얼 export 완료, 2026-06-28 수령)

| 항목 | 값 |
|---|---|
| **원본 파일** | `gt_scene_full.fbx` (4.0MB, Kaydara FBX Binary, 언리얼 export) |
| 로컬 | `C:\Users\sdh97\Desktop\학교\캔위성\2차 발표자료\현재결과\gt_mesh\gt_scene_full.fbx` |
| 서버 | `sdh@sysai3:/home/sdh/Desktop/data/gt_mesh/gt_scene_full.fbx` |
| 출처 다운로드 | `C:\Users\sdh97\Downloads\my_box.fbx` (중복본 `my_box (1).fbx`는 동일 md5 → 삭제) |

**FBX 내 메시 노드 (Geometry 10개):**
- `Cube8`~`Cube12` — GT 큐브 5개 (기존 `Cube{8..12}.ply`와 동일)
- `Cube` — 추가 큐브 1개 (기존 PLY엔 없던 것)
- `Landscape` — **지면/지형 메시** (= 주변 환경)
- `MI_LayerGround` — 지면 머티리얼
- `UCX_*` — 언리얼 충돌 메시 (평가 제외 대상)

> ⭐ **의의**: 이 FBX엔 큐브뿐 아니라 **`Landscape`(지면)**가 포함됨 → 기존 "지면 오염 편향"
> (GT에 큐브만 있어 crop 안 지면 복원점이 Precision/CD를 왜곡) **해결 가능**.
> 이제 "물체만(큐브)" 평가와 "환경 포함(큐브+지면)" 평가를 **둘 다** 산출 가능.

### 4-2. FBX → PLY 변환 완료 (2026-06-29, 검증 통과)

**도구**: 서버에 blender/assimp/pymeshlab 전무 → 격리 venv(`/tmp/venv-fbx`)에 `pyassimp` 설치.
pymeshlab은 11개 메시를 1개로 병합해버려 부적합 → pyassimp로 노드별 접근.

**핵심 발견 — Y축 handedness 반전**:
- FBX 노드 월드변환 적용 후 큐브 좌표가 PLY와 **X·Z는 일치, Y만 부호 반전**
  - 예: FBX `Cube8` Y[5063,5103] vs PLY Y[−5103,−5063]
- 원인: 언리얼(left-handed) FBX export → assimp 임포트 시 Y축 부호. STATUS 0단계 handedness 그 자체
- **조치**: `Y → −Y` flip 적용 → 기존 검증된 `Cube{8..12}.ply`(카메라 ATE 0.6cm 정렬)와
  **maxΔ=0.000cm 완전 일치 확인** → 변환 신뢰성 검증됨

**메시 구조 (pyassimp, 노드 월드변환 + Y-flip 적용)**:
- `Cube8`~`Cube12`: 평가용 큐브 5개 (각 144V/48F)
- `UCX_Cube*`: 충돌 메시 → **제외**
- `Landscape`: **z=0 완전 평탄 지면**, ±252m (1.52M V/508k F)
- `Cube` 노드: 메시 없는 그룹 노드 (무시)

**산출 PLY** (서버 `/home/sdh/Desktop/data/gt_mesh/`, 로컬 `gt_mesh/`, Unreal cm 프레임):
| 파일 | 내용 | 크기 | 용도 |
|---|---|---|---|
| `gt_cubes.ply` | 큐브 5개 | 720V/240F (12K) | **물체-only 평가** |
| `gt_landscape.ply` | 지면 z=0 | 1.52M V (24M) | 지면 단독 |
| `gt_scene_clean.ply` | 큐브+지면 (UCX 제외) | 1.52M V (24M) | **환경 포함 평가** (지면 오염 해소) |

**변환 스크립트**: `/tmp/fbx_to_ply.py` (월드변환+Y-flip+UCX제외+큐브 검증 내장)

> ⚠️ **환경 평가 시 주의**: `gt_landscape`는 ±252m 무한지면 → Recall 계산 시 GT를
> **카메라 가시영역으로 crop** 필요 (안 그러면 미복원 원거리 지면이 Recall≈0 만듦).
> 물체-only 평가는 큐브 bbox crop으로 충분.

### 4-3. 메시 평가 결과 (2026-06-29, NED 공간 직접 CD)

**평가 방법** (`/tmp/eval_env.py`):
- 정렬: **3m Umeyama** (COLMAP→NED), s=3.0798, ATE=0.60cm (큐브가 3m 카메라 바로 아래)
- NED(m) 공간 직접 CD/F-score, **밀도 일관 샘플링** (crop 영역 face만 80k 동일)
- 환경 평가: 카메라 지면 footprint(2.1×2.1m)+1m crop, z∈[−1.2,0.3]
- 물체 평가: 큐브 bbox+5cm crop

**환경 포함 (gt_scene_clean = 큐브+지면)** — 지면 오염 해소된 정식 평가:

| 순위 | 모델 | CD↓ | F@1cm | F@5cm | F@10cm |
|---|---|---|---|---|---|
| 🥇 | **2DGS** | **6.85cm** | 0.028 | **0.156** | **0.914** |
| 🥈 | GS-2M | 7.47cm | 0.020 | 0.123 | 0.897 |
| 🥉 | MILo | 11.17cm | 0.008 | 0.080 | 0.366 |
| 4 | 3DGS+TSDF | 89.80cm | 0.000 | 0.000 | 0.000 |

**물체-only (gt_cubes = 큐브 5개)** — 참고:

| 순위 | 모델 | CD↓ | F@5cm | F@10cm |
|---|---|---|---|---|
| 🥇 | **2DGS** | **4.93cm** | **0.635** | **0.800** |
| 🥈 | GS-2M | 5.15cm | 0.622 | 0.789 |
| 🥉 | MILo | 7.32cm | 0.448 | 0.659 |
| 4 | 3DGS+TSDF | 큐브 bbox 내 surface 0개 (복원 실패) | — | — |

> **해석** (최종, 4모델):
> - **순위: 2DGS > GS-2M > MILo ≫ 3DGS** (물체·환경 일관)
> - **2DGS ≈ GS-2M**: F@10 0.914 vs 0.897, 거친 형상 모두 양호. 1~5cm 정밀도에서 2DGS 근소 우위.
> - **MILo 환경 F@10=0.366** (2DGS의 절반↓): MILo 메시의 **가장자리 스파이크 노이즈**가 지면
>   평탄도를 깸 (이전 real_test에서도 관찰된 MILo 고질). 물체-only(F@10=0.659)는 상대적으로 양호.
> - **3DGS는 surface method 아님** → TSDF 메시가 수직 2.7m 두께 노이즈, 지면이 GT보다
>   0.16~2.86m 위에 분포 → 환경 CD 89.8cm, 큐브 bbox 내 surface 전무. baseline으로서
>   "왜 2DGS/GS-2M/MILo가 필요한가"를 정량 입증 (3DGS 원본 가우시안 top-view 시각자료도 확보).

> ⚠️ **검증 이력 (편향 제거)**: 초기 평가는 ① 전체메시 균일샘플 → crop 영역 밀도 불일치,
> ② margin 과대(11cm) → 지면 오염으로 CD 부풀림. 밀도 일관 샘플 + GT에 지면 포함으로 수정.
> Recall@5cm은 margin 무관하게 robust (2DGS 1.0 vs 3DGS 0.096 — 초기부터 일관).

### 4-4. MILo SOR 출력메시 실험 (2026-06-29, 재학습 없이 출력 메시에 SOR 적용)

**방법**: `mesh_learnable_sdf.ply` (5,029,391V) → open3d SOR → 3강도 메시 저장 → 재평가  
**환경**: `recon3d` env (open3d 0.19.0, libstdc++ 호환)

| 강도 | nb | std | 보존V(%) | 출력 파일 |
|---|---|---|---|---|
| weak   | 30 | 2.0 | 96.7% (4,862,809V) | `mesh_sor_weak.ply` |
| mid    | 30 | 1.0 | 91.9% (4,624,059V) | `mesh_sor_mid.ply` |
| strong | 30 | 0.5 | 86.6% (4,354,600V) | `mesh_sor_strong.ply` |

**물체-only (gt_cubes) 평가:**

| 모델 | CD↓ | F@1cm | F@5cm | F@10cm |
|---|---|---|---|---|
| MILo 원본 | 7.32cm | 0.081 | 0.448 | 0.659 |
| SOR_weak   | 7.24cm | 0.082 | **0.449** | **0.661** |
| SOR_mid    | **7.17cm** | 0.070 | 0.437 | 0.659 |
| SOR_strong | 7.57cm | 0.039 | 0.368 | 0.631 |

**환경포함 (gt_scene_clean) 평가:**

| 모델 | CD↓ | F@1cm | F@5cm | F@10cm |
|---|---|---|---|---|
| MILo 원본 | 11.17cm | 0.008 | 0.080 | 0.366 |
| SOR_weak   | 10.99cm | 0.008 | 0.082 | 0.375 |
| SOR_mid    | **10.81cm** | 0.008 | 0.078 | **0.390** |
| SOR_strong | **10.81cm** | 0.005 | 0.064 | 0.406 |

> **결론**: 출력 메시 SOR은 효과가 미미하다.
> - **SOR_mid**: 환경 F@10 0.366 → 0.390 (+0.024, +6.6%) 개선이 가장 좋은 tradeoff.
> - **SOR_strong**: 환경 F@10 0.406이지만 물체 F@10 0.631로 크게 하락 (큐브 표면 정점 과도 제거).
> - **전체 결론**: 출력 메시 SOR은 이전 COLMAP prior SOR 실험과 동일하게 효과 미미.
>   MILo 한계(환경 F@10≈0.4)는 edge spike 노이즈 구조 문제 → 알고리즘 수준 해결 필요.
> - **최종 순위(환경 F@10) 변경 없음**: 2DGS(0.914) > GS-2M(0.897) ≫ MILo_SOR_mid(0.390) ≫ 3DGS(0.000)

### 5. 시뮬레이션 실험 최종 결론 (2026-06-29 확정)

### 핵심 결론

| 항목 | 결론 |
|---|---|
| **최고 모델 (메시)** | **2DGS** — CD 6.85cm, F@10 0.914, 모든 지표 1위 |
| **2위** | GS-2M — F@10 0.897, 근접하지만 전 지표 2위 |
| **NVS depth prior** | 효과 없음 (−0.4dB 악화). depth가 아닌 데이터 품질이 병목 |
| **GS-2M < 2DGS 이유** | BRDF 추정이 photometric gradient 부재 씬에서 ill-posed → 기하 왜곡 |
| **다른 모델도 못 이김** | 이 씬의 한계(텍스처 빈약+sparse view)는 알고리즘이 아닌 데이터 문제. 복잡한 방법일수록 역효과 |

### 조건부 명제 (일반화 주의)

> **"텍스처가 빈약한 미터 스케일 sparse-view 야외 씬에서는 BRDF 기반 방법이 단순 surfel 기반보다 불리하다"**
>
> GS-2M을 이기려면 더 좋은 알고리즘이 아니라 **더 좋은 데이터(텍스처, dense view)**가 필요.
> 실제 드론 데이터(자연 텍스처 존재)에서는 결과가 달라질 수 있음.

### F-score 스케일 해석 기준

- F@1cm 낮음(0.028): 결함 아님. 미터 스케일 씬에서 1cm는 의도적으로 tight한 임계값
- F@10cm 기준(0.914): 이 씬의 실용적 정확도 지표
- CD 6.85cm: NED meter 기준 ~2m orbit 씬에서 합리적 수치

---

### 6. 의존성 / 시작 순서

| 트랙 | 필요 입력 | 상태 |
|---|---|---|
| **MASt3R-SfM** (포즈 + ply) | `real_test_3m_uniform` + `real_test_7m_uniform` | ✅ **완료** |
| **COLMAP 변환** (`build_colmap_uniform.py`) | poses.npy + pointcloud.ply | ✅ **완료** → `real_test_combined_uniform__colmap/` |
| **3DGS** (30k, gs3d env) | COLMAP | ✅ 학습+TSDF메시 완료 |
| **2DGS** (30k, venv-2dgs) | COLMAP | ✅ 학습+메시 완료 |
| **GS-2M** (30k, miniforge3/gs2m) | COLMAP | ✅ 학습+메시 완료 |
| **MILo** (18k, venv-milo, outdoor+radegs) | COLMAP | ✅ 학습+메시 완료 |
| 메시 추출 (4모델) | 학습 완료 체크포인트 | ✅ **완료** |
| 메시 CD/F-score 평가 | GT + 4모델 메시 | ✅ **완료** (4-3 결과표) |
| NVS depth ablation (3DGS eval=True) | MASt3R depth map 34장 | ✅ 완료 (peak PSNR 20.54 @7k) |
| NVS depth ablation (3DGS+depth eval=True) | MASt3R depth map 34장 | ✅ 완료 (peak PSNR 20.15 @7k) |

### 메시 추출 명령 (학습 완료 후)
| 모델 | 명령 |
|---|---|
| 2DGS | `render.py --voxel_size 0.01 --depth_trunc 6.0 --sdf_trunc 0.04 --num_cluster 1` |
| GS-2M | `render.py --extract_mesh --skip_test` |
| MILo | `mesh_extract_sdf.py` |
| 3DGS | 별도 open3d TSDF 후처리 |

---


## 시뮬레이션 4m 독립 데이터셋 구축 (2026-06-29)

> **독립 데이터셋**: `real_test_4m_old`는 AirSim/Unreal 단일 씬의 4m 고도 시뮬레이션.
> 3m, 7m과 동일 씬 → GT 메시 재사용 가능. 다고도 조합 실험용으로 사용.

### 실험 조건

| 항목 | 값 |
|---|---|
| 데이터 | 4m 고도, **17장** |
| 이미지 경로 | `datasets/real_test_4m_old/rgb/` (000000~000016.png) |
| 해상도 | 1920×1080 |
| GT 메시 | 동일 (`gt_scene_clean.ply`, `gt_cubes.ply`) |
| prior | **① ply 초기화** 전 모델 공통 적용 (MASt3R pointcloud) |
| 외부 depth | ❌ 없음 (공정 비교) |

### 포즈 품질 (Umeyama Sim3, 2026-06-29)

| 항목 | 4m old (17장) | 3m uniform (참고) |
|---|---|---|
| ATE RMSE | **0.029 cm** | 0.76 cm |
| ATE Max | 0.095 cm | — |
| RPE 회전 Mean | 0.58° | 0.23° |
| Scale | 3.865 | 3.08 |

> 단일 고도 단독 실행 → scale mismatch 없음 → ATE 매우 우수.

### 학습 진행 상황 (2026-06-29 갱신)

| 모델 | 환경 | 상태 |
|---|---|---|
| **2DGS** | venv-2dgs | ✅ 학습+메시 완료 |
| **3DGS** | miniforge3/gs3d | ✅ 학습 완료 |
| **GS-2M** | miniforge3/gs2m | ✅ 학습(30k, PSNR 31.74) + 메시 완료 |
| **MILo** | venv-milo | ✅ 학습(18k) + 메시 완료 |

> **메시 산출물 (2026-06-29, 4모델 전부 완료)**
> - GS-2M: `real_test_4m_old__gs2m/train/ours_30000/mesh/tsdf_post.ply` (193M, 3.94M V)
> - MILo: `real_test_4m_old__milo/mesh_learnable_sdf.ply` (146M, vertex color 포함)
> - 메시 추출 명령: GS-2M `render.py -m <out> --extract_mesh --skip_test` / MILo `mesh_extract_sdf.py -s <colmap> -m <out> --rasterizer radegs` (milo/milo/ 에서)
> - 메시 추출 명령(추가): 2DGS `render.py -m <out> -s <colmap> --num_cluster 1` (voxel_size/depth_trunc/sdf_trunc는 지정하지 말 것 — 아래 주의사항 참고) / 3DGS `/tmp/run_3dgs_tsdf_4m.py` (open3d TSDF)
> - ⚠️ **주의**: 과거 이 문서에 2DGS 명령을 `--voxel_size 0.01 --depth_trunc 6.0 --sdf_trunc 0.04` 절대값으로 기록했었으나, 이는 GT-free 원칙 위반이자 불필요한 오버라이드였음. GS-2M/2DGS 모두 이 인자들을 비워두면(기본값 -1) 카메라 포즈 기반 씬 반지름(GS-2M: `cameras_extent`, 2DGS: `radius`, 둘 다 GT 불필요)으로 자동 스케일링됨 — MASt3R가 up-to-scale이라 씬마다 절대값이 다르게 맞아야 하므로 반드시 기본값(자동 계산)을 쓸 것. (2026-07-06, [[pipeline_improvement_plan]] 4번 항목 조사에서 확인)

### 메시 CD/F-score 평가 결과 (2026-06-29, GT 큐브 Cube8-12 기준)

> 정렬: MASt3R poses ↔ GT meta(NED) **Umeyama Sim3 1회** (단일 17장, ATE 1.86cm, s=3.865).
> 평가 스크립트: `/tmp/eval_mesh_4m_old.py` (combined_uniform 평가의 4m old 버전).
> GT를 COLMAP 공간으로 역변환 후 큐브 주변 crop → CD/F@thr (threshold는 COLMAP scale 적용).

| 순위 | 모델 | CD↓ | F@1cm↑ | F@5cm↑ | F@10cm↑ |
|---|---|---|---|---|---|
| 🥇 | **2DGS** | **3.63cm** | 0.2483 | **0.5408** | **0.8556** |
| 🥈 | **GS-2M** | 3.69cm | 0.2743 | 0.5280 | 0.8496 |
| 🥉 | **MILo** | 3.77cm | **0.2746** | 0.5284 | 0.8350 |
| — | 3DGS | 측정불가 | — | — | — |

> **3DGS 측정불가**: TSDF 메시가 GT 큐브 영역 crop 후 표면 0개 → 큐브 복원 실패(floater/지면 위주).
> combined_uniform에서도 3DGS 환경 F@10=0.000 → **동일 현상**.

#### 기존 3m+7m(combined_uniform) 물체 결과와 비교

| 모델 | 4m old CD | 4m F@10 | combined 물체 CD | combined 물체 F@10 |
|---|---|---|---|---|
| 2DGS | 3.63cm | 0.856 | (최고) | 0.914 |
| GS-2M | 3.69cm | 0.850 | 5.15cm | 0.789 |
| MILo | 3.77cm | 0.835 | 7.32cm | 0.659 |
| 3DGS | 실패 | — | 실패 | 0.000 |

> **결론: 순위·경향 기존과 동일** — 2DGS ≥ GS-2M > MILo ≫ 3DGS, 3DGS 메시 붕괴도 재현.
> **차이점**: 4m 단독(17장)은 scale mismatch가 없어 CD 절대값이 전반적으로 더 낮고(3.6~3.8cm),
> **모델 간 격차가 매우 작음**(0.14cm 차). combined는 3m↔7m inter-orbit 오차로 모델 간 격차가 컸음(5→7cm).
> → 데이터 품질(단일고도 균일)이 좋을수록 모델 선택의 영향이 줄어든다는 점을 정량 확인.

#### 박스(큐브)만 tight crop 결과 (±5cm, 지면 제외, 2026-06-30)

> 스크립트: `/tmp/eval_box_crop_4m.py`. GT 큐브 5개 각각 bbox±5cm만 pred에서 crop → CD/F-score.
> 지면 floater 제외로 큐브 복원 정확도만 순수 측정.

| 모델 | CD↓ | F@1cm↑ | F@5cm↑ | F@10cm↑ |
|---|---|---|---|---|
| **2DGS** | **2.11cm** | 0.3850 | **0.7893** | 1.000 |
| **GS-2M** | 2.14cm | **0.4324** | 0.7793 | 1.000 |
| **MILo** | 2.13cm | 0.4309 | 0.7881 | 1.000 |
| 3DGS | 실패(0개) | — | — | — |

> **해석**: CD는 2DGS가 미세하게 유리(floater 없음), F@1cm는 GS-2M이 높음(조밀 vertex → 근접 커버리지 우위).
> CD와 F@1cm의 상충은 각 지표의 특성 차이: CD는 outlier(먼 점)에 민감, F@1cm는 근접 커버리지에 민감.

#### 논문 DTU 벤치마크 CD vs 우리 시뮬 4m 비교

| 모델 | 우리 4m CD (박스 crop) | 논문 DTU CD | 씬 조건 |
|---|---|---|---|
| **2DGS** | **2.11cm** | **~0.48mm** | DTU: 텍스처 풍부, 49+장, 물체 스케일 |
| **GS-2M** | 2.14cm | ~0.80mm | DTU: GS-2M 논문 자체 보고 |
| **MILo** | 2.13cm | ~0.62mm | DTU 기준 추정 |
| **PGSR** | 미실험 | 0.52mm | DTU 최고 성능 참고값 |

> **절대값 차이(~25×)**: DTU는 cm 스케일 물체·정밀 3D 스캐너 GT. 우리는 4m 고도 항공 씬·17장. 씬 스케일 상이 → 직접 비교 불가.
> **순위 비교**: 2DGS ≥ GS-2M 경향은 DTU·우리 씬 모두 유사. 단, 우리 씬은 저텍스처+sparse view로 GS-2M BRDF 분해가 ill-posed → 모델 간 격차가 DTU보다 훨씬 작음(0.03cm 차).

### 실행 명령 (4m old — 재현용)

```bash
# GS-2M (30k)
source /home/sdh/miniforge3/bin/activate gs2m && \
python3 /home/sdh/Desktop/models/GS-2M/train.py \
  -s /home/sdh/Desktop/data/experiments/real_test_4m_old__colmap \
  -m /home/sdh/Desktop/data/experiments/real_test_4m_old__gs2m \
  --iterations 30000 --port 6012

# MILo (18k, outdoor+radegs 필수, milo/milo/ 에서 실행)
source /home/sdh/Desktop/venvs/venv-milo/bin/activate && cd /home/sdh/Desktop/models/milo/milo && \
python3 train.py \
  -s /home/sdh/Desktop/data/experiments/real_test_4m_old__colmap \
  -m /home/sdh/Desktop/data/experiments/real_test_4m_old__milo \
  --iterations 18000 --port 6013 --imp_metric outdoor --rasterizer radegs
```

> ⚠️ 주의: MILo는 `--imp_metric outdoor --rasterizer radegs` 빠뜨리면 안 됨.
> COLMAP/PLY prior 경로: `real_test_4m_old__colmap`, `real_test_4m_old__mast3r/pointcloud.ply`.

---


## retrieval-20-5 채택 & GS-2M 파이프라인 개선 조사 (2026-07-05~07)

> 상세 근거/수치는 전부 `pipeline_improvement_plan.md`에 있음. 여기는 요약+포인터만.

### scene_graph=retrieval-20-5 최종 채택 (2026-07-05)

- swin-5(순차 슬라이딩 윈도) 대신 MASt3R-SfM `scene_graph=retrieval` (Na=20 anchor, k=5 neighbor)로 전환.
- 근거: 3m+7m 다고도 조합에서 ATE 10배 개선(박스 분리 문제 해결), 4m 단일고도에서도 궤도 매끄러움 1.9% 개선, 5m_1 실제 드론 데이터에서도 동등 이상.
- **모든 신규 실험은 retrieval-20-5를 기본값으로 사용할 것.**

### GS-2M 파이프라인 개선 4항목 조사 결과 (2026-07-06~07, `real_test_4m_old` 4m 시뮬 데이터 기준)

| # | 항목 | 결론 |
|---|---|---|
| 2 | Depth Supervision | GS-2M 미지원(죽은 배선) — 구현 비용 大, 보류 |
| 3 | Confidence/voxel/SOR 필터링 | ✅ 완료. GT-free 적응형 voxel(median NN distance 배수) 설계, k=1.4(~1cm)가 메시 단계 최종 최적(raw 대비 CD -1.2%, F@1cm +6.3%) |
| 4 | 메시 추출 파라미터 정규화 | ✅ 이미 구현되어 있었음 — GS-2M/2DGS 둘 다 voxel_size 등을 비워두면 카메라 포즈 기반 자동 스케일링(GT 불필요). 과거 STATUS.md의 2DGS 하드코딩 명령은 안티패턴이라 삭제 |
| — | 표면 지터 노이즈 / 딥러닝 디노이저 검토 | 오라클 실험 3종(discrete-snap/dedup/연속표면투영-밀도보존) 전부 raw/k1.4보다 나쁨(CD 2.46/2.43 vs 2.79~3.15cm) → 밀도를 count 기준 완전히 보존해도 결과가 나쁨 → 병목은 밀도 손실만이 아니라 **정확도 개선 개입 자체**(색상-위치 불일치, 단순 평면 표면에서 노이즈의 자연 산포가 오히려 Gaussian 국소형상 추정에 유리했을 가능성). 병합형(voxel/confidence merging)·변위형(StraightPCF 등) 디노이저 모두 폐기 |

**핵심 방법론 교훈**: 점군 단계 proxy 지표(CD/F-score)로 고른 최적 파라미터가 GS-2M 메시 단계에서 정반대로 뒤집힘 — 반드시 최종 소비 단계까지 검증할 것. 오라클(이론적 최선) 실험도 밀도 보존 여부를 반드시 별도 확인해야 함(discrete-snap은 겉보기 count 유지에도 실제로는 밀도가 붕괴할 수 있음). 이 병목(작은 물체+저뷰수 씬에서 점 정확도 개선이 순이득이 아님)을 다루려면 "노이즈 제거/정교화"가 아니라 "관심 영역 점 늘리기" 방향이 유망한 다음 후보.

### swin-5 vs retrieval-20-5 × raw vs filtered(k1.4) 4개 조합 최종 메시 검증 (2026-07-09)

> 상세는 `pipeline_improvement_plan.md` §⑫. `real_test_4m_old` 기준 4개 전부 GS-2M 30k 학습→메시→`eval_mesh_4m_old_v2.py` 평가 완료.

| | raw | filtered(k1.4) |
|---|---|---|
| **swin-5** | CD 18.05cm, F-score 전부 0 (박스 복원 실패, crop 내 점 0.2%) | **CD 4.20cm**, F@10cm 0.81 (극적 개선) |
| **retrieval-20-5** | CD 2.46cm, F@10cm 0.99 | CD 2.43cm, F@10cm 0.99 (미미한 개선) |

> **핵심 발견**: "필터링 효과는 미미하다"는 기존 결론(§9 표 3번 항목)은 **retrieval-20-5 한정**이었음. SfM 품질이 나쁜 swin-5에서는 필터링이 복원 성패 자체를 가름 — raw는 박스 영역이 통째로 비어 F-score 0, 필터링 후에야 정상 복원. voxel dedup+SOR이 swin-5의 (아직 원인 미확인) 과밀/중복 노이즈를 제거해준 것으로 추정. retrieval-20-5 우위 자체는 변함없음.

---


## 4m_old 박스 복원 버그 수정 + 그림자 전처리 실험 (2026-07-10)

### ⓪ 배경: 7/5 이후 박스 소실 버그 진단·수정

4m_old(17장 시뮬)에서 6/29에 성공했던 박스 복원이 7/5 이후 실행에서 **박스가 안 보이는** 문제 발생. 원인 2가지 확정:

| # | 버그 | 실패값 → 수정값 | 영향 |
|---|---|---|---|
| 1 (주) | **COLMAP focal 스케일백 누락** | 260.4px → **979.7px** (×1920/512) | 512px 기준 focal을 1920px에 그대로 사용 → FOV 89°가 아닌 150° 어안 → 기하 뭉개짐 → TSDF 150만~420만 조각 파편화 → 1-cluster 후처리가 박스 삭제. PSNR은 31dB 나와서(가우시안 2배 보상) 지표만 보면 정상처럼 보이는 함정 |
| 2 (부) | **`--shared_intrinsics` 누락** | 프레임별 focal 제각각 → 공유 | ATE 8.90cm → 1.86cm |

> 주의: MASt3R는 실행마다 좌표계가 임의로 잡히므로 **poses.npy 값 자체가 달라지는 건 정상**(내부일관성 ATE만 보면 됨). prior PLY도 정상이었음.
> 1번(swin5_raw)만 살아남았던 건 raw여서가 아니라 **버그 발생 전 6/29에 학습**된 산출물이었기 때문.

### ① 수정 후 4-combo 결과 (retrieval vs swin × raw vs k1.4 필터)

focal 979.7 + shared_intrinsics 적용, 4개 전부 박스 복원 성공.

| 조합 | 전체 CD | ATE |
|---|---|---|
| swin_raw | 3.70cm | 1.86cm |
| swin_k1.4 | 3.73cm | 1.86cm |
| retr_raw | 2.80cm | 1.90cm |
| **retr_k1.4** | **2.72cm** | 1.90cm |

→ **retrieval-20-5가 swin-5보다 ~25% 우수**, k1.4 필터는 retrieval에서만 소폭 개선. 기존 "retrieval + k1.4" 채택 방향이 올바른 focal에서도 유효함 재확인.

### ② 그림자 요철 개선 — 이미지 전처리 3종 실험

문제: ㄷ자 오목부(요철) **그림자 영역에 prior 점이 희박**해 GS-2M이 벽을 못 세움.
전처리 3종을 retrieval-20-5(raw)로 각각 MASt3R→GS-2M 학습. baseline = 무전처리 retr_raw.

| 변형 | 출처 | 방법 |
|---|---|---|
| edge | 내(사용자) 아이디어 | FFT 하이패스(엣지만) |
| gamma | Claude 추천 | γ=0.6 그림자 밝히기 |
| clahe | Claude(GS 표준 전처리) | LAB-L에 CLAHE(clip2.0, 타일8×8) |

### ③ 핵심 발견: 측정 범위별로 승자가 완전히 다름

| 변형 | 전체 씬 CD | 박스(5cm) CD | 요철 CD | 요철 pred점 | 요철 정밀도(pred→gt) | 요철 void점(적을수록↑) |
|---|---|---|---|---|---|---|
| baseline | **2.80** | **1.76** | 8.30 | 370 | 8.87cm | 107 |
| gamma | 2.76 | 2.02 | 9.19 | 376 | 8.67cm | 114 |
| clahe | 4.08 | 2.34 | 6.46 | 514 | 7.15cm | 117 |
| edge | 11.01 | 2.36 | **5.65** | **648** | **4.78cm** | **63** |

- **밝은 박스 표면**: baseline 최고 (전처리는 노이즈만 추가)
- **그림자 요철(점-거리 지표)**: edge > clahe >> baseline > gamma
- edge는 **전역 최악(11cm, 배경 쓰레기 웹)·요철 국소 최고** → 그대로는 못 씀

### ④ 결정적: CD가 이 데이터셋의 시각품질 지표로 부적합

육안(CloudCompare)으로 본 오목부 형상 충실도:
- **clahe = 최고**: 외곽선은 거칠지만 오목부가 또렷이 열려 벽·틈이 명확 (ㄷ자 가장 정확)
- **edge**: 표면 매끈하나 **안쪽 코너를 둥글려 오목부를 메꿈**(형상 뭉개짐) — "그림자 연결돼 보임"의 정체
- **gamma**: 쪼그라들고 녹은 형태 (최악)

→ **CD/정밀도는 "매끈하지만 코너 뭉개짐(edge)"을 편애**하고 "거칠지만 형상정확(clahe)"에 벌점. **위상 충실도를 못 잡음.** README 1차 기준이 "시각적 품질"이므로 **clahe가 실질 승자**(사용자 첫 직감과 일치).

정리:
- 형태 충실도(육안, 오목부): **clahe > edge > gamma**
- 표면 매끄러움: edge > gamma > clahe
- 평균 CD: baseline > gamma > clahe > edge (형상충실도와 역상관 → 신뢰 부적합)

### ⑤ 산출물 위치

- 서버: `real_test_4m_old__colmap_retr_{edge,gamma,clahe}_raw__gs2m/` (메시), `real_test_4m_old__mast3r_retr_{edge,gamma,clahe}/` (prior)
- 평가 스크립트: `eval_mesh_boxcrop.py`(박스 타이트), `eval_mesh_notch.py`(요철), `eval_notch_precision.py`(정밀도/재현율/void 분리)
- 전처리: `preprocess_3variants.py` / 파이프라인: `run_3variants_full.sh`
- 윈도우 바탕화면: `4m_old_prep_mesh_{edge,gamma,clahe}.ply`, `4m_old_prep_prior_*.ply`

### ⑥ 다음 스텝 (clahe 후속, 미실행)

clahe 강점(국소대비→그림자 형상) 유지 + 약점(거친면·배경쓰레기) 제거:
1. **clahe + bilateral 디노이즈**: 엣지보존하며 표면 노이즈만 제거 (최우선)
2. **clahe_soft**: clip1.5 + 타일16 + γ0.8 (완만한 그림자회복)
3. **clahe 마스킹**: 그림자 픽셀에만 적용, 밝은면·배경 원본 (배경오염 차단)
4. **clahe prior + k1.4 필터**: 이미지 재처리 없이 배경 쓰레기 정리 (제일 쌈, GS-2M 1회)

### ⑦ 요소별 순차 실험 — confidence 완화 기각, Dense MVS 부분 성공 (2026-07-10)

**동기**: "raw에서 할 수 있는 최대한의 전처리를 시도 → 유의미한 결과 앙상블" 방침 확정.
축 분류: ①이미지 전처리(위 ②에서 완료) ②MASt3R confidence 완화 ③Dense MVS(점 채우기) ④GS 단계 내부 튜닝 ⑤평가지표.

**② confidence 완화 — 기각.**
`min_conf_thr` 1.5→1.0→0.5로 완화(매칭 캐시 재사용, 재매칭 없음), notch bbox 내 점 개수 측정:

| min_conf_thr | notch 점 | void 점 |
|---|---|---|
| 1.5(기존) | 1,750 | 697 |
| 1.0 | 1,750 | 697 |
| 0.5 | 1,747 | 698 |

완화로 늘어난 점(1.3만~2.3만)은 전량 notch 바깥(배경)에 낙하 → **그림자는 confidence 문제가 아니라 애초에 매칭 신호 자체가 없음**. GS 재학습 없이 폐기.

같은 방식으로 ①(edge/gamma/clahe) prior도 재검증: notch 점 raw 1,750 > gamma 1,610 > clahe 1,599 > edge 762.
**raw가 이미 notch 점 최다** — clahe의 시각적 우위는 prior 커버리지가 아니라 GS 단계 photometric 최적화 효과로 재해석.

**③ Dense MVS(COLMAP patch_match_stereo + stereo_fusion) — 부분 성공.**
raw prior의 sparse COLMAP 모델(포즈 고정) 위에 dense stereo를 얹어 점을 채움. 디버깅 중 숨은 기본값 문제 3개 발견:
- `depth_min/max` 미지정 시 실패 → 카메라 좌표계 깊이 분포(1~99%ile)로 수동 계산해 지정
- `write_consistency_graph`(기본 `0`) 꺼져있으면 fusion이 무조건 0점 반환
- `StereoFusion.min_num_pixels`(기본 5)가 17장 sparse-view(넓은 베이스라인)엔 과함 → 2도 0점, **1로 낮춰야** 함(어차피 cross-view consensus는 patch_match 단계의 `filter_min_num_consistent=2`에서 이미 적용됨, fusion에서 또 요구하면 이중 필터링)

결과(voxel 다운샘플 후 147만점 vs 450만점 비교):

| | notch 점 | void 점 | notch/void |
|---|---|---|---|
| sparse raw(기존) | 1,750 | 697 | 2.51 |
| dense(voxel_mult 6.0, 147만점) | 1,611 | 481 | 3.35 — but notch **절대량 기존보다 감소** |
| **dense(voxel_mult 2.8, 450만점)** | **5,051** | 1,536 | 3.29 |
| dense(무압축, 1030만점) | 9,001 | 2,665 | 3.38 |

voxel 다운샘플은 공간적으로 균일해서 세게 걸면 notch 이득이 사라짐 → 450만점 버전 채택, `colmap_dense_raw` 빌드 후 GS-2M 학습(**raw baseline과 동일 파라미터**: `--iterations 30000`, 추가 플래그 없음).

**최종 메시 지표 (raw baseline vs dense_raw, 동일 crop):**

| 지표 | raw baseline | dense_raw |
|---|---|---|
| notch mesh 점 수 | 370 | 587 (+59%) |
| notch CD | 8.30cm | 8.45cm |
| notch F@5cm | 0.358 | 0.367 |
| notch 정밀도(pred→gt) | 8.87cm | 9.42cm |
| notch 재현율(gt→pred) | 7.73cm | 7.48cm |
| notch spurious>5cm | 62.4% | 64.2% |
| **void(열린 입) floater 점** | 107 | **193 (+80%)** |
| 전체 box CD | 3.78cm | 3.77cm |

**해석**: notch 점은 늘었지만(+59%) void 가짜 floater도 비슷하거나 더 늘어(+80%) 지표상 우열 불명확 — dense fusion이 그림자 표면과 빈 공간 노이즈를 구분 없이 같이 채웠기 때문(전역 union의 한계).
육안 확인(CloudCompare, 사용자 보고): **"표면은 dense가 더 좋게 나옴, 다만 요철 부분 오염은 여전함"** — 정량 지표(void floater 증가)와 일치하는 소견.

**다음 후보**:
1. **외과적 union**: dense 점을 notch bbox로만 crop + 로컬 outlier 제거 → raw baseline prior(전역 검증됨)에 patch로만 삽입, 나머지 씬은 오염 없이 유지 (미실행)
2. **clahe 이미지로 Dense MVS 재시도**: clahe가 대비를 살려 매칭 신호가 원본보다 강할 수 있어 floater가 raw보다 적을 가능성 (미실행)
3. ④(GS 내부: densify 완화, depth/normal prior, opacity entropy) — 아래 실행함

**산출물**: 서버 `real_test_4m_old__colmap_dense_raw__gs2m/`, dense 점구름 `real_test_4m_old__colmapfix_retr_raw/dense/fused_ds.ply`(450만점). 윈도우 바탕화면 `4m_old_dense_raw.ply`, `4m_old_raw_baseline.ply`.

### ⑧ GS-2M 내부 floater 억제 옵션(`use_opacity_reduce`) 실험 — 효과는 있으나 미미 (2026-07-10)

**배경**: 외부 논문 조사(DN-Splatter/2D-SuGaR, adaptive densification+planar prior, GSurf/StableGS opacity entropy, G4Splat/Gaussian Scenes 생성모델)로 ④(GS 내부 튜닝) 방향을 구체화하려던 중,
GS-2M 코드 확인 결과 **depth-normal 일관성 손실은 이미 기본값으로 전 실험(raw/dense 포함)에 적용 중이었음**을 발견(`lambda_depth_normal=0.03`, `lambda_multi_view=1.0`, `geometry_from_iter=5000` — 이미 5000~30000 iter 구간 내내 활성).
반면 **`use_opacity_reduce`(500 iter마다 opacity를 0.8로 캡 — "Periodically reduce opacity to remove floaters" 주석)는 기본값 `False`로, 지금까지 모든 런에서 꺼진 채였음** → dense MVS의 void floater 문제(②참고, +80%)에 정확히 대응하는 미사용 레버 발견.

**실험**: `colmap_dense_raw`(dense MVS prior, ⑦ 참고) 소스에 `--use_opacity_reduce` 플래그 하나만 추가, 나머지 파라미터(`--iterations 30000`) 동일 유지하고 재학습.

| 지표 | raw baseline | dense_raw | **dense_opred** |
|---|---|---|---|
| notch mesh 점 | 370 | 587 | 562 |
| notch CD | 8.30cm | 8.45cm | **8.18cm**(최고) |
| notch F@5cm | 0.358 | 0.367 | **0.374**(최고) |
| notch spurious>5cm | 62.4% | 64.2% | 62.1%(최소) |
| **void floater 점** | **107** | 193 | 186 |
| 전체 box CD | 3.78cm | 3.77cm | 3.78cm |

**결론**: notch 지표 전반이 소폭 개선되고 void floater도 193→186로 약간 줄었으나, **raw baseline(107)에는 한참 못 미침** — 효과는 실재하나 결정적이지 않음.
원인 추정: opacity 상한 캡은 "순한 감쇠"라 이미 자리잡은 floater를 확실히 제거하지 못함(GSurf/StableGS의 진짜 entropy 정규화 항보다 약한 메커니즘). 하이퍼파라미터(`opacity_reduce_interval` 축소, cap 하향)로 더 튜닝하는 것보다, **다음 후보 1번(외과적 union)이 근본적으로 더 확실한 해법**으로 판단 — void 영역에 dense 점 자체가 안 들어가게 원천 차단.

**산출물**: 서버 `real_test_4m_old__colmap_dense_raw_opred__gs2m/`.

### ⑨ 보편(GT-free) 기준 전환 + **근본원인 발견: dense MVS 점구름 20cm 계통 오프셋** (2026-07-11)

**방침 전환**: 사용자 지시로 "이 데이터셋 전용(GT bbox 수동 crop)" 방법 배제, **어느 씬에나 적용 가능한 보편 기준**만 사용. GT는 검증(notch/void 측정)에만 사용.

**⑨-1. TSDF 재추출 실험 (D1/D2, 재학습 없음, 씬 무관 파라미터)**

| 지표 | dense_opred 기준 | D1 `--filter_depth` | **D2 `sdf_trunc` ½ (6cm→3cm)** |
|---|---|---|---|
| notch CD | 8.18cm | 8.18 (동일) | **8.11** |
| notch F@5cm | 0.374 | 0.374 (동일) | **0.389** |
| 전체 box CD | 3.78cm | 3.78 (동일) | **3.67** (box-crop 기준 전체 최고) |

- **D1은 no-op 코드버그**: `render.py:100` 필터 조건 `acos(|dot|) > 100°`인데 `abs()` 탓에 acos 범위가 [0°,90°] → 100°에 절대 도달 불가. 결과 바이트 동일로 확인.
- **D2는 전 지표 개선 + 공짜(재학습 없음) + 씬 무관** → 이후 파이프라인에 기본 반영 가치.

**⑨-2. GT-free 점구름 필터 2종 — 모두 기각 (정직 기록)**

| 필터 | 원리 | 결과 (dense 원본: notch 5,051 / void 1,536) |
|---|---|---|
| PCA planarity (`pca_filter.py`) | 로컬 PCA λ3/Σλ로 비평면 산란점 제거 | notch 4,091(-19%) / void 1,512(-1.6%) — **역효과** |
| Free-space carving (`freespace_carve.py`) | 카메라~관측표면 ray 사이 허공점 제거 (MVS depth 사용) | notch 4,720(-7%) / **void 1,536(0개 제거)** — **무효** |

기각 원인이 곧 힌트였음: void 오염이 (a) 산란이 아니고(PCA 실패) (b) depth 관측과 모순도 아님(FSC 실패) → "모든 뷰가 일관되게 표면이라 믿는 매끈한 시트" = **점 자체가 계통적으로 밀려있다**는 신호.

**⑨-3. 근본원인: dense 점구름 전체가 sparse 대비 강체 이동 (ICP로 발견·해결)** ⭐

dense→sparse 최근접거리 median **15cm(실측)** — 같은 월드좌표인데 표면이 안 겹침. ICP(point-to-point, with_scaling) 진단:
- 회전≈단위행렬, scale≈0.9995, **translation ≈ 20cm(실측, 주로 +z)**, fitness 0.999
- 해석: 전 카메라가 nadir(공통 시선축)라 MVS depth의 계통 편향(~4%)이 전역 z-이동으로 나타남 (원인 미규명 — cameras.txt focal은 982.125=focals.npy×3.75로 정확히 일치 확인, focal 탓 아님. sparse 기하가 GT와 정합함은 기존 CD 2.8cm로 입증되어 있으므로 편향은 MVS 쪽. open question)

**ICP 정합 적용 후 (보편 기법: dense→sparse ICP는 GT-free 표준 정합):**

| prior | notch | void |
|---|---|---|
| sparse raw | 1,750 | 697 |
| dense 정합 전 | 5,051 | 1,536 |
| **dense + ICP** | **9,961 (5.7배)** | **56 (sparse의 1/12)** |

→ **"void 오염"의 정체 = 오염이 아니라 20cm 밀린 진짜 표면**. ⑦~⑧에서 관찰된 dense의 void floater 문제, PCA/FSC 실패까지 전부 이 오프셋 하나로 설명됨.

**⑨-4. Anchored union 파이프라인 (GT-free)**

`anchored_union.py`: dense 점은 "sparse 점 반경 r 이내"일 때만 채택(r = sparse median-NN × 8, 씬 적응형) 후 sparse와 union.
- 정합 전 채택률 0.7% → **정합 후 48.8%** (오프셋 진단의 또다른 증거)
- union 결과: total 349만, notch **9,731**, void 753(≈sparse 자체 몫 697+dense 56)

**⑨-5. union+ICP prior GS-2M 결과 — 정량 신기록 + 단, 메시 파편화 발견 (2026-07-11)**

| 지표 | 기존 최고 | unionicp_default | **unionicp_D2** |
|---|---|---|---|
| **full CD** | 2.72cm (retr_k1.4) | 2.36cm | **2.27cm (-17%, 신기록)** |
| full F@5cm | — | 0.783 | **0.802** |
| box CD | 3.67cm | 2.84cm | **2.73cm** |
| box F@5cm(실측) | 0.517 | 0.705 | **0.728** |
| **void 메시 점** | 107(최선) | 531 | **3 (사실상 0)** |

- notch CD는 18~21cm로 커 보이나 **지표 함정**: 그림자 속 바닥면이 처음으로 복원되며 pred 점이 370→2,450개로 늘었는데, GT는 큐브 표면만 있고 바닥이 없어 올바른 표면이 "먼 점"으로 계산됨(§④ CD 부적합 결론의 재현). void≈0이 진짜 신호.
- **문제 발견**: 가우시안 598만 과증식(prior 349만점 → 통상의 3배), TSDF 메시 30만 클러스터로 파편화 → 1-cluster 후처리가 96% 삭제(400만→17만 vertex). 살아남은 최대 클러스터가 박스+주변이라 지표는 좋지만 배경이 사라짐. 좋은 지표에 "배경 삭제" 효과 일부 포함됨을 유의.
- 30-cluster 재추출(`tsdf_post_D2_c30.ply`): CD 2.26 유지, 배경 일부 복원(36만 vertex).

**⑨-6. 파편화 대응 실험 2건 — prior 다운샘플 실패, TSDF 굵게도 실패 (2026-07-11)**

가설 검증 결과 둘 다 기각:
1. **prior 다운샘플(349만→199만, notch 5,101 유지) 재학습**: 가우시안 **629만으로 오히려 증가**(밀도가 원인 아님 확정 — densification 동역학이 원인), 전 지표 악화(full CD 2.79~2.94, void 830~1,109). 그림자 초기화만 약화시켜 순손해.
2. **TSDF voxel 2배 굵게 재추출(D3: trunc 4×, D4: trunc 2×)**: 클러스터 9만/6.3만으로 여전히 파편화, 지표는 D2 수준(2.29~2.37), void는 D2보다 나쁨(395~471). 추출 단계 조정 한계 확인.

→ **확정 최고: `unionicp_D2`** (full CD 2.27, void 3). 파편화의 남은 정공법 = **densification 자체 억제**.

**⑨-7. 마지막 2건 — densify 억제 기각, clahe×MVS 프로브 기각 (2026-07-11)**

3. **`densify_grad_threshold` 2배(의도적 변경) 재학습**: 가우시안 598만→382만 감소는 의도대로 됐으나 파편화 여전(55만 클러스터), 지표 전면 악화(full CD 2.79~2.86, void 919~1,091). 용량 부족으로 표면 품질만 하락 → 기각. **unionicp 원본 런이 3연속 방어** — 파편화 "수정" 시도(prior 다운샘플·TSDF 굵게·densify 억제) 전부가 오히려 악화. 파편화는 배경 한정 문제로 두고 수용.
4. **clahe 이미지 × MVS 프로브**(GS 학습 없이 prior만 비교): clahe-MVS+ICP notch 9,817 / void 137 vs raw-MVS+ICP notch 9,961 / void 56 — **raw가 동급 커버리지 + 더 깨끗**. 계통 오프셋도 clahe에 동일 존재(|t|≈0.052). MVS 단계에서 raw가 이미 notch를 포화시켜 clahe 이득 없음 → GS 학습 생략하고 기각. (clahe의 가치는 sparse-prior-only 파이프라인의 photometric 품질에 한정)

**⑨-8. 육안 판정: unionicp 전체 불합격 → double-shell 가설 → dense-우선 union 재실험 (2026-07-11)**

사용자 CloudCompare 판정:
- `raw_baseline`·`dense_D2_trunc2x`: 금간 결손 (각각 그림자 점부족 / trunc½ 트레이드오프 — 예상 내)
- `dense_raw`: 형태 이상 (20cm 오프셋 미교정 — 예상 내, ICP 발견과 일치)
- **`unionicp` 3종 전부: "싹다 손상"** — 파편화가 배경뿐 아니라 박스까지 침범. **정량 신기록(CD 2.27)이 시각적으로 무의미 → §④(CD≠시각품질)의 재확인. unionicp 시각 기준 불합격.**

**double-shell 가설**: 순수 dense prior(dense_raw)는 메시 온전(250만 vertex 생존, 육안 "표면 좋음")했는데 union만 파편화 →
sparse층+dense층이 ICP 잔차(~2cm)만큼 어긋난 **이중 껍질**로 같은 표면을 덮어 TSDF가 두 껍질 사이에서 간섭·분열.

**대응 1 — dense-우선 union (기각)**: union 방향 반전(dense_icp 427만 + dense에서 먼 sparse 78만 = 단일 껍질 527만점) 재학습 → **여전히 파편화**(45만 클러스터, 가우시안 634만), void 1,885~2,381로 악화. **double-shell 가설 기각**.

**⑨-9. 파편화의 진짜 판별 변수 = 최종 가우시안 수 (2026-07-11)**

| 런 | 최종 가우시안 | 파편화 | 비고 |
|---|---|---|---|
| raw_baseline | 96만 | 없음 | |
| dense_opred | 130만 | 없음 | **육안 합격("표면 좋음")** |
| dense_raw | 168만 | 없음 | init 450만→168만 (오프셋 탓 표면 밖 점 자동 prune) |
| dgt2x | 382만 | 파편화 | |
| unionicp/unionds/densefirst | 598~634만 | 파편화 | ICP 정합 후엔 점들이 표면 위라 생존→과증식 |

**역설 발견**: 20cm 오프셋이 사실상 "자연 prune" 역할을 했음 — 정합하면 prior가 다 살아남아 가우시안이 4~6배 불어나고, 17장 nadir 씬에서 600만 가우시안은 TSDF 파편화를 유발. **경계는 약 170만~380만 사이**.

**⑨-10. 해결: denseicp(정합 prior + opacity_reduce) — 온전 메시 중 전 지표 최고 (2026-07-11)** ⭐

육안 합격 레시피(dense_opred: 30k + `use_opacity_reduce`)에서 prior만 ICP 정합본(`fused_ds_icp.ply`, 450만점)으로 교체한 단일변수 실험:
- **가우시안 101만** (opacity_reduce가 과증식 억제 — 예상 적중), **파편화 없음**(322만 중 266만 vertex 생존)

| 지표 | raw_baseline | dense_opred | **denseicp_D2** |
|---|---|---|---|
| full CD | 2.80cm | 2.77cm | **2.70cm** |
| box CD | 3.78cm | 3.70cm | **3.65cm** |
| notch CD | 8.30cm | 8.18cm | **8.00cm** |
| notch F@5cm | 0.358 | 0.374 | **0.400** |
| void 점 | **107** | 186 | 177~188 |

**온전한(비파편화) 메시 중 전 지표 최고.** 시각 검증 대기 (`4m_old_results/1_final/4m_old_denseicp_{default,D2}.ply`).

**⑨-11. Retinex(MSR) 전처리 프로브 — 기각, 전처리 축 종결 (2026-07-12)**

CLAHE가 단조 대비변환이라 NCC 매칭에 불변인 점을 지적, 비단조 변환인 Retinex(MSR 3-scale, σ=15/80/250)로 마지막 시도. GS 학습 없이 2단 프로브만:

| 프로브 | retinex | raw 기준 | 판정 |
|---|---|---|---|
| ① MASt3R prior notch/void | 1,621/661 | 1,750/697 | 짐 |
| ② MVS+ICP notch/void | 8,789/19 | 9,961/56 | notch -12%로 짐 |

→ **기각 (GS 학습 생략)**. **전처리 축 완전 종결**: edge/gamma/clahe/retinex 4종 전부 기하에서 raw 패배.
비단조 변환마저 진 것은 MASt3R·MVS가 그림자 속 가용 신호를 이미 소진하고 있다는 뜻 — **"원본이 최선"이 이 데이터셋의 결론**.
산출물: `rgb_retinex/`(서버), `preprocess_retinex.py`, 바탕화면 `6_전처리_이미지/4m_old_prep_retinex_008.png`.

**⑨-12. MASt3R 입력 해상도 스윕 — 768 승리, sparse 천장을 올린 첫 레버 (2026-07-12)** ⭐

`--image_size` 512(기본)/768/1024 프로브 (retrieval-20-5, shared_intrinsics 동일):

| 해상도 | total | notch | void | ATE |
|---|---|---|---|---|
| 512 | 129만 | 1,750 | 697 | 1.90cm |
| **768** | **222만** | **2,945 (+68%)** | 774(비례 이하) | **1.81cm (개선)** |
| 1024 | 32만 | 0 | 0 | **501cm (포즈 붕괴)** |

- 512 학습 모델이지만 768은 허용 범위 — notch +68% & ATE 개선. **1024는 분포 밖 → 완전 붕괴** (스위트스팟 = 768)
- 전처리 4종이 모두 실패한 것과 대조: 그림자 신호를 늘리는 건 이미지 변형이 아니라 **입력 해상도**였음
- focal 스케일백 일반화: `build_colmap_4mold_resbase.py` (base 해상도 argv[4], 768 → ×2.5 = 968.0px)

**⑨-13. RoPE 보간으로 1024 부활 + res768 전체 파이프라인 결과 (2026-07-13)**

**(a) RoPE position interpolation (naver/dust3r#62)**: 1024 붕괴 원인 = CroCo ViT의 RoPE 위치인코딩이 학습범위(512) 2배 밖에서 외삽 실패(LLM context 초과와 동일 현상). `pos_embed.py`의 `t`에 `ROPE_INTERP_SCALE`(=512/입력, 환경변수, 기본 1.0) 곱하는 패치 적용:

| | 768 | 1024 원본 | **1024+RoPE(0.5)** |
|---|---|---|---|
| notch | 2,945 | 0 | **4,733** |
| void | **774** | 0 | 1,554 |
| ATE | **1.81cm** | 501cm | 1.96cm |

→ 붕괴 완전 해소, notch 최다. 단 **denseicp 파이프라인에서 MASt3R의 역할은 포즈+focal+ICP타깃뿐**(기하는 1920px MVS가 생성)이라 승부처는 ATE = **768이 파이프라인용 확정**. 1024+RoPE는 sparse prior를 직접 쓰는 용례에서 가치.

**(b) res768 denseicp 전체 파이프라인 결과**:

| 지표 | denseicp512 | **denseicp768_D2** |
|---|---|---|
| **notch CD** | 8.00cm | **7.25cm (역대 최고)** |
| **notch F@5cm** | 0.400 | **0.440 (역대 최고)** |
| box F@1cm | 0.257 | **0.315** |
| box CD | 3.65cm | 3.60cm |
| void | 177~188 | 186~241 |
| full CD | **2.70cm** | 3.35cm (후퇴) |
| 가우시안 | 101만 | 83만 (파편화 없음 ✅) |

- 타깃(그림자 요철)은 768이 전 지표 갱신. full CD 후퇴 원인 추정: 768 런의 MVS 계통 오프셋이 더 컸음(ICP fitness 0.999→0.951, |t|≈0.31 colmap) → 배경 정합 잔차.
- **육안 판정 대기**: `1_final/4m_old_denseicp768_{default,D2}.ply` vs `4m_old_denseicp_{default,D2}.ply`(512).

**최종 파이프라인(보편, GT-free)**: raw 이미지 → MASt3R sparse(retrieval-20-5, shared_intrinsics) → COLMAP dense MVS(`__all__`, depth범위 자동, consistency graph, min_num_pixels=1) → **dense→sparse ICP 정합** → voxel 다운샘플(450만) → GS-2M 30k + **`--use_opacity_reduce`** → TSDF(D2: sdf_trunc=2×voxel).
핵심 교훈 3가지: ① MVS 점구름은 sparse와 계통 오프셋이 있을 수 있다(반드시 ICP) ② 정합 후엔 prior가 다 살아남아 가우시안이 과증식한다(opacity_reduce 필수) ③ sparse+dense 혼합 union은 이득이 없었다(순수 dense가 더 깨끗).

**바탕화면 산출물**: `4m_old_unionicp_default.ply`, `4m_old_unionicp_D2.ply`, `4m_old_unionicp_D2_c30.ply` (+기존 `4m_old_dense_D2_trunc2x.ply`).

**확립된 보편 파이프라인(요약)**: MASt3R sparse → COLMAP dense MVS(`__all__` cfg, depth범위 자동, consistency graph on, min_num_pixels=1) → **dense→sparse ICP 정합** → **anchored union**(r=8×median-NN) → GS-2M(동일 파라미터) → TSDF(D2: sdf_trunc=2×voxel). GT 사용 단계 없음.

---


## GS-2M 원본 철학 검증: Prior 있는 버전 vs 없는 버전 비교 (2026-07-14~19)

### ⓐ 배경: GS-2M 논문 확인 후 설계 철학 재정의

**발견:**
- GS-2M = "Material-aware Gaussian Splatting" (Eurographics 2026, Nguyen et al.)
- **핵심**: "completely independent of priors from pre-trained models"
- 의도: Multi-view photometric consistency **만**으로 기하를 학습

### ⓑ 현재까지의 작업 (Prior 있는 버전) — 진행 중

| 항목 | 구성 | 상태 |
|---|---|---|
| **포즈** | MASt3R-SfM res768 (retrieval-20-5, shared_intrinsics) | ✅ |
| **Prior (3D 포인트)** | MASt3R sparse 또는 Dense MVS (ICP 정합) | ✅ |
| **학습** | GS-2M standard (--iterations 30000, --use_opacity_reduce) | 진행 중 |
| **출력** | `real_test_4m_old__mast3r_res768__gs2m_origin__result` (dense prior 포함) | 대기 |

**명시**: 현재 파이프라인 **prior 의존적**. GS-2M 논문 철학과 다름.

### ⓒ 환경 분리 원칙 (2026-07-14 확정) ⭐

**문제 발견**: prior-free를 돌리려면 `prune_init_points`(기본값 `True`, 초기 SfM 점 중 outlier를 실제로 삭제하는 동작)를 코드 레벨에서 `False`로 바꿔야 함 — CLI 플래그로 끌 수 없는 `store_true` 인자라 코드 수정이 강제됨.
이 수정은 단순 에러 회피가 아니라 **prior 있는 버전의 학습 동작 자체를 바꾸는 변경**이라, 같은 디렉토리에서 공유하면 "학습파라미터 동일 유지" 원칙이 깨짐 (기존 dense_icp768 결과는 `True`로 학습됨).

**결론: 서버에 존재하는 두 GS-2M 디렉토리를 역할별로 완전히 분리해서 사용한다.**

| 디렉토리 | 역할 | 코드 상태 | 비고 |
|---|---|---|---|
| **`~/Desktop/models/GS-2M`** | **Prior 있는 버전 전용** | 원본 유지 (`prune_init_points=True`). `pbr/light.py`의 `sys.path.insert` 1줄만 예외 허용(동작에 영향 없는 import 경로 수정, LD_LIBRARY_PATH 트랩과 무관하게 필요) | 기존 dense_icp768, sparse 등 모든 prior 실험이 이 디렉토리 기준 |
| **`~/Desktop/models/GS-2M-v2-algo`** | **Prior 없는 버전(원본 철학 검증) 전용** | 자유롭게 수정 (`prune_init_points=False` 등 prior-free 대응 패치를 여기에만 적용) | git 상태 깨끗한 별도 clone. 여기서 무엇을 바꾸든 prior 버전에 영향 없음 |

**실행 환경 (두 디렉토리 공통)**:
```bash
source ~/Desktop/venvs/venv-milo/bin/activate   # diff_gaussian_rasterization 정상 동작 확인됨 (Python 3.10)
unset LD_LIBRARY_PATH                            # LD_LIBRARY_PATH 트랩 방지 (pipeline_strategy_3branches.md 참고)
export CUDA_HOME=/usr/local/cuda-12.3
```
> venv-milo가 GS-2M CUDA 확장과 호환됨을 최초 확인 (2026-07-14). GS-2M 원래 환경(`~/.local/lib/python3.8`)은 diff_gaussian_rasterization undefined symbol 에러로 사용 불가.

### ⓓ 새로운 계획: Prior 없는 버전 (GS-2M-v2-algo에서 진행)

**목표:**
- MASt3R-SfM 포즈 + 빈 point cloud (prior 없음)로 GS-2M 학습
- GS-2M 논문의 원래 설계대로 작동하는지 검증

**기술 진행 (모두 GS-2M-v2-algo 디렉토리 내에서만):**
1. ✅ MASt3R → COLMAP 형식 변환 (`build_colmap_4mold_sharedfix.py`)
2. ✅ points3D.txt 제거 (prior 없음 상태, 헤더만 남긴 빈 파일)
3. 🔧 `pbr/light.py` import 경로 수정 (render_utils submodule 인식 안 됨)
4. 🔧 `prune_init_points` False 처리 (포인트 0개 시 크래시 방지)
5. 🔧 `_populate_neighbor_cameras` 등 이후 단계에서 포인트 0개 처리 계속 확인 필요

**실험 설계:**
```
Input: MASt3R-SfM res768 포즈 + 빈 point cloud
Repo: ~/Desktop/models/GS-2M-v2-algo (prior-free 전용, 자유 수정)
Output: real_test_4m_old__mast3r_res768__gs2m_v2algo_nopior
Comparison:
  - Prior 있는 버전(GS-2M, 원본 코드) vs Prior 없는 버전(GS-2M-v2-algo, 패치됨)
  - CD, notch CD, void points 정량 비교
  - 육안 품질 (CloudCompare)
```

**진행 로그 (GS-2M-v2-algo, 순차 해결)**:
1. ✅ `pbr/light.py`: `render_utils` submodule이 PYTHONPATH에 없어 `ModuleNotFoundError` → GS-2M와 동일하게 `sys.path.insert(...)` 1줄 추가
2. ✅ `arguments/__init__.py`: `prune_init_points=True`(기본값)가 포인트 0개에서 `torch.max(empty tensor)` 에러 → `False`로 변경 (이 파일은 v2-algo 전용이라 prior 버전에 영향 없음)
3. ✅ **`scene/dataset_readers.py`의 `readColmapSceneInfo`**: `points3D.txt`가 비어있을 때 fallback이 전혀 없어 `create_from_pcd`의 `distCUDA2` 커널이 0-size 텐서로 "CUDA invalid configuration argument" 발생 (에러 자체는 나중 `torch.stack` 호출에서 비동기적으로 보고됨, 오해하기 쉬움) → **패치**: 코드에 이미 있던 synthetic(Blender) 씬용 랜덤 초기화 로직(100,000 pts, 250~263행)과 동일한 패턴을, COLMAP 씬 로더에도 추가. `nerf_normalization["radius"]`(카메라 배치 기준 씬 크기)를 사용해 씬에 맞는 범위로 랜덤 점 생성. → **결론: GS-2M(3DGS 계열)은 구조적으로 최소한의 point 초기화가 필요함 — 논문의 "prior-independent"는 depth/normal supervision에 pretrained model을 안 쓴다는 의미이지, point cloud initialization 자체를 생략할 수 있다는 뜻이 아님.** 랜덤 초기화 100,000 pts로 대체.
4. 🔧 **현재 막힌 지점**: `GaussianRasterizationSettings.__new__() got an unexpected keyword argument 'feature_count'`
   - 원인: 지금 쓰던 `venv-milo`에는 **MILo 전용** `diff_gaussian_rasterization`(구버전, `feature_count` 미지원)이 설치돼 있음. GS-2M은 자체 `submodules/diff-gaussian-rasterization`(feature_count 지원 커스텀 CUDA 렌더러)이 필요.
   - venv-milo에서 바로 재설치하면 MILo 환경이 깨짐 (환경 분리 원칙 위반) → **GS-2M 전용 새 venv 필요**

### ⓔ venv-gs2m 신규 구축 계획 (2026-07-14, 진행 중)

**배경**: GS-2M 공식 `environment.yml`은 Python 3.9 + torch 2.0.1+cu118을 요구하지만, 서버에는 **CUDA 11.8도 Python 3.9도 없음** (CUDA 12.3만 존재, `venv-*`는 전부 Python 3.10). 이 프로젝트의 기존 venv 전부가 원 논문 스펙 대신 **CUDA 12.3 + torch 2.1.2+cu121**로 적응해서 빌드해온 선례(`pipeline_strategy_3branches.md`)를 그대로 따름.

**빌드 절차 (venv-milo/venv-meshsplat10과 동일 패턴)**:
```bash
python3 -m venv ~/Desktop/venvs/venv-gs2m
source ~/Desktop/venvs/venv-gs2m/bin/activate
unset LD_LIBRARY_PATH
export CUDA_HOME=/usr/local/cuda-12.3
export CUDACXX=/usr/local/cuda-12.3/bin/nvcc
export CPATH=/usr/local/cuda-12.3/include

pip install torch==2.1.2 torchvision==0.16.2 --index-url https://download.pytorch.org/whl/cu121
pip install "numpy<2" plyfile tqdm opencv-python matplotlib scikit-image scikit-learn trimesh \
    tensorboard einops kornia timm huggingface_hub open3d

cd ~/Desktop/models/GS-2M-v2-algo
pip install submodules/diff-gaussian-rasterization --no-build-isolation
pip install submodules/simple-knn --no-build-isolation
pip install submodules/render-utils --no-build-isolation
pip install submodules/fused-ssim --no-build-isolation
pip install submodules/nvdiffrast --no-build-isolation
```
- `_lzma.so` 누락 이슈 발생 시 conda `recon3d`에서 복사 (기존 공통 이슈, `pipeline_strategy_3branches.md` 참고)
- **이 venv는 GS-2M(prior 버전)과 GS-2M-v2-algo(prior-free) 둘 다 공용으로 사용 가능** — MILo 등 다른 프로젝트 venv와 완전히 분리되어 오염 위험 없음

**용도**: 이후 GS-2M 계열 모든 학습(prior 있음/없음 모두)은 `venv-gs2m` 사용 권장. `venv-milo`는 다시 MILo 전용으로 원복(이미 손대지 않음, 현재 상태 그대로 안전).

**venv-gs2m 빌드 완료 (2026-07-14 16:44)** — Python 3.10 + torch 2.1.2+cu121 + CUDA 12.3로 5개 submodule 전부 빌드 성공(`diff_gaussian_rasterization`이 `feature_count` 인자 지원하는 GS-2M 전용 버전으로 확인됨).

### ⓕ Prior-free 학습 시작 (2026-07-14 16:44, 진행 중)

```bash
source ~/Desktop/venvs/venv-gs2m/bin/activate
unset LD_LIBRARY_PATH
export CUDA_HOME=/usr/local/cuda-12.3
cd ~/Desktop/models/GS-2M-v2-algo
python3 train.py \
  -s ~/Desktop/data/experiments/real_test_4m_old__mast3r_res768__gs2m_origin \
  -m ~/Desktop/data/experiments/real_test_4m_old__mast3r_res768__gs2m_v2algo_nopior \
  --iterations 30000 --port 6039
```
- Loss 7.04 → 3.13로 정상 감소, 학습 루프 안정적으로 도는 것 확인 (2026-07-14 16:44 기준 260/30000 iter)
- 로그: `~/Desktop/logs/gs2m_v2algo_nopior.log`
- 예상 완료: ~17:05 (약 20분 소요 예상, 30k iter 기준)

**비교 예정 (완료 후)**:
| | 포즈 | Prior | Repo/venv | full CD |
|---|---|---|---|---|
| dense_icp768 (기존 최고, 온전 메시) | MASt3R res768 | Dense MVS+ICP | GS-2M / (원래 python3.8 env) | 2.70cm |
| **v2algo_nopior (신규)** | MASt3R res768 | ❌ 없음 (랜덤 100k init) | GS-2M-v2-algo / venv-gs2m | 측정 예정 |

### ⓖ 관찰: Prior 유무에 따른 학습 속도/densification 패턴 차이 (2026-07-14 17:06)

**진행 상황 체크 (9,940/30,000 iter, 33%, 21분 46초 경과 시점)**:
| 항목 | 값 |
|---|---|
| Loss | 0.208~0.221 (안정적, 초기 7.04에서 크게 감소) |
| Points | 100,000 → **4,766,922**로 급증 (densification) |
| 속도 | 3.24 → 3.05 it/s로 계속 저하 |
| 남은 예상 시간 | 약 1시간 50분 (총 완료 ~18:33, 애초 예상 17:05보다 크게 늦어짐) |

**해석 — prior 있음/없음의 구조적 차이**:
- `dense_icp768`(prior 있음) 런은 시작부터 ICP로 정렬된 강한 prior(수백만 개의, 이미 표면 근처에 위치한 점)를 가지고 시작 → densification이 이미 있는 점을 다듬는 정도로 상대적으로 가벼움
- 이번 prior-free 런은 씬 전체에 균일하게 뿌려진 랜덤 100k점에서 출발 → multi-view geometry loss(`Lgeo`)만으로 "어디에 표면이 있는지"부터 찾아나가야 하므로, densification이 훨씬 공격적으로 점을 늘림(100k → 470만, 47배)
- 포인트 수가 늘수록 rasterization 비용이 커져 iteration 속도가 지속적으로 저하(3.24→3.05 it/s) — **prior 없음은 정확도뿐 아니라 학습 시간 비용도 훨씬 크다**는 것을 시사
- 이는 GS-2M(3DGS 계열)이 "point cloud initialization을 아예 생략할 수 있다"는 게 아니라 "어떤 형태로든 초기 점이 필요하고, 그 초기 점의 질이 densification 부담과 직결된다"는 ⓒ의 결론(구조적으로 최소 point 초기화가 필요함)을 시간 비용 측면에서도 뒷받침하는 정황 증거

### ⓗ 학습 완료 + 메시 추출 + 최종 비교 (2026-07-14 19:04~19:35) ⭐ 결론

**학습 완료**: 30,000/30,000 iter, 총 소요 **2시간 18분 43초** (16:44:12 → 19:02:57), 최종 PSNR 35.60, L1 0.0110, 가우시안 5,287,089개.

**메시 추출 (dense_icp768과 완전히 동일한 파라미터로, `run_res768_full.sh` 재사용)**:
```bash
render.py --extract_mesh --skip_test                                    # default (voxel/sdf_trunc 자동)
render.py --extract_mesh --skip_test --voxel_size 0.00381 --sdf_trunc 0.00762   # D2
```

| 지표 | default | D2 |
|---|---|---|
| TSDF raw mesh vertex | 2,171,947 | 1,802,122 |
| 클러스터 수 | 162,462 | 155,619 |
| **1-cluster 후처리 생존 vertex** | 160,316 (7.4%) | 47,585 (2.6%) |

**⚠️ 파편화가 기존 unionicp 사례(§9-5, ~96% 삭제)보다 더 심각** — 가우시안 528만 개는 STATUS.md에 기록된 파편화 임계값(170만~380만) 대비 훨씬 큼.

**결정적 확인 (box crop 진단)**: `eval_mesh_boxcrop.py`로 GT 박스 영역 크롭 시 post-processed 메시(default/D2 둘 다) **박스 영역 point 0개(0.0%)**. 원인 추적을 위해 1-cluster 후처리 **이전** 전체 메시(`tsdf_mesh.ply`, 180만 vertex)에 동일 크롭 적용 → **383/200,000 (0.19%) 는 박스 영역에 존재**함을 확인.
→ **결론: 학습 자체는 박스 형태를 어느 정도 잡았으나(전체 메시엔 흔적 존재), "가장 큰 단일 연결 클러스터"를 고르는 1-cluster 후처리에서 박스가 아닌 다른(배경 추정) 조각이 선택되어 최종 메시에서 박스가 통째로 소실됨.** 이는 §9-8의 "정량 신기록이 시각적으로 무의미"보다 더 심각한 실패 모드 — 박스가 아예 존재하지 않음.

**최종 정량 비교 (동일 소스: MASt3R-SfM res768, retrieval-20-5, poses.npy 공유)**:

| 지표 | **denseicp768 (prior 있음)** | **v2algo_nopior (prior 없음)** |
|---|---|---|
| 학습 시간 | 30k iter (opacity_reduce 포함) | 30k iter, **2h18m43s** (prior 있는 쪽보다 densification 부담 커 훨씬 느릴 것으로 추정, 직접 시간 비교는 미기록) |
| 최종 가우시안 수 | 1,010,000 (opacity_reduce로 억제됨) | **5,287,089** (5.2배, 억제 수단 없음) |
| 파편화 | 없음 (322만 중 266만 vertex 생존) | **심각** (7.4%/2.6% 생존, 박스 자체가 최대 클러스터에서 누락) |
| full CD (default) | 2.77cm | 측정 불가 (박스 crop 0점) |
| full CD (D2) | **2.70cm** | 측정 불가 (박스 crop 0점) |
| box CD | 3.65~3.70cm | **nan** (crop 0/200,000) |
| notch CD | 8.00~8.18cm | **측정 불가** (notch crop도 pred=0) |
| void 점 | 177~188 | N/A (박스 자체가 없어 무의미) |

### ⓘ 최종 결론 — "GS-2M은 prior가 필요없다"는 논문 주장에 대한 검증 결과

1. **구조적 요구사항 확인 (ⓒ)**: GS-2M(PGSR/3DGS 계열)은 **point cloud initialization 자체는 반드시 필요**함(0점이면 `distCUDA2` 커널이 크래시). 논문의 "prior-independent"는 **depth/normal supervision에 pretrained model을 쓰지 않는다**는 의미였고, "point cloud init을 생략할 수 있다"는 뜻이 아니었음 — 랜덤 100k 점으로 대체하면 크래시는 피할 수 있음.
2. **실전 성능 검증 (ⓗ)**: 랜덤 초기화만으로도 **photometric 품질(PSNR 35.60)은 우수**했으나, **기하학적 결과는 실패**함 — densification이 억제 없이 폭주(100k→528만)해 TSDF 추출 시 심각한 파편화가 발생했고, 관심 대상(박스)이 "가장 큰 클러스터"에서 탈락해 최종 메시에서 완전히 사라짐.
3. **이 데이터셋(17장 nadir, 저텍스처, 그림자 요철)에서의 실질 결론**: **prior(sparse MASt3R 점 또는 dense MVS+ICP)가 사실상 densification을 억제하는 정규화 역할을 함.** prior 없이는 opacity_reduce 같은 장치 없이 가우시안이 과증식하고, 그 결과가 §9-9에서 이미 규명한 "가우시안 170만~380만 초과 시 파편화" 임계값을 완전히 벗어나 실패함.
→ **논문 주장(구조적으로 pretrained-model prior 불필요)은 참이지만, 이 프로젝트의 실전 데이터(sparse-view, 저텍스처, 그림자)에서는 prior가 densification을 억제하는 사실상 필수적인 정규화 장치로 기능함.** "prior 없이도 된다"와 "prior 없이도 잘 된다"는 다른 명제였음.

**산출물**: 서버 `real_test_4m_old__mast3r_res768__gs2m_v2algo_nopior/train/ours_30000/mesh/{tsdf_mesh,tsdf_post_default,tsdf_post_D2}.ply`

### ⓙ ⚠️ 사용자 재검증 요청 (2026-07-14 23:27) — "진짜 prior 유무로만 학습한 게 맞는지" → **교란변수 2개 발견, ⓗ⓲ 결론 무효**

ⓗ의 "prior 없으면 densification 폭주로 박스 소실" 결론을 신뢰하기 전에, denseicp768(prior 있음)과 v2algo_nopior(prior 없음)이 **정말 prior 하나만 다른지** 항목별로 직접 대조.

**대조 결과**:

| 변수 | denseicp768 (prior 있음) | nopior768 (prior 없음) | 동일? |
|---|---|---|---|
| 이미지 (md5sum) | `6b8804bf...` | `6b8804bf...` | ✅ 동일 |
| 카메라 포즈 (images.txt) | 동일 MASt3R run | 오차 ~1e-8 (기록 정밀도 수준) | ✅ 사실상 동일 |
| **focal** | **967.97px** | **1451.96px** | ❌ **1.5배 오류** |
| `--use_opacity_reduce` | ✅ 사용 | ❌ 미사용 | ❌ 교란변수 |
| `prune_init_points` | True(원본) | False(패치) | ❌ 경미한 교란 |
| 학습 환경 | conda `gs2m` (원본 파이프라인) | `venv-gs2m` (신규 구축) | 별개 빌드, 코드는 동등 확인됨 |

**치명적 원인 — focal 스케일백 버그 재발**:
`nopior768`은 `build_colmap_4mold_sharedfix.py`로 COLMAP 변환했는데, 이 스크립트는 `focal × (1920/512)`를 **하드코딩**함. 그러나 입력 소스(`mast3r_retr_res768`)의 `focals.npy`는 **768 스케일**이었음:
```
387.19 × 1920/512 = 1451.96px  ← nopior768이 실제로 사용한 값 (틀림)
387.19 × 1920/768 =  967.97px  ← denseicp768/colmap_res768과 정확히 일치 (맞는 값)
```
→ **FOV가 실제(~89°)보다 훨씬 좁게(~67°) 잘못 설정된 카메라로 학습됨.**

이는 §⓪에 기록된 **2026-07-05 "박스 소실" 버그(focal 스케일백 누락 → FOV 왜곡 → 기하 뭉개짐 → TSDF 파편화 → 1-cluster 후처리가 박스 삭제)와 실패 시그니처가 완전히 동일**함. PSNR이 정상으로 나온 것도(가우시안 과다 배치로 보상) 그때와 같은 패턴.

**결론**: ⓗ/ⓘ에서 내린 "prior 없이는 densification이 폭주해 실패한다"는 결론은 **prior 부재 때문인지 focal 오류 때문인지 분리되지 않은 채 내려진 것 — 신뢰 불가, 철회**. opacity_reduce 미사용도 추가 교란(denseicp768은 opacity_reduce로 가우시안 101만개로 억제됐는데 nopior768엔 이 억제 장치가 아예 없었음 — "prior 없음"의 효과와 "억제장치 없음"의 효과가 섞임).

**다음: 통제된 재실험 준비** — denseicp768과 **prior 유무만** 다르게:
1. focal을 967.97px로 수정한 COLMAP 디렉토리 재생성 (`build_colmap_4mold_resbase.py` 사용 또는 `colmap_res768`의 cameras.txt 재사용)
2. `--use_opacity_reduce` 플래그 포함 (denseicp768과 동일)
3. `prune_init_points`는 prior 없는 조건 자체가 요구하는 최소 수정이므로 유지(0점 방지 목적, 학습 동작상 영향 미미 — outlier 초기점이 애초에 없으므로)
4. venv-gs2m 환경 유지 (코드 동등성 이미 확인됨)

### ⓚ 통제 재실험 시작 (2026-07-14 23:31)

`real_test_4m_old__mast3r_res768__gs2m_nopior_fixed` 신규 생성, denseicp768과 **cameras.txt/images.txt/이미지 완전 동일**(diff 무출력으로 확인), `points3D.txt`만 빈 헤더로 차이:
```bash
python3 train.py \
  -s .../real_test_4m_old__mast3r_res768__gs2m_nopior_fixed \
  -m .../real_test_4m_old__mast3r_res768__gs2m_nopior_fixed__result \
  --iterations 30000 --port 6040 --use_opacity_reduce   # denseicp768과 동일 플래그
```
- 랜덤 100k 초기화 정상 진입, loss 6.97→하락 정상 확인
- 로그: `~/Desktop/logs/gs2m_nopior_fixed.log`
- 예상 소요: 이전 nopior 런 참고 시 ~2시간 (opacity_reduce로 densification 억제되면 더 짧을 가능성)

**이번엔 진짜 prior 유무만 다른 변수**로 통제됨 — 완료 후 denseicp768과 비교해야 ⓘ의 결론을 다시 낼 수 있음.

### ⓛ 통제 재실험 완료 + 최종 비교 (2026-07-15 02:00~17:30) — **ⓘ 결론 재확정 (교란변수 제거 후에도 동일 실패 재현)**

**학습 완료**: 30000/30000 iter, 2h27m43s, 최종 PSNR 34.34 (train), 최종 gaussian 수 **5,680,910개**.

**Mesh 추출**: `run_res768_full.sh`와 동일한 명령 (`--extract_mesh --skip_test`, D2는 `--voxel_size 0.00381 --sdf_trunc 0.00762`).

| 항목 | denseicp768 (prior 있음, 기준) | nopior768_fixed (prior 없음, 통제됨) |
|---|---|---|
| 최종 gaussian 수 | 1,010,000 | **5,680,910** (5.6배) |
| raw mesh vertices (클러스터링 전) | 파편화 없음 | default 3,754,769 / D2 2,815,621 |
| 클러스터 개수 (1-cluster 후처리) | 소수 (파편화 없음) | default **414,173개** / D2 **350,856개** |
| post-cluster vertices 생존률 | 거의 100% | default 229,887/3,754,769 (6.1%) / D2 56,132/2,815,621 (2.0%) |
| 전체 CD (eval_mesh_4m_old_v2) | 2.70cm | **평가 불가 — crop 후 0/200,000 (0%)** |
| box crop CD (10cm margin) | 3.65-3.70cm | **0/200,000 (0%) → CD=nan** |
| notch CD | 8.00-8.18cm | **gt=24677 pred=0 (부족)** |
| void points | 177-188 | **pred 부족 0** |
| raw(클러스터링 전) mesh의 box crop | — | **1,593/200,000 (0.8%), CD=3.19cm, F@3cm=0.172** ← 박스 자체는 존재 |
| ATE (Umeyama 정렬) | — | 1.81cm (정렬 자체는 정상) |

**해석**:
- focal 1.5배 오류와 `--use_opacity_reduce` 누락을 모두 수정하고, denseicp768과 **이미지·포즈·focal·학습 플래그가 완전히 동일한 통제된 조건**에서 재실험해도, **동일한 실패 패턴이 그대로 재현됨**: prior 없이 학습하면 gaussian 수가 억제되지 않고(5.68M, opacity_reduce가 있어도), TSDF mesh가 극심하게 파편화되며(35~41만 클러스터), 1-cluster 후처리 시 박스/노치가 있는 얇은 구조물 영역이 통째로 삭제됨.
- 파편화 이전(raw mesh) 단계에서는 박스 영역이 실제로 존재하고 품질도 나쁘지 않음(CD=3.19cm) — 이는 §ⓘ에서 이미 확인한 것과 동일한 진단으로, "prior 없이는 아예 그 영역을 학습 못한다"가 아니라 **"prior 없이는 노이즈성 gaussian이 과다 생성되어 TSDF 파편화를 유발하고, 그 결과 후처리 단계에서 얇은/작은 구조물이 통째로 잘려나간다"**는 메커니즘.
- **§ⓘ에서 내렸던 "prior가 필요한 정규화 메커니즘" 결론은, 교란변수(focal 오류·opacity_reduce 누락)를 모두 제거한 뒤에도 유효한 것으로 재확정됨.** GS-2M 논문의 "prior-independent" 주장은 depth/normal supervision에 한정된 것이며, point-cloud 초기화 없이(랜덤 초기화만으로) 학습 시 이 특정 씬(작은 박스/노치가 있는 4m 시뮬레이션 데이터)에서는 densification이 과다해지고 TSDF 파편화로 인한 세부 구조 소실이 실제로 발생함이 통제된 실험으로 확인됨.

**다운로드**: `/mnt/c/Users/sdh97/Desktop/4m_old_results/1_final/4m_old_nopior768_fixed_{default,D2,raw_fragmented}.ply` (기존 미검증 `4m_old_nopior768_{default,D2,raw_fragmented}.ply`는 교란변수 포함된 버전이므로 참고용으로만 유지, 결론은 `_fixed` 버전 기준).

### ⓜ 실패 메커니즘 정밀 조사 (2026-07-15 18:00) — gaussian 분포·클러스터 구조·densification 궤적 분석

30k 체크포인트 `point_cloud.ply` 직접 분석 (plyfile) + raw mesh 클러스터 분석 (open3d) + 로그 타임라인 추출로 "왜 파편화되는가"를 정량 규명.

**① Densification 궤적 — 두 런은 정반대 방향으로 수렴:**

| iter | denseicp768 (prior) | nopior_fixed (no prior) |
|---|---|---|
| 0 | **7,974,199** (dense prior 그대로) | 100,000 (랜덤) |
| 4,000 | 1,378,909 (프루닝 중) | 1,656,361 (폭증 중) |
| 6,000 | 1,221,895 | **7,810,576 (피크)** |
| 16,000~30,000 | **825,731 (고정)** | **5,680,910 (고정)** |

- prior 런은 "정확한 800만 점에서 시작 → 잉여分 프루닝 → 83만"으로 **수렴**
- no-prior 런은 "랜덤 10만 → iter 2k~6k에 78배 폭증 → 568만"으로 **발산 후 고착**. opacity_reduce가 있어도 못 막음

**② Gaussian 형태 — no-prior는 "점묘화(pointillism)":**

| 지표 | denseicp768 | nopior_fixed |
|---|---|---|
| max_scale 중앙값 | 0.0060 | **0.0017 (3.5배 작음)** |
| max_scale p95 | 0.0344 | 0.0062 |
| 극소형(<0.001) 비율 | 4.2% | **16.2%** |
| opacity<0.1 비율 | 12.7% | 28.4% |
| NaN gaussian | 0개 | **356개** (수치 불안정 흔적) |

→ prior가 있으면 표면 위에서 태어난 gaussian이 **크고 평평하게 자라 표면을 연속적으로 덮음**. prior가 없으면 랜덤 위치에서 photometric loss를 맞추려 **쪼개고 줄이기를 반복 → 수백만 개 미세 반점**으로 씬을 그림. 렌더된 depth가 자글자글해지고 TSDF 표면이 조각남.

**③ Raw mesh 클러스터 구조 — 바닥조차 연결되어 있지 않음:**
- 총 350,856 클러스터, **최대 클러스터가 전체 삼각형의 3.2%에 불과** (denseicp768은 최대 클러스터 ≈ 씬 전체). 즉 박스만 떨어져 나간 게 아니라 **바닥 평면 자체가 산산조각**
- 박스 영역 삼각형 32,886개(전체 1.11%)는 수백 개 클러스터에 분산, 가장 큰 박스 조각이 전역 크기순위 **#68** (3,780 tris, 다른 무엇과도 연결 안 됨)
- **top-N 클러스터 유지로도 구제 불가**: top-100 유지 시 박스 생존 19.6%, top-10,000까지 늘려도 33.6%. `--num_clusters` 조정은 해결책이 아님

**종합 결론**: prior의 역할은 "초기 위치 힌트" 이상 — **gaussian의 크기·형태·연결성을 결정하는 구조적 정규화**. 랜덤 초기화로는 기하 정보 자체는 얻어도(raw box CD 3.19cm) 표면 연결성이 파괴되어 TSDF 기반 mesh 파이프라인 전체가 무력화됨. 개선을 시도한다면 후처리가 아니라 학습 단계(densification threshold 강화, scale 정규화 항 추가, densify 시작 지연 등)를 건드려야 함.

분석 스크립트: 로컬 scratchpad `analyze_gaussians2.py`, `analyze_clusters.py` (서버 /tmp에서 실행).

### ⓝ 초정밀 비교 (2026-07-15 18:30) — **"점묘화"가 아니라 "공중 안개(fog)"였다: ⓜ 메커니즘 수정**

4종 추가 분석(A: gaussian↔GT 거리, B: depth 노이즈 정량화, C: iter7000 시점 비교, D: 시각 비교)으로 실패 메커니즘을 재규명. **ⓜ의 "표면 위 점묘화" 해석은 부분적으로 틀렸음** — 실제로는 대부분의 gaussian이 표면에 있지도 않았다.

**A. Gaussian 중심 ↔ GT 표면 거리** (Umeyama 정렬, GT = 평지 z=0 + Cube8-12):

| 지표 (iter 30000, solid op>0.5) | denseicp768 | nopior_fixed |
|---|---|---|
| 씬 내 GT까지 거리 중앙값 | 4.9cm | **305cm (3m!)** |
| <3cm 비율 | 15% | **0%** |
| floater(>20cm) 비율 | 17.8% | **99.8%** |
| 박스 영역 gaussian (개수 / 중앙값) | 3,344개 / 3.6cm | 2,614개 / 6.1cm |

→ no-prior의 gaussian 수백만 개는 표면이 아니라 **지상 1.5~3.5m 높이(카메라 4.3m 바로 아래)의 안개층**에 떠 있음. 외형(PSNR 34.3)은 이 안개가 뷰별 appearance를 과적합한 결과. **예외는 박스 영역** — 파란 박스처럼 대비가 뚜렷한 물체 위에는 gaussian이 실제로 표면 근처(중앙값 6.1cm)에 앉음. 반복 텍스처인 잔디 지면은 photometric loss만으로 기하가 유일하게 결정되지 않아 안개 해로 수렴한 것.

**B. 렌더된 depth map 국소 거칠기** (|Laplacian| 중앙값, 17뷰 평균): denseicp768 **0.06** vs nopior_fixed **0.82 (13.7배)**. 컬러맵 이미지 기준이지만 상대 비교로 유효 — 안개층의 depth가 뷰마다 제멋대로라 TSDF fusion이 일관된 표면을 만들 수 없음.

**C. iter 7000 시점**: no-prior는 이미 7000에서 floater 99.4%, 거리 중앙값 305cm — **안개 수렴은 densification 폭증기(iter 2k~6k)에 이미 완료**된 것이고 후반에 악화된 게 아님. 박스 영역 gaussian은 7000엔 412개뿐 → 30000에 2,614개로 늘어남 (박스 기하는 늦게, 부분적으로만 형성).

**D. TSDF mesh 높이 분포 + 시각 비교** (fog 가설 mesh 레벨 검증):

| mesh | 지면 근처(±30cm) | 1m 이상 상공 |
|---|---|---|
| denseicp768 post | 62.9% | 16.6% |
| nopior raw | **2.1%** | **83.7%** |
| nopior post(최대 클러스터) | **0.0%** | **99.3%** |

→ 후처리에서 살아남은 "최대 클러스터"조차 지면이 아니라 **안개 조각**. 시각 비교(`compare_000000.png`, `compare_000008.png`, 바탕화면 1_final): RGB는 양쪽 다 정상(박스 선명), depth는 prior=매끈한 그라데이션 vs no-prior=검정/보라 패치워크, normal은 prior=일관된 평면 vs no-prior=색종이 노이즈.

**최종 메커니즘 (ⓜ 수정판)**:
1. 랜덤 초기화 → densification 폭증기(2k~6k)에 **카메라 아래 안개층으로 수렴** (photometric 과적합, 잔디의 반복 텍스처가 기하 모호성 유발)
2. 대비 강한 박스만 예외적으로 표면 기하 형성 (그래서 raw mesh 박스 CD 3.19cm가 나왔던 것 — "기하를 배웠다"가 아니라 "박스만 배웠다")
3. 안개의 depth 렌더는 뷰 간 비일관 (거칠기 13.7배) → TSDF 파편화 → 최대 클러스터도 안개 조각 → 지면·박스 전부 소실
4. **결론 강화**: prior는 "정규화"를 넘어 **기하 모호 영역(무텍스처/반복 텍스처)에서 유일해를 고정하는 역할**. 이 씬에서 prior 없는 GS-2M은 mesh 파이프라인이 아니라 기하 학습 자체가 실패 (박스 제외). GS-2M 논문의 DTU 같은 데이터셋(가까운 거리, 풍부한 텍스처 변화)과 드론 잔디 씬의 차이가 핵심 변수일 가능성.

분석: `analyze_gauss_gt2.py`, `analyze_depth.py` + mesh 높이 분포 인라인 스크립트. 시각자료 2장 바탕화면 다운로드 완료.

### ⓞ 중간 조건 실험 시작 (2026-07-15 19:22) — MASt3R 네이티브 prior (ICP 없음)

3조건 비교의 빈칸 채우기: **dense MVS+ICP prior(denseicp768) / MASt3R 네이티브 prior(신규) / prior 없음(nopior_fixed)**. ICP 정합 없이 MASt3R-SfM이 포즈와 같은 좌표계로 직접 출력한 `pointcloud.ply`를 prior로 사용 — 정합 오차 유입이 없는 "순수한" prior 조건이자, 3DGS 표준 워크플로(포즈와 점이 같은 SfM 런에서 나옴)에 가장 가까운 형태.

- 기존 `gs2m_origin` 디렉토리는 **focal 1451.96 버그 보유 확인** (미검증 sharedfix 산출물) → 폐기하고 신규 구축
- `real_test_4m_old__mast3r_res768__gs2m_mast3rprior_fixed`: cameras.txt/images.txt/images를 denseicp768과 diff 무출력 확인, points3D.ply = `mast3r_retr_res768/pointcloud.ply` (85MB, 네이티브 그대로)
- 학습: 원본 GS-2M(보호 디렉토리) + conda gs2m — denseicp768과 동일 파이프라인, `--iterations 30000 --port 6041 --use_opacity_reduce`
- 초기점 2,221,226개 (setup 프루닝 2,221개), loss 0.93→하락 정상, 로그 `~/Desktop/logs/gs2m_mast3rprior_fixed.log`

이로써 prior "강도"에 따른 스펙트럼 비교 가능: dense 800만점(ICP) vs 네이티브 222만점(정합 無) vs 랜덤 10만점.

### ⓟ 중간 조건 완료 + 3조건 최종 비교 (2026-07-16 00:37~14:52) ⭐ 결론

**학습 완료**: 30,000/30,000 iter, 총 소요 **5시간 15분** (19:22:13→00:36:58), 최종 PSNR 35.23, L1 0.0116, 최종 gaussian **5,687,205개**.

**메시 추출**: default/D2 동시 실행 시 같은 파일명(`tsdf_mesh.ply`/`tsdf_post.ply`)으로 서로 덮어쓰는 사고 발생 → D2 결과를 `*_D2.ply`로 백업 후 default 재추출로 복구.

**3조건 최종 비교표** (동일 이미지·포즈·focal·`--use_opacity_reduce`, prior만 다름):

| 지표 | **denseicp768**<br>(Dense MVS+ICP, 800만→83만) | **mast3rprior_fixed**<br>(MASt3R 네이티브, 222만, ICP 無) | **nopior_fixed**<br>(prior 없음, 랜덤 10만) |
|---|---|---|---|
| 최종 gaussian 수 | 1,010,000 | **5,687,205** | 5,680,910 |
| raw mesh 클러스터 수 (default/D2) | 소수 (파편화 없음) | **432,199 / 370,462** | 414,173 / 350,856 |
| 최대 클러스터 비율 | ≈전체 | **3.3% / 2.1%** | ≈3% (동급) |
| raw mesh box crop CD | 3.65~3.70cm | **3.69cm / 2.88cm** | 3.19cm |
| post-cluster box crop | 0.8~7.4% 생존 | **0/200,000 (0%)** | 0/200,000 (0%) |
| 전체 CD (post-cluster) | 2.70~2.77cm | **평가 불가 (0%)** | 평가 불가 (0%) |
| notch CD | 8.00~8.18cm | **gt=24677 pred=0** | gt=24677 pred=0 |

**핵심 발견 — MASt3R 네이티브 prior(ICP 없음)는 "prior 없음"과 사실상 동일하게 실패함:**

1. gaussian 수(568만 vs 568만), 클러스터 파편화(43만 vs 41만), post-cluster 생존율(0% vs 0%) 모두 nopior_fixed와 **거의 동일한 수치**. denseicp768(83만 gaussian, 파편화 없음)과는 완전히 다른 양상.
2. 초기점 222만 개(랜덤 10만보다 22배 많음)를 줬음에도 densification이 억제되지 않고 568만까지 폭증 — **prior의 "점 개수"가 아니라 "dense MVS+ICP로 표면에 정합된 정도"가 densification 억제의 핵심 변수**임을 시사.
3. raw mesh 단계에서는 박스가 존재(CD 2.88~3.69cm, denseicp768과 대등하거나 더 나음)하지만 1-cluster 후처리에서 소실 — ⓘ~ⓝ에서 규명한 "안개 수렴 → TSDF 파편화 → 후처리 시 구조물 탈락" 메커니즘이 그대로 재현됨.

**"최소 요구 prior 수준" 결론**: 이 데이터셋(4m 시뮬레이션, sparse-view, 저텍스처 잔디)에서는 **SfM이 자연스럽게 뱉는 sparse pointcloud만으로는 부족**하며, **dense MVS 재구성 + ICP 정합까지 거친 강한 prior가 사실상 필수**임이 확인됨. "가벼운 prior면 충분하지 않을까"라는 가설(ⓞ의 동기)은 기각됨 — prior 스펙트럼은 이분법(있음/없음)이 아니라 "정합 품질" 축으로 봐야 하며, 이 씬에서는 그 축의 상당히 강한 지점(dense+ICP)까지 가야 실패를 면함.

**GS-2M 논문 "prior-independent" 주장에 대한 최종 정리**: 논문의 주장은 depth/normal supervision에 pretrained model을 쓰지 않는다는 의미로 참이나, point-cloud initialization 관점에서는 (a) 최소한의 점 초기화가 구조적으로 필요하고(ⓒ), (b) 이 프로젝트의 sparse-view/저텍스처 드론 데이터셋에서는 **단순 SfM 점이 아니라 dense+ICP 수준의 정합된 prior가 densification 정규화·기하 모호성 해소에 사실상 필수**임이 3조건 통제 실험으로 확정됨.

**산출물**: 서버 `real_test_4m_old__mast3r_res768__gs2m_mast3rprior_fixed__result/train/ours_30000/mesh/{tsdf_mesh,tsdf_post}_default.ply`, `{tsdf_mesh,tsdf_post}_D2.ply`

### ⓠ Prior 자체 품질 정량 비교 (2026-07-16 17:00) — "native가 더 깔끔해 보인다"는 관찰 검증 → **사실로 확인, ⓟ 해석 수정 필요**

CloudCompare 육안 비교에서 native prior가 dense+ICP보다 오히려 깔끔해 보인다는 관찰이 나와, 두 prior의 GT 표면 밀착도를 직접 측정 (`analyze_prior_quality.py`, Umeyama 정렬 후 지면 z=0·박스 GT 대비):

| 지표 | dense+ICP (797만점) | MASt3R native (222만점) |
|---|---|---|
| 지면 \|z\| 중앙값 | 2.20cm | **0.95cm (2.3배 정확)** |
| 지면 \|z\| p90 | **3.84cm** | 11.80cm |
| 지면 <10cm 비율 | **100.0%** | 89.1% |
| 지면 **>20cm floater** | **0.0%** | **7.2% (≈16만점)** |
| 박스영역 CD / recall@3cm | 6.27cm / 24.8% | **4.56cm / 55.5%** |

**→ "native가 부정확해서 실패했다"는 ⓟ의 단순 해석은 틀림.** native는 중앙값 기준으로 오히려 2배 이상 정확함. 진짜 차이는:
1. **오차 분포의 꼬리**: dense+ICP는 SOR 필터링·fusion 제약 덕에 오차가 10cm에서 **하드 바운드**(>20cm 0%). native는 중앙값은 좋지만 **heavy-tail** — 16만 개 점이 표면에서 20cm+ 이탈(안개 수렴의 씨앗 후보)
2. **밀도/커버리지**: 797만 vs 222만 (3.6배)
3. **⚠️ 추가 교란 발견**: mast3rprior 학습 로그에 `Load Ply colors and normals failed, random init...` — native ply는 색/법선 로드 실패로 랜덤 초기화됨 (denseicp768 로그엔 이 메시지 없음). GS-2M(PGSR 계열)의 평면 정규화에 초기 normal이 영향을 줄 수 있음

**3가지 후보(꼬리 outlier / 밀도 / normal 초기화) 중 무엇이 결정적인지는 미분리.** 분리 실험 옵션: (a) native prior에서 >20cm outlier만 SOR 제거 후 재학습, (b) dense prior를 222만으로 다운샘플 후 재학습, (c) native ply에 색/법선 필드 추가 후 재학습.

### ⓡ 교란 원인 확정 + 분리 실험 1차: normfix (2026-07-16 17:00 시작)

**"Load Ply failed"의 원인 확정** (`scene/dataset_readers.py:110` `fetchPly`):
- native ply(MASt3R 출력)는 `x,y,z,red,green,blue`만 있고 **`nx,ny,nz`가 없음** (dense+ICP ply는 Open3D가 normal 포함해 저장)
- `fetchPly`는 색과 normal을 **한 try 블록**에서 읽음 → normal KeyError 시 **멀쩡한 RGB까지 통째로 버리고** `np.random.rand/255`(≈검정)로 대체
- **normal 자체는 무해로 판정**: `create_from_pcd`(gaussian_model.py:181-190)는 `pcd.normals`를 아예 사용하지 않음 (GS-2M의 normal은 gaussian 회전축에서 실시간 유도) — ⓠ-3의 "normal이 평면 정규화에 영향" 추정은 철회
- **실제 교란은 색**: mast3rprior_fixed 런은 222만 점 전부 검정으로 시작 → 초기 photometric error 과대 → densification 폭증기에 잘못된 gradient 증폭 가능성

**분리 실험 (a′) normfix**: native ply에 `nx,ny,nz=0` 필드만 추가(GS-2M `storePly` 관례와 동일)한 `real_test_4m_old__mast3r_res768__gs2m_mast3rprior_normfix` 생성 — cameras.txt diff 무출력 확인, fetchPly 색 로드 검증(mean RGB 0.66/0.69/0.46 = 잔디색). 점 좌표는 mast3rprior_fixed와 완전 동일하므로 **"색 초기화"만 분리한 실험**.
- 학습 시작 17:00, 포트 6042, 동일 플래그, 로그 `~/Desktop/logs/gs2m_mast3rprior_normfix.log`
- **조기 신호**: 초기 Loss 0.35 (검정 초기화였던 mast3rprior_fixed는 0.93) — 색 로드 효과 즉시 확인됨
- **중간 경과 (iter 12,800, 43%)**: Points **792,439로 수렴 중** — mast3rprior_fixed의 568만 폭증과 정반대, denseicp768의 수렴값(825,731)과 거의 동일한 궤적. "색 초기화가 범인" 가설 쪽으로 강하게 기움
- 판정 기준: 이 런이 성공(파편화 없음)하면 "색 초기화가 범인", 실패하면 "floater 꼬리(7.2%, 16만점)가 범인" → 후속 (b′) outlier 제거 실험으로

### ⓢ normfix 결과 (2026-07-19 완료) — **성공. "색 초기화 버그가 범인" 확정, ⓟ 결론 뒤집힘**

**학습**: 30,000 iter 완료, 최종 PSNR **34.07** / L1 0.0146. Points 궤적 222만 → 79만 → **101만 수렴** (mast3rprior_fixed의 568만 폭증과 정반대, denseicp768과 동일한 가지치기 패턴).

**메시 추출**: default(자동 파라미터) → D2(voxel 0.00381/sdf_trunc 0.00762) 순차 실행. raw 클러스터 **95,608 / 100,689개** — 실패 케이스(35만~43만)의 1/4 수준이고, 1-cluster 후처리에서 **215만 vertex 본체가 생존** (실패 케이스는 박스 0% 소실).

**4조건 최종 비교** (동일 이미지·포즈·focal·플래그, points3D.ply만 다름):

| 지표 | denseicp768<br>(dense MVS+ICP) | mast3rprior_fixed<br>(native, 색버그) | **mast3rprior_normfix**<br>(native, 색 로드 OK) | nopior_fixed<br>(랜덤) |
|---|---|---|---|---|
| 최종 gaussian 수 | 1,010,000 | 5,687,205 | **1,016,549** | 5,680,910 |
| raw 클러스터 (default/D2) | 소수 | 43만/37만 | **9.6만/10.1만** | 41만/35만 |
| post-cluster box 생존 | 0.8~7.4% | 0% | **6.3~8.0%** | 0% |
| 전체 CD (post) | 2.70~2.77cm | 평가불가 | **3.34 / 3.38cm** | 평가불가 |
| F@10cm | — | — | 0.879 / 0.875 | — |
| notch CD | 8.00~8.18cm | pred=0 | **6.60 / 6.78cm (더 좋음)** | pred=0 |
| boxcrop CD (margin 10cm) | — | — | 3.58 / 3.62cm | — |

(주의: notch precision 스크립트는 pred 232~254점으로 표본 부족 → inf/nan, 판정 제외)

**결론 — ⓟ의 "dense+ICP prior 사실상 필수" 결론 철회**:
1. mast3rprior_fixed의 실패 원인은 prior 품질이 아니라 **fetchPly 색 로딩 버그**(nx,ny,nz 부재 → RGB까지 폐기 → 222만 점 전부 ≈검정 초기화)였음. 점 좌표가 완전히 동일한 normfix가 성공함으로써 분리 증명 완료.
2. **MASt3R native SfM 점군(ICP 없음)만으로도 GS-2M은 성공** — 전체 CD 3.34cm(denseicp768 2.70cm 대비 +0.6cm), notch CD는 오히려 6.60cm로 더 좋음(8.0cm 대비). native prior의 7.2% floater 꼬리(16만점)는 opacity_reduce 가지치기가 흡수 가능한 수준.
3. GS-2M "prior-independent" 검증의 최종 답: **dense MVS+ICP는 불필요. SfM이 자연스럽게 출력하는 점군이면 충분**(단, ply에 normal 필드가 있어야 색이 로드되는 fetchPly 구현 특성 주의). 랜덤 초기화(nopior)만은 여전히 실패.
4. 후속 (b′) floater 제거 실험은 불필요해짐 (범인 확정).

**산출물**: 서버 `real_test_4m_old__mast3r_res768__gs2m_mast3rprior_normfix__result/train/ours_30000/mesh/{tsdf_mesh,tsdf_post}_{default,D2}.ply`

---

---


---

# PART B. 실제 드론 데이터

## 실제 드론 촬영 데이터 (2026-06-24 추가)

- **출처**: 실제 드론으로 촬영한 영상 프레임 (시뮬레이션 아님)
- **Google Drive**: https://drive.google.com/file/d/1y_HwAsE0eA3lCimyzNVqh3dLiGPdrs-e/view?usp=sharing
- **파일**: ZIP (197MB), 총 1,656장 JPG
- **로컬**: `C:\Users\sdh97\Desktop\drone_images\{3m_1,5m_1,7m_1}\`

| 폴더 | 장수 | 설명 |
|---|---|---|
| `3m_1` | 547장 | 고도 3m 촬영 |
| `5m_1` | 544장 | 고도 5m 촬영 |
| `7m_1` | 565장 | 고도 7m 촬영 |

- **비고**: 기존 STATUS.md의 `real_test` 항목에 "시뮬레이션"으로 잘못 기재된 부분 있음. real_test 34장도 실제 드론 촬영 데이터임.

### 드론 영상 MASt3R-SfM 결과 (2026-06-24 완료)

- **서버**: sysai3, 환경 `venv-mast3r`, 스크립트 `~/Desktop/run_drone_mast3r.sh`
- **설정**: scene_graph=swin winsize=5, 10프레임마다 1장 샘플링 (~55장/폴더), shared_intrinsics

| 폴더 | 입력 장수 | 출력 | 경로 |
|---|---|---|---|
| `3m_1` | 55장 | poses.npy (55,4,4), focals.npy, pointcloud.ply | `~/Desktop/data/drone_real_sfm/3m_1/` |
| `5m_1` | 55장 | poses.npy (55,4,4), focals.npy, pointcloud.ply | `~/Desktop/data/drone_real_sfm/5m_1/` |
| `7m_1` | 57장 | poses.npy (57,4,4), focals.npy, pointcloud.ply | `~/Desktop/data/drone_real_sfm/7m_1/` |

---


## GS-2M (Eurographics 2026) 학습 + 평가 (2026-06-24)

### 학습 정보

- **입력**: `real_test` 데이터 (34장, 1920×1080), COLMAP 초기화
- **모델**: GS-2M — material-aware Gaussian Splatting + TSDF mesh extraction
- **환경**: `conda gs2m` (GCC11, CUDA11.8, NumPy<2)
- **출력 디렉토리**: `C:\Users\sdh97\Desktop\3d_results\real_test\gs2m_output\`

| 항목 | 값 |
|---|---|
| 학습 iter | 30,000 |
| L1 loss (최종) | 0.0199 |
| PSNR (최종) | 31.21 dB |
| TSDF 메시 | `train/ours_30000/mesh/tsdf_post.ply` ✅ |

### 메시 품질 평가 (재평가, Umeyama 카메라 포즈 정렬 기반)

> **평가 방법**: Umeyama 변환 (카메라 포즈 기반, scale=3.89×, ATE=2.3cm) → GT 큐브를 COLMAP 공간으로 역변환 → COLMAP 공간에서 직접 CD/F-score 계산
>
> **GT**: UE5 Cube8~12.ply (5개 큐브, X=1.235~1.587 COLMAP units)
>
> **참고**: 이전 평가(21-24cm)는 스크립트 미확인으로 재현 불가. 아래는 동일 방법으로 전 방법 재평가한 공정 비교임.

| 방법 | CD (cm) | F@1cm | F@5cm | F@10cm | 비고 |
|---|---|---|---|---|---|
| AGS baseline | **25.41** | 0.019 | 0.057 | **0.127** | ✅ CD 최저 |
| **GS-2M (ours)** | **27.79** | 0.004 | 0.029 | 0.057 | ← **NEW** |
| MILo SOR2 | 30.72 | 0.003 | 0.013 | 0.024 | — |
| MILo baseline | 37.20 | 0.003 | 0.010 | 0.018 | — |
| 2DGS | 37.25 | 0.003 | 0.019 | 0.036 | — |

> **해석**: CD 기준으로 GS-2M은 MILo/2DGS 대비 25-27% 개선. 5개 큐브 전체 씬에서 CD가 높은 것은 전체 씬 메시를 GT 소형 큐브와 비교하기 때문 (대부분의 메시 포인트가 큐브 표면과 거리가 멈).

### 스크립트

- **학습**: `/tmp/GS-2M/train.py` — `source ~/miniconda3/bin/activate gs2m && export CC=~/miniconda3/envs/gs2m/bin/gcc ...`
- **메시 추출**: `/tmp/GS-2M/render.py --extract_mesh --skip_test`
- **평가**: `/tmp/eval_gs2m.py` (Umeyama + COLMAP 공간 CD/F-score)

---


## 3m_1 드론 데이터 평가 파이프라인 설계 (2026-06-25 확정)

### 목적
실제 드론 촬영 데이터(GT 없음) → GS-2M / 2DGS / MILo 3모델 비교
- **정량**: PSNR / SSIM / LPIPS (NVS, held-out test views)
- **정성**: 메시 추출 후 시각 비교 (GT 없으므로 CD 불가)

### ❌ 첫 번째 시도 폐기 사유: focal 버그

MASt3R는 이미지를 **512px로 리사이즈**하여 처리하고 focal을 그 해상도 기준으로 저장.
변환 스크립트에서 원본 해상도(1920)로 스케일백을 누락.

| 항목 | 잘못된 값 | 올바른 값 |
|------|----------|----------|
| MASt3R 처리 해상도 | 512×288 | — |
| 저장된 focal | 359.88px | — |
| 스케일 팩터 | 누락 | 1920/512 = **×3.75** |
| 1920px 기준 focal | ❌ 359.88 (FOV 139°) | ✅ **1349.55** (FOV 71°) |

→ 카메라가 사실상 어안렌즈로 설정되어 학습 → PSNR 16dB (정상 22dB+)

### 데이터 준비 (재실행 필요)

- **소스**: sysai3 `~/Desktop/data/drone_real/3m_1/` (55장, 매 10프레임)
- **포즈**: `drone_real_sfm/3m_1/poses.npy` (55,4,4) c2w
- **COLMAP 변환 수정 사항**:
  - `focal = focals_npy[0] * (1920 / 512)` = **1349.55px**
  - cx=960, cy=540 (변경 없음)
  - images.txt: poses.npy 순서 = 이미지 정렬 순서 ✅ 확인됨
  - points3D.ply: 200k 다운샘플 (0pt 버그 수정 유지)

### 학습 설정 (확정)

| 항목 | 설정 | 비고 |
|------|------|------|
| 해상도 | **1920×1080** (코드 패치 필요) | `camera_utils.py` 1600 캡 제거 |
| eval | `--eval` (llffhold=8) | 7장 held-out test |
| iter | GS-2M/2DGS: 30k, MILo: 18k | 각자 수렴 스케줄 |
| points3D | 200k 다운샘플 PLY | |

> ⚠️ **1920 코드 패치**: GS-2M/2DGS/MILo 모두 `utils/camera_utils.py`에 `if orig_w > 1600: rescale` 하드코딩됨
> → 각 모델의 `camera_utils.py`에서 해당 분기 제거 또는 1920으로 임계값 상향 필요

### 평가 파이프라인 (3단계)

```
1. train.py --eval        → 학습 + PSNR 출력 (train 로그에서)
2. render.py              → test 뷰 렌더링
3. metrics.py             → SSIM / LPIPS 계산
```

> ⚠️ SSIM/LPIPS는 train 로그에 **안 나옴** — 반드시 render→metrics 실행 필요

### 메시 추출 방식 (모델별 상이, 정상)

| 모델 | 방식 | 실행 |
|------|------|------|
| GS-2M | depth 렌더 → TSDF fusion → Marching Cubes | `render.py --extract_mesh` |
| 2DGS | depth 렌더 → TSDF fusion → Marching Cubes | `render.py --mesh` |
| MILo | 학습된 occupancy/SDF → Marching Tetrahedra | `mesh_extract_sdf.py` |

> TSDF `voxel_size = max_depth/1024`, `sdf_trunc = 4×voxel` — 씬 스케일 자동 적응 (수동 튜닝 불필요)

### NVS 결과 (실행 후 업데이트 예정)

| 방법 | iter | PSNR (dB) | SSIM | LPIPS | 메시 |
|------|------|-----------|------|-------|------|
| GS-2M | 30k | - | - | - | TSDF |
| 2DGS | 30k | - | - | - | TSDF |
| MILo | 18k | - | - | - | SDF |

---


## AirSim GPS 오차 시뮬레이션 & Waypoint 개수 결정 (2026-07-09)

### 목적

실제 드론 촬영 계획(waypoint 개수, 반경)을 정하기 전에, GPS 오차가 실제 비행경로/waypoint 도달 오차에 얼마나 영향을 주는지 AirSim 시뮬레이션으로 먼저 정량화. 그 결과를 바탕으로 MASt3R-SfM 입력용 waypoint 개수(사진 장수)를 공학적으로 결정하고, 실제 촬영 스크립트를 준비.

### GPS 오차 모델 (Cosys-AirSim estimator, 신규 구현)

| 항목 | 값/방식 |
|---|---|
| 대상 하드웨어 | u-blox MAX-M10S (M10050) |
| 수평 오차 | 1.5m CEP (데이터시트) |
| 수직 오차 | 2.5m LEP (스펙 미기재, 수평의 ~1.7배로 가정) |
| 갱신율 | 10Hz, latency 0.1s |
| TTFF | 26s (cold start; 이 구간 동안 GPS 출력 자체가 없음 → 스폰 위치 고정) |
| 오차 구조 | Gauss-Markov 바이어스(분산 90%, τ=60s) + 백색잡음(10%) — PX4 SITL `gazebo_gps_plugin`과 동일 계열 |
| Heading 오차 | QMC5883L급, 2° 표류 바이어스(τ=120s) |
| 제어 반영 | 저역통과(시상수 5s) 필터 — 빠른 백색잡음은 걸러내되 느린 바이어스는 실제 제어용 위치추정(estimator)에 통과시켜, 실비행처럼 GPS 오차가 궤적에 흔들림으로 나타나게 함 |

- 파일: `Plugins/AirSim/Source/AirLib/include/vehicles/multirotor/firmwares/simple_flight/AirSimSimpleFlightEstimatorGps.hpp` (신규 파일, 원본 estimator는 미수정)
- 배선: `SimpleFlightApi.hpp`에서 GPS 센서 참조 3줄만 추가
- settings.json 프리셋 (`D:\UE_5.4\Engine\Binaries\Win64\`): `settings_disturbed.json`(바람 3m/s + GPS 오차, 현재 활성), `settings_baseline.json`(외란 없음, ablation 기준경로용)

> ⚠️ **시행착오**: 첫 시도는 완벽한 ground-truth 속도로 dead-reckoning + 분산기반 칼만게인 조합 → 관성이 완벽하다고 가정되어 GPS를 0.0015%만 반영, 오차가 사실상 사라짐(truth-est 간극 2.4cm). 시상수 고정 저역통과 필터로 교체 후 간극 평균 2.5m(≈GPS CEP 스케일)로 정상화 확인.

### Waypoint 개수 결정 — 기하 오차 vs GPS 오차 균형

원을 n개 점으로 근사할 때 코너커팅(기하) 오차 ≈ R(1-cos(π/n)). R=15m 기준:

| n | 기하 오차 | 판단 |
|---|---|---|
| 4 | 4.4m | GPS 오차(1.5m CEP)의 3배 — 측정을 오염시킴 |
| 8 | 1.14m | 경계선 |
| **12** | **0.51m** | GPS 오차의 1/3 — 균형점, 실제 비행 waypoint 개수 추천값 |
| 36 | 0.06m | GPS 오차 순수 측정(ablation)용 — 실제 임무엔 과잉 |

→ MASt3R-SfM 최소 요구 뷰 수(문헌상 객체 중심 sparse-view 6~8장)와도 부합 → **실제 비행은 12 waypoint 권장**.

### 스크립트

`D:\epic\CitySample\scripts\waypoint_gps_error_test.py` (Cosys-AirSim, cosysairsim 파이썬 클라이언트, RPC 포트 47000)

- `--circle`: 연속경로(`moveOnPathAsync`)로 원형 비행, 실제/추정 위치를 실시간 샘플링 → CSV (GPS 오차 정량화용)
- `--capture`: 각 지점에서 정지(`moveToPositionAsync`) + 중심 오브젝트를 향해 yaw 자동 조준 + 촬영 → `{out_dir}/NNN.png` + `capture_log.csv`(참값/추정 pose, eph/epv 포함) — **MASt3R-SfM 입력용**
- `--targets <오브젝트명>`: `simGetObjectPose`로 씬 오브젝트(예: `StaticMeshActor_7`) 위치를 자동 조회해 원 중심으로 사용 (UE 에디터 액터 라벨과 내부 `GetName()`이 다를 수 있음 — `--list-objects ".*"` 로 사전 확인 필요)
- 예: `--capture --targets StaticMeshActor_7 --radius 15 --num-points 18 --out-dir captures_18`

### 현재 상태 / 다음 단계

- [x] GPS 오차 모델 구현 + 검증 (제어 루프에 실제 반영 확인, truth-est 간극 ~2.5m로 정상화)
- [x] 18장 MASt3R-SfM 정합 서버에서 1회 완료
- [ ] AirSim `--capture`로 18장 촬영 → 각도 균등 서브샘플링(12/8/6장) → MASt3R-SfM 재실행 → 18장 결과 기준 포즈/포인트클라우드 비교(ICP RMSE, 카메라 등록 성공률)로 최소 waypoint 개수 실측 검증
- [ ] GPS 오차 몬테카를로 반복(시드 다중화, 30~100회) — waypoint 추종오차의 분포/CEP 추정. 현재 난수 시드가 코드에 고정돼 있어 Python에서 제어 불가 → RPC로 시드 노출 필요(미착수)

---


## 실촬영 waypoint 개수 ablation + 서버 MASt3R-SfM 검증 (2026-07-09)

### 촬영 스크립트 전환

커스텀 `waypoint_gps_error_test.py` 대신, 기존 프로젝트 스크립트 `D:\epic\CitySample\test_auto_operate_optimal.py`를 그대로 사용(중복 구현 방지). 카메라(짐벌)는 이미 `CAMERA_PITCH_DEG=-45.0`로 고정 구현되어 있어 고도/반경과 무관하게 45도 하향 촬영됨. `--mode orbit_only`로 원형 궤도 지정 waypoint 수만큼 정지-촬영, 각 프레임에 `vehicle_pose`(ground truth)와 `multirotor_kinematics`(GPS-필터링 추정치)를 함께 JSON으로 기록.

### 고도 드리프트 버그 (발견 + 수정)

- **증상**: 반경10m/고도4m 지정 촬영(`captures_4m_r11`)에서 `vehicle_pose.z`(실제 고도)가 -7.5m → -11m까지 표류. `multirotor_kinematics.position.z`(추정치)는 -4~-4.8m로 정상 표시 — 즉 **드론이 실제로는 명령한 4m보다 훨씬 높이 떠서, 잘못된 추정치를 4m로 보이게 만드는 중**이었음.
- **원인**: 수평(X/Y)과 동일한 Gauss-Markov GPS 바이어스 모델을 Z축에도 그대로 적용(EpvFinal=2.5). 상관시간(τ=60s)이 촬영 1회 비행시간(~30~40s)보다 길어서, 한 번의 큰 Z바이어스 표본이 비행 내내 거의 고정값으로 유지됨 → 고도유지 제어기가 편향된 추정치를 4m로 맞추려다 실제 고도를 계속 밀어올림. 실기체는 이 문제를 기압계(정확·표류 없음)로 회피하는데, 시뮬레이션은 GPS-Z(가장 나쁜 채널)를 그대로 쓰고 있었음.
- **수정**: `AirSimSimpleFlightEstimatorGps.hpp`에 `getBaroAltitude()` 추가 — Z 추정치는 GPS Gauss-Markov 바이어스를 완전히 우회하고 매 틱 `true_z + 백색잡음(σ=0.15m)`만 사용. 검증(`captures_4m_r10`): 실제 고도 -4.1~-4.65m로 안정화(±0.1~0.3m), 수평 GPS 오차(추정-실제 간극 1.4~2.9m)는 정상적으로 유지됨.

### MASt3R-SfM 서버 환경 문제 해결

- **증상**: `conda recon3d` 환경에서 실행 시 `ImportError: GLIBCXX_3.4.29 not found`(PIL/Lerc, libstdc++ 구버전) 발생. `LD_LIBRARY_PATH`로 conda 환경의 최신 libstdc++를 우선시키면 PIL은 해결되나 `AttributeError: torch._C has no attribute _OutOfMemoryError`(torch CUDA 확장 ABI 깨짐)로 다른 에러 발생 — PIL과 torch가 서로 다른 libstdc++ 버전을 요구하는 환경 자체 결함.
- **해결**: `recon3d` 대신 기존에 준비되어 있던 **`venv-mast3r`**(`/home/sdh/Desktop/venvs/venv-mast3r`, python3.10, torch 2.1.2+cu121) 사용 — PIL/torch 동시 임포트 정상 확인. `run_mast3r_sfm.py`는 `/home/sdh/Desktop/models/MAST3R_2/`에 위치.
- **주의**: sysai3 로그인 셸 `.bashrc`에 문법 오류(`line 3: unexpected token 'fi'`) 있음 — 매 SSH 세션마다 경고 출력되지만 명령 실행 자체엔 영향 없음(작업 범위가 `~/Desktop/` 이하로 제한되어 있어 별도 수정하지 않음).

### Waypoint 개수 ablation 결과 (반경10m/고도4m, `scene_graph=retrieval-20-5` — 프로젝트 표준)

| 데이터셋 | 장수 | 외란 | 매칭 페어 | 포인트 수 |
|---|---|---|---|---|
| `captures_4m_r10_n8` | 8 | 있음 | 78쌍 | 650,879 |
| `captures_4m_r10` | 12 | 있음 | 174쌍 | 752,969 |
| `captures_4m_r10_n17` | 17 | 있음 | 318쌍 | 1,177,574 |
| `captures_4m_r10_n17_baseline` | 17 | 없음(바람0, GPS오차~0) | 318쌍 | 1,164,628 |

- retrieval anchor 수(Na=20)가 실험 이미지 수(8~17장)보다 많아, 이 규모에서는 retrieval이 사실상 complete 그래프와 동일하게 동작(초기 `scene_graph=complete`로 돌린 결과와 포인트 수 거의 일치 — 12장 750,305 vs 752,969, 8장 652,179 vs 650,879). 대형 데이터셋(수십~수백 장)에서만 retrieval의 효율 이점이 실제로 발휘될 것으로 예상.
- 결과 파일(`pointcloud.ply`/`poses.npy`/`focals.npy`) 전부 `C:\Users\손동한\Desktop\mast3r_results\<데이터셋명>[_retrieval]\`로 다운로드 완료 — 로컬에서 CloudCompare로 시각 비교 가능.
- 다음 단계: 12/8/17장 결과의 포인트클라우드 밀도/노이즈를 GT(시뮬레이션 실제 지오메트리) 대비 정량 비교(ICP RMSE 등)하여 "12개가 균형점"이라는 기하학적 추정을 실측으로 검증할 것.

### settings.json 프리셋 전환 방법

`D:\UE_5.4\Engine\Binaries\Win64\settings.json`이 실제 활성 파일(우선순위: 커맨드라인 > 실행파일 폴더 > 실행 폴더 > `Documents\AirSim\`, `Documents` 경로는 최하위라 무시되기 쉬움 — 주의). `settings_baseline.json`(외란 없음) / `settings_disturbed.json`(바람+GPS 오차) 두 프리셋을 파일로 복사해 스왑하는 방식 사용. 전환 후 UE5 Play를 재시작해야 반영됨(런타임 중 설정 재로드 안 됨).


## 실제 드론 데이터 복원: 3m/5m/7m combined (2026-06-29)

### 데이터

| 항목 | 값 |
|---|---|
| 드론 | DJI, 실제 야외 비행 |
| 고도별 폴더 | `3m_1` (55장), `5m_1` (55장), `7m_1` (57장) |
| 해상도 | 1920×1080 |
| GT | ❌ 없음 (시각적 평가만 가능) |
| 서버 | sysai3, `/home/sdh/Desktop/data/drone_real/` |

### 실험 이력

#### ① 3m+7m combined (순차 순서) — 박스 분리 문제 발견

| 항목 | 값 |
|---|---|
| 이미지 | 3m_1(55) + 7m_1(57) = 112장, 프리픽스 `3m_frame_`, `7m_frame_` |
| MASt3R-SfM | ✅ 완료 → `drone_real_sfm/combined_3m7m/` (focal=1364.30px) |
| COLMAP 변환 | ✅ 완료 → `drone_real_combined__colmap/` |
| 2DGS 학습 | ✅ 완료 (30k iter) → `drone_real_combined__2dgs/` |
| 메시 추출 | ✅ 완료 → `fuse_post.ply` (96MB, 1.95M V) |
| **문제** | **박스가 두 개로 분리** — 3m↔7m inter-orbit scale mismatch |

> 원인: swin-5 그래프가 순차 정렬 시 경계 1곳만 cross-altitude 연결 →  
> 3m 그룹과 7m 그룹이 서로 다른 스케일로 복원 → 동일 박스가 2개로 보임.  
> 시뮬레이션에서도 7m ATE=27.6cm로 이미 확인된 구조적 문제.

#### ② 3m+5m combined (순차 순서) — 동일 문제

| 항목 | 값 |
|---|---|
| 이미지 | 3m_1(55) + 5m_1(55) = 110장 |
| MASt3R-SfM | ✅ 완료 → `drone_real_sfm/combined_3m5m/` |
| **문제** | **박스 분리 동일** — 고도 차 2m도 순차 정렬 시 동일 증상 |

#### ②-1 5m_1 단독 GS-2M 추가 (2026-06-30)

| 항목 | 값 |
|---|---|
| 배경 | 시뮬 4m에서 GS-2M이 큐브 세밀/조밀(F@1cm·vertex 6.7×) 우위 → 실제 5m도 검증 |
| 학습 | ✅ GS-2M 30k 완료 (PSNR 29.33) → `drone_real_5m_1__gs2m` |
| 메시 | ✅ `tsdf_post.ply` (152M, 3.0M V) / 2DGS는 기존 `fuse_post.ply` (27M) |
| 평가 | ❌ GT 없음 → 시각 비교만 (박스 영역 조밀도 확인) |
| 다운로드 | 로컬 `Desktop/real_5m_meshes/` (2DGS, GS-2M) |
| 주의 | 5m 고도 → 박스 이미지 점유 작음·측면 미관측 → GS-2M 3M V 중 지면/배경 floater 다수 가능 |

#### ③ 3m+5m 인터리빙 (현재 진행 중) — 해결 시도

| 항목 | 값 |
|---|---|
| 핵심 아이디어 | 같은 각도의 3m·5m 이미지를 교대 배치 → swin-5가 매 쌍마다 cross-altitude 연결 생성 |
| 이미지 순서 | `img_0000_3m, img_0001_5m, img_0002_3m, img_0003_5m, ...` (110장) |
| 경로 | `drone_real/combined_3m5m_interleaved/` |
| MASt3R-SfM | ⏳ 진행 중 (tmux: mast3r_3m5m) |
| 2DGS | ⏳ SfM 완료 후 즉시 시작 예정 |

> **검증 방법**: pointcloud.ply 또는 fuse_post.ply를 CloudCompare로 열어 박스가 하나로 합쳐지는지 확인.

### 다운로드 현황 (로컬 바탕화면)

| 파일 | 경로 | 내용 |
|---|---|---|
| `sfm_poses.npy` | `Desktop/` | 3m+7m combined 포즈 |
| `sfm_pointcloud.ply` | `Desktop/` | 3m+7m SfM 점군 |
| `sfm_3m5m/` | `Desktop/sfm_3m5m/` | 3m+5m combined SfM 결과 |
| `simulation_3m/`, `simulation_7m/` | `Desktop/` | 시뮬레이션 이미지 34장 |
| `simulation_sfm/` | `Desktop/simulation_sfm/` | 시뮬레이션 MASt3R-SfM 결과 |
| `2dgs_combined/fuse_post.ply` | `Desktop/2dgs_combined/` | 3m+7m 2DGS 메시 (96MB) |

---

---


---

# PART C. 구 데이터셋 (real_test / blue_1) — 초기 실험 아카이브

## real_test 결과 요약 (시뮬레이션, 34장, AirSim)

> **데이터**: AirSim 시뮬레이션 렌더링 (실제 드론 아님), 34장, 1920×1080, GT 메시 있음  
> **3m_1과 다른 데이터셋** — 비교 혼용 금지

### NVS PSNR (real_test) — 재학습 예정

> ❌ **문제**: 2DGS/MILo 모두 `eval=False`로 학습됨 → 34장 전부 train에 사용  
> AGS만 `eval=True` (4장 held-out). 직접 수치 비교 불가 → **2DGS·MILo 재학습 필요**

**현재 측정값 (참고용, 공정 비교 아님)**

| 모델 | PSNR | 뷰 | 비고 |
|---|---|---|---|
| 2DGS (30k) | 32.31 | train-34 | ❌ eval=False, 재학습 필요 |
| MILo baseline (18k) | 28.07 | train-34 | ❌ eval=False, 재학습 필요 |
| MILo+prior (18k) | 28.13 | train-34 | ❌ eval=False, 재학습 필요 |
| MILo SOR2 (18k) | 27.53 | train-34 | ❌ eval=False, 재학습 필요 |
| GS-2M (30k) | 31.21 | train (추정) | 모델 없음 |
| AGS baseline (7k) | 17.49 | **test-4** | ✅ eval=True |
| AGS+prior (30k) | 17.28 | **test-4** | ✅ eval=True, 30k 과적합 |

**재학습 계획 (eval=True, llffhold=8 → 30 train / 4 test)**

| 모델 | 우선순위 | 예상 시간 | 상태 |
|---|---|---|---|
| 2DGS (30k) | 🔴 높음 | ~1시간 | ⏳ 대기 |
| MILo baseline (18k) | 🔴 높음 | ~2시간 | ⏳ 대기 |
| MILo+prior (18k) | 🟡 낮음 (ablation) | ~2시간 | ⏳ 대기 |
| MILo SOR2 (18k) | 🟡 낮음 (ablation) | ~2시간 | ⏳ 대기 |
| AGS baseline (7k) | — | — | ✅ 완료 |

### 메시 품질 (real_test, GT 큐브 5개 기준, Umeyama 정렬 후 CD/F-score)

| 순위 | 모델 | CD (cm) | F@1cm | F@5cm | F@10cm |
|---|---|---|---|---|---|
| 🥇 | **AGS baseline** | **25.41** | 0.019 | 0.057 | **0.127** |
| 🥈 | GS-2M | 27.79 | 0.004 | 0.029 | 0.057 |
| 🥉 | MILo SOR2 | 30.72 | 0.003 | 0.013 | 0.024 |
| 4 | MILo baseline | 37.20 | 0.003 | 0.010 | 0.018 |
| 5 | 2DGS | 37.25 | 0.003 | 0.019 | 0.036 |

> ⚠️ CD가 높은 건 전체 씬 메시를 GT 소형 큐브(5개)랑 비교하기 때문 — 절대값보다 상대 순위가 의미 있음  
> 모든 방법 실패에 가까움 → 근본 원인: **texture 부족** (단색 배경 + 큐브)

---


## real_test 메시 복원 실험 이력 (2026-06-21 완료)

> 데이터: `~/Desktop/data/datasets/rgb/` (34장, 1920×1080, 시뮬레이션)
> 공통 초기화: MASt3R-SfM 점군 1,490,920 pts (전부 동일 PLY 사용 확인)

### Baseline (depth/normal supervision 미적용)

| 방법 | 결과 | 출력 | 로컬 다운로드 |
|---|---|---|---|
| 2DGS (baseline) | ✅ 완료 | `real_test__mast3r__2dgs/.../fuse_post.ply` 6.4M V | `C:\Users\sdh97\Desktop\real_test_2dgs_fuse_post.ply` (315MB) |
| MILo (baseline) | ✅ 완료 | `real_test__milo/mesh_learnable_sdf.ply` | `C:\Users\sdh97\Desktop\real_test_milo.ply` (288MB) |
| AGS-Mesh (baseline) | ✅ 완료 | `real_test__ags_mesh_output/.../fuse_post.ply` 3.5M V | `C:\Users\sdh97\Desktop\real_test_ags_fuse_post.ply` (165MB) |

### Prior 버전 (MASt3R depth map supervision 적용)

| 방법 | 결과 | 출력 | 평가 |
|---|---|---|---|
| MILo + MASt3R depth prior | ✅ 완료 | `real_test__milo_prior/mesh_learnable_sdf.ply` | `C:\Users\sdh97\Desktop\real_test_milo_prior.ply` (268MB) — **전체 형상 양호, 잡음 많음** |
| AGS-Mesh + MASt3R depth prior | ✅ 완료 | `real_test__ags_prior_output/.../fuse_post.ply` 3.1M V | `C:\Users\sdh97\Desktop\real_test_ags_prior_fuse_post.ply` (152MB) — **평가 필요** |

### Prior 구현 내역

- MASt3R depth maps: `~/Desktop/data/experiments/real_test__mast3r_depth/depth_maps/` (`.npy`, float32, m)
- **MILo 패치** (`~/Desktop/milo/milo/regularization/regularizer/depth_order.py`):
  - `initialize_depth_order_supervision()` 상단에 MASt3R `.npy` 로딩 경로 추가
  - config key `mast3r_depth_dir` 존재 시 `.npy` 로드 후 [0,1] 정규화 → depth order supervision에 사용
  - 없으면 기존 DepthAnythingV2 경로 fallback
- **MILo config** (`~/Desktop/milo/milo/configs/depth_order/mast3r_depth.yaml`):
  - `mast3r_depth_dir: '/home/sdh/Desktop/data/experiments/real_test__mast3r_depth/depth_maps'`
  - `weight_update_iters: [600, 1500, 3000, 8000, 13000]` / `weight_update_values: [1, 0.1, 0.01, 0.001, 0.0001]`
- **AGS-Mesh**: MASt3R `.npy` → uint16 PNG (mm) 변환 후 `--depth_supervision`, TSDF `voxel_size=0.004 sdf_trunc=0.02`

### 주의사항 (재실행 시 참고)
- AGS input 디렉토리는 `colmap/` 심링크 필수 (없으면 "Could not recognize scene type!" 에러)
- MASt3R depth format: `.npy` float32 meters. AGS는 uint16 PNG mm 필요 → `(depth * 1000).astype(np.uint16)`
- AGS normal: dummy flat (2DGS normal 저장이 주석처리됨, `utils/mesh_utils.py:294`)

---


## outlier 제거 실험 (2026-06-22 착수)

### 배경: 직접-위상 방법(MILo/SuGaR/AGS)의 잡음 원인 분석

TSDF 기반(2DGS)은 멀티뷰 평균화로 outlier에 강건하지만, 직접-위상 방법들은 각기 다른 이유로 outlier에 취약:

- **MILo**: 학습 중 매 iteration마다 Gaussian → SDF → 미분 가능한 메시 추출 → occupancy/normal supervision에 사용. outlier가 학습 전 과정 내내 메시 위상을 오염시킴. `mesh_extract_sdf.py`는 학습 후 최종 export만 담당.
- **SuGaR/AGS**: 학습 완료된 Gaussian에서 별도 후처리로 메시화 → outlier가 Gaussian 분포를 왜곡해 결과 메시를 깨뜨림.

가설 검증:

| 가설 | 판정 | 근거 |
|---|---|---|
| H1: 스케일/좌표계 깨짐 | ❌ 기각 | scene_extent real_test 1.99 / blue_1 4.24 = **정상 범위** (100↑/0.1↓ 아님) |
| H2: low-confidence outlier 포인트 | ✅ **핵심 원인** | 점군이 scene_extent의 **4.9x(real_test)/4.6x(blue_1)**까지 퍼짐. MILo "잡음 많음"의 직접 원인 |
| H3: imp_metric outdoor/indoor 미스매치 | ⏸ 미확인 | MILo config 확인 필요 |

### scene extent 측정값 (COLMAP 변환 기준)

| 항목 | real_test | blue_1 |
|---|---|---|
| 카메라 diagonal | 5.14 | 9.06 |
| scene_extent (3DGS radius) | 1.99 | 4.24 |
| points3D full diag | 15.84 | 29.77 |
| points3D p1–p99 diag | 9.84 | 19.39 |
| 점군/extent 비율 | 4.93x | 4.57x |

### 기존 시도 여부 확인
- **기하 outlier 제거(SOR/bbox crop)는 MILo/SuGaR에 적용한 적 없음** (real_test MILo는 raw 1.49M 점군 그대로 사용)
- 존재하던 `mast3r_color_filter.py`는 파란색 추출→Poisson(blue_1 전용), 기하 필터 아님

### 결정: SOR만 적용 (환경 최대 보존 목적, bbox crop 제외)

bbox crop은 주변 환경(지면)까지 잘릴 수 있어 제외. SOR은 공간 경계 없이 고립 floater만 제거 → 환경 보존.

| SOR 설정 | 보존 | full diag | p1–p99 |
|---|---|---|---|
| (원본) | 100% | 15.84 | 9.84 |
| **nb=20, std=2.0 (채택)** | **94.6%** | 12.79 | ~7.98 |
| nb=20, std=1.5 | 92.0% | 11.61 | 7.28 |
| nb=30, std=1.0 | 88.0% | 9.53 | 6.30 |

- 1차 시도: `nb_neighbors=20, std_ratio=2.0` (80,414 pts 제거, 94.6% 보존). 로컬 `real_test_pointcloud_sor.ply` (37MB)
- outlier 위치 확인: `real_test_SOR_outlier_비교.png` → 제거점이 전부 바깥 성긴 가장자리(고립 floater)에 분포 확인

### 검증 실행 결정 (2026-06-22)

효과 유무를 빠르게 확인하기 위해 **강한 SOR + 가장 적합한 모델**로 1회 테스트:

- **SOR 강도**: `nb=30, std=1.0` (88% 보존, full diag 15.84→9.53) — 강하게 적용해 효과 명확히 확인
- **테스트 모델**: **MILo** 선정
  - 이유: ① baseline 명확("형상 양호, 잡음 많음") ② 학습 중 매 iter마다 미분 가능한 메시 추출 → outlier가 학습 전체에 걸쳐 위상 오염 → 가장 민감하여 SOR 효과 가장 뚜렷 ③ 별도 TSDF 없이 learnable SDF에서 직접 메시 export ④ SuGaR보다 빠름(~30분)
- **복원 속도 순위**: 2DGS(robust, 제외) > MILo(~30분) > AGS(depth prep+TSDF) > SuGaR(다단계, 최저)
- **파이프라인**: points3D.txt를 SOR로 필터 → `real_test__mast3r_clean/02_colmap` → MILo 재실행(`real_test__milo_sor`) → 기존 MILo와 비교

### SOR 실험 결과 요약

| 항목 | MILo baseline | MILo SOR1 (nb=30, std=1.0) | MILo SOR2 (nb=30, std=0.5) |
|---|---|---|---|
| 입력 pts | 1,490,920 | 1,308,980 (88%) | 1,218,707 (81.7%) |
| 출력 파일 | `real_test_milo.ply` (288MB) | `real_test_milo_sor.ply` (221MB) | `real_test_milo_sor2.ply` (215MB) ✅ |
| Vertices | 7,203,794 | 5,534,552 (-23%) | 측정 필요 |
| bbox diagonal | 15.20 | 12.35 (-19%) | 측정 필요 |
| 메시 품질 | 가장자리 스파이크 심각 | 가장자리 스파이크 여전히 존재 | 평가 예정 |
| 학습 시간 | — | — | 18000 iter / 6828초 (~114분) |

- **SOR1 결과**: 수치 개선됐으나 3D 뷰어에서 여전히 가장자리 파편 다수 → std=0.5로 강화
- **SOR2 결과**: ✅ 완료. 215MB PLY 로컬 저장 (`현재결과/03_Mesh/sor/real_test_milo_sor2.ply`)
- outlier 위치 시각화: `real_test_SOR_outlier_topview.png` (단일 상단뷰, RGB 컬러 포인트)

### GOF (Gaussian Opacity Fields) — 학습 완료, 메시 추출 실패

- **목적**: unbounded 야외 씬 특화 최신 모델 (NeurIPS 2024) 적용
- **학습**: ✅ 30000 iter 완료
- **메시 추출**: ❌ binary search step 0~7 완료 후 hanging (최종 메시 생성 단계서 멈춤)
- **입력**: SOR2 필터링된 COLMAP (`real_test__milo_sor2/colmap/`, 1,218,707 pts)

---


## 메시 품질 정량 평가 (2026-06-24)

### 평가 방법

- **GT**: UE5 scene cube extract (정확한 좌표 + 메시)
- **지표**: Chamfer Distance (CD), F-score @1cm/@5cm/@10cm
- **정렬**: 스케일 정규화 (bbox diagonal 기준)

### blue_1 결과 (1개 큐브)

| 방법 | CD (cm) | F@1cm | F@5cm | F@10cm | 평가 |
|---|---|---|---|---|---|
| MILo baseline | **9.40** | 0.022 | 0.178 | **0.556** | ✅ 최고 |
| MILo+prior | 11.35 | 0.014 | 0.135 | 0.365 | ❌ depth prior 악화 |
| AGS-Mesh | 9.67 | 0.023 | **0.221** | 0.523 | 유사 |
| MASt3R TSDF | 10.08 | 0.016 | 0.153 | 0.470 | — |
| 2DGS | 10.63 | 0.008 | 0.114 | 0.446 | — |

**발견**: depth prior가 오히려 해악 (단색 박스에서 depth map 노이즈가 학습 왜곡)

### real_test 결과 (5개 큐브, 구 평가 방법)

> ⚠️ 아래 수치는 원본 eval 스크립트 결과 (스크립트 분실로 재현 불가). GS-2M 비교는 상단 "GS-2M 학습+평가" 섹션 참조.

| 방법 | CD (cm) | F@1cm | F@5cm | F@10cm | 평가 |
|---|---|---|---|---|---|
| MILo baseline | **21.54** | 0.000 | 0.025 | 0.088 | — |
| **MILo SOR2** | 21.44 | 0.000 | 0.010 | **0.101** | ⚠️ SOR 무의미 |
| MILo+prior | 21.42 | 0.000 | 0.014 | 0.081 | ⚠️ prior 무의미 |
| AGS baseline | 23.52 | 0.000 | 0.013 | 0.052 | — |
| AGS+prior | 24.31 | 0.000 | 0.009 | 0.102 | — |
| 2DGS | 21.52 | 0.001 | 0.012 | 0.100 | — |

**발견**: SOR, depth prior 모두 효과 없음 → 모든 방법 ~21-24cm (texture 부족이 병목)

### 결론

**근본 원인: Texture/photometric gradient 부족**
- blue_1: 단색 파란 박스 → 색상 정보 전무
- real_test: 주로 단색 배경 + 5개 큐브 → signal 극도로 부족

**시사점**:
1. Outlier 제거(SOR) → 실제 메시 품질 향상 무
2. Depth prior → blue_1에서 악화, real_test에서 무의미
3. 모든 방법(2DGS/MILo/AGS) 비슷하게 실패 → **알고리즘이 아니라 데이터 문제**

---


## 포즈 정확도 비교 — GT vs MASt3R-SfM (2026-06-23)

### 데이터
- **GT**: AirSim meta JSON `camera.pose` (ground truth, 오차 없음) — 34프레임
- **SfM**: COLMAP `images.txt` → world 위치 복원 (R^T(-t))
- **IMU**: Euler preintegration (gravity 보정, 초기값=GT frame 0)

### 결과

| 방법 | ATE (m) | RPE (m) | 비고 |
|---|---|---|---|
| MASt3R-SfM | **0.023** | **0.024** | Umeyama 정렬 후 (scale 3.89x) |
| IMU preintegration | 6.23 | 2.39 | 초기값 GT, drift 누적 |

> 좌표계: AirSim NED (m), 드론 고도 약 5m (Z ≈ -5)  
> GT trajectory span: 18.88m (X×Y≈13m씩 이동)  
> SfM scale factor 3.89: COLMAP arbitrary unit → meter 변환  
> **결론: MASt3R-SfM 위치 오차 2.3cm (매우 정확), IMU는 6.2m drift 누적**

> ⚠️ 버그 수정 이력: Umeyama `.mean()`→`.sum()/n` (scale 3x 과대추정 → ATE 13m 오류)

- **궤적 비교 플롯**: `현재결과/pose_comparison_imu.png` (상단뷰 + 정면뷰)
- **스크립트**: `/tmp/pose_eval2.py`

---


## 3m_1 평가 파이프라인 설계 — 이전 버전 (2026-06-25)

- **완료**: `real_test__milo_sor2` — 18000 iter, `mesh_learnable_sdf.ply` (215MB)
- **완료**: `blue_1__milo_fhd_prior` — 20MB PLY (depth prior 적용)
- **완료**: 메시 정량 평가 (Chamfer Distance + F-score) — 모든 데이터셋에서 texture 부족이 병목
- **보류**: `real_test__gof` — 메시 추출 hanging (binary search step 7 후 멈춤)
- **완료 (2026-06-24)**: GS-2M (Eurographics 2026) 학습 + TSDF 메시 추출 — `/mnt/c/Users/sdh97/Desktop/3d_results/real_test/gs2m_output/`
- **완료 (2026-06-24)**: 실제 드론 3개 고도 폴더 MASt3R-SfM — `sysai3:~/Desktop/data/drone_real_sfm/{3m_1,5m_1,7m_1}/`
- **완료 (2026-06-24)**: GS-2M 메시 품질 정량 평가 (재평가, Umeyama 기반 COLMAP 공간 비교)
- **❌ 취소**: 3m_1 첫 번째 학습 시도 (GS-2M/2DGS/MILo) — focal 버그로 전량 폐기
- **⏳ 대기**: focal 수정 후 3m_1 재실행 (파이프라인 설계 완료)

---


## 현재 상태: 3m_1 전 모델 NVS + 메시 추출 완료 (2026-06-27)

### 3m_1 NVS 결과 (test 7장, llffhold=8 → 48 train / 7 test, 2026-06-27 완료)

**30k 기준 PSNR/SSIM/LPIPS 전체 비교**

| 순위 | 모델 | PSNR↑ | SSIM↑ | LPIPS↓ | iter |
|---|---|---|---|---|---|
| PSNR 🥇 | **MILo** | **22.106** | 0.5853 | 0.3543 | 18k |
| SSIM 🥇 | **3DGS+depth** | 21.900 | **0.5949** | **0.2641** | 30k |
| LPIPS 🥇 | **3DGS+depth** | 21.900 | 0.5949 | **0.2641** | 30k |
| — | GS-2M | 21.915 | 0.5889 | 0.2823 | 30k |
| — | 2DGS | 21.906 | 0.5903 | 0.2741 | 30k |
| — | 3DGS baseline | 21.811 | 0.5878 | 0.2642 | 30k |

**best iter 기준 (peak PSNR)**

| 모델 | best test PSNR | iter | 비고 |
|---|---|---|---|
| **3DGS+depth** | **22.23** | 7k | depth prior 효과 (7k peak, 이후 하락) |
| **MILo** | **22.11** | 18k | 과적합 gap 최소 (1.7dB) |
| GS-2M | 21.93 | 25k | 30k 소폭 하락 |
| 3DGS | 22.07 | 7k | 30k 과적합 (gap 6.1dB) |
| 2DGS | 21.91 | 30k | 수렴 안정 |

**지표별 해석:**
- **PSNR**: MILo 1위 — 픽셀 정확도 가장 높음
- **SSIM/LPIPS**: 3DGS+depth 동시 1위 — 지각적 품질(텍스처·구조)은 depth prior Gaussian이 우수
- 모든 모델 PSNR ~21.8~22.1dB 수렴 → 알고리즘 아닌 **데이터 천장**이 지배
- depth prior: PSNR +0.09dB 미미, LPIPS −0.0001 미미하지만 **SSIM +0.007** 유의미

### 3m_1 메시 추출 현황 (2026-06-27 완료)

| 모델 | 메시 파일 | 크기 | 버텍스 수 | 방식 |
|---|---|---|---|---|
| ✅ MILo | `3m_1__mast3r__milo/mesh_learnable_sdf.ply` | 80MB | 2,003,258 | Learnable SDF |
| ✅ GS-2M | `3m_1__mast3r__gs2m/train/ours_30000/mesh/tsdf_post.ply` | 27MB | 523,613 | TSDF fusion |
| ✅ 2DGS | `3m_1__mast3r__2dgs/train/ours_30000/fuse_post.ply` | 33MB | 671,316 | TSDF fusion |
| — | 3DGS / 3DGS+depth | — | — | point cloud만 (별도 TSDF 필요) |

TSDF 파라미터 (GS-2M / 2DGS 공통): `voxel_size=0.01, depth_trunc=6.0, sdf_trunc=0.04, num_cluster=1`

### 핵심 발견
1. **모든 모델 ~21.8–22.2dB에 수렴** → 알고리즘 차이보다 **데이터 천장**이 지배
2. 천장 원인 = **포즈 정확도(MASt3R 누적오차) + 잔디/야외 장면** (geometry가 아님)
3. depth prior는 geometry만 제약 → +0.17dB로 천장 못 뚫음
4. 3DGS는 자유도 높아 sparse view에서 과적합 (train-test gap 6.1dB)
5. 천장 돌파 레버: **COLMAP BA 포즈 재정렬** > 프레임 조밀화(10→5)

### MASt3R-SfM → 3DGS depth prior 파이프라인 (공식 경로, 신규 구축)
- depth 추출: `gen_3m1_depth_maps.py` (버그 2개 수정: reshape, world→camera Z 변환). 출력 `3m_1__mast3r_depth/depth_maps/*.npy` (55장, 288×512, metric 1.4~4m)
- 역깊이 PNG 변환: `/tmp/convert_depth_3dgs.py` → `depths_png/` + `sparse/0/depth_params.json`
- **스케일 검증**: COLMAP/MASt3R 깊이 median 비율 **1.0019** (동일 metric 스케일 → scale=0.998 offset=0 직접 사용, make_depth_scale 불필요)
- ⚠️ MASt3R COLMAP export에 2D-3D 대응점 없음 → make_depth_scale.py 사용 불가 (metric 일치로 우회)
- 학습 env: `gs3d` (gs2m clone + 바닐라 diff-gaussian-rasterization/simple-knn/fused-ssim 설치). gs2m rasterizer는 GS-2M 커스텀(feature_count 필드)이라 바닐라 3DGS 비호환
- 출력: `3m_1__mast3r__3dgs{,_depth}/point_cloud/iteration_{7000,30000}/`

### 서버 경로 요약 (sysai3)
```
/home/sdh/Desktop/data/experiments/
├── 3m_1__mast3r__milo/        mesh_learnable_sdf.ply (80MB)
├── 3m_1__mast3r__gs2m/        train/ours_30000/mesh/tsdf_post.ply (27MB)
├── 3m_1__mast3r__2dgs/        train/ours_30000/fuse_post.ply (33MB)
├── 3m_1__mast3r__3dgs/        point_cloud/iteration_30000/point_cloud.ply (702MB)
├── 3m_1__mast3r__3dgs_depth/  point_cloud/iteration_30000/point_cloud.ply (735MB)
└── 3m_1__mast3r__colmap/      02_colmap/ (COLMAP 입력)
```

### 미완 / 다음
- 메시 3개(MILo/GS-2M/2DGS) 로컬 다운로드 미완료
- 5m_1, 7m_1 동일 파이프라인 미실행
- COLMAP BA 포즈 재정렬 미시도

---


## blue_1 실험 이력 (초기 시뮬 데이터, 막힘 → 아카이브)

단색 파란 박스 → 모든 접근법 실패. 근본 원인은 아래 참조.

---

### 근본 실패 구조

```
단색 파란 박스 → texture gradient 없음
    ├─ feature matching 실패 → SfM 희소 포인트 → Poisson 불완전
    ├─ photometric loss ≈ 0 → GS 계열 전부 학습 실패 (2DGS/SuGaR/MeshSplatting)
    └─ depth map 노이즈 심함 → TSDF floater 과다 (AGS-Mesh / MASt3R TSDF)
```

Gaussian splatting을 시도한 이유도 바로 이 때문이었으나, photometric gradient가 없어 학습이 안 됨.

---

### blue_1 메시 복원 실험 이력

| 방법 | 결과 | 실패 원인 |
|---|---|---|
| Screened Poisson (720p) | ❌ | 저해상도 노이즈 |
| Screened Poisson (FHD) | ⚠️ 형태 식별 | 지면 격자 아티팩트 |
| PPSurf | ❌ | 입력 점군 품질 한계 |
| SuGaR | ❌ | photometric gradient 부족 |
| GOF | ❌ | CUDA 11.3 불일치 |
| MILo | ❌ | 1.23M pts → CUDA kernel limit |
| MeshSplatting CVPR2026 | ❌ | photometric loss ≈ 0 |
| 2DGS BASE + dense prior | ✅ 학습 | 메시 미추출 |
| 2DGS CONTROL + dense prior | ✅ 학습 | TSDF floater 심각 |
| 2DGS TSDF (render.py) | ❌ | 559MB, floater 과다 |
| AGS-Mesh-2dgs | ⚠️ 완료 | TSDF 여전히 noisy (13M V) |
| MASt3R direct TSDF | ⚠️ 완료 | 16.7M V, 클러스터 후 noisy |
| MASt3R 컬러 필터 → Poisson (loose) | ❌ | gray점 혼입, 17K V |
| MASt3R 컬러 필터 → Poisson (strict) | ⚠️ 완료 | 법선 방향 불안정 |
| MASt3R 컬러 필터 → Poisson (카메라 정렬) | ⚠️ 완료 | blue_box_poisson2.ply, 평가 중 |

### 컬러 필터 결과 (가장 최근)

- 박스 점군: `blue_box_cluster.ply` (12,374 pts, 1cm 간격, 전면 커버)
- OBB 직육면체: `blue_box_obb.ply` (0.616×0.587×0.222m)
- Poisson (카메라 정렬): `blue_box_poisson2.ply` (2.4MB) — 평가 필요
- 필터 조건: `B>0.40 AND B>R+0.25 AND B>G+0.15`

---

### 남은 선택지

| 방법 | 설명 | 가능성 |
|---|---|---|
| **BPA** | Ball Pivoting. 법선 불필요, 있는 점만 연결 | ⭐ 시도 안 함 |
| **OBB 직육면체 대체** | 정확한 형상 아니지만 크기/위치 정확 | ⭐ 이미 있음 |
| structured light / depth sensor | 소프트웨어 한계 돌파 | 하드웨어 필요 |

---

### AGS-Mesh-2dgs 결과 (참고용)

- 학습: 30000 iter 완료 (2026-06-20 00:00 KST)
- 메시: `~/Desktop/data/experiments/blue_1__ags_mesh_output/train/ours_30000/fuse_post.ply`
  - 13M V, 24M F → 단순화 후 `ags_mesh_simplified.ply` (50만 F, 14MB)
- 다운로드: `C:\Users\sdh97\Desktop\ags_mesh_simplified.ply`
- 다운로드: `C:\Users\sdh97\Desktop\ags_mesh_fuse_post.ply.gz` (246MB)

### 코드 패치 내역 (dn-splatter)

- `scene/dataset_readers.py`: `load_every=5` → `load_every=1`
- `utils/camera_utils.py`: depth/normal/confidence F.interpolate 리사이즈
- `utils/general_utils.py`: `tostring_rgb()` → `buffer_rgba()` (matplotlib 호환)

---

### MASt3R direct TSDF 결과 (참고용)

- 스크립트: `~/Desktop/mast3r_tsdf_fusion.py`
- 결과: `~/Desktop/mast3r_tsdf_mesh.ply` (1.2GB), 클리닝 후 `mast3r_tsdf_clean.ply`
- 다운로드: `C:\Users\sdh97\Desktop\mast3r_tsdf_clean.ply` (20MB)
- 파라미터: voxel=1cm, sdf_trunc=5cm, 125장 fusion

---


---

# PART D. 문제 이력 (트러블슈팅 아카이브)

## 문제 이력 — real_test_combined_uniform 복원 중 발생 (2026-06-28)

### 1. 데이터셋 비균일성 (근본 교체 사유)
- **증상**: HTML 시각화에서 프레임 간격이 불규칙해 보임
- **원인**: 이전 `real_test_3m/7m`는 웨이포인트 기반 비행 → 각도 간격 std=9.4° (14°~43°)
- **조치**: 새 균일 데이터셋(`real_test_3m_uniform/7m_uniform`, std=1.5~2.5°) Google Drive에서 재다운로드, 기존 실험 전량 삭제

### 2. venv-mast3r 경로 오류
- **증상**: `source /home/sdh/venv-mast3r/bin/activate` → `No such file`
- **원인**: 실제 경로는 `/home/sdh/Desktop/venvs/venv-mast3r/`
- **조치**: 경로 수정

### 3. MASt3R-SfM `--weights` 인자 누락
- **증상**: `error: the following arguments are required: --weights`
- **조치**: `--weights /home/sdh/Desktop/models/MAST3R_2/checkpoints/MASt3R_ViTLarge_BaseDecoder_512_catmlpdpt_metric.pth` 추가

### 4. 합산 복원 시 7m ATE 악화 (inter-orbit scale mismatch)
- **증상**: 3m+7m 합산 MASt3R-SfM에서 7m ATE=27.6cm (단독 2.03cm 대비 13.6× 악화)
- **원인**: swin-5 그래프 경계(frame 12-16 ↔ frame 17)에서 cross-altitude feature matching 불충분 → 7m frame 0 이상치(103cm). 순서 바꿔도(7m 먼저) 동일하게 88cm 이상치 → 구조적 한계
- **조치**: 합산 유지 (지형 커버리지 우선). per-group Umeyama로 평가.

### 5. HTML 좌표계 틸트
- **증상**: 시각화에서 궤적이 ~18° 기울어짐
- **원인**: MASt3R world frame ≠ NED/ENU 정렬
- **조치**: per-group Umeyama로 GT NED 정렬 후 NED→ENU 변환 (`ned2enu = [y, x, -z]`)

### 6. conda 환경 이중 설치 혼란 (sysai3)
- **증상**: `conda activate gs2m` → `EnvironmentNameNotFound`
- **원인**: sysai3에 conda 두 곳: `~/miniconda3/` (MonoGS만 있음) vs `~/miniforge3/` (gs2m, gs3d, mast3r310, milo 등 전부)
- **조치**: GPU ML 환경은 모두 `source /home/sdh/miniforge3/bin/activate <env>` 사용

### 7. 3DGS `diff_gaussian_rasterization` 패키지 미발견
- **증상**: `gs_env`에서 import error
- **원인**: `gs_env`에 바닐라 3DGS 설치 안 됨
- **조치**: `miniforge3/envs/gs3d` 사용

### 8. MILo `FileNotFoundError: ./configs/fast`
- **증상**: `python3 /home/sdh/Desktop/models/milo/train.py` 실행 시 config 파일 못 찾음
- **원인**: config 경로가 상대경로(`./configs/`) 기준 → working directory가 `milo/milo/`이어야 함
- **조치**: `cd /home/sdh/Desktop/models/milo/milo/ && python3 train.py ...`

### 9. combined 디렉토리에 68장 (old symlink 잔류)
- **증상**: 34장이어야 할 `real_test_combined_uniform_rev/rgb/`에 68장 존재
- **원인**: 이전 데이터 삭제 후 디렉토리 재생성 시 구 symlink 미정리
- **조치**: `rm -rf real_test_combined_uniform_rev/` 후 재생성

---


## 문제 이력 — real_test (구 데이터) 복원 중 발생

### 10. 점군 outlier 문제 → SOR 실험 → 효과 없음 결론

**배경**: MILo/AGS 등 직접-위상 방법에서 메시 가장자리 스파이크/잡음 심각.
점군이 scene_extent의 4.9×까지 퍼진 것 확인 (COLMAP points3D full diag 15.84, scene_extent 1.99).

**가설 검증:**
- H1 스케일/좌표계 이상 → ❌ 기각 (scene_extent 정상 범위)
- H2 low-confidence outlier → ✅ 핵심 원인으로 판단 → SOR 실험 진행

**SOR 실험 경과:**

| 단계 | 설정 | 보존율 | 결과 |
|---|---|---|---|
| SOR1 | nb=20, std=2.0 | 94.6% | 가장자리 스파이크 여전히 존재 |
| SOR2 | nb=30, std=1.0 | 88.0% | MILo 재학습 → 수치 소폭 개선 |
| SOR3 | nb=30, std=0.5 | 81.7% | `real_test_milo_sor2.ply` 완료 |

**메시 품질 비교 (5개 큐브 GT 기준):**

| 모델 | CD (cm) | F@10cm |
|---|---|---|
| MILo baseline | 37.20 | 0.018 |
| MILo SOR2 (nb=30, std=0.5) | 30.72 | 0.024 |
| 차이 | △-6.5cm | △+0.006 |

**결론**: SOR로 floater 제거해도 CD 개선 미미, F-score 거의 변화 없음.
**근본 원인은 outlier가 아니라 texture 부족** (단색 배경 + 큐브 → photometric gradient 없음).
→ 알고리즘 튜닝으로는 해결 불가, 데이터 자체의 한계.

### 11. GOF (Gaussian Opacity Fields) 메시 추출 hanging
- **증상**: 학습 30k 완료 후 메시 추출 시 binary search step 7에서 멈춤, 무한대기
- **원인**: unbounded 야외 씬에서 GOF binary search가 occupancy threshold 수렴 실패로 추정
- **조치**: 타임아웃 후 포기, GOF 메시 결과 없음

### 12. depth prior가 오히려 악화 (blue_1)
- **증상**: `MILo+prior` CD=11.35cm > `MILo baseline` CD=9.40cm
- **원인**: 단색 파란 박스에서 MASt3R depth map 자체가 노이즈 심함 → 잘못된 depth 정보가 학습 왜곡
- **결론**: texture gradient 없는 씬에서 외부 depth supervision은 역효과. real_test에서도 prior 효과 미미 (21.44cm vs 21.54cm).

### 13. 3m_1 focal 버그 (첫 번째 시도 전량 폐기)
- **증상**: 학습 PSNR 16dB (정상 22dB+), 카메라가 어안렌즈처럼 동작
- **원인**: MASt3R는 512px 기준으로 focal 저장 → COLMAP 변환 시 원본 해상도 스케일백 누락
  - 잘못된 값: 359.88px (FOV 139°) → 올바른 값: `359.88 × (1920/512)` = **1349.55px** (FOV 71°)
- **조치**: `build_colmap.py`에 `focal = focal_512 * (W / 512)` 수정, 실험 전량 재실행
  - uniform 데이터에서는 focal=969.40px (`258.51 × 3.75`)

### 14. AGS input `colmap/` 심링크 누락
- **증상**: `"Could not recognize scene type!"` 오류
- **원인**: AGS는 입력 디렉토리 안에 `colmap/` 심링크 필수
- **조치**: `ln -s /path/to/colmap ./input_dir/colmap`

---


---

# PART E. 환경 / 유틸리티 / 데이터셋 (참고)

## 완료된 환경

| venv/conda | 상태 | 용도 |
|---|---|---|
| `venv-mast3r` | ✅ | MASt3R-SfM |
| `venv-2dgs` | ✅ | 2DGS (Python 3.10) |
| `venv-meshsplat10` | ✅ | MeshSplatting CVPR2026 |
| `conda ags_mesh` | ✅ | AGS-Mesh-2dgs (Python 3.10, PyTorch 2.1.2) |
| `venv-gof` | ✅ | GOF (NeurIPS2024, numpy 1.26.4, setuptools 69.5.1) |
| `/usr/bin/python3` | ✅ | open3d 0.13.0 (시스템) |

- **주의**: 환경 전환 시 `unset LD_LIBRARY_PATH` 필수
- `venv-pointmap`: python3.10이 venv-sdf 심링크 → 깨진 상태
- conda `recon3d`: PIL/libstdc++ 손상, 사용 불가

---


## 유틸리티 스크립트

| 스크립트 | 위치 | 용도 |
|---|---|---|
| `run_mast3r_sfm_with_depth.py` | `~/Desktop/MAST3R_2/` | MASt3R-SfM + depth map 저장 |
| `prepare_ags_data.py` | `~/Desktop/` | AGS-Mesh 데이터 디렉토리 준비 (blue_1용) |
| `prepare_ags_data_realtest.py` | `~/Desktop/` | AGS-Mesh 데이터 디렉토리 준비 (real_test용) |
| `mast3r_tsdf_fusion.py` | `~/Desktop/` | MASt3R depth → TSDF mesh |
| `mast3r_color_filter.py` | `~/Desktop/` | 파란 점 필터 → Poisson mesh |
| `blue_box_obb.py` | `~/Desktop/` | 컬러 필터 + OBB + Poisson |
| `inference_realesrgan.py` | `~/Desktop/Real-ESRGAN/` | 720p → FHD 업스케일 |
| `run_real_test_mesh.sh` | `~/Desktop/` | 2DGS→AGS prep→MILo→AGS 순차 실행 (real_test baseline)

---


## 데이터셋

| 객체 | 장수 | 상태 |
|---|---|---|
| blue_1 | 125 | ⚠️ 막힘 — blue_box_poisson2.ply 평가 후 BPA 또는 OBB 대체 고려 |
| blue_2 | 106 | ⬜ 대기 |
| streetlight_low | 75 | ⬜ 대기 |
| blue_person | 38 | ⬜ 대기 |

---


# PART F. 로켓/캔샛 하강 궤적 재현 & SfM/SLAM 평가 (2026-08-06~08-10)

> **기존 트랙과 다른 촬영 구도** — PART A~C는 대상을 둘러싸는 궤도(orbit) 촬영이다.
> 이 트랙은 **하강하면서 단 한 번 통과하는 궤적**이라 루프 클로저가 없고 기선/거리 비가
> 0.0054~0.0973로 극단적으로 작다. PART A~C의 교훈을 그대로 끌어오지 말 것.

## F.1 목적과 데이터 생성 조건

실제 비행 로그(PX4 `log_22_2026-8-6-15-04-00.ulg`, 2026-08-06 로켓/캔샛 하강)의 카메라 포즈를
합성 환경에서 재현해 프레임을 만들고, MASt3R-SfM / MASt3R-SLAM 복원 결과를 GT 포즈와 비교했다.
GT가 있는 트랙이므로 포즈 정확도 정량 비교가 가능하다.

| 항목 | 값 |
|---|---|
| GT 궤적 | PX4 `vehicle_local_position` / `vehicle_attitude` (NED), EKF 리셋 보정 적용 |
| 평가 구간 | t = 117.4 ~ 215.8 s (요청 112~240 s 중 대지고도 35 m 이상) |
| 고도 범위 (AGL) | 35 ~ 348 m |
| 기선/거리 비 | 0.0054 ~ 0.0973 |
| 렌더 | 1920×1080, 수평 FOV 90° (fx = 960 px) |
| SfM | MASt3R-SfM, 512 px 입력, `scene_graph=swin-5-noncyclic`, `shared_intrinsics=true` |
| SLAM | MASt3R-SLAM |
| GPU | A6000 (기존 트랙과 동일) |

변량은 **카메라 피치**(-45° / -90°)와 **지형**(city = 고층 빌딩 밀집, forest = 수목,
ground = 개활지) 두 축. 여기에 어안 왜곡(`fish`)과 그 역보정(`undist`) 변형이 붙는다.
`cap45`/`cap90`은 city 계열의 디렉터리 이름이다(문서에 따라 `city_45`/`city_90`으로도 표기됨).

## F.2 SfM 실행 실적 — 전 케이스 등록 성공

`~/Desktop/rocket_eval_captures/{name}_out/mast3r/run_stats.json` 10건 전수. 입력 프레임을
100% 등록하는 데는 모두 성공했다(정확도는 별개, F.4 참고).

| 데이터셋 | 프레임 | 쌍 | 정렬 시간 | 피크 VRAM | 산출 점 수 |
|---|---:|---:|---:|---:|---:|
| cap45 | 120 | 1170 | 478.5 s | 4.90 GB | 4,169,687 |
| cap90 | 120 | 1170 | 406.2 s | 5.84 GB | 3,558,608 |
| forest45 | 120 | 1170 | 477.4 s | 4.83 GB | 2,106,349 |
| forest90 | 120 | 1170 | 667.5 s | 5.59 GB | 2,673,748 |
| ground45 | 120 | 1170 | 826.8 s | 5.29 GB | 585,535 |
| ground90 | 120 | 1170 | 800.9 s | 5.78 GB | 753,914 |
| city90fish | 120 | 1170 | 659.7 s | 6.20 GB | 578,222 |
| city90undist | 120 | 1170 | 431.2 s | 6.23 GB | 1,531,573 |
| forest45fish | 120 | 1170 | 594.2 s | 5.70 GB | 627,980 |
| forest90stable | 80 | 770 | 300.2 s | 4.66 GB | 1,775,544 |

- **메모리가 아니라 시간이 제약이다.** 피크 VRAM 최대치(6.23 GB)도 A6000 49 GB의 13% 이하다.
  배치를 키워 시간을 줄일 여지가 남아 있다.
- **같은 120프레임/1170쌍인데 정렬 시간이 406 s ~ 827 s로 2배 이상 벌어진다.** 텍스처가 빈약한
  개활지(ground)에서 수렴이 느리다. 프레임 수만으로 처리 시간을 예측하면 안 된다.
- **산출 점 수는 장면에 따라 7배 차이**(578K ~ 4.17M). 점 수를 품질 지표로 단독 사용 금지 —
  PART A의 "수치 1위가 시각 1위는 아니다" 교훈과 같은 종류의 함정이다.

## F.3 SLAM 트래킹 성립 여부 — 16건 중 11건 실패

`~/Desktop/MASt3R-SLAM/logs/{name}/frames.txt` 줄 수(= 생성 키프레임 수) 전수.
입력은 120프레임(`_stable` 계열은 t≥155 s 구간 80프레임).

| 데이터셋 | 키프레임 | 판정 |
|---|---:|---|
| forest45_stable_relaxed | 33 | ✅ |
| cap45 (city, -45°) | 20 | ✅ |
| cap90 (city, -90°) | 14 | ✅ |
| forest45_stable | 12 | ✅ |
| forest90_stable | 11 | ✅ |
| forest45 / forest90 (전 구간) | 1 / 1 | ❌ |
| ground45 / ground90 | 1 / 1 | ❌ |
| ground45_relaxed / ground90_relaxed | 1 / 1 | ❌ |
| ground45_stable_relaxed / ground90_stable_relaxed | 1 / 1 | ❌ |
| city90fish / city90undist / forest45fish | 1 / 1 / 1 | ❌ |

키프레임 1개는 **결과가 나쁜 게 아니라 결과가 없는 상태**다. GT와의 공통 프레임이 3개 미만이라
Sim(3) 정렬 자체가 성립하지 않아 ATE·회전오차를 산출할 수 없다. 실패 원인은 셋으로 갈리고,
각각 대응이 다르다.

**① 입력 구간 문제 (파이프라인 정상, 입력이 부적합)**
궤적 초반 약 40프레임(t=112~155 s)은 낙하산 전개 직후 기체가 격렬히 회전(각속도 ~20 rad/s,
직립 기준 최대 155° 이탈)해 동체 장착 카메라가 대부분 하늘을 향한다. 이 구간을 잘라낸
`_stable` 계열은 forest에서 **실패(1 kf) → 성립(11~12 kf)** 으로 뒤집혔고, 매칭 조건을 완화한
`forest45_stable_relaxed`(`reloc.min_match_frac 0.3→0.1`, `strict True→False`)는 33 kf까지 올랐다.
→ **프레임 순서 보존만으로는 부족하고, 트래킹 가능 구간을 판별해 잘라내는 전처리가 필요하다.**

**② 테스트 환경 제약 (파이프라인 무관)**
ground 지형은 구간을 자르고 조건을 완화해도 4개 변형 전부 1 kf에서 멈췄다. 지형 텍스처가
완전히 반복되는 타일 패턴이라 특징점 매칭이 원천적으로 불가능하다(시각 확인함).
→ **합성 지형 머티리얼의 한계이므로 ground 실패를 파이프라인 결함으로 계상하면 안 된다.**
근본 해결은 지형 텍스처 교체뿐이다.

**③ 렌즈 모델 불일치**
어안(`fish`)과 그 역보정(`undist`) 변형은 전부 실패. 이 합성 어안은 진짜 어안 렌더가 아니라
광각을 흉내낸 재투영이고(지평선이 직선으로 나옴), MASt3R-SLAM의 `Intrinsics.from_calib`도
OpenCV radial-tangential 왜곡만 지원하며 `cv2.fisheye` 모델이 없다.
→ **실촬영 어안 영상을 투입하려면 별도 dewarp 단계가 선행되어야 한다.**

## F.4 ⚠️ ATE 수치는 아직 확정하지 말 것 — 두 평가 경로가 상충

포즈 정확도 절대값은 **이 문서에 결론으로 싣지 않는다.** 이유 둘.

**(1) 평가 지형이 더미다.** 서버 평가 리포트(`{name}_out/eval/report.md`)가 지형을 `fractal`
더미 데이터로 명시하고, 실제 정사영상/DSM으로 교체하면 수치가 달라진다고 스스로 경고한다.

**(2) 같은 데이터셋에 대해 두 평가 경로가 다른 값을 낸다.**

| 데이터셋 | 평가 A (작업 기록 `mem.md`) | 평가 B (서버 `eval/pose_metrics.json`, 8/10 22:41) |
|---|---|---|
| city90undist SfM | ATE 53.01 m / 회전 11.73° | ATE 34.05 m / 회전 median 102.5° |
| city90fish SfM | ATE 40.68 m / 회전 30.46° | ATE 31.06 m / 회전 median 102.2° |

ATE도 다르지만 회전오차가 한 자릿수 대 100° 대로 갈린다. 카메라 축 규약(FRD ↔ OpenCV) 처리
차이가 유력하나, **평가 A를 생성한 `eval_pose_accuracy.py`가 임시 폴더에 있다가 소실되어
재현이 불가능**해 어느 쪽이 옳은지 판정하지 못했다.

→ **다음에 할 일**: 축 규약과 정렬 방식(Sim(3) Umeyama)을 명시한 **단일 평가 스크립트를 이
저장소에 포함**시키고, 그 출력만 근거로 인정한다. 실지형 교체 후 재측정하기 전까지 절대값
비교는 보류.

참고로 작업 기록에 "SfM 300~594 s, VRAM 4.66~5.70 GB"로 적혀 있던 실행 실적도 실제보다
좁았다(실측 최대 826.8 s / 6.23 GB, F.2 표). 요약 수치는 원본 `run_stats.json`에서 재확인할 것.

## F.5 미완결

- **`forest90stable` SLAM 미실행** — 8/10 16:30에 SfM(Step 4)까지 끝나고 `poses.json` /
  `points.ply`(1,775,544점)는 저장됐으나 SLAM 단계 로그가 없고 `ALL_DONE_FOREST90STABLE`
  마커도 없다. `MASt3R-SLAM/logs/`에도 항목 없음. 크래시가 아니라 중단으로 보임.
  (F.3 표의 `forest90_stable` 11 kf는 13:59에 돌린 **다른 디렉터리** `forest90_out_stable/`
  결과이므로 혼동 금지)
- **ground 지형 텍스처 교체** — ②의 근본 해결. 미착수.
- **평가 스크립트 일원화** — F.4. 미착수.

## F.6 서버 산출물 위치

```
~/Desktop/rocket_eval_captures/
  config_{cap45,cap90,ground45,ground90,forest45,forest90,
          city90fish,city90undist,forest45fish,forest90stable}.yaml
  {name}_out/render/frames/*.png          렌더 프레임
  {name}_out/mast3r/{poses.json,points.ply,run_stats.json}
  {city90fish,city90undist}_out/eval/     pose_metrics.json, cloud_metrics.json, report.md
  {ground45,ground90,forest45,forest90}_out_stable/   t≥155 s 서브셋(프레임 40-119 → 0000-0079)
  logs/  sfm_*.log, slam_*.log, driver_*.log, ALL_DONE_*
~/Desktop/MASt3R-SLAM/logs/{name}/{frames.txt,frames.ply}
```

> 합성 렌더라 모션블러·롤링셔터가 실제 카메라와 다르다. 이 결과를 실촬영 예상 성능으로
> 그대로 일반화하지 말 것.
