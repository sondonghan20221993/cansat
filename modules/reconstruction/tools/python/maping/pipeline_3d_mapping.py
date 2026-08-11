import argparse
from pathlib import Path
from typing import List, Optional, Tuple

import cv2
import numpy as np


IMAGE_EXTENSIONS = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff", ".webp"}


def list_images(rgb_dir: Path) -> List[Path]:
    if not rgb_dir.exists() or not rgb_dir.is_dir():
        raise FileNotFoundError(f"Invalid rgb directory: {rgb_dir}")
    images = [p for p in rgb_dir.iterdir() if p.is_file() and p.suffix.lower() in IMAGE_EXTENSIONS]
    images.sort(key=lambda p: p.name)
    if len(images) < 2:
        raise ValueError("Need at least 2 RGB images")
    return images


def make_intrinsics(fx: float, fy: float, cx: float, cy: float) -> np.ndarray:
    return np.array([[fx, 0.0, cx], [0.0, fy, cy], [0.0, 0.0, 1.0]], dtype=np.float64)


def create_detector(detector_name: str, nfeatures: int):
    name = detector_name.lower()
    if name == "sift":
        if not hasattr(cv2, "SIFT_create"):
            raise RuntimeError("SIFT is unavailable in current OpenCV build")
        return cv2.SIFT_create(nfeatures=nfeatures)
    if name == "orb":
        return cv2.ORB_create(nfeatures=nfeatures if nfeatures > 0 else 2000)
    raise ValueError(f"Unsupported detector: {detector_name}")


def detect_and_describe(detector, image_gray: np.ndarray):
    keypoints, descriptors = detector.detectAndCompute(image_gray, None)
    if descriptors is None:
        descriptors = np.empty((0, 128), dtype=np.float32)
    return keypoints, descriptors


def match_descriptors(desc1: np.ndarray, desc2: np.ndarray, detector_name: str, ratio: float):
    if len(desc1) == 0 or len(desc2) == 0:
        return []

    if detector_name.lower() == "orb":
        norm_type = cv2.NORM_HAMMING
    else:
        norm_type = cv2.NORM_L2

    matcher = cv2.BFMatcher(norm_type)
    knn = matcher.knnMatch(desc1, desc2, k=2)
    good = []
    for pair in knn:
        if len(pair) < 2:
            continue
        m, n = pair
        if m.distance < ratio * n.distance:
            good.append(m)
    return sorted(good, key=lambda x: x.distance)


def points_from_matches(kp1, kp2, matches):
    pts1 = np.float32([kp1[m.queryIdx].pt for m in matches])
    pts2 = np.float32([kp2[m.trainIdx].pt for m in matches])
    return pts1, pts2


def estimate_relative_pose(pts1: np.ndarray, pts2: np.ndarray, k: np.ndarray, ransac_thr: float):
    e, mask = cv2.findEssentialMat(pts1, pts2, k, method=cv2.RANSAC, prob=0.999, threshold=ransac_thr)
    if e is None:
        raise RuntimeError("Essential matrix estimation failed")

    _, r, t, recover_mask = cv2.recoverPose(e, pts1, pts2, k)
    inlier_mask = (mask.ravel() > 0) & (recover_mask.ravel() > 0)
    return r, t.reshape(3), inlier_mask


def rt_to_transform(r: np.ndarray, t: np.ndarray) -> np.ndarray:
    tmat = np.eye(4, dtype=np.float64)
    tmat[:3, :3] = r
    tmat[:3, 3] = t
    return tmat


def invert_transform(tmat: np.ndarray) -> np.ndarray:
    r = tmat[:3, :3]
    t = tmat[:3, 3]
    inv = np.eye(4, dtype=np.float64)
    inv[:3, :3] = r.T
    inv[:3, 3] = -r.T @ t
    return inv


def triangulate_points_world(
    pts_prev: np.ndarray,
    pts_curr: np.ndarray,
    k: np.ndarray,
    t_w_c_prev: np.ndarray,
    t_w_c_curr: np.ndarray,
) -> np.ndarray:
    t_c_w_prev = invert_transform(t_w_c_prev)
    t_c_w_curr = invert_transform(t_w_c_curr)

    p1 = k @ t_c_w_prev[:3, :]
    p2 = k @ t_c_w_curr[:3, :]

    x_h = cv2.triangulatePoints(p1, p2, pts_prev.T, pts_curr.T)
    x_h = x_h / np.clip(x_h[3:4], 1e-12, None)
    return x_h[:3].T


