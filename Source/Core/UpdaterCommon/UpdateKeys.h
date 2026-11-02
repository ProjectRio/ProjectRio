// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <span>

#include "UpdaterCommon/UpdateManifest.h"

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

// The keys of the maintainers who can sign a release. An update installs if any one of them signed
// it, and a build only ever trusts the keys it was compiled with. Tools/sign-release.py prints the
// entry to add here, ending in the rio-update-key comment that the release job in
// .github/workflows/main.yml counts. Keep that comment off of anything but a production key.
//
// With entries in it, the list reads:
//   inline constexpr std::array<UpdateManifest::PublicKey, 2> UPDATE_PUBLIC_KEYS{{
//       <first entry>
//       <second entry>
//   }};
inline constexpr std::array<UpdateManifest::PublicKey, 0> UPDATE_PUBLIC_KEYS{};

#ifdef RIO_UPDATER_TEST_KEYS
// Throwaway key for testing the updater against a test release, instead of the keys above. It is
// not secret and must never be in a release build: RIO_UPDATER_TEST_KEYS is only ever defined by
// asking for it, and the release job refuses a workflow or build script that does.
inline constexpr std::array<UpdateManifest::PublicKey, 1> UPDATE_TEST_PUBLIC_KEYS{{
    UpdateManifest::PublicKey{{0x33, 0xb8, 0xa3, 0x3d, 0x64, 0xb8, 0x02, 0xf4, 0x59, 0xbc, 0xf2,
                               0xb7, 0xab, 0xa6, 0x99, 0x3f, 0x17, 0x99, 0x90, 0x48, 0xb4, 0x49,
                               0xf6, 0xac, 0x01, 0xd0, 0x5d, 0xf9, 0x01, 0xdf, 0x89, 0xae}},
}};
#endif

// The keys this build accepts.
inline std::span<const UpdateManifest::PublicKey> TrustedUpdateKeys()
{
#ifdef RIO_UPDATER_TEST_KEYS
  return UPDATE_TEST_PUBLIC_KEYS;
#else
  return UPDATE_PUBLIC_KEYS;
#endif
}
