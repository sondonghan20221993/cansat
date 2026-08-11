import argparse
import json
import webbrowser
from pathlib import Path

import numpy as np
import open3d as o3d
import plotly.graph_objects as go


def load_point_cloud(path: Path) -> tuple[np.ndarray, np.ndarray | None]:
    pcd = o3d.io.read_point_cloud(str(path))
    if pcd.is_empty():
        raise ValueError(f"Point cloud is empty or unreadable: {path}")
    points = np.asarray(pcd.points, dtype=np.float64)
    colors = np.asarray(pcd.colors, dtype=np.float64)
    if colors.size == 0 or len(colors) != len(points):
        return points, None
    return points, colors


def load_pose_json(path: Path):
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, list):
        raise ValueError("Pose JSON must contain a list.")
    return data


def pose_center(item: dict) -> np.ndarray:
    mat = np.array(item["T_wc"], dtype=np.float64)
    return mat[:3, 3].copy()


def pose_forward(item: dict) -> np.ndarray:
    mat = np.array(item["T_wc"], dtype=np.float64)
    rot = mat[:3, :3]
    # Empirically for this pipeline, the camera looks along -Z in camera coordinates.
    return normalize(-rot[:, 2].copy())


def pose_rotation(item: dict) -> np.ndarray:
    mat = np.array(item["T_wc"], dtype=np.float64)
    return mat[:3, :3].copy()


def normalize(v: np.ndarray) -> np.ndarray:
    n = np.linalg.norm(v)
    if n <= 1e-12:
        return np.zeros_like(v)
    return v / n


def downsample_points(points: np.ndarray, every: int) -> np.ndarray:
    if every <= 1:
        return points
    return points[::every]


def downsample_colors(colors: np.ndarray | None, every: int) -> np.ndarray | None:
    if colors is None:
        return None
    if every <= 1:
        return colors
    return colors[::every]


def filter_points_near_main_body(points: np.ndarray) -> np.ndarray:
    center = points.mean(axis=0)
    distances = np.linalg.norm(points - center, axis=1)
    threshold = np.percentile(distances, 85)
    filtered = points[distances <= threshold]
    return filtered if len(filtered) > 0 else points


