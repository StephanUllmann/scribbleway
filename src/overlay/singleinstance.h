#pragma once

#include <QLocalServer>
#include <QLocalSocket>
#include <QString>

class SingleInstanceGuard
{
public:
    explicit SingleInstanceGuard(const QString &name) : m_name(name) {}

    // Returns false when another instance already owns the socket. `handoff`, if given,
    // is delivered to that instance first — this is how --toggle reaches the running
    // daemon on compositors where KGlobalAccel does nothing.
    bool tryAcquire(const QByteArray &handoff = {})
    {
        QLocalSocket probe;
        probe.connectToServer(m_name);
        if (probe.waitForConnected(100)) {
            if (!handoff.isEmpty()) {
                probe.write(handoff);
                probe.waitForBytesWritten(100);
            }
            probe.disconnectFromServer();
            return false;
        }

        QLocalServer::removeServer(m_name);
        return m_server.listen(m_name);
    }

    QLocalServer *server() { return &m_server; }

private:
    QString m_name;
    QLocalServer m_server;
};