def read_depth_map(depth_dir: Optional[Path], rgb_path: Path, depth_scale: float) -> Optional[np.ndarray]:
    if depth_dir is None:
        return None

    stem = rgb_path.stem
    npy_path = depth_dir / f"{stem}.npy"
    png_path = depth_dir / f"{stem}.png"

    if npy_path.exists():
        depth = np.load(npy_path).astype(np.float32)
        return depth

    if png_path.exists():
        depth_raw = cv2.imread(str(png_path), cv2.IMREAD_UNCHANGED)
        if depth_raw is None:
            return None
        depth = depth_raw.astype(np.float32) / depth_scale
        return depth

    return None


def depth_to_camera_points(depth: np.ndarray, k: np.ndarray, stride: int, max_depth: float) -> np.ndarray:
    h, w = depth.shape
    ys, xs = np.mgrid[0:h:stride, 0:w:stride]
    z = depth[0:h:stride, 0:w:stride]

    valid = (z > 0.0) & np.isfinite(z)
    if max_depth > 0:
        valid &= z < max_depth

    xs = xs[valid].astype(np.float64)
    ys = ys[valid].astype(np.float64)
    z = z[valid].astype(np.float64)

    fx, fy = k[0, 0], k[1, 1]
    cx, cy = k[0, 2], k[1, 2]

    x = (xs - cx) * z / fx
    y = (ys - cy) * z / fy

    return np.stack([x, y, z], axis=1)


def transform_points(t_w_c: np.ndarray, points_c: np.ndarray) -> np.ndarray:
    if len(points_c) == 0:
        return points_c
    r = t_w_c[:3, :3]
    t = t_w_c[:3, 3]
    return (r @ points_c.T).T + t


def save_ply_xyz(path: Path, points: np.ndarray):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        f.write("ply\n")
        f.write("format ascii 1.0\n")
        f.write(f"element vertex {len(points)}\n")
        f.write("property float x\n")
        f.write("property float y\n")
        f.write("property float z\n")
        f.write("end_header\n")
        for p in points:
            f.write(f"{p[0]} {p[1]} {p[2]}\n")


def load_uwb_distances(path: Optional[Path]) -> Optional[List[float]]:
    if path is None:
        return None
    if not path.exists():
        raise FileNotFoundError(f"UWB file not found: {path}")

    distances = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line:
            continue
        distances.append(float(line))
    return distances


def parse_args():
    parser = argparse.ArgumentParser(description="Incremental 3D mapping pipeline")
    parser.add_argument("--rgb-dir", required=True, help="RGB image sequence directory")
    parser.add_argument("--depth-dir", help="Depth map directory (.npy or .png)")
    parser.add_argument("--uwb-file", help="Optional UWB distances per frame-pair (one float per line)")

    parser.add_argument("--fx", type=float, required=True)
    parser.add_argument("--fy", type=float, required=True)
    parser.add_argument("--cx", type=float, required=True)
    parser.add_argument("--cy", type=float, required=True)

    parser.add_argument("--detector", choices=["sift", "orb"], default="sift")
    parser.add_argument("--nfeatures", type=int, default=2000)
    parser.add_argument("--ratio", type=float, default=0.75)
    parser.add_argument("--min-matches", type=int, default=30)
    parser.add_argument("--ransac-thr", type=float, default=1.0)

    parser.add_argument("--triangulate", action="store_true", help="Enable sparse triangulation")
    parser.add_argument(
        "--min-triangulation-inliers",
        type=int,
        default=8,
        help="Minimum inliers required to run triangulation",
    )
    parser.add_argument("--depth-scale", type=float, default=1000.0, help="Depth png scale divisor")
    parser.add_argument("--depth-stride", type=int, default=4)
    parser.add_argument("--max-depth", type=float, default=80.0)

    parser.add_argument("--save-pose", default="./output/trajectory.txt")
    parser.add_argument("--save-sparse", default="./output/sparse_points.ply")
    parser.add_argument("--save-dense", default="./output/dense_points.ply")
    return parser.parse_args()


