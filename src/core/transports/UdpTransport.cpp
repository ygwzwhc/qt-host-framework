#include "core/transports/UdpTransport.h"

UdpTransport::UdpTransport(quint16 localPort, const QString &targetHost, quint16 targetPort,
                           QObject *parent)
    : ITransport(parent)
    , m_localPort(localPort)
    , m_targetHost(targetHost)
    , m_targetPort(targetPort)
{
    m_socket = new QUdpSocket(this);
    connect(m_socket, &QUdpSocket::readyRead, this, [this] {
        while (m_socket->hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(int(m_socket->pendingDatagramSize()));
            m_socket->readDatagram(datagram.data(), datagram.size());
            emit bytesReceived(datagram);
        }
    });
    connect(m_socket, &QUdpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) { emit errorHappened(m_socket->errorString()); });
}

bool UdpTransport::open(QString *errMsg)
{
    close();
    if (!m_socket->bind(QHostAddress::Any, m_localPort)) {
        if (errMsg)
            *errMsg = m_socket->errorString();
        return false;
    }
    emit opened();
    return true;
}

void UdpTransport::close()
{
    if (m_socket->state() == QAbstractSocket::BoundState) {
        m_socket->close();
        emit closed();
    }
}

bool UdpTransport::isOpen() const
{
    return m_socket->state() == QAbstractSocket::BoundState;
}

void UdpTransport::write(const QByteArray &bytes)
{
    if (m_socket->state() != QAbstractSocket::BoundState)
        return;
    m_socket->writeDatagram(bytes, QHostAddress(m_targetHost), m_targetPort);
    emit bytesSent(bytes);
}
