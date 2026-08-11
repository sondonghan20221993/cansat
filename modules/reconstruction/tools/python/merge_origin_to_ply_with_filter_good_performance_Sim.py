import os
import json
import glob
import numpy as np
from scipy.spatial import cKDTree

DEPTH_DIR = "/mnt/d/epic/CitySample/airsim_dataset/grid_merge/depth_pro"
META_DIR  = "/mnt/d/epic/CitySample/airsim_dataset/grid_merge/meta"
OUT_DIR   = "/mnt/d/epic/CitySample/airsim_dataset/grid_merge_sim3"

GROUP_SIZE = 10

FOV = 90.0
MIN_DEPTH = 0.0
MAX_DEPTH = 10.0
STRIDE = 2

VOXEL_SIZE = 0.08
ICP_ITERS = 20
MAX_CORR_DIST = 1.5
MIN_PAIRS = 200

np.set_printoptions(suppress=True, precision=6)


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


def depth_to_points_cam(depth, fov_deg=90.0, min_depth=0.0, max_depth=10.0, stride=1):
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

            # 중요: pose 검증 결과에 맞춘 X-forward
            X = d
            Y = (u - cx) * d / fx
            Z = -(v - cy) * d / fy

            pts.append([X, Y, Z])

    if len(pts) == 0:
        return np.zeros((0, 3), dtype=np.float64)

    return np.asarray(pts, dtype=np.float64)


def transform_points_rigid(pts, R, t):
    return (R @ pts.T).T + t


def voxel_downsample(points, voxel_size):
    if len(points) == 0:
        return points

    idx = np.floor(points / voxel_size).astype(np.int64)
    uniq, inv = np.unique(idx, axis=0, return_inverse=True)

    out = np.zeros((len(uniq), 3), dtype=np.float64)
    cnt = np.zeros(len(uniq), dtype=np.int64)

    for i, g in enumerate(inv):
        out[g] += points[i]
        cnt[g] += 1

    out /= cnt[:, None]
    return out


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


def save_matrix_txt(path, s, R, t):
    T = np.eye(4, dtype=np.float64)
    T[:3, :3] = s * R
    T[:3, 3] = t

    with open(path, "w", encoding="utf-8") as f:
        f.write(f"scale = {s:.12f}\n")
        f.write("R =\n")
        f.write(np.array2string(R, precision=12, suppress_small=False))
        f.write("\n")
        f.write("t =\n")
        f.write(np.array2string(t, precision=12, suppress_small=False))
        f.write("\n")
        f.write("Sim3_4x4 =\n")
        f.write(np.array2string(T, precision=12, suppress_small=False))
        f.write("\n")


def estimate_sim3_umeyama(src, dst):
    # src -> dst
    assert src.shape == dst.shape
    n = src.shape[0]
    if n < 3:
        raise RuntimeError("not enough correspondences")

    mu_src = src.mean(axis=0)
    mu_dst = dst.mean(axis=0)

    X = src - mu_src
    Y = dst - mu_dst

    cov = (Y.T @ X) / n
    U, D, Vt = np.linalg.svd(cov)

    S = np.eye(3)
    if np.linalg.det(U @ Vt) < 0:
        S[2, 2] = -1.0

    R = U @ S @ Vt

    var_src = np.mean(np.sum(X * X, axis=1))
    if var_src <= 1e-12:
        raise RuntimeError("source variance too small")

    scale = np.trace(np.diag(D) @ S) / var_src
    t = mu_dst - scale * (R @ mu_src)

    return scale, R, t


def apply_sim3(points, s, R, t):
    return (s * (R @ points.T)).T + t


def compose_sim3(s2, R2, t2, s1, R1, t1):
    # first T1, then T2
    # p' = s2 R2 (s1 R1 p + t1) + t2
    s = s2 * s1
    R = R2 @ R1
    t = s2 * (R2 @ t1) + t2
    return s, R, t


def sim3_icp(source_pts, target_pts, iters=20, max_corr_dist=1.5, min_pairs=200):
    src_work = source_pts.copy()

    s_total = 1.0
    R_total = np.eye(3, dtype=np.float64)
    t_total = np.zeros(3, dtype=np.float64)

    tree = cKDTree(target_pts)

    for it in range(iters):
        dists, idx = tree.query(src_work, k=1)
        mask = np.isfinite(dists) & (dists < max_corr_dist)

        src_corr = src_work[mask]
        dst_corr = target_pts[idx[mask]]

        if len(src_corr) < min_pairs:
            print(f"  ICP iter {it:02d}: stop, pairs={len(src_corr)} < {min_pairs}")
            break

        s_inc, R_inc, t_inc = estimate_sim3_umeyama(src_corr, dst_corr)

        src_work = apply_sim3(src_work, s_inc, R_inc, t_inc)
        s_total, R_total, t_total = compose_sim3(
            s_inc, R_inc, t_inc,
            s_total, R_total, t_total
        )

        rmse = np.sqrt(np.mean(np.sum((apply_sim3(src_corr, s_inc, R_inc, t_inc) - dst_corr) ** 2, axis=1)))
        print(f"  ICP iter {it:02d}: pairs={len(src_corr)}, rmse={rmse:.6f}, s_inc={s_inc:.6f}")

    return src_work, s_total, R_total, t_total


