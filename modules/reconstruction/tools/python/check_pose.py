import os
import json
import glob

meta_dir = r'C:\Users\sdh97\Desktop\airsim_dataset_implement\airsim_dataset_cross\meta'

files = sorted(glob.glob(os.path.join(meta_dir, '*.json')))

print('num_files =', len(files))

for p in files[:3]:
    with open(p, 'r', encoding='utf-8') as f:
        d = json.load(f)

    print('\nFILE:', os.path.basename(p))
    print('frame_id =', d.get('frame_id'))
    print('rgb_path =', d.get('image', {}).get('rgb_path'))
    print('depth_png_path =', d.get('image', {}).get('depth_png_path'))
    print('rgb_width =', d.get('image', {}).get('rgb_width'))
    print('rgb_height =', d.get('image', {}).get('rgb_height'))
    print('position =', d.get('vehicle_pose', {}).get('position'))
    print('orientation =', d.get('vehicle_pose', {}).get('orientation'))

input('\nPress Enter to exit...')