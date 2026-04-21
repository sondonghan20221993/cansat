from __future__ import annotations

from typing import Any, Dict, List, Optional

import cv2
import numpy as np

from reconstruction.backend.base import ReconstructionBackend
from reconstruction.models.job import ImageDescriptor


class FeatureSfmBackend(ReconstructionBackend):
    """Runnable sparse reconstruction backend using real images and OpenCV."""

    def __init__(self) -> None:
        self._loaded = False
        self._orb = None
        self._matcher = None

    def load(self) -> None:
        self._orb = cv2.ORB_create(3000)
        self._matcher = cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=True)
        self._loaded = True

    def unload(self) -> None:
        self._loaded = False
        self._orb = None
        self._matcher = None

    def preprocess(
        self,
        images: List[ImageDescriptor],
        aux_pose: Optional[Any] = None,
    ) -> Any:
        if not self._loaded:
            raise RuntimeError("FeatureSfmBackend must be loaded before preprocess().")
        if len(images) < 2:
            raise RuntimeError("FeatureSfmBackend requires at least 2 validated images.")

        loaded_images = []
        for descriptor in images:
            image = cv2.imread(descriptor.source_path, cv2.IMREAD_COLOR)
            if image is None:
                raise RuntimeError(f"Failed to decode image: {descriptor.source_path}")
            loaded_images.append({
                "descriptor": descriptor,
                "image": image,
                "gray": cv2.cvtColor(image, cv2.COLOR_BGR2GRAY),
            })
        return {"images": loaded_images, "aux_pose": aux_pose}

    def infer(self, preprocessed: Any) -> Any:
        frames = preprocessed["images"]
        all_points: List[List[float]] = []
        all_colors: List[tuple[int, int, int]] = []
        total_matches = 0
        total_inliers = 0
        successful_pairs = 0
        camera_positions: List[np.ndarray] = [np.zeros(3, dtype=np.float64)]
        current_rotation = np.eye(3, dtype=np.float64)
        current_translation = np.zeros(3, dtype=np.float64)

        for idx in range(len(frames) - 1):
            pair_result = self._triangulate_pair(frames[idx], frames[idx + 1])
            if pair_result is None:
                continue
            all_points.extend(pair_result["points"])
            all_colors.extend(pair_result["colors"])
            total_matches += pair_result["match_count"]
            total_inliers += pair_result["inlier_count"]
            successful_pairs += 1

            rel_rotation = pair_result["rotation"]
            rel_translation = pair_result["translation"]
            current_translation = current_translation + current_rotation @ rel_translation
            current_rotation = rel_rotation @ current_rotation
            camera_positions.append(current_translation.copy())

        if not all_points:
            raise RuntimeError("No valid 3D points could be reconstructed from the provided image sequence.")

        camera_traj = []
        for idx, descriptor in enumerate(frames):
            if idx < len(camera_positions):
                pos = camera_positions[idx]
            else:
                pos = camera_positions[-1]
            camera_traj.append({
                "image_id": descriptor["descriptor"].image_id,
                "source_path": descriptor["descriptor"].source_path,
                "position": [float(pos[0]), float(pos[1]), float(pos[2])],
            })

        return {
            "points": all_points,
            "colors": all_colors,
            "images_used": len(frames),
            "successful_pairs": successful_pairs,
            "match_count": total_matches,
            "inlier_count": total_inliers,
            "camera_trajectory": camera_traj,
        }

    def _triangulate_pair(self, frame_a: Dict[str, Any], frame_b: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        kp_a, des_a = self._orb.detectAndCompute(frame_a["gray"], None)
        kp_b, des_b = self._orb.detectAndCompute(frame_b["gray"], None)
        if des_a is None or des_b is None:
            return None

        matches = self._matcher.match(des_a, des_b)
        if len(matches) < 12:
            return None

        matches = sorted(matches, key=lambda m: m.distance)[:800]
        pts_a = np.float32([kp_a[m.queryIdx].pt for m in matches])
        pts_b = np.float32([kp_b[m.trainIdx].pt for m in matches])

        h, w = frame_a["gray"].shape[:2]
        focal = float(max(w, h))
        camera_matrix = np.array(
            [[focal, 0.0, w / 2.0], [0.0, focal, h / 2.0], [0.0, 0.0, 1.0]],
            dtype=np.float64,
        )

        essential, _ = cv2.findEssentialMat(
            pts_a, pts_b, camera_matrix, method=cv2.RANSAC, prob=0.999, threshold=1.0
        )
        if essential is None:
            return None

        _, rotation, translation, pose_mask = cv2.recoverPose(essential, pts_a, pts_b, camera_matrix)
        if pose_mask is None:
            return None

        inliers = pose_mask.ravel().astype(bool)
        pts_a = pts_a[inliers]
        pts_b = pts_b[inliers]
        if len(pts_a) < 8:
            return None

        proj_a = camera_matrix @ np.hstack((np.eye(3), np.zeros((3, 1))))
        proj_b = camera_matrix @ np.hstack((rotation, translation))
        points_h = cv2.triangulatePoints(proj_a, proj_b, pts_a.T, pts_b.T)
        points_3d = (points_h[:3] / points_h[3]).T

        finite_mask = np.isfinite(points_3d).all(axis=1)
        positive_mask = points_3d[:, 2] > 0
        valid_mask = finite_mask & positive_mask
        points_3d = points_3d[valid_mask]
        pts_a = pts_a[valid_mask]
        if len(points_3d) == 0:
            return None

        colors = []
        image = frame_a["image"]
        for x, y in pts_a:
            px = int(np.clip(round(float(x)), 0, image.shape[1] - 1))
            py = int(np.clip(round(float(y)), 0, image.shape[0] - 1))
            bgr = image[py, px]
            colors.append((int(bgr[2]), int(bgr[1]), int(bgr[0])))

        return {
            "points": points_3d.tolist(),
            "colors": colors,
            "match_count": len(matches),
            "inlier_count": int(inliers.sum()),
            "rotation": rotation,
            "translation": translation.reshape(3),
        }

    def postprocess(
        self,
        raw_result: Any,
        output_format: str,
        job_id: str,
        image_set_id: Any,
    ) -> Dict[str, Any]:
        return {
            "normalized_scene": {
                "points": raw_result["points"],
                "colors": raw_result["colors"],
            },
            "output_format": output_format,
            "job_id": job_id,
            "image_set_id": image_set_id,
            "quality_indicators": {
                "backend": self.backend_name,
                "images_used": raw_result["images_used"],
                "successful_pairs": raw_result["successful_pairs"],
                "match_count": raw_result["match_count"],
                "inlier_count": raw_result["inlier_count"],
                "point_count": len(raw_result["points"]),
            },
            "camera_trajectory": raw_result.get("camera_trajectory", []),
        }

    @property
    def backend_name(self) -> str:
        return "feature_sfm"
