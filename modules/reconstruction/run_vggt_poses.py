"""
VGGT로 카메라 포즈 추출
출력: poses_vggt.npy (N, 4, 4) camera-to-world 행렬
"""
import os
import sys
import glob
import numpy as np
import torch
import argparse
from plyfile import PlyData, PlyElement

sys.path.insert(0, os.path.expanduser("~/Desktop/vggt"))

from vggt.models.vggt import VGGT
from vggt.utils.load_fn import load_and_preprocess_images
from vggt.utils.pose_enc import pose_encoding_to_extri_intri


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--image_dir", required=True)
    parser.add_argument("--output_dir", required=True)
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    image_names = sorted(glob.glob(os.path.join(args.image_dir, "*.png")) +
                         glob.glob(os.path.join(args.image_dir, "*.jpg")))
    print(f"이미지 수: {len(image_names)}")

    device = "cuda" if torch.cuda.is_available() else "cpu"
    dtype = torch.bfloat16 if torch.cuda.is_available() else torch.float32

    print("VGGT 모델 로딩...")
    model = VGGT()
    ckpt_path = os.path.expanduser("~/.cache/torch/hub/checkpoints/model.pt")
    model.load_state_dict(torch.load(ckpt_path, map_location="cpu"))
    model = model.to(device).eval()

    print("이미지 전처리 중...")
    images = load_and_preprocess_images(image_names).to(device)

    print("추론 중...")
    with torch.no_grad(), torch.cuda.amp.autocast(dtype=dtype):
        predictions = model(images[None])

    extrinsics, intrinsics = pose_encoding_to_extri_intri(
        predictions["pose_enc"],
        image_size_hw=(images.shape[-2], images.shape[-1])
    )

    # extrinsics: (1, N, 3, 4) world-to-camera → camera-to-world로 변환
    extrinsics = extrinsics[0].cpu().numpy()  # (N, 3, 4)
    N = extrinsics.shape[0]

    poses_c2w = np.zeros((N, 4, 4))
    for i in range(N):
        R = extrinsics[i, :3, :3]
        t = extrinsics[i, :3, 3]
        # world-to-camera → camera-to-world
        R_c2w = R.T
        t_c2w = -R.T @ t
        poses_c2w[i, :3, :3] = R_c2w
        poses_c2w[i, :3, 3] = t_c2w
        poses_c2w[i, 3, 3] = 1.0

    out_path = os.path.join(args.output_dir, "poses_vggt.npy")
    np.save(out_path, poses_c2w)
    print(f"저장 완료: {out_path}  shape={poses_c2w.shape}")

    # 이미지 순서도 저장
    with open(os.path.join(args.output_dir, "image_list.txt"), "w") as f:
        for p in image_names:
            f.write(os.path.basename(p) + "\n")

    # 포인트 클라우드 저장
    if "world_points" in predictions and "world_points_conf" in predictions:
        print("포인트 클라우드 저장 중...")
        world_points = predictions["world_points"][0].cpu().numpy()  # (N, H, W, 3)
        conf = predictions["world_points_conf"][0].cpu().numpy()     # (N, H, W)
        imgs_np = images.cpu().numpy()                               # (N, 3, H, W)

        conf_threshold = np.percentile(conf, 50)  # 하위 50% 필터링
        mask = conf > conf_threshold

        pts = world_points[mask]       # (M, 3)
        imgs_t = imgs_np.transpose(0, 2, 3, 1)  # (N, H, W, 3)
        colors = (imgs_t[mask] * 255).clip(0, 255).astype(np.uint8)

        vertex = np.array(
            [(pts[i, 0], pts[i, 1], pts[i, 2], colors[i, 0], colors[i, 1], colors[i, 2])
             for i in range(len(pts))],
            dtype=[("x", "f4"), ("y", "f4"), ("z", "f4"),
                   ("red", "u1"), ("green", "u1"), ("blue", "u1")]
        )
        ply_path = os.path.join(args.output_dir, "pointcloud_vggt.ply")
        PlyData([PlyElement.describe(vertex, "vertex")]).write(ply_path)
        print(f"포인트 클라우드 저장 완료: {ply_path}  ({len(pts):,}점)")


if __name__ == "__main__":
    main()
