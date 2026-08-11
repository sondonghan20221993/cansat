import trimesh
import numpy as np
import os
import time

# ==========================================
# 1. 설정 (사용자 환경에 맞게 수정하세요)
# ==========================================
# 입력 파일: 이전에 합쳤던 대용량 PLY 파일 경로
INPUT_PATH = r"/mnt/d/epic/CitySample/airsim_dataset/merge/ai_depth_ply/merged_world_map_final.ply"

# 출력 파일: 다운샘플링된 결과 파일 경로
OUTPUT_PATH = r"/mnt/d/epic/CitySample/airsim_dataset/merge/ai_depth_ply/merged_world_map_downsampled.ply"

# 보셀 크기 (단위: 미터)
# 0.05 = 5cm 격자당 점 1개 (추천)
# 0.1  = 10cm 격자당 점 1개 (더 가벼워짐)
VOXEL_SIZE = 0.05 

# ==========================================
# 2. 다운샘플링 함수
# ==========================================
def voxel_downsample(points, voxel_size):
    """
    Numpy를 이용한 고속 Grid Downsampling.
    공간을 격자로 나누고 각 격자 내의 점들 중 하나만 남깁니다.
    """
    if voxel_size <= 0:
        return points
    
    # 각 점의 좌표를 보셀 크기로 나누고 내림하여 격자 인덱스 생성
    indices = np.floor(points / voxel_size).astype(np.int32)
    
    # 중복된 격자 인덱스를 찾아 고유한 인덱스만 남김 (고속 처리)
    _, unique_indices = np.unique(indices, axis=0, return_index=True)
    
    return points[unique_indices]

# ==========================================
# 3. 메인 실행부
# ==========================================
def main():
    if not os.path.exists(INPUT_PATH):
        print(f"오류: 파일을 찾을 수 없습니다: {INPUT_PATH}")
        return

    print(f"--- 다운샘플링 작업 시작 ---")
    start_time = time.time()

    # 1. PLY 파일 로드
    print(f"1. 파일 로딩 중: {os.path.basename(INPUT_PATH)}")
    pcd = trimesh.load(INPUT_PATH)
    
    # 점 데이터 추출
    if hasattr(pcd, 'vertices'):
        points = np.asarray(pcd.vertices, dtype=np.float64)
    else:
        points = np.asarray(pcd, dtype=np.float64)
    
    initial_count = len(points)
    print(f"   - 원본 포인트 수: {initial_count:,}개")

    # 2. 다운샘플링 수행
    print(f"2. 보셀 다운샘플링 진행 중 (Voxel Size: {VOXEL_SIZE}m)...")
    downsampled_points = voxel_downsample(points, VOXEL_SIZE)
    
    final_count = len(downsampled_points)
    reduction = (1 - final_count / initial_count) * 100

    # 3. 결과 저장
    print(f"3. 결과 파일 저장 중...")
    new_pcd = trimesh.points.PointCloud(downsampled_points)
    new_pcd.export(OUTPUT_PATH)

    end_time = time.time()
    
    print("=" * 40)
    print(f"완료!")
    print(f" - 처리 시간: {end_time - start_time:.2f}초")
    print(f" - 최종 포인트 수: {final_count:,}개")
    print(f" - 데이터 감소율: {reduction:.1f}%")
    print(f" - 저장 경로: {OUTPUT_PATH}")
    print("=" * 40)

if __name__ == "__main__":
    main()