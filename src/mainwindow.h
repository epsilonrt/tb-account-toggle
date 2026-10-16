#pragma once

#include "accountstate.h"
#include "thunderbird.h"

#include <QElapsedTimer>
#include <QFrame>
#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;
class QVBoxLayout;
class ToggleSwitch;

// Account name and address, with a switch (or a badge for the default account).
// Looks like a row of the Windows 11 Settings app.
class AccountRow : public QFrame
{
    Q_OBJECT

public:
    explicit AccountRow(const Account &account, QWidget *parent = nullptr);

    const Account &account() const { return m_account; }
    ToggleSwitch *toggle() const { return m_switch; }
    bool isPending() const;
    bool wantsActive() const;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void updateStateLabel();

    Account m_account;
    ToggleSwitch *m_switch = nullptr;
    QLabel *m_stateLabel = nullptr;
};

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    // Translated application title, used for the window and the dialogs
    static QString appTitle() { return tr("Thunderbird Accounts"); }

    // manageThunderbird = false when working on a profile copy: never touch the real Thunderbird
    MainWindow(AccountState *state, bool manageThunderbird, QWidget *parent = nullptr);

private:
    void buildRows();
    void refresh();
    void setBusy(bool busy, const QString &message = {});
    void resetSwitches();
    void showAbout();
    void apply();
    void waitForClose();
    void stopWaiting();
    void write(bool restart);

    AccountState *m_state;
    bool m_manageThunderbird;
    QList<AccountRow *> m_rows;
    QVBoxLayout *m_rowsBox;
    QLabel *m_status;
    QPushButton *m_btnReset;
    QPushButton *m_btnApply;
    QTimer *m_poll;
    QElapsedTimer m_closeClock;
    Thunderbird::Launcher m_launcher;
};
