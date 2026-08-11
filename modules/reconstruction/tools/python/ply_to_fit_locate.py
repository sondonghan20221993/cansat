import os
import json
import numpy as np
import trimesh
from scipy.spatial.transform import Rotation

PLY_DIR  = "/mnt/d/epic/CitySample/airsim_dataset/ai_depth"
#PLY_DIR  = "/mnt/d/epic/CitySample/airsim_dataset/ai_depth_ply"
META_DIR = "/mnt/d/epic/CitySample/airsim_dataset/merged/meta"
OUT_DIR  = "/mnt/d/epic/CitySample/airsim_dataset/ai_depth_test"


os.makedirs(OUT_DIR, exist_ok=True)

def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)

def pose_to_matrix(meta):
    pos = meta["vehicle_pose"]["position"]
    ori = meta["vehicle_pose"]["orientation"]

    t = np.array([pos["x"], pos["y"], pos["z"]], dtype=np.float64)
    q = np.array([ori["x"], ori["y"], ori["z"], ori["w"]], dtype=np.float64)

    R = Rotation.from_quat(q).as_matrix()

    T = np.eye(4, dtype=np.float64)
    T[:3, :3] = R
    T[:3, 3] = t
    return T

for i in range(30):
    #if i != 3: continue
    ply_path  = os.path.join(PLY_DIR, f"{i}.ply")
    meta_path = os.path.join(META_DIR, f"{i:06d}.json")
    out_path  = os.path.join(OUT_DIR, f"{i}.ply")

    if not os.path.exists(ply_path):
        print(f"ply 없음, 건너뜀: {ply_path}")
        continue

    if not os.path.exists(meta_path):
        print(f"meta 없음, 건너뜀: {meta_path}")
        continue

    print("loading ply :", ply_path)
    print("loading meta:", meta_path)

    obj = trimesh.load(ply_path)

    if isinstance(obj, trimesh.points.PointCloud):
        points = np.asarray(obj.vertices, dtype=np.float64)

    elif isinstance(obj, trimesh.Trimesh):
        points = np.asarray(obj.vertices, dtype=np.float64)

    elif isinstance(obj, trimesh.Scene):
        pts = []
        for g in obj.geometry.values():
            if isinstance(g, trimesh.points.PointCloud):
                pts.append(np.asarray(g.vertices, dtype=np.float64))
            elif isinstance(g, trimesh.Trimesh):
                pts.append(np.asarray(g.vertices, dtype=np.float64))

        if len(pts) == 0:
            print(f"점 없음, 건너뜀: {ply_path}")
            continue

        points = np.vstack(pts)

    else:
        print(f"지원 안 되는 타입, 건너뜀: {type(obj)} / {ply_path}")
        continue

    if points.shape[0] == 0:
        print(f"점 없음, 건너뜀: {ply_path}")
        continue

    meta = load_json(meta_path)
    T = pose_to_matrix(meta)
    T_inv = np.linalg.inv(T)

    ones = np.ones((points.shape[0], 1), dtype=np.float64)
    points_h = np.hstack([points, ones])
    aligned_h = (T @ points_h.T).T
    aligned_points = aligned_h[:, :3]

    out_pc = trimesh.points.PointCloud(aligned_points)
    out_pc.export(out_path)

    print("saved:", out_path, "points:", aligned_points.shape[0])

print("done")