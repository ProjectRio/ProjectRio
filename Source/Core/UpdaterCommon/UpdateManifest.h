// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Common/CommonTypes.h"

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

// Checks the signed manifest attached to every release. Everything in here is pure: no network, no
// file access and no UI. Errors go through the out-parameter so the caller can log them.
namespace UpdateManifest
{
// Neither update_manifest.json nor update_manifest.sig may be larger than this.
constexpr size_t MAX_FILE_SIZE = 64 * 1024;

struct PublicKey
{
  std::array<u8, 32> bytes;
};

struct Asset
{
  u64 size;
  std::string sha256;
};

struct Manifest
{
  std::string version;
  std::map<std::string, Asset> assets;
};

// True if any line of `signature_file` is a valid signature of `manifest` by one of `keys`. The
// signature covers the exact bytes of the file, so call this before parsing them.
bool VerifySignatures(std::span<const u8> manifest, std::string_view signature_file,
                      std::span<const PublicKey> keys, std::string* error);

// Only call this on a manifest that VerifySignatures() accepted.
std::optional<Manifest> Parse(std::string_view manifest, std::string* error);

// True if `package` has the size and sha256 that `asset` lists.
bool VerifyPackage(const Asset& asset, std::span<const u8> package, std::string* error);
}  // namespace UpdateManifest
