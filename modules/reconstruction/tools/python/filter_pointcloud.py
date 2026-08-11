#!/usr/bin/env python3
"""
MASt3R-SfM PLY 후처리: voxel downsampling + statistical outlier removal.
MASt3R 자체 confidence 필터링(min_conf_thr)은 이미 적용된 상태에서,
남은 과밀(중복 표면)/floater를 open3d로 추가 정리한다.

voxel 크기는 GT 없이 계산 가능해야 실전(드론 실측) 데이터에 쓸 수 있다.
MASt3R는 up-to-scale 복원이라 좌표 단위의 실제 물리적 크기가 씬마다 다르므로,
절대값(예: 0.01) 대신 점군 자체의 median nearest-neighbor distance 배수로 적응시킨다.
(Umeyama/GT scale은 결과를 실측 단위로 "해석"하는 검증 용도로만 쓰고, 파라미터 계산에는 쓰지 않는다.)

사용법:
  python filter_pointcloud.py <input.ply> <output.ply> --voxel_multiplier 2.8 [--sor_nb 20] [--sor_std 2.0]
  python filter_pointcloud.py <input.ply> <output.ply> --voxel 0.01 [--sor_nb 20] [--sor_std 2.0]   # 수동 절대값
"""
import argparse
import numpy as np
import open3d as o3d
from scipy.spatial import KDTree


def median_nn_distance(points, sample=20000, seed=0):
    rng = np.random.RandomState(seed)
    idx = rng.choice(len(points), min(sample, len(points)), replace=False)
    tree = KDTree(points)
    d, _ = tree.query(points[idx], k=2)  # k=1은 자기 자신
    return float(np.median(d[:, 1]))


parser = argparse.ArgumentParser()
parser.add_argument("input_ply")
parser.add_argument("output_ply")
parser.add_argument("--voxel", type=float, default=None, help="voxel downsample 크기 절대값 (MASt3R 좌표 단위, 씬마다 실제 크기 다름 주의)")
parser.add_argument("--voxel_multiplier", type=float, default=None, help="GT-free: median NN distance의 배수로 voxel 크기 결정 (권장)")
parser.add_argument("--sor_nb", type=int, default=20, help="SOR: 이웃 점 개수")
parser.add_argument("--sor_std", type=float, default=2.0, help="SOR: 표준편차 배수 임계값")
args = parser.parse_args()

if args.voxel is None and args.voxel_multiplier is None:
    args.voxel_multiplier = 2.8  # 기본값: median NN distance의 2.8배

pcd = o3d.io.read_point_cloud(args.input_ply)
n_before = len(pcd.points)
pts = np.asarray(pcd.points)

if args.voxel_multiplier is not None:
    med_nn = median_nn_distance(pts)
    voxel_size = args.voxel_multiplier * med_nn
    print(f"[적응형 voxel] median_NN={med_nn:.6f} x multiplier={args.voxel_multiplier} = voxel={voxel_size:.6f}")
else:
    voxel_size = args.voxel
    print(f"[수동 voxel] voxel={voxel_size:.6f} (절대값, GT 없이는 실제 cm 환산 불가)")

pcd_down = pcd.voxel_down_sample(voxel_size=voxel_size)
n_after_voxel = len(pcd_down.points)

pcd_clean, inlier_idx = pcd_down.remove_statistical_outlier(nb_neighbors=args.sor_nb, std_ratio=args.sor_std)
n_after_sor = len(pcd_clean.points)

o3d.io.write_point_cloud(args.output_ply, pcd_clean)

print(f"[필터링] {args.input_ply} -> {args.output_ply}")
print(f"  원본:            {n_before:,}개")
print(f"  voxel downsample: {n_after_voxel:,}개 (voxel={voxel_size:.6f}) [{n_after_voxel/n_before*100:.1f}%]")
print(f"  SOR 제거 후:      {n_after_sor:,}개 (nb={args.sor_nb}, std={args.sor_std}) [{n_after_sor/n_after_voxel*100:.1f}%]")
print(f"  최종 비율:        {n_after_sor/n_before*100:.1f}%")
