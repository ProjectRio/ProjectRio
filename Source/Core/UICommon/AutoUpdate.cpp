// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/AutoUpdate.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <picojson.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Version.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <sys/stat.h>
#endif

#if defined(_WIN32) || defined(__APPLE__)
#define OS_SUPPORTS_UPDATER
#endif

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

namespace
{
bool s_update_triggered = false;

// Updates are pulled straight from the GitHub releases page: the tag name of the latest release is
// the version number, and the release assets are the packages we install.
constexpr char UPDATE_REPO[] = "ProjectRio/ProjectRio";

// Setting RIO_UPDATE_REPO (e.g. "someone/ProjectRio") checks and downloads from that repo's
// releases instead, so the whole update flow can be tested against a throwaway release on a fork.
std::string UpdateRepo()
{
  if (const char* repo = std::getenv("RIO_UPDATE_REPO"); repo && *repo)
    return repo;
  return UPDATE_REPO;
}

std::string LatestReleaseApiUrl()
{
  return fmt::format("https://api.github.com/repos/{}/releases/latest", UpdateRepo());
}

// Every asset of a GitHub release is served from this prefix. Checked before handing a URL to the
// updater so that a surprising API response can never make us download from somewhere else.
std::string ReleaseAssetUrlPrefix()
{
  return fmt::format("https://github.com/{}/releases/download/", UpdateRepo());
}

#ifdef __APPLE__
// Must match MacUpdater_NAME in Source/Core/MacUpdater/CMakeLists.txt.
const char UPDATER_CONTENT_PATH[] = "/Contents/MacOS/Project Rio Updater";
#endif

#ifdef OS_SUPPORTS_UPDATER

const char UPDATER_LOG_FILE[] = "Updater.log";

std::string UpdaterPath(bool relocated = false)
{
  std::string path(File::GetExeDirectory() + DIR_SEP);
#ifdef __APPLE__
  if (relocated)
    path += ".Project Rio Updater.2.app";
  else
    path += "Project Rio Updater.app";
  return path;
#else
  if (relocated)
    return path + "Updater.exe.bak";
  return path + "Updater.exe";
#endif
}

std::string MakeUpdaterCommandLine(const std::map<std::string, std::string>& flags)
{
#ifdef __APPLE__
  std::string cmdline = "\"" + UpdaterPath(true) + UPDATER_CONTENT_PATH + "\"";
#else
  std::string cmdline = UpdaterPath();
#endif

  cmdline += " ";

  for (const auto& pair : flags)
  {
    std::string value = "--" + pair.first + "=" + pair.second;
    value = ReplaceAll(value, "\"", "\\\"");  // Escape double quotes.
    value = "\"" + value + "\" ";
    cmdline += value;
  }
  return cmdline;
}

void CleanupFromPreviousUpdate()
{
#ifdef __APPLE__
  // Remove the relocated updater file.
  File::DeleteDirRecursively(UpdaterPath(true));
#else
  // Windows cannot overwrite a running executable, so the updater renames itself out of the way
  // before replacing the install. Clean up the leftover now that nothing is holding it open.
  if (File::Exists(UpdaterPath(true)))
    File::Delete(UpdaterPath(true));
#endif
}

#endif

// Parses "2.3.0" (or "v2.3.0") into its numeric components. Returns nothing if the string is not
// purely dot-separated numbers, so callers can fall back to comparing the strings as-is.
std::optional<std::vector<u32>> ParseVersion(std::string_view version)
{
  if (version.starts_with('v') || version.starts_with('V'))
    version.remove_prefix(1);
  if (version.empty())
    return std::nullopt;

  std::vector<u32> components;
  for (const std::string& part : SplitString(std::string(version), '.'))
  {
    u32 value;
    if (!TryParse(part, &value, 10))
      return std::nullopt;
    components.push_back(value);
  }
  return components;
}

// True if the release tagged `candidate` is newer than the version we are running.
bool IsNewerVersion(const std::string& candidate, const std::string& current)
{
  auto candidate_parts = ParseVersion(candidate);
  auto current_parts = ParseVersion(current);
  if (!candidate_parts || !current_parts)
  {
    // Not a version we know how to order (a dated tag, a hash, ...). Treat anything different as
    // an update rather than silently never updating again.
    return candidate != current;
  }

  // "2.3" and "2.3.0" are the same version, so pad both to the same length before comparing.
  const size_t size = std::max(candidate_parts->size(), current_parts->size());
  candidate_parts->resize(size, 0);
  current_parts->resize(size, 0);
  return *candidate_parts > *current_parts;
}

// Scores a release asset by how well it fits the platform we are running on. 0 means "not for this
// platform", higher is a better match. Asset names have drifted between releases
// (Project-Rio-Windows.exe, Project.Rio.Windows.zip, Project_Rio_Windows_Installer.exe, ...), so
// match on substrings instead of requiring one exact filename.
int ScoreAssetName(std::string name)
{
  Common::ToLower(&name);
  const auto contains = [&name](std::string_view needle) {
    return name.find(needle) != std::string::npos;
  };
  const bool is_zip = name.ends_with(".zip");

#if defined(_WIN32)
#if defined(_M_ARM_64)
  constexpr bool want_arm = true;
#else
  constexpr bool want_arm = false;
#endif
  if (!contains("win") || contains("arm") != want_arm)
    return 0;
  // Update from the zip of the portable build whenever a release has one: unpacking it over the
  // install needs no elevation, leaves shortcuts and the uninstall entry alone (so a portable copy
  // stays portable), and is the same thing macOS does. The updater checks that the zip really
  // holds a build before unpacking it. The installer is only a fallback for a release without a
  // zip.
  if (is_zip)
    return 2;
  if (name.ends_with(".exe"))
    return 1;
  return 0;
#elif defined(__APPLE__)
  if (!contains("mac") || !is_zip)
    return 0;
#if defined(_M_ARM_64)
  if (contains("arm") || contains("silicon") || contains("apple"))
    return 2;
#else
  if (contains("intel") || contains("x86") || contains("x64"))
    return 2;
#endif
  return contains("universal") ? 1 : 0;
#else
  return 0;
#endif
}

// Picks the best asset of a GitHub release for this platform, if the release has one at all.
void SelectPackage(const picojson::array& assets, AutoUpdateChecker::NewVersionInformation* nvi)
{
  int best_score = 0;
  for (const auto& asset : assets)
  {
    if (!asset.is<picojson::object>())
      continue;
    const picojson::object& obj = asset.get<picojson::object>();

    const auto name_it = obj.find("name");
    const auto url_it = obj.find("browser_download_url");
    if (name_it == obj.end() || !name_it->second.is<std::string>() || url_it == obj.end() ||
        !url_it->second.is<std::string>())
    {
      continue;
    }
    const std::string& name = name_it->second.get<std::string>();
    const std::string& url = url_it->second.get<std::string>();

    const int score = ScoreAssetName(name);
    if (score <= best_score)
      continue;
    if (!url.starts_with(ReleaseAssetUrlPrefix()))
    {
      WARN_LOG_FMT(COMMON, "Auto-update: ignoring asset {} with unexpected URL {}", name, url);
      continue;
    }

    best_score = score;
    nvi->package_url = url;
    nvi->package_filename = name;
    nvi->package_digest.clear();
    const auto digest_it = obj.find("digest");
    if (digest_it != obj.end() && digest_it->second.is<std::string>())
      nvi->package_digest = digest_it->second.get<std::string>();
  }
}
}  // namespace

