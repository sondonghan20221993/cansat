import os
import json
import glob
import struct
import numpy as np

PLY_DIR   = "/mnt/d/epic/CitySample/airsim_dataset_test/ai_depth_ply"
META_DIR  = "/mnt/d/epic/CitySample/airsim_dataset_test/meta"
OUT_PATH  = "/mnt/d/epic/CitySample/airsim_dataset_test/merged_test_2.ply"


def quat_to_rotmat(qx, qy, qz, qw):
    n = np.sqrt(qx*qx + qy*qy + qz*qz + qw*qw)
    if n == 0:
        return np.eye(3, dtype=np.float64)

    qx, qy, qz, qw = qx/n, qy/n, qz/n, qw/n

    R = np.array([
        [1 - 2*(qy*qy + qz*qz),     2*(qx*qy - qz*qw),     2*(qx*qz + qy*qw)],
        [    2*(qx*qy + qz*qw), 1 - 2*(qx*qx + qz*qz),     2*(qy*qz - qx*qw)],
        [    2*(qx*qz - qy*qw),     2*(qy*qz + qx*qw), 1 - 2*(qx*qx + qy*qy)]
    ], dtype=np.float64)
    return R


def read_pose(meta_path):
    with open(meta_path, "r", encoding="utf-8") as f:
        meta = json.load(f)

    pos = meta["camera_position"]
    ori = meta["camera_orientation"]

    tx = float(pos["x"])
    ty = float(pos["y"])
    tz = float(pos["z"])

    qw = float(ori["w"])
    qx = float(ori["x"])
    qy = float(ori["y"])
    qz = float(ori["z"])

    R = quat_to_rotmat(qx, qy, qz, qw)
    t = np.array([tx, ty, tz], dtype=np.float64)
    return R, t


def parse_ply_header(f):
    fmt = None
    vertex_count = None
    vertex_props = []
    in_vertex_element = False

    while True:
        line = f.readline()
        if not line:
            raise RuntimeError("Invalid PLY: header not found")

        line = line.decode("ascii").strip()

        if line == "ply":
            continue

        if line.startswith("format "):
            fmt = line.split()[1]

        elif line.startswith("element "):
            parts = line.split()
            elem_name = parts[1]
            elem_count = int(parts[2])

            if elem_name == "vertex":
                vertex_count = elem_count
                in_vertex_element = True
            else:
                in_vertex_element = False

        elif line.startswith("property ") and in_vertex_element:
            parts = line.split()
            if len(parts) == 3:
                prop_type = parts[1]
                prop_name = parts[2]
                vertex_props.append((prop_type, prop_name))
            else:
                raise RuntimeError("List property in vertex is not supported")

        elif line == "end_header":
            break

    if fmt is None or vertex_count is None:
        raise RuntimeError("Invalid PLY header")

    return fmt, vertex_count, vertex_props


def ply_type_to_struct_fmt(t):
    table = {
        "char": "b",
        "uchar": "B",
        "int8": "b",
        "uint8": "B",
        "short": "h",
        "ushort": "H",
        "int16": "h",
        "uint16": "H",
        "int": "i",
        "uint": "I",
        "int32": "i",
        "uint32": "I",
        "float": "f",
        "float32": "f",
        "double": "d",
        "float64": "d",
    }
    if t not in table:
        raise RuntimeError(f"Unsupported PLY property type: {t}")
    return table[t]


def read_ply_xyz(path):
    with open(path, "rb") as f:
        fmt, vertex_count, vertex_props = parse_ply_header(f)

        prop_names = [name for _, name in vertex_props]
        if "x" not in prop_names or "y" not in prop_names or "z" not in prop_names:
            raise RuntimeError(f"x/y/z property not found in {path}")

        x_idx = prop_names.index("x")
        y_idx = prop_names.index("y")
        z_idx = prop_names.index("z")

        if fmt == "ascii":
            pts = np.zeros((vertex_count, 3), dtype=np.float64)
            for i in range(vertex_count):
                line = f.readline().decode("ascii").strip()
                vals = line.split()
                pts[i, 0] = float(vals[x_idx])
                pts[i, 1] = float(vals[y_idx])
                pts[i, 2] = float(vals[z_idx])
            return pts

        elif fmt == "binary_little_endian":
            struct_fmt = "<" + "".join(ply_type_to_struct_fmt(t) for t, _ in vertex_props)
            row_size = struct.calcsize(struct_fmt)

            pts = np.zeros((vertex_count, 3), dtype=np.float64)

            for i in range(vertex_count):
                row = f.read(row_size)
                if len(row) != row_size:
                    raise RuntimeError(f"Unexpected EOF in {path}")
                vals = struct.unpack(struct_fmt, row)
                pts[i, 0] = float(vals[x_idx])
                pts[i, 1] = float(vals[y_idx])
                pts[i, 2] = float(vals[z_idx])

            return pts

        else:
            raise RuntimeError(f"Unsupported PLY format: {fmt}")


def convert_cam_xyz_to_airsim_xyz(pts):
    out = np.empty_like(pts, dtype=np.float64)
    out[:, 0] = pts[:, 2]    # X_air = Z
    out[:, 1] = pts[:, 0]    # Y_air = X
    out[:, 2] = -pts[:, 1]   # Z_air = -Y
    return out


def transform_points(pts_local, R, t):
    return (R @ pts_local.T).T + t


def save_ply_binary(path, pts):
    pts = np.asarray(pts, dtype=np.float32)

    header = "\n".join([
        "ply",
        "format binary_little_endian 1.0",
        f"element vertex {len(pts)}",
        "property float x",
        "property float y",
        "property float z",
        "end_header\n"
    ])

    with open(path, "wb") as f:
        f.write(header.encode("ascii"))
        pts.tofile(f)


ply_files = sorted(
    glob.glob(os.path.join(PLY_DIR, "*.ply")),
    key=lambda p: int(os.path.splitext(os.path.basename(p))[0])
)[:4]

all_points = []
print("ply count:", len(ply_files))

for ply_path in ply_files:
    stem = os.path.splitext(os.path.basename(ply_path))[0]

    # 0.ply -> 000000.json
    stem_padded = f"{int(stem):06d}"
    meta_path = os.path.join(META_DIR, stem_padded + ".json")

    if not os.path.exists(meta_path):
        print(f"skip: meta not found for {stem} -> {meta_path}")
        continue

    pts_local = read_ply_xyz(ply_path)

    if len(pts_local) == 0:
        print(f"skip: empty ply {ply_path}")
        continue

    R, t = read_pose(meta_path)

    pts_cam_fixed = np.zeros_like(pts_local)
    pts_cam_fixed[:, 0] = pts_local[:, 2]
    pts_cam_fixed[:, 1] = pts_local[:, 0]
    pts_cam_fixed[:, 2] = -pts_local[:, 1]

    pts_world = (R.T @ pts_cam_fixed.T).T + t
    #pts_world = transform_points(pts_local_fixed, R, t)
    pts_world = (R.T @ pts_cam_fixed.T).T + t
    center = pts_world.mean(axis=0)
    mins = pts_world.min(axis=0)
    maxs = pts_world.max(axis=0)

    print(f"{stem}")
    print("  center =", center)
    print("  min    =", mins)
    print("  max    =", maxs)

    all_points.append(pts_world)
    print(f"{stem}: local_points={len(pts_local)}, world_points={len(pts_world)}")

if len(all_points) == 0:
    raise RuntimeError("No valid points collected.")

merged_points = np.vstack(all_points)
print("merged shape:", merged_points.shape)

save_ply_binary(OUT_PATH, merged_points)
print("saved:", OUT_PATH)