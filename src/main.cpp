#include "accountstate.h"
#include "mainwindow.h"
#include "tberror.h"
#include "thunderbird.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFont>
#include <QIcon>
#include <QLocale>
#include <QMessageBox>
#include <QStyleFactory>
#include <QStyleHints>
#include <QTranslator>

namespace {

QString styleSheet(const QPalette &pal)
{
    const bool dark = pal.color(QPalette::Window).lightness() < 128;
    const QString card = dark ? "rgba(255,255,255,0.05)" : "rgba(255,255,255,0.70)";
    const QString cardBorder = dark ? "rgba(0,0,0,0.25)" : "rgba(0,0,0,0.07)";
    const QString secondary = dark ? "rgba(255,255,255,0.65)" : "rgba(0,0,0,0.60)";
    const QColor accent = pal.color(QPalette::Accent);
    const QString accentText = accent.lightness() > 150 ? "black" : "white";

    return QStringLiteral(R"(
        QFrame#card {
            background: %1;
            border: 1px solid %2;
            border-radius: 6px;
        }
        QLabel#pageTitle { font-size: 20pt; font-weight: 600; }
        QLabel#rowTitle { font-size: 10.5pt; }
        QLabel#secondary { color: %3; }
        QPushButton { min-width: 90px; min-height: 28px; }
        QPushButton#accent:enabled {
            background: %4;
            color: %5;
            border: 1px solid %6;
            border-radius: 4px;
        }
        QPushButton#accent:enabled:hover { background: %7; }
    )")
        .arg(card, cardBorder, secondary, accent.name(), accentText,
             accent.darker(115).name(), accent.lighter(110).name());
}

// Load the app and Qt translations embedded under :/i18n for the system language.
// English is the source language: nothing to load for it.
void installTranslations(QApplication &app)
{
    for (const char *catalog : {"tb-account-toggle", "qtbase"}) {
        auto *translator = new QTranslator(&app);
        if (translator->load(QLocale(), QString::fromLatin1(catalog), QStringLiteral("_"), QStringLiteral(":/i18n")))
            QApplication::installTranslator(translator);
        else
            delete translator;
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("tb-account-toggle"));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));
    installTranslations(app);
    QApplication::setApplicationDisplayName(MainWindow::appTitle());
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/app.ico")));
    if (QStyleFactory::keys().contains(QStringLiteral("windows11"), Qt::CaseInsensitive))
        QApplication::setStyle(QStringLiteral("windows11"));
    QApplication::setFont(QFont(QStringLiteral("Segoe UI Variable Text"), 10));

    const auto restyle = [&app] { app.setStyleSheet(styleSheet(app.palette())); };
    restyle();
    QObject::connect(app.styleHints(), &QStyleHints::colorSchemeChanged, &app, restyle);

    QCommandLineParser parser;
    parser.setApplicationDescription(QApplication::translate("main", "Temporarily disables or enables Thunderbird accounts."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption profileOpt(QStringLiteral("profile"),
        QApplication::translate("main", "Profile folder to use (test mode: Thunderbird is never closed or restarted)."),
        QApplication::translate("main", "folder"));
    parser.addOption(profileOpt);
    parser.process(app);
    const bool testMode = parser.isSet(profileOpt);

    std::unique_ptr<AccountState> state;
    try {
        const QString profile = testMode ? parser.value(profileOpt) : Thunderbird::defaultProfilePath();
        state = std::make_unique<AccountState>(profile);
    } catch (const TbError &e) {
        QMessageBox::critical(nullptr, MainWindow::appTitle(), e.message());
        return 1;
    }

    MainWindow window(state.get(), !testMode);
    if (testMode)
        window.setWindowTitle(QApplication::translate("main", "%1 — test on %2")
                                  .arg(MainWindow::appTitle(), QDir::toNativeSeparators(parser.value(profileOpt))));
    window.show();
    return app.exec();
}