def main():
    args = parse_args()

    rgb_dir = Path(args.rgb_dir)
    depth_dir = Path(args.depth_dir) if args.depth_dir else None
    uwb_path = Path(args.uwb_file) if args.uwb_file else None

    images = list_images(rgb_dir)
    k = make_intrinsics(args.fx, args.fy, args.cx, args.cy)
    detector = create_detector(args.detector, args.nfeatures)
    uwb = load_uwb_distances(uwb_path)

    poses_w_c = [np.eye(4, dtype=np.float64)]
    sparse_points_world = []
    dense_points_world = []

    prev_img = cv2.imread(str(images[0]), cv2.IMREAD_GRAYSCALE)
    if prev_img is None:
        raise FileNotFoundError(f"Cannot read image: {images[0]}")
    prev_kp, prev_desc = detect_and_describe(detector, prev_img)

    # Optional dense conversion for first frame.
    first_depth = read_depth_map(depth_dir, images[0], args.depth_scale)
    if first_depth is not None:
        pts_c = depth_to_camera_points(first_depth, k, args.depth_stride, args.max_depth)
        dense_points_world.append(transform_points(poses_w_c[0], pts_c))

    for i in range(1, len(images)):
        curr_img = cv2.imread(str(images[i]), cv2.IMREAD_GRAYSCALE)
        if curr_img is None:
            print(f"[WARN] skip unreadable image: {images[i]}")
            poses_w_c.append(poses_w_c[-1].copy())
            continue

        curr_kp, curr_desc = detect_and_describe(detector, curr_img)
        matches = match_descriptors(prev_desc, curr_desc, args.detector, args.ratio)

        if len(matches) < args.min_matches:
            print(f"[WARN] frame {i}: not enough matches ({len(matches)})")
            poses_w_c.append(poses_w_c[-1].copy())
            prev_img, prev_kp, prev_desc = curr_img, curr_kp, curr_desc
            continue

        pts_prev, pts_curr = points_from_matches(prev_kp, curr_kp, matches)

        try:
            r, t, inlier_mask = estimate_relative_pose(pts_prev, pts_curr, k, args.ransac_thr)
        except RuntimeError as ex:
            print(f"[WARN] frame {i}: {ex}")
            poses_w_c.append(poses_w_c[-1].copy())
            prev_img, prev_kp, prev_desc = curr_img, curr_kp, curr_desc
            continue

        # recoverPose gives transform from previous-camera to current-camera.
        t_norm = float(np.linalg.norm(t))
        if uwb is not None and i - 1 < len(uwb) and t_norm > 1e-12:
            t = t * (uwb[i - 1] / t_norm)

        t_curr_prev = rt_to_transform(r, t)
        t_prev_curr = invert_transform(t_curr_prev)
        t_w_c_curr = poses_w_c[-1] @ t_prev_curr
        poses_w_c.append(t_w_c_curr)

        if args.triangulate:
            in_prev = pts_prev[inlier_mask]
            in_curr = pts_curr[inlier_mask]
            if len(in_prev) >= args.min_triangulation_inliers:
                pts_w = triangulate_points_world(in_prev, in_curr, k, poses_w_c[-2], poses_w_c[-1])
                finite = np.isfinite(pts_w).all(axis=1)
                sparse_points_world.append(pts_w[finite])
            else:
                print(
                    f"[WARN] frame {i}: triangulation skipped "
                    f"(inliers={len(in_prev)} < min={args.min_triangulation_inliers})"
                )

        depth = read_depth_map(depth_dir, images[i], args.depth_scale)
        if depth is not None:
            pts_c = depth_to_camera_points(depth, k, args.depth_stride, args.max_depth)
            dense_points_world.append(transform_points(poses_w_c[-1], pts_c))

        print(
            f"frame {i:04d} | kp={len(curr_kp):4d} | matches={len(matches):4d} | inliers={int(inlier_mask.sum()):4d}"
        )

        prev_img, prev_kp, prev_desc = curr_img, curr_kp, curr_desc

    pose_path = Path(args.save_pose)
    pose_path.parent.mkdir(parents=True, exist_ok=True)
    with pose_path.open("w", encoding="utf-8") as f:
        for i, t_w_c in enumerate(poses_w_c):
            r = t_w_c[:3, :3]
            t = t_w_c[:3, 3]
            row = [i, *r.reshape(-1).tolist(), *t.tolist()]
            f.write(" ".join(map(str, row)) + "\n")
    print(f"saved trajectory: {pose_path.resolve()}")

    if sparse_points_world:
        sparse = np.concatenate(sparse_points_world, axis=0)
        save_ply_xyz(Path(args.save_sparse), sparse)
        print(f"saved sparse cloud: {Path(args.save_sparse).resolve()} ({len(sparse)} pts)")
    else:
        print("no sparse points to save")

    if dense_points_world:
        dense = np.concatenate(dense_points_world, axis=0)
        save_ply_xyz(Path(args.save_dense), dense)
        print(f"saved dense cloud: {Path(args.save_dense).resolve()} ({len(dense)} pts)")
    else:
        print("no dense points to save")


if __name__ == "__main__":
    main()
