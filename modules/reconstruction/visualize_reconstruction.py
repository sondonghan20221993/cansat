#!/usr/bin/env python3
"""
PLY 포인트클라우드 + 카메라 포즈 시각화 스크립트
run_reconstruction.py 출력물(pointcloud.ply, poses.npy, focals.npy)을 trimesh로 시각화.

사용법:
    python visualize_reconstruction.py --output_dir ./output
    python visualize_reconstruction.py --ply a.ply --poses poses.npy --focals focals.npy
"""
import argparse
import numpy as np
import trimesh

SPHERE_R  = 0.06
ARROW_LEN = 0.25


def make_gradient_color(i, total):
    """파랑(시작) → 빨강(끝) 그라디언트"""
    t = i / max(total - 1, 1)
    return np.array([255 * t, 80, 255 * (1 - t)], dtype=np.uint8)


def add_arrow(scene, origin, direction, length, color, radius=0.02):
    """origin → direction 방향으로 화살표(cylinder + cone) 추가"""
    body_len = length * 0.75
    head_len = length * 0.25

    z = np.array([0, 0, 1.0])
    d = direction / np.linalg.norm(direction)
    v = np.cross(z, d)
    s = np.linalg.norm(v)
    c_val = np.dot(z, d)
    R = np.eye(3)
    if s > 1e-6:
        Vx = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
        R = np.eye(3) + Vx + Vx @ Vx * ((1 - c_val) / s ** 2)

    cyl = trimesh.creation.cylinder(radius=radius, height=body_len)
    T = np.eye(4)
    T[:3, :3] = R
    T[:3, 3] = origin + d * body_len / 2
    cyl.apply_transform(T)
    cyl.visual.face_colors[:, :3] = color
    scene.add_geometry(cyl)

    cone = trimesh.creation.cone(radius * 2.5, head_len)
    T2 = np.eye(4)
    T2[:3, :3] = R
    T2[:3, 3] = origin + d * length
    cone.apply_transform(T2)
    cone.visual.face_colors[:, :3] = color
    scene.add_geometry(cone)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output_dir', default=None, help='run_reconstruction.py 출력 폴더')
    parser.add_argument('--ply',    default=None)
    parser.add_argument('--poses',  default=None)
    parser.add_argument('--focals', default=None)
    parser.add_argument('--point_size', type=int, default=2)
    args = parser.parse_args()

    if args.output_dir:
        import os
        ply_path    = os.path.join(args.output_dir, 'pointcloud.ply')
        poses_path  = os.path.join(args.output_dir, 'poses.npy')
        focals_path = os.path.join(args.output_dir, 'focals.npy')
    else:
        ply_path    = args.ply
        poses_path  = args.poses
        focals_path = args.focals

    print("포인트클라우드 로딩 중...")
    pcd = trimesh.load(ply_path)
    print(f"  {len(pcd.vertices):,} 포인트 로드 완료")

    poses  = np.load(poses_path)
    focals = np.load(focals_path)
    N = len(poses)
    print(f"  카메라 {N}개 로드 완료 (focal={focals[0]:.1f}px)")

    scene = trimesh.Scene()
    scene.add_geometry(pcd)

    print(f"카메라 화살표 {N}개 추가 중...")
    for i, pose in enumerate(poses):
        color = make_gradient_color(i, N)
        pos     = pose[:3, 3]
        forward = pose[:3, 2]
        add_arrow(scene, pos, forward, ARROW_LEN, color, radius=SPHERE_R * 0.3)

    # 카메라 이동 궤적
    positions = poses[:, :3, 3]
    for i in range(N - 1):
        seg = trimesh.load_path(np.array([[positions[i], positions[i + 1]]]))
        scene.add_geometry(seg)

    print("시각화 시작 (창을 닫으면 종료)")
    scene.show(line_settings={'point_size': args.point_size})


if __name__ == '__main__':
    main()
