"""Portable packaging checks; never install into a real profile."""
import hashlib
import importlib.util
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def load(name, relative):
    spec = importlib.util.spec_from_file_location(name, ROOT / relative)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


windows = load("windows_package", "packaging/windows/package.py")
source = load("source_package", "packaging/tools/archive_source.py")
flatpak = load("flatpak_package", "packaging/flatpak/bundle.py")


class PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="studio-packaging-")
        self.root = Path(self.temp.name)
        self.stage = self.root / "Stage with spaces"
        for name in ["steam-art-studio.exe", "platforms/qwindows.dll", "imageformats/qwebp.dll", "docs/LICENSE"]:
            path = self.stage / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"synthetic-not-an-executable")

    def tearDown(self):
        self.temp.cleanup()

    def test_incomplete_windows_stage_is_rejected(self):
        (self.stage / "platforms/qwindows.dll").unlink()
        with self.assertRaises(ValueError):
            windows.package(self.stage, self.root / "dist", "0.0.0-test")

    def test_windows_zip_and_checksum(self):
        output = self.root / "dist"
        windows.package(self.stage, output, "0.0.0-test")
        archive = next(output.glob("*.zip"))
        with zipfile.ZipFile(archive) as zipped:
            self.assertEqual(len(zipped.namelist()), 4)
            self.assertTrue(all(not name.startswith("/") and ".." not in name.split("/") for name in zipped.namelist()))
        self.assertIn(hashlib.sha256(archive.read_bytes()).hexdigest(), archive.with_suffix(".zip.sha256").read_text())
        with self.assertRaises(FileExistsError):
            windows.package(self.stage, output, "0.0.0-test")

    def test_installer_is_per_user_and_never_removes_user_data(self):
        output = self.root / "dist"
        windows.package(self.stage, output, "0.0.0-test")
        text = next(output.glob("*.nsi")).read_text()
        self.assertIn("RequestExecutionLevel user", text)
        self.assertIn("SetShellVarContext current", text)
        self.assertIn("Call .onVerifyInstDir", text)
        self.assertNotIn("HKLM", text)
        self.assertNotIn("RMDir /r", text)
        self.assertNotIn("SteamArtStudio\\steam-art-studio", text)
        self.assertIn("GetFileAttributesW", text)

    def test_nsis_paths_escape_dollars_and_quotes(self):
        self.assertEqual(windows.nsis_quote('C:/User $name/"test"'), 'C:/User $$name/$\\"test$\\"')

    def test_source_is_reproducible_and_has_no_builds_or_user_data(self):
        first, second = self.root / "first.tar.gz", self.root / "second.tar.gz"
        self.assertEqual(source.archive(ROOT, first), source.archive(ROOT, second))
        with tarfile.open(first) as tar:
            names = ["/".join(name.split("/")[1:]) for name in tar.getnames()]
        self.assertIn("LICENSE", names)
        self.assertIn("src/main.cpp", names)
        self.assertIn("packaging/windows/package.py", names)
        self.assertFalse(any(name.startswith(("build", "dist/")) or "__pycache__" in name for name in names))
        self.assertNotIn("CONTINUAR.md", names)
        self.assertNotIn("AGENTS.md", names)
        self.assertFalse(any(Path(name).name.startswith("HISTORIAL-") for name in names))
        with self.assertRaises(FileExistsError):
            source.archive(ROOT, first)

    def test_flatpak_has_only_specific_steam_permissions(self):
        appdir = self.root / "AppDir"
        binary = appdir / "usr/bin/steam-art-studio"
        binary.parent.mkdir(parents=True)
        binary.write_bytes(b"synthetic")
        output = self.root / "test.flatpak"
        with patch.object(flatpak.subprocess, "run") as run:
            def fake_run(*args, **kwargs):
                output.write_bytes(b"synthetic-flatpak")
            run.side_effect = fake_run
            flatpak.bundle(appdir, self.root / "flatpak", self.root / "repo", output)
        metadata = (self.root / "flatpak/metadata").read_text()
        self.assertIn("~/.local/share/Steam", metadata)
        self.assertIn("~/.var/app/com.valvesoftware.Steam/data/Steam", metadata)
        self.assertNotIn("filesystems=home", metadata)
        self.assertNotIn("filesystems=host", metadata)
        self.assertNotIn("flatpak-spawn", metadata)


if __name__ == "__main__":
    unittest.main()