def estimate_ground_frame(points: np.ndarray, camera_centers: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    filtered = filter_points_near_main_body(points)
    sample = downsample_points(filtered, max(1, len(filtered) // 20000))
    center = sample.mean(axis=0)
    centered = sample - center
    cov = centered.T @ centered / max(len(centered), 1)
    eigvals, eigvecs = np.linalg.eigh(cov)
    normal = normalize(eigvecs[:, np.argmin(eigvals)])
    camera_mean = camera_centers.mean(axis=0)
    if np.dot(camera_mean - center, normal) < 0.0:
        normal = -normal
    return center, normal


def pick_target(points: np.ndarray, camera_centers: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    # Fast heuristic:
    # 1) downsample points
    # 2) remove far outliers from the main point-cloud body
    # 3) score points by mean distance from existing cameras
    # 4) among top far points, choose a dense local centroid
    filtered = filter_points_near_main_body(points)
    pts = downsample_points(filtered, max(1, len(filtered) // 3000))
    dists = np.linalg.norm(pts[:, None, :] - camera_centers[None, :, :], axis=2)
    mean_dist = dists.mean(axis=1)
    top_k = min(200, len(pts))
    top_idx = np.argsort(mean_dist)[-top_k:]
    far_pts = pts[top_idx]
    target = far_pts.mean(axis=0)
    return target, far_pts


def recommend_camera(target: np.ndarray, camera_centers: np.ndarray) -> np.ndarray:
    mean_center = camera_centers.mean(axis=0)
    avg_radius = np.mean(np.linalg.norm(camera_centers - mean_center, axis=1))
    avg_radius = max(float(avg_radius), 0.5)

    # Place the next camera on the opposite side of the existing camera centroid,
    # looking back toward the target.
    direction = normalize(target - mean_center)
    if np.linalg.norm(direction) <= 1e-12:
        direction = np.array([1.0, 0.0, 0.0], dtype=np.float64)
    recommended = target - direction * avg_radius
    return recommended


def make_candidate_positions(
    target: np.ndarray,
    camera_centers: np.ndarray,
    ground_center: np.ndarray,
    up_dir: np.ndarray,
    azimuth_count: int = 12,
    min_altitude: float = 2.0,
    max_altitude: float = 8.0,
    min_y: float | None = None,
    max_y: float | None = None,
    radius_scale: float = 1.0,
    min_radius: float | None = None,
    max_radius: float | None = None,
    radius_override: float | None = None,
    height_offsets: np.ndarray | None = None,
) -> np.ndarray:
    mean_center = camera_centers.mean(axis=0)
    avg_radius = np.mean(np.linalg.norm(camera_centers - mean_center, axis=1))
    avg_radius = max(float(avg_radius), 0.5)
    if radius_override is not None:
        candidate_radius = float(radius_override)
    else:
        candidate_radius = avg_radius * float(radius_scale)
        if min_radius is not None:
            candidate_radius = max(candidate_radius, float(min_radius))
        if max_radius is not None:
            candidate_radius = min(candidate_radius, float(max_radius))
    z_span = float(np.ptp(camera_centers[:, 2]))
    if height_offsets is None:
        vertical_offsets = np.array([0.0, 0.18, 0.36], dtype=np.float64) * max(z_span, 0.8)
    else:
        vertical_offsets = np.asarray(height_offsets, dtype=np.float64)

    ref = np.array([1.0, 0.0, 0.0], dtype=np.float64)
    if abs(float(np.dot(ref, up_dir))) > 0.9:
        ref = np.array([0.0, 1.0, 0.0], dtype=np.float64)
    tangent_a = normalize(np.cross(up_dir, ref))
    tangent_b = normalize(np.cross(up_dir, tangent_a))

    ground_target = target - np.dot(target - ground_center, up_dir) * up_dir
    ground_camera_mean = mean_center - np.dot(mean_center - ground_center, up_dir) * up_dir
    base_dir_plane = normalize(ground_target - ground_camera_mean)
    if np.linalg.norm(base_dir_plane) <= 1e-12:
        base_dir_plane = tangent_a
    base_angle = float(np.arctan2(np.dot(base_dir_plane, tangent_b), np.dot(base_dir_plane, tangent_a)))
    target_height = float(np.dot(target - ground_center, up_dir))
    candidate_floor_height = max(target_height, float(min_altitude))
    candidate_ceil_height = max(candidate_floor_height, float(max_altitude))

    candidates: list[np.ndarray] = []
    for level_offset in vertical_offsets:
        h = float(np.clip(candidate_floor_height + level_offset, candidate_floor_height, candidate_ceil_height))
        for idx in range(azimuth_count):
            angle = base_angle + (2.0 * np.pi * idx / azimuth_count)
            planar = target + candidate_radius * (np.cos(angle) * tangent_a + np.sin(angle) * tangent_b)
            current_h = float(np.dot(planar - ground_center, up_dir))
            pos = planar + (h - current_h) * up_dir
            if min_y is not None and pos[1] < min_y:
                pos[1] = min_y
            if max_y is not None and pos[1] > max_y:
                pos[1] = max_y
            candidates.append(pos)
    return np.stack(candidates, axis=0)


def make_camera_basis(origin: np.ndarray, target: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    forward = normalize(target - origin)
    world_up = np.array([0.0, 0.0, 1.0], dtype=np.float64)
    if abs(float(np.dot(forward, world_up))) > 0.95:
        world_up = np.array([0.0, 1.0, 0.0], dtype=np.float64)
    right = normalize(np.cross(forward, world_up))
    up = normalize(np.cross(right, forward))
    return right, up, forward


def projection_coverage_score(origin: np.ndarray, target: np.ndarray, support_points: np.ndarray, fov_deg: float = 70.0, grid_size: int = 24) -> float:
    right, up, forward = make_camera_basis(origin, target)
    rel = support_points - origin[None, :]
    depth = rel @ forward
    visible = depth > 1e-4
    if not np.any(visible):
        return 0.0
    rel_vis = rel[visible]
    depth_vis = depth[visible]
    x_cam = (rel_vis @ right) / depth_vis
    y_cam = (rel_vis @ up) / depth_vis
    limit = float(np.tan(np.deg2rad(fov_deg * 0.5)))
    inside = (np.abs(x_cam) <= limit) & (np.abs(y_cam) <= limit)
    if not np.any(inside):
        return 0.0
    xn = (x_cam[inside] / limit + 1.0) * 0.5
    yn = (y_cam[inside] / limit + 1.0) * 0.5
    xi = np.clip((xn * (grid_size - 1)).astype(int), 0, grid_size - 1)
    yi = np.clip((yn * (grid_size - 1)).astype(int), 0, grid_size - 1)
    occupied = np.zeros((grid_size, grid_size), dtype=bool)
    occupied[yi, xi] = True
    return float(occupied.sum()) / float(grid_size * grid_size)


def build_voxel_occupancy(points: np.ndarray, voxel_size: float) -> set[tuple[int, int, int]]:
    if voxel_size <= 1e-8:
        raise ValueError(f"voxel_size must be > 0, got {voxel_size}")
    coords = np.floor(points / float(voxel_size)).astype(np.int32)
    return {tuple(row.tolist()) for row in coords}


def voxel_occlusion_coverage_score(
    origin: np.ndarray,
    target: np.ndarray,
    support_points: np.ndarray,
    occupancy: set[tuple[int, int, int]],
    voxel_size: float,
    fov_deg: float = 70.0,
    grid_size: int = 24,
    max_support_points: int = 1200,
) -> float:
    right, up, forward = make_camera_basis(origin, target)
    rel = support_points - origin[None, :]
    depth = rel @ forward
    visible = depth > 1e-4
    if not np.any(visible):
        return 0.0

    rel_vis = rel[visible]
    depth_vis = depth[visible]
    pts_vis = support_points[visible]

    limit = float(np.tan(np.deg2rad(fov_deg * 0.5)))
    x_cam = (rel_vis @ right) / depth_vis
    y_cam = (rel_vis @ up) / depth_vis
    inside = (np.abs(x_cam) <= limit) & (np.abs(y_cam) <= limit)
    if not np.any(inside):
        return 0.0

    pts_vis = pts_vis[inside]
    x_cam = x_cam[inside]
    y_cam = y_cam[inside]
    depth_vis = depth_vis[inside]

    if len(pts_vis) > max_support_points:
        step = max(1, len(pts_vis) // max_support_points)
        pts_vis = pts_vis[::step][:max_support_points]
        x_cam = x_cam[::step][:max_support_points]
        y_cam = y_cam[::step][:max_support_points]
        depth_vis = depth_vis[::step][:max_support_points]

    origin_voxel = np.floor(origin / float(voxel_size)).astype(np.int32)
    occupied = np.zeros((grid_size, grid_size), dtype=bool)

    for pt, xc, yc, depth_pt in zip(pts_vis, x_cam, y_cam, depth_vis):
        delta = pt - origin
        ray_len = float(np.linalg.norm(delta))
        if ray_len <= 1e-8:
            continue
        step_count = max(2, int(np.ceil(depth_pt / float(voxel_size))))
        blocked = False
        for s in range(1, step_count):
            alpha = s / step_count
            sample = origin + delta * alpha
            voxel = tuple(np.floor(sample / float(voxel_size)).astype(np.int32).tolist())
            if voxel == tuple(origin_voxel.tolist()):
                continue
            if voxel in occupancy:
                blocked = True
                break
        if blocked:
            continue

        xn = (xc / limit + 1.0) * 0.5
        yn = (yc / limit + 1.0) * 0.5
        xi = int(np.clip(xn * (grid_size - 1), 0, grid_size - 1))
        yi = int(np.clip(yn * (grid_size - 1), 0, grid_size - 1))
        occupied[yi, xi] = True

    return float(occupied.sum()) / float(grid_size * grid_size)


def select_projection_best_view(
    target: np.ndarray,
    support_points: np.ndarray,
    camera_centers: np.ndarray,
    camera_forwards: np.ndarray,
    ground_center: np.ndarray,
    up_dir: np.ndarray,
    fov_deg: float = 70.0,
    min_altitude: float = 2.0,
    max_altitude: float = 8.0,
    min_y: float | None = None,
    max_y: float | None = None,
) -> tuple[np.ndarray, np.ndarray]:
    candidates = make_candidate_positions(
        target,
        camera_centers,
        ground_center,
        up_dir,
        azimuth_count=12,
        min_altitude=min_altitude,
        max_altitude=max_altitude,
        min_y=min_y,
        max_y=max_y,
    )
    # Soft-constrain candidates into the feasible box instead of rejecting them
    # outright. This keeps the prototype producing usable candidates even when
    # the initial sampling falls slightly outside the allowed region.
    candidates[:, 1] = np.clip(candidates[:, 1], min_y if min_y is not None else candidates[:, 1], max_y if max_y is not None else candidates[:, 1])
    candidate_heights = (candidates - ground_center[None, :]) @ up_dir
    clamped_heights = np.clip(candidate_heights, min_altitude, max_altitude)
    candidates = candidates + (clamped_heights - candidate_heights)[:, None] * up_dir[None, :]

    scores = np.zeros(len(candidates), dtype=np.float64)
    existing_dirs = camera_forwards.copy()
    existing_dirs = existing_dirs / np.clip(np.linalg.norm(existing_dirs, axis=1, keepdims=True), 1e-8, None)
    avg_radius = float(np.mean(np.linalg.norm(camera_centers - camera_centers.mean(axis=0), axis=1)))

    for i, candidate in enumerate(candidates):
        view_dir = normalize(target - candidate)
        # Require the viewing direction to be level or downward relative to the
        # estimated ground-up direction. Upward-looking candidates are invalid.
        if float(np.dot(view_dir, up_dir)) > 0.0:
            scores[i] = -1e9
            continue
        coverage = projection_coverage_score(candidate, target, support_points, fov_deg=fov_deg, grid_size=24)
        similarity = existing_dirs @ view_dir
        novelty = 1.0 - float(np.max(similarity))
        distance_penalty = abs(float(np.linalg.norm(candidate - target)) - avg_radius)
        distance_penalty = min(distance_penalty / 5.0, 1.0)
        scores[i] = 0.65 * coverage + 0.35 * novelty - 0.10 * distance_penalty

    best_idx = int(np.argmax(scores))
    return candidates[best_idx], scores


def add_direction_vector(fig, origin: np.ndarray, direction: np.ndarray, length: float, color: str, name=None, showlegend=False, width: int = 5, dash=None):
    unit = normalize(direction)
    tip = origin + unit * length
    fig.add_trace(
        go.Scatter3d(
            x=[origin[0], tip[0]],
            y=[origin[1], tip[1]],
            z=[origin[2], tip[2]],
            mode="lines",
            name=name,
            showlegend=showlegend,
            line=dict(color=color, width=width, dash=dash),
        )
    )
    fig.add_trace(
        go.Scatter3d(
            x=[tip[0]],
            y=[tip[1]],
            z=[tip[2]],
            mode="markers",
            name=None,
            showlegend=False,
            marker=dict(size=4, color=color, symbol="diamond"),
        )
    )


def add_ray_fan(fig, origin: np.ndarray, targets: np.ndarray, color: str, name: str, showlegend: bool, width: int = 2, opacity: float = 0.35, dash=None):
    for idx, target in enumerate(targets):
        fig.add_trace(
            go.Scatter3d(
                x=[origin[0], target[0]],
                y=[origin[1], target[1]],
                z=[origin[2], target[2]],
                mode="lines",
                name=name if idx == 0 else None,
                showlegend=showlegend if idx == 0 else False,
                line=dict(color=color, width=width, dash=dash),
                opacity=opacity,
                hoverinfo="skip",
            )
        )


def add_beam_bundle(fig, origins: np.ndarray, targets: np.ndarray, color: str, name: str, showlegend: bool, width: int = 1, opacity: float = 0.12, dash=None):
    xs: list[float | None] = []
    ys: list[float | None] = []
    zs: list[float | None] = []
    for origin in origins:
        for target in targets:
            xs.extend([origin[0], target[0], None])
            ys.extend([origin[1], target[1], None])
            zs.extend([origin[2], target[2], None])
    fig.add_trace(
        go.Scatter3d(
            x=xs,
            y=ys,
            z=zs,
            mode="lines",
            name=name,
            showlegend=showlegend,
            line=dict(color=color, width=width, dash=dash),
            opacity=opacity,
            hoverinfo="skip",
        )
    )


def sample_support_points_for_fan(support_points: np.ndarray, max_count: int = 48) -> np.ndarray:
    if len(support_points) <= max_count:
        return support_points
    step = max(1, len(support_points) // max_count)
    return support_points[::step][:max_count]


def colors_to_plotly(colors: np.ndarray | None) -> list[str] | str:
    if colors is None:
        return "#d1d5db"
    rgb = np.clip(np.round(colors * 255.0), 0, 255).astype(int)
    return [f"rgb({r},{g},{b})" for r, g, b in rgb]


def compute_coverage_scores(points: np.ndarray, camera_centers: np.ndarray, camera_forwards: np.ndarray, fov_deg: float = 70.0) -> np.ndarray:
    cos_thresh = float(np.cos(np.deg2rad(fov_deg * 0.5)))
    scores = np.zeros(len(points), dtype=np.float64)
    chunk = 4000
    for start in range(0, len(points), chunk):
        end = min(start + chunk, len(points))
        pts = points[start:end]
        vecs = pts[:, None, :] - camera_centers[None, :, :]
        dists = np.linalg.norm(vecs, axis=2)
        unit = vecs / np.clip(dists[..., None], 1e-8, None)
        cosines = np.einsum("pck,ck->pc", unit, camera_forwards)
        visible = cosines >= cos_thresh
        weighted = np.where(visible, cosines / np.clip(dists, 1.0, None), 0.0)
        scores[start:end] = weighted.sum(axis=1)
    return scores


def build_camera_frustum_segments(
    center: np.ndarray,
    rotation: np.ndarray,
    scale: float = 0.10,
    aspect: float = 1.2,
) -> tuple[np.ndarray, np.ndarray]:
    corners_cam = np.array(
        [
            [-aspect, -1.0, 1.6],
            [aspect, -1.0, 1.6],
            [aspect, 1.0, 1.6],
            [-aspect, 1.0, 1.6],
        ],
        dtype=np.float64,
    ) * scale
    corners_world = (rotation @ corners_cam.T).T + center[None, :]
    segments = []
    for corner in corners_world:
        segments.append((center, corner))
    for i, j in [(0, 1), (1, 2), (2, 3), (3, 0)]:
        segments.append((corners_world[i], corners_world[j]))
    return corners_world, np.array(segments, dtype=np.float64)


def add_frustum_trace_3d(fig, camera_centers: np.ndarray, camera_rotations: np.ndarray, scene_name: str = "scene"):
    xs: list[float | None] = []
    ys: list[float | None] = []
    zs: list[float | None] = []
    for center, rotation in zip(camera_centers, camera_rotations):
        _, segments = build_camera_frustum_segments(center, rotation)
        for seg_start, seg_end in segments:
            xs.extend([seg_start[0], seg_end[0], None])
            ys.extend([seg_start[1], seg_end[1], None])
            zs.extend([seg_start[2], seg_end[2], None])
    fig.add_trace(
        go.Scatter3d(
            x=xs,
            y=ys,
            z=zs,
            mode="lines",
            name="Camera Frustums",
            line=dict(color="#2563eb", width=2),
            scene=scene_name,
        )
    )


def add_frustum_trace_2d(fig, camera_centers: np.ndarray, camera_rotations: np.ndarray, plane: str, xaxis: str, yaxis: str, name: str):
    xs: list[float | None] = []
    ys: list[float | None] = []
    for center, rotation in zip(camera_centers, camera_rotations):
        _, segments = build_camera_frustum_segments(center, rotation)
        for seg_start, seg_end in segments:
            if plane == "xy":
                xs.extend([seg_start[0], seg_end[0], None])
                ys.extend([seg_start[1], seg_end[1], None])
            elif plane == "yz":
                xs.extend([seg_start[1], seg_end[1], None])
                ys.extend([seg_start[2], seg_end[2], None])
    fig.add_trace(
        go.Scatter(
            x=xs,
            y=ys,
            mode="lines",
            line=dict(color="#2563eb", width=1),
            xaxis=xaxis,
            yaxis=yaxis,
            name=name,
            showlegend=False,
        )
    )


def build_figure(points, point_colors, camera_centers, camera_rotations, camera_forwards, target, support_points, recommended, candidate_positions, candidate_scores, point_size, color_mode: str, fov_deg: float, max_points: int):
    sample_step = max(1, len(points) // max_points)
    pts = points[::sample_step]
    cols = downsample_colors(point_colors, sample_step)
    valid_mask = candidate_scores > -1e8
    valid_candidates = candidate_positions[valid_mask]
    valid_scores = candidate_scores[valid_mask]
    rejected_candidates = candidate_positions[~valid_mask]
    marker_kwargs = dict(size=point_size, opacity=0.32)
    if color_mode == "coverage":
        coverage = compute_coverage_scores(pts, camera_centers, camera_forwards, fov_deg=fov_deg)
        marker_kwargs.update(
            color=coverage,
            colorscale=[
                [0.0, "#ff2d55"],
                [0.2, "#ff9500"],
                [0.45, "#ffd60a"],
                [0.7, "#30d158"],
                [1.0, "#0a84ff"],
            ],
            colorbar=dict(title="Ray Coverage"),
        )
        marker_kwargs["opacity"] = 0.9
    elif color_mode == "rgb":
        marker_kwargs.update(color=colors_to_plotly(cols))
        marker_kwargs["opacity"] = 0.7
    else:
        marker_kwargs.update(color="#d1d5db")
        marker_kwargs["opacity"] = 0.55
    fig = go.Figure()

    # Main 3D view
    fig.add_trace(
        go.Scatter3d(
            x=pts[:, 0], y=pts[:, 1], z=pts[:, 2],
            mode="markers",
            name="Point Cloud",
            marker=marker_kwargs,
            scene="scene",
        )
    )
    fig.add_trace(
        go.Scatter3d(
            x=camera_centers[:, 0], y=camera_centers[:, 1], z=camera_centers[:, 2],
            mode="lines",
            name="Camera Trajectory",
            line=dict(color="#60a5fa", width=5),
            scene="scene",
        )
    )
    add_frustum_trace_3d(fig, camera_centers, camera_rotations, scene_name="scene")
    fig.add_trace(
        go.Scatter3d(
            x=rejected_candidates[:, 0], y=rejected_candidates[:, 1], z=rejected_candidates[:, 2],
            mode="markers",
            name="Rejected Candidates",
            marker=dict(size=4, color="#6b7280", opacity=0.35),
            scene="scene",
        )
    )
    if len(valid_candidates) > 0:
        fig.add_trace(
            go.Scatter3d(
                x=valid_candidates[:, 0], y=valid_candidates[:, 1], z=valid_candidates[:, 2],
                mode="markers",
                name="Projection Candidates",
                marker=dict(
                    size=4,
                    color=valid_scores,
                    colorscale="Viridis",
                    opacity=0.9,
                    showscale=True,
                    colorbar=dict(title="NBV Score"),
                ),
                scene="scene",
            )
        )
    fig.add_trace(
        go.Scatter3d(
            x=[target[0]], y=[target[1]], z=[target[2]],
            mode="markers",
            name="Target Region Center",
            marker=dict(size=8, color="#22c55e"),
            scene="scene",
        )
    )
    fig.add_trace(
        go.Scatter3d(
            x=[recommended[0]], y=[recommended[1]], z=[recommended[2]],
            mode="markers",
            name="Recommended Camera",
            marker=dict(size=10, color="#ef4444", symbol="diamond"),
            scene="scene",
        )
    )

    # Top view (X-Y)
    fig.add_trace(
        go.Scatter(
            x=pts[:, 0], y=pts[:, 1],
            mode="markers",
            name="Point Cloud (Top)",
            marker=dict(size=2, color=marker_kwargs["color"], opacity=0.45),
            xaxis="x2", yaxis="y2",
            showlegend=False,
        )
    )
    fig.add_trace(
        go.Scatter(
            x=camera_centers[:, 0], y=camera_centers[:, 1],
            mode="lines",
            line=dict(color="#60a5fa", width=3),
            xaxis="x2", yaxis="y2",
            name="Camera Trajectory (Top)",
            showlegend=False,
        )
    )
    add_frustum_trace_2d(fig, camera_centers, camera_rotations, plane="xy", xaxis="x2", yaxis="y2", name="Camera Frustums (Top)")
    fig.add_trace(
        go.Scatter(
            x=rejected_candidates[:, 0], y=rejected_candidates[:, 1],
            mode="markers",
            marker=dict(size=5, color="#6b7280", opacity=0.35),
            xaxis="x2", yaxis="y2",
            name="Rejected Candidates (Top)",
            showlegend=False,
        )
    )
    if len(valid_candidates) > 0:
        fig.add_trace(
            go.Scatter(
                x=valid_candidates[:, 0], y=valid_candidates[:, 1],
                mode="markers",
                marker=dict(size=5, color=valid_scores, colorscale="Viridis", opacity=0.9),
                xaxis="x2", yaxis="y2",
                name="Projection Candidates (Top)",
                showlegend=False,
            )
        )
    fig.add_trace(
        go.Scatter(
            x=[target[0]], y=[target[1]],
            mode="markers",
            marker=dict(size=10, color="#22c55e"),
            xaxis="x2", yaxis="y2",
            name="Target (Top)",
            showlegend=False,
        )
    )
    fig.add_trace(
        go.Scatter(
            x=[recommended[0]], y=[recommended[1]],
            mode="markers",
            marker=dict(size=12, color="#ef4444", symbol="diamond"),
            xaxis="x2", yaxis="y2",
            name="Recommended (Top)",
            showlegend=False,
        )
    )

    # Side view (Y-Z)
    fig.add_trace(
        go.Scatter(
            x=pts[:, 1], y=pts[:, 2],
            mode="markers",
            name="Point Cloud (Side)",
            marker=dict(size=2, color=marker_kwargs["color"], opacity=0.45),
            xaxis="x3", yaxis="y3",
            showlegend=False,
        )
    )
    fig.add_trace(
        go.Scatter(
            x=camera_centers[:, 1], y=camera_centers[:, 2],
            mode="lines",
            line=dict(color="#60a5fa", width=3),
            xaxis="x3", yaxis="y3",
            name="Camera Trajectory (Side)",
            showlegend=False,
        )
    )
    add_frustum_trace_2d(fig, camera_centers, camera_rotations, plane="yz", xaxis="x3", yaxis="y3", name="Camera Frustums (Side)")
    fig.add_trace(
        go.Scatter(
            x=rejected_candidates[:, 1], y=rejected_candidates[:, 2],
            mode="markers",
            marker=dict(size=5, color="#6b7280", opacity=0.35),
            xaxis="x3", yaxis="y3",
            name="Rejected Candidates (Side)",
            showlegend=False,
        )
    )
    if len(valid_candidates) > 0:
        fig.add_trace(
            go.Scatter(
                x=valid_candidates[:, 1], y=valid_candidates[:, 2],
                mode="markers",
                marker=dict(size=5, color=valid_scores, colorscale="Viridis", opacity=0.9),
                xaxis="x3", yaxis="y3",
                name="Projection Candidates (Side)",
                showlegend=False,
            )
        )
    fig.add_trace(
        go.Scatter(
            x=[target[1]], y=[target[2]],
            mode="markers",
            marker=dict(size=10, color="#22c55e"),
            xaxis="x3", yaxis="y3",
            name="Target (Side)",
            showlegend=False,
        )
    )
    fig.add_trace(
        go.Scatter(
            x=[recommended[1]], y=[recommended[2]],
            mode="markers",
            marker=dict(size=12, color="#ef4444", symbol="diamond"),
            xaxis="x3", yaxis="y3",
            name="Recommended (Side)",
            showlegend=False,
        )
    )

    fig.update_layout(
        title="Quick Next Viewpoint Prototype (PB-NBV Style)",
        template="plotly_dark",
        scene=dict(
            domain=dict(x=[0.0, 0.6], y=[0.0, 1.0]),
            xaxis_title="X",
            yaxis_title="Y",
            zaxis_title="Z",
            aspectmode="data",
        ),
        xaxis2=dict(domain=[0.66, 1.0], anchor="y2", title="X"),
        yaxis2=dict(domain=[0.55, 1.0], anchor="x2", title="Y", scaleanchor="x2", scaleratio=1),
        xaxis3=dict(domain=[0.66, 1.0], anchor="y3", title="Y"),
        yaxis3=dict(domain=[0.0, 0.45], anchor="x3", title="Z", scaleanchor="x3", scaleratio=1),
        legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="left", x=0.0),
        margin=dict(l=0, r=0, t=60, b=0),
        height=950,
    )
    return fig


def make_marker_cluster(center: np.ndarray, radius: float, samples: int) -> np.ndarray:
    if samples <= 1 or radius <= 1e-9:
        return center[None, :].copy()
    offsets = [np.zeros(3, dtype=np.float64)]
    phi = np.pi * (3.0 - np.sqrt(5.0))
    for i in range(samples - 1):
        y = 1.0 - 2.0 * (i + 0.5) / max(samples - 1, 1)
        r = np.sqrt(max(0.0, 1.0 - y * y))
        theta = phi * i
        x = np.cos(theta) * r
        z = np.sin(theta) * r
        offsets.append(np.array([x, y, z], dtype=np.float64) * radius)
    return center[None, :] + np.stack(offsets, axis=0)


def save_overlay_ply(
    output_path: Path,
    points: np.ndarray,
    point_colors: np.ndarray | None,
    camera_centers: np.ndarray,
    candidate_positions: np.ndarray,
    candidate_scores: np.ndarray,
    recommended: np.ndarray,
) -> None:
    valid_mask = candidate_scores > -1e8
    valid_candidates = candidate_positions[valid_mask]
    rejected_candidates = candidate_positions[~valid_mask]

    cloud_points = [points]
    if point_colors is None:
        cloud_colors = [np.full((len(points), 3), 0.82, dtype=np.float64)]
    else:
        cloud_colors = [point_colors]

    if len(rejected_candidates) > 0:
        rejected_clusters = np.vstack([make_marker_cluster(c, radius=0.06, samples=48) for c in rejected_candidates])
        cloud_points.append(rejected_clusters)
        cloud_colors.append(np.tile(np.array([[0.42, 0.45, 0.50]], dtype=np.float64), (len(rejected_clusters), 1)))

    if len(camera_centers) > 0:
        camera_clusters = np.vstack([make_marker_cluster(c, radius=0.08, samples=56) for c in camera_centers])
        cloud_points.append(camera_clusters)
        cloud_colors.append(np.tile(np.array([[0.38, 0.65, 1.00]], dtype=np.float64), (len(camera_clusters), 1)))

    if len(valid_candidates) > 0:
        valid_clusters = np.vstack([make_marker_cluster(c, radius=0.07, samples=64) for c in valid_candidates])
        cloud_points.append(valid_clusters)
        cloud_colors.append(np.tile(np.array([[0.10, 0.95, 0.95]], dtype=np.float64), (len(valid_clusters), 1)))

    recommended_cluster = make_marker_cluster(recommended, radius=0.22, samples=120)
    cloud_points.append(recommended_cluster)
    cloud_colors.append(np.tile(np.array([[0.94, 0.27, 0.27]], dtype=np.float64), (len(recommended_cluster), 1)))

    merged_points = np.vstack(cloud_points)
    merged_colors = np.vstack(cloud_colors)

    pcd = o3d.geometry.PointCloud()
    pcd.points = o3d.utility.Vector3dVector(merged_points)
    pcd.colors = o3d.utility.Vector3dVector(np.clip(merged_colors, 0.0, 1.0))
    o3d.io.write_point_cloud(str(output_path), pcd, write_ascii=False, compressed=False)


def main():
    parser = argparse.ArgumentParser(description="5-minute prototype: recommend next view from priority point-cloud region.")
    parser.add_argument("--base-dir", default=r"C:\Users\sdh97\Desktop\airsim_degree_notilt_more_distance")
    parser.add_argument("--ply", default="rgb.ply")
    parser.add_argument("--poses", default="rgb_poses.json")
    parser.add_argument("--point-size", type=float, default=0.25)
    parser.add_argument("--max-points", type=int, default=150000)
    parser.add_argument("--color-mode", choices=["rgb", "coverage", "plain"], default="plain")
    parser.add_argument("--fov-deg", type=float, default=70.0)
    parser.add_argument("--min-altitude", type=float, default=None)
    parser.add_argument("--max-altitude", type=float, default=None)
    parser.add_argument("--output-html", default=None)
    parser.add_argument("--output-ply", default=None)
    parser.add_argument("--open", action="store_true")
    args = parser.parse_args()

    base_dir = Path(args.base_dir)
    points, point_colors = load_point_cloud(base_dir / args.ply)
    poses = load_pose_json(base_dir / args.poses)
    camera_centers = np.stack([pose_center(item) for item in poses], axis=0)
    camera_rotations = np.stack([pose_rotation(item) for item in poses], axis=0)
    camera_forwards = np.stack([pose_forward(item) for item in poses], axis=0)
    ground_center, up_dir = estimate_ground_frame(points, camera_centers)
    camera_altitudes = (camera_centers - ground_center[None, :]) @ up_dir
    observed_min_altitude = float(camera_altitudes.min())
    observed_max_altitude = float(camera_altitudes.max())
    effective_min_altitude = observed_min_altitude if args.min_altitude is None else max(observed_min_altitude, float(args.min_altitude))
    effective_max_altitude = observed_max_altitude if args.max_altitude is None else min(observed_max_altitude, float(args.max_altitude))
    if effective_max_altitude < effective_min_altitude:
        effective_max_altitude = effective_min_altitude

    min_y = float(camera_centers[:, 1].min())
    max_y = float(camera_centers[:, 1].max())

    target, support_points = pick_target(points, camera_centers)
    recommended, candidate_scores = select_projection_best_view(
        target,
        support_points,
        camera_centers,
        camera_forwards,
        ground_center,
        up_dir,
        fov_deg=args.fov_deg,
        min_altitude=effective_min_altitude,
        max_altitude=effective_max_altitude,
        min_y=min_y,
        max_y=max_y,
    )
    candidate_positions = make_candidate_positions(
        target,
        camera_centers,
        ground_center,
        up_dir,
        azimuth_count=12,
        min_altitude=effective_min_altitude,
        max_altitude=effective_max_altitude,
        min_y=min_y,
        max_y=max_y,
    )
    anchor_index = int(np.argmin(np.linalg.norm(camera_centers - target[None, :], axis=1)))

    output_html = Path(args.output_html) if args.output_html else base_dir / "next_viewpoint_quick_b.html"
    output_ply = Path(args.output_ply) if args.output_ply else base_dir / "next_viewpoint_result.ply"
    fig = build_figure(points, point_colors, camera_centers, camera_rotations, camera_forwards, target, support_points, recommended, candidate_positions, candidate_scores, args.point_size, args.color_mode, args.fov_deg, args.max_points)
    fig.write_html(str(output_html), include_plotlyjs="cdn")
    save_overlay_ply(output_ply, points, point_colors, camera_centers, candidate_positions, candidate_scores, recommended)

    summary = {
        "base_dir": str(base_dir),
        "existing_camera_count": int(len(camera_centers)),
        "existing_anchor_camera_index": anchor_index,
        "target_region_center": target.tolist(),
        "estimated_up_direction": up_dir.tolist(),
        "recommended_camera_position": recommended.tolist(),
        "recommended_altitude_along_up": float(np.dot(recommended - ground_center, up_dir)),
        "observed_min_camera_altitude_along_up": observed_min_altitude,
        "observed_max_camera_altitude_along_up": observed_max_altitude,
        "min_allowed_y": min_y,
        "max_allowed_y": max_y,
        "recommended_view_direction": normalize(target - recommended).tolist(),
        "candidate_count": int(len(candidate_positions)),
        "best_candidate_score": float(candidate_scores.max()),
        "support_point_count": int(len(support_points)),
        "reference_ray_count": int(len(camera_centers)),
        "color_mode": args.color_mode,
        "fov_deg": args.fov_deg,
        "max_points": int(args.max_points),
        "effective_min_altitude": effective_min_altitude,
        "effective_max_altitude": effective_max_altitude,
    }
    print(json.dumps(summary, indent=2))
    print(f"\nSaved prototype HTML: {output_html}")
    print(f"Saved overlay PLY: {output_ply}")
    if args.open:
        webbrowser.open(output_html.resolve().as_uri())


if __name__ == "__main__":
    main()
