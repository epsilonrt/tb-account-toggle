#pragma once

#include <QString>

// Thunderbird profile and process helpers (Windows).
namespace Thunderbird {

// Profile Thunderbird starts with, as declared in profiles.ini. Throws TbError.
QString defaultProfilePath();

bool isRunning();

// Ask Thunderbird to close, like a click on its close button (never forced).
void requestClose();

// Path of thunderbird.exe (running process, registry, then Program Files), or empty.
QString executablePath();

bool launch(const QString &exe);

} // namespace Thunderbird
