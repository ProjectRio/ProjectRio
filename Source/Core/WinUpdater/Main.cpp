// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <ShlObj.h>
#include <shellapi.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/StringUtil.h"

#include "UpdaterCommon/UI.h"
#include "UpdaterCommon/UpdaterCommon.h"

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
  if (lstrlenW(pCmdLine) == 0)
  {
    MessageBoxW(nullptr,
                L"This updater is not meant to be launched directly. Configure Auto-Update in "
                "Dolphin's settings instead.",
                L"Error", MB_ICONERROR);
    return 1;
  }

  std::vector<std::string> args = Common::CommandLineToUtf8Argv(pCmdLine);

  // Test for write permissions
  bool need_admin = false;

  auto path = Common::GetModuleName(hInstance);
  if (!path)
  {
    UI::Error("Failed to get updater filename.");
    return 1;
  }

  const auto test_fh = ::CreateFileW(
      (std::filesystem::path(*path).parent_path() / "directory_writable_check.tmp").c_str(),
      GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
      FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);

  if (test_fh == INVALID_HANDLE_VALUE)
    need_admin = true;
  else
    CloseHandle(test_fh);

  if (need_admin)
  {
    if (IsUserAnAdmin())
    {
      MessageBox(nullptr, L"Failed to write to directory despite administrator priviliges.",
                 L"Error", MB_ICONERROR);
      return 1;
    }

    // Relaunch the updater as administrator
    const auto result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"runas", path->c_str(), pCmdLine, nullptr, SW_SHOW));
    if (result > 32)
      return 0;

    // The user declined, and Rio closed so that the update could run, so bring it back.
    MessageBoxW(nullptr, L"The update needs administrator access, so nothing was changed.",
                L"Update canceled", MB_ICONINFORMATION);
    constexpr std::string_view RESTART_FLAG = "--binary-to-restart=";
    for (const std::string& arg : args)
    {
      if (arg.starts_with(RESTART_FLAG))
        UI::LaunchApplication(arg.substr(RESTART_FLAG.size()));
    }
    return 1;
  }

  return RunUpdater(args) ? 0 : 1;
}
