"""camera/ 프로토타입 정적 검증 테스트

하드웨어 없이 검증 가능한 범위:
 - 셸 스크립트 문법 (bash -n)
 - majestic 설정 조각의 키/값 정합 (spec 문서와 일치)
 - ArduPilot param 파일 형식 (NAME,VALUE)
 - 파일 간 일관성 (기본 IP, msposd 인자, README 참조 무결성)
실기기 검증은 camera/verify_camera.sh 담당.
"""

import pathlib
import re
import subprocess
import unittest

CAMERA_DIR = pathlib.Path(__file__).resolve().parent.parent / "camera"


class ShellSyntaxTest(unittest.TestCase):
    def _check(self, name):
        result = subprocess.run(
            ["bash", "-n", str(CAMERA_DIR / name)],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, msg=result.stderr)

    def test_apply_script_syntax(self):
        self._check("apply_camera_config.sh")

    def test_verify_script_syntax(self):
        self._check("verify_camera.sh")

    def test_msposd_script_syntax(self):
        # 카메라 탑재용은 busybox sh — sh 문법으로 검사
        result = subprocess.run(
            ["sh", "-n", str(CAMERA_DIR / "msposd_air.sh")],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, msg=result.stderr)


class MajesticFragmentTest(unittest.TestCase):
    def setUp(self):
        self.text = (CAMERA_DIR / "majestic_fragment.yaml").read_text()

    def test_osd_enabled_with_timestamp_template(self):
        self.assertIn("enabled: true", self.text)
        self.assertIn('template: "%d.%m.%Y %H:%M:%S"', self.text)

    def test_records_split_and_quota(self):
        self.assertRegex(self.text, r"split:\s*60")
        self.assertRegex(self.text, r"maxUsage:\s*90")

    def test_apply_script_covers_fragment_keys(self):
        """yaml 조각에 있는 설정이 apply 스크립트 cli 명령에도 존재해야 함 (이중 관리 drift 방지)."""
        apply_text = (CAMERA_DIR / "apply_camera_config.sh").read_text()
        for key in (".osd.enabled", ".osd.template", ".records.enabled",
                    ".records.split", ".records.maxUsage", ".rtsp.enabled"):
            self.assertIn(key, apply_text, msg=f"apply 스크립트에 {key} 누락")


class ArdupilotParamTest(unittest.TestCase):
    def setUp(self):
        self.lines = [
            line.strip() for line
            in (CAMERA_DIR / "ardupilot_msp_osd.param").read_text().splitlines()
        ]

    def _params(self):
        out = {}
        for line in self.lines:
            if not line or line.startswith("#"):
                continue
            body = line.split("#")[0].strip()
            name, value = body.split(",", 1)
            out[name.strip()] = value.strip()
        return out

    def test_param_line_format(self):
        for line in self.lines:
            if not line or line.startswith("#"):
                continue
            body = line.split("#")[0].strip()
            self.assertRegex(body, r"^[A-Z0-9_]+,[-0-9.]+$", msg=f"형식 위반: {line}")

    def test_msp_displayport_pair(self):
        """MSP DisplayPort는 SERIALx_PROTOCOL=42 + OSD_TYPE=5 세트여야 함."""
        params = self._params()
        self.assertEqual(params.get("OSD_TYPE"), "5")
        protocols = [v for k, v in params.items() if re.match(r"SERIAL\d+_PROTOCOL$", k)]
        self.assertIn("42", protocols)


class ConsistencyTest(unittest.TestCase):
    def test_default_ip_consistent(self):
        """OpenIPC 기본 IP(192.168.1.10)가 스크립트/README에 일관되게 사용."""
        for name in ("apply_camera_config.sh", "verify_camera.sh", "README.md"):
            self.assertIn("192.168.1.10", (CAMERA_DIR / name).read_text(), msg=name)

    def test_msposd_airside_burnin_mode(self):
        """번인 모드 필수 인자(--osd)와 FC UART 지정(--master) 존재."""
        text = (CAMERA_DIR / "msposd_air.sh").read_text()
        self.assertIn("--osd", text)
        self.assertIn("--master", text)

    def test_readme_references_exist(self):
        """README 파일 표에 나열된 파일이 전부 실재해야 함."""
        readme = (CAMERA_DIR / "README.md").read_text()
        # 파일 구성 표의 행(| `파일명` | ...)만 검사 — 본문에 언급되는 외부 경로(wfb.yaml 등) 제외
        for name in re.findall(r"^\| `([a-z_]+\.(?:sh|yaml|param|conf))`", readme, re.MULTILINE):
            self.assertTrue((CAMERA_DIR / name).exists(), msg=f"README 참조 파일 없음: {name}")

    def test_bench_todos_tracked(self):
        """실기기 확인 항목은 TODO(bench) 마커로 추적 — 최소 1개 이상 존재해야
        프로토타입 상태임이 문서화된 것."""
        count = 0
        for path in CAMERA_DIR.iterdir():
            count += path.read_text().count("TODO(bench)")
        self.assertGreaterEqual(count, 5)

    def test_chrony_allows_camera_subnet(self):
        text = (CAMERA_DIR / "pi_chrony_camera.conf").read_text()
        self.assertIn("allow 192.168.1.0/24", text)
        self.assertIn("local stratum", text)


class RowEpochMsTest(unittest.TestCase):
    """fc_serial_ws_server.py 실제 CSV 포맷(정수 epoch ms) 파싱 회귀 테스트."""

    def setUp(self):
        import sys
        sys.path.insert(0, str(CAMERA_DIR))
        global row_epoch_ms
        from correlate_video_telemetry import row_epoch_ms

    def test_integer_epoch_ms(self):
        self.assertEqual(row_epoch_ms("1784034321805"), 1784034321805)

    def test_iso_string_fallback(self):
        from datetime import datetime
        expected = int(datetime.fromisoformat("2026-07-16T03:00:00").timestamp() * 1000)
        self.assertEqual(row_epoch_ms("2026-07-16T03:00:00"), expected)

    def test_invalid_raises(self):
        with self.assertRaises(ValueError):
            row_epoch_ms("not-a-timestamp")


if __name__ == "__main__":
    unittest.main()
