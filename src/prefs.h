#pragma once

#include <QCoreApplication>
#include <QString>
#include <QStringList>

// Line-preserving view of Thunderbird's prefs.js: untouched lines are written back verbatim,
// so a disable/enable round trip gives a byte-identical file.
class Prefs
{
    Q_DECLARE_TR_FUNCTIONS(Prefs)

public:
    explicit Prefs(const QString &path);  // throws TbError

    // Value of a pref (string literal unescaped), or a null QString when absent
    QString get(const QString &name) const;
    // Comma-separated list pref
    QStringList ids(const QString &name) const;

    void setString(const QString &name, const QString &value);
    void remove(const QString &name);
    void save() const;  // throws TbError

private:
    int find(const QString &name) const;

    QString m_path;
    QStringList m_lines;
    QString m_newline;
    bool m_trailingNewline = false;
};
