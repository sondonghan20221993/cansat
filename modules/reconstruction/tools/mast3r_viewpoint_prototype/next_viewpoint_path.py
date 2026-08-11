import argparse
import json
from pathlib import Path

import numpy as np
import plotly.graph_objects as go

import importlib.util


BASE_MODULE_PATH = Path(r"C:\Users\sdh97\Desktop\mast3r_viewpoint_prototype\next_viewpoint_quick_b.py")
spec = importlib.util.spec_from_file_location("next_viewpoint_quick_b", BASE_MODULE_PATH)
nbv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(nbv)


def score_candidates(
    candidates: np.ndarray,
    target: np.ndarray,
    support_points: np.ndarray,
    camera_centers: np.ndarray,
    camera_forwards: np.ndarray,
    up_dir: np.ndarray,
    avg_radius: float,
    fov_deg: float,
    visibility_mode: str = "projection",
    occupancy: set[tuple[int, int, int]] | None = None,
    voxel_size: float = 0.12,
    coverage_weight: float = 0.65,
    novelty_weight: float = 0.35,
    distance_penalty_weight: float = 0.10,
) -> np.ndarray:
    scores = np.zeros(len(candidates), dtype=np.float64)
    existing_dirs = camera_forwards.copy()
    existing_dirs = existing_dirs / np.clip(np.linalg.norm(existing_dirs, axis=1, keepdims=True), 1e-8, None)
    for i, candidate in enumerate(candidates):
        view_dir = nbv.normalize(target - candidate)
        if float(np.dot(view_dir, up_dir)) > 0.0:
            scores[i] = -1e9
            continue
        if visibility_mode == "voxel_occlusion":
            if occupancy is None:
                raise ValueError("occupancy is required when visibility_mode='voxel_occlusion'")
            coverage = nbv.voxel_occlusion_coverage_score(
                candidate,
                target,
                support_points,
                occupancy,
                voxel_size=voxel_size,
                fov_deg=fov_deg,
                grid_size=24,
            )
        else:
            coverage = nbv.projection_coverage_score(candidate, target, support_points, fov_deg=fov_deg, grid_size=24)
        similarity = existing_dirs @ view_dir
        novelty = 1.0 - float(np.max(similarity))
        distance_penalty = abs(float(np.linalg.norm(candidate - target)) - avg_radius)
        distance_penalty = min(distance_penalty / 5.0, 1.0)
        scores[i] = (
            float(coverage_weight) * coverage
            + float(novelty_weight) * novelty
            - float(distance_penalty_weight) * distance_penalty
        )
    return scores


def greedy_path(
    start: np.ndarray,
    candidates: np.ndarray,
    candidate_scores: np.ndarray,
    target: np.ndarray,
    start_forward: np.ndarray | None = None,
    max_steps: int = 5,
    min_spacing: float = 0.35,
    entry_distance_weight: float = 0.0,
    entry_heading_weight: float = 0.0,
    entry_height_weight: float = 0.0,
) -> tuple[np.ndarray, np.ndarray]:
    valid_mask = candidate_scores > -1e8
    pool = candidates[valid_mask]
    pool_scores = candidate_scores[valid_mask]
    if len(pool) == 0:
        return np.empty((0, 3), dtype=np.float64), np.empty((0,), dtype=np.float64)

    chosen = []
    chosen_scores = []
    current = start.copy()
    remaining_points = pool.copy()
    remaining_scores = pool_scores.copy()

    for step_idx in range(min(max_steps, len(remaining_points))):
        dists = np.linalg.norm(remaining_points - current[None, :], axis=1)
        path_scores = remaining_scores / np.clip(dists, 0.25, None)
        if step_idx == 0:
            entry_distance_penalty = dists / np.clip(np.max(dists), 1e-8, None)
            if start_forward is not None:
                candidate_forwards = target[None, :] - remaining_points
                candidate_forwards = candidate_forwards / np.clip(
                    np.linalg.norm(candidate_forwards, axis=1, keepdims=True),
                    1e-8,
                    None,
                )
                start_forward_unit = start_forward / np.clip(np.linalg.norm(start_forward), 1e-8, None)
                heading_similarity = np.clip(candidate_forwards @ start_forward_unit, -1.0, 1.0)
                entry_heading_penalty = 1.0 - ((heading_similarity + 1.0) * 0.5)
            else:
                entry_heading_penalty = np.zeros_like(path_scores)

            height_delta = np.abs(remaining_points[:, 2] - current[2])
            entry_height_penalty = height_delta / np.clip(np.max(height_delta), 1e-8, None)

            path_scores = (
                path_scores
                - float(entry_distance_weight) * entry_distance_penalty
                - float(entry_heading_weight) * entry_heading_penalty
                - float(entry_height_weight) * entry_height_penalty
            )
        best_idx = int(np.argmax(path_scores))
        best_point = remaining_points[best_idx]
        best_score = remaining_scores[best_idx]
        chosen.append(best_point)
        chosen_scores.append(best_score)
        current = best_point

        keep = np.linalg.norm(remaining_points - best_point[None, :], axis=1) > min_spacing
        remaining_points = remaining_points[keep]
        remaining_scores = remaining_scores[keep]
        if len(remaining_points) == 0:
            break

    return np.stack(chosen, axis=0), np.asarray(chosen_scores, dtype=np.float64)


