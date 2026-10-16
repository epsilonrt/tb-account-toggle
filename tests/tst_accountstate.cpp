#include "accountstate.h"
#include "tberror.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

// Synthetic profile: 3 IMAP accounts (account1 = default), Local Folders, CRLF line endings
static const char *const SamplePrefs =
    "// Mozilla User Preferences\r\n"
    "\r\n"
    "user_pref(\"mail.account.account1.identities\", \"id1\");\r\n"
    "user_pref(\"mail.account.account1.server\", \"server1\");\r\n"
    "user_pref(\"mail.account.account2.server\", \"server2\");\r\n"
    "user_pref(\"mail.account.account3.identities\", \"id3,id4\");\r\n"
    "user_pref(\"mail.account.account3.server\", \"server3\");\r\n"
    "user_pref(\"mail.account.account4.identities\", \"id5\");\r\n"
    "user_pref(\"mail.account.account4.server\", \"server4\");\r\n"
    "user_pref(\"mail.account.lastKey\", 4);\r\n"
    "user_pref(\"mail.accountmanager.accounts\", \"account1,account3,account4,account2\");\r\n"
    "user_pref(\"mail.accountmanager.defaultaccount\", \"account1\");\r\n"
    "user_pref(\"mail.identity.id1.useremail\", \"perso@example.org\");\r\n"
    "user_pref(\"mail.identity.id3.useremail\", \"pro@example.org\");\r\n"
    "user_pref(\"mail.identity.id5.useremail\", \"autre@example.org\");\r\n"
    "user_pref(\"mail.server.server1.name\", \"Perso\");\r\n"
    "user_pref(\"mail.server.server1.type\", \"imap\");\r\n"
    "user_pref(\"mail.server.server2.name\", \"Dossiers locaux\");\r\n"
    "user_pref(\"mail.server.server2.type\", \"none\");\r\n"
    "user_pref(\"mail.server.server3.name\", \"Académique \\\"pro\\\"\");\r\n"
    "user_pref(\"mail.server.server3.type\", \"imap\");\r\n"
    "user_pref(\"mail.server.server4.name\", \"Autre\");\r\n"
    "user_pref(\"mail.server.server4.type\", \"imap\");\r\n";

class TestAccountState : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString prefsPath() const { return m_dir.filePath(QStringLiteral("prefs.js")); }
    QByteArray readPrefs() const
    {
        QFile f(prefsPath());
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }
    static QSet<QString> idsOf(const AccountState &s)
    {
        QSet<QString> ids;
        for (const Account &a : s.accounts())
            ids.insert(a.id);
        return ids;
    }

private slots:
    void init()
    {
        QFile f(prefsPath());
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(SamplePrefs);
    }

    void listsMailAccountsOnly()
    {
        AccountState s(m_dir.path());
        QCOMPARE(s.accounts().size(), 3);  // Local Folders excluded
        QCOMPARE(s.accounts().at(0).id, QStringLiteral("account1"));
        QVERIFY(s.accounts().at(0).isDefault);
        QCOMPARE(s.accounts().at(1).name, QStringLiteral("Académique \"pro\""));
        QCOMPARE(s.accounts().at(1).email, QStringLiteral("pro@example.org"));
        for (const Account &a : s.accounts())
            QVERIFY(a.active);
    }

    void refusesToDisableDefault()
    {
        AccountState s(m_dir.path());
        const QByteArray before = readPrefs();
        QVERIFY_THROWS_EXCEPTION(TbError, s.apply({QStringLiteral("account3"), QStringLiteral("account4")}, false));
        QCOMPARE(readPrefs(), before);
    }

    void disableThenEnableIsByteIdentical()
    {
        const QByteArray original = readPrefs();
        AccountState s(m_dir.path());
        const QSet<QString> all = idsOf(s);

        s.apply(all - QSet<QString>{QStringLiteral("account3")}, false);
        const QByteArray disabled = readPrefs();
        QVERIFY(disabled.contains("user_pref(\"mail.accountmanager.accounts\", \"account1,account4,account2\");"));
        QVERIFY(disabled.contains("user_pref(\"mail.accountmanager.disabled_accounts\", \"account3\");"));
        QVERIFY(disabled.contains("user_pref(\"mail.accountmanager.accounts_order\", \"account1,account3,account4,account2\");"));
        QVERIFY(disabled.contains("mail.server.server3.name"));  // settings kept
        QVERIFY(!s.accounts().at(1).active);

        AccountState reread(m_dir.path());
        reread.apply(all, false);
        QCOMPARE(readPrefs(), original);
    }

    void accountAddedWhileDisabledIsKept()
    {
        AccountState s(m_dir.path());
        s.apply(idsOf(s) - QSet<QString>{QStringLiteral("account3")}, false);

        // Simulate Thunderbird appending a new account meanwhile
        QByteArray data = readPrefs();
        data.replace("\"account1,account4,account2\"", "\"account1,account4,account2,account5\"");
        data += "user_pref(\"mail.account.account5.server\", \"server5\");\r\n"
                "user_pref(\"mail.server.server5.type\", \"imap\");\r\n";
        QFile f(prefsPath());
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(data);
        f.close();

        AccountState after(m_dir.path());
        QCOMPARE(after.accounts().size(), 4);
        after.apply(idsOf(after), false);
        QVERIFY(readPrefs().contains("\"account1,account3,account4,account2,account5\""));
        QVERIFY(!readPrefs().contains("disabled_accounts"));
    }

    void backupIsCreated()
    {
        AccountState s(m_dir.path());
        const QString backup = s.apply(idsOf(s), false);
        QVERIFY(QFile::exists(backup));
        QFile::remove(backup);
    }
};

QTEST_GUILESS_MAIN(TestAccountState)
#include "tst_accountstate.moc"
