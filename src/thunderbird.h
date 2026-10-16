#pragma once

#include <QString>
#include <QStringList>

// Thunderbird profile and process helpers.
// The profile lookup is shared (thunderbird.cpp); process handling is implemented per
// platform (thunderbird_win.cpp, thunderbird_unix.cpp).
namespace Thunderbird {

// Command that starts Thunderbird
struct Launcher
{
    QString program;
    QStringList arguments;
    bool isValid() const { return !program.isEmpty(); }
};

// Folders that may contain profiles.ini on this system, most likely first
QStringList profileRoots();

// Profile Thunderbird starts with, as declared in profiles.ini. Throws TbError.
QString defaultProfilePath();

bool isRunning();

// Whether requestClose() can close Thunderbird cleanly on this system. On Linux there
// is no such way (SIGTERM kills it without a regular shutdown, Wayland forbids closing
// another application's window): the user closes it.
bool canRequestClose();

// Ask Thunderbird to close, like a click on its close button (never forced).
void requestClose();

// How to start Thunderbird for this profile (Flatpak, Snap, installed binary...).
// Call it while Thunderbird is still running, so that its location can be read.
Launcher launcher(const QString &profileDir);

bool launch(const Launcher &launcher);

} // namespace Thunderbird
