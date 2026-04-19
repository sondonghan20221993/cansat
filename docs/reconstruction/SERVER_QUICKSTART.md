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

## 4. Notes

- The current `feature_sfm` backend uses real images and OpenCV feature matching.
- The current `dust3r` backend is still a placeholder boundary and will return `BACKEND_NOT_IMPLEMENTED`.
- Use at least two images with overlapping scene content.
- This is a prototype pipeline, not a final-quality reconstruction pipeline.