bool AutoUpdateChecker::SystemSupportsAutoUpdates()
{
#if defined(AUTOUPDATE) && defined(OS_SUPPORTS_UPDATER)
  // Builds that do not ship the updater next to the executable (currently every macOS build) can
  // still check for updates, they just cannot install them.
  return File::Exists(UpdaterPath());
#else
  return false;
#endif
}

static u32 GetOwnProcessId()
{
#ifdef _WIN32
  return GetCurrentProcessId();
#else
  return getpid();
#endif
}

void AutoUpdateChecker::CheckForUpdate(std::string_view update_track,
                                       std::string_view hash_override, const CheckType check_type)
{
  const bool is_manual_check = check_type == CheckType::Manual;

  // An empty update track means the user asked us to stop checking on startup. A check the user
  // started themselves always runs.
  if (!is_manual_check && update_track.empty())
    return;

#ifdef OS_SUPPORTS_UPDATER
  CleanupFromPreviousUpdate();
#endif

  // This url returns a json containing info about the latest release
  const std::string url = LatestReleaseApiUrl();
  // The GitHub API rejects requests that don't identify themselves.
  const Common::HttpRequest::Headers headers = {
      {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like "
                     "Gecko) Chrome/97.0.4692.71 Safari/537.36"}};

  Common::HttpRequest req{std::chrono::seconds{10}};
  req.FollowRedirects(10);
  auto resp = req.Get(url, headers);
  if (!resp)
  {
    if (is_manual_check)
      CriticalAlertFmtT("Unable to contact update server.");
    return;
  }
  const std::string contents(reinterpret_cast<char*>(resp->data()), resp->size());
  INFO_LOG_FMT(COMMON, "Auto-update JSON response: {}", contents);

  picojson::value json;
  const std::string err = picojson::parse(json, contents);
  // Bad responses (a captive portal's HTML page, a GitHub rate-limit error) only get a popup when
  // the user asked for the check; the startup check just logs them.
  if (!err.empty())
  {
    ERROR_LOG_FMT(COMMON, "Auto-update: invalid JSON: {}", err);
    if (is_manual_check)
      CriticalAlertFmtT("Invalid JSON received from auto-update service : {0}", err);
    return;
  }
  picojson::object obj;
  if (json.is<picojson::object>())
    obj = json.get<picojson::object>();
  if (!obj["tag_name"].is<std::string>())
  {
    ERROR_LOG_FMT(COMMON, "Auto-update: response has no tag_name");
    if (is_manual_check)
      CriticalAlertFmtT("Unexpected response from auto-update service.");
    return;
  }

  // check if latest version == current
  const std::string latest_version = obj["tag_name"].get<std::string>();
  if (!IsNewerVersion(latest_version, Common::GetRioRevStr()))
  {
    if (is_manual_check)
      SuccessAlertFmtT("You are running the latest version available on this update track.");
    INFO_LOG_FMT(COMMON, "Auto-update status: we are up to date.");
    return;
  }

  NewVersionInformation nvi;
  nvi.new_shortrev = latest_version;
  if (obj["body"].is<std::string>())
    nvi.changelog_html = obj["body"].get<std::string>();
  if (obj["assets"].is<picojson::array>())
    SelectPackage(obj["assets"].get<picojson::array>(), &nvi);

  INFO_LOG_FMT(COMMON, "Auto-update: {} is available, package: {}", nvi.new_shortrev,
               nvi.package_filename.empty() ? "(none for this platform)" : nvi.package_filename);

  OnUpdateAvailable(nvi);
}

