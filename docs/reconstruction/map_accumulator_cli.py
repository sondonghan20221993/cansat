from __future__ import annotations

import argparse
import json
import os
import threading
import webbrowser
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Sequence
from urllib.parse import parse_qs, urlparse
from urllib.request import urlopen

import numpy as np

from reconstruction.artifact_loader import load_reconstruction_artifact
from reconstruction.map_manifest import (
    ChunkTransform,
    MapChunk,
    create_manifest,
    load_manifest,
    save_manifest,
)


def _parse_linear(values: Sequence[float] | None) -> list[list[float]]:
    if values is None:
        return ChunkTransform().linear
    if len(values) != 9:
        raise ValueError("--linear requires exactly 9 values")
    return [
        [float(values[0]), float(values[1]), float(values[2])],
        [float(values[3]), float(values[4]), float(values[5])],
        [float(values[6]), float(values[7]), float(values[8])],
    ]


def _parse_translate(values: Sequence[float] | None) -> list[float]:
    if values is None:
        return [0.0, 0.0, 0.0]
    if len(values) != 3:
        raise ValueError("--translate requires exactly 3 values")
    return [float(values[0]), float(values[1]), float(values[2])]


def _resolve_artifact_path(manifest_path: str, artifact_ref: str) -> str:
    if os.path.isabs(artifact_ref):
        return artifact_ref
    return os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(manifest_path)), artifact_ref))


def _apply_chunk_transform(points: np.ndarray, transform: ChunkTransform) -> np.ndarray:
    linear = np.asarray(transform.linear, dtype=np.float64)
    translate = np.asarray(transform.translate, dtype=np.float64)
    return transform.scale * (points @ linear.T) + translate


def _identity_if_unaligned(transform: ChunkTransform | None) -> ChunkTransform:
    return transform if transform is not None else ChunkTransform()


def _sample_points(points: np.ndarray, colors: np.ndarray, max_points: int) -> tuple[np.ndarray, np.ndarray]:
    if len(points) <= max_points:
        return points, colors
    idx = np.linspace(0, len(points) - 1, num=max_points, dtype=np.int64)
    return points[idx], colors[idx]


