import os
import json
import numpy as np

DEPTH_PATH = "/mnt/d/epic/CitySample/airsim_dataset/test/depth_000000.npy"
META_PATH  = "/mnt/d/epic/CitySample/airsim_dataset/merge/meta/000000.json"
OUT_PATH   = "/mnt/d/epic/CitySample/airsim_dataset/test_merge/000000.ply"

FOV = 90.0
MIN_DEPTH = 0.0
MAX_DEPTH = 20.0
STRIDE = 1 #점 


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

    pos = meta["camera_position"]
    ori = meta["camera_orientation"]

    tx = float(pos["x"])
    ty = float(pos["y"])
    tz = float(pos["z"])

    qw = float(ori["w"])
    qx = float(ori["x"])
    qy = float(ori["y"])
    qz = float(ori["z"])

    R = quat_to_rotmat(qx, qy, qz, qw)
    t = np.array([tx, ty, tz], dtype=np.float64)

    fov = float(meta.get("fov", FOV))
    return R, t, fov


def depth_to_points_cam(depth, fov_deg=90.0, min_depth=0.0, max_depth=2000.0, stride=1):
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
            if d < min_depth or d > max_depth:
                continue

            Z = d
            X = (u - cx) * Z / fx
            Y = (v - cy) * Z / fy

            # camera -> AirSim/world local axis
            X_air = Z
            Y_air = X
            Z_air = -Y

            pts.append([X_air, Y_air, Z_air])

    if len(pts) == 0:
        return np.zeros((0, 3), dtype=np.float64)

    return np.asarray(pts, dtype=np.float64)


def transform_points(pts_cam, R, t):
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


if not os.path.exists(DEPTH_PATH):
    raise FileNotFoundError(f"depth not found: {DEPTH_PATH}")

if not os.path.exists(META_PATH):
    raise FileNotFoundError(f"meta not found: {META_PATH}")

depth = np.load(DEPTH_PATH)
print("depth shape:", depth.shape, "dtype:", depth.dtype)
print("depth min/max:", np.nanmin(depth), np.nanmax(depth))

R, t, fov = read_pose(META_PATH)
print("fov:", fov)

pts_cam = depth_to_points_cam(
    depth,
    fov_deg=fov,
    min_depth=MIN_DEPTH,
    max_depth=MAX_DEPTH,
    stride=STRIDE
)
print("cam points:", len(pts_cam))

if len(pts_cam) == 0:
    raise RuntimeError("No valid points in this npy after filtering.")

pts_world = transform_points(pts_cam, R, t)
print("world points:", len(pts_world))

os.makedirs(os.path.dirname(OUT_PATH), exist_ok=True)
save_ply_binary(OUT_PATH, pts_world)
print("saved:", OUT_PATH)