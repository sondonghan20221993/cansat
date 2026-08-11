Prototype: Next Viewpoint Recommendation

Files:
- next_viewpoint_prototype.py

Default input folder:
- C:\Users\sdh97\Desktop\airsim_degree_notilt_more_distance

Required input files inside the folder:
- rgb.ply
- rgb_poses.json

Run:
python C:\Users\sdh97\Desktop\mast3r_viewpoint_prototype\next_viewpoint_prototype.py --open

Optional:
--base-dir "C:\path\to\dataset"
--candidate-count 16
--radius-scale 1.15
--point-size 0.8

Output:
- next_viewpoint_prototype.html

What it shows:
- existing camera trajectory
- candidate camera positions placed around the point cloud center
- recommended next viewpoint
- viewing direction from the recommended viewpoint to the point cloud center
