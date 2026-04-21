from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

import numpy as np


def main() -> int:
    parser = argparse.ArgumentParser(description="Run DUSt3R and export a point cloud as ASCII PLY.")
    parser.add_argument("--dust3r-repo", required=True, help="Path to the official DUSt3R repository")
    parser.add_argument("--output", required=True, help="Output PLY path")
    parser.add_argument("--model-name", default="naver/DUSt3R_ViTLarge_BaseDecoder_512_dpt")
    parser.add_argument("--image-size", type=int, default=512)
    parser.add_argument("--niter", type=int, default=300)
    parser.add_argument("--lr", type=float, default=0.01)
    parser.add_argument("--schedule", default="cosine")
    parser.add_argument("--device", default="cuda")
    parser.add_argument("images", nargs="+", help="Input image paths")
    args = parser.parse_args()

    repo = os.path.abspath(args.dust3r_repo)
    if repo not in sys.path:
        sys.path.insert(0, repo)

    from dust3r.cloud_opt import GlobalAlignerMode, global_aligner
    from dust3r.image_pairs import make_pairs
    from dust3r.inference import inference
    from dust3r.model import AsymmetricCroCo3DStereo
    from dust3r.utils.image import load_images

    print("Loading DUSt3R model...", flush=True)
    model = AsymmetricCroCo3DStereo.from_pretrained(args.model_name).to(args.device)

    print(f"Loading {len(args.images)} images...", flush=True)
    images = load_images(args.images, size=args.image_size)

    print("Making image pairs...", flush=True)
    pairs = make_pairs(images, scene_graph="complete", prefilter=None, symmetrize=True)

    print("Running DUSt3R inference...", flush=True)
    output = inference(pairs, model, args.device, batch_size=1)

    print("Running global alignment...", flush=True)
    scene = global_aligner(output, device=args.device, mode=GlobalAlignerMode.PointCloudOptimizer)
    loss = scene.compute_global_alignment(init="mst", niter=args.niter, schedule=args.schedule, lr=args.lr)

    pts3d = scene.get_pts3d()
    masks = scene.get_masks()

    all_points = []
    for idx in range(len(pts3d)):
        pts = pts3d[idx].detach().cpu().numpy()
        mask = masks[idx].detach().cpu().numpy().astype(bool)
        valid_pts = pts[mask]
        if len(valid_pts) > 0:
            all_points.append(valid_pts.reshape(-1, 3))

    if not all_points:
        raise RuntimeError("DUSt3R produced no valid points.")

    points = np.concatenate(all_points, axis=0)
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with output_path.open("w", encoding="utf-8") as fp:
        fp.write("ply\n")
        fp.write("format ascii 1.0\n")
        fp.write(f"element vertex {len(points)}\n")
        fp.write("property float x\n")
        fp.write("property float y\n")
        fp.write("property float z\n")
        fp.write("end_header\n")
        for point in points:
            fp.write(f"{point[0]} {point[1]} {point[2]}\n")

    print(f"saved: {output_path}", flush=True)
    print(f"loss: {float(loss)}", flush=True)
    print(f"num_points: {len(points)}", flush=True)
    print(f"num_images: {len(args.images)}", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
