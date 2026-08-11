import os
import json
import glob
import math
import itertools
import numpy as np
import open3d as o3d

DEPTH_DIR = r"D:\\epic\\CitySample\\airsim_dataset\\merge\\depth_pro"
META_DIR  = r"D:\\epic\\CitySample\\airsim_dataset\\merge\\meta"

GROUP_SIZE = 10
FOV = 90.0
MIN_DEPTH = 0.0
MAX_DEPTH = 15.0
STRIDE = 4           # 전수검사용이라 조금 키움
VOXEL = 0.30         # 다운샘플 크기
ICP_THRESHOLD = 2.0
MAX_ITER = 50


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


def get_base_xyz(depth, fov_deg, min_depth, max_depth, stride):
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

            pts.append([X, Y, Z])

    if len(pts) == 0:
        return np.zeros((0, 3), dtype=np.float64)

    return np.asarray(pts, dtype=np.float64)


def make_axis_mappings():
    # 6 permutations * 8 sign combinations = 48
    perms = list(itertools.permutations([0, 1, 2]))
    signs = list(itertools.product([-1, 1], repeat=3))

    mappings = []
    for perm in perms:
        for sign in signs:
            mappings.append((perm, sign))
    return mappings


def apply_mapping(pts_xyz, perm, sign):
    out = pts_xyz[:, perm].copy()
    out[:, 0] *= sign[0]
    out[:, 1] *= sign[1]
    out[:, 2] *= sign[2]
    return out


def transform_points(pts_cam, R, t):
    return (R @ pts_cam.T).T + t


def np_to_pcd(pts):
    pcd = o3d.geometry.PointCloud()
    if len(pts) > 0:
        pcd.points = o3d.utility.Vector3dVector(pts)
    return pcd


def preprocess_pcd(pcd, voxel):
    if len(pcd.points) == 0:
        return pcd
    down = pcd.voxel_down_sample(voxel)
    if len(down.points) == 0:
        return down
    down.estimate_normals(
        o3d.geometry.KDTreeSearchParamHybrid(radius=voxel * 2.0, max_nn=30)
    )
    return down


def icp_score(source, target, threshold, max_iter):
    if len(source.points) == 0 or len(target.points) == 0:
        return None

    reg = o3d.pipelines.registration.registration_icp(
        source,
        target,
        threshold,
        np.eye(4),
        o3d.pipelines.registration.TransformationEstimationPointToPlane(),
        o3d.pipelines.registration.ICPConvergenceCriteria(max_iteration=max_iter),
    )
    return reg


def build_group_clouds_for_mapping(depth_paths, perm, sign):
    group_points = {0: [], 10: [], 20: []}

    for idx, depth_path in enumerate(depth_paths):
        name = os.path.splitext(os.path.basename(depth_path))[0]
        meta_path = os.path.join(META_DIR, name + ".json")
        if not os.path.exists(meta_path):
            continue

        depth = np.load(depth_path)
        R, t, fov = read_pose(meta_path)

        base_pts = get_base_xyz(
            depth,
            fov_deg=fov,
            min_depth=MIN_DEPTH,
            max_depth=MAX_DEPTH,
            stride=STRIDE
        )
        if len(base_pts) == 0:
            continue

        mapped_pts = apply_mapping(base_pts, perm, sign)
        pts_world = transform_points(mapped_pts, R, t)

        group_start = (idx // GROUP_SIZE) * GROUP_SIZE
        if group_start in group_points:
            group_points[group_start].append(pts_world)

    out = {}
    for k in [0, 10, 20]:
        if len(group_points[k]) == 0:
            out[k] = np_to_pcd(np.zeros((0, 3), dtype=np.float64))
        else:
            merged = np.vstack(group_points[k])
            out[k] = np_to_pcd(merged)
    return out


def score_mapping(depth_paths, perm, sign):
    groups = build_group_clouds_for_mapping(depth_paths, perm, sign)

    g0 = preprocess_pcd(groups[0], VOXEL)
    g1 = preprocess_pcd(groups[10], VOXEL)
    g2 = preprocess_pcd(groups[20], VOXEL)

    reg1 = icp_score(g1, g0, ICP_THRESHOLD, MAX_ITER)
    reg2 = icp_score(g2, g0, ICP_THRESHOLD, MAX_ITER)

    if reg1 is None or reg2 is None:
        return None

    # fitness 높을수록 좋고 rmse 낮을수록 좋음
    total_score = (reg1.fitness + reg2.fitness) - 0.2 * (reg1.inlier_rmse + reg2.inlier_rmse)

    return {
        "perm": perm,
        "sign": sign,
        "score": total_score,
        "fit1": reg1.fitness,
        "rmse1": reg1.inlier_rmse,
        "fit2": reg2.fitness,
        "rmse2": reg2.inlier_rmse,
    }


def main():
    depth_paths = sorted(glob.glob(os.path.join(DEPTH_DIR, "*.npy")))
    if len(depth_paths) == 0:
        raise RuntimeError("No depth files found.")

    mappings = make_axis_mappings()
    results = []

    print("total candidates:", len(mappings))

    for i, (perm, sign) in enumerate(mappings, 1):
        print(f"\n[{i}/48] testing perm={perm}, sign={sign}")
        try:
            r = score_mapping(depth_paths, perm, sign)
            if r is not None:
                results.append(r)
                print(
                    f"score={r['score']:.6f}, "
                    f"fit1={r['fit1']:.6f}, rmse1={r['rmse1']:.6f}, "
                    f"fit2={r['fit2']:.6f}, rmse2={r['rmse2']:.6f}"
                )
            else:
                print("invalid result")
        except Exception as e:
            print("failed:", e)

    if len(results) == 0:
        raise RuntimeError("No valid mapping result.")

    results.sort(key=lambda x: x["score"], reverse=True)

    print("\n===== TOP 10 =====")
    for rank, r in enumerate(results[:10], 1):
        print(
            f"{rank:02d}. perm={r['perm']}, sign={r['sign']}, "
            f"score={r['score']:.6f}, "
            f"fit1={r['fit1']:.6f}, rmse1={r['rmse1']:.6f}, "
            f"fit2={r['fit2']:.6f}, rmse2={r['rmse2']:.6f}"
        )

    best = results[0]
    print("\n===== BEST =====")
    print("perm :", best["perm"])
    print("sign :", best["sign"])
    print("score:", best["score"])


if __name__ == "__main__":
    main()