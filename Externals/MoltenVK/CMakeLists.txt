include(ExternalProject)

set(MOLTENVK_VERSION "v1.2.8")

# MoltenVK's Makefile only provides a universal "macos" target (arm64 + x86_64)
# and no per-architecture targets like "macos-arm64". The universal build can
# fail on CI runners that lack cross-compilation SDKs. To work around this, we
# call xcodebuild directly with ARCHS restricted to the native architecture.
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
  set(MOLTENVK_ARCHS "arm64")
else()
  set(MOLTENVK_ARCHS "x86_64")
endif()

ExternalProject_Add(MoltenVK
  GIT_REPOSITORY https://github.com/KhronosGroup/MoltenVK.git
  GIT_TAG ${MOLTENVK_VERSION}

  CONFIGURE_COMMAND ${CMAKE_CURRENT_LIST_DIR}/configure.sh <LOG_DIR> <SOURCE_DIR> ${MOLTENVK_VERSION}

  PATCH_COMMAND ${CMAKE_CURRENT_LIST_DIR}/patch.sh ${CMAKE_SOURCE_DIR}/Externals/MoltenVK/patches/ ${MOLTENVK_VERSION}

  BUILD_COMMAND bash -c "xcodebuild build -project MoltenVKPackaging.xcodeproj -scheme 'MoltenVK Package (macOS only)' -destination 'generic/platform=macOS' ARCHS=${MOLTENVK_ARCHS} ONLY_ACTIVE_ARCH=YES"
  BUILD_IN_SOURCE ON
  BUILD_BYPRODUCTS <SOURCE_DIR>/Package/Release/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib

  INSTALL_COMMAND ""

  LOG_CONFIGURE ON
  LOG_BUILD ON
  LOG_OUTPUT_ON_FAILURE ON
)
