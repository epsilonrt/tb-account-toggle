#pragma once

#include "prefs.h"

#include <QCoreApplication>
#include <QList>
#include <QSet>
#include <QString>
#include <memory>

/*
 * A "disabled" account is removed from mail.accountmanager.accounts, so Thunderbird
 * does not load it at all (no connection, not displayed). Its settings
 * (mail.account.* / mail.server.* / mail.identity.*) and its local message cache stay
 * untouched, so re-enabling restores it as it was.
 *
 * Reactivation data is kept in prefs.js itself (Thunderbird preserves unknown prefs)
 * and removed once no account is disabled:
 *   mail.accountmanager.disabled_accounts  disabled account ids
 *   mail.accountmanager.accounts_order     original order of all accounts
 *
 * The default account can never be disabled. Thunderbird must be closed while
 * prefs.js is written: it rewrites it on exit.
 */

struct Account
{
    QString id;
    QString name;
    QString email;
    bool active = false;
    bool isDefault = false;
};

class AccountState
{
    Q_DECLARE_TR_FUNCTIONS(AccountState)

public:
    explicit AccountState(const QString &profileDir);  // throws TbError

    void reload();  // throws TbError

    const QList<Account> &accounts() const { return m_accounts; }
    const Account *defaultAccount() const;
    QString prefsFile() const { return m_prefsFile; }
    QString profileDir() const { return m_profileDir; }

    // Write prefs.js so that exactly `activeIds` are enabled. Returns the backup path.
    QString apply(const QSet<QString> &activeIds, bool checkRunning = true);  // throws TbError

private:
    QString m_profileDir;
    QString m_prefsFile;
    std::unique_ptr<Prefs> m_prefs;
    QStringList m_enabledIds;
    QStringList m_master;  // every known account, in original order
    QList<Account> m_accounts;
};
