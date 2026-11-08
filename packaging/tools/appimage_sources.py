#!/usr/bin/env python3
"""Preserve the pinned AppImage runtime's source and upstream dependency notices."""
import argparse
import hashlib
from pathlib import Path
import shutil
import tarfile
import urllib.request

INPUTS = [
    ("type2-runtime-fedfad5.tar.gz", "https://codeload.github.com/AppImage/type2-runtime/tar.gz/fedfad5", None),
    ("fuse-3.15.0.tar.xz", "https://github.com/libfuse/libfuse/releases/download/fuse-3.15.0/fuse-3.15.0.tar.xz", "70589cfd5e1cff7ccd6ac91c86c01be340b227285c5e200baa284e401eea2ca0"),
    ("squashfuse-0.5.2.tar.gz", "https://codeload.github.com/vasi/squashfuse/tar.gz/refs/tags/0.5.2", "db0238c5981dabbd80ee09ae15387f390091668ca060a7bc38047912491443d3"),
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--notices", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    args.notices.mkdir(parents=True, exist_ok=True)
    checksums = []
    for name, url, expected in INPUTS:
        path = args.output / name
        if not path.exists():
            temporary = args.output / (name + ".partial")
            with urllib.request.urlopen(url, timeout=120) as response, temporary.open("wb") as output:
                shutil.copyfileobj(response, output)
            temporary.rename(path)
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if expected and actual != expected:
            raise ValueError("Upstream source hash mismatch: " + name)
        checksums.append(f"{actual}  {name}\n")
        with tarfile.open(path) as tar:
            for member in tar:
                relative = member.name.split("/")[1:]
                if member.isfile() and len(relative) == 1 and relative[0].lower().startswith(("license", "copying", "lgpl")):
                    (args.notices / (name + "-" + relative[0])).write_bytes(tar.extractfile(member).read())
    for name, url in [
        ("musl-COPYRIGHT", "https://git.musl-libc.org/cgit/musl/plain/COPYRIGHT?h=v1.2.5"),
        ("zstd-LICENSE", "https://raw.githubusercontent.com/facebook/zstd/v1.5.6/LICENSE"),
        ("zlib-LICENSE", "https://raw.githubusercontent.com/madler/zlib/v1.3.1/LICENSE"),
    ]:
        with urllib.request.urlopen(url, timeout=60) as response:
            (args.notices / name).write_bytes(response.read())
    (args.output / "SHA256SUMS").write_text("".join(checksums))


if __name__ == "__main__":
    main()
