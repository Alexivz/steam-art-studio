#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
"$ROOT/scripts/build.sh"
mkdir -p /tmp/opencode "$ROOT/build-tests" "$ROOT/build-gui-tests" "$ROOT/build-crash-tests" "$ROOT/build-network-tests" "$ROOT/build-portable-tests" "$ROOT/build-playback-tests"
cd "$ROOT/build-portable-tests"
qmake6 "$ROOT/tests/portable.pro"
make -j"${JOBS:-4}"
./bin/studio-portable-tests -txt
cd "$ROOT/build-playback-tests"
qmake6 "$ROOT/tests/playback.pro"
make -j"${JOBS:-4}"
QT_QPA_PLATFORM=offscreen ./bin/studio-playback-tests -txt
python3 "$ROOT/tests/test_packaging.py"
cd "$ROOT/build-crash-tests"
qmake6 "$ROOT/tests/crash_worker.pro"
make -j"${JOBS:-4}"
cd "$ROOT/build-tests"
qmake6 "$ROOT/tests/tests.pro"
make -j"${JOBS:-4}"
./bin/studio-tests -txt
cd "$ROOT/build-network-tests"
qmake6 "$ROOT/tests/network.pro"
make -j"${JOBS:-4}"
./bin/studio-network-tests -txt
cd "$ROOT/build-gui-tests"
qmake6 "$ROOT/tests/gui.pro"
make -j"${JOBS:-4}"
QT_QPA_PLATFORM=offscreen ./bin/studio-gui-tests -txt
cd "$ROOT"
python3 -m unittest discover -s tests -p 'test_distribution.py' -v
