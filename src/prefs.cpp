#include "prefs.h"
#include "tberror.h"

#include <QFile>
#include <QRegularExpression>

namespace {

const QRegularExpression &prefLine()
{
    static const QRegularExpression re(QStringLiteral(R"re(^user_pref\("((?:[^"\\]|\\.)*)", (.*)\);$)re"));
    return re;
}

// Decode a JS string literal such as "a\"b" (without validating it strictly)
QString unquote(const QString &literal)
{
    if (!literal.startsWith(u'"'))
        return literal;  // bool or int
    QString out;
    for (qsizetype i = 1; i < literal.size(); ++i) {
        const QChar c = literal.at(i);
        if (c == u'"')
            break;
        if (c != u'\\' || i + 1 >= literal.size()) {
            out += c;
            continue;
        }
        const QChar e = literal.at(++i);
        switch (e.unicode()) {
        case 'n': out += u'\n'; break;
        case 'r': out += u'\r'; break;
        case 't': out += u'\t'; break;
        case 'u':
            if (i + 4 < literal.size()) {
                out += QChar(literal.mid(i + 1, 4).toUShort(nullptr, 16));
                i += 4;
            }
            break;
        default: out += e; break;  // \" \\ and anything else
        }
    }
    return out;
}

QString quote(QString value)
{
    value.replace(u'\\', QStringLiteral("\\\\")).replace(u'"', QStringLiteral("\\\""));
    return u'"' + value + u'"';
}

} // namespace

Prefs::Prefs(const QString &path) : m_path(path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        throw TbError(tr("Cannot read %1: %2").arg(path, f.errorString()));
    const QString text = QString::fromUtf8(f.readAll());

    m_newline = text.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    m_trailingNewline = text.endsWith(u'\n');
    m_lines = text.split(u'\n');
    if (m_trailingNewline)
        m_lines.removeLast();
    for (QString &line : m_lines)
        if (line.endsWith(u'\r'))
            line.chop(1);
}

int Prefs::find(const QString &name) const
{
    for (int i = 0; i < m_lines.size(); ++i) {
        const auto m = prefLine().match(m_lines.at(i));
        if (m.hasMatch() && m.captured(1) == name)
            return i;
    }
    return -1;
}

QString Prefs::get(const QString &name) const
{
    const int i = find(name);
    if (i < 0)
        return {};
    return unquote(prefLine().match(m_lines.at(i)).captured(2));
}

QStringList Prefs::ids(const QString &name) const
{
    return get(name).split(u',', Qt::SkipEmptyParts);
}

void Prefs::setString(const QString &name, const QString &value)
{
    const QString line = QStringLiteral("user_pref(\"%1\", %2);").arg(name, quote(value));
    const int i = find(name);
    if (i >= 0)
        m_lines[i] = line;
    else
        m_lines.append(line);
}

void Prefs::remove(const QString &name)
{
    const int i = find(name);
    if (i >= 0)
        m_lines.removeAt(i);
}

void Prefs::save() const
{
    QString text = m_lines.join(m_newline);
    if (m_trailingNewline)
        text += m_newline;
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(text.toUtf8()) < 0)
        throw TbError(tr("Cannot write %1: %2").arg(m_path, f.errorString()));
}
