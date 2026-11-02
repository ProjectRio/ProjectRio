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
to a **draft** GitHub release under the names below. The same job refuses to continue unless
Source/Core/UpdaterCommon/UpdateKeys.h holds two to five production keys, and no workflow or build
script enables the updater's test key.

CI can't sign, so the draft can't install anywhere yet. On a maintainer's own machine:

1. Download every asset of the draft to an empty folder, and try them.
2. Sign them, which writes `update_manifest.json` and `update_manifest.sig` into that folder:
   `python Tools/sign-release.py sign --dir <folder> --version 2.3.1 --key <private key file>`.
   The version has no leading `v`. To co-sign, run the same command with a second key on a folder
   that already has both files. The manifest is left alone and a line is added to the signatures.
3. Check the result:
   `python Tools/sign-release.py verify --dir <folder> --pubkey <hex>`.
4. Upload both files to the draft.

Then write the notes (they are shown in the update prompt) and publish it. Installs only see a
release once it is published and not marked as a pre-release.

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
* `update_manifest.json` and `update_manifest.sig`, which the release job does **not** take care
  of: Tools/sign-release.py writes them on a maintainer's machine (see "Publishing a release"). A
  release without both still prompts, but only points the user at the Rio website.

### Signing
`update_manifest.json` lists the size and sha256 of every release asset except the two signature
files, plus the version, the product (`ProjectRio`), and the manifest format (`1`).
`update_manifest.sig` has one `<key id> <base64 signature>` line per signature, up to 8, where the
key id is the first 16 hex characters of the public key and the signature is Ed25519 over the exact
bytes of the manifest. One signature from a key the build trusts is enough, and lines with an
unknown key id are ignored.

The updater checks these in order. Any failure ends the update with nothing changed:

1. Download `update_manifest.sig` and `update_manifest.json` into memory, at most 64 KiB each.
2. Verify a signature over the manifest's raw bytes.
3. Parse the JSON.
4. Require format `1` and product `ProjectRio`.
5. Require the manifest's version to equal `--package-version`, ignoring a leading `v`.
6. Require the manifest's version to be strictly newer than the `RIO_REV_STR` the updater was built
   with, so an old signed release can't be served as a new one.
7. Require `--package-filename` to be listed in the manifest.
8. Download the package, and check its size and sha256 before writing it to disk.

The checks live in UpdaterCommon/UpdateManifest.cpp and in VerifyRelease() in
UpdaterCommon/UpdaterCommon.cpp. A failure is logged to `Updater.log` with its specific reason, and
the user sees "Could not verify this update, so nothing was changed."

### Keys
UpdaterCommon/UpdateKeys.h lists the public keys a build trusts. Every maintainer who signs releases
has their own key, with two to five in the list.

* Create a key with `python Tools/sign-release.py keygen --out <file>`. It prints the entry to add
  to `UPDATE_PUBLIC_KEYS`, and public keys aren't secret. Update the array size to match.
* Keep the private key offline, encrypted with a passphrase, with a separate physical backup. It
  never goes in the repo, cloud storage, or chat, and CI never gets one.
* Adding a maintainer: add their public key in a normal release. It is trusted once users have
  installed that release.
* A lost key: another maintainer signs the next release, which removes the old key and adds a new
  one.
* A stolen key: another maintainer ships a release that removes it right away. Users are only
  protected after installing that release, since a client trusts the keys it was built with.

A build with no keys can't install any update, so the keys have to be in the code before a tag is
pushed.

### What signing doesn't cover
* A stolen key can sign a fake release until a replacement release reaches users.
* A maintainer can't detect a malicious binary by signing it. Signing proves who released a file,
  not that the build was clean, which is why pinning CI actions still matters.
* The update prompt shows release notes before verification, because they come from the GitHub API.
  They're only displayed, so they can't install anything, but a fake release could show misleading
  text.

## Class and file responsibilities:
* AutoUpdateChecker (UICommon/AutoUpdate.h): Checks if an update is available.
    * Deletes the leftover updater from a previous update.
    * Fetches `/releases/latest` from the GitHub API and compares the tag against this build.
    * Picks the release asset matching this platform and verifies its URL is a GitHub release
      asset URL. Does the same for `update_manifest.json` and `update_manifest.sig`, and offers no
      install if either is missing.
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
    * Verifies the release's signature and manifest, then downloads the release asset and verifies
      its size and sha256 against the manifest.
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
    * If there is no updater, no asset for this platform, or no signature files, it just points at
      the Rio website.
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
    * Downloads the signature and manifest, and runs the first seven checks listed under
      "Signing".
    * Downloads the release asset, verifies its size and sha256 against the manifest, and only then
      writes it to a temporary directory.
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
2. Sign them with the throwaway test key, using `sign-release.py sign` as in "Publishing a
   release". Add `--allow-missing-assets` to sign a test release that doesn't have every platform's
   asset, which `sign` otherwise refuses. Production keys are never used for this.
3. Publish them, with both signature files, as a release tagged with that version on any **public**
   GitHub repo. It does not have to be a fork (GitHub won't fork ProjectRio into an account that
   already has a Dolphin fork), and it must not be marked as a pre-release, which
   `/releases/latest` skips.
4. Make a build with a lower `RIO_REV_STR` that trusts the test key: pass
   `-DRIO_UPDATER_TEST_KEYS=ON` to CMake, or `/p:RioUpdaterTestKeys=true` to MSBuild. Such a build
   trusts only the test key and must never be published, and a build without the option can't
   accept a release signed with it. Run it with `RIO_UPDATE_REPO=<owner>/<repo>` set. That
   redirects both the release check and the URL prefix downloads are allowed from. Use a copy of
   the install: on Windows the installer asset rewrites the Start menu/desktop shortcuts and the
   Add/Remove Programs entry even when it installs into another folder.
5. Choose "Install Update". Rio should close, the updater should download, verify and install,
   and Rio should come back reporting the new version without prompting again. `Updater.log` in
   the user folder's `Logs` directory records each step, including the reason for any failure.

On Windows the updater prefers the zip when a release has both; leave it off the release to
exercise the installer fallback.

The updater can also be run directly:

```
Updater.exe --package-url=<asset url> --package-filename=<asset name> \
            --package-version=<version> --manifest-url=<update_manifest.json url> \
            --manifest-signature-url=<update_manifest.sig url> \
            --install-base-path=<dir> --log-file=<path>
```

It refuses any URL that isn't a `https://github.com/` one, and a release that doesn't pass the
checks under "Signing". Add `--parent-pid` to make it wait for
a process to exit first, and `--binary-to-restart` to have it relaunch Rio afterwards.
