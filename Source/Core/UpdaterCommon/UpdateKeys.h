// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <span>

#include "UpdaterCommon/UpdateManifest.h"

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

// The keys of the maintainers who can sign a release. An update installs if any one of them signed
// it, and a build only ever trusts the keys it was compiled with. Tools/sign-release.py prints the
// entry to add here. The release job in .github/workflows/main.yml requires two to five, counted
// by Tools/count-update-keys.py, which also checks the array's size matches its entries.
//
// With entries in it, the list reads:
//   inline constexpr std::array<UpdateManifest::PublicKey, 2> UPDATE_PUBLIC_KEYS{{
//       <first entry>
//       <second entry>
//   }};
inline constexpr std::array<UpdateManifest::PublicKey, 1> UPDATE_PUBLIC_KEYS{{
    UpdateManifest::PublicKey{
        {0xbb, 0x04, 0x55, 0x24, 0xbc, 0xf2, 0x4b, 0xc6, 0x66, 0x27, 0x58,
         0x4f, 0xba, 0xb9, 0x7d, 0xc2, 0xa4, 0x9d, 0x55, 0xe8, 0x1b, 0xdc,
         0xf0, 0x98, 0xb6, 0x30, 0xdd, 0x59, 0xe3, 0x36, 0xd7, 0x16}},  // key bb045524bcf24bc6
}};

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
