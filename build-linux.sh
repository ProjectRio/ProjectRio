#!/bin/bash -e
# build-linux.sh

CMAKE_FLAGS='-DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DLINUX_LOCAL_DEV=true -DCMAKE_BUILD_WITH_INSTALL_RPATH=true -DCMAKE_BUILD_TYPE=Release -GNinja'

# Use ccache if available (speeds up incremental CI builds significantly)
if command -v ccache &> /dev/null; then
    CMAKE_FLAGS+=' -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache'
fi

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/"

# Move into the build directory, run CMake, and compile the project
mkdir -p build
(
    cd ./build
    cmake .. ${CMAKE_FLAGS}
    ninja -j$(nproc)
)

# Copy the Sys folder in
cp -r --update=none ${DATA_SYS_PATH} ${BINARY_PATH}

touch ${BINARY_PATH}/portable.txt