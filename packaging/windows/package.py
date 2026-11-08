#!/usr/bin/env python3
"""Create a portable ZIP and explicit-file, per-user NSIS recipe from a stage."""
import argparse
import hashlib
from pathlib import Path
import zipfile


def nsis_quote(value):
    return str(value).replace("$", "$$").replace('"', '$\\"')


def package(stage, output, version):
    required = ["steam-art-studio.exe", "platforms/qwindows.dll", "imageformats/qwebp.dll", "docs/LICENSE"]
    if any(not (stage / name).is_file() for name in required):
        raise ValueError("Incomplete Windows stage")
    files = sorted(path for path in stage.rglob("*") if path.is_file())
    if any(path.is_symlink() for path in files):
        raise ValueError("Symlinks are not allowed in the Windows stage")
    output.mkdir(parents=True, exist_ok=True)
    stem = f"steam-art-studio-{version}-windows-x86_64"
    archive = output / (stem + ".zip")
    with archive.open("xb") as raw, zipfile.ZipFile(raw, "w", zipfile.ZIP_DEFLATED) as zipped:
        for path in files:
            zipped.write(path, stem + "/" + path.relative_to(stage).as_posix())
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix(".zip.sha256").write_text(f"{digest}  {archive.name}\n")
    installer = output / (stem + "-setup.exe")
    recipe = output / (stem + ".nsi")
    if installer.exists() or recipe.exists():
        raise ValueError("Installer output already exists")
    lines = [
        'Unicode true', '!include "MUI2.nsh"', '!include "LogicLib.nsh"',
        'RequestExecutionLevel user', 'SetCompressor /SOLID lzma',
        'Name "Steam Art Studio"', f'OutFile "{nsis_quote(installer.resolve())}"',
        'InstallDir "$LOCALAPPDATA\\Programs\\Steam Art Studio"',
        f'!insertmacro MUI_PAGE_LICENSE "{nsis_quote((stage / "docs/LICENSE").resolve())}"',
        '!insertmacro MUI_PAGE_DIRECTORY', '!insertmacro MUI_PAGE_INSTFILES',
        '!insertmacro MUI_UNPAGE_CONFIRM', '!insertmacro MUI_UNPAGE_INSTFILES',
        '!insertmacro MUI_LANGUAGE "Spanish"',
        'Function .onInit', '  SetShellVarContext current', 'FunctionEnd',
        'Function .onVerifyInstDir',
        # Refuse reparse points in every existing ancestor, not just the leaf.
        '  StrCpy $0 "$INSTDIR"', '  ancestor:',
        '  System::Call \'kernel32::GetFileAttributesW(w r0) i.r1\'',
        '  IntCmp $1 -1 parent', '  IntOp $1 $1 & 0x400', '  IntCmp $1 0 parent',
        '  MessageBox MB_ICONSTOP "No se instalará a través de enlaces o junctions."', '  Abort',
        '  parent:', '  ${GetParent} "$0" $2', '  StrCmp $0 $2 checked', '  StrCmp $2 "" checked', '  StrCpy $0 $2', '  Goto ancestor',
        '  checked:',
        '  FindFirst $0 $1 "$INSTDIR\\*"', '  loop:',
        '  StrCmp $1 "" done', '  StrCmp $1 "." next', '  StrCmp $1 ".." next',
        '  FindClose $0', '  MessageBox MB_ICONSTOP "Elige una carpeta vacía. Desinstala antes la versión anterior; se conservarán tus datos."', '  Abort',
        '  next:', '  FindNext $0 $1', '  Goto loop', '  done:', '  FindClose $0', 'FunctionEnd',
        'Section "Steam Art Studio"', '  Call .onVerifyInstDir', '  SetShellVarContext current',
    ]
    lines.insert(3, '!include "FileFunc.nsh"')
    for path in files:
        relative = path.relative_to(stage)
        folder = str(relative.parent).replace("/", "\\")
        lines += [f'  SetOutPath "$INSTDIR\\{nsis_quote(folder)}"', f'  File "{nsis_quote(path.resolve())}"']
    lines += [
        '  SetOutPath "$INSTDIR"', '  WriteUninstaller "$INSTDIR\\uninstall.exe"',
        '  WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SteamArtStudio" "DisplayName" "Steam Art Studio"',
        f'  WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SteamArtStudio" "DisplayVersion" "{version}"',
        '  WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SteamArtStudio" "UninstallString" \'$\\"$INSTDIR\\uninstall.exe$\\"\'',
        '  WriteRegDWORD HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SteamArtStudio" "NoModify" 1',
        '  WriteRegDWORD HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SteamArtStudio" "NoRepair" 1',
        'SectionEnd',
        'Section "Uninstall"', '  SetShellVarContext current',
    ]
    for path in files:
        relative = str(path.relative_to(stage)).replace("/", "\\")
        lines.append(f'  Delete "$INSTDIR\\{nsis_quote(relative)}"')
    folders = {parent for path in files for parent in path.relative_to(stage).parents if str(parent) != "."}
    for folder in sorted(folders, key=lambda path: (-len(path.parts), str(path))):
        relative = str(folder).replace("/", "\\")
        lines.append(f'  RMDir "$INSTDIR\\{nsis_quote(relative)}"')
    lines += [
        '  Delete "$INSTDIR\\uninstall.exe"', '  RMDir "$INSTDIR"',
        '  DeleteRegKey HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SteamArtStudio"',
        # No recursive deletion and no reference to the Qt data/cache locations.
        'SectionEnd',
    ]
    recipe.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(archive)
    print("Compile installer with: makensis", recipe)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    version = (Path(__file__).resolve().parents[2] / "VERSION").read_text().strip()
    package(args.stage.resolve(), args.output.resolve(), version)
