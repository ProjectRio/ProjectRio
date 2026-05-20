#!/bin/bash -e
# build-mac.sh

QT_BREW_PATH=$(brew --prefix qt@6)
CMAKE_FLAGS="-DQt6_DIR=${QT_BREW_PATH}/lib/cmake/Qt6 -DENABLE_NOGUI=false"

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/ProjectRio.app/Contents/Resources/"

BREW_PREFIX=$(brew --prefix)
export LIBRARY_PATH=$LIBRARY_PATH:${BREW_PREFIX}/lib:/usr/local/lib:/usr/lib/

if [[ -z "${CERTIFICATE_MACOS_APPLICATION}" ]]
    then
        echo "Building without code signing"
        CMAKE_FLAGS+=' -DMACOS_CODE_SIGNING="OFF"'
else
        echo "Building with code signing"
        CMAKE_FLAGS+=' -DMACOS_CODE_SIGNING="ON"'
fi

CMAKE_FLAGS+=' -DCMAKE_POLICY_VERSION_MINIMUM=3.5'

# When BUILD_ARCH is set (CI cross-builds x86_64 on Apple Silicon via Rosetta),
# pin the slice we produce. Otherwise CMake targets the host arch.
if [[ -n "${BUILD_ARCH}" ]]; then
    CMAKE_FLAGS+=" -DCMAKE_OSX_ARCHITECTURES=${BUILD_ARCH}"
fi

# Use ccache if available (speeds up incremental CI builds significantly)
if command -v ccache &> /dev/null; then
    CMAKE_FLAGS+=' -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache'
fi

# Move into the build directory, run CMake, and compile the project
mkdir -p build
pushd build
cmake ${CMAKE_FLAGS} ..
cmake --build . --target dolphin-emu -- -j$(sysctl -n hw.logicalcpu)
popd

# Copy the Sys folder in
echo "Copying Sys files into the bundle"
cp -Rfn "${DATA_SYS_PATH}" "${BINARY_PATH}" 