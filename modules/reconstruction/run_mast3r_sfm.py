#!/usr/bin/env python3
"""
MASt3R-SfM 배치 실행 스크립트
실행 위치: ~/Desktop/MAST3R_2/

사전 준비:
  source /home/sdh/miniforge3/etc/profile.d/conda.sh
  conda activate recon3d
  export PYTHONPATH=$PYTHONPATH:~/Desktop/MAST3R_2:~/Desktop/MAST3R_2/dust3r

실행 예시:
  python run_mast3r_sfm.py \
      --img_dir ~/Desktop/data/optimal_orbit34_plus_orbit34_h5p2_1/rgb \
      --output_dir ~/Desktop/data/experiment_B/mast3r_output \
      --weights checkpoints/MASt3R_ViTLarge_BaseDecoder_512_catmlpdpt_metric.pth
"""
import os
import sys
import copy
import argparse
import numpy as np
import torch
import tempfile

from mast3r.model import AsymmetricMASt3R
import mast3r.utils.path_to_dust3r  # noqa: dust3r를 PYTHONPATH에 추가

from dust3r.utils.image import load_images
from dust3r.image_pairs import make_pairs
from mast3r.cloud_opt.sparse_ga import sparse_global_alignment


def save_ply(pts3d_list, colors_list, masks_list, output_path):
    pts_all, col_all = [], []
    for pts, col, mask in zip(pts3d_list, colors_list, masks_list):
        pts_np = pts if isinstance(pts, np.ndarray) else pts.detach().cpu().numpy()
        col_np = col if isinstance(col, np.ndarray) else col.detach().cpu().numpy()
        mask_np = mask if isinstance(mask, np.ndarray) else mask.detach().cpu().numpy()
        mask_np = mask_np.astype(bool)
        col_uint8 = (col_np * 255).clip(0, 255).astype(np.uint8)
        pts_all.append(pts_np[mask_np])
        col_all.append(col_uint8[mask_np])

    all_pts = np.concatenate(pts_all, axis=0)
    all_col = np.concatenate(col_all, axis=0)

    with open(output_path, 'w') as f:
        f.write("ply\nformat ascii 1.0\n")
        f.write(f"element vertex {len(all_pts)}\n")
        f.write("property float x\nproperty float y\nproperty float z\n")
        f.write("property uchar red\nproperty uchar green\nproperty uchar blue\n")
        f.write("end_header\n")
        for p, c in zip(all_pts, all_col):
            f.write(f"{p[0]:.6f} {p[1]:.6f} {p[2]:.6f} {c[0]} {c[1]} {c[2]}\n")

    print(f"[저장] 포인트 클라우드: {output_path} ({len(all_pts):,}개 포인트)")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--img_dir', required=True, help='rgb 이미지 폴더 경로')
    parser.add_argument('--output_dir', required=True, help='결과 저장 폴더')
    parser.add_argument('--weights', required=True,
                        help='MASt3R 체크포인트 경로 (예: checkpoints/MASt3R_ViTLarge_BaseDecoder_512_catmlpdpt_metric.pth)')
    parser.add_argument('--image_size', type=int, default=512)
    parser.add_argument('--device', default='cuda')
    # scene graph: swin이 순차 이미지(orbit 등)에 적합
    parser.add_argument('--scene_graph', default='swin',
                        choices=['complete', 'swin', 'logwin', 'oneref'],
                        help='complete: 전체 쌍(소규모), swin: 슬라이딩 윈도(순차 이미지 권장)')
    parser.add_argument('--winsize', type=int, default=5, help='swin/logwin 윈도우 크기')
    # sparse_global_alignment 파라미터
    parser.add_argument('--lr1', type=float, default=0.07)
    parser.add_argument('--niter1', type=int, default=500, help='coarse 최적화 반복 수')
    parser.add_argument('--lr2', type=float, default=0.014)
    parser.add_argument('--niter2', type=int, default=200, help='fine 최적화 반복 수 (0이면 coarse만)')
    parser.add_argument('--matching_conf_thr', type=float, default=2.0)
    parser.add_argument('--min_conf_thr', type=float, default=1.5, help='포인트 클라우드 confidence 필터')
    parser.add_argument('--shared_intrinsics', action='store_true',
                        help='모든 이미지가 동일 카메라일 때 사용')
    parser.add_argument('--coarse_only', action='store_true', help='niter2=0 (빠른 실행)')
    args = parser.parse_args()

    if args.coarse_only:
        args.niter2 = 0

    os.makedirs(args.output_dir, exist_ok=True)
    cache_dir = os.path.join(args.output_dir, 'cache')
    os.makedirs(cache_dir, exist_ok=True)

    # 이미지 목록 (정렬)
    img_files = sorted([
        os.path.join(args.img_dir, f)
        for f in os.listdir(args.img_dir)
        if f.lower().endswith(('.png', '.jpg', '.jpeg'))
    ])
    print(f"[로드] 이미지 {len(img_files)}장: {img_files[0]} ~ {img_files[-1]}")

    # 모델 로드
    print(f"[모델] {args.weights}")
    model = AsymmetricMASt3R.from_pretrained(args.weights).to(args.device)
    model.eval()

    # 이미지 로드
    imgs = load_images(img_files, size=args.image_size, verbose=True)

    # 이미지 쌍 생성
    scene_graph_str = f"{args.scene_graph}-{args.winsize}" if args.scene_graph in ('swin', 'logwin') else args.scene_graph
    pairs = make_pairs(imgs, scene_graph=scene_graph_str, prefilter=None, symmetrize=True)
    print(f"[페어링] {len(pairs)}쌍 (scene_graph={scene_graph_str})")

    # MASt3R-SfM: Sparse Global Alignment
    print("[SfM] sparse_global_alignment 시작...")
    scene = sparse_global_alignment(
        img_files, pairs, cache_dir,
        model,
        lr1=args.lr1, niter1=args.niter1,
        lr2=args.lr2, niter2=args.niter2,
        device=args.device,
        opt_depth=True,
        shared_intrinsics=args.shared_intrinsics,
        matching_conf_thr=args.matching_conf_thr,
    )

    # 결과 추출
    print("[추출] 결과 추출 중...")
    rgbimgs = scene.imgs                   # list of (H, W, 3) numpy float [0,1]
    focals = scene.get_focals().cpu()      # (N,)
    poses = scene.get_im_poses().cpu()     # (N, 4, 4)

    # dense pts3d + confidence (demo.py 방식)
    pts3d_list, _, confs_list = scene.get_dense_pts3d(clean_depth=True)
    masks = [c.cpu().numpy() > args.min_conf_thr for c in confs_list]

    # 포인트 클라우드 저장
    ply_path = os.path.join(args.output_dir, 'pointcloud.ply')
    save_ply(
        [p.cpu().numpy() for p in pts3d_list],
        rgbimgs,
        masks,
        ply_path
    )

    # 카메라 포즈 / focal 저장
    poses_np = poses.numpy()
    focals_np = focals.numpy()
    np.save(os.path.join(args.output_dir, 'poses.npy'), poses_np)
    np.save(os.path.join(args.output_dir, 'focals.npy'), focals_np)

    # 이미지별 포즈 요약 출력
    print(f"\n[결과] 카메라 {len(poses_np)}개")
    print(f"  poses.npy  : {poses_np.shape}")
    print(f"  focals.npy : {focals_np.shape}")
    print(f"  pointcloud : {ply_path}")
    print("\n완료!")


if __name__ == '__main__':
    main()
