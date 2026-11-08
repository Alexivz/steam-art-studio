#!/usr/bin/env python3
"""Download build inputs only after matching the reviewed SHA256 pin."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import urllib.request


def fetch(name, directory):
    manifest = json.loads((Path(__file__).parents[1] / "linux/tools.json").read_text())
    if name not in manifest:
        raise ValueError("Unknown build input")
    record = manifest[name]
    if not record["url"].startswith("https://"):
        raise ValueError("HTTPS required")
    directory.mkdir(parents=True, exist_ok=True)
    target = directory / name
    if target.is_symlink():
        raise ValueError("Symlink build input rejected")
    if target.exists():
        if hashlib.sha256(target.read_bytes()).hexdigest() != record["sha256"]:
            raise ValueError("Existing build input does not match its pin; it was preserved")
        return target
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=directory, prefix=".download-", delete=False) as output:
            temporary = Path(output.name)
            digest = hashlib.sha256()
            size = 0
            request = urllib.request.Request(record["url"], headers={"User-Agent": "Steam-Art-Studio-build"})
            with urllib.request.urlopen(request, timeout=90) as response:
                while data := response.read(1024 * 1024):
                    size += len(data)
                    if size > 256 * 1024 * 1024:
                        raise ValueError("Build input exceeds 256 MiB")
                    digest.update(data)
                    output.write(data)
        if digest.hexdigest() != record["sha256"]:
            raise ValueError("Build input SHA256 mismatch; update the pin only after review")
        temporary.chmod(0o755 if record["executable"] else 0o644)
        # Exclusive hard-link creation prevents replacing a concurrent/unknown file.
        os.link(temporary, target)
        return target
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("name")
    parser.add_argument("--directory", type=Path, required=True)
    args = parser.parse_args()
    print(fetch(args.name, args.directory).resolve())
