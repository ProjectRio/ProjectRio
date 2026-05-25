#!/bin/bash -e
# build-mac.sh

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/ProjectRio.app/Contents/Resources/"

BREW_PREFIX=$(brew --prefix)
export LIBRARY_PATH=$LIBRARY_PATH:${BREW_PREFIX}/lib:/usr/local/lib:/usr/lib/

QT_BREW_PATH=$(brew --prefix qt@6)
CMAKE_FLAGS="-DQt6_DIR=${QT_BREW_PATH}/lib/cmake/Qt6 -DENABLE_NOGUI=false"

if [[ -z "${CERTIFICATE_MACOS_APPLICATION}" ]]; then
    echo "Building without code signing"
    CMAKE_FLAGS+=' -DMACOS_CODE_SIGNING="OFF"'
else
    echo "Building with code signing"
    CMAKE_FLAGS+=' -DMACOS_CODE_SIGNING="ON"'
fi

CMAKE_FLAGS+=' -DCMAKE_POLICY_VERSION_MINIMUM=3.5'

# Use ccache if available
if command -v ccache &> /dev/null; then
    CMAKE_FLAGS+=' -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache'
fi

# Normalize Homebrew dylib hard links to proper symlinks before cmake runs.
# CI Homebrew sometimes installs versioned dylib aliases (e.g. libavcodec.62.dylib
# and libavcodec.62.28.101.dylib) as hard links rather than symlinks. CMake's
# fixup_bundle copies both. Converting aliases to symlinks means fixup_bundle
# sees only one canonical copy per library.
echo "Normalizing Homebrew dylib symlinks in ${BREW_PREFIX}/lib..."
python3 - "${BREW_PREFIX}/lib" <<'PYEOF'
import os, sys

lib_dir = sys.argv[1]
if not os.path.isdir(lib_dir):
    sys.exit(0)

inodes = {}
for name in os.listdir(lib_dir):
    if not name.endswith('.dylib'):
        continue
    path = os.path.join(lib_dir, name)
    if os.path.islink(path):
        continue
    try:
        inodes.setdefault(os.stat(path).st_ino, []).append(path)
    except OSError:
        pass

normalized = 0
for paths in inodes.values():
    if len(paths) < 2:
        continue
    paths.sort(key=lambda p: len(os.path.basename(p)), reverse=True)
    canonical = paths[0]
    for alias in paths[1:]:
        os.remove(alias)
        os.symlink(os.path.basename(canonical), alias)
        print(f"  {os.path.basename(alias)} -> {os.path.basename(canonical)}")
        normalized += 1

print(f"Normalized {normalized} dylib alias(es).")
PYEOF

mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ..
cmake --build . --target dolphin-emu -- -j$(sysctl -n hw.logicalcpu)
popd

echo "Copying Sys files into the bundle"
cp -Rfn "${DATA_SYS_PATH}" "${BINARY_PATH}"
