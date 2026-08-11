import cv2
import numpy as np

IMG1 = "/mnt/d/epic/CitySample/airsim_dataset/grid_merge/rgb/000000.png"
IMG2 = "/mnt/d/epic/CitySample/airsim_dataset/grid_merge/rgb/000001.png"

# 카메라 내부파라미터 예시
# 반드시 당신 데이터 기준으로 바꿔야 함
W = 256
H = 144
FOV_DEG = 90.0

fx = W / (2.0 * np.tan(np.deg2rad(FOV_DEG / 2.0)))
fy = fx
cx = W / 2.0
cy = H / 2.0

K = np.array([
    [fx, 0,  cx],
    [0,  fy, cy],
    [0,  0,  1 ]
], dtype=np.float64)

img1 = cv2.imread(IMG1, cv2.IMREAD_GRAYSCALE)
img2 = cv2.imread(IMG2, cv2.IMREAD_GRAYSCALE)

if img1 is None or img2 is None:
    raise FileNotFoundError("이미지 경로 확인 필요")

# 1) ORB 특징점
orb = cv2.ORB_create(
    nfeatures=3000,
    scaleFactor=1.2,
    nlevels=8
)

kp1, des1 = orb.detectAndCompute(img1, None)
kp2, des2 = orb.detectAndCompute(img2, None)

if des1 is None or des2 is None:
    raise RuntimeError("특징점 추출 실패")

# 2) 매칭
bf = cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=True)
matches = bf.match(des1, des2)

if len(matches) < 8:
    raise RuntimeError(f"매칭 수 부족: {len(matches)}")

matches = sorted(matches, key=lambda m: m.distance)

# 상위 매칭만 사용
good = matches[:500]

pts1 = np.float64([kp1[m.queryIdx].pt for m in good])
pts2 = np.float64([kp2[m.trainIdx].pt for m in good])

# 3) Essential Matrix
E, mask = cv2.findEssentialMat(
    pts1, pts2, K,
    method=cv2.RANSAC,
    prob=0.999,
    threshold=1.0
)

if E is None:
    raise RuntimeError("Essential matrix 계산 실패")

inlier_pts1 = pts1[mask.ravel() == 1]
inlier_pts2 = pts2[mask.ravel() == 1]

if len(inlier_pts1) < 8:
    raise RuntimeError(f"inlier 부족: {len(inlier_pts1)}")

# 4) R, t 복원
num_inliers, R, t, pose_mask = cv2.recoverPose(E, inlier_pts1, inlier_pts2, K)

print("recoverPose inliers:", int(num_inliers))
print("R =\n", R)
print("t =\n", t)

# t는 방향만 맞고 크기는 모름
# 즉 monocular에서는 scale이 없음