def build_group_clouds():
    depth_paths = sorted(glob.glob(os.path.join(DEPTH_DIR, "*.npy")))
    if len(depth_paths) == 0:
        raise RuntimeError("no npy files found")

    groups = {}

    for idx, depth_path in enumerate(depth_paths):
        name = os.path.splitext(os.path.basename(depth_path))[0]
        meta_path = os.path.join(META_DIR, name + ".json")
        if not os.path.exists(meta_path):
            print(f"skip {name}: meta not found")
            continue

        depth = np.load(depth_path)
        R, t, fov = read_pose(meta_path)

        pts_cam = depth_to_points_cam(
            depth,
            fov_deg=fov,
            min_depth=MIN_DEPTH,
            max_depth=MAX_DEPTH,
            stride=STRIDE
        )

        if len(pts_cam) == 0:
            print(f"skip {name}: no valid points")
            continue

        pts_world = transform_points_rigid(pts_cam, R, t)

        g = idx // GROUP_SIZE
        groups.setdefault(g, []).append(pts_world)

        print(f"[{name}] group={g}, cam_pts={len(pts_cam)}, world_pts={len(pts_world)}")

    out = []
    for g in sorted(groups.keys()):
        pts = np.vstack(groups[g])
        pts_ds = voxel_downsample(pts, VOXEL_SIZE)
        out.append((g, pts, pts_ds))
        print(f"group {g}: raw={len(pts)}, down={len(pts_ds)}")

    return out


def main():
    if not os.path.exists(DEPTH_DIR):
        raise FileNotFoundError(DEPTH_DIR)
    if not os.path.exists(META_DIR):
        raise FileNotFoundError(META_DIR)

    os.makedirs(OUT_DIR, exist_ok=True)

    groups = build_group_clouds()
    if len(groups) == 0:
        raise RuntimeError("no valid group clouds")

    # 저장: 그룹 원본/다운샘플
    for g, pts_raw, pts_ds in groups:
        save_ply_binary(os.path.join(OUT_DIR, f"group_{g:02d}_raw.ply"), pts_raw)
        save_ply_binary(os.path.join(OUT_DIR, f"group_{g:02d}_down.ply"), pts_ds)

    # 기준 그룹 = 0
    g0, raw0, base0 = groups[0]
    merged_full = [raw0]
    merged_ref = base0.copy()

    # 각 그룹의 누적 Sim3
    global_s = 1.0
    global_R = np.eye(3, dtype=np.float64)
    global_t = np.zeros(3, dtype=np.float64)
    save_matrix_txt(os.path.join(OUT_DIR, f"group_{g0:02d}_sim3.txt"), global_s, global_R, global_t)

    for i in range(1, len(groups)):
        g, raw_pts, ds_pts = groups[i]
        print(f"\n=== align group {g} -> merged ===")
        aligned_ds, s, R, t = sim3_icp(
            source_pts=ds_pts,
            target_pts=merged_ref,
            iters=ICP_ITERS,
            max_corr_dist=MAX_CORR_DIST,
            min_pairs=MIN_PAIRS
        )

        aligned_raw = apply_sim3(raw_pts, s, R, t)

        save_ply_binary(os.path.join(OUT_DIR, f"group_{g:02d}_aligned_raw.ply"), aligned_raw)
        save_ply_binary(os.path.join(OUT_DIR, f"group_{g:02d}_aligned_down.ply"), aligned_ds)
        save_matrix_txt(os.path.join(OUT_DIR, f"group_{g:02d}_sim3.txt"), s, R, t)

        merged_full.append(aligned_raw)
        merged_ref = voxel_downsample(np.vstack([merged_ref, aligned_ds]), VOXEL_SIZE)

        print(f"group {g}: s={s:.6f}")
        print("R=\n", R)
        print("t=", t)

    merged_full = np.vstack(merged_full)
    merged_full_ds = voxel_downsample(merged_full, VOXEL_SIZE)

    save_ply_binary(os.path.join(OUT_DIR, "merged_sim3_all_raw.ply"), merged_full)
    save_ply_binary(os.path.join(OUT_DIR, "merged_sim3_all_down.ply"), merged_full_ds)

    print("\nDONE")
    print("saved:", os.path.join(OUT_DIR, "merged_sim3_all_raw.ply"))
    print("saved:", os.path.join(OUT_DIR, "merged_sim3_all_down.ply"))


if __name__ == "__main__":
    main()