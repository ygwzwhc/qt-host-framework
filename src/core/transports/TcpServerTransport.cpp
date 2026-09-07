#include "core/transports/TcpServerTransport.h"

TcpServerTransport::TcpServerTransport(quint16 port, QObject *parent)
    : ITransport(parent)
    , m_port(port)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, [this] {
        while (QTcpSocket *client = m_server->nextPendingConnection()) {
            m_clients.append(client);
            connect(client, &QTcpSocket::readyRead, this, [this, client] {
                const QByteArray bytes = client->readAll();
                if (!bytes.isEmpty())
                    emit bytesReceived(bytes);
            });
            connect(client, &QTcpSocket::disconnected, this, [this, client] {
                m_clients.removeAll(client);
                client->deleteLater();
                if (m_clients.isEmpty() && !m_server->isListening())
                    emit closed();
            });
        }
    });
    connect(m_server, &QTcpServer::acceptError, this,
            [this](QAbstractSocket::SocketError) { emit errorHappened(m_server->errorString()); });
}

bool TcpServerTransport::open(QString *errMsg)
{
    close();
    if (!m_server->listen(QHostAddress::Any, m_port)) {
        if (errMsg)
            *errMsg = m_server->errorString();
        return false;
    }
    emit opened();
    return true;
}

void TcpServerTransport::close()
{
    if (m_server->isListening()) {
        for (QTcpSocket *client : m_clients) {
            client->disconnectFromHost();
            client->deleteLater();
        }
        m_clients.clear();
        m_server->close();
        emit closed();
    }
}

bool TcpServerTransport::isOpen() const
{
    return m_server->isListening();
}

void TcpServerTransport::write(const QByteArray &bytes)
{
    if (!m_server->isListening())
        return;
    for (QTcpSocket *client : m_clients)
        client->write(bytes);
    if (!m_clients.isEmpty())
        emit bytesSent(bytes);
}
