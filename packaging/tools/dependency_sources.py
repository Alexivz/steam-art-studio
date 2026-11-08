#!/usr/bin/env python3
"""Fetch matching distro source packages into a separate publication companion."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import urllib.request
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fedora", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    inventory = json.loads(args.inventory.read_text())
    if args.fedora:
        for filename in sorted({record["source_rpm"] for record in inventory.values()}):
            name, version, release = filename.removesuffix(".src.rpm").rsplit("-", 2)
            url = f"https://kojipkgs.fedoraproject.org/packages/{name}/{version}/{release}/src/{filename}"
            destination = args.output / filename
            if not destination.exists():
                temporary = args.output / (filename + ".partial")
                with urllib.request.urlopen(url, timeout=120) as response, temporary.open("wb") as output:
                    shutil.copyfileobj(response, output)
                temporary.rename(destination)
            # Fedora builder contains the official distribution signing keys.
            subprocess.run(["rpmkeys", "--checksig", str(destination)], check=True)
    else:
        packages = sorted({record["source"] + "=" + record["source_version"] for record in inventory.values()})
        for package in packages:
            subprocess.run(["apt-get", "source", "--download-only", package], cwd=args.output, check=True)
    records = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(args.output.iterdir()) if path.is_file() and path.name != "SHA256SUMS"}
    (args.output / "SHA256SUMS").write_text("".join(f"{value}  {name}\n" for name, value in records.items()))


if __name__ == "__main__":
    main()
