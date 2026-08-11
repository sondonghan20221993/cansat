"""
4개 강도 점군을 각각 MILo prior (COLMAP dense) 형식으로 변환
"""
import numpy as np
import open3d as o3d
from plyfile import PlyData, PlyElement
import os
import shutil
import glob

base_dir = os.path.expanduser("~/Desktop/data/experiments")
colmap_base = os.path.join(base_dir, "blue_1__mast3r_fhd/02_colmap")  # 원본 COLMAP
preproc_dir = base_dir  # 전처리 점군 있는 곳

# 4개 강도 PLY 찾기
ply_files = sorted(glob.glob(os.path.join(preproc_dir, "blue_1__mast3r_fhd_preprocessed_*.ply")))

print(f"{'#'*70}")
print(f"# MILo Prior Batch 생성 (4개 강도)")
print(f"{'#'*70}")
print(f"입력 COLMAP: {colmap_base}")
print(f"입력 점군 {len(ply_files)}개 감지\n")

for ply_path in ply_files:
    name = os.path.basename(ply_path).replace("blue_1__mast3r_fhd_preprocessed_", "").replace(".ply", "")
    out_dir = os.path.join(base_dir, f"blue_1__milo_fhd_{name}")
    dense_dir = os.path.join(out_dir, "02_colmap_dense")

    print(f"{'='*70}")
    print(f"[*] 강도: {name}")
    print(f"{'='*70}")

    # 1. 기존 COLMAP 복사 (images, cameras.txt 등 보존)
    if os.path.exists(out_dir):
        shutil.rmtree(out_dir)
    os.makedirs(dense_dir, exist_ok=True)

    # colmap 폴더 전체 복사
    for item in os.listdir(colmap_base):
        src = os.path.join(colmap_base, item)
        dst = os.path.join(dense_dir, item)
        if os.path.isdir(src):
            shutil.copytree(src, dst)
        else:
            shutil.copy2(src, dst)

    # 2. 점군 로드
    print(f"  [1/4] 점군 로드: {os.path.basename(ply_path)}")
    pcd = o3d.io.read_point_cloud(ply_path)
    print(f"        {len(pcd.points):,}점 로드됨")

    # 3. 법선 확인 (전처리에서 이미 추정됨)
    if not pcd.has_normals():
        print(f"  [2/4] 법선 추정 (없음)")
        pcd.estimate_normals(search_param=o3d.geometry.KDTreeSearchParamHybrid(radius=0.1, max_nn=30))
        pcd.orient_normals_consistent_tangent_plane(k=15)
    else:
        print(f"  [2/4] 법선 확인 (이미 있음)")

    # 4. COLMAP points3D.ply 형식으로 변환
    print(f"  [3/4] COLMAP points3D.ply 형식 변환")
    xyz = np.asarray(pcd.points).astype(np.float32)
    rgb = (np.asarray(pcd.colors) * 255).astype(np.uint8) if pcd.has_colors() else np.full((len(xyz), 3), 128, dtype=np.uint8)
    nxyz = np.asarray(pcd.normals).astype(np.float32) if pcd.has_normals() else np.zeros((len(xyz), 3), dtype=np.float32)

    dtype = [('x','f4'),('y','f4'),('z','f4'),
             ('nx','f4'),('ny','f4'),('nz','f4'),
             ('red','u1'),('green','u1'),('blue','u1')]
    verts = np.empty(len(xyz), dtype=dtype)
    verts['x'], verts['y'], verts['z'] = xyz[:,0], xyz[:,1], xyz[:,2]
    verts['nx'], verts['ny'], verts['nz'] = nxyz[:,0], nxyz[:,1], nxyz[:,2]
    verts['red'], verts['green'], verts['blue'] = rgb[:,0], rgb[:,1], rgb[:,2]

    # 5. 저장
    out_ply = os.path.join(dense_dir, "sparse/0/points3D.ply")
    os.makedirs(os.path.dirname(out_ply), exist_ok=True)
    PlyData([PlyElement.describe(verts, 'vertex')]).write(out_ply)
    print(f"  [4/4] 저장: {out_ply}")
    print(f"        {len(xyz):,}점, {len(verts)}정점\n")
    print(f"  → MILo 실행 대상: {out_dir}\n")

print(f"{'='*70}")
print(f"[✓] 모든 prior 생성 완료")
print(f"{'='*70}\n")
