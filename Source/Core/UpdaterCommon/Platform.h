// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "UpdaterCommon/UpdaterCommon.h"

namespace Platform
{
// Checks that this machine can actually run the version we are about to install, using the
// build_info.txt shipped in the package. `package_path` holds the unpacked new version, and
// `install_base_path` the install we are replacing. Returns false to abort the update.
bool VersionCheck(const std::string& install_base_path, const std::string& package_path);
}  // namespace Platform
