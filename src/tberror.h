#pragma once

#include <QString>
#include <exception>

// Error meant to be shown to the user (message in French).
class TbError : public std::exception
{
public:
    explicit TbError(QString message) : m_message(std::move(message)), m_utf8(m_message.toUtf8()) {}
    const QString &message() const { return m_message; }
    const char *what() const noexcept override { return m_utf8.constData(); }

private:
    QString m_message;
    QByteArray m_utf8;
};
