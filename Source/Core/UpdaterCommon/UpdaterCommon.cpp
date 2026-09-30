// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UpdaterCommon/UpdaterCommon.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <OptionParser.h>
#include <mbedtls/sha256.h>
#include <mz_compat.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/IOFile.h"
#include "Common/MinizipUtil.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "UpdaterCommon/Platform.h"
#include "UpdaterCommon/UI.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#ifdef __APPLE__
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;
#endif

// Refer to docs/autoupdate_overview.md for a detailed overview of the autoupdate process

// Where to log updater output.
static File::IOFile log_file;

void LogToFile(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  log_file.WriteString(StringFromFormatV(fmt, args));
  log_file.Flush();

  va_end(args);
}

namespace
{
struct Options
{
  std::string package_url;
  std::string package_filename;
  // "sha256:<hex>" as reported by the GitHub API, empty if the release did not provide one.
  std::string package_digest;
  std::string install_base_path;
  std::optional<std::string> binary_to_restart;
  std::optional<u32> parent_pid;
  std::optional<std::string> log_file;
};

bool ProgressCallback(s64 total, s64 now, s64, s64)
{
  UI::SetCurrentProgress(static_cast<int>(now), static_cast<int>(total));
  return true;
}

std::string HexEncode(const u8* buffer, size_t size)
{
  std::string out(size * 2, '\0');

  for (size_t i = 0; i < size; ++i)
  {
    out[2 * i] = "0123456789abcdef"[buffer[i] >> 4];
    out[2 * i + 1] = "0123456789abcdef"[buffer[i] & 0xF];
  }

  return out;
}

void FlushLog()
{
  log_file.Flush();
  log_file.Close();
}

void FatalError(const std::string& message)
{
  LogToFile("%s\n", message.c_str());

  UI::SetVisible(true);
  UI::Error(message);
}

// The GitHub API reports a sha256 for every release asset. That is not a signature - it arrives in
// the same TLS-protected response as the download URL - but it does catch a truncated or corrupted
// download before we unpack or execute it.
bool VerifyDigest(const std::vector<u8>& data, const std::string& digest)
{
  constexpr std::string_view SHA256_PREFIX = "sha256:";

  if (digest.empty())
  {
    LogToFile("Release provided no digest for this package, skipping verification.\n");
    return true;
  }
  if (!digest.starts_with(SHA256_PREFIX))
  {
    LogToFile("Unsupported package digest \"%s\".\n", digest.c_str());
    return false;
  }

  std::array<u8, 32> hash;
  mbedtls_sha256_ret(data.data(), data.size(), hash.data(), false);
  const std::string computed = HexEncode(hash.data(), hash.size());

  std::string expected(digest.substr(SHA256_PREFIX.size()));
  Common::ToLower(&expected);
  if (computed != expected)
  {
    LogToFile("Package hash mismatch: expected %s, got %s.\n", expected.c_str(), computed.c_str());
    return false;
  }

  LogToFile("Package hash %s verified.\n", computed.c_str());
  return true;
}

bool DownloadPackage(const Options& opts, const std::string& destination)
{
  UI::SetDescription("Downloading " + opts.package_filename + "...");
  UI::SetTotalMarquee(true);
  UI::SetCurrentMarquee(false);

  LogToFile("Downloading %s ...\n", opts.package_url.c_str());

  Common::HttpRequest req(std::chrono::seconds(30), ProgressCallback);
  req.FollowRedirects(10);  // Release assets redirect to GitHub's asset host.
  const auto resp = req.Get(opts.package_url);
  if (!resp)
  {
    LogToFile("Download of %s failed.\n", opts.package_url.c_str());
    return false;
  }

  UI::SetDescription("Verifying " + opts.package_filename + "...");
  UI::SetCurrentMarquee(true);
  if (!VerifyDigest(*resp, opts.package_digest))
    return false;

  File::IOFile out(destination, "wb");
  if (!out || !out.WriteBytes(resp->data(), resp->size()))
  {
    LogToFile("Could not write %s.\n", destination.c_str());
    return false;
  }
  return true;
}

#ifdef __APPLE__
// Runs a command with an explicit argument vector, so no shell is involved and paths with spaces
// need no quoting. Returns true only if it exited with status 0.
bool RunProcess(const std::vector<std::string>& args)
{
  std::vector<char*> argv;
  for (const std::string& arg : args)
    argv.push_back(const_cast<char*>(arg.c_str()));
  argv.push_back(nullptr);

  pid_t pid;
  if (posix_spawn(&pid, argv[0], nullptr, nullptr, argv.data(), environ) != 0)
  {
    LogToFile("Could not run %s.\n", args[0].c_str());
    return false;
  }

  int status = 0;
  if (waitpid(pid, &status, 0) != pid)
  {
    LogToFile("Could not wait for %s.\n", args[0].c_str());
    return false;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
  {
    LogToFile("%s did not succeed (status %d).\n", args[0].c_str(), status);
    return false;
  }
  return true;
}
#endif

#ifndef __APPLE__
// Rejects archive entries that would write outside of the directory we extract into.
bool IsSafeArchivePath(const std::string& name)
{
  if (name.empty() || name.front() == '/' || name.front() == '\\')
    return false;
  if (name.size() >= 2 && name[1] == ':')  // Drive letter.
    return false;
  for (const std::string& component : SplitString(ReplaceAll(name, "\\", "/"), '/'))
  {
    if (component == "..")
      return false;
  }
  return true;
}

bool ExtractPackageWithMinizip(const std::string& package_path, const std::string& destination)
{
  unzFile zip = unzOpen(package_path.c_str());
  if (zip == nullptr)
  {
    LogToFile("Could not open %s as a zip archive.\n", package_path.c_str());
    return false;
  }
  Common::ScopeGuard zip_guard{[&] { unzClose(zip); }};

  unz_global_info64 global_info{};
  if (unzGetGlobalInfo64(zip, &global_info) != MZ_OK)
  {
    LogToFile("Could not read the archive's directory.\n");
    return false;
  }
  const int total_entries = static_cast<int>(global_info.number_entry);

  if (unzGoToFirstFile(zip) != MZ_OK)
  {
    LogToFile("Archive is empty.\n");
    return false;
  }

  int entry_index = 0;
  do
  {
    std::string entry_name(UINT16_MAX + 1, '\0');
    unz_file_info64 entry_info{};
    if (unzGetCurrentFileInfo64(zip, &entry_info, entry_name.data(), UINT16_MAX, nullptr, 0,
                                nullptr, 0) != MZ_OK)
    {
      LogToFile("Could not read the info of archive entry %d.\n", entry_index);
      return false;
    }
    TruncateToCString(&entry_name);

    UI::SetTotalProgress(++entry_index, total_entries);

    if (!IsSafeArchivePath(entry_name))
    {
      LogToFile("Refusing to extract archive entry \"%s\".\n", entry_name.c_str());
      return false;
    }

    const std::string out_path = destination + DIR_SEP + entry_name;
    // For a directory entry this creates the directory itself; for a file it creates its parents.
    if (!File::CreateFullPath(out_path))
    {
      LogToFile("Could not create the directory structure for %s.\n", entry_name.c_str());
      return false;
    }
    if (entry_name.ends_with('/') || entry_name.ends_with('\\'))
      continue;

    const size_t size = static_cast<size_t>(entry_info.uncompressed_size);
    std::vector<u8> contents(size);
    if (!Common::ReadFileFromZip(zip, contents.data(), size))
    {
      LogToFile("Could not read %s out of the archive.\n", entry_name.c_str());
      return false;
    }

    File::IOFile out(out_path, "wb");
    if (!out || (size != 0 && !out.WriteBytes(contents.data(), size)))
    {
      LogToFile("Could not write %s.\n", out_path.c_str());
      return false;
    }
  } while (unzGoToNextFile(zip) == MZ_OK);

  LogToFile("Extracted %d entries.\n", entry_index);
  return true;
}
#endif  // !__APPLE__

bool ExtractPackage(const std::string& package_path, const std::string& destination)
{
#ifdef __APPLE__
  // An app bundle is held together by symlinks (every framework's Versions/Current, plus the
  // versioned dylib aliases) and by permission bits. Our own unzip restores neither, which leaves
  // a bundle macOS reports as damaged, so hand the job to ditto instead.
  UI::SetCurrentMarquee(true);
  return RunProcess({"/usr/bin/ditto", "-x", "-k", package_path, destination});
#else
  return ExtractPackageWithMinizip(package_path, destination);
#endif
}

// Where the package is unpacked before being moved into place. On macOS this has to sit next to
// the install: the bundles are moved in with a rename, and a rename across volumes fails.
std::string StagingPath([[maybe_unused]] const Options& opts,
                        [[maybe_unused]] const std::string& temp_dir)
{
#ifdef __APPLE__
  return opts.install_base_path + DIR_SEP + ".projectrio-update";
#else
  return temp_dir + DIR_SEP + "package";
#endif
}

#ifdef __APPLE__
// Moves each top level item of the package (the app bundle and the updater next to it) into place.
// Bundles are replaced wholesale rather than merged, because macOS caches a Mach-O's code
// signature against its inode: writing new bytes over an existing executable makes the kernel
// check the new binary against the old signature and kill it on launch.
bool SwapInPackage(const std::string& source_path, const std::string& install_base_path)
{
  const File::FSTEntry contents = File::ScanDirectoryTree(source_path, false);
  if (contents.children.empty())
  {
    LogToFile("Package is empty.\n");
    return false;
  }

  for (const auto& child : contents.children)
  {
    const std::string destination = install_base_path + DIR_SEP + child.virtualName;
    LogToFile("Replacing %s.\n", destination.c_str());

    if (File::IsDirectory(destination))
    {
      if (!File::DeleteDirRecursively(destination))
      {
        LogToFile("Could not remove the existing %s.\n", destination.c_str());
        return false;
      }
    }
    else if (File::Exists(destination) && !File::Delete(destination))
    {
      LogToFile("Could not remove the existing %s.\n", destination.c_str());
      return false;
    }

    if (!File::Rename(child.physicalName, destination))
    {
      LogToFile("Could not move %s into place.\n", child.virtualName.c_str());
      return false;
    }
  }
  return true;
}
#endif

// Where MakeWayForNewUpdater() moved the running updater, so a failed install can put it back.
std::string s_original_self;
std::string s_relocated_self;

// Windows will not overwrite a running executable, but it will happily rename one. Move ourselves
// aside so the incoming package can drop in a new Updater.exe; Dolphin deletes the leftover the
// next time it starts.
bool MakeWayForNewUpdater()
{
#ifdef _WIN32
  const auto self_path = Common::GetModuleName(nullptr);
  if (!self_path)
  {
    LogToFile("Could not determine the updater's own path.\n");
    return false;
  }

  const std::string self = WStringToUTF8(*self_path);
  const std::string backup = self + ".bak";
  if (File::Exists(backup))
    File::Delete(backup);
  if (!File::Rename(self, backup))
  {
    LogToFile("Could not rename %s out of the way.\n", self.c_str());
    return false;
  }
  LogToFile("Renamed %s to %s.\n", self.c_str(), backup.c_str());

  s_original_self = self;
  s_relocated_self = backup;
#endif
  return true;
}

// Undoes MakeWayForNewUpdater() after a failed install, so one bad update doesn't leave the
// install with no updater to try again with.
void RestoreUpdater()
{
  // If the install got far enough to write a new updater, leave that one in place.
  if (s_relocated_self.empty() || File::Exists(s_original_self))
    return;

  if (File::Rename(s_relocated_self, s_original_self))
    LogToFile("Moved %s back to %s.\n", s_relocated_self.c_str(), s_original_self.c_str());
}

// Unpacks a build over the existing install. Files the new version no longer ships are left behind
// rather than deleted, which is the tradeoff for not needing a file manifest. The archive has to
// be rooted at the install directory, i.e. built from inside it rather than from its parent.
bool InstallArchive(const Options& opts, const std::string& package_path,
                    const std::string& temp_dir)
{
  const std::string source_path = StagingPath(opts, temp_dir);

  // Anything left over from an interrupted attempt would get mixed into this one.
  if (File::IsDirectory(source_path))
    File::DeleteDirRecursively(source_path);
  if (!File::CreateFullPath(source_path + DIR_SEP))
  {
    LogToFile("Could not create %s.\n", source_path.c_str());
    return false;
  }
  Common::ScopeGuard staging_guard{[&] { File::DeleteDirRecursively(source_path); }};

  UI::SetDescription("Extracting update...");
  UI::SetTotalMarquee(false);
  UI::SetCurrentMarquee(true);
  if (!ExtractPackage(package_path, source_path))
    return false;

  // Make sure this really is a build before letting any of it near the install directory.
#ifdef __APPLE__
  if (!File::IsDirectory(source_path + DIR_SEP + MAC_APP_BUNDLE))
  {
    LogToFile("Package does not contain %s, aborting.\n", MAC_APP_BUNDLE);
    return false;
  }
#elif defined(_WIN32)
  // A genuine portable build ships build_info.txt next to the executable. Some releases have
  // attached a zip that merely wraps the installer instead; unpacking one of those over the
  // install directory would just leave junk behind.
  if (!File::Exists(source_path + DIR_SEP + "build_info.txt"))
  {
    LogToFile("Package does not look like a portable build (no build_info.txt), aborting.\n");
    return false;
  }
#endif

  UI::SetDescription("Checking platform...");
  if (!Platform::VersionCheck(opts.install_base_path, source_path))
    return false;

  UI::SetDescription("Installing update...");
  UI::SetTotalMarquee(true);
  UI::SetCurrentMarquee(true);
  if (!MakeWayForNewUpdater())
    return false;

#ifdef __APPLE__
  if (!SwapInPackage(source_path, opts.install_base_path))
    return false;
#else
  if (!File::Copy(source_path, opts.install_base_path, true))
  {
    LogToFile("Could not copy %s over %s.\n", source_path.c_str(),
              opts.install_base_path.c_str());
    return false;
  }
#endif
  return true;
}

#ifdef _WIN32
// Runs the release's NSIS installer silently, pointed at the directory we are already installed in.
// /D is only honoured because Installer.nsi restores $INSTDIR after MULTIUSER_INIT clobbers it.
bool InstallWithInstaller(const Options& opts, const std::string& package_path)
{
  UI::SetDescription("Installing update...");
  UI::SetTotalMarquee(true);
  UI::SetCurrentMarquee(true);

  if (!MakeWayForNewUpdater())
    return false;

  std::wstring install_dir = UTF8ToWString(opts.install_base_path);
  while (!install_dir.empty() && (install_dir.back() == L'\\' || install_dir.back() == L'/'))
    install_dir.pop_back();

  // /D must come last, must not be quoted, and must not end in a separator.
  const std::wstring installer = UTF8ToWString(package_path);
  std::wstring command_line = L"\"" + installer + L"\" /S /D=" + install_dir;
  LogToFile("Running installer: %s\n", WStringToUTF8(command_line).c_str());

  // Without this, a package that isn't a runnable executable makes Windows put up its own modal
  // error box behind our window instead of letting CreateProcessW just fail.
  DWORD previous_error_mode = 0;
  const bool error_mode_set = SetThreadErrorMode(SEM_FAILCRITICALERRORS, &previous_error_mode);
  Common::ScopeGuard error_mode_guard{[&] {
    if (error_mode_set)
      SetThreadErrorMode(previous_error_mode, nullptr);
  }};

  STARTUPINFOW startup_info{.cb = sizeof(startup_info)};
  PROCESS_INFORMATION process_info;
  if (!CreateProcessW(installer.c_str(), command_line.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      nullptr, &startup_info, &process_info))
  {
    LogToFile("Could not start the installer: %s\n", Common::GetLastErrorString().c_str());
    return false;
  }
  CloseHandle(process_info.hThread);

  WaitForSingleObject(process_info.hProcess, INFINITE);
  DWORD exit_code = 1;
  const bool has_exit_code = GetExitCodeProcess(process_info.hProcess, &exit_code);
  CloseHandle(process_info.hProcess);

  if (!has_exit_code || exit_code != 0)
  {
    LogToFile("Installer exited with code %lu.\n", exit_code);
    return false;
  }
  return true;
}
#endif

bool InstallPackage(const Options& opts, const std::string& package_path,
                    const std::string& temp_dir)
{
  if (opts.package_filename.ends_with(".zip"))
    return InstallArchive(opts, package_path, temp_dir);

#ifdef _WIN32
  if (opts.package_filename.ends_with(".exe"))
    return InstallWithInstaller(opts, package_path);
#endif

  LogToFile("Don't know how to install %s.\n", opts.package_filename.c_str());
  return false;
}

std::optional<Options> ParseCommandLine(std::vector<std::string>& args)
{
  using optparse::OptionParser;

  OptionParser parser =
      OptionParser().prog("Project Rio Updater").description("Project Rio Updater binary");

  parser.add_option("--package-url")
      .dest("package-url")
      .help("URL of the release asset to install.")
      .metavar("URL");
  parser.add_option("--package-filename")
      .dest("package-filename")
      .help("Filename of the release asset. Its extension decides how it gets installed.")
      .metavar("NAME");
  parser.add_option("--package-digest")
      .dest("package-digest")
      .help("(optional) \"sha256:<hex>\" digest the downloaded package must match.")
      .metavar("DIGEST");
  parser.add_option("--install-base-path")
      .dest("install-base-path")
      .help("Base path of the Project Rio install to be updated.")
      .metavar("PATH");
  parser.add_option("--binary-to-restart")
      .dest("binary-to-restart")
      .help("Binary to restart after the update is over.")
      .metavar("PATH");
  parser.add_option("--log-file")
      .dest("log-file")
      .help("File where to log updater debug output.")
      .metavar("PATH");
  parser.add_option("--parent-pid")
      .dest("parent-pid")
      .type("int")
      .help("(optional) PID of the parent process. The updater will wait for this process to "
            "complete before proceeding.")
      .metavar("PID");

  optparse::Values options = parser.parse_args(args);

  Options opts;

  // Required arguments.
  std::vector<std::string> required{"package-url", "package-filename", "install-base-path"};
  for (const auto& req : required)
  {
    if (!options.is_set(req) || options[req].empty())
    {
      parser.print_help();
      return {};
    }
  }
  opts.package_url = options["package-url"];
  opts.package_filename = options["package-filename"];
  opts.install_base_path = options["install-base-path"];

  // The filename is used to build a path in our temp directory, so make sure it is only a name.
  if (opts.package_filename.find('/') != std::string::npos ||
      opts.package_filename.find('\\') != std::string::npos)
  {
    return {};
  }
  // The URL comes from the GitHub API, which only ever serves assets over https from github.com.
  if (!opts.package_url.starts_with("https://github.com/"))
    return {};

  // Optional arguments.
  if (options.is_set("package-digest"))
    opts.package_digest = options["package-digest"];
  if (options.is_set("binary-to-restart"))
    opts.binary_to_restart = options["binary-to-restart"];
  if (options.is_set("parent-pid"))
    opts.parent_pid = static_cast<u32>(options.get("parent-pid"));
  if (options.is_set("log-file"))
    opts.log_file = options["log-file"];

  return opts;
}
}  // namespace

bool RunUpdater(std::vector<std::string> args)
{
  std::optional<Options> maybe_opts = ParseCommandLine(args);

  if (!maybe_opts)
  {
    return false;
  }

  UI::Init();
  UI::SetVisible(false);

  Common::ScopeGuard ui_guard{[] { UI::Stop(); }};
  Options opts = std::move(*maybe_opts);

  if (opts.log_file)
  {
    if (!log_file.Open(opts.log_file.value(), "w"))
      log_file.SetHandle(stderr);
    else
      atexit(FlushLog);
  }

  LogToFile("Updating to:  %s\n", opts.package_filename.c_str());
  LogToFile("Install path: %s\n", opts.install_base_path.c_str());

  if (!File::IsDirectory(opts.install_base_path))
  {
    FatalError("Cannot find install base path, or not a directory.");
    return false;
  }

  if (opts.parent_pid)
  {
    LogToFile("Waiting for parent PID %d to complete...\n", *opts.parent_pid);

    auto pid = opts.parent_pid.value();

    UI::WaitForPID(static_cast<u32>(pid));

    LogToFile("Completed! Proceeding with update.\n");
  }

  UI::SetVisible(true);

  const std::string temp_dir = File::CreateTempDir();
  if (temp_dir.empty())
  {
    FatalError("Could not create temporary directory. Aborting.");
    return false;
  }
  Common::ScopeGuard temp_dir_guard{[&] { File::DeleteDirRecursively(temp_dir); }};

  const std::string package_path = temp_dir + DIR_SEP + opts.package_filename;
  if (!DownloadPackage(opts, package_path))
  {
    FatalError("Failed to download the update.");
    return false;
  }

  if (!InstallPackage(opts, package_path, temp_dir))
  {
    RestoreUpdater();
    FatalError("Failed to apply the update.");
    return false;
  }

  UI::ResetCurrentProgress();
  UI::ResetTotalProgress();
  UI::SetCurrentMarquee(false);
  UI::SetTotalMarquee(false);
  UI::SetCurrentProgress(1, 1);
  UI::SetTotalProgress(1, 1);
  UI::SetDescription("Done!");

  // Let the user process that we are done.
  UI::Sleep(1);

  if (opts.binary_to_restart)
  {
    UI::LaunchApplication(opts.binary_to_restart.value());
  }

  return true;
}
