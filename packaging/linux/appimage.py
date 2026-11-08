#!/usr/bin/env python3
"""Bundle a Linux CMake build using pinned linuxdeploy tools (run on Ubuntu 22.04)."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--appdir", type=Path, required=True)
    parser.add_argument("--tools", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--runtime-notices", type=Path, required=True)
    args = parser.parse_args()
    if args.appdir.exists() or args.output.exists():
        raise ValueError("Choose new AppDir/output paths; existing output was preserved")
    if not (args.runtime_notices / "type2-runtime-fedfad5.tar.gz-LICENSE").is_file():
        raise ValueError("Run packaging/tools/appimage_sources.py first to preserve the runtime notices/source")
    root = Path(__file__).resolve().parents[2]
    pins = json.loads((root / "packaging/linux/tools.json").read_text())
    for name in ["linuxdeploy", "linuxdeploy-plugin-qt", "runtime-x86_64"]:
        if hashlib.sha256((args.tools / name).read_bytes()).hexdigest() != pins[name]["sha256"]:
            raise ValueError("Build tool SHA256 mismatch")
    env = dict(os.environ, DESTDIR=str(args.appdir.resolve()))
    subprocess.run(["cmake", "--install", str(args.build), "--prefix", "/usr"], env=env, check=True)
    shutil.copytree(args.runtime_notices, args.appdir / "usr/share/doc/steam-art-studio/appimage-runtime")
    qmake = shutil.which("qmake6")
    plugins = Path(subprocess.check_output([qmake, "-query", "QT_INSTALL_PLUGINS"], text=True).strip())
    tls = args.appdir / "usr/plugins/tls"
    shutil.copytree(plugins / "tls", tls)
    platforms = ["libqoffscreen.so"]
    for name in ["libqwayland-egl.so", "libqwayland-generic.so"]:
        if (plugins / "platforms" / name).is_file():
            platforms.append(name)
    env.update(APPIMAGE_EXTRACT_AND_RUN="1", QMAKE=qmake, EXTRA_QT_MODULES="svg", EXTRA_PLATFORM_PLUGINS=";".join(platforms), ARCH="x86_64", NO_STRIP="1")
    env["PATH"] = str(args.tools.resolve()) + os.pathsep + env["PATH"]
    command = [str((args.tools / "linuxdeploy").resolve()), "--appdir", str(args.appdir), "--plugin", "qt", "--icon-file", str(root / "assets/steam-art-studio.png"), "--icon-filename", "org.steamartstudio.SteamArtStudio"]
    for library in ["libssl.so.3", "libcrypto.so.3"]:
        command += ["--library", "/usr/lib/x86_64-linux-gnu/" + library]
    subprocess.run(command, env=env, check=True)
    notices = args.appdir / "usr/share/doc/steam-art-studio/third-party"
    notices.mkdir(parents=True)
    packages = {}
    # Preserve copyright and exact source-package versions for shipped Ubuntu libraries.
    for file in args.appdir.rglob("*"):
        if not file.is_file() or ".so" not in file.name:
            continue
        owners = subprocess.run(["dpkg-query", "-S", "*/" + file.name], capture_output=True, text=True).stdout.splitlines()
        for owner in owners:
            package = owner.split(": ", 1)[0]
            if package in packages:
                continue
            record = subprocess.run(["dpkg-query", "-W", "-f=${binary:Package}\t${Version}\t${source:Package}\t${source:Version}", package], capture_output=True, text=True, check=True).stdout.split("\t")
            packages[package] = dict(zip(["binary", "version", "source", "source_version"], record))
            copyright_file = Path("/usr/share/doc") / package.split(":")[0] / "copyright"
            if copyright_file.is_file():
                destination = notices / package.replace(":", "_")
                destination.mkdir(exist_ok=True)
                shutil.copy2(copyright_file, destination / "copyright")
    (notices / "ubuntu-packages.json").write_text(json.dumps(packages, indent=2) + "\n")
    env["OUTPUT"] = str(args.output.resolve())
    env["LDAI_OUTPUT"] = str(args.output.resolve())
    env["LDAI_RUNTIME_FILE"] = str((args.tools / "runtime-x86_64").resolve())
    subprocess.run([str((args.tools / "linuxdeploy").resolve()), "--appdir", str(args.appdir), "--output", "appimage"], env=env, check=True)
    if not args.output.is_file():
        raise ValueError("AppImage output missing")
    digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
    args.output.with_name(args.output.name + ".sha256").write_text(f"{digest}  {args.output.name}\n")


if __name__ == "__main__":
    main()
