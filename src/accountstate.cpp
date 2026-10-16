#include "accountstate.h"
#include "tberror.h"
#include "thunderbird.h"

#include <QDateTime>
#include <QDir>
#include <QFile>

namespace {
const QString PrefAccounts = QStringLiteral("mail.accountmanager.accounts");
const QString PrefDefault = QStringLiteral("mail.accountmanager.defaultaccount");
const QString PrefDisabled = QStringLiteral("mail.accountmanager.disabled_accounts");
const QString PrefOrder = QStringLiteral("mail.accountmanager.accounts_order");
} // namespace

AccountState::AccountState(const QString &profileDir)
    : m_profileDir(profileDir), m_prefsFile(QDir(profileDir).filePath(QStringLiteral("prefs.js")))
{
    if (!QFile::exists(m_prefsFile))
        throw TbError(tr("prefs.js not found: %1").arg(QDir::toNativeSeparators(m_prefsFile)));
    reload();
}

void AccountState::reload()
{
    m_prefs = std::make_unique<Prefs>(m_prefsFile);
    const Prefs &p = *m_prefs;

    m_enabledIds = p.ids(PrefAccounts);
    const QString defaultId = p.get(PrefDefault);

    // Master list: saved order first, then any account added since
    m_master.clear();
    for (const QStringList &list : {p.ids(PrefOrder), m_enabledIds, p.ids(PrefDisabled)})
        for (const QString &id : list)
            if (!m_master.contains(id))
                m_master.append(id);

    m_accounts.clear();
    for (const QString &id : std::as_const(m_master)) {
        const QString server = p.get(QStringLiteral("mail.account.%1.server").arg(id));
        if (server.isEmpty() || p.get(QStringLiteral("mail.server.%1.type").arg(server)) == u"none")
            continue;  // unknown account, or Local Folders

        const QStringList identities = p.ids(QStringLiteral("mail.account.%1.identities").arg(id));
        QString email;
        if (!identities.isEmpty())
            email = p.get(QStringLiteral("mail.identity.%1.useremail").arg(identities.first()));
        if (email.isEmpty())
            email = p.get(QStringLiteral("mail.server.%1.userName").arg(server));
        QString name = p.get(QStringLiteral("mail.server.%1.name").arg(server));
        if (name.isEmpty())
            name = id;

        m_accounts.append({id, name, email, m_enabledIds.contains(id), id == defaultId});
    }
}

const Account *AccountState::defaultAccount() const
{
    for (const Account &a : m_accounts)
        if (a.isDefault)
            return &a;
    return nullptr;
}

QString AccountState::apply(const QSet<QString> &activeIds, bool checkRunning)
{
    if (checkRunning && Thunderbird::isRunning())
        throw TbError(tr("Thunderbird is running. Close it before changing the accounts."));
    if (const Account *def = defaultAccount(); def && !activeIds.contains(def->id))
        throw TbError(tr("The default account (%1) cannot be disabled.").arg(def->name));

    const QString backup = m_prefsFile + QStringLiteral(".bak-")
                           + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QFile::remove(backup);
    if (!QFile::copy(m_prefsFile, backup))
        throw TbError(tr("Cannot back up prefs.js to %1").arg(QDir::toNativeSeparators(backup)));

    QSet<QString> known;
    for (const Account &a : std::as_const(m_accounts))
        known.insert(a.id);

    QStringList active, disabled;
    for (const QString &id : std::as_const(m_master)) {
        // Accounts not shown (Local Folders...) keep their current state
        const bool isActive = known.contains(id) ? activeIds.contains(id) : m_enabledIds.contains(id);
        (isActive ? active : disabled).append(id);
    }

    Prefs &p = *m_prefs;
    p.setString(PrefAccounts, active.join(u','));
    if (!disabled.isEmpty()) {
        p.setString(PrefDisabled, disabled.join(u','));
        p.setString(PrefOrder, m_master.join(u','));
    } else {
        // Nothing disabled anymore: leave no trace in prefs.js
        p.remove(PrefDisabled);
        p.remove(PrefOrder);
    }
    p.save();

    reload();
    return backup;
}
