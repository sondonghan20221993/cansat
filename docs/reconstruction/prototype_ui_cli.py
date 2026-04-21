from __future__ import annotations

import argparse
import json
import os
import webbrowser
from dataclasses import asdict, dataclass
from typing import List, Sequence

import numpy as np

from reconstruction.artifact_loader import load_reconstruction_artifact
from reconstruction.backends.dust3r_backend import Dust3rBackend
from reconstruction.backends.feature_sfm_backend import FeatureSfmBackend
from reconstruction.config import ReconstructionConfig
from reconstruction.models.job import ImageDescriptor
from reconstruction.validation.image_validator import ImageValidator


@dataclass
class FrameTransform:
    frame: str
    scale: float
    yaw_deg: float
    pitch_deg: float
    roll_deg: float
    translate_x: float
    translate_y: float
    translate_z: float


def _build_descriptors(image_paths: Sequence[str]) -> List[ImageDescriptor]:
    return [
        ImageDescriptor(
            image_id=f"img-{idx + 1}",
            timestamp=idx,
            source_path=os.path.abspath(path),
            metadata={},
        )
        for idx, path in enumerate(image_paths)
    ]


def _frame_matrix(frame: str) -> np.ndarray:
    if frame == "opencv":
        return np.eye(3, dtype=np.float64)
    if frame == "enu":
        # OpenCV camera frame (x:right, y:down, z:forward) -> ENU-like (x:east, y:north, z:up)
        return np.array(
            [
                [1.0, 0.0, 0.0],
                [0.0, 0.0, 1.0],
                [0.0, -1.0, 0.0],
            ],
            dtype=np.float64,
        )
    raise ValueError(f"Unsupported frame preset: {frame}")


def _rotation_matrix(yaw_deg: float, pitch_deg: float, roll_deg: float) -> np.ndarray:
    yaw = np.deg2rad(yaw_deg)
    pitch = np.deg2rad(pitch_deg)
    roll = np.deg2rad(roll_deg)

    rz = np.array(
        [
            [np.cos(yaw), -np.sin(yaw), 0.0],
            [np.sin(yaw), np.cos(yaw), 0.0],
            [0.0, 0.0, 1.0],
        ],
        dtype=np.float64,
    )
    ry = np.array(
        [
            [np.cos(pitch), 0.0, np.sin(pitch)],
            [0.0, 1.0, 0.0],
            [-np.sin(pitch), 0.0, np.cos(pitch)],
        ],
        dtype=np.float64,
    )
    rx = np.array(
        [
            [1.0, 0.0, 0.0],
            [0.0, np.cos(roll), -np.sin(roll)],
            [0.0, np.sin(roll), np.cos(roll)],
        ],
        dtype=np.float64,
    )
    return rz @ ry @ rx


def _apply_transform(points: np.ndarray, transform: FrameTransform) -> tuple[np.ndarray, np.ndarray]:
    base = _frame_matrix(transform.frame)
    rot = _rotation_matrix(transform.yaw_deg, transform.pitch_deg, transform.roll_deg)
    linear = transform.scale * (rot @ base)
    translation = np.array(
        [transform.translate_x, transform.translate_y, transform.translate_z],
        dtype=np.float64,
    )
    transformed = points @ linear.T + translation
    return transformed, linear


def _sample_points(points: np.ndarray, colors: np.ndarray, max_points: int) -> tuple[np.ndarray, np.ndarray]:
    if len(points) <= max_points:
        return points, colors
    idx = np.linspace(0, len(points) - 1, num=max_points, dtype=np.int64)
    return points[idx], colors[idx]


def _transform_camera_trajectory(
    trajectory: Sequence[dict],
    transform: FrameTransform,
) -> list[dict]:
    if not trajectory:
        return []

    base = _frame_matrix(transform.frame)
    rot = _rotation_matrix(transform.yaw_deg, transform.pitch_deg, transform.roll_deg)
    linear = transform.scale * (rot @ base)
    translation = np.array(
        [transform.translate_x, transform.translate_y, transform.translate_z],
        dtype=np.float64,
    )

    transformed = []
    for node in trajectory:
        pos = np.asarray(node.get("position", [0.0, 0.0, 0.0]), dtype=np.float64)
        out = pos @ linear.T + translation
        transformed.append({
            "image_id": node.get("image_id"),
            "source_path": node.get("source_path"),
            "position": [float(out[0]), float(out[1]), float(out[2])],
        })
    return transformed


