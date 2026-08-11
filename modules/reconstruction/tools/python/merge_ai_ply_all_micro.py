import os
import glob
import numpy as np
import trimesh

PLY_DIR  = "/mnt/d/epic/CitySample/airsim_dataset/test_merge/"
OUT_PATH = "/mnt/d/epic/CitySample/airsim_dataset/merge/depth_pro_no_icp.ply"

NUM_FRAMES = 30

CAMERA_TO_AIRSIM = np.array([
    [0, 0, 1, 0],
    [1, 0, 0, 0],
    [0, 1, 0, 0],
    [0, 0, 0, 1],
], dtype=np.float64)

def load_ply_points(path):
    mesh = trimesh.load(path)
    if hasattr(mesh, "vertices"):
        pts = np.asarray(mesh.vertices, dtype=np.float64)
    else:
        raise RuntimeError(f"PLY 로드 실패: {path}")
    return pts

def world_from_frame(ply_path):
    pts = load_ply_points(ply_path)
    pts_h = np.hstack([pts, np.ones((len(pts), 1))])
    pts = (CAMERA_TO_AIRSIM @ pts_h.T).T[:, :3]
    return pts

ply_files = sorted(glob.glob(os.path.join(PLY_DIR, "*.ply")))

if len(ply_files) == 0:
    raise RuntimeError("PLY 파일이 없습니다.")

usable_n = min(NUM_FRAMES, len(ply_files))
if usable_n < 2:
    raise RuntimeError("최소 2개 프레임이 필요합니다.")

ply_files = ply_files[:usable_n]

print("사용 프레임 수:", usable_n)

merged_world = world_from_frame(ply_files[0])
print("frame 0 loaded:", merged_world.shape)

trimesh.points.PointCloud(merged_world).export(
    "/mnt/d/epic/CitySample/airsim_dataset/merge/merged_until_0.ply"
)

for i in range(1, usable_n):
    print("\n==============================")
    print(f"merge frame {i}")

    source_world = world_from_frame(ply_files[i])
    print("source_world:", source_world.shape)
    print("merged_world:", merged_world.shape)

    before_merge = np.vstack([merged_world, source_world])
    trimesh.points.PointCloud(before_merge).export(
        f"/mnt/d/epic/CitySample/airsim_dataset/merge/before_merge_{i}.ply"
    )
    print(f"saved before_merge_{i}.ply")

    merged_world = np.vstack([merged_world, source_world])
    print("merged_world updated:", merged_world.shape)

    trimesh.points.PointCloud(merged_world).export(
        f"/mnt/d/epic/CitySample/airsim_dataset/merge/merged_until_{i}.ply"
    )
    print(f"saved merged_until_{i}.ply")

trimesh.points.PointCloud(merged_world).export(OUT_PATH)
print(f"\n저장 완료: {OUT_PATH}")