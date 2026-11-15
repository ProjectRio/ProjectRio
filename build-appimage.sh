#!/bin/bash -e
# build-appimage.sh

# CI runs this with "bash ./build-appimage.sh", which ignores the -e above.
set -e

APPIMAGE_STRING="Project_Rio-x86_64.AppImage"

# Fixed releases with checksums, because these run as part of a release build.
LINUXDEPLOY_PATH="https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1"
LINUXDEPLOY_FILE="linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_URL="${LINUXDEPLOY_PATH}/${LINUXDEPLOY_FILE}"
LINUXDEPLOY_SHA256="c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"

UPDATEPLUG_PATH="https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/1-alpha-20250213-1"
UPDATEPLUG_FILE="linuxdeploy-plugin-appimage-x86_64.AppImage"
UPDATEPLUG_URL="${UPDATEPLUG_PATH}/${UPDATEPLUG_FILE}"
UPDATEPLUG_SHA256="992d502a248e14ab185448ddf6f6e7d25558cb84d4623c354c3af350c25fccb3"

QTPLUG_PATH="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1"
QTPLUG_FILE="linuxdeploy-plugin-qt-x86_64.AppImage"
QTPLUG_URL="${QTPLUG_PATH}/${QTPLUG_FILE}"
QTPLUG_SHA256="15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724"

APPDIR_BIN="./AppDir/usr/bin"
APPDIR_HOOKS="./AppDir/apprun-hooks"

# Grab various appimage binaries from GitHub if we don't have them
if [ ! -e ./Tools/linuxdeploy ]; then
	wget ${LINUXDEPLOY_URL} -O ./Tools/linuxdeploy
fi
if [ ! -e ./Tools/linuxdeploy-update-plugin ]; then
	wget ${UPDATEPLUG_URL} -O ./Tools/linuxdeploy-update-plugin
fi
if [ ! -e ./Tools/linuxdeploy-plugin-qt ]; then
	wget ${QTPLUG_URL} -O ./Tools/linuxdeploy-plugin-qt
fi

# Stops the build if a download doesn't match, including one left over from an older build.
sha256sum -c - <<EOF
${LINUXDEPLOY_SHA256}  ./Tools/linuxdeploy
${UPDATEPLUG_SHA256}  ./Tools/linuxdeploy-update-plugin
${QTPLUG_SHA256}  ./Tools/linuxdeploy-plugin-qt
EOF

chmod +x ./Tools/linuxdeploy
chmod +x ./Tools/linuxdeploy-update-plugin
chmod +x ./Tools/linuxdeploy-plugin-qt

# Delete the AppDir folder to prevent build issues
rm -rf ./AppDir/

# Add the linux-env script to the AppDir prior to running linuxdeploy
mkdir -p ${APPDIR_HOOKS}
cp Data/linux-env.sh ${APPDIR_HOOKS}

# Build the AppDir directory for this image
mkdir -p AppDir
./Tools/linuxdeploy \
	--appdir=./AppDir \
	-e ./build/Binaries/dolphin-emu \
	-d ./Data/ProjectRio.desktop \
	-i ./Data/ProjectRio.png \
	-p qt

# Add the Sys dir to the AppDir for packaging
cp -r Data/Sys ${APPDIR_BIN}

cp Data/AppRun ${APPDIR_BIN}

# Build type
echo "Using Netplay build config"

# remove existing appimage just in case
rm -f ${APPIMAGE_STRING}

# Bake the AppImage
./Tools/linuxdeploy-update-plugin --appdir=./AppDir/

# Remove underscore from created AppImages and relocate to `./build/Binaries/`
mv ./${APPIMAGE_STRING} ./build/Binaries/ProjectRio-Linux.AppImage