def _build_html(
    title: str,
    points: np.ndarray,
    colors: np.ndarray,
    camera_trajectory: Sequence[dict],
    matrix: np.ndarray,
    transform: FrameTransform,
    quality: dict,
) -> str:
    mins = points.min(axis=0)
    maxs = points.max(axis=0)
    span = float(max(maxs - mins))
    axis_len = 1.0 if span <= 0.0 else 0.25 * span

    points_payload = points.tolist()
    colors_payload = colors.tolist()
    cameras_payload = list(camera_trajectory)
    matrix_payload = matrix.tolist()
    transform_payload = asdict(transform)

    return f"""<!doctype html>
<html lang=\"en\">
<head>
  <meta charset=\"utf-8\" />
  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />
  <title>{title}</title>
  <script src=\"https://cdn.plot.ly/plotly-2.35.2.min.js\"></script>
  <style>
    :root {{
      --bg: #0d1117;
      --panel: #161b22;
      --fg: #e6edf3;
      --muted: #8b949e;
      --accent: #58a6ff;
      --border: #30363d;
    }}
    body {{
      margin: 0;
      background: radial-gradient(1200px 600px at 10% 10%, #172033, var(--bg));
      color: var(--fg);
      font-family: Consolas, "Courier New", monospace;
      display: grid;
      grid-template-columns: 320px 1fr;
      min-height: 100vh;
    }}
    .panel {{
      border-right: 1px solid var(--border);
      padding: 16px;
      background: linear-gradient(180deg, #121923, var(--panel));
      overflow: auto;
    }}
    .panel h1 {{ font-size: 16px; margin: 0 0 10px; color: var(--accent); }}
    .panel h2 {{ font-size: 13px; margin: 16px 0 8px; color: #c9d1d9; }}
    .panel pre {{
      margin: 0;
      font-size: 12px;
      color: var(--muted);
      white-space: pre-wrap;
      word-break: break-word;
      border: 1px solid var(--border);
      border-radius: 8px;
      padding: 10px;
      background: rgba(0,0,0,0.2);
    }}
    #plot {{ width: 100%; height: 100vh; }}
  </style>
</head>
<body>
  <aside class=\"panel\">
    <h1>Reconstruction Viewer</h1>
    <h2>Transform</h2>
    <pre id=\"transform\"></pre>
    <h2>Linear Matrix (3x3)</h2>
    <pre id=\"matrix\"></pre>
    <h2>Quality</h2>
    <pre id=\"quality\"></pre>
  </aside>
  <main id=\"plot\"></main>

  <script>
    const points = {json.dumps(points_payload)};
    const colors = {json.dumps(colors_payload)};
    const cameras = {json.dumps(cameras_payload)};
    const transform = {json.dumps(transform_payload)};
    const linearMatrix = {json.dumps(matrix_payload)};
    const quality = {json.dumps(quality)};
    const axisLength = {axis_len};

    const xs = points.map(p => p[0]);
    const ys = points.map(p => p[1]);
    const zs = points.map(p => p[2]);
    const rgb = colors.map(c => `rgb(${{c[0]}},${{c[1]}},${{c[2]}})`);

    const cloud = {{
      type: 'scatter3d',
      mode: 'markers',
      x: xs,
      y: ys,
      z: zs,
      marker: {{ size: 2, color: rgb, opacity: 0.9 }},
      name: 'point-cloud'
    }};

        const camX = cameras.map(c => c.position[0]);
        const camY = cameras.map(c => c.position[1]);
        const camZ = cameras.map(c => c.position[2]);
        const camText = cameras.map(c => `${{c.image_id}}\n${{c.source_path || ''}}`);
        const cameraTrace = {{
            type: 'scatter3d',
            mode: 'markers+lines',
            x: camX,
            y: camY,
            z: camZ,
            text: camText,
            hovertemplate: '%{text}<extra></extra>',
            marker: {{ size: 6, color: '#f59e0b' }},
            line: {{ color: '#fbbf24', width: 3 }},
            name: 'camera-trajectory'
        }};

    const axisX = {{
      type: 'scatter3d', mode: 'lines',
      x: [0, axisLength], y: [0, 0], z: [0, 0],
      line: {{ color: '#ff4d4f', width: 6 }}, name: 'X'
    }};
    const axisY = {{
      type: 'scatter3d', mode: 'lines',
      x: [0, 0], y: [0, axisLength], z: [0, 0],
      line: {{ color: '#52c41a', width: 6 }}, name: 'Y'
    }};
    const axisZ = {{
      type: 'scatter3d', mode: 'lines',
      x: [0, 0], y: [0, 0], z: [0, axisLength],
      line: {{ color: '#1677ff', width: 6 }}, name: 'Z'
    }};

    const layout = {{
      paper_bgcolor: '#0d1117',
      plot_bgcolor: '#0d1117',
      font: {{ color: '#e6edf3' }},
      margin: {{ l: 0, r: 0, b: 0, t: 30 }},
      title: '3D Reconstruction (Fixed Coordinate Frame)',
      scene: {{
        aspectmode: 'cube',
        xaxis: {{ title: 'X' }},
        yaxis: {{ title: 'Y' }},
        zaxis: {{ title: 'Z' }}
      }}
    }};

    Plotly.newPlot('plot', [cloud, cameraTrace, axisX, axisY, axisZ], layout, {{responsive: true}});

    document.getElementById('transform').textContent = JSON.stringify(transform, null, 2);
    document.getElementById('matrix').textContent = JSON.stringify(linearMatrix, null, 2);
    document.getElementById('quality').textContent = JSON.stringify(quality, null, 2);

        const plot = document.getElementById('plot');
        const qualityPanel = document.getElementById('quality');
        plot.on('plotly_click', function(evt) {{
            if (!evt || !evt.points || evt.points.length === 0) return;
            const p = evt.points[0];
            if (!p || p.curveNumber !== 1 || p.pointIndex == null) return;
            const cam = cameras[p.pointIndex];
            if (!cam) return;
            const payload = {{
                selected_image_id: cam.image_id,
                selected_image_path: cam.source_path,
                selected_camera_position: cam.position,
            }};
            qualityPanel.textContent = JSON.stringify({{...quality, ...payload}}, null, 2);
        }});
  </script>
</body>
</html>
"""