void AutoUpdateChecker::TriggerUpdate(const AutoUpdateChecker::NewVersionInformation& info,
                                      const AutoUpdateChecker::RestartMode restart_mode)
{
  // Check to make sure we don't already have an update triggered
  if (s_update_triggered)
  {
    WARN_LOG_FMT(COMMON, "Auto-update: received a redundant trigger request, ignoring");
    return;
  }

  s_update_triggered = true;
#ifdef OS_SUPPORTS_UPDATER
  std::map<std::string, std::string> updater_flags;
  updater_flags["package-url"] = info.package_url;
  updater_flags["package-filename"] = info.package_filename;
  updater_flags["package-digest"] = info.package_digest;
  updater_flags["package-version"] = info.new_shortrev;
  updater_flags["parent-pid"] = std::to_string(GetOwnProcessId());
  updater_flags["install-base-path"] = File::GetExeDirectory();
  updater_flags["log-file"] = File::GetUserPath(D_LOGS_IDX) + UPDATER_LOG_FILE;

  if (restart_mode == RestartMode::RESTART_AFTER_UPDATE)
    updater_flags["binary-to-restart"] = File::GetExePath();

#ifdef __APPLE__
  // Copy the updater so it can update itself if needed.
  const std::string reloc_updater_path = UpdaterPath(true);
  if (!File::Copy(UpdaterPath(), reloc_updater_path))
  {
    CriticalAlertFmtT("Unable to create updater copy.");
    return;
  }
  if (chmod((reloc_updater_path + UPDATER_CONTENT_PATH).c_str(), 0700) != 0)
  {
    CriticalAlertFmtT("Unable to set permissions on updater copy.");
    return;
  }
#endif

  // Run the updater!
  std::string command_line = MakeUpdaterCommandLine(updater_flags);
  INFO_LOG_FMT(COMMON, "Updater command line: {}", command_line);

#ifdef _WIN32
  STARTUPINFO sinfo{.cb = sizeof(sinfo)};
  sinfo.dwFlags = STARTF_FORCEOFFFEEDBACK;  // No hourglass cursor after starting the process.
  PROCESS_INFORMATION pinfo;
  if (CreateProcessW(UTF8ToWString(UpdaterPath()).c_str(), UTF8ToWString(command_line).data(),
                     nullptr, nullptr, FALSE, 0, nullptr, nullptr, &sinfo, &pinfo))
  {
    CloseHandle(pinfo.hThread);
    CloseHandle(pinfo.hProcess);
  }
  else
  {
    const std::string error = Common::GetLastErrorString();
    CriticalAlertFmtT("Could not start updater process: {0}", error);
  }
#else
  if (popen(command_line.c_str(), "r") == nullptr)
  {
    const std::string error = Common::LastStrerrorString();
    CriticalAlertFmtT("Could not start updater process: {0}", error);
  }
#endif

#endif
}
