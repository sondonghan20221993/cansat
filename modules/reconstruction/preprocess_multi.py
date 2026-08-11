import open3d as o3d
import numpy as np
import sys

def preprocess_mast3r_ply(input_path, output_path, voxel_size, strength_name):
    """
    MASt3R 포인트 클라우드 전처리

    Args:
        input_path: 입력 PLY 경로
        output_path: 출력 PLY 경로
        voxel_size: 복셀 크기 (낮을수록 더 압축)
        strength_name: 강도 이름 (로그용)
    """
    print(f"\n{'='*60}")
    print(f"[*] 강도: {strength_name}")
    print(f"[*] Voxel Size: {voxel_size}")
    print(f"{'='*60}")

    print(f"[*] 원본 포인트 클라우드 로드 중: {input_path}")
    pcd = o3d.io.read_point_cloud(input_path)

    if pcd.is_empty():
        print("[!] 에러: 포인트 클라우드를 불러오지 못했습니다.")
        return False

    print(f"  -> 초기 점 개수: {len(pcd.points):,}")

    # 1. 통계적 이상치 제거 (SOR)
    print("[*] 1단계: 통계적 이상치 제거(SOR) 진행 중...")
    cl, ind = pcd.remove_statistical_outlier(nb_neighbors=20, std_ratio=2.0)
    pcd_clean = pcd.select_by_index(ind)
    print(f"  -> SOR 적용 후 점 개수: {len(pcd_clean.points):,}")

    # 2. 복셀 다운샘플링
    print("[*] 2단계: 복셀 다운샘플링 진행 중...")
    pcd_down = pcd_clean.voxel_down_sample(voxel_size=voxel_size)
    print(f"  -> 다운샘플링 후 점 개수: {len(pcd_down.points):,}")

    # 3. 법선 추정 및 방향 정렬
    print("[*] 3단계: 법선 추정 및 방향 정렬 중...")
    pcd_down.estimate_normals(
        search_param=o3d.geometry.KDTreeSearchParamHybrid(radius=voxel_size*2, max_nn=30)
    )
    pcd_down.orient_normals_consistent_tangent_plane(100)
    print(f"  -> 법선 추정 완료")

    # 결과 저장
    o3d.io.write_point_cloud(output_path, pcd_down)
    print(f"[✓] 저장 완료: {output_path}")
    return True

if __name__ == "__main__":
    # 서버 경로 (자동 감지)
    import os
    if os.path.exists("/home/sdh/Desktop"):
        # 서버 환경
        base_dir = "/home/sdh/Desktop/data/experiments"
        input_file = f"{base_dir}/blue_1__mast3r_fhd/01_sfm/pointcloud.ply"
        output_base = f"{base_dir}/blue_1__mast3r_fhd_preprocessed"
    else:
        # Windows 로컬 환경
        input_file = r"D:\3d_reconstruction\blue_1_fhd_mast3r.ply"
        output_base = r"D:\3d_reconstruction\blue_1_fhd_mast3r_preprocessed"

    # 4가지 강도 설정
    strengths = [
        (0.005, "강도1_엄청빡셈_vs0005"),
        (0.003, "강도2_엄청빡셈_vs0003"),
        (0.01, "강도1-1_일부압축_vs0010"),
        (0.008, "강도1-2_일부압축_vs0008"),
    ]

    print(f"\n{'#'*60}")
    print(f"# MASt3R 포인트 클라우드 4단계 전처리")
    print(f"{'#'*60}")
    print(f"입력 파일: {input_file}")
    print(f"출력 기본 경로: {output_base}")

    if not os.path.exists(input_file):
        print(f"[!] 에러: 입력 파일이 없습니다: {input_file}")
        sys.exit(1)

    success_count = 0
    for voxel_size, strength_name in strengths:
        output_file = f"{output_base}_{strength_name}.ply"
        try:
            if preprocess_mast3r_ply(input_file, output_file, voxel_size, strength_name):
                success_count += 1
        except Exception as e:
            print(f"[!] 에러 발생: {e}")

    print(f"\n{'='*60}")
    print(f"[✓] 전처리 완료: {success_count}/4 파일 생성됨")
    print(f"{'='*60}\n")
