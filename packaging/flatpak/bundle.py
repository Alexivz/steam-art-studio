#!/usr/bin/env python3
"""Export the tested AppDir as a local Flatpak, without installing into a profile."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess

APP_ID = "org.steamartstudio.SteamArtStudio"


def bundle(appdir, build, repo, output):
    if build.exists() or repo.exists() or output.exists():
        raise ValueError("Choose new build/repo/output paths")
    if not (appdir / "usr/bin/steam-art-studio").is_file():
        raise ValueError("Complete AppDir required")
    files = build / "files"
    shutil.copytree(appdir / "usr", files, symlinks=True)
    executable = files / "bin/steam-art-studio"
    executable.rename(executable.with_suffix(".bin"))
    executable.write_text('#!/bin/sh\nexport LD_LIBRARY_PATH="/app/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\nexport QT_PLUGIN_PATH=/app/plugins\nexec /app/bin/steam-art-studio.bin "$@"\n')
    executable.chmod(0o755)
    # Explicit Steam roots only: no filesystem=home/host. Outside libraries can
    # be granted by the user later; local images use the file chooser portal.
    (build / "metadata").write_text(f"""[Application]
name={APP_ID}
runtime=org.freedesktop.Platform/x86_64/26.08
sdk=org.freedesktop.Sdk/x86_64/26.08
command=steam-art-studio

[Context]
shared=network;ipc;
sockets=x11;wayland;
devices=dri;
filesystems=~/.local/share/Steam;~/.steam;~/.var/app/com.valvesoftware.Steam/data/Steam;~/.var/app/com.valvesoftware.Steam/.local/share/Steam;

[Session Bus Policy]
org.kde.kwalletd5=talk
org.kde.kwalletd6=talk
""")
    subprocess.run(["flatpak", "build-finish", "--command=steam-art-studio", str(build)], check=True)
    subprocess.run(["flatpak", "build-export", "--arch=x86_64", str(repo), str(build), "preview"], check=True)
    subprocess.run(["flatpak", "build-bundle", str(repo), str(output), APP_ID, "preview"], check=True)
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    output.with_name(output.name + ".sha256").write_text(f"{digest}  {output.name}\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--appdir", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    bundle(args.appdir, args.build, args.repo, args.output)
