import os
import glob
import json
import numpy as np
import trimesh
from scipy.spatial.transform import Rotation

# =========================
# 경로 설정 (WSL 기준)
# =========================
GLB_DIR = r"/mnt/d/epic/CitySample/airsim_dataset/merge/blend"
META_DIR = r"/mnt/d/epic/CitySample/airsim_dataset/merge/meta"
OUT_PATH = r"/mnt/d/epic/CitySample/airsim_dataset/merge/merged_pose_blend.glb"

# 필요하면 로컬 스케일 보정
SCALE = 1.0

def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def pose_to_matrix(meta):
    pos = meta["camera_position"]
    ori = meta["camera_orientation"]

    t = np.array([pos["x"], pos["y"], pos["z"]], dtype=np.float64)

    # scipy quat 순서: x, y, z, w
    q = np.array([
        ori["x"],
        ori["y"],
        ori["z"],
        ori["w"],
    ], dtype=np.float64)

    R = Rotation.from_quat(q).as_matrix()

    T = np.eye(4, dtype=np.float64)
    T[:3, :3] = R
    T[:3, 3] = t
    return T


# =========================
# 좌표계 변환
# =========================
# problem_1.py 기준:
# camera 좌표계: X right, Y down, Z forward
# AirSim 좌표계: X forward, Y right, Z down
#
# 즉,
# Xa = Zc
# Ya = Xc
# Za = Yc
#
# 이를 4x4 행렬로 쓰면:
CAMERA_TO_AIRSIM = np.array([
    [0, 0, 1, 0],  # Xa = Zc
    [1, 0, 0, 0],  # Ya = Xc
    [0, 1, 0, 0],  # Za = Yc
    [0, 0, 0, 1],
], dtype=np.float64)


glb_files = sorted(glob.glob(os.path.join(GLB_DIR, "*.glb")))

if len(glb_files) == 0:
    raise FileNotFoundError(f"glb 파일 없음: {GLB_DIR}")

scene = trimesh.Scene()
for glb_path in glb_files:
    stem = os.path.splitext(os.path.basename(glb_path))[0]
    meta_path = os.path.join(META_DIR, f"{stem.zfill(6)}.json")

    if not os.path.exists(meta_path):
        print(f"meta 없음, 건너뜀: {meta_path}")
        continue

    meta = load_json(meta_path)
    T_world_from_camera = pose_to_matrix(meta)

    # scene 전체 로드
    obj = trimesh.load(glb_path, force="scene")

    # scene graph transform까지 포함해서 하나의 mesh로 결합
    dumped = obj.dump(concatenate=True)

    if isinstance(dumped, trimesh.Trimesh):
        g = dumped.copy()
    else:
        meshes = [m.copy() for m in dumped if isinstance(m, trimesh.Trimesh)]
        if len(meshes) == 0:
            print(f"mesh 없음, 건너뜀: {glb_path}")
            continue
        g = trimesh.util.concatenate(meshes)

    # 필요하면 로컬 스케일 조정
    if SCALE != 1.0:
        S = np.eye(4, dtype=np.float64)
        S[0, 0] = SCALE
        S[1, 1] = SCALE
        S[2, 2] = SCALE
        g.apply_transform(S)

    # 1) GLB/camera 기준 mesh -> AirSim vehicle 기준
    g.apply_transform(CAMERA_TO_AIRSIM)

    # 2) AirSim vehicle 기준 -> world 기준
    g.apply_transform(T_world_from_camera)

    scene.add_geometry(g)

    print(f"추가 완료: {os.path.basename(glb_path)}")

scene.export(OUT_PATH)
print("저장 완료:", OUT_PATH)