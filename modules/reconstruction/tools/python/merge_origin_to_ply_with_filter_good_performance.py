import os
import json
import glob
import numpy as np

DEPTH_DIR = "/mnt/d/epic/CitySample/airsim_dataset/grid_merge/depth_pro"
META_DIR  = "/mnt/d/epic/CitySample/airsim_dataset/grid_merge/meta"
OUT_DIR   = "/mnt/d/epic/CitySample/airsim_dataset/grid_merge"
MERGED_OUT_PATH = "/mnt/d/epic/CitySample/airsim_dataset/grid_merge/merged_all.ply"

FOV = 90.0
MIN_DEPTH = 0.0
MAX_DEPTH = 14.0
STRIDE = 1


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

            X = d
            Y = (u - cx) * d / fx
            Z = -(v - cy) * d / fy


            pts.append([X, Y, Z])

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


if not os.path.exists(DEPTH_DIR):
    raise FileNotFoundError(f"depth dir not found: {DEPTH_DIR}")

if not os.path.exists(META_DIR):
    raise FileNotFoundError(f"meta dir not found: {META_DIR}")

os.makedirs(OUT_DIR, exist_ok=True)

depth_paths = sorted(glob.glob(os.path.join(DEPTH_DIR, "*.npy")))
print("total npy files:", len(depth_paths))

all_world_points = []
GROUP_SIZE = 10
group_t0 = {}

for idx, depth_path in enumerate(depth_paths):
    name = os.path.splitext(os.path.basename(depth_path))[0]
    meta_path = os.path.join(META_DIR, name + ".json")
    out_path = os.path.join(OUT_DIR, name + ".ply")

    if not os.path.exists(meta_path):
        print(f"skip {name}: meta not found")
        continue

    depth = np.load(depth_path)

    print(f"\n[{name}]")
    print("depth shape:", depth.shape, "dtype:", depth.dtype)
    print("depth min/max:", np.nanmin(depth), np.nanmax(depth))

    R, t, fov = read_pose(meta_path)
    print("fov:", fov)
    T = np.array([
    [ 0, -1,  0],
    [ 1,  0,  0],
    [ 0,  0, -1]
    ], dtype=np.float64)
    R = T @ R @ np.linalg.inv(T)

    group_start = (idx // GROUP_SIZE) * GROUP_SIZE

    if group_start not in group_t0:
        group_t0[group_start] = t.copy()

    #t_rel = t 사용시 3등분
    t_rel = t - group_t0[group_start]
    pts_cam = depth_to_points_cam(
        depth,
        fov_deg=fov,
        min_depth=MIN_DEPTH,
        max_depth=MAX_DEPTH,
        stride=STRIDE
    )
    print("cam points:", len(pts_cam))

    if len(pts_cam) == 0:
        print(f"skip {name}: no valid points after filtering")
        continue

    #pts_world = transform_points(pts_cam, R, t)
    pts_world = (R.T @ (pts_cam.T)).T + t_rel
    print("world points:", len(pts_world))

    save_ply_binary(out_path, pts_world)
    print("saved:", out_path)

    all_world_points.append(pts_world)

if len(all_world_points) == 0:
    raise RuntimeError("No valid points collected from all npy files.")

merged_points = np.vstack(all_world_points)
print("\nmerged shape:", merged_points.shape)

save_ply_binary(MERGED_OUT_PATH, merged_points)
print("saved merged:", MERGED_OUT_PATH)