// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UpdaterCommon/MoveIntoPlace.h"

#include <chrono>
#include <thread>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"

namespace UpdateInstall
{
namespace
{
constexpr int MOVE_ATTEMPTS = 5;
constexpr auto MOVE_RETRY_DELAY = std::chrono::milliseconds(200);

// Antivirus and indexers can hold on to a file for a moment.
bool MoveWithRetries(const std::string& from, const std::string& to)
{
  for (int attempt = 0; attempt < MOVE_ATTEMPTS; ++attempt)
  {
    if (attempt != 0)
      std::this_thread::sleep_for(MOVE_RETRY_DELAY);
    if (File::Rename(from, to))
      return true;
  }
  return false;
}

void Remove(const std::string& path)
{
  if (File::IsDirectory(path))
    File::DeleteDirRecursively(path);
  else
    File::Delete(path);
}
}  // namespace

bool MoveIntoPlace(const std::string& source_dir, const std::string& install_dir,
                   const std::string& backup_dir, const std::vector<std::string>& items,
                   std::string* error)
{
  std::vector<std::string> placed;
  std::vector<std::string> moved_aside;

  const auto undo = [&](std::string message) {
    for (const std::string& item : placed)
      Remove(install_dir + DIR_SEP + item);

    for (const std::string& item : moved_aside)
    {
      if (!MoveWithRetries(backup_dir + DIR_SEP + item, install_dir + DIR_SEP + item))
        message += " Could not put back " + item + ", it's still in " + backup_dir + ".";
    }

    *error = std::move(message);
    return false;
  };

  for (const std::string& item : items)
  {
    const std::string destination = install_dir + DIR_SEP + item;
    const std::string backup = backup_dir + DIR_SEP + item;

    // These create the parent directories of the path they're given.
    if (!File::CreateFullPath(destination) || !File::CreateFullPath(backup))
      return undo("Could not create the directories for " + item + ".");

    if (File::Exists(destination))
    {
      if (!MoveWithRetries(destination, backup))
        return undo("Could not move the existing " + item + " aside.");
      moved_aside.push_back(item);
    }

    if (!MoveWithRetries(source_dir + DIR_SEP + item, destination))
      return undo("Could not move " + item + " into place.");
    placed.push_back(item);
  }

  return true;
}
}  // namespace UpdateInstall
