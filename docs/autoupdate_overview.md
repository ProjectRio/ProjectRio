# AutoUpdate Overview
Project Rio updates itself straight from the GitHub releases page of
[ProjectRio/ProjectRio](https://github.com/ProjectRio/ProjectRio). This overview describes the
update flow, which is spread across a number of files.

## General notes:
* Update *checks* run on every platform. Actually *installing* an update needs the separate
  updater application, which is built and shipped on Windows and macOS. Where it is missing (a
  Linux AppImage, a Flatpak, or a build made without it), the user is told a new version exists
  and is pointed at the Project Rio website instead.
* The tag name of the latest GitHub release is the version number, compared against
  `RIO_REV_STR` in Common/Version.cpp. Tags are ordered numerically ("2.10.0" > "2.9.1"), so
  running a build newer than the latest release never prompts.
* The release asset to install is picked from the release's asset list by matching the filename
  against the platform (see `ScoreAssetName` in UICommon/AutoUpdate.cpp). On Windows the zip of
  the portable build is preferred: unpacking it needs no elevation and leaves shortcuts and the
  uninstall entry alone, so a portable copy stays portable and an installed copy stays installed.
  The NSIS installer is only used to update when a release has no zip.
* Each install keeps `install_manifest.txt`, the list of files its version shipped. After
  unpacking a zip the updater deletes whatever is on the old list but not in the new package, so
  files a version stops shipping don't accumulate; files the user added are on neither list and
  are left alone. The release build writes the list into the package, so installer-made installs
  have one too.
* Applications can't overwrite themselves so a separate application is responsible for actually
  updating the Rio executable and other files. The updater can't overwrite itself either, so on
  Windows it renames itself out of the way before installing and on macOS Rio runs a copy of it;
  either way Rio deletes the leftover on its next launch.
* The install base path is the directory the updater unpacks a package over. On Windows that is
  the directory holding the executable. On macOS `File::GetExePath()` returns the bundle itself,
  so it is the directory *containing* `ProjectRio.app` — which is also where `Project Rio Updater.app`
  lives, and why both have to be in the archive.

## Publishing a release
Set `RIO_REV_STR` in Common/Version.cpp to the new version, merge, then push a tag with exactly
that version (`2.3.0`, or `v2.3.0`). The "Draft release" job in .github/workflows/main.yml builds
every platform, refuses to continue if the tag and `RIO_REV_STR` differ, and attaches the builds
to a **draft** GitHub release under the names below. Review the draft, write the notes (they are
shown in the update prompt), and publish it. Installs only see a release once it is published and
not marked as a pre-release.

## Release requirements
The release job takes care of these. For auto-update to work, a release needs:
* A tag name that is just the version number, e.g. `2.3.0` (a leading `v` is also accepted). It has
  to match `RIO_REV_STR` in Common/Version.cpp in the build being released — if the tag is bumped
  but `RIO_REV_STR` isn't, the updated build still reports the old version and prompts to install
  the same release on every launch.
* Two Windows assets whose names contain "win" (any casing): the zip of the portable build, e.g.
  `Project_Rio_Windows.zip`, which is what installs update from, and the NSIS installer built from
  Installer/Installer.nsi, e.g. `Project_Rio_Windows_Installer.exe`, for first-time installs.
* A macOS asset per architecture, named so that it contains "mac" plus one of "arm"/"silicon"/
  "apple" or "intel"/"x86"/"x64", e.g. `Project_Rio_macOS-Apple_silicon.zip`. It has to be the zip
  the build produces, holding **both** `ProjectRio.app` and `Project Rio Updater.app` at its root —
  the updater lives beside the app, not inside it, and the archive is unpacked over the directory
  containing them.
* Assets must be the archive itself, not a GitHub Actions artifact download (which wraps whatever
  you built in another zip). Every macOS asset published so far has been double-zipped this way,
  and the updater cannot see through that.
* The Windows zip has to contain the actual build (the files that sit next to the executable,
  including `build_info.txt` and `install_manifest.txt`), not an installer wrapped in a zip. The
  updater refuses a zip without `build_info.txt` rather than unpacking junk over the install.

GitHub reports a `sha256` digest for each asset, which the updater checks after downloading. That
is not a signature — it arrives in the same TLS-protected API response as the download URL — but it
catches a corrupted or truncated download before anything is unpacked or executed.

## Class and file responsibilities:
* AutoUpdateChecker (UICommon/AutoUpdate.h): Checks if an update is available.
    * Deletes the leftover updater from a previous update.
    * Fetches `/releases/latest` from the GitHub API and compares the tag against this build.
    * Picks the release asset matching this platform and verifies its URL is a GitHub release
      asset URL.
    * Launches the updater application with the chosen asset.
* Updater (DolphinQt/Updater.h): Serves as the interface between AutoUpdateChecker and Qt.
    * Spawns a background thread when Rio launches that calls AutoUpdateChecker.
    * Converts the release's Markdown body to rich text and creates the update prompt window.
    * If the user wants to update now, closes Rio.
* MacUpdater/main.m and MacUpdater/AppDelegate.mm: Entry point to the macOS updater.
    * Converts command line arguments to vector\<string\> and passes them to
      UpdaterCommon::RunUpdate().
* MacUpdater/MacUI.mm: The updater's macOS UI, plus its platform check.
    * Reads `LSMinimumSystemVersion` out of the new bundle's Info.plist and refuses the update if
      this Mac is running something older.
* WinUpdater/main.cpp: Entry point to Windows updater.
    * Converts command line arguments to vector\<string\>.
    * Checks if updater has write access to the Rio executable directory. If not, attempts
      to relaunch itself with admin privileges (creating a User Account Control prompt).
    * Passes argument vector to RunUpdater() in UpdaterCommon.h.
* WinUpdater/Platform.cpp: Checks that the machine can run the new build.
    * Compares the Windows version and the installed VC++ runtime against the `build_info.txt`
      shipped inside the package, and installs the VC++ redistributable if it is too old.
* UpdaterCommon/UpdaterCommon.cpp: Performs the actual update process.
    * Manages updater UI.
    * Downloads the release asset and verifies its sha256 against the digest from the API.
    * Installs it. On Windows a `.zip` is unpacked and copied over the install directory, files
      the previous version shipped that this one doesn't are deleted, and the version in the
      uninstall entry (if the install has one) is updated. An `.exe` is run as a silent NSIS
      install against the current install directory, without elevation and in the install's own
      per-user or all-users mode. On macOS the zip is unpacked
      with `ditto` (our own unzip restores neither the symlinks holding the framework layout
      together nor the permission bits, which leaves a bundle macOS calls damaged), and each
      bundle in it is then moved into place with a rename — replacing a Mach-O's bytes in place
      makes the kernel check the new binary against the cached old code signature and kill it.
    * If the user updated immediately (rather than waiting for Rio to close before starting
      the update), starts Rio again when the update is complete.
* Installer/Installer.nsi: The NSIS installer, also used as the update package on Windows.
    * `.onInit` stashes `$INSTDIR` across `MULTIUSER_INIT`, which would otherwise overwrite the
      directory given on the command line. Without that, the updater's `/S /D=<install dir>` is
      silently ignored and the update lands in a second, default location.

## Update flow:
* An update check is started in one of two ways:
    * When Rio is launched (unless in NoGUI or batch mode):
         * In main.cpp an instance of Updater is created and invokes start(), which is inherited
           from QThread and creates a new thread which performs the check off the main thread.
         * QThread::start() calls run() which is overridden in Updater and calls
           AutoUpdateChecker::CheckForUpdate().
         * The check is skipped if the update track config value is empty, which is what the
           "Check for Updates on Startup" setting and the "Never Auto-Update" button clear.
    * When the user selects Help -> "Check for Updates..." in the main menu:
         * The menu option runs a callback to MenuBar::InstallUpdateManually().
         * Updater::CheckForUpdate() is called, which calls AutoUpdateChecker::CheckForUpdate()
           with CheckType::Manual. A manual check runs even when the startup check is off, and
           reports when there is nothing to install.
* AutoUpdateChecker::CheckForUpdate() checks if the latest release is newer than this build.
    * If not the check ends. If the user started it, they are told they are up to date.
* Information about the update is passed to OnUpdateAvailable(), which is overridden by Updater.
* OnUpdateAvailable() creates a window displaying the release notes.
    * If there is no updater or no asset for this platform, it just points at the Rio website.
    * Otherwise it asks the user if they want to update now, update after Rio closes, not update,
      or never auto-update.
* If the user wants to update AutoUpdateChecker::TriggerUpdate() is called.
* TriggerUpdate() builds the command line arguments for the updater process and runs it.
* TriggerUpdate() returns to OnUpdateAvailable(). If the user chose to update now, Rio's
  main window is closed which results in the Rio process ending.
* The updater process begins.
    * On macOS (starts main() in MacUpdater/main.m):
         * Checks that the process received command line arguments. If not it tells the user the
           updater can't be launched directly and quits.
         * Calls NSApplicationMain(), which passes control to the AppDelegate defined in
           MacUpdater/AppDelegate.mm.
         * The command line arguments are converted to a vector\<string\>
           and passed to RunUpdater() in UpdaterCommon.h.
    * On Windows (starts wWinMain() in WinUpdater/Main.cpp):
         * Checks that the process received command line arguments. If not it tells the user the
           updater can't be launched directly and quits.
         * Attempts to open Updater.log in the same directory as the Rio executable. If this
           fails, checks to see if the process has admin privileges.
             * If not, attempts to relaunch the updater as admin. This will spawn a User Account
               Control prompt.
             * If the user declines the UAC prompt, or if the updater already has admin status,
               the update aborts.
         * Converts the command line arguments to a vector\<string\> and passes them to RunUpdater()
           in UpdaterCommon.h.
* RunUpdater() parses and validates the command line arguments, hides the updater UI, then waits
  for the Rio process to quit.
* RunUpdater() begins the actual update.
    * Downloads the release asset into a temporary directory and verifies its digest.
    * Renames the running updater out of the way, then installs the package. If the install fails
      the updater is moved back so the next attempt still has one.
    * The temporary directory is deleted whether the update succeeded or not.
* If the user updated immediately (rather than waiting for Rio to close before starting
  the update), Rio restarts.
* As part of Rio's normal startup process, the renamed updater is deleted.

## Testing an update
The whole flow can be run against a throwaway release without touching the real repo:

1. Build with `RIO_REV_STR` raised (e.g. `9.9.9`) and package it the way a release would be: the
   NSIS installer and/or a zip of the portable build on Windows, the zip holding `ProjectRio.app`
   and `Project Rio Updater.app` on macOS. Name the assets per "Release requirements".
2. Publish them as a release tagged with that version on any **public** GitHub repo. It does not
   have to be a fork (GitHub won't fork ProjectRio into an account that already has a Dolphin
   fork), and it must not be marked as a pre-release, which `/releases/latest` skips.
3. Run a normal build (lower `RIO_REV_STR`) with `RIO_UPDATE_REPO=<owner>/<repo>` set. That
   redirects both the release check and the URL prefix downloads are allowed from. Use a copy of
   the install: on Windows the installer asset rewrites the Start menu/desktop shortcuts and the
   Add/Remove Programs entry even when it installs into another folder.
4. Choose "Install Update". Rio should close, the updater should download, verify and install,
   and Rio should come back reporting the new version without prompting again. `Updater.log` in
   the user folder's `Logs` directory records each step, including the reason for any failure.

On Windows the updater prefers the zip when a release has both; leave it off the release to
exercise the installer fallback.

The updater can also be run directly:

```
Updater.exe --package-url=<asset url> --package-filename=<asset name> \
            --package-digest=sha256:<hex> --install-base-path=<dir> --log-file=<path>
```

It refuses any URL that isn't a `https://github.com/` one. Add `--parent-pid` to make it wait for
a process to exit first, and `--binary-to-restart` to have it relaunch Rio afterwards.
