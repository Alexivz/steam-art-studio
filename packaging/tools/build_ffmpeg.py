#!/usr/bin/env python3
"""Build the pinned, limited LGPL FFmpeg. Never installs into the host system."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
from fetch_tool import fetch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--windows-cross", action="store_true")
    parser.add_argument("--jobs", type=int, default=3)
    args = parser.parse_args()
    output = args.output.absolute()
    if output.exists():
        raise ValueError("Output already exists; choose a new FFmpeg prefix")
    archive = fetch("ffmpeg-9.0.2.tar.xz", args.cache)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ffmpeg-source-", dir=output.parent) as temporary:
        with tarfile.open(archive) as tar:
            for member in tar.getmembers():
                if Path(member.name).is_absolute() or ".." in Path(member.name).parts or member.issym() or member.islnk():
                    raise ValueError("Unsafe source archive")
            tar.extractall(temporary)
        source = Path(temporary) / "ffmpeg-9.0.2"
        flags = ["--prefix=" + str(output), "--disable-everything", "--disable-autodetect", "--disable-doc", "--disable-debug", "--disable-ffplay", "--disable-ffprobe", "--disable-network", "--enable-ffmpeg", "--enable-zlib", "--enable-libwebp", "--enable-protocol=file,pipe", "--enable-demuxer=apng,gif,webp_anim,webp_pipe,image2,png_pipe,matroska,mov", "--enable-decoder=apng,gif,webp,webp_anim,png,mjpeg,bmp,vp8,vp9,h264", "--enable-encoder=apng,rawvideo,gif,libwebp,libwebp_anim", "--enable-muxer=apng,rawvideo,gif,webp", "--enable-indev=lavfi", "--enable-filter=scale,pad,fps,select,format,testsrc,testsrc2,color,colorchannelmixer", "--enable-small", "--disable-shared", "--enable-static"]
        if args.windows_cross:
            flags += ["--enable-cross-compile", "--target-os=mingw32", "--arch=x86_64", "--cross-prefix=x86_64-w64-mingw32-", "--pkg-config=mingw64-pkg-config", "--disable-x86asm"]
        flags += ["--enable-decoder=wrapped_avframe,rawvideo", "--enable-demuxer=image_png_pipe,image_webp_pipe", "--enable-parser=gif,png,webp,h264,vp8,vp9"]
        subprocess.run([str(source / "configure"), *flags], cwd=source, check=True)
        subprocess.run(["make", "-j" + str(max(1, args.jobs))], cwd=source, check=True)
        executable = source / ("ffmpeg.exe" if args.windows_cross else "ffmpeg")
        (output / "bin").mkdir(parents=True)
        shutil.copy2(executable, output / "bin" / executable.name)
        notices = output / "share/doc/studio-ffmpeg"
        notices.mkdir(parents=True)
        shutil.copy2(source / "COPYING.LGPLv2.1", notices / "COPYING.LGPLv2.1")
        shutil.copy2(source / "LICENSE.md", notices / "LICENSE.md")
        shutil.copy2(archive, notices / archive.name)
        (notices / "build.json").write_text(json.dumps({"version": "9.0.2", "source_url": "https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz", "source_sha256": "8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e", "configure": flags, "gpl_enabled": False, "nonfree_enabled": False}, indent=2) + "\n")
    print(output)


if __name__ == "__main__":
    main()
