#!/bin/bash -e
# build-linux.sh

CMAKE_FLAGS='-DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DLINUX_LOCAL_DEV=true -DCMAKE_BUILD_WITH_INSTALL_RPATH=true -DCMAKE_BUILD_TYPE=Release -GNinja'

DATA_SYS_PATH="./Data/Sys/"
BINARY_PATH="./build/Binaries/"
LOCAL_SHARE="$HOME/.local/share"
DATA_ICON_PATH="./Data/ProjectRio.png"
DATA_DESKTOP_PATH="./Data/ProjectRio.desktop"

# Move into the build directory, run CMake, and compile the project
mkdir -p build
(
    cd ./build
    cmake .. ${CMAKE_FLAGS}
    ninja -j$(nproc)
)

# Copy the Sys folder in
cp -rn ${DATA_SYS_PATH} ${BINARY_PATH}

touch ${BINARY_PATH}/portable.txt

mkdir -p ${LOCAL_SHARE}/project-rio

cp -r ${BINARY_PATH}/. ${LOCAL_SHARE}/project-rio/

mkdir -p ${LOCAL_SHARE}/icons/

cp ${DATA_ICON_PATH} ${LOCAL_SHARE}/icons/

mkdir -p ${LOCAL_SHARE}/applications/

cp ${DATA_DESKTOP_PATH} ${LOCAL_SHARE}/applications/

chmod +x ${LOCAL_SHARE}/applications/ProjectRio.desktop


