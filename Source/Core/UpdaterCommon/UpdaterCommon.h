// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

#ifdef __APPLE__
// Name of the application bundle a macOS package is expected to contain. The updater bundle sits
// next to it, so the install base path is the directory holding both.
constexpr char MAC_APP_BUNDLE[] = "ProjectRio.app";
#endif

void LogToFile(const char* fmt, ...);
bool RunUpdater(std::vector<std::string> args);
