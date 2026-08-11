import argparse
from pathlib import Path

import cv2
import numpy as np


def load_color(path: str):
    img = cv2.imread(path, cv2.IMREAD_COLOR)
    if img is None:
        raise FileNotFoundError(f"이미지를 읽을 수 없습니다: {path}")
    return img


def create_sift(nfeatures: int):
    if not hasattr(cv2, "SIFT_create"):
        raise RuntimeError("현재 OpenCV에서 SIFT를 사용할 수 없습니다.")
    return cv2.SIFT_create(nfeatures=nfeatures)


def find_homography(img1, img2, ratio: float, ransac_thresh: float, nfeatures: int):
    gray1 = cv2.cvtColor(img1, cv2.COLOR_BGR2GRAY)
    gray2 = cv2.cvtColor(img2, cv2.COLOR_BGR2GRAY)

    sift = create_sift(nfeatures)
    kp1, desc1 = sift.detectAndCompute(gray1, None)
    kp2, desc2 = sift.detectAndCompute(gray2, None)

    if desc1 is None or desc2 is None:
        raise RuntimeError("디스크립터를 생성하지 못했습니다.")

    matcher = cv2.BFMatcher(cv2.NORM_L2)
    knn = matcher.knnMatch(desc1, desc2, k=2)

    good = []
    for pair in knn:
        if len(pair) < 2:
            continue
        m, n = pair
        if m.distance < ratio * n.distance:
            good.append(m)

    if len(good) < 4:
        raise RuntimeError(f"호모그래피 계산에 필요한 매칭 수가 부족합니다: {len(good)}")

    pts1 = np.float32([kp1[m.queryIdx].pt for m in good]).reshape(-1, 1, 2)
    pts2 = np.float32([kp2[m.trainIdx].pt for m in good]).reshape(-1, 1, 2)

    h, mask = cv2.findHomography(pts2, pts1, cv2.RANSAC, ransac_thresh)
    if h is None:
        raise RuntimeError("호모그래피 추정에 실패했습니다.")

    inliers = int(mask.ravel().sum()) if mask is not None else 0
    return h, len(good), inliers


def stitch(img1, img2, h):
    h1, w1 = img1.shape[:2]
    h2, w2 = img2.shape[:2]

    corners1 = np.float32([[0, 0], [w1, 0], [w1, h1], [0, h1]]).reshape(-1, 1, 2)
    corners2 = np.float32([[0, 0], [w2, 0], [w2, h2], [0, h2]]).reshape(-1, 1, 2)

    warped_corners2 = cv2.perspectiveTransform(corners2, h)
    all_corners = np.concatenate((corners1, warped_corners2), axis=0)

    [xmin, ymin] = np.floor(all_corners.min(axis=0).ravel()).astype(int)
    [xmax, ymax] = np.ceil(all_corners.max(axis=0).ravel()).astype(int)

    tx, ty = -xmin, -ymin
    t = np.array([[1, 0, tx], [0, 1, ty], [0, 0, 1]], dtype=np.float64)

    width = int(xmax - xmin)
    height = int(ymax - ymin)

    canvas2 = cv2.warpPerspective(img2, t @ h, (width, height))
    canvas1 = np.zeros_like(canvas2)
    canvas1[ty:ty + h1, tx:tx + w1] = img1

    mask1 = (canvas1.sum(axis=2) > 0).astype(np.uint8)
    mask2 = (canvas2.sum(axis=2) > 0).astype(np.uint8)

    overlap = (mask1 == 1) & (mask2 == 1)
    only1 = (mask1 == 1) & (mask2 == 0)
    only2 = (mask1 == 0) & (mask2 == 1)

    out = np.zeros_like(canvas1)
    out[only1] = canvas1[only1]
    out[only2] = canvas2[only2]
    out[overlap] = ((canvas1[overlap].astype(np.float32) + canvas2[overlap].astype(np.float32)) * 0.5).astype(np.uint8)

    return out


def parse_args():
    parser = argparse.ArgumentParser(description="두 이미지 파노라마 스티칭")
    parser.add_argument("--img1", required=True, help="기준 이미지")
    parser.add_argument("--img2", required=True, help="워프할 이미지")
    parser.add_argument("--output", default="./result/stitched.jpg", help="출력 파일 경로")
    parser.add_argument("--ratio", type=float, default=0.75, help="Lowe ratio")
    parser.add_argument("--ransac-thresh", type=float, default=4.0, help="RANSAC reprojection threshold")
    parser.add_argument("--nfeatures", type=int, default=2000, help="SIFT 특징점 개수")
    return parser.parse_args()


def main():
    args = parse_args()
    img1 = load_color(args.img1)
    img2 = load_color(args.img2)

    h, total_matches, inliers = find_homography(
        img1,
        img2,
        ratio=args.ratio,
        ransac_thresh=args.ransac_thresh,
        nfeatures=args.nfeatures,
    )

    pano = stitch(img1, img2, h)

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    cv2.imwrite(str(out_path), pano)

    print(f"matches: {total_matches}")
    print(f"inliers: {inliers}")
    print(f"saved: {out_path.resolve()}")


if __name__ == "__main__":
    main()
