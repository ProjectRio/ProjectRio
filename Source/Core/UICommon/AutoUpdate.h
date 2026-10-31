// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <string_view>

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

// This class defines all the logic for Dolphin auto-update checking. UI-specific elements have to
// be defined in a backend specific subclass.
class AutoUpdateChecker
{
public:
  enum class CheckType
  {
    Automatic,
    Manual,
  };
  // Initiates a check for updates in the background. Calls the OnUpdateAvailable callback if an
  // update is available, does "nothing" otherwise.
  void CheckForUpdate(std::string_view update_track, std::string_view hash_override,
                      CheckType check_type);

  // True if this build ships an updater that can download and apply an update by itself. When
  // false, update checks still run, but the user is only told that a new version exists.
  static bool SystemSupportsAutoUpdates();

  struct NewVersionInformation
  {
    // Tag name of the GitHub release, e.g. "2.3.0".
    std::string new_shortrev;

    // Body of the GitHub release, in Markdown. The UI layer is responsible for rendering it.
    std::string changelog_html;

    // The release asset to install. Empty if the release has no asset for this platform, in which
    // case the user can only be pointed at the download page.
    std::string package_url;
    std::string package_filename;
    // "sha256:<hex>" as reported by the GitHub API. Empty on releases published before GitHub
    // started attaching digests, in which case the download is not verified.
    std::string package_digest;
  };

  // Starts the updater process, which will wait in the background until the current process exits.
  enum class RestartMode
  {
    NO_RESTART_AFTER_UPDATE = 0,
    RESTART_AFTER_UPDATE,
  };
  void TriggerUpdate(const NewVersionInformation& info, RestartMode restart_mode);

protected:
  virtual void OnUpdateAvailable(const NewVersionInformation& info) = 0;
};
