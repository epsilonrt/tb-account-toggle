#include "thunderbird.h"
#include "tberror.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSettings>

namespace Thunderbird {

QString defaultProfilePath()
{
    const QStringList roots = profileRoots();
    for (const QString &rootPath : roots) {
        const QDir root(rootPath);
        const QString iniPath = root.filePath(QStringLiteral("profiles.ini"));
        if (!QFile::exists(iniPath))
            continue;
        QSettings ini(iniPath, QSettings::IniFormat);

        // [Install...] sections hold the profile actually used by the installation
        const QStringList groups = ini.childGroups();
        for (const QString &g : groups) {
            if (g.startsWith(u"Install") && ini.contains(g + QStringLiteral("/Default")))
                return QDir::cleanPath(root.filePath(ini.value(g + QStringLiteral("/Default")).toString()));
        }
        for (const QString &g : groups) {
            if (g.startsWith(u"Profile") && ini.value(g + QStringLiteral("/Default")).toString() == u"1") {
                const QString path = ini.value(g + QStringLiteral("/Path")).toString();
                const bool relative = ini.value(g + QStringLiteral("/IsRelative"), QStringLiteral("1")).toString() == u"1";
                return QDir::cleanPath(relative ? root.filePath(path) : path);
            }
        }
        throw TbError(QCoreApplication::translate("Thunderbird", "Default profile not found in %1")
                          .arg(QDir::toNativeSeparators(iniPath)));
    }

    QStringList shown;
    for (const QString &r : roots)
        shown << QDir::toNativeSeparators(r);
    throw TbError(QCoreApplication::translate("Thunderbird", "Thunderbird profile not found (profiles.ini searched in: %1)")
                      .arg(shown.join(QStringLiteral(", "))));
}

bool launch(const Launcher &launcher)
{
    if (!launcher.isValid())
        return false;
    // Detached, and without Thunderbird's diagnostics in this tool's terminal
    QProcess process;
    process.setProgram(launcher.program);
    process.setArguments(launcher.arguments);
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    return process.startDetached();
}

} // namespace Thunderbird
