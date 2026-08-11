# SIFT 사용법 + 현재 시스템 구조/로직 문서

## 1) 이미지 경로는 어디에 넣나?
현재 구현은 실행할 때 인자(옵션)로 넣습니다.

- `--img1`: 첫 번째 이미지 경로
- `--img2`: 두 번째 이미지 경로
- `--img-dir`: 이미지 폴더 경로 (폴더만 주고 자동 선택 가능)
- `--idx1`, `--idx2`: 폴더 모드에서 선택할 이미지 인덱스
- `--img1-name`, `--img2-name`: 폴더 모드에서 파일명 직접 지정
- `--output`: 결과 이미지 저장 경로 (선택, 기본값 `sift_matches.jpg`)

예시 (현재 폴더 기준):

```bash
python Features.py --img1 ./image1.jpg --img2 ./image2.jpg --output ./result/sift_matches.jpg
```

예시 (Windows 절대 경로, 파일 2개 직접 지정):

```bash
python Features.py --img1 "C:/data/a.jpg" --img2 "C:/data/b.jpg" --output "C:/data/out/matches.jpg"
```

예시 (폴더 경로만 사용, 정렬 기준 첫 번째/두 번째 파일 자동 선택):

```bash
python Features.py --img-dir "D:/epic/CitySample/airsim_dataset/grid_merge/rgb" --output "./result/sift_matches.jpg"
```

예시 (폴더에서 인덱스로 선택):

```bash
python Features.py --img-dir "D:/epic/CitySample/airsim_dataset/grid_merge/rgb" --idx1 10 --idx2 25 --output "./result/sift_matches.jpg"
```

예시 (폴더에서 파일명으로 선택):

```bash
python3 Features.py --img-dir "D:/epic/CitySample/airsim_dataset/grid_merge/rgb" --img1-name "000000.png" --img2-name "000001.png" --output "./result/sift_matches.jpg"
```

코드 기준으로 이미지 경로가 들어가는 지점:

- 인자 정의: `parse_args()`
  - `--img1`, `--img2`, `--img-dir` 등 정의
- 입력 해석: `resolve_input_images(args)`
  - 파일 모드 또는 폴더 모드로 실제 `img1`, `img2` 경로 결정
- 실제 사용: `main()`
  - `img1_path, img2_path = resolve_input_images(args)`
  - `image1 = load_gray_image(img1_path)`
  - `image2 = load_gray_image(img2_path)`

## 2) 현재 시스템 구조

### 파일 관점
- `Features.py`: SIFT 추출/매칭/시각화 전체 파이프라인 실행 파일
- `SIFT_SYSTEM_DOC.md`: 이 문서

### 처리 단계 관점
1. 실행 인자 파싱 (`parse_args`)
2. 입력 이미지 경로 확정 (`resolve_input_images`)
3. 입력 이미지 로드 (`load_gray_image`)
4. SIFT 생성 (`create_sift`)
5. 특징점 + 디스크립터 추출 (`detect_and_describe`)
6. BFMatcher KNN 매칭 (`match_descriptors`)
7. Lowe ratio test로 좋은 매칭 필터링 (`match_descriptors`)
8. 매칭 이미지 그리기 (`draw_matches`)
9. 결과 파일 저장 및 통계 출력 (`main`)

## 3) 로직 상세

### 3-1. 입력/검증
- `cv2.imread(..., cv2.IMREAD_GRAYSCALE)`로 그레이스케일 로드
- 로드 실패 시 `FileNotFoundError` 발생

### 3-2. 특징 추출
- `cv2.SIFT_create(...)`로 SIFT 객체 생성
- OpenCV에 SIFT가 없으면 `RuntimeError` 발생
- `detectAndCompute`로 `keypoints`, `descriptors` 획득

### 3-3. 매칭
- `cv2.BFMatcher(cv2.NORM_L2)` 사용
- `knnMatch(..., k=2)` 수행
- 각 후보 쌍 `(m, n)`에 대해 `m.distance < ratio * n.distance`이면 채택
- 채택된 매칭을 거리 오름차순 정렬

### 3-4. 시각화/출력
- `cv2.drawMatches`로 매칭 결과 렌더링
- `--max-matches`만큼만 시각화
- 결과 이미지를 `--output` 경로로 저장
- 콘솔 출력:
  - `img1 keypoints`
  - `img2 keypoints`
  - `good matches`
  - `saved`

## 4) 데이터 흐름 요약

```text
img1, img2 경로
  -> 이미지 로드
  -> SIFT 특징 추출
  -> 디스크립터 KNN 매칭
  -> ratio test 필터링
  -> 매칭 이미지 생성
  -> 파일 저장 + 통계 출력
```

## 5) 자주 생기는 이슈
- `SIFT_create` 오류: `opencv-contrib-python` 미설치 가능성 높음
- 매칭이 너무 적음:
  - `--ratio`를 조금 키워보기 (예: `0.8`)
  - 이미지 해상도/겹치는 영역 확인
- 결과가 너무 복잡함:
  - `--max-matches`를 줄여서 시각화

## 6) 전체 매핑 파이프라인 실행 (신규)

매핑 폴더에 아래 파일이 추가되었습니다.

- `pipeline_3d_mapping.py`: RGB 시퀀스 기반 포즈 누적 + (선택) sparse triangulation + (선택) dense depth 누적

### 6-1. 기본 실행 (RGB + 카메라 내참수)

```bash
python3 pipeline_3d_mapping.py \
  --rgb-dir "D:/epic/CitySample/airsim_dataset/grid_merge/rgb" \
  --fx 320 --fy 320 --cx 320 --cy 240 \
  --detector sift \
  --save-pose "./output/trajectory.txt"
```

### 6-2. Sparse point cloud까지 생성

```bash
python3 pipeline_3d_mapping.py \
  --rgb-dir "D:/epic/CitySample/airsim_dataset/grid_merge/rgb" \
  --fx 320 --fy 320 --cx 320 --cy 240 \
  --triangulate \
  --save-sparse "./output/sparse_points.ply"
```

### 6-3. Dense depth 누적 (depth 폴더가 있을 때)

```bash
python3 pipeline_3d_mapping.py \
  --rgb-dir "D:/epic/CitySample/airsim_dataset/grid_merge/rgb" \
  --depth-dir "D:/epic/CitySample/airsim_dataset/grid_merge/depth_npy" \
  --fx 320 --fy 320 --cx 320 --cy 240 \
  --depth-stride 4 --max-depth 80 \
  --save-dense "./output/dense_points.ply"
```

### 6-4. UWB 스케일 보정 사용

`--uwb-file`은 프레임 쌍 간 거리값을 한 줄에 하나씩 넣은 텍스트 파일입니다.

```text
0.52
0.48
0.55
...
```

```bash
python3 pipeline_3d_mapping.py \
  --rgb-dir "D:/epic/CitySample/airsim_dataset/grid_merge/rgb" \
  --uwb-file "./uwb_distances.txt" \
  --fx 320 --fy 320 --cx 320 --cy 240
```

### 6-5. 출력 파일

- `trajectory.txt`: 프레임별 pose (R 3x3 + t 3)
- `sparse_points.ply`: triangulation 기반 sparse 점군
- `dense_points.ply`: depth 누적 dense 점군
