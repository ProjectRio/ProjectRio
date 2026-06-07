#!/bin/bash -e
# build-mac.sh

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/ProjectRio.app/Contents/Resources/"

BREW_PREFIX=$(brew --prefix)
export LIBRARY_PATH=$LIBRARY_PATH:${BREW_PREFIX}/lib:/usr/local/lib:/usr/lib/

QT_BREW_PATH=$(brew --prefix qt@6)
CMAKE_FLAGS="-DQt6_DIR=${QT_BREW_PATH}/lib/cmake/Qt6 -DENABLE_NOGUI=false"

# Disable CMake's POST_BUILD codesign step. Any signature it produced would be
# invalidated by the framework layout pass below, so we ad-hoc sign once at the
# end of this script instead.
CMAKE_FLAGS+=' -DMACOS_CODE_SIGNING="OFF"'

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

# fixup_bundle expands every framework/dylib symlink into a full file copy:
#   - Versioned dylib aliases (libfoo.62.dylib + libfoo.62.28.101.dylib) become
#     two identical full-size files instead of one file + one symlink.
#   - Qt frameworks end up with the binary duplicated at Versions/A/Foo,
#     Versions/Current/Foo, and Foo.framework/Foo, and Versions/Current is a
#     real directory instead of a symlink to A. That layout is structurally
#     invalid for a macOS framework and Gatekeeper rejects it as "damaged",
#     regardless of code signing.
#
# Restore the canonical layout: for each framework, keep Versions/<X> as the
# only real copy and replace Versions/Current and the top-level entries with
# the symlinks Apple's framework spec requires. For loose dylib aliases,
# keep the most-specific version as the real file and symlink the shorter
# aliases to it (mirroring Homebrew's own layout).
echo "Restoring framework symlinks and deduplicating..."
python3 - "./build/Binaries/ProjectRio.app/Contents/Frameworks" <<'PYEOF'
import os, sys, shutil, hashlib

def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()

def tree_size(path):
    if os.path.islink(path) or not os.path.exists(path):
        return 0
    if os.path.isfile(path):
        return os.path.getsize(path)
    total = 0
    for dp, _, fns in os.walk(path):
        for f in fns:
            fp = os.path.join(dp, f)
            if not os.path.islink(fp):
                try:
                    total += os.path.getsize(fp)
                except OSError:
                    pass
    return total

def remove(path):
    if os.path.islink(path) or os.path.isfile(path):
        os.unlink(path)
    elif os.path.isdir(path):
        shutil.rmtree(path)

frameworks = sys.argv[1]
if not os.path.isdir(frameworks):
    sys.exit(0)

layout_freed = 0
dedup_freed = 0

# Pass 1: rebuild proper framework layout (Versions/Current symlink + top-level symlinks).
for entry in sorted(os.listdir(frameworks)):
    if not entry.endswith('.framework'):
        continue
    fw = os.path.join(frameworks, entry)
    versions_dir = os.path.join(fw, 'Versions')
    if not os.path.isdir(versions_dir) or os.path.islink(versions_dir):
        continue

    # Find the actual version directory (anything that isn't "Current" and is a real dir).
    real_version = None
    for v in sorted(os.listdir(versions_dir)):
        if v == 'Current':
            continue
        vp = os.path.join(versions_dir, v)
        if os.path.isdir(vp) and not os.path.islink(vp):
            real_version = v
            break
    if real_version is None:
        continue
    real_version_path = os.path.join(versions_dir, real_version)

    # Replace Versions/Current with a symlink to the real version directory.
    current = os.path.join(versions_dir, 'Current')
    if os.path.lexists(current):
        layout_freed += tree_size(current)
        remove(current)
    os.symlink(real_version, current)

    # For each top-level entry inside Versions/<X>, ensure a matching symlink at
    # the framework root pointing at Versions/Current/<entry>.
    for child in os.listdir(real_version_path):
        top = os.path.join(fw, child)
        if os.path.islink(top):
            continue
        if os.path.lexists(top):
            layout_freed += tree_size(top)
            remove(top)
        os.symlink(os.path.join('Versions', 'Current', child), top)

# Pass 2: dedup any remaining content-identical real files (mostly versioned
# dylib aliases at the top of Frameworks/). os.walk does not follow symlinks
# by default, so framework internals are visited exactly once via Versions/<X>.
file_hashes = {}
for dp, _, fns in os.walk(frameworks):
    for fn in fns:
        fp = os.path.join(dp, fn)
        if os.path.islink(fp):
            continue
        try:
            file_hashes.setdefault(sha256(fp), []).append(fp)
        except OSError:
            pass

def canonical_score(p):
    # Prefer files living inside a Versions/<X>/ directory (the proper home for
    # a framework binary). Otherwise prefer the longest filename, which for
    # dylib aliases is the most-specific version (libfoo.62.28.101.dylib over
    # libfoo.62.dylib).
    rel = os.path.relpath(p, frameworks)
    in_version = '/Versions/' in rel and '/Versions/Current/' not in rel
    return (in_version, len(os.path.basename(p)))

linked = 0
for paths in file_hashes.values():
    if len(paths) < 2:
        continue
    paths.sort(key=canonical_score, reverse=True)
    keep = paths[0]
    for dup in paths[1:]:
        rel_target = os.path.relpath(keep, os.path.dirname(dup))
        dedup_freed += os.path.getsize(dup)
        os.unlink(dup)
        os.symlink(rel_target, dup)
        linked += 1
        print(f"  Linked: {os.path.relpath(dup, frameworks)} -> {rel_target}")

total_freed = layout_freed + dedup_freed
print(f"Framework layout fix freed {layout_freed / 1024 / 1024:.1f} MB; "
      f"deduped {linked} file(s) freed {dedup_freed / 1024 / 1024:.1f} MB "
      f"(total {total_freed / 1024 / 1024:.1f} MB)")
PYEOF

# Ad-hoc sign the bundle AFTER the layout fix so the signature covers the final
# state. fixup_bundle rewrites install names on Homebrew dylibs (invalidating
# their original signatures) and the layout pass above further modifies the
# bundle, so without a fresh signature macOS marks the app as "damaged".
#
# Signing is done inside-out (Apple's recommended pattern, replacing the now-
# deprecated --deep flag): every nested .dylib and .framework is signed first
# (deepest paths first, via `find -depth`), then the outer .app is signed last
# so its signature seals over the already-signed nested code. All nested items
# are passed to a single codesign invocation via xargs so we pay process-startup
# overhead once instead of once per file — the Intel CI runner is slow enough
# that the per-call overhead dominated total build time when this was a loop.
#
# This script only ad-hoc signs; release signing + notarization is not wired up
# yet. When it is, add the cert import, identity, hardened-runtime opts, and
# entitlements as one cohesive change rather than half-scaffolded here.
APP="./build/Binaries/ProjectRio.app"
echo "Ad-hoc signing bundle (inside-out)..."

# Pipe all nested items into one codesign invocation so process-startup cost is paid once.
find "${APP}/Contents" -depth \( -name "*.dylib" -o -name "*.framework" \) -print0 \
    | xargs -0 codesign --force --sign -

codesign --force --sign - "${APP}"

# Verify so a broken bundle fails the build instead of shipping. The outer
# seal's CodeResources hashes already cover every nested file, so verifying
# without --deep catches the failure modes we care about (corrupted bundle,
# missed signature on the main binary). --deep would re-hash every nested
# Mach-O a second time — not worth the time on CI for ad-hoc builds.
codesign --verify --strict --verbose=2 "${APP}"
