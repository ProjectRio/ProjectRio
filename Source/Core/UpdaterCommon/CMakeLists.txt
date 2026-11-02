add_library(updatercommon
  UI.h
  UpdaterCommon.cpp
  UpdaterCommon.h
  UpdateKeys.h
  UpdateManifest.cpp
  UpdateManifest.h
)

target_link_libraries(updatercommon PRIVATE
  uicommon
  ed25519
  MbedTLS::mbedtls
  ZLIB::ZLIB
  minizip::minizip
  cpp-optparse
)

if(MSVC)
  # Add precompiled header
  target_link_libraries(updatercommon PRIVATE use_pch)
endif()
