import os
import copy
import numpy as np
import open3d as o3d

PLY_DIR = r"D:\\epic\\CitySample\\airsim_dataset\\test_merge\\"

SRC0 = os.path.join(PLY_DIR, "merged_000_009.ply")
SRC1 = os.path.join(PLY_DIR, "merged_010_019.ply")
SRC2 = os.path.join(PLY_DIR, "merged_020_029.ply")

OUT_MERGED = os.path.join(PLY_DIR, "merged_icp_all.ply")
OUT_1_ALIGNED = os.path.join(PLY_DIR, "merged_010_019_icp.ply")
OUT_2_ALIGNED = os.path.join(PLY_DIR, "merged_020_029_icp.ply")

VOXEL_SIZE = 0.10
ICP_THRESHOLD = 1.5
MAX_ITER = 100


def load_pcd(path):
    if not os.path.exists(path):
        raise FileNotFoundError(path)
    pcd = o3d.io.read_point_cloud(path)
    if len(pcd.points) == 0:
        raise RuntimeError(f"empty point cloud: {path}")
    return pcd


def preprocess(pcd, voxel_size):
    pcd_down = pcd.voxel_down_sample(voxel_size)
    pcd_down.estimate_normals(
        o3d.geometry.KDTreeSearchParamHybrid(radius=voxel_size * 2.0, max_nn=30)
    )
    return pcd_down


def run_icp(source, target, threshold, max_iter):
    reg = o3d.pipelines.registration.registration_icp(
        source,
        target,
        threshold,
        np.eye(4),
        o3d.pipelines.registration.TransformationEstimationPointToPlane(),
        o3d.pipelines.registration.ICPConvergenceCriteria(max_iteration=max_iter),
    )
    return reg


def transform_copy(pcd, T):
    out = copy.deepcopy(pcd)
    out.transform(T)
    return out


def merge_and_save(pcd_list, out_path):
    merged = o3d.geometry.PointCloud()
    for p in pcd_list:
        merged += p
    o3d.io.write_point_cloud(out_path, merged)
    return merged


def print_result(name, reg):
    print(f"\n[{name}]")
    print("fitness  :", reg.fitness)
    print("rmse     :", reg.inlier_rmse)
    print("transform:")
    print(reg.transformation)


def main():
    base = load_pcd(SRC0)
    sweep1 = load_pcd(SRC1)
    sweep2 = load_pcd(SRC2)

    print("base  points:", len(base.points))
    print("sweep1 points:", len(sweep1.points))
    print("sweep2 points:", len(sweep2.points))

    base_down = preprocess(base, VOXEL_SIZE)
    sweep1_down = preprocess(sweep1, VOXEL_SIZE)
    sweep2_down = preprocess(sweep2, VOXEL_SIZE)

    reg1 = run_icp(sweep1_down, base_down, ICP_THRESHOLD, MAX_ITER)
    print_result("ICP sweep1 -> base", reg1)

    sweep1_aligned = transform_copy(sweep1, reg1.transformation)
    o3d.io.write_point_cloud(OUT_1_ALIGNED, sweep1_aligned)
    print("saved:", OUT_1_ALIGNED)

    merged01 = o3d.geometry.PointCloud()
    merged01 += base
    merged01 += sweep1_aligned

    merged01_down = preprocess(merged01, VOXEL_SIZE)

    reg2 = run_icp(sweep2_down, merged01_down, ICP_THRESHOLD, MAX_ITER)
    print_result("ICP sweep2 -> merged01", reg2)

    sweep2_aligned = transform_copy(sweep2, reg2.transformation)
    o3d.io.write_point_cloud(OUT_2_ALIGNED, sweep2_aligned)
    print("saved:", OUT_2_ALIGNED)

    merged_all = merge_and_save([base, sweep1_aligned, sweep2_aligned], OUT_MERGED)
    print("\nfinal merged points:", len(merged_all.points))
    print("saved:", OUT_MERGED)

    o3d.visualization.draw_geometries([merged_all])


if __name__ == "__main__":
    main()