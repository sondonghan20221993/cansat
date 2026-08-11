import os
import json
import glob

meta_dir = r'C:\Users\sdh97\Desktop\airsim_dataset_implement\airsim_dataset_cross\meta'
out_path = r'C:\Users\sdh97\Desktop\airsim_dataset_implement\airsim_dataset_cross\traj.txt'

files = sorted(glob.glob(os.path.join(meta_dir, '*.json')))

with open(out_path, 'w', encoding='utf-8') as fout:
    for p in files:
        with open(p, 'r', encoding='utf-8') as f:
            d = json.load(f)

        pos = d['vehicle_pose']['position']
        ori = d['vehicle_pose']['orientation']

        tx = pos['x']
        ty = pos['y']
        tz = pos['z']

        qx = ori['x']
        qy = ori['y']
        qz = ori['z']
        qw = ori['w']

        fout.write(f"{tx} {ty} {tz} {qx} {qy} {qz} {qw}\n")

print(f"written: {out_path}")
print(f"num_frames: {len(files)}")