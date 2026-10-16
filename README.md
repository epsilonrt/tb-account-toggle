# tb-account-toggle

A small utility for Windows, Linux and macOS to **temporarily disable Thunderbird mail
accounts** and enable them again, from a window with one switch per account.

Thunderbird cannot disable an IMAP account: even with automatic mail checks turned off,
it reconnects as soon as you click one of the account's folders (see
[bug 689067](https://bugzilla.mozilla.org/show_bug.cgi?id=689067)).
`tb-account-toggle` removes the account from the list Thunderbird loads
(`mail.accountmanager.accounts` in `prefs.js`), so no connection is made at all.
The account's settings and its cached messages are kept, and re-enabling it restores it
exactly as it was.

## How it works

- One switch per account. The default account cannot be disabled.
- A disabled account does not appear in Thunderbird until it is enabled again.
- Thunderbird must be closed to apply changes. On Windows and macOS the tool offers to
  close it gracefully; on Linux, where no clean way exists from another program, it asks
  you to close it and waits. Thunderbird is then restarted.
- `prefs.js` is backed up (`prefs.js.bak-<timestamp>`) before every change.
- Reactivation data is stored in `prefs.js` itself
  (`mail.accountmanager.disabled_accounts` and `mail.accountmanager.accounts_order`) and
  removed once no account is disabled.

The default Thunderbird profile is looked up in:

| System | Folder |
|---|---|
| Windows | `%APPDATA%\Thunderbird` |
| Linux | `~/.thunderbird`, Snap `~/snap/thunderbird/common/.thunderbird`, Flatpak `~/.var/app/org.mozilla.Thunderbird/.thunderbird` |
| macOS | `~/Library/Thunderbird` |

`--profile <folder>` works on another profile folder (for instance a copy, to try it
out); Thunderbird is then never closed or restarted.

The user interface follows the system language: English, or French
(`translations/tb-account-toggle_fr.ts`). The Windows installer is available in both
languages too.

## Installation

Packages are attached to each [release](https://github.com/epsilonrt/tb-account-toggle/releases):

- **Windows**: run `tb-account-toggle-setup-<version>.exe`. It installs for the current
  user, without administrator rights, in `%LOCALAPPDATA%\Programs\tb-account-toggle`.
- **Debian 13 (trixie) and derivatives**: `sudo apt install ./tb-account-toggle_<version>_amd64.deb`.
- **macOS**: open `tb-account-toggle-<version>-macos.dmg` and drag the application to
  *Applications*. The application is not signed: the first time, open it with a right
  click > *Open*.

## Building

Requirements: CMake 3.27 or later, Ninja, Qt 6.7 or later with a compiler matching
your Qt build, and, for the Windows installer, Inno Setup 6.

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
cmake --build build --target installer  # package in build/ (same as "package")
```

The package is an Inno Setup installer on Windows, a disk image on macOS and a Debian
package on Linux.

On Windows, with Qt installed by the Qt online installer and its `bin` folder on `PATH`,
nothing else is needed: the build picks the Qt MinGW kit (`<Qt>/<version>/mingw_64`)
and the MinGW compiler and Ninja shipped in `<Qt>/Tools`, even if another compiler
comes first on `PATH`. The `QT_ROOT` and `QT_KIT_VERSION` cache variables select another
Qt installation or version. Standard CMake variables (`CMAKE_PREFIX_PATH`,
`CMAKE_CXX_COMPILER`) still take precedence. Visual Studio generators (MSVC) are not
supported; with the VS Code CMake Tools extension, the workspace setting
`"cmake.generator": "Ninja"` takes care of it.

On Linux, the distribution's Qt is used. On Debian 13:

```sh
sudo apt install cmake ninja-build g++ dpkg-dev qt6-base-dev qt6-tools-dev \
  qt6-tools-dev-tools qt6-l10n-tools qt6-translations-l10n
```

The GitHub Actions workflows (`.github/workflows/ci_build_*.yml`) build, test and package
on the three systems, and publish the packages when a `v*` tag is pushed.

### Translations

Source strings are in English and go through `tr()`. After changing them, refresh the
catalogs with `cmake --build build --target update_translations`, then edit the `.ts`
files with Qt Linguist. Catalogs are compiled and embedded in the executable at build
time, together with Qt's own `qtbase` translation. To add a language, add its `.ts`
file to `qt_add_translations()` in `CMakeLists.txt`.

## License

[BSD 3-Clause](LICENSE)
