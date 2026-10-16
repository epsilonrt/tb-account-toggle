// Linux and macOS implementation of the Thunderbird process helpers
#include "thunderbird.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <sys/types.h>
#include <unistd.h>

namespace {

QString home()
{
    return QDir::homePath();
}

#ifdef Q_OS_MACOS

QList<qint64> thunderbirdPids()
{
    QProcess pgrep;
    pgrep.start(QStringLiteral("pgrep"), {QStringLiteral("-x"), QStringLiteral("-u"), QString::number(getuid()),
                                          QStringLiteral("thunderbird")});
    pgrep.waitForFinished(3000);
    QList<qint64> pids;
    for (const QByteArray &line : pgrep.readAllStandardOutput().split('\n'))
        if (const qint64 pid = line.trimmed().toLongLong(); pid > 0)
            pids.append(pid);
    return pids;
}

#else

// Field of /proc/<pid>/stat after the "(comm)" part (comm may contain spaces)
qint64 parentPid(const QString &procDir)
{
    QFile stat(procDir + QStringLiteral("/stat"));
    if (!stat.open(QIODevice::ReadOnly))
        return -1;
    const QByteArray data = stat.readAll();
    const QList<QByteArray> fields = data.mid(data.lastIndexOf(')') + 2).split(' ');
    return fields.size() > 1 ? fields.at(1).toLongLong() : -1;  // state, ppid, ...
}

// Main Thunderbird processes of the current user (child processes have other names,
// and are skipped anyway when their parent is a Thunderbird process)
QList<qint64> thunderbirdPids()
{
    QList<qint64> all;
    QList<qint64> parents;
    const QDir proc(QStringLiteral("/proc"));
    const QStringList entries = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool numeric = false;
        const qint64 pid = entry.toLongLong(&numeric);
        if (!numeric || pid == getpid())
            continue;
        const QString dir = proc.filePath(entry);
        if (QFileInfo(dir).ownerId() != getuid())
            continue;
        QFile comm(dir + QStringLiteral("/comm"));
        if (!comm.open(QIODevice::ReadOnly))
            continue;
        const QByteArray name = comm.readAll().trimmed();
        if (name == "thunderbird" || name == "thunderbird-bin") {
            all.append(pid);
            parents.append(parentPid(dir));
        }
    }
    QList<qint64> mains;
    for (qsizetype i = 0; i < all.size(); ++i)
        if (!all.contains(parents.at(i)))
            mains.append(all.at(i));
    return mains;
}

#endif

} // namespace

namespace Thunderbird {

QStringList profileRoots()
{
#ifdef Q_OS_MACOS
    return {home() + QStringLiteral("/Library/Thunderbird")};
#else
    return {
        home() + QStringLiteral("/.thunderbird"),
        home() + QStringLiteral("/snap/thunderbird/common/.thunderbird"),
        home() + QStringLiteral("/.var/app/org.mozilla.Thunderbird/.thunderbird"),
    };
#endif
}

bool isRunning()
{
    return !thunderbirdPids().isEmpty();
}

bool canRequestClose()
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

void requestClose()
{
#ifdef Q_OS_MACOS
    // Same as Thunderbird > Quit
    QProcess::execute(QStringLiteral("osascript"), {QStringLiteral("-e"), QStringLiteral("tell application \"Thunderbird\" to quit")});
#endif
    // Linux: nothing to do, see canRequestClose()
}

Launcher launcher(const QString &profileDir)
{
#ifdef Q_OS_MACOS
    Q_UNUSED(profileDir)
    return {QStringLiteral("open"), {QStringLiteral("-a"), QStringLiteral("Thunderbird")}};
#else
    // The sandboxed packages are started through their own launcher
    const QString profile = QDir::cleanPath(profileDir);
    if (profile.startsWith(home() + QStringLiteral("/.var/app/org.mozilla.Thunderbird/")))
        return {QStringLiteral("flatpak"), {QStringLiteral("run"), QStringLiteral("org.mozilla.Thunderbird")}};
    if (profile.startsWith(home() + QStringLiteral("/snap/thunderbird/")))
        return {QStringLiteral("snap"), {QStringLiteral("run"), QStringLiteral("thunderbird")}};

    if (const QString exe = QStandardPaths::findExecutable(QStringLiteral("thunderbird")); !exe.isEmpty())
        return {exe, {}};
    // Not on PATH: binary of the running process
    for (qint64 pid : thunderbirdPids())
        if (const QString exe = QFile::symLinkTarget(QStringLiteral("/proc/%1/exe").arg(pid)); !exe.isEmpty())
            return {exe, {}};
    return {};
#endif
}

} // namespace Thunderbird
