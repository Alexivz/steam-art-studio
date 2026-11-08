#!/usr/bin/env python3
"""Build a deterministic archive from the same binary; no private data included."""
import argparse
import gzip
import hashlib
import io
import os
from pathlib import Path
import platform
import re
import subprocess
import tarfile


def main():
    root = Path(__file__).resolve().parent.parent
    version = (root / "VERSION").read_text().strip()
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[a-z0-9.-]+)?", version):
        raise RuntimeError("VERSION no válido")
    name = f"steam-art-studio-{version}-linux-{platform.machine()}"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=root / "dist" / f"{name}.tar.gz")
    args = parser.parse_args()
    if args.output.exists():
        raise RuntimeError("El archivo de salida ya existe; elige otro --output para conservarlo.")
    binary = root / "build/bin/steam-art-studio"
    check = subprocess.run([str(binary), "--version"], env={**os.environ, "QT_QPA_PLATFORM": "offscreen"}, capture_output=True, text=True, check=True, timeout=15)
    if check.stdout.strip() != f"steam-art-studio {version}":
        raise RuntimeError("La versión compilada no coincide con VERSION. Recompila antes de empaquetar.")
    files = {"bin/steam-art-studio": binary.read_bytes(), "assets/steam-art-studio.svg": (root / "assets/steam-art-studio.svg").read_bytes(), "scripts/install.py": (root / "scripts/install.py").read_bytes()}
    for relative in ["README.md", "CHANGELOG.md", "VERSION", "LICENSE", "THIRD_PARTY_NOTICES.md", "docs/ICONOS.md", "docs/FIABILIDAD.md", "docs/DISTRIBUCION.md", "docs/ESPACIO-UNICO.md"]:
        files[relative] = (root / relative).read_bytes()
    files["run.sh"] = b'#!/bin/sh\nROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$ROOT/bin/steam-art-studio" "$@"\n'
    files["install.sh"] = b'#!/bin/sh\nROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec python3 "$ROOT/scripts/install.py" install "$@"\n'
    files["LEEME-PAQUETE.txt"] = "Abrir desde esta carpeta: ./run.sh\nInstalar para tu usuario (pide confirmación): ./install.sh\nDespués: ~/.local/bin/steam-art-studio\nDesinstalar: ~/.local/bin/steam-art-studio-uninstall\nLa desinstalación conserva datos, cola, caché y copias.\nNo se cierra una instancia existente ni se modifica Steam al instalar.\nConsulta RUNTIME.txt para dependencias y límites de compatibilidad.\n".encode()
    files["RUNTIME.txt"] = ("Paquete dinámico para Linux " + platform.machine() + ".\nRequiere las bibliotecas Qt 6 compatibles con el equipo de compilación (Qt " + subprocess.run(["qmake6", "-query", "QT_VERSION"], capture_output=True, text=True, check=True).stdout.strip() + "), plugins de plataforma y FFmpeg opcional.\nNo es un AppImage autocontenido ni garantiza compatibilidad con todas las distribuciones.\nNo se han incluido bibliotecas de terceros, claves, datos de Steam, caché o copias.\nPython 3 solo es necesario para instalar/desinstalar, no para ejecutar la aplicación.\nCódigo propio GPL-3.0-only; ver LICENSE y THIRD_PARTY_NOTICES.md. Este archivo no es una publicación automática.\n").encode()
    files["SHA256SUMS"] = "".join(f"{hashlib.sha256(data).hexdigest()}  {path}\n" for path, data in sorted(files.items())).encode()
    archive = io.BytesIO()
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "0"))
    with gzip.GzipFile(fileobj=archive, mode="wb", filename="", mtime=0) as zipped:
        with tarfile.open(fileobj=zipped, mode="w|", format=tarfile.PAX_FORMAT) as tar:
            for path, data in sorted(files.items()):
                info = tarfile.TarInfo(name + "/" + path); info.size = len(data); info.mtime = epoch; info.uid = info.gid = 0; info.uname = info.gname = ""
                info.mode = 0o755 if path in {"bin/steam-art-studio", "run.sh", "install.sh"} else 0o644
                tar.addfile(info, io.BytesIO(data))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("xb") as stream:
        stream.write(archive.getvalue())
    checksum = hashlib.sha256(archive.getvalue()).hexdigest()
    print(args.output)
    print("SHA256", checksum)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
