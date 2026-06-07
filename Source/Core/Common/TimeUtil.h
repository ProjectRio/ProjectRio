// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <ctime>
#include <optional>

namespace Common
{
// Threadsafe and error-checking variant of std::localtime()
std::optional<std::tm> LocalTime(std::time_t time);
// Threadsafe and error-checking variant of std::gmtime()
std::optional<std::tm> GmTime(std::time_t time);
}  // namespace Common
