#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mkdir -p "$ROOT/build"
cd "$ROOT/build"
qmake6 "$ROOT/steam-art-studio.pro"
make -j"${JOBS:-4}"
