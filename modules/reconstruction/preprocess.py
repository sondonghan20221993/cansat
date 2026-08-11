import open3d as o3d
import numpy as np

def preprocess_mast3r_ply(input_path, output_path):
    print(f"[*] 원본 포인트 클라우드 로드 중: {input_path}")
    pcd = o3d.io.read_point_cloud(input_path)
    
    if pcd.is_empty():
        print("[!] 에러: 포인트 클라우드를 불러오지 못했습니다. 경로를 다시 확인해 주세요.")
        return
        
    print(f"  -> 초기 점 개수: {len(pcd.points)}")

    # 1. 통계적 이상치 제거 (SOR)
    print("[*] 1단계: 통계적 이상치 제거(SOR) 진행 중...")
    cl, ind = pcd.remove_statistical_outlier(nb_neighbors=20, std_ratio=2.0)
    pcd_clean = pcd.select_by_index(ind)
    print(f"  -> SOR 적용 후 점 개수: {len(pcd_clean.points)}")

    # 2. 복셀 다운샘플링 (스펙트럴 로우패스 필터 역할)
    print("[*] 2단계: 복셀 다운샘플링 진행 중...")
    voxel_size = 0.01  # 박스 크기에 맞춰 0.01 ~ 0.1 사이로 조절 가능
    pcd_down = pcd_clean.voxel_down_sample(voxel_size=voxel_size)
    print(f"  -> 다운샘플링 후 점 개수: {len(pcd_down.points)}")

    # 3. 법선(Normal) 추정 및 방향 정렬
    print("[*] 3단계: 법선 추정 및 방향 정렬 중...")
    pcd_down.estimate_normals(
        search_param=o3d.geometry.KDTreeSearchParamHybrid(radius=voxel_size*2, max_nn=30)
    )
    pcd_down.orient_normals_consistent_tangent_plane(100)

    # 시각화 검증
    print("\n[*] 시각화 창을 띄웁니다.")
    print("    -> 팁 1: 마우스로 드래그하여 노이즈가 제거되었는지 확인하세요.")
    print("    -> 팁 2: 키보드 'N'을 누르면 법선(Normal) 벡터가 표시됩니다.")
    print("    -> 확인을 마치고 창을 닫으면 저장이 진행됩니다.")
    o3d.visualization.draw_geometries([pcd_down], window_name="Preprocessed: blue_1_fhd")

    # 결과 저장
    o3d.io.write_point_cloud(output_path, pcd_down)
    print(f"\n[*] 전처리 완료! 결과 저장됨: {output_path}")

if __name__ == "__main__":
    # Windows 경로 오류 방지를 위해 r(Raw String) 사용
    input_file = r"D:\3d_reconstruction\blue_1_fhd_mast3r.ply"
    output_file = r"D:\3d_reconstruction\blue_1_fhd_mast3r_preprocessed.ply"
    
    preprocess_mast3r_ply(input_file, output_file)