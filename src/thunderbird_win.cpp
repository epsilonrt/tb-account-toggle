// Windows implementation of the Thunderbird process helpers (Win32 API)
#include "thunderbird.h"

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QSettings>

#include <windows.h>
#include <tlhelp32.h>

namespace {

QList<DWORD> thunderbirdPids()
{
    QList<DWORD> pids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return pids;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        if (_wcsicmp(pe.szExeFile, L"thunderbird.exe") == 0)
            pids.append(pe.th32ProcessID);
    CloseHandle(snap);
    return pids;
}

QString processImagePath(DWORD pid)
{
    QString path;
    if (HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        wchar_t buf[MAX_PATH * 2];
        DWORD size = DWORD(std::size(buf));
        if (QueryFullProcessImageNameW(h, 0, buf, &size))
            path = QString::fromWCharArray(buf, int(size));
        CloseHandle(h);
    }
    return path;
}

} // namespace

namespace Thunderbird {

QStringList profileRoots()
{
    return {qEnvironmentVariable("APPDATA") + QStringLiteral("/Thunderbird")};
}

bool isRunning()
{
    return !thunderbirdPids().isEmpty();
}

bool canRequestClose()
{
    return true;
}

void requestClose()
{
    const QList<DWORD> pids = thunderbirdPids();
    EnumWindows(
        [](HWND hwnd, LPARAM lp) -> BOOL {
            const auto *pids = reinterpret_cast<const QList<DWORD> *>(lp);
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            // Main windows only: visible, top-level, not owned by another window
            if (pids->contains(pid) && IsWindowVisible(hwnd) && !GetWindow(hwnd, GW_OWNER))
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&pids));
}

Launcher launcher(const QString &)
{
    // Running process first, then the registry, then the default install folders
    for (DWORD pid : thunderbirdPids())
        if (const QString path = processImagePath(pid); !path.isEmpty())
            return {path, {}};

    for (const char *hive : {"HKEY_LOCAL_MACHINE", "HKEY_CURRENT_USER"}) {
        QSettings reg(QString::fromLatin1(hive) + QStringLiteral(R"(\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\thunderbird.exe)"),
                      QSettings::NativeFormat);
        const QString path = reg.value(QStringLiteral("Default")).toString().remove(u'"');
        if (!path.isEmpty() && QFileInfo(path).isFile())
            return {path, {}};
    }

    for (const char *var : {"ProgramFiles", "ProgramFiles(x86)"}) {
        const QString path = qEnvironmentVariable(var) + QStringLiteral("/Mozilla Thunderbird/thunderbird.exe");
        if (QFileInfo(path).isFile())
            return {QDir::toNativeSeparators(path), {}};
    }
    return {};
}

} // namespace Thunderbird
