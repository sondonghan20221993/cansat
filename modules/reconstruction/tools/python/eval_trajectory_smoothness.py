#!/usr/bin/env python3
"""
GT 없는 실제 드론 데이터용 궤도 매끄러움(smoothness) 체크.
연속 프레임 간 카메라 위치 변화량(step)을 봐서 튀는 프레임(SfM 국소 실패)을 잡아낸다.
같은 궤도를 촬영했으므로 인접 프레임 간 이동거리는 완만하게 변해야 정상.
"""
import os
import sys
import numpy as np

variant = sys.argv[1]
poses_path = os.path.expanduser(f"~/Desktop/data/{variant}/poses.npy")
poses = np.load(poses_path)
pos = poses[:, :3, 3]

steps = np.linalg.norm(pos[1:] - pos[:-1], axis=1)
# 사이클릭(원형 궤도) 가정 시 마지막->처음도 체크
loop = np.linalg.norm(pos[0] - pos[-1])

print(f"[{variant}] N={len(pos)}")
print(f"  step 평균={steps.mean():.4f}  중앙값={np.median(steps):.4f}  표준편차={steps.std():.4f}  최대={steps.max():.4f} (frame {steps.argmax()}->{steps.argmax()+1})")
print(f"  loop closure (마지막-처음) = {loop:.4f}")
# 이상치: 중앙값의 5배 넘는 급격한 step
thresh = np.median(steps) * 5
outliers = np.where(steps > thresh)[0]
print(f"  중앙값 5배 초과 급격한 step: {len(outliers)}개 -> {list(zip(outliers.tolist(), steps[outliers].round(3).tolist()))}")
