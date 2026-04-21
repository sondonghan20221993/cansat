# Reconstruction Server Quickstart

This quickstart is for the current real-image prototype pipeline.

Current runnable backend:

- `feature_sfm`

Future backend boundary already present:

- `dust3r`

## 1. Create a virtual environment

```bash
cd /path/to/cansat_2/docs
python3.10 -m venv .venv-reconstruction
source .venv-reconstruction/bin/activate
python -m pip install --upgrade pip
python -m pip install -r reconstruction/requirements-prototype.txt
```

## 2. Run the prototype pipeline

```bash
python -m reconstruction.prototype_cli \
  --backend feature_sfm \
  --image-set-id demo \
  /absolute/path/to/image1.png \
  /absolute/path/to/image2.png
```

## 3. Expected output

The command prints a JSON result including:

- `status`
- `output_ref`
- `output_format`
- `quality`

If successful, `output_ref` points to a generated GLB file under:

```text
artifacts/reconstruction/
```

## 3A. Test with fixed coordinate frame in UI

You can also render reconstructed points in a browser UI while fixing the
coordinate frame transform.

```bash
python -m reconstruction.prototype_ui_cli \
  --backend feature_sfm \
  --image-set-id demo-ui \
  --frame enu \
  --yaw-deg 0 --pitch-deg 0 --roll-deg 0 \
  --tx 0 --ty 0 --tz 0 \
  --open \
  /absolute/path/to/image1.png \
  /absolute/path/to/image2.png
```

Notes:

- `--frame enu` applies a fixed OpenCV-to-ENU-like axis mapping.
- Additional fixed transform can be applied with `--yaw-deg`, `--pitch-deg`,
  `--roll-deg`, and `--tx/--ty/--tz`.
- Generated HTML is saved under `artifacts/reconstruction/ui/`.

## 4. Notes

- The current `feature_sfm` backend uses real images and OpenCV feature matching.
- The current `dust3r` backend is still a placeholder boundary and will return `BACKEND_NOT_IMPLEMENTED`.
- Use at least two images with overlapping scene content.
- This is a prototype pipeline, not a final-quality reconstruction pipeline.

## 5. Prototype HTTP polling flow

The current remote-execution prototype uses HTTP polling:

```text
ground client -> POST /jobs -> GET /jobs/{job_id} until complete -> GET /jobs/{job_id}/artifact
```

Start the server on the A6000 machine:

```bash
cd /path/to/cansat_2/docs
python -m reconstruction.server.http_server \
  --host 0.0.0.0 \
  --port 8765 \
  --backend feature_sfm
```

Submit a job from the ground-side/client machine:

```bash
cd /path/to/cansat_2/docs
python -m reconstruction.prototype_remote_cli \
  --endpoint http://SERVER_IP:8765 \
  --image-set-id demo-remote \
  --download-dir artifacts/reconstruction/downloads \
  --open-viewer \
  /absolute/path/on/server/image1.png \
  /absolute/path/on/server/image2.png
```

Important prototype limitation:

- Image paths are interpreted on the server side. This matches the current A6000 workflow where images already exist on the server.
- The server executes jobs synchronously internally, but the client contract is already polling-based and can support asynchronous job execution later.
- The downloaded artifact can also be visualized directly:

```bash
python -m reconstruction.prototype_ui_cli \
  --artifact artifacts/reconstruction/downloads/JOB_ID.glb \
  --frame enu \
  --open
```
