#!/usr/bin/env python3
"""User-only installer. Never removes Qt application data, caches or backups."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def locations():
    home = Path.home().absolute()
    data = Path(os.environ.get("XDG_DATA_HOME", str(home / ".local/share")))
    if not data.is_absolute():
        raise RuntimeError("XDG_DATA_HOME debe ser una ruta absoluta.")
    app = data / "steam-art-studio-app"
    return app, {
        "binary": app / "bin/steam-art-studio",
        "uninstaller": app / "uninstall.py",
        "launcher": home / ".local/bin/steam-art-studio",
        "uninstall_launcher": home / ".local/bin/steam-art-studio-uninstall",
        "desktop": data / "applications/steam-art-studio.desktop",
        "icon": data / "icons/hicolor/scalable/apps/steam-art-studio.svg",
    }


def safe_path(path):
    for component in (path, *path.parents):
        if component.is_symlink():
            raise RuntimeError(f"Se rechaza un enlace simbólico: {component}")
    if path.exists() and not path.is_file():
        raise RuntimeError(f"El destino no es un archivo: {path}")


def atomic_write(path, data, mode=0o644):
    safe_path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    name = None
    try:
        with tempfile.NamedTemporaryFile(prefix=".studio-install-", dir=path.parent, delete=False) as stream:
            name = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        name.chmod(mode)
        os.replace(name, path)
    finally:
        if name is not None and name.exists():
            name.unlink()


def load_state(app, paths):
    marker = app / ".studio-install.json"
    safe_path(marker)
    if not marker.exists():
        if app.exists() and any(app.iterdir()):
            raise RuntimeError("La carpeta de programa contiene archivos ajenos; no se sobrescribirá.")
        return None
    if marker.stat().st_size > 1024 * 1024:
        raise RuntimeError("Registro de instalación demasiado grande.")
    state = json.loads(marker.read_text())
    if not isinstance(state, dict) or state.get("format") != 1 or not isinstance(state.get("files"), dict) or set(state["files"]) != set(paths):
        raise RuntimeError("Registro de instalación no válido.")
    for key, path in paths.items():
        entry = state["files"][key]
        if not isinstance(entry, dict) or entry.get("path") != str(path) or not isinstance(entry.get("sha256"), str) or not re.fullmatch(r"[0-9a-f]{64}", entry["sha256"]):
            raise RuntimeError("El registro no coincide con las rutas de esta instalación.")
    return state


def install(root):
    app, paths = locations()
    state = load_state(app, paths)
    for key, path in paths.items():
        safe_path(path)
        if path.exists() and (state is None or digest(path.read_bytes()) != state["files"][key]["sha256"]):
            raise RuntimeError(f"No se sobrescribirá un archivo ajeno o modificado: {path}")
    binary = root / "bin/steam-art-studio"
    if not binary.is_file():
        binary = root / "build/bin/steam-art-studio"
    if not binary.is_file() or binary.is_symlink():
        raise RuntimeError("Falta el ejecutable. Ejecuta scripts/build.sh antes de instalar.")
    check = subprocess.run([str(binary), "--version"], env={**os.environ, "QT_QPA_PLATFORM": "offscreen"}, capture_output=True, text=True, timeout=15)
    if check.returncode:
        raise RuntimeError("No se puede ejecutar con las bibliotecas Qt instaladas: " + check.stderr.strip())
    launcher = ("#!/bin/sh\nexec " + shlex.quote(str(paths["binary"])) + ' "$@"\n').encode()
    uninstall_launcher = ("#!/bin/sh\nexec python3 " + shlex.quote(str(paths["uninstaller"])) + ' uninstall "$@"\n').encode()
    escaped = str(paths["launcher"]).replace("\\", "\\\\").replace('"', '\\"').replace("`", "\\`").replace("$", "\\$").replace("%", "%%")
    desktop = f'[Desktop Entry]\nType=Application\nVersion=1.0\nName=Steam Art Studio\nComment=Biblioteca y arte de Steam — versión preliminar\nExec="{escaped}"\nIcon=steam-art-studio\nTerminal=false\nCategories=Game;Utility;\nStartupWMClass=steam-art-studio\n'.encode()
    payloads = {"binary": binary.read_bytes(), "uninstaller": Path(__file__).read_bytes(), "launcher": launcher, "uninstall_launcher": uninstall_launcher, "desktop": desktop, "icon": (root / "assets/steam-art-studio.svg").read_bytes()}
    previous = {key: (path.read_bytes(), path.stat().st_mode & 0o777) if path.exists() else None for key, path in paths.items()}
    written = []
    try:
        for key, path in paths.items():
            atomic_write(path, payloads[key], 0o755 if key in {"binary", "launcher", "uninstall_launcher"} else 0o644)
            written.append(key)
        record = {"format": 1, "version": check.stdout.strip(), "files": {key: {"path": str(path), "sha256": digest(payloads[key])} for key, path in paths.items()}}
        atomic_write(app / ".studio-install.json", json.dumps(record, indent=2).encode(), 0o600)
    except Exception:
        for key in reversed(written):
            path = paths[key]
            if previous[key] is not None:
                atomic_write(path, *previous[key])
            elif path.is_file() and not path.is_symlink() and digest(path.read_bytes()) == digest(payloads[key]):
                path.unlink()
        raise
    print("Instalado para este usuario. Datos, cola y copias se conservan por separado.")
    print("Abrir:", paths["launcher"])


def uninstall():
    app, paths = locations()
    state = load_state(app, paths)
    if state is None:
        raise RuntimeError("No hay una instalación marcada de este programa.")
    for path in paths.values():
        safe_path(path)
    changed = [str(path) for key, path in paths.items() if path.exists() and digest(path.read_bytes()) != state["files"][key]["sha256"]]
    if changed:
        raise RuntimeError("Desinstalación cancelada; se conservan todos los archivos, incluidos los modificados: " + ", ".join(changed))
    # Keep the uninstall command available if preflight detects customizations.
    for path in paths.values():
        if path.exists():
            path.unlink()
    (app / ".studio-install.json").unlink()
    for directory in [app / "bin", app]:
        try:
            directory.rmdir()
        except OSError:
            pass
    print("Programa desinstalado. Datos, caché, cola y copias NO se han borrado.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["install", "uninstall"])
    parser.add_argument("--yes", action="store_true", help="Confirmar instalación/desinstalación sin pregunta interactiva.")
    args = parser.parse_args()
    if not args.yes:
        print("Solo archivos del programa para este usuario; nunca datos de Steam o copias.")
        if not sys.stdin.isatty() or input("Escribe SI para continuar: ") != "SI":
            return 2
    try:
        if args.action == "install":
            install(Path(__file__).resolve().parent.parent)
        else:
            uninstall()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
