#!/usr/bin/env python3
"""Archive project source from an explicit allowlist; excludes all user/build data."""
import argparse
import gzip
import hashlib
import io
from pathlib import Path
import tarfile

FILES = ["README.md", "CHANGELOG.md", "VERSION", "LICENSE", "THIRD_PARTY_NOTICES.md", "CMakeLists.txt", "steam-art-studio.pro", "run.sh", ".gitignore", ".containerignore"]
DIRECTORIES = ["src", "assets", "tests", "scripts", "packaging", "docs", ".github"]


def archive(root, output):
    version = (root / "VERSION").read_text().strip()
    paths = [root / name for name in FILES]
    for directory in DIRECTORIES:
        paths.extend(path for path in (root / directory).rglob("*") if path.is_file() and "__pycache__" not in path.parts and path.suffix != ".pyc" and not path.name.startswith("HISTORIAL-"))
    with output.open("xb") as raw, gzip.GzipFile(fileobj=raw, mode="wb", filename="", mtime=0) as zipped, tarfile.open(fileobj=zipped, mode="w|") as tar:
        for path in sorted(paths):
            if path.is_symlink() or not path.is_file():
                raise ValueError("Unexpected source input: " + str(path))
            relative = path.relative_to(root)
            data = path.read_bytes()
            info = tarfile.TarInfo(f"steam-art-studio-{version}/{relative}")
            info.size = len(data)
            info.mode = 0o755 if path.stat().st_mode & 0o111 else 0o644
            tar.addfile(info, io.BytesIO(data))
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_name(output.name + ".sha256").write_text(f"{digest}  {output.name}\n")
    return digest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    print(archive(Path(__file__).resolve().parents[2], args.output))
