# tb-account-toggle

A small Windows utility to **temporarily disable Thunderbird mail accounts** and enable
them again, from a window with one switch per account.

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
- Thunderbird must be closed to apply changes: the tool offers to close it gracefully,
  then restarts it.
- `prefs.js` is backed up (`prefs.js.bak-<timestamp>`) before every change.
- Reactivation data is stored in `prefs.js` itself
  (`mail.accountmanager.disabled_accounts` and `mail.accountmanager.accounts_order`) and
  removed once no account is disabled.

`--profile <folder>` works on another profile folder (for instance a copy, to try it
out); Thunderbird is then never closed or restarted.

The user interface follows the system language: English, or French
(`translations/tb-account-toggle_fr.ts`). The installer is available in both languages
too.

## Installation

Run `tb-account-toggle-setup-<version>.exe`. It installs for the current user, without
administrator rights, in `%LOCALAPPDATA%\Programs\tb-account-toggle`.

## Building

Requirements: CMake 3.27 or later, Ninja, Qt 6.6 or later with a compiler matching
your Qt build, and Inno Setup 6 for the Windows installer.

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build
cmake --build build --target installer  # installer in build/ (same as "package")
```

On Windows, with Qt installed by the Qt online installer and its `bin` folder on `PATH`,
nothing else is needed: the build picks the Qt MinGW kit (`<Qt>/<version>/mingw_64`)
and the MinGW compiler and Ninja shipped in `<Qt>/Tools`, even if another compiler
comes first on `PATH`. The `QT_ROOT` and `QT_KIT_VERSION` cache variables select another
Qt installation or version. Standard CMake variables (`CMAKE_PREFIX_PATH`,
`CMAKE_CXX_COMPILER`) still take precedence. Visual Studio generators (MSVC) are not
supported; with the VS Code CMake Tools extension, the workspace setting
`"cmake.generator": "Ninja"` takes care of it.

### Translations

Source strings are in English and go through `tr()`. After changing them, refresh the
catalogs with `cmake --build build --target update_translations`, then edit the `.ts`
files with Qt Linguist. Catalogs are compiled and embedded in the executable at build
time, together with Qt's own `qtbase` translation. To add a language, add its `.ts`
file to `qt_add_translations()` in `CMakeLists.txt`.

Note: the application itself currently targets Windows (Win32 API to manage the
Thunderbird process).

## License

[BSD 3-Clause](LICENSE)