def _build_map_html(title: str, traces: list[dict[str, Any]], manifest_payload: dict[str, Any]) -> str:
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
      background: #0d1117;
      color: var(--fg);
      font-family: Consolas, \"Courier New\", monospace;
      display: grid;
      grid-template-columns: 340px 1fr;
      min-height: 100vh;
    }}
    .panel {{
      border-right: 1px solid var(--border);
      padding: 16px;
      background: var(--panel);
      overflow: auto;
    }}
    h1 {{ font-size: 16px; margin: 0 0 10px; color: var(--accent); }}
    h2 {{ font-size: 13px; margin: 16px 0 8px; color: #c9d1d9; }}
    pre {{
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
    <h1>Accumulated Map</h1>
    <h2>Manifest</h2>
    <pre id=\"manifest\"></pre>
  </aside>
  <main id=\"plot\"></main>
  <script>
    const traces = {json.dumps(traces)};
    const manifest = {json.dumps(manifest_payload)};
    const axisLength = 1.0;
    traces.push(
      {{type: 'scatter3d', mode: 'lines', x: [0, axisLength], y: [0, 0], z: [0, 0], line: {{color: '#ff4d4f', width: 6}}, name: 'X'}},
      {{type: 'scatter3d', mode: 'lines', x: [0, 0], y: [0, axisLength], z: [0, 0], line: {{color: '#52c41a', width: 6}}, name: 'Y'}},
      {{type: 'scatter3d', mode: 'lines', x: [0, 0], y: [0, 0], z: [0, axisLength], line: {{color: '#1677ff', width: 6}}, name: 'Z'}}
    );
    Plotly.newPlot('plot', traces, {{
      paper_bgcolor: '#0d1117',
      plot_bgcolor: '#0d1117',
      font: {{color: '#e6edf3'}},
      margin: {{l: 0, r: 0, b: 0, t: 30}},
      title: 'Accumulated Reconstruction Map',
      scene: {{aspectmode: 'data', xaxis: {{title: 'X'}}, yaxis: {{title: 'Y'}}, zaxis: {{title: 'Z'}}}}
    }}, {{responsive: true}});
    document.getElementById('manifest').textContent = JSON.stringify(manifest, null, 2);
  </script>
</body>
</html>
"""


def _build_live_map_html(title: str, poll_interval_ms: int) -> str:
    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>{title}</title>
  <script src="https://cdn.plot.ly/plotly-2.35.2.min.js"></script>
  <style>
    :root {{
      --bg: #0d1117; --panel: #161b22; --fg: #e6edf3; --muted: #8b949e; --accent: #58a6ff; --border: #30363d;
    }}
    body {{ margin:0; background:#0d1117; color:var(--fg); font-family:Consolas, "Courier New", monospace; display:grid; grid-template-columns:340px 1fr; min-height:100vh; }}
    .panel {{ border-right:1px solid var(--border); padding:16px; background:var(--panel); overflow:auto; }}
    h1 {{ font-size:16px; margin:0 0 10px; color:var(--accent); }}
    h2 {{ font-size:13px; margin:16px 0 8px; color:#c9d1d9; }}
    pre {{ margin:0; font-size:12px; color:var(--muted); white-space:pre-wrap; word-break:break-word; border:1px solid var(--border); border-radius:8px; padding:10px; background:rgba(0,0,0,0.2); }}
    #plot {{ width:100%; height:100vh; }}
  </style>
</head>
<body>
  <aside class="panel">
    <h1>Accumulated Map Live Viewer</h1>
    <h2>Status</h2>
    <pre id="status"></pre>
    <h2>Manifest</h2>
    <pre id="manifest"></pre>
  </aside>
  <main id="plot"></main>
  <script>
    const plotEl = document.getElementById('plot');
    const statusEl = document.getElementById('status');
    const manifestEl = document.getElementById('manifest');
    const knownChunks = new Set();
    const traces = [];
    const axisTraces = [
      {{type:'scatter3d', mode:'lines', x:[0,1], y:[0,0], z:[0,0], line:{{color:'#ff4d4f', width:6}}, name:'X'}},
      {{type:'scatter3d', mode:'lines', x:[0,0], y:[0,1], z:[0,0], line:{{color:'#52c41a', width:6}}, name:'Y'}},
      {{type:'scatter3d', mode:'lines', x:[0,0], y:[0,0], z:[0,1], line:{{color:'#1677ff', width:6}}, name:'Z'}}
    ];

    Plotly.newPlot(plotEl, axisTraces.slice(), {{
      paper_bgcolor:'#0d1117', plot_bgcolor:'#0d1117', font:{{color:'#e6edf3'}}, margin:{{l:0,r:0,b:0,t:30}},
      title:'Accumulated Reconstruction Map (Live)', scene:{{aspectmode:'data', xaxis:{{title:'X'}}, yaxis:{{title:'Y'}}, zaxis:{{title:'Z'}}}}
    }}, {{responsive:true}});

    function updateStatus(state) {{
      statusEl.textContent = JSON.stringify({{
        chunk_count: state.chunk_count,
        rendered_point_count: state.rendered_point_count,
        last_updated: state.last_updated,
        poll_interval_ms: {poll_interval_ms}
      }}, null, 2);
      manifestEl.textContent = JSON.stringify(state.manifest_summary, null, 2);
    }}

    async function poll() {{
      const response = await fetch('/map_state');
      const state = await response.json();
      updateStatus(state);
      for (const chunk of state.chunks) {{
        if (knownChunks.has(chunk.chunk_id)) continue;
        knownChunks.add(chunk.chunk_id);
        traces.push({{
          type: 'scatter3d',
          mode: 'markers',
          x: chunk.points.map(p => p[0]),
          y: chunk.points.map(p => p[1]),
          z: chunk.points.map(p => p[2]),
          marker: {{ size: 2, color: chunk.colors.map(c => `rgb(${{c[0]}},${{c[1]}},${{c[2]}})`), opacity: 0.85 }},
          name: chunk.chunk_id,
          customdata: chunk.points.map(_ => [chunk.alignment_status, chunk.artifact_ref]),
          hovertemplate: `${{chunk.chunk_id}}<br>%{{customdata[0]}}<br>%{{customdata[1]}}<extra></extra>`
        }});
        Plotly.addTraces(plotEl, [traces[traces.length - 1]]);
      }}
    }}

    poll();
    setInterval(() => {{ poll().catch(console.error); }}, {poll_interval_ms});
  </script>
</body>
</html>
"""


def _build_live_session_html(title: str, poll_interval_ms: int) -> str:
    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>{title}</title>
  <script src="https://cdn.plot.ly/plotly-2.35.2.min.js"></script>
  <style>
    :root {{
      --bg: #0d1117; --panel: #161b22; --fg: #e6edf3; --muted: #8b949e; --accent: #58a6ff; --border: #30363d;
    }}
    body {{ margin:0; background:#0d1117; color:var(--fg); font-family:Consolas, "Courier New", monospace; display:grid; grid-template-columns:340px 1fr; min-height:100vh; }}
    .panel {{ border-right:1px solid var(--border); padding:16px; background:var(--panel); overflow:auto; }}
    h1 {{ font-size:16px; margin:0 0 10px; color:var(--accent); }}
    h2 {{ font-size:13px; margin:16px 0 8px; color:#c9d1d9; }}
    pre {{ margin:0; font-size:12px; color:var(--muted); white-space:pre-wrap; word-break:break-word; border:1px solid var(--border); border-radius:8px; padding:10px; background:rgba(0,0,0,0.2); }}
    #plot {{ width:100%; height:100vh; }}
  </style>
</head>
<body>
  <aside class="panel">
    <h1>Session Live Viewer</h1>
    <h2>Status</h2>
    <pre id="status"></pre>
    <h2>Session State</h2>
    <pre id="session-state"></pre>
  </aside>
  <main id="plot"></main>
  <script>
    const plotEl = document.getElementById('plot');
    const statusEl = document.getElementById('status');
    const sessionStateEl = document.getElementById('session-state');
    const axisTraces = [
      {{type:'scatter3d', mode:'lines', x:[0,1], y:[0,0], z:[0,0], line:{{color:'#ff4d4f', width:6}}, name:'X'}},
      {{type:'scatter3d', mode:'lines', x:[0,0], y:[0,1], z:[0,0], line:{{color:'#52c41a', width:6}}, name:'Y'}},
      {{type:'scatter3d', mode:'lines', x:[0,0], y:[0,0], z:[0,1], line:{{color:'#1677ff', width:6}}, name:'Z'}}
    ];
    Plotly.newPlot(plotEl, axisTraces.slice(), {{
      paper_bgcolor:'#0d1117', plot_bgcolor:'#0d1117', font:{{color:'#e6edf3'}}, margin:{{l:0,r:0,b:0,t:30}},
      title:'Session Reconstruction Map (Live)', scene:{{aspectmode:'data', xaxis:{{title:'X'}}, yaxis:{{title:'Y'}}, zaxis:{{title:'Z'}}}}
    }}, {{responsive:true}});

    function frustumSegments(position, scale) {{
      const [x, y, z] = position;
      const s = scale;
      const corners = [
        [x + s, y + s, z + 2*s],
        [x - s, y + s, z + 2*s],
        [x - s, y - s, z + 2*s],
        [x + s, y - s, z + 2*s]
      ];
      const lines = [];
      for (const c of corners) {{
        lines.push([[x, y, z], c]);
      }}
      for (let i = 0; i < corners.length; i++) {{
        lines.push([corners[i], corners[(i + 1) % corners.length]]);
      }}
      return lines;
    }}

    function buildTraces(state) {{
      const traces = axisTraces.slice();
      const poses = (state.pose_stream_ref && state.pose_stream_ref.poses) ? state.pose_stream_ref.poses : [];
      if (poses.length) {{
        traces.push({{
          type: 'scatter3d',
          mode: 'lines+markers',
          x: poses.map(p => p.position[0]),
          y: poses.map(p => p.position[1]),
          z: poses.map(p => p.position[2]),
          marker: {{ size: 3, color: '#58a6ff' }},
          line: {{ width: 4, color: '#58a6ff' }},
          name: 'Trajectory',
          hovertemplate: '%{{text}}<extra></extra>',
          text: poses.map(p => `${{p.image_id}} (#${{p.index}})`)
        }});

        const frustumX = [];
        const frustumY = [];
        const frustumZ = [];
        for (const pose of poses.filter(p => p.is_keyframe)) {{
          const segments = frustumSegments(pose.position, 0.03);
          for (const segment of segments) {{
            frustumX.push(segment[0][0], segment[1][0], null);
            frustumY.push(segment[0][1], segment[1][1], null);
            frustumZ.push(segment[0][2], segment[1][2], null);
          }}
        }}
        if (frustumX.length) {{
          traces.push({{
            type: 'scatter3d',
            mode: 'lines',
            x: frustumX,
            y: frustumY,
            z: frustumZ,
            line: {{ width: 3, color: '#ff4d4f' }},
            name: 'Keyframes'
          }});
        }}
      }}

      if (state.map_points && state.map_points.length) {{
        traces.push({{
          type: 'scatter3d',
          mode: 'markers',
          x: state.map_points.map(p => p[0]),
          y: state.map_points.map(p => p[1]),
          z: state.map_points.map(p => p[2]),
          marker: {{
            size: 2,
            color: state.map_colors.map(c => `rgb(${{c[0]}},${{c[1]}},${{c[2]}})`),
            opacity: 0.85
          }},
          name: 'Map'
        }});
      }}
      return traces;
    }}

    async function poll() {{
      const response = await fetch('/session_state');
      const state = await response.json();
      statusEl.textContent = JSON.stringify({{
        session_id: state.session_id,
        status: state.status,
        frame_count: state.frame_count,
        keyframe_count: state.keyframe_count,
        rendered_point_count: state.rendered_point_count,
        tracking_state: state.tracking_state,
        alignment_status: state.alignment_status,
        last_updated: state.last_updated,
        poll_interval_ms: {poll_interval_ms}
      }}, null, 2);
      sessionStateEl.textContent = JSON.stringify(state.session_summary, null, 2);
      Plotly.react(plotEl, buildTraces(state), {{
        paper_bgcolor:'#0d1117', plot_bgcolor:'#0d1117', font:{{color:'#e6edf3'}}, margin:{{l:0,r:0,b:0,t:30}},
        title:'Session Reconstruction Map (Live)', scene:{{aspectmode:'data', xaxis:{{title:'X'}}, yaxis:{{title:'Y'}}, zaxis:{{title:'Z'}}}}
      }}, {{responsive:true}});
    }}

    poll();
    setInterval(() => {{ poll().catch(console.error); }}, {poll_interval_ms});
  </script>
</body>
</html>
"""


def render_map(manifest_path: str, output_html: str, max_points_per_chunk: int) -> dict[str, Any]:
    if max_points_per_chunk <= 0:
        raise ValueError("--max-points-per-chunk must be > 0")
    manifest = load_manifest(manifest_path)
    traces: list[dict[str, Any]] = []
    rendered_chunks = 0
    displayed_points = 0
    for chunk in manifest.active_chunks():
        artifact_path = _resolve_artifact_path(manifest_path, chunk.artifact_ref)
        artifact = load_reconstruction_artifact(artifact_path)
        points, colors = _sample_points(artifact.points, artifact.colors, max_points_per_chunk)
        points = _apply_chunk_transform(points, _identity_if_unaligned(chunk.transform))
        traces.append({
            "type": "scatter3d",
            "mode": "markers",
            "x": points[:, 0].tolist(),
            "y": points[:, 1].tolist(),
            "z": points[:, 2].tolist(),
            "marker": {
                "size": 2,
                "color": [f"rgb({int(c[0])},{int(c[1])},{int(c[2])})" for c in colors],
                "opacity": 0.85,
            },
            "name": chunk.chunk_id,
            "customdata": [[chunk.alignment_status, chunk.artifact_ref] for _ in range(len(points))],
            "hovertemplate": f"{chunk.chunk_id}<br>%{{customdata[0]}}<br>%{{customdata[1]}}<extra></extra>",
        })
        rendered_chunks += 1
        displayed_points += int(len(points))

    html = _build_map_html(f"Accumulated Map - {manifest.map_id}", traces, {
        "map_id": manifest.map_id,
        "display_frame_id": manifest.display_frame_id,
        "chunk_count": len(manifest.chunks),
        "rendered_chunk_count": rendered_chunks,
        "displayed_point_count": displayed_points,
    })
    abs_output = os.path.abspath(output_html)
    os.makedirs(os.path.dirname(abs_output) or ".", exist_ok=True)
    with open(abs_output, "w", encoding="utf-8") as fp:
        fp.write(html)
    return {
        "status": "rendered",
        "viewer_html": abs_output,
        "rendered_chunk_count": rendered_chunks,
        "displayed_point_count": displayed_points,
    }


def build_map_state(manifest_path: str, max_points_per_chunk: int) -> dict[str, Any]:
    manifest = load_manifest(manifest_path)
    chunks_payload: list[dict[str, Any]] = []
    total_points = 0
    for chunk in manifest.active_chunks():
        artifact_path = _resolve_artifact_path(manifest_path, chunk.artifact_ref)
        artifact = load_reconstruction_artifact(artifact_path)
        points, colors = _sample_points(artifact.points, artifact.colors, max_points_per_chunk)
        points = _apply_chunk_transform(points, _identity_if_unaligned(chunk.transform))
        total_points += int(len(points))
        chunks_payload.append({
            "chunk_id": chunk.chunk_id,
            "alignment_status": chunk.alignment_status,
            "artifact_ref": chunk.artifact_ref,
            "points": points.tolist(),
            "colors": colors.tolist(),
        })
    return {
        "chunk_count": len(chunks_payload),
        "rendered_point_count": total_points,
        "last_updated": manifest.updated_at,
        "manifest_summary": {
            "map_id": manifest.map_id,
            "display_frame_id": manifest.display_frame_id,
            "updated_at": manifest.updated_at,
            "chunk_ids": [chunk.chunk_id for chunk in manifest.active_chunks()],
        },
        "chunks": chunks_payload,
    }


def _fetch_json(url: str) -> dict[str, Any]:
    with urlopen(url) as response:  # noqa: S310
        return json.loads(response.read().decode("utf-8"))


def _sample_pose_points(poses: list[dict[str, Any]], max_points: int) -> tuple[list[dict[str, Any]], int]:
    total = len(poses)
    if total <= max_points:
        return poses, total
    idx = np.linspace(0, total - 1, num=max_points, dtype=np.int64)
    return [poses[int(i)] for i in idx], total


def build_session_state(session_endpoint: str, session_id: str, max_points: int) -> dict[str, Any]:
    endpoint = session_endpoint.rstrip("/")
    if endpoint.startswith("file:///"):
        state = _fetch_json(endpoint)
    else:
        state = _fetch_json(f"{endpoint}/sessions/{session_id}/state")
    poses_payload = []
    raw_poses = []
    if isinstance(state.get("pose_stream_ref"), dict):
        raw_poses = list(state["pose_stream_ref"].get("poses", []))
    sampled_poses, total_pose_count = _sample_pose_points(raw_poses, max_points)
    for pose in sampled_poses:
        index = int(pose.get("index", 0))
        poses_payload.append({
            "image_id": pose.get("image_id", f"frame-{index:06d}"),
            "index": index,
            "position": [
                float(index),
                float(index % 5) * 0.1,
                float(index % 7) * 0.05,
            ],
            "is_keyframe": bool(pose.get("is_keyframe", index % 5 == 0)),
            "source_path": pose.get("source_path"),
        })

    map_points: list[list[float]] = []
    map_colors: list[list[int]] = []
    for pose in poses_payload:
        x, y, z = pose["position"]
        map_points.extend([
            [x, y, z],
            [x + 0.05, y + 0.02, z + 0.01],
            [x - 0.05, y - 0.02, z + 0.015],
        ])
        map_colors.extend([[88, 166, 255], [255, 77, 79], [82, 196, 26]])

    rendered_point_count = state.get("rendered_point_count")
    if rendered_point_count is None:
        rendered_point_count = len(raw_poses) * 3
    return {
        "session_id": state["session_id"],
        "status": state["status"],
        "frame_count": state.get("frame_count", len(raw_poses)),
        "keyframe_count": state.get("keyframe_count", sum(1 for p in poses_payload if p["is_keyframe"])),
        "rendered_point_count": int(rendered_point_count),
        "tracking_state": state.get("tracking_state", "unknown"),
        "alignment_status": state.get("alignment_status", "UNALIGNED"),
        "last_updated": state.get("last_updated"),
        "pose_stream_ref": {"poses": poses_payload, "total_pose_count": total_pose_count},
        "map_points": map_points,
        "map_colors": map_colors,
        "session_summary": {
            "session_id": state["session_id"],
            "current_frame_ref": state.get("current_frame_ref"),
            "pose_stream_ref": state.get("pose_stream_ref"),
            "map_state_ref": state.get("map_state_ref"),
            "world_transform": state.get("world_transform"),
            "artifact_ref": state.get("artifact_ref"),
            "output_format": state.get("output_format"),
        },
    }


def serve_live_map(manifest_path: str, host: str, port: int, poll_interval_s: float, max_points_per_chunk: int) -> None:
    viewer_html = _build_live_map_html("Accumulated Map Live Viewer", int(max(poll_interval_s, 0.1) * 1000))

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:  # noqa: N802
            parsed = urlparse(self.path)
            if parsed.path == "/":
                payload = viewer_html.encode("utf-8")
                self.send_response(HTTPStatus.OK)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)
                return
            if parsed.path == "/map_state":
                payload = json.dumps(build_map_state(manifest_path, max_points_per_chunk)).encode("utf-8")
                self.send_response(HTTPStatus.OK)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.send_header("Cache-Control", "no-store")
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)
                return
            self.send_error(HTTPStatus.NOT_FOUND)

        def log_message(self, format: str, *args: Any) -> None:  # noqa: A003
            return

    server = ThreadingHTTPServer((host, port), Handler)
    try:
        server.serve_forever()
    finally:
        server.server_close()


def serve_live_session(session_endpoint: str, session_id: str, host: str, port: int, poll_interval_s: float, max_points: int) -> None:
    viewer_html = _build_live_session_html("Session Live Viewer", int(max(poll_interval_s, 0.1) * 1000))

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:  # noqa: N802
            parsed = urlparse(self.path)
            if parsed.path == "/":
                payload = viewer_html.encode("utf-8")
                self.send_response(HTTPStatus.OK)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)
                return
            if parsed.path == "/session_state":
                payload = json.dumps(build_session_state(session_endpoint, session_id, max_points)).encode("utf-8")
                self.send_response(HTTPStatus.OK)
                self.send_header("Content-Type", "application/json; charset=utf-8")
                self.send_header("Cache-Control", "no-store")
                self.send_header("Content-Length", str(len(payload)))
                self.end_headers()
                self.wfile.write(payload)
                return
            self.send_error(HTTPStatus.NOT_FOUND)

        def log_message(self, format: str, *args: Any) -> None:  # noqa: A003
            return

    server = ThreadingHTTPServer((host, port), Handler)
    try:
        server.serve_forever()
    finally:
        server.server_close()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Create, update, and render accumulated reconstruction map manifests.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    create = subparsers.add_parser("create_manifest")
    create.add_argument("--manifest", required=True)
    create.add_argument("--map-id", required=True)
    create.add_argument("--display-frame-id", default="map")

    append = subparsers.add_parser("append_chunk")
    append.add_argument("--manifest", required=True)
    append.add_argument("--chunk-id", required=True)
    append.add_argument("--job-id", required=True)
    append.add_argument("--image-set-id", required=True)
    append.add_argument("--artifact-ref", required=True)
    append.add_argument("--output-format", required=True, choices=["ply", "glb"])
    append.add_argument("--alignment-status", default="UNALIGNED", choices=["ALIGNED", "PARTIAL_ALIGNMENT", "UNALIGNED"])
    append.add_argument("--scale", type=float, default=1.0)
    append.add_argument("--linear", nargs=9, type=float)
    append.add_argument("--translate", nargs=3, type=float)

    update = subparsers.add_parser("update_chunk_transform")
    update.add_argument("--manifest", required=True)
    update.add_argument("--chunk-id", required=True)
    update.add_argument("--alignment-status", required=True, choices=["ALIGNED", "PARTIAL_ALIGNMENT", "UNALIGNED"])
    update.add_argument("--scale", type=float, default=1.0)
    update.add_argument("--linear", nargs=9, type=float)
    update.add_argument("--translate", nargs=3, type=float)

    invalidate = subparsers.add_parser("invalidate_chunk")
    invalidate.add_argument("--manifest", required=True)
    invalidate.add_argument("--chunk-id", required=True)

    render = subparsers.add_parser("render_map")
    render.add_argument("--manifest", required=True)
    render.add_argument("--output-html", required=True)
    render.add_argument("--max-points-per-chunk", type=int, default=15000)
    render.add_argument("--open", action="store_true")

    live = subparsers.add_parser("serve_live_map")
    live.add_argument("--manifest", required=True)
    live.add_argument("--host", default="127.0.0.1")
    live.add_argument("--port", type=int, default=8011)
    live.add_argument("--poll-interval-s", type=float, default=5.0)
    live.add_argument("--max-points-per-chunk", type=int, default=15000)
    live.add_argument("--open", action="store_true")

    live_session = subparsers.add_parser("serve_live_session")
    live_session.add_argument("--session-endpoint", required=True)
    live_session.add_argument("--session-id", required=True)
    live_session.add_argument("--host", default="127.0.0.1")
    live_session.add_argument("--port", type=int, default=8012)
    live_session.add_argument("--poll-interval-s", type=float, default=5.0)
    live_session.add_argument("--max-pose-points", type=int, default=500)
    live_session.add_argument("--open", action="store_true")

    args = parser.parse_args(argv)

    if args.command == "create_manifest":
        manifest = create_manifest(args.map_id, args.display_frame_id)
        path = save_manifest(manifest, args.manifest)
        payload = {"status": "created", "manifest": path, "map_id": manifest.map_id}
    elif args.command == "append_chunk":
        manifest = load_manifest(args.manifest)
        transform = None
        if args.alignment_status != "UNALIGNED":
            transform = ChunkTransform(args.scale, _parse_linear(args.linear), _parse_translate(args.translate))
        try:
            manifest.append_chunk(MapChunk(
                chunk_id=args.chunk_id,
                job_id=args.job_id,
                image_set_id=args.image_set_id,
                artifact_ref=args.artifact_ref,
                output_format=args.output_format,
                alignment_status=args.alignment_status,
                transform=transform,
            ))
            path = save_manifest(manifest, args.manifest)
            payload = {"status": "appended", "manifest": path, "chunk_id": args.chunk_id}
        except ValueError as exc:
            if "job_id already exists" in str(exc):
                payload = {"status": "duplicate_rejected", "manifest": os.path.abspath(args.manifest), "job_id": args.job_id}
            else:
                raise
    elif args.command == "update_chunk_transform":
        manifest = load_manifest(args.manifest)
        transform = None
        if args.alignment_status != "UNALIGNED":
            transform = ChunkTransform(args.scale, _parse_linear(args.linear), _parse_translate(args.translate))
        try:
            manifest.update_chunk_transform(args.chunk_id, transform, args.alignment_status)
            path = save_manifest(manifest, args.manifest)
            payload = {"status": "updated", "manifest": path, "chunk_id": args.chunk_id}
        except KeyError:
            payload = {"status": "not_found", "manifest": os.path.abspath(args.manifest), "chunk_id": args.chunk_id}
    elif args.command == "invalidate_chunk":
        manifest = load_manifest(args.manifest)
        manifest.invalidate_chunk(args.chunk_id)
        path = save_manifest(manifest, args.manifest)
        payload = {"status": "invalidated", "manifest": path, "chunk_id": args.chunk_id, "invalidated": True}
    else:
        if args.command == "render_map":
            payload = render_map(args.manifest, args.output_html, args.max_points_per_chunk)
            if args.open:
                webbrowser.open(f"file:///{payload['viewer_html'].replace(os.sep, '/')}")
        elif args.command == "serve_live_map":
            url = f"http://{args.host}:{args.port}/"
            if args.open:
                threading.Timer(0.3, lambda: webbrowser.open(url)).start()
            print(json.dumps({
                "status": "serving",
                "url": url,
                "manifest": os.path.abspath(args.manifest),
                "poll_interval_s": args.poll_interval_s,
            }, indent=2))
            serve_live_map(args.manifest, args.host, args.port, args.poll_interval_s, args.max_points_per_chunk)
            return 0
        else:
            url = f"http://{args.host}:{args.port}/"
            if args.open:
                threading.Timer(0.3, lambda: webbrowser.open(url)).start()
            print(json.dumps({
                "status": "serving",
                "url": url,
                "session_endpoint": args.session_endpoint,
                "session_id": args.session_id,
                "poll_interval_s": args.poll_interval_s,
            }, indent=2))
            serve_live_session(args.session_endpoint, args.session_id, args.host, args.port, args.poll_interval_s, args.max_pose_points)
            return 0

    print(json.dumps(payload, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
