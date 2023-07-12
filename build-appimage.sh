#!/bin/bash -e
# build-appimage.sh

ZSYNC_STRING="gh-releases-zsync|project-lylat|Ishiiruka|latest|Lylat_Online-x86_64.AppImage.zsync"
NETPLAY_APPIMAGE_STRING="Lylat_Online-x86_64.AppImage"

LINUXDEPLOY_PATH="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous"
LINUXDEPLOY_FILE="linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_URL="${LINUXDEPLOY_PATH}/${LINUXDEPLOY_FILE}"

UPDATEPLUG_PATH="https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous"
UPDATEPLUG_FILE="linuxdeploy-plugin-appimage-x86_64.AppImage"
UPDATEPLUG_URL="${UPDATEPLUG_PATH}/${UPDATEPLUG_FILE}"

UPDATETOOL_PATH="https://github.com/AppImage/AppImageUpdate/releases/download/continuous"
UPDATETOOL_FILE="appimageupdatetool-x86_64.AppImage"
UPDATETOOL_URL="${UPDATETOOL_PATH}/${UPDATETOOL_FILE}"

APPDIR_BIN="./AppDir/usr/bin"
APPDIR_HOOKS="./AppDir/apprun-hooks"

# Grab various appimage binaries from GitHub if we don't have them
if [ ! -e ./Tools/linuxdeploy ]; then
	wget ${LINUXDEPLOY_URL} -O ./Tools/linuxdeploy
fi
if [ ! -e ./Tools/linuxdeploy-update-plugin ]; then
	wget ${UPDATEPLUG_URL} -O ./Tools/linuxdeploy-update-plugin
fi
if [ ! -e ./Tools/appimageupdatetool ]; then
	wget ${UPDATETOOL_URL} -O ./Tools/appimageupdatetool
fi

chmod +x ./Tools/linuxdeploy
chmod +x ./Tools/linuxdeploy-update-plugin
chmod +x ./Tools/appimageupdatetool

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
	-d ./Data/dolphin-emu.desktop \
	-i ./Data/RioLogo.png

# Add the Sys dir to the AppDir for packaging
cp -r Data/Sys ${APPDIR_BIN}

# Build type
echo "Using Netplay build config"

# remove existing appimage just in case
rm -f ${NETPLAY_APPIMAGE_STRING}

# Package up the update tool within the AppImage
cp ./Tools/appimageupdatetool ./AppDir/usr/bin/

# Bake an AppImage with the update metadata
UPDATE_INFORMATION="${ZSYNC_STRING}" \
  ./Tools/linuxdeploy-update-plugin --appdir=./AppDir/