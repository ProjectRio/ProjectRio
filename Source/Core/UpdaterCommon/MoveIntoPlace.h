// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

namespace UpdateInstall
{
// Moves each of `items`, paths relative to `source_dir`, to the same path in `install_dir`. What
// each one replaces is moved to the same path in `backup_dir`. All three have to be on one volume.
// On failure everything already moved is put back and `error` says why. If something couldn't be
// put back, `error` says so too, and it's still in `backup_dir`.
bool MoveIntoPlace(const std::string& source_dir, const std::string& install_dir,
                   const std::string& backup_dir, const std::vector<std::string>& items,
                   std::string* error);
}  // namespace UpdateInstall
