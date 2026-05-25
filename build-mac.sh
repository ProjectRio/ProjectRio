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

mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ..
cmake --build . --target dolphin-emu -- -j$(sysctl -n hw.logicalcpu)
popd

echo "Copying Sys files into the bundle"
cp -Rfn "${DATA_SYS_PATH}" "${BINARY_PATH}"

# When no real certificate is present, ad-hoc sign the bundle so macOS does not
# reject it as "damaged" due to Homebrew dylibs that fixup_bundle modified after
# they were originally signed. Ad-hoc signing replaces the broken signatures and
# allows users to open the app via right-click -> Open or Settings > Privacy.
if [[ -z "${CERTIFICATE_MACOS_APPLICATION}" ]]; then
    echo "Ad-hoc signing bundle..."
    codesign --force --deep --sign - "./build/Binaries/ProjectRio.app"
fi

# fixup_bundle copies versioned dylib aliases (e.g. libavcodec.62.dylib and
# libavcodec.62.28.101.dylib) as separate full-size files, and copies Qt framework
# binaries to multiple locations (Versions/A/, Versions/Current/, and the top-level
# framework directory) instead of keeping symlinks. Deduplicate by content after the
# bundle is fully assembled: for each set of identical files, keep the most canonical
# copy and delete the rest. Nothing in the bundle references the deleted paths because
# fixup_bundle already rewrote all install names to point at the canonical copies.
echo "Deduplicating bundle frameworks..."
python3 - "./build/Binaries/ProjectRio.app/Contents/Frameworks" <<'PYEOF'
import os, sys, hashlib

def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()

frameworks = sys.argv[1]
if not os.path.isdir(frameworks):
    sys.exit(0)

file_hashes = {}
for dirpath, dirnames, filenames in os.walk(frameworks):
    for fname in filenames:
        fpath = os.path.join(dirpath, fname)
        if os.path.islink(fpath):
            continue
        try:
            file_hashes.setdefault(sha256(fpath), []).append(fpath)
        except OSError:
            pass

def priority(p):
    rel = os.path.relpath(p, frameworks)
    name = os.path.basename(p)
    if 'Versions/A' in rel:
        return (2, len(name))
    if 'Versions/Current' in rel:
        return (0, len(name))
    return (1, len(name))

deleted = 0
freed = 0
for paths in file_hashes.values():
    if len(paths) < 2:
        continue
    paths.sort(key=priority, reverse=True)
    for dup in paths[1:]:
        size = os.path.getsize(dup)
        os.remove(dup)
        freed += size
        deleted += 1
        print(f"  Removed: {os.path.relpath(dup, frameworks)}")

print(f"Removed {deleted} duplicate file(s), freed {freed / 1024 / 1024:.1f} MB")
PYEOF