def build_path_figure(
    points: np.ndarray,
    point_colors: np.ndarray | None,
    camera_centers: np.ndarray,
    camera_rotations: np.ndarray,
    camera_forwards: np.ndarray,
    candidates: np.ndarray,
    candidate_scores: np.ndarray,
    path_points: np.ndarray,
    path_scores: np.ndarray,
    target: np.ndarray,
    ground_z: float,
    max_points: int,
    point_size: float,
):
    sample_step = max(1, len(points) // max_points)
    pts = points[::sample_step]
    cols = nbv.downsample_colors(point_colors, sample_step)

    fig = go.Figure()
    marker_kwargs = dict(size=point_size, opacity=0.7)
    if cols is None:
        marker_kwargs["color"] = "#d1d5db"
    else:
        marker_kwargs["color"] = nbv.colors_to_plotly(cols)

    fig.add_trace(
        go.Scatter3d(
            x=pts[:, 0], y=pts[:, 1], z=pts[:, 2],
            mode="markers",
            name="Point Cloud",
            marker=marker_kwargs,
        )
    )
    fig.add_trace(
        go.Scatter3d(
            x=camera_centers[:, 0], y=camera_centers[:, 1], z=camera_centers[:, 2],
            mode="lines",
            name="Existing Trajectory",
            line=dict(color="#60a5fa", width=5),
        )
    )
    nbv.add_frustum_trace_3d(fig, camera_centers, camera_rotations)

    ray_xs: list[float | None] = []
    ray_ys: list[float | None] = []
    ray_zs: list[float | None] = []
    for center, forward in zip(camera_centers, camera_forwards):
        if abs(float(forward[2])) > 1e-8:
            t = (ground_z - float(center[2])) / float(forward[2])
        else:
            t = 0.0
        if t <= 0.0:
            t = 0.40
        ray_end = center + forward * t
        ray_xs.extend([center[0], ray_end[0], None])
        ray_ys.extend([center[1], ray_end[1], None])
        ray_zs.extend([center[2], ray_end[2], None])
    fig.add_trace(
        go.Scatter3d(
            x=ray_xs,
            y=ray_ys,
            z=ray_zs,
            mode="lines",
            name="View Rays",
            line=dict(color="#93c5fd", width=3),
        )
    )

    fig.add_trace(
        go.Scatter3d(
            x=[target[0]], y=[target[1]], z=[target[2]],
            mode="markers",
            name="Target Region Center",
            marker=dict(size=8, color="#22c55e"),
        )
    )

    valid_mask = candidate_scores > -1e8
    if np.any(valid_mask):
        valid_candidates = candidates[valid_mask]
        valid_scores = candidate_scores[valid_mask]
        fig.add_trace(
            go.Scatter3d(
                x=valid_candidates[:, 0],
                y=valid_candidates[:, 1],
                z=valid_candidates[:, 2],
                mode="markers",
                name="All Candidate Viewpoints",
                marker=dict(
                    size=5,
                    color=valid_scores,
                    colorscale="Viridis",
                    opacity=0.45,
                    colorbar=dict(title="Candidate Score"),
                    symbol="circle",
                ),
            )
        )

    if len(path_points) > 0:
        fig.add_trace(
            go.Scatter3d(
                x=path_points[:, 0], y=path_points[:, 1], z=path_points[:, 2],
                mode="lines+markers+text",
                name="Recommended Path",
                text=[str(i + 1) for i in range(len(path_points))],
                textposition="top center",
                line=dict(color="#ef4444", width=6),
                marker=dict(
                    size=7,
                    color=path_scores,
                    colorscale="Turbo",
                    colorbar=dict(title="Path Score"),
                    symbol="diamond",
                ),
            )
        )

    fig.update_layout(
        title="Greedy Recommended Candidate Path",
        template="plotly_dark",
        scene=dict(
            xaxis=dict(
                title="X",
                showgrid=False,
                zeroline=False,
                showbackground=False,
                showspikes=False,
            ),
            yaxis=dict(
                title="Y",
                showgrid=False,
                zeroline=False,
                showbackground=False,
                showspikes=False,
            ),
            zaxis=dict(
                title="Z",
                showgrid=False,
                zeroline=False,
                showbackground=False,
                showspikes=False,
            ),
            aspectmode="data",
        ),
        margin=dict(l=0, r=0, t=60, b=0),
        height=900,
    )
    return fig


def main():
    parser = argparse.ArgumentParser(description="Generate a greedy recommended viewpoint path from NBV candidates.")
    parser.add_argument("--base-dir", default=r"C:\Users\sdh97\Desktop\d_rock")
    parser.add_argument("--ply", default="rgb.ply")
    parser.add_argument("--poses", default="rgb_poses.json")
    parser.add_argument("--target-x", type=float, default=None)
    parser.add_argument("--target-y", type=float, default=None)
    parser.add_argument("--target-z", type=float, default=None)
    parser.add_argument("--max-steps", type=int, default=17)
    parser.add_argument("--min-spacing", type=float, default=0.20)
    parser.add_argument("--azimuth-count", type=int, default=24)
    parser.add_argument("--radius-scale", type=float, default=1.0)
    parser.add_argument("--min-radius", type=float, default=None)
    parser.add_argument("--max-radius", type=float, default=None)
    parser.add_argument("--radius-override", type=float, default=None)
    parser.add_argument("--height-min", type=float, default=None)
    parser.add_argument("--height-max", type=float, default=None)
    parser.add_argument("--height-offsets", type=str, default=None, help="Comma-separated offsets along up-dir")
    parser.add_argument("--support-radius", type=float, default=None)
    parser.add_argument("--visibility-mode", choices=["projection", "voxel_occlusion"], default="projection")
    parser.add_argument("--voxel-size", type=float, default=0.12)
    parser.add_argument("--coverage-weight", type=float, default=0.65)
    parser.add_argument("--novelty-weight", type=float, default=0.35)
    parser.add_argument("--distance-penalty-weight", type=float, default=0.10)
    parser.add_argument("--entry-distance-weight", type=float, default=0.35)
    parser.add_argument("--entry-heading-weight", type=float, default=0.25)
    parser.add_argument("--entry-height-weight", type=float, default=0.20)
    parser.add_argument("--fov-deg", type=float, default=70.0)
    parser.add_argument("--max-points", type=int, default=150000)
    parser.add_argument("--point-size", type=float, default=0.8)
    parser.add_argument("--output-html", default=None)
    parser.add_argument("--output-json", default=None)
    args = parser.parse_args()

    base_dir = Path(args.base_dir)
    points, point_colors = nbv.load_point_cloud(base_dir / args.ply)
    poses = nbv.load_pose_json(base_dir / args.poses)
    camera_centers = np.stack([nbv.pose_center(item) for item in poses], axis=0)
    camera_rotations = np.stack([nbv.pose_rotation(item) for item in poses], axis=0)
    camera_forwards = np.stack([nbv.pose_forward(item) for item in poses], axis=0)

    ground_center, up_dir = nbv.estimate_ground_frame(points, camera_centers)
    camera_altitudes = (camera_centers - ground_center[None, :]) @ up_dir
    observed_min_altitude = float(camera_altitudes.min())
    observed_max_altitude = float(camera_altitudes.max())
    min_altitude = observed_min_altitude if args.height_min is None else float(args.height_min)
    max_altitude = observed_max_altitude if args.height_max is None else float(args.height_max)
    if max_altitude < min_altitude:
        max_altitude = min_altitude
    min_y = float(camera_centers[:, 1].min())
    max_y = float(camera_centers[:, 1].max())
    height_offsets = None
    if args.height_offsets:
        height_offsets = np.array([float(x) for x in args.height_offsets.split(",") if x.strip()], dtype=np.float64)

    if args.target_x is not None and args.target_y is not None and args.target_z is not None:
        target = np.asarray([args.target_x, args.target_y, args.target_z], dtype=np.float64)
        support_dist = np.linalg.norm(points - target[None, :], axis=1)
        if args.support_radius is not None:
            support_radius = float(args.support_radius)
        else:
            support_radius = max(2.5, float(np.percentile(np.linalg.norm(camera_centers - target[None, :], axis=1), 35)))
        support_mask = support_dist <= support_radius
        support_points = points[support_mask]
        if len(support_points) == 0:
            nearest_count = min(500, len(points))
            nearest_idx = np.argsort(support_dist)[:nearest_count]
            support_points = points[nearest_idx]
    else:
        target, support_points = nbv.pick_target(points, camera_centers)
    candidates = nbv.make_candidate_positions(
        target,
        camera_centers,
        ground_center,
        up_dir,
        azimuth_count=args.azimuth_count,
        min_altitude=min_altitude,
        max_altitude=max_altitude,
        min_y=min_y,
        max_y=max_y,
        radius_scale=args.radius_scale,
        min_radius=args.min_radius,
        max_radius=args.max_radius,
        radius_override=args.radius_override,
        height_offsets=height_offsets,
    )
    candidates[:, 1] = np.clip(candidates[:, 1], min_y, max_y)
    candidate_heights = (candidates - ground_center[None, :]) @ up_dir
    clamped_heights = np.clip(candidate_heights, min_altitude, max_altitude)
    candidates = candidates + (clamped_heights - candidate_heights)[:, None] * up_dir[None, :]

    avg_radius = float(np.mean(np.linalg.norm(camera_centers - camera_centers.mean(axis=0), axis=1)))
    occupancy = None
    if args.visibility_mode == "voxel_occlusion":
        occupancy = nbv.build_voxel_occupancy(points, float(args.voxel_size))
    candidate_scores = score_candidates(
        candidates,
        target,
        support_points,
        camera_centers,
        camera_forwards,
        up_dir,
        avg_radius,
        args.fov_deg,
        visibility_mode=args.visibility_mode,
        occupancy=occupancy,
        voxel_size=float(args.voxel_size),
        coverage_weight=args.coverage_weight,
        novelty_weight=args.novelty_weight,
        distance_penalty_weight=args.distance_penalty_weight,
    )

    start = camera_centers[-1]
    start_forward = camera_forwards[-1] if len(camera_forwards) > 0 else None
    path_points, path_scores = greedy_path(
        start,
        candidates,
        candidate_scores,
        target,
        start_forward=start_forward,
        max_steps=args.max_steps,
        min_spacing=args.min_spacing,
        entry_distance_weight=args.entry_distance_weight,
        entry_heading_weight=args.entry_heading_weight,
        entry_height_weight=args.entry_height_weight,
    )

    fig = build_path_figure(
        points,
        point_colors,
        camera_centers,
        camera_rotations,
        camera_forwards,
        candidates,
        candidate_scores,
        path_points,
        path_scores,
        target,
        float(points[:, 2].min()),
        max_points=args.max_points,
        point_size=args.point_size,
    )

    output_html = Path(args.output_html) if args.output_html else base_dir / "recommended_candidate_path.html"
    output_json = Path(args.output_json) if args.output_json else base_dir / "recommended_candidate_path.json"
    fig.write_html(str(output_html), include_plotlyjs="cdn")

    summary = {
        "base_dir": str(base_dir),
        "path_step_count": int(len(path_points)),
        "pose_count": int(len(camera_centers)),
        "azimuth_count": int(args.azimuth_count),
        "min_spacing": float(args.min_spacing),
        "start_position": start.tolist(),
        "target_region_center": target.tolist(),
        "candidate_count": int(len(candidates)),
        "support_point_count": int(len(support_points)),
        "observed_avg_radius": float(avg_radius),
        "radius_scale": float(args.radius_scale),
        "min_radius": None if args.min_radius is None else float(args.min_radius),
        "max_radius": None if args.max_radius is None else float(args.max_radius),
        "radius_override": None if args.radius_override is None else float(args.radius_override),
        "height_min": float(min_altitude),
        "height_max": float(max_altitude),
        "observed_min_altitude_along_up": observed_min_altitude,
        "observed_max_altitude_along_up": observed_max_altitude,
        "support_radius": None if args.support_radius is None else float(args.support_radius),
        "visibility_mode": args.visibility_mode,
        "voxel_size": float(args.voxel_size),
        "coverage_weight": float(args.coverage_weight),
        "novelty_weight": float(args.novelty_weight),
        "distance_penalty_weight": float(args.distance_penalty_weight),
        "entry_distance_weight": float(args.entry_distance_weight),
        "entry_heading_weight": float(args.entry_heading_weight),
        "entry_height_weight": float(args.entry_height_weight),
        "candidate_bounds": {
            "min": candidates.min(axis=0).tolist(),
            "max": candidates.max(axis=0).tolist(),
        },
        "waypoints": [
            {"index": i + 1, "position": path_points[i].tolist(), "score": float(path_scores[i])}
            for i in range(len(path_points))
        ],
        "min_altitude_along_up": min_altitude,
        "max_altitude_along_up": max_altitude,
        "min_y": min_y,
        "max_y": max_y,
    }
    output_json.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    print(f"\nSaved path HTML: {output_html}")
    print(f"Saved path JSON: {output_json}")


if __name__ == "__main__":
    main()
