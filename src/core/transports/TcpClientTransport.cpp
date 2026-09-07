#include "core/transports/TcpClientTransport.h"

TcpClientTransport::TcpClientTransport(const QString &host, quint16 port, QObject *parent)
    : ITransport(parent)
    , m_host(host)
    , m_port(port)
{
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &ITransport::opened);
    connect(m_socket, &QTcpSocket::disconnected, this, &ITransport::closed);
    connect(m_socket, &QTcpSocket::readyRead, this, [this] {
        const QByteArray bytes = m_socket->readAll();
        if (!bytes.isEmpty())
            emit bytesReceived(bytes);
    });
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit errorHappened(m_socket->errorString());
        if (m_socket->state() != QAbstractSocket::ConnectedState
            && m_socket->state() != QAbstractSocket::ConnectingState) {
            m_socket->abort();
            emit closed();
        }
    });
}

bool TcpClientTransport::open(QString *errMsg)
{
    close();
    m_socket->connectToHost(m_host, m_port);
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        if (errMsg)
            *errMsg = m_socket->errorString();
        return false;
    }
    return true; // 异步连接，结果以信号为准
}

void TcpClientTransport::close()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
        emit closed();
    }
}

bool TcpClientTransport::isOpen() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void TcpClientTransport::write(const QByteArray &bytes)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(bytes);
        emit bytesSent(bytes);
    }
}
