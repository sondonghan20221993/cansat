import argparse
import json
import math
import webbrowser
from pathlib import Path

import numpy as np
import open3d as o3d
import plotly.graph_objects as go


def load_point_cloud(path: Path) -> np.ndarray:
    pcd = o3d.io.read_point_cloud(str(path))
    if pcd.is_empty():
        raise ValueError(f"Point cloud is empty or unreadable: {path}")
    return np.asarray(pcd.points, dtype=np.float64)


def load_pose_json(path: Path):
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        raise ValueError("Pose JSON must contain a top-level list.")
    return data


def pose_center(item: dict) -> np.ndarray:
    mat = np.array(item["T_wc"], dtype=np.float64)
    if mat.shape != (4, 4):
        raise ValueError("Each pose must contain a 4x4 T_wc matrix.")
    return mat[:3, 3].copy()


def normalize(vec: np.ndarray) -> np.ndarray:
    norm = np.linalg.norm(vec)
    if norm <= 1e-12:
        return np.zeros_like(vec)
    return vec / norm


def camera_view_direction(center: np.ndarray, target: np.ndarray) -> np.ndarray:
    return normalize(target - center)


def build_candidates(target_center: np.ndarray, existing_centers: np.ndarray, radius_scale: float, count: int):
    bbox_min = existing_centers.min(axis=0)
    bbox_max = existing_centers.max(axis=0)
    span = bbox_max - bbox_min
    base_radius = max(float(np.linalg.norm(span[:2]) * 0.5), 0.5)
    radius = base_radius * radius_scale
    z_level = float(existing_centers[:, 2].mean())

    candidates = []
    for idx in range(count):
        angle = 2.0 * math.pi * idx / count
        pos = np.array(
            [
                target_center[0] + radius * math.cos(angle),
                target_center[1] + radius * math.sin(angle),
                z_level,
            ],
            dtype=np.float64,
        )
        view_dir = camera_view_direction(pos, target_center)
        candidates.append(
            {
                "index": idx,
                "angle_rad": angle,
                "position": pos,
                "view_dir": view_dir,
            }
        )
    return candidates


def score_candidates(candidates, existing_view_dirs: np.ndarray, existing_centers: np.ndarray):
    if len(existing_view_dirs) == 0:
        for cand in candidates:
            cand["score"] = 1.0
            cand["max_similarity"] = 0.0
            cand["distance_penalty"] = 0.0
        return candidates

    mean_existing_radius = np.mean(np.linalg.norm(existing_centers[:, :2] - existing_centers[:, :2].mean(axis=0), axis=1))
    mean_existing_radius = max(float(mean_existing_radius), 1e-6)

    for cand in candidates:
        sims = existing_view_dirs @ cand["view_dir"]
        max_similarity = float(np.max(sims))
        novelty = 1.0 - max_similarity

        cand_radius = float(np.linalg.norm(cand["position"][:2] - existing_centers[:, :2].mean(axis=0)))
        distance_penalty = abs(cand_radius - mean_existing_radius) / mean_existing_radius

        score = novelty - 0.15 * distance_penalty
        cand["score"] = float(score)
        cand["max_similarity"] = max_similarity
        cand["distance_penalty"] = float(distance_penalty)
    return candidates


def sample_points(points: np.ndarray, max_points: int) -> np.ndarray:
    if len(points) <= max_points:
        return points
    idx = np.linspace(0, len(points) - 1, num=max_points, dtype=np.int64)
    return points[idx]