def main(argv: List[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Run reconstruction and visualize fixed-frame coordinates in a local HTML UI.")
    parser.add_argument("images", nargs="*", help="Input image paths. Omit when --artifact is used.")
    parser.add_argument("--artifact", default=None, help="Server-generated 3D artifact to visualize directly (.glb or ASCII .ply)")
    parser.add_argument("--metadata-json", default=None, help="Optional sidecar metadata JSON with camera_trajectory and quality fields")
    parser.add_argument("--image-set-id", default="prototype-ui", help="Logical image set identifier")
    parser.add_argument("--backend", default="feature_sfm", choices=["feature_sfm", "dust3r"], help="Reconstruction backend")
    parser.add_argument("--frame", default="enu", choices=["opencv", "enu"], help="Base coordinate frame preset")
    parser.add_argument("--scale", type=float, default=1.0, help="Uniform coordinate scale")
    parser.add_argument("--yaw-deg", type=float, default=0.0, help="Yaw rotation in degrees")
    parser.add_argument("--pitch-deg", type=float, default=0.0, help="Pitch rotation in degrees")
    parser.add_argument("--roll-deg", type=float, default=0.0, help="Roll rotation in degrees")
    parser.add_argument("--tx", type=float, default=0.0, help="Translation X")
    parser.add_argument("--ty", type=float, default=0.0, help="Translation Y")
    parser.add_argument("--tz", type=float, default=0.0, help="Translation Z")
    parser.add_argument("--max-points", type=int, default=15000, help="Max displayed points in UI")
    parser.add_argument("--output-html", default=None, help="Output HTML path")
    parser.add_argument("--open", action="store_true", help="Open generated HTML in default browser")
    args = parser.parse_args(argv)

    if args.max_points <= 0:
        raise ValueError("--max-points must be > 0")

    raw_result = {}
    validation_stats = {}
    source_mode = "images"
    backend_name = args.backend
    if args.artifact:
        artifact = load_reconstruction_artifact(args.artifact, metadata_path=args.metadata_json)
        points = artifact.points
        colors = artifact.colors
        raw_result = {
            "camera_trajectory": artifact.camera_trajectory,
            "artifact_quality": artifact.quality,
        }
        source_mode = "artifact"
        backend_name = f"artifact:{artifact.output_format}"
    else:
        if not args.images:
            parser.error("provide input images or use --artifact")
        descriptors = _build_descriptors(args.images)
        config = ReconstructionConfig(backend_name=args.backend)
        validator = ImageValidator(config.min_image_count)
        report = validator.validate(descriptors)
        if not report.is_valid:
            print(json.dumps({
                "status": "failed",
                "error": "INSUFFICIENT_VALID_IMAGES",
                "accepted_count": len(report.accepted),
                "min_required": config.min_image_count,
                "rejected": [{"image_id": img.image_id, "reason": reason} for img, reason in report.rejected],
            }, indent=2))
            return 1

        backend = FeatureSfmBackend() if args.backend == "feature_sfm" else Dust3rBackend(model_name=args.backend)
        backend.load()
        try:
            preprocessed = backend.preprocess(report.accepted)
            raw_result = backend.infer(preprocessed)
        finally:
            backend.unload()

        points = np.asarray(raw_result["points"], dtype=np.float64)
        colors = np.asarray(raw_result["colors"], dtype=np.uint8)
        validation_stats = report.recorded_stats

    points, colors = _sample_points(points, colors, max_points=args.max_points)

    transform = FrameTransform(
        frame=args.frame,
        scale=args.scale,
        yaw_deg=args.yaw_deg,
        pitch_deg=args.pitch_deg,
        roll_deg=args.roll_deg,
        translate_x=args.tx,
        translate_y=args.ty,
        translate_z=args.tz,
    )
    transformed_points, linear = _apply_transform(points, transform)
    camera_trajectory = _transform_camera_trajectory(raw_result.get("camera_trajectory", []), transform)

    output_html = args.output_html
    if output_html is None:
        output_dir = os.path.join("artifacts", "reconstruction", "ui")
        os.makedirs(output_dir, exist_ok=True)
        output_html = os.path.join(output_dir, f"{args.image_set_id}_viewer.html")

    html = _build_html(
        title=f"Reconstruction Viewer - {args.image_set_id}",
        points=transformed_points,
        colors=colors,
        camera_trajectory=camera_trajectory,
        matrix=linear,
        transform=transform,
        quality={
            "source_mode": source_mode,
            "backend": backend_name,
            "images_used": len(args.images),
            "point_count_displayed": int(len(transformed_points)),
            "camera_count": len(camera_trajectory),
            "match_count": raw_result.get("match_count"),
            "inlier_count": raw_result.get("inlier_count"),
            "successful_pairs": raw_result.get("successful_pairs"),
            "validation_stats": validation_stats,
            "artifact_quality": raw_result.get("artifact_quality", {}),
        },
    )

    abs_output = os.path.abspath(output_html)
    with open(abs_output, "w", encoding="utf-8") as fp:
        fp.write(html)

    print(json.dumps({
        "status": "success",
        "viewer_html": abs_output,
        "source_mode": source_mode,
        "frame": transform.frame,
        "linear_matrix": linear.tolist(),
        "point_count_displayed": int(len(transformed_points)),
        "camera_count": len(camera_trajectory),
    }, indent=2))

    if args.open:
        webbrowser.open(f"file:///{abs_output.replace(os.sep, '/')}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
