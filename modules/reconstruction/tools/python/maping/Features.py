import argparse
import re
from pathlib import Path

import cv2


IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff", ".webp"}


def normalize_path(path_str: str):
	original = Path(path_str)
	if original.exists():
		return original

	# WSL/Linux 환경에서 Windows 경로(D:/...)를 /mnt/d/...로 변환
	win_match = re.match(r"^([A-Za-z]):[\\/](.*)$", path_str)
	if win_match:
		drive = win_match.group(1).lower()
		rest = win_match.group(2).replace("\\", "/")
		wsl_path = Path(f"/mnt/{drive}/{rest}")
		if wsl_path.exists():
			return wsl_path

	# Windows 환경에서 /mnt/d/... 경로를 D:/...로 변환
	wsl_match = re.match(r"^/mnt/([A-Za-z])/(.*)$", path_str)
	if wsl_match:
		drive = wsl_match.group(1).upper()
		rest = wsl_match.group(2).replace("\\", "/")
		win_path = Path(f"{drive}:/{rest}")
		if win_path.exists():
			return win_path

	return original


def load_gray_image(image_path: str):
	resolved = normalize_path(image_path)
	image = cv2.imread(str(resolved), cv2.IMREAD_GRAYSCALE)
	if image is None:
		raise FileNotFoundError(f"이미지를 읽을 수 없습니다: {image_path} (해석 경로: {resolved})")
	return image


def list_image_files(image_dir: Path):
	image_dir = normalize_path(str(image_dir))
	if not image_dir.exists() or not image_dir.is_dir():
		raise FileNotFoundError(f"유효한 이미지 폴더가 아닙니다: {image_dir}")

	files = [p for p in image_dir.iterdir() if p.is_file() and p.suffix.lower() in IMAGE_EXTENSIONS]
	files = sorted(files, key=lambda p: p.name.lower())
	if len(files) < 2:
		raise ValueError(f"이미지 폴더에 최소 2장의 이미지가 필요합니다: {image_dir}")
	return files


def resolve_input_images(args):
	if args.img1 and args.img2:
		return str(normalize_path(args.img1)), str(normalize_path(args.img2))

	if args.img_dir:
		image_dir = normalize_path(args.img_dir)
		files = list_image_files(image_dir)

		if args.img1_name and args.img2_name:
			img1 = image_dir / args.img1_name
			img2 = image_dir / args.img2_name
			if not img1.exists():
				raise FileNotFoundError(f"img1 파일이 없습니다: {img1}")
			if not img2.exists():
				raise FileNotFoundError(f"img2 파일이 없습니다: {img2}")
			return str(img1), str(img2)

		if args.idx1 < 0 or args.idx2 < 0:
			raise ValueError("idx1, idx2는 0 이상이어야 합니다")
		if args.idx1 >= len(files) or args.idx2 >= len(files):
			raise IndexError(
				f"idx 범위를 벗어났습니다. 전체 이미지 수: {len(files)}, 요청 idx: {args.idx1}, {args.idx2}"
			)
		if args.idx1 == args.idx2:
			raise ValueError("idx1과 idx2는 서로 달라야 합니다")

		return str(files[args.idx1]), str(files[args.idx2])

	raise ValueError(
		"입력 이미지 지정이 필요합니다. --img1/--img2 또는 --img-dir 옵션을 사용하세요."
	)


def create_sift(nfeatures: int, contrast_threshold: float, edge_threshold: float, sigma: float):
	if not hasattr(cv2, "SIFT_create"):
		raise RuntimeError(
			"현재 OpenCV 빌드에서 SIFT를 사용할 수 없습니다. "
			"opencv-contrib-python 설치를 확인하세요."
		)

	return cv2.SIFT_create(
		nfeatures=nfeatures,
		contrastThreshold=contrast_threshold,
		edgeThreshold=edge_threshold,
		sigma=sigma,
	)


def detect_and_describe(sift, image):
	keypoints, descriptors = sift.detectAndCompute(image, None)
	if descriptors is None:
		descriptors = []
	return keypoints, descriptors


def match_descriptors(desc1, desc2, ratio: float):
	if len(desc1) == 0 or len(desc2) == 0:
		return []

	matcher = cv2.BFMatcher(cv2.NORM_L2)
	knn_matches = matcher.knnMatch(desc1, desc2, k=2)

	good_matches = []
	for pair in knn_matches:
		if len(pair) < 2:
			continue
		m, n = pair
		if m.distance < ratio * n.distance:
			good_matches.append(m)

	return sorted(good_matches, key=lambda m: m.distance)


def draw_matches(image1, keypoints1, image2, keypoints2, matches, max_matches: int):
	selected = matches[:max_matches] if max_matches > 0 else matches
	return cv2.drawMatches(
		image1,
		keypoints1,
		image2,
		keypoints2,
		selected,
		None,
		flags=cv2.DrawMatchesFlags_NOT_DRAW_SINGLE_POINTS,
	)


def parse_args():
	parser = argparse.ArgumentParser(description="SIFT 특징점 추출 + 매칭 예제")
	parser.add_argument("--img1", help="첫 번째 이미지 경로")
	parser.add_argument("--img2", help="두 번째 이미지 경로")
	parser.add_argument("--img-dir", help="이미지 폴더 경로 (img1/img2 대신 사용 가능)")
	parser.add_argument("--img1-name", help="img-dir 내부 첫 번째 파일명")
	parser.add_argument("--img2-name", help="img-dir 내부 두 번째 파일명")
	parser.add_argument("--idx1", type=int, default=0, help="img-dir 사용 시 첫 번째 이미지 인덱스")
	parser.add_argument("--idx2", type=int, default=1, help="img-dir 사용 시 두 번째 이미지 인덱스")
	parser.add_argument("--output", default="sift_matches.jpg", help="매칭 결과 이미지 저장 경로")
	parser.add_argument("--ratio", type=float, default=0.75, help="Lowe ratio test 임계값")
	parser.add_argument("--max-matches", type=int, default=80, help="시각화할 최대 매칭 수")
	parser.add_argument("--nfeatures", type=int, default=0, help="SIFT가 찾을 최대 특징점 수 (0이면 제한 없음)")
	parser.add_argument("--contrast-threshold", type=float, default=0.04, help="SIFT contrastThreshold")
	parser.add_argument("--edge-threshold", type=float, default=10.0, help="SIFT edgeThreshold")
	parser.add_argument("--sigma", type=float, default=1.6, help="SIFT sigma")
	return parser.parse_args()


def main():
	args = parse_args()
	img1_path, img2_path = resolve_input_images(args)

	image1 = load_gray_image(img1_path)
	image2 = load_gray_image(img2_path)

	sift = create_sift(
		nfeatures=args.nfeatures,
		contrast_threshold=args.contrast_threshold,
		edge_threshold=args.edge_threshold,
		sigma=args.sigma,
	)

	kp1, desc1 = detect_and_describe(sift, image1)
	kp2, desc2 = detect_and_describe(sift, image2)

	matches = match_descriptors(desc1, desc2, ratio=args.ratio)
	match_image = draw_matches(image1, kp1, image2, kp2, matches, args.max_matches)

	output_path = Path(args.output)
	output_path.parent.mkdir(parents=True, exist_ok=True)
	cv2.imwrite(str(output_path), match_image)

	print(f"img1 keypoints: {len(kp1)}")
	print(f"img2 keypoints: {len(kp2)}")
	print(f"good matches: {len(matches)}")
	print(f"img1 path: {img1_path}")
	print(f"img2 path: {img2_path}")
	print(f"saved: {output_path.resolve()}")


if __name__ == "__main__":
	main()