def build_figure(
    points: np.ndarray,
    existing_centers: np.ndarray,
    target_center: np.ndarray,
    candidates,
    best_candidate,
    point_size: float,
):
    pts = sample_points(points, 100000)
    fig = go.Figure()

    fig.add_trace(
        go.Scatter3d(
            x=pts[:, 0],
            y=pts[:, 1],
            z=pts[:, 2],
            mode="markers",
            name="Point Cloud",
            marker=dict(size=point_size, color="#d1d5db", opacity=0.35),
        )
    )

    fig.add_trace(
        go.Scatter3d(
            x=existing_centers[:, 0],
            y=existing_centers[:, 1],
            z=existing_centers[:, 2],
            mode="lines+markers",
            name="Existing Cameras",
            line=dict(color="#3b82f6", width=5),
            marker=dict(size=4, color="#60a5fa"),
        )
    )

    candidate_positions = np.stack([c["position"] for c in candidates], axis=0)
    candidate_scores = np.array([c["score"] for c in candidates], dtype=np.float64)
    hover_text = [
        f"candidate={c['index']}<br>score={c['score']:.4f}<br>novelty={1.0-c['max_similarity']:.4f}<br>similarity={c['max_similarity']:.4f}"
        for c in candidates
    ]
    fig.add_trace(
        go.Scatter3d(
            x=candidate_positions[:, 0],
            y=candidate_positions[:, 1],
            z=candidate_positions[:, 2],
            mode="markers",
            name="Candidate Cameras",
            text=hover_text,
            hovertemplate="%{text}<extra></extra>",
            marker=dict(
                size=6,
                color=candidate_scores,
                colorscale="Viridis",
                colorbar=dict(title="Score"),
                opacity=0.9,
            ),
        )
    )

    fig.add_trace(
        go.Scatter3d(
            x=[best_candidate["position"][0]],
            y=[best_candidate["position"][1]],
            z=[best_candidate["position"][2]],
            mode="markers",
            name="Recommended Next View",
            marker=dict(size=10, color="#ef4444", symbol="diamond"),
        )
    )

    fig.add_trace(
        go.Scatter3d(
            x=[target_center[0]],
            y=[target_center[1]],
            z=[target_center[2]],
            mode="markers",
            name="Point Cloud Center",
            marker=dict(size=8, color="#22c55e"),
        )
    )

    fig.add_trace(
        go.Scatter3d(
            x=[best_candidate["position"][0], target_center[0]],
            y=[best_candidate["position"][1], target_center[1]],
            z=[best_candidate["position"][2], target_center[2]],
            mode="lines",
            name="Recommended View Direction",
            line=dict(color="#f59e0b", width=6),
        )
    )

    fig.update_layout(
        title="Next Viewpoint Recommendation Prototype",
        template="plotly_dark",
        scene=dict(
            xaxis_title="X",
            yaxis_title="Y",
            zaxis_title="Z",
            aspectmode="data",
        ),
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="left", x=0.0),
        margin=dict(l=0, r=0, t=60, b=0),
        height=900,
    )
    return fig


def main():
    parser = argparse.ArgumentParser(description="Prototype next-viewpoint recommender using MASt3R-SLAM poses and PLY.")
    parser.add_argument(
        "--base-dir",
        default=r"C:\Users\sdh97\Desktop\airsim_degree_notilt_more_distance",
        help="Directory containing rgb.ply and rgb_poses.json",
    )
    parser.add_argument("--ply", default="rgb.ply", help="Point cloud filename")
    parser.add_argument("--poses", default="rgb_poses.json", help="Pose JSON filename")
    parser.add_argument("--candidate-count", type=int, default=16, help="Number of circular candidate viewpoints")
    parser.add_argument("--radius-scale", type=float, default=1.15, help="Scale factor for candidate ring radius")
    parser.add_argument("--point-size", type=float, default=0.4, help="Displayed point size for the point cloud")
    parser.add_argument("--output-html", default=None, help="Output HTML path")
    parser.add_argument("--open", action="store_true", help="Open the generated HTML automatically")
    args = parser.parse_args()

    base_dir = Path(args.base_dir)
    ply_path = base_dir / args.ply
    pose_path = base_dir / args.poses
    if not ply_path.is_file():
        raise FileNotFoundError(f"Missing point cloud file: {ply_path}")
    if not pose_path.is_file():
        raise FileNotFoundError(f"Missing pose file: {pose_path}")

    points = load_point_cloud(ply_path)
    poses = load_pose_json(pose_path)
    existing_centers = np.stack([pose_center(item) for item in poses], axis=0)
    target_center = points.mean(axis=0)
    existing_view_dirs = np.stack([camera_view_direction(center, target_center) for center in existing_centers], axis=0)

    candidates = build_candidates(target_center, existing_centers, args.radius_scale, args.candidate_count)
    candidates = score_candidates(candidates, existing_view_dirs, existing_centers)
    best_candidate = max(candidates, key=lambda c: c["score"])

    output_html = Path(args.output_html) if args.output_html else base_dir / "next_viewpoint_prototype.html"
    fig = build_figure(points, existing_centers, target_center, candidates, best_candidate, args.point_size)
    fig.write_html(str(output_html), include_plotlyjs="cdn")

    summary = {
        "base_dir": str(base_dir),
        "point_cloud_center": target_center.tolist(),
        "existing_camera_count": int(len(existing_centers)),
        "candidate_count": int(len(candidates)),
        "recommended_candidate_index": int(best_candidate["index"]),
        "recommended_position": best_candidate["position"].tolist(),
        "recommended_view_direction": best_candidate["view_dir"].tolist(),
        "recommended_score": float(best_candidate["score"]),
    }
    print(json.dumps(summary, indent=2))
    print(f"\nSaved prototype HTML: {output_html}")

    if args.open:
        webbrowser.open(output_html.resolve().as_uri())


if __name__ == "__main__":
    main()
