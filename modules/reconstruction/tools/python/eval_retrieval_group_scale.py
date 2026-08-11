#!/usr/bin/env python3
"""
retrieval SfM 결과의 3m/7m 그룹별 Umeyama 스케일 비교
(swin-5 결과의 "inter-orbit scale mismatch" 재현 여부 확인용)
"""
import os
import json
import numpy as np


def umeyama(src, dst):
    # src, dst: (N,3) 대응점. dst = s * R @ src + t 를 만족하는 s,R,t 추정
    assert src.shape == dst.shape
    n, dim = src.shape
    mu_src = src.mean(axis=0)
    mu_dst = dst.mean(axis=0)
    src_c = src - mu_src
    dst_c = dst - mu_dst
    sigma_src = (src_c ** 2).sum() / n
    cov = (dst_c.T @ src_c) / n
    U, D, Vt = np.linalg.svd(cov)
    S = np.eye(dim)
    if np.linalg.det(U) * np.linalg.det(Vt) < 0:
        S[-1, -1] = -1
    R = U @ S @ Vt
    scale = np.trace(np.diag(D) @ S) / sigma_src
    t = mu_dst - scale * R @ mu_src
    return scale, R, t


def load_gt_positions(meta_dir, frame_ids):
    pos = []
    for fid in frame_ids:
        with open(os.path.join(meta_dir, f"{fid}.json")) as f:
            d = json.load(f)
        p = d["camera"]["pose"]["position"]
        pos.append([p["x"], p["y"], p["z"]])
    return np.array(pos)


def main():
    base = os.path.expanduser("~/Desktop/data")
    import sys
    variant = sys.argv[1] if len(sys.argv) > 1 else "real_test_combined_uniform__mast3r_retrieval"
    poses_path = os.path.join(base, f"experiments/{variant}/poses.npy")
    rgb_dir = os.path.join(base, "datasets/real_test_combined_uniform/rgb")

    names = sorted([f for f in os.listdir(rgb_dir) if f.lower().endswith(".png")])
    poses = np.load(poses_path)  # (34,4,4), c2w
    assert len(names) == poses.shape[0], (len(names), poses.shape)

    est_pos = poses[:, :3, 3]  # 카메라 위치 (c2w translation)

    idx_3m = [i for i, n in enumerate(names) if n.startswith("3m")]
    idx_7m = [i for i, n in enumerate(names) if n.startswith("7m")]
    print(f"3m: {len(idx_3m)}장, 7m: {len(idx_7m)}장")

    frames_3m = [names[i].replace("3m_", "").replace(".png", "") for i in idx_3m]
    frames_7m = [names[i].replace("7m_", "").replace(".png", "") for i in idx_7m]

    gt_3m = load_gt_positions(os.path.join(base, "datasets/real_test_3m_uniform/meta"), frames_3m)
    gt_7m = load_gt_positions(os.path.join(base, "datasets/real_test_7m_uniform/meta"), frames_7m)

    est_3m = est_pos[idx_3m]
    est_7m = est_pos[idx_7m]

    s3, R3, t3 = umeyama(est_3m, gt_3m)
    s7, R7, t7 = umeyama(est_7m, gt_7m)

    def residuals(est, gt, s, R, t):
        pred = (s * (R @ est.T).T + t)
        return np.sqrt(((pred - gt) ** 2).sum(axis=1))

    res3 = residuals(est_3m, gt_3m, s3, R3, t3)
    res7 = residuals(est_7m, gt_7m, s7, R7, t7)

    print(f"\n[per-group Umeyama, {variant}]")
    print(f"  3m  scale={s3:.4f}  RMSE={np.sqrt((res3**2).mean())*100:.2f}cm  mean={res3.mean()*100:.2f}cm  median={np.median(res3)*100:.2f}cm  max={res3.max()*100:.2f}cm (frame {frames_3m[res3.argmax()]})")
    print(f"  7m  scale={s7:.4f}  RMSE={np.sqrt((res7**2).mean())*100:.2f}cm  mean={res7.mean()*100:.2f}cm  median={np.median(res7)*100:.2f}cm  max={res7.max()*100:.2f}cm (frame {frames_7m[res7.argmax()]})")
    mismatch = abs(s3 - s7) / min(s3, s7) * 100
    print(f"  scale mismatch = {mismatch:.2f}%  (swin-5 기존 결과: ~11.6%, 7m ATE=27.6cm)")
    print(f"\n  7m 프레임별 잔차(cm): " + ", ".join(f"{f}={r*100:.1f}" for f, r in zip(frames_7m, res7)))


if __name__ == "__main__":
    main()
