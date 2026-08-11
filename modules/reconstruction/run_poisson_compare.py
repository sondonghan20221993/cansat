import open3d as o3d
import numpy as np
import os
import glob

# 전처리된 4개 PLY를 각각 Screened Poisson 메싱하여 강도별 품질 비교
base = os.path.expanduser("~/Desktop/data/experiments")
inputs = sorted(glob.glob(f"{base}/blue_1__mast3r_fhd_preprocessed_*.ply"))
out_dir = os.path.expanduser("~/Desktop/data/experiments/blue_1__poisson_compare/")
os.makedirs(out_dir, exist_ok=True)

print(f"{'#'*60}")
print(f"# Screened Poisson 강도별 비교 ({len(inputs)}개 입력)")
print(f"{'#'*60}")

for input_ply in inputs:
    name = os.path.basename(input_ply).replace("blue_1__mast3r_fhd_preprocessed_", "").replace(".ply", "")
    print(f"\n{'='*60}")
    print(f"[*] 입력: {name}")
    print(f"{'='*60}")

    pcd = o3d.io.read_point_cloud(input_ply)
    print(f"  -> {len(pcd.points):,}점 로드 (법선 보유: {pcd.has_normals()})")

    # 전처리 단계에서 법선이 이미 추정됨 → 없을 때만 재추정 (안전장치)
    if not pcd.has_normals():
        print("  -> 법선 없음, 재추정")
        pcd.estimate_normals(search_param=o3d.geometry.KDTreeSearchParamHybrid(radius=0.1, max_nn=30))
        pcd.orient_normals_consistent_tangent_plane(k=15)

    # Screened Poisson (기존 run_poisson_fhd.py와 동일 파라미터)
    print("  -> Screened Poisson 메시 생성 (depth=10, scale=1.1)...")
    mesh, densities = o3d.geometry.TriangleMesh.create_from_point_cloud_poisson(pcd, depth=10, scale=1.1)

    # 밀도 하위 10% 제거
    thresh = np.percentile(np.asarray(densities), 10)
    mesh.remove_vertices_by_mask(np.asarray(densities) < thresh)
    mesh.remove_degenerate_triangles()
    mesh.remove_unreferenced_vertices()

    out_path = os.path.join(out_dir, f"mesh_poisson_{name}.ply")
    o3d.io.write_triangle_mesh(out_path, mesh)
    print(f"  [✓] 저장: {out_path}")
    print(f"      삼각형: {len(mesh.triangles):,}, 정점: {len(mesh.vertices):,}")

print(f"\n{'='*60}")
print(f"[✓] 전체 완료 → {out_dir}")
print(f"{'='*60}\n")
