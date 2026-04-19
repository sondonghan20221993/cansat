"""
reconstruction — image-based 3D reconstruction module skeleton.

Fixed policies (05-reconstruction-requirements.md):
  - Primary backend: DUSt3R-family (REC-PROC-04), replaceable (REC-PROC-06)
  - Camera pose: optional auxiliary input only (REC-IN-06 ~ REC-IN-08)
  - Compute: ground-side receiver + remote A6000 GPU server (REC-PROC-09~11)
  - Output format: GLB is primary candidate, NOT hardcoded (REC-OUT-04, OI-REC-03)
  - Message/quality structures: extensible pending interface spec (REC-IFC-01~06)
"""
