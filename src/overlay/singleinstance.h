#pragma once

#include <QLocalServer>
#include <QLocalSocket>
#include <QString>

class SingleInstanceGuard
{
public:
    explicit SingleInstanceGuard(const QString &name) : m_name(name) {}

    bool tryAcquire()
    {
        QLocalSocket probe;
        probe.connectToServer(m_name);
        if (probe.waitForConnected(100)) {
            probe.disconnectFromServer();
            return false;
        }

        QLocalServer::removeServer(m_name);
        return m_server.listen(m_name);
    }

private:
    QString m_name;
    QLocalServer m_server;
};
