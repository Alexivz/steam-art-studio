import hashlib
import importlib.util
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("studio_install", ROOT / "scripts/install.py")
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class DistributionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="studio-install-", dir="/tmp/opencode")
        self.root = Path(self.temp.name)
        # Include spaces and '%' to exercise desktop Exec escaping.
        self.home = self.root / "User Home % test"
        self.home.mkdir()
        runtime = self.root / "runtime"
        runtime.mkdir(mode=0o700)
        self.environment = patch.dict(os.environ, {"HOME": str(self.home), "XDG_DATA_HOME": str(self.home / "share"), "XDG_CACHE_HOME": str(self.home / "cache"), "XDG_CONFIG_HOME": str(self.home / "config"), "XDG_RUNTIME_DIR": str(runtime)})
        self.environment.start()

    def tearDown(self):
        self.environment.stop()
        self.temp.cleanup()

    def test_install_launch_and_uninstall_preserve_user_data(self):
        user_data = self.home / "share/SteamArtStudio/steam-art-studio/backups/keep.bin"
        user_data.parent.mkdir(parents=True)
        user_data.write_bytes(b"synthetic-user-backup")
        installer.install(ROOT)
        app, paths = installer.locations()
        self.assertTrue(paths["desktop"].is_file())
        desktop = paths["desktop"].read_text()
        self.assertIn("%%", desktop)
        result = subprocess.run([str(paths["launcher"]), "--version"], env={**os.environ, "QT_QPA_PLATFORM": "offscreen"}, capture_output=True, text=True, check=True)
        self.assertIn((ROOT / "VERSION").read_text().strip(), result.stdout)
        installer.uninstall()
        self.assertFalse(paths["binary"].exists())
        self.assertEqual(user_data.read_bytes(), b"synthetic-user-backup")

    def test_unknown_launcher_is_not_overwritten(self):
        app, paths = installer.locations()
        paths["launcher"].parent.mkdir(parents=True)
        paths["launcher"].write_bytes(b"user-work")
        with self.assertRaises(RuntimeError):
            installer.install(ROOT)
        self.assertEqual(paths["launcher"].read_bytes(), b"user-work")
        self.assertFalse(app.exists())

    def test_modified_file_is_preserved_on_uninstall(self):
        installer.install(ROOT)
        app, paths = installer.locations()
        paths["desktop"].write_bytes(b"custom-user-launcher")
        with self.assertRaises(RuntimeError):
            installer.uninstall()
        self.assertEqual(paths["desktop"].read_bytes(), b"custom-user-launcher")
        self.assertTrue((app / ".studio-install.json").exists())
        self.assertTrue(paths["binary"].exists())
        self.assertTrue(paths["uninstall_launcher"].exists())

    def test_invalid_installation_record_is_rejected_without_changes(self):
        installer.install(ROOT)
        app, paths = installer.locations()
        original = paths["binary"].read_bytes()
        for malformed in [b"[]", b'{"format":1,"files":[]}']:
            (app / ".studio-install.json").write_bytes(malformed)
            with self.assertRaises(RuntimeError):
                installer.uninstall()
            self.assertEqual(paths["binary"].read_bytes(), original)

    def test_failed_upgrade_preserves_previous_installation(self):
        installer.install(ROOT)
        app, paths = installer.locations()
        before = {key: path.read_bytes() for key, path in paths.items()}
        marker = (app / ".studio-install.json").read_bytes()
        original = installer.atomic_write
        def failing(path, data, mode=0o644):
            if path == app / ".studio-install.json":
                raise OSError("Synthetic final journal failure")
            return original(path, data, mode)
        with patch.object(installer, "atomic_write", failing):
            with self.assertRaises(OSError):
                installer.install(ROOT)
        self.assertEqual((app / ".studio-install.json").read_bytes(), marker)
        for key, path in paths.items():
            self.assertEqual(path.read_bytes(), before[key])

    def test_write_failure_rolls_back_program_files(self):
        original = installer.atomic_write
        app, paths = installer.locations()
        def failing(path, data, mode=0o644):
            if path == paths["launcher"]:
                raise OSError("Synthetic disk failure")
            return original(path, data, mode)
        with patch.object(installer, "atomic_write", failing):
            with self.assertRaises(OSError):
                installer.install(ROOT)
        self.assertFalse(paths["binary"].exists())
        self.assertFalse((app / ".studio-install.json").exists())

    def test_symlink_destination_is_rejected(self):
        _, paths = installer.locations()
        outside = self.root / "outside"
        outside.write_bytes(b"do-not-change")
        paths["launcher"].parent.mkdir(parents=True)
        paths["launcher"].symlink_to(outside)
        with self.assertRaises(RuntimeError):
            installer.install(ROOT)
        self.assertEqual(outside.read_bytes(), b"do-not-change")

    def test_archive_is_reproducible_and_excludes_private_state(self):
        outputs = [self.root / "one.tar.gz", self.root / "two.tar.gz"]
        for output in outputs:
            subprocess.run(["python3", str(ROOT / "scripts/package.py"), "--output", str(output)], check=True, capture_output=True)
        self.assertEqual(hashlib.sha256(outputs[0].read_bytes()).digest(), hashlib.sha256(outputs[1].read_bytes()).digest())
        with tarfile.open(outputs[0]) as archive:
            names = archive.getnames()
            self.assertFalse(any(".." in Path(name).parts for name in names))
            self.assertFalse(any("draft" in name or "settings.ini" in name or "/backups/" in name or "/cache/" in name for name in names))
            archive.extractall(self.root / "extracted", filter="data")
        bundle = next((self.root / "extracted").iterdir())
        result = subprocess.run([str(bundle / "run.sh"), "--version"], env={**os.environ, "QT_QPA_PLATFORM": "offscreen"}, capture_output=True, text=True, check=True)
        self.assertIn((ROOT / "VERSION").read_text().strip(), result.stdout)
        for line in (bundle / "SHA256SUMS").read_text().splitlines():
            checksum, name = line.split("  ", 1)
            self.assertEqual(hashlib.sha256((bundle / name).read_bytes()).hexdigest(), checksum)
        refused = subprocess.run([str(bundle / "install.sh")], input="SI\n", capture_output=True, text=True, timeout=15)
        self.assertEqual(refused.returncode, 2)  # Noninteractive use requires explicit --yes.
        app, paths = installer.locations()
        self.assertFalse(app.exists())
        subprocess.run([str(bundle / "install.sh"), "--yes"], check=True, capture_output=True, timeout=15)
        screenshot = self.root / "installed-demo.png"
        subprocess.run([str(paths["launcher"]), "--demo", "--smoke-test", "--data-dir", str(self.root / "demo-data"), "--cache-dir", str(self.root / "demo-cache"), "--screenshot", str(screenshot)], env={**os.environ, "QT_QPA_PLATFORM": "offscreen"}, check=True, capture_output=True, timeout=20)
        self.assertTrue(screenshot.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"))
        subprocess.run([str(paths["uninstall_launcher"]), "--yes"], check=True, capture_output=True, timeout=15)
        self.assertFalse(paths["binary"].exists())
        self.assertTrue((self.root / "demo-data/.steam-art-studio-data").exists())


if __name__ == "__main__":
    unittest.main()
