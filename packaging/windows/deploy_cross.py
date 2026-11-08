#!/usr/bin/env python3
"""Stage MinGW Qt plugins and resolve PE imports recursively; fail on missing DLLs."""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess

SYSTEM = set("advapi32 avicap32 avrt bcrypt cfgmgr32 comctl32 comdlg32 crypt32 cryptbase d2d1 d3d9 d3d11 d3d12 dcomp dinput8 dnsapi dsound dwmapi dwrite dxgi dxva2 gdi32 hid imm32 iphlpapi kernel32 kernelbase mpr mscms msimg32 msvcrt ncrypt netapi32 normaliz ntdll ole32 oleacc oleaut32 opengl32 powrprof propsys psapi rasapi32 rpcrt4 secur32 setupapi shell32 shlwapi user32 userenv usp10 uxtheme version winhttp wininet winmm winscard winspool wintrust wldap32 ws2_32 wtsapi32 ucrtbase".split())
SYSTEM.update(["authz", "wsock32", "shcore"])


def imports(path):
    result = subprocess.run(["x86_64-w64-mingw32-objdump", "-p", str(path)], capture_output=True, text=True, check=True)
    return re.findall(r"DLL Name:\s*(\S+)", result.stdout)


def deploy(stage, sysroot):
    if not (stage / "steam-art-studio.exe").is_file():
        raise ValueError("Install the application to a dedicated stage first")
    plugins = sysroot / "lib/qt6/plugins"
    selected = {
        "platforms": ["qwindows.dll", "qoffscreen.dll"],
        "imageformats": ["qgif.dll", "qico.dll", "qjpeg.dll", "qsvg.dll", "qwebp.dll", "qtiff.dll"],
        "iconengines": ["qsvgicon.dll"],
        "tls": ["qopensslbackend.dll", "qschannelbackend.dll"],
    }
    for group, names in selected.items():
        (stage / group).mkdir(exist_ok=True)
        for name in names:
            source = plugins / group / name
            if not source.is_file():
                raise ValueError("Required plugin missing: " + str(source))
            shutil.copy2(source, stage / group / name)
    available = {path.name.lower(): path for path in (sysroot / "bin").glob("*.dll")}
    pending = list(stage.rglob("*.exe")) + list(stage.rglob("*.dll"))
    checked = set()
    records = {}
    while pending:
        binary = pending.pop()
        if binary in checked:
            continue
        checked.add(binary)
        for name in imports(binary):
            lower = name.lower()
            if lower in available:
                target = stage / available[lower].name
                if not target.exists():
                    shutil.copy2(available[lower], target)
                    pending.append(target)
                records[lower] = available[lower]
            elif lower.removesuffix(".dll") not in SYSTEM and not lower.startswith(("api-ms-win-", "ext-ms-win-")):
                raise ValueError(f"Unresolved dependency {name} in {binary.name}")
    notices = stage / "docs/third-party"
    notices.mkdir(parents=True, exist_ok=True)
    packages = {}
    for path in records.values():
        owner = subprocess.run(["rpm", "-qf", "--qf", "%{NAME}\t%{NEVRA}\t%{SOURCERPM}\n", str(path)], capture_output=True, text=True, check=True).stdout.strip().split("\t")
        packages[owner[0]] = {"binary_rpm": owner[1], "source_rpm": owner[2]}
    # Include plugin owners too, even when they do not add a distinct DLL.
    for group, names in selected.items():
        for name in names:
            data = subprocess.run(["rpm", "-qf", "--qf", "%{NAME}\t%{NEVRA}\t%{SOURCERPM}\n", str(plugins / group / name)], capture_output=True, text=True, check=True).stdout.strip().split("\t")
            packages[data[0]] = {"binary_rpm": data[1], "source_rpm": data[2]}
    for name in packages:
        license_dir = Path("/usr/share/licenses") / name
        if license_dir.is_dir():
            shutil.copytree(license_dir, notices / name, dirs_exist_ok=True)
    (notices / "fedora-packages.json").write_text(json.dumps(packages, indent=2) + "\n")
    (stage / "qt.conf").write_text("[Paths]\nPrefix=.\nPlugins=.\n")
    return len(checked)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", type=Path, required=True)
    parser.add_argument("--sysroot", type=Path, default=Path("/usr/x86_64-w64-mingw32/sys-root/mingw"))
    args = parser.parse_args()
    print("PE files checked:", deploy(args.stage, args.sysroot))
