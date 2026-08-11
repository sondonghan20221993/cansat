import os
import json
import glob
import numpy as np

DEPTH_DIR = "/mnt/d/epic/CitySample/airsim_dataset/test/depth_000000_npy"
META_DIR  = "/mnt/d/epic/CitySample/airsim_dataset/merged/meta"
OUT_PATH  = "/mnt/d/epic/CitySample/airsim_dataset/merged/merged_30frames.ply"

FOV = 90.0
MAX_DEPTH = 2000.0
STRIDE = 2   # 1이면 전부 사용, 2면 2픽셀 간격 샘플링


def quat_to_rotmat(qx, qy, qz, qw):
    n = np.sqrt(qx*qx + qy*qy + qz*qz + qw*qw)
    if n == 0:
        return np.eye(3, dtype=np.float64)
    qx, qy, qz, qw = qx/n, qy/n, qz/n, qw/n

    R = np.array([
        [1 - 2*(qy*qy + qz*qz),     2*(qx*qy - qz*qw),     2*(qx*qz + qy*qw)],
        [    2*(qx*qy + qz*qw), 1 - 2*(qx*qx + qz*qz),     2*(qy*qz - qx*qw)],
        [    2*(qx*qz - qy*qw),     2*(qy*qz + qx*qw), 1 - 2*(qx*qx + qy*qy)]
    ], dtype=np.float64)
    return R


def read_pose(meta_path):
    with open(meta_path, "r", encoding="utf-8") as f:
        meta = json.load(f)

    vp = meta["vehicle_pose"]
    pos = vp["position"]
    ori = vp["orientation"]

    tx = float(pos["x"])
    ty = float(pos["y"])
    tz = float(pos["z"])

    qw = float(ori["w"])
    qx = float(ori["x"])
    qy = float(ori["y"])
    qz = float(ori["z"])

    R = quat_to_rotmat(qx, qy, qz, qw)
    t = np.array([tx, ty, tz], dtype=np.float64)
    return R, t


def depth_to_points_cam(depth, fov_deg=90.0, max_depth=2000.0, stride=1):
    H, W = depth.shape
    fx = W / (2.0 * np.tan(np.deg2rad(fov_deg / 2.0)))
    fy = fx
    cx = W / 2.0
    cy = H / 2.0

    pts = []

    for v in range(0, H, stride):
        for u in range(0, W, stride):
            d = float(depth[v, u])

            if not np.isfinite(d):
                continue
            if d <= 0 or d > max_depth:
                continue

            Z = d
            X = (u - cx) * Z / fx
            Y = (v - cy) * Z / fy

            X_air = Z
            Y_air = X
            Z_air = -Y

            pts.append([X_air, Y_air, Z_air])

    if len(pts) == 0:
        return np.zeros((0, 3), dtype=np.float64)

    return np.asarray(pts, dtype=np.float64)


def transform_points(pts_cam, R, t):
    # pts_world = R @ pts_cam + t
    return (R @ pts_cam.T).T + t


def save_ply_binary(path, pts):
    pts = np.asarray(pts, dtype=np.float32)

    header = "\n".join([
        "ply",
        "format binary_little_endian 1.0",
        f"element vertex {len(pts)}",
        "property float x",
        "property float y",
        "property float z",
        "end_header\n"
    ])

    with open(path, "wb") as f:
        f.write(header.encode("ascii"))
        pts.tofile(f)


depth_files = sorted(glob.glob(os.path.join(DEPTH_DIR, "*.npy")))[:30]

all_points = []

for depth_path in depth_files:
    name = os.path.splitext(os.path.basename(depth_path))[0]
    meta_path = os.path.join(META_DIR, name + ".json")

    if not os.path.exists(meta_path):
        print(f"skip: meta not found for {name}")
        continue

    depth = np.load(depth_path)
    pts_cam = depth_to_points_cam(depth, fov_deg=FOV, max_depth=MAX_DEPTH, stride=STRIDE)

    R, t = read_pose(meta_path)
    pts_world = transform_points(pts_cam, R, t)

    all_points.append(pts_world)
    print(f"{name}: cam_points={len(pts_cam)}, world_points={len(pts_world)}")

if len(all_points) == 0:
    raise RuntimeError("No valid points collected.")

merged_points = np.vstack(all_points)
print("merged shape:", merged_points.shape)

save_ply_binary(OUT_PATH, merged_points)
print("saved:", OUT_PATH)