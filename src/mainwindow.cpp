#include "mainwindow.h"
#include "tberror.h"
#include "thunderbird.h"
#include "toggleswitch.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int CloseTimeoutMs = 30000;

QLabel *secondaryLabel(const QString &text = {})
{
    auto *label = new QLabel(text);
    label->setObjectName(QStringLiteral("secondary"));
    return label;
}
} // namespace

// --------------------------------------------------------------------------
// AccountRow
// --------------------------------------------------------------------------

AccountRow::AccountRow(const Account &account, QWidget *parent) : QFrame(parent), m_account(account)
{
    setObjectName(QStringLiteral("card"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(16, 10, 12, 10);
    lay->setSpacing(12);

    auto *texts = new QVBoxLayout;
    texts->setSpacing(0);
    auto *name = new QLabel(account.name);
    name->setObjectName(QStringLiteral("rowTitle"));
    texts->addWidget(name);
    texts->addWidget(secondaryLabel(account.email));
    lay->addLayout(texts, 1);

    if (account.isDefault) {
        lay->addWidget(secondaryLabel(tr("Always enabled")));
        return;
    }

    m_stateLabel = secondaryLabel();
    m_switch = new ToggleSwitch;
    m_switch->setCheckedImmediate(account.active);
    m_switch->setAccessibleName(tr("%1 enabled").arg(account.name));
    connect(m_switch, &QAbstractButton::toggled, this, &AccountRow::updateStateLabel);
    lay->addWidget(m_stateLabel);
    lay->addWidget(m_switch);
    updateStateLabel();
    setCursor(Qt::PointingHandCursor);
}

bool AccountRow::isPending() const
{
    return m_switch && m_switch->isChecked() != m_account.active;
}

bool AccountRow::wantsActive() const
{
    return m_account.isDefault || (m_switch && m_switch->isChecked());
}

void AccountRow::updateStateLabel()
{
    m_stateLabel->setText(m_switch->isChecked() ? tr("Enabled") : tr("Disabled"));
}

void AccountRow::mouseReleaseEvent(QMouseEvent *event)
{
    // The whole row toggles the switch, as in Windows Settings
    if (m_switch && m_switch->isEnabled() && event->button() == Qt::LeftButton)
        m_switch->toggle();
    QFrame::mouseReleaseEvent(event);
}

// --------------------------------------------------------------------------
// MainWindow
// --------------------------------------------------------------------------

MainWindow::MainWindow(AccountState *state, bool manageThunderbird, QWidget *parent)
    : QWidget(parent), m_state(state), m_manageThunderbird(manageThunderbird), m_poll(new QTimer(this))
{
    setWindowTitle(appTitle());
    setMinimumWidth(480);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(4);

    auto *title = new QLabel(appTitle());
    title->setObjectName(QStringLiteral("pageTitle"));
    auto *subtitle = secondaryLabel(tr("A disabled account is no longer loaded by Thunderbird: no connection, "
                                       "and it stays hidden until it is enabled again."));
    subtitle->setWordWrap(true);
    root->addWidget(title);
    root->addWidget(subtitle);
    root->addSpacing(16);

    m_rowsBox = new QVBoxLayout;
    m_rowsBox->setSpacing(4);
    root->addLayout(m_rowsBox);
    buildRows();

    root->addSpacing(16);
    auto *footer = new QHBoxLayout;
    m_status = secondaryLabel();
    m_status->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_btnReset = new QPushButton(tr("Revert"));
    m_btnApply = new QPushButton(tr("Apply"));
    m_btnApply->setObjectName(QStringLiteral("accent"));
    m_btnApply->setDefault(true);
    connect(m_btnReset, &QPushButton::clicked, this, &MainWindow::resetSwitches);
    connect(m_btnApply, &QPushButton::clicked, this, &MainWindow::apply);
    footer->addWidget(m_status);
    footer->addWidget(m_btnReset);
    footer->addWidget(m_btnApply);
    root->addLayout(footer);

    m_poll->setInterval(300);
    connect(m_poll, &QTimer::timeout, this, &MainWindow::waitForClose);

    refresh();
}

void MainWindow::buildRows()
{
    // Remove previous rows and spacers
    while (QLayoutItem *item = m_rowsBox->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
    m_rows.clear();

    QList<Account> ordered;
    if (const Account *def = m_state->defaultAccount())
        ordered.append(*def);
    for (const Account &a : m_state->accounts())
        if (!a.isDefault)
            ordered.append(a);

    for (qsizetype i = 0; i < ordered.size(); ++i) {
        auto *row = new AccountRow(ordered.at(i));
        if (row->toggle())
            connect(row->toggle(), &QAbstractButton::toggled, this, &MainWindow::refresh);
        m_rowsBox->addWidget(row);
        // Visual separation between the default account and the others
        if (ordered.at(i).isDefault && i < ordered.size() - 1)
            m_rowsBox->addSpacing(8);
        m_rows.append(row);
    }
}

void MainWindow::refresh()
{
    int n = 0;
    for (const AccountRow *row : std::as_const(m_rows))
        n += row->isPending();
    m_btnApply->setEnabled(n > 0);
    m_btnReset->setEnabled(n > 0);
    m_status->setText(n ? tr("%n pending change(s)", nullptr, n) : QString());
}

void MainWindow::setBusy(bool busy, const QString &message)
{
    for (AccountRow *row : std::as_const(m_rows))
        if (row->toggle())
            row->toggle()->setEnabled(!busy);
    m_btnApply->setEnabled(!busy);
    m_btnReset->setEnabled(!busy);
    setCursor(busy ? Qt::WaitCursor : Qt::ArrowCursor);
    if (!message.isEmpty())
        m_status->setText(message);
}

void MainWindow::resetSwitches()
{
    for (AccountRow *row : std::as_const(m_rows))
        if (row->toggle())
            row->toggle()->setChecked(row->account().active);
    refresh();
}

void MainWindow::apply()
{
    if (!m_manageThunderbird || !Thunderbird::isRunning()) {
        write(false);
        return;
    }
    const auto answer = QMessageBox::question(this, appTitle(),
        tr("Thunderbird must be closed to apply the changes.\n\n"
           "Close it now? It will be restarted afterwards."));
    if (answer != QMessageBox::Yes)
        return;

    m_restartExe = Thunderbird::executablePath();  // before it closes
    setBusy(true, tr("Closing Thunderbird…"));
    Thunderbird::requestClose();
    m_closeClock.start();
    m_poll->start();
}

void MainWindow::waitForClose()
{
    if (!Thunderbird::isRunning()) {
        m_poll->stop();
        write(true);
    } else if (m_closeClock.elapsed() > CloseTimeoutMs) {
        m_poll->stop();
        setBusy(false);
        refresh();
        QMessageBox::warning(this, appTitle(),
            tr("Thunderbird did not close (a confirmation dialog may be open, or it is "
               "minimized to the notification area).\n\n"
               "Close it yourself, then click Apply again."));
    }
}

void MainWindow::write(bool restart)
{
    QSet<QString> active;
    for (const AccountRow *row : std::as_const(m_rows))
        if (row->wantsActive())
            active.insert(row->account().id);

    try {
        m_state->apply(active, m_manageThunderbird);
    } catch (const TbError &e) {
        setBusy(false);
        refresh();
        QMessageBox::critical(this, appTitle(), e.message());
        return;
    }

    buildRows();
    setBusy(false);
    refresh();
    QString message = tr("Changes applied.");
    if (restart) {
        if (Thunderbird::launch(m_restartExe.isEmpty() ? Thunderbird::executablePath() : m_restartExe))
            message = tr("Changes applied, Thunderbird restarted.");
        else
            message = tr("Changes applied. Please restart Thunderbird yourself.");
    }
    m_status->setText(message);
}
