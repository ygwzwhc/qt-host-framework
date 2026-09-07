#include "core/transports/MqttTransport.h"

#include <QRandomGenerator>
#include <QTcpSocket>

namespace {

// 剩余长度 varint 编码（MQTT 规范 1-4 字节）
QByteArray encodeRemainingLength(int len)
{
    QByteArray out;
    do {
        quint8 d = quint8(len % 128);
        len /= 128;
        if (len > 0)
            d |= 0x80;
        out.append(char(d));
    } while (len > 0);
    return out;
}

// UTF-8 字符串：2 字节大端长度 + 内容
QByteArray encodeMqttString(const QString &s)
{
    const QByteArray utf8 = s.toUtf8();
    QByteArray out;
    out.append(char((utf8.size() >> 8) & 0xFF));
    out.append(char(utf8.size() & 0xFF));
    out.append(utf8);
    return out;
}

quint16 packetIdNext(quint16 &id)
{
    ++id;
    if (id == 0)
        id = 1;
    return id;
}

} // namespace

MqttTransport::MqttTransport(const MqttSettings &settings, QObject *parent)
    : ITransport(parent)
    , m_settings(settings)
{
    if (m_settings.clientId.trimmed().isEmpty()) {
        m_settings.clientId = QStringLiteral("QtHost_%1")
                                 .arg(QRandomGenerator::global()->bounded(100000), 5, 10, QLatin1Char('0'));
    }

    m_keepAliveTimer.setInterval(qMax(5, m_settings.keepAliveSec) * 1000 / 2);
    m_keepAliveTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_keepAliveTimer, &QTimer::timeout, this, &MqttTransport::onKeepAlive);
}

MqttTransport::~MqttTransport()
{
    close();
}

bool MqttTransport::open(QString *errMsg)
{
    if (m_socket)
        return true;

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &MqttTransport::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &MqttTransport::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &MqttTransport::onSocketError);
    connect(m_socket, &QTcpSocket::disconnected, this, &MqttTransport::onSocketDisconnected);

    m_tcpConnected = false;
    m_connAcked = false;
    m_closing = false;
    m_buffer.clear();
    m_socket->connectToHost(m_settings.host, m_settings.port);
    return true;
}

void MqttTransport::close()
{
    m_keepAliveTimer.stop();
    m_closing = true;
    const bool wasAcked = m_connAcked;
    m_connAcked = false;
    m_tcpConnected = false;

    if (m_socket) {
        if (wasAcked)
            sendDisconnect();
        m_socket->abort();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_buffer.clear();
}

bool MqttTransport::isOpen() const
{
    return m_connAcked;
}

QString MqttTransport::detail() const
{
    return QStringLiteral("%1:%2 发布:%3 订阅:%4")
        .arg(m_settings.host, QString::number(m_settings.port),
             m_settings.pubTopic.isEmpty() ? QStringLiteral("-") : m_settings.pubTopic,
             m_settings.subTopic.isEmpty() ? QStringLiteral("-") : m_settings.subTopic);
}

void MqttTransport::write(const QByteArray &bytes)
{
    if (!m_connAcked || m_settings.pubTopic.isEmpty() || bytes.isEmpty())
        return;

    QByteArray body;
    body += encodeMqttString(m_settings.pubTopic);   // 主题
    body += bytes;                                   // QoS0 无报文标识
    writePacket(0x30, body);                         // PUBLISH, QoS0
    emit bytesSent(bytes);
}

// ---------------- 内部：发送 ----------------

void MqttTransport::writePacket(quint8 header, const QByteArray &variableAndPayload)
{
    if (!m_socket)
        return;
    QByteArray pkt;
    pkt.append(char(header));
    pkt += encodeRemainingLength(variableAndPayload.size());
    pkt += variableAndPayload;
    m_socket->write(pkt);
}

void MqttTransport::sendConnect()
{
    QByteArray body;
    body += encodeMqttString(QStringLiteral("MQTT"));            // 协议名
    body += char(0x04);                                          // Level = 4 (3.1.1)
    quint8 flags = 0x02;                                         // Clean Session
    if (!m_settings.username.isEmpty())
        flags |= 0x80;
    if (!m_settings.password.isEmpty())
        flags |= 0x40;
    body += char(flags);
    body += char((m_settings.keepAliveSec >> 8) & 0xFF);
    body += char(m_settings.keepAliveSec & 0xFF);
    body += encodeMqttString(m_settings.clientId);
    if (!m_settings.username.isEmpty())
        body += encodeMqttString(m_settings.username);
    if (!m_settings.password.isEmpty())
        body += encodeMqttString(m_settings.password);
    writePacket(0x10, body);
}

void MqttTransport::sendSubscribe(const QString &topic, quint16 packetId)
{
    QByteArray body;
    body += char((packetId >> 8) & 0xFF);
    body += char(packetId & 0xFF);
    body += encodeMqttString(topic);
    body += char(0x00);                                          // QoS0
    writePacket(0x82, body);
}

void MqttTransport::sendPingReq()
{
    writePacket(0xC0, QByteArray());
}

void MqttTransport::sendDisconnect()
{
    writePacket(0xE0, QByteArray());
}

// ---------------- 内部：接收 ----------------

void MqttTransport::onConnected()
{
    m_tcpConnected = true;
    sendConnect();
    m_keepAliveTimer.start();
}

void MqttTransport::onKeepAlive()
{
    if (m_connAcked)
        sendPingReq();
}

void MqttTransport::onReadyRead()
{
    if (!m_socket)
        return;
    m_buffer += m_socket->readAll();
    processBuffer();
}

void MqttTransport::processBuffer()
{
    while (true) {
        if (m_buffer.size() < 2)
            return;

        // 解析剩余长度 varint
        int remain = 0;
        int multiplier = 1;
        int pos = 1;
        bool done = false;
        for (; pos < m_buffer.size() && pos <= 4; ++pos) {
            const quint8 byte = quint8(m_buffer.at(pos));
            remain += (byte & 0x7F) * multiplier;
            multiplier *= 128;
            if ((byte & 0x80) == 0) {
                done = true;
                ++pos;
                break;
            }
        }
        if (!done)
            return;                                   // 长度字节未收全
        if (m_buffer.size() < pos + remain)
            return;                                   // 包体未收全

        const quint8 header = quint8(m_buffer.at(0));
        const QByteArray body = m_buffer.mid(pos, remain);
        m_buffer.remove(0, pos + remain);

        switch (header >> 4) {
        case 0x2: {                                   // CONNACK
            const quint8 rc = quint8(body.size() >= 2 ? body.at(1) : 0xFF);
            if (rc == 0x00) {
                m_connAcked = true;
                if (!m_settings.subTopic.isEmpty())
                    sendSubscribe(m_settings.subTopic, packetIdNext(m_packetId));
                emit opened();
            } else {
                emit errorHappened(QStringLiteral("MQTT 连接被拒绝（返回码 0x%1）")
                                       .arg(rc, 2, 16, QLatin1Char('0')));
                close();
                emit closed();
            }
            break;
        }
        case 0x3: {                                   // PUBLISH
            if (body.size() < 4)
                break;
            const int topicLen = (quint8(body.at(0)) << 8) | quint8(body.at(1));
            if (body.size() < 2 + topicLen)
                break;
            const QString topic = QString::fromUtf8(body.mid(2, topicLen));
            int offset = 2 + topicLen;
            const quint8 qos = (header >> 1) & 0x03;
            if (qos > 0)
                offset += 2;                          // 跳过报文标识（QoS1/2）
            const QByteArray payload = body.mid(offset);
            if (!payload.isEmpty()) {
                emit bytesReceived(payload);
                emit messageReceived(topic, payload);
            }
            break;
        }
        case 0x9:                                     // SUBACK（成功与否不阻断）
        case 0xD:                                     // PINGRESP
        default:
            break;
        }
    }
}

void MqttTransport::onSocketError()
{
    if (m_closing)
        return;
    if (m_socket)
        emit errorHappened(m_socket->errorString());
}

void MqttTransport::onSocketDisconnected()
{
    m_keepAliveTimer.stop();
    if (m_closing)
        return;
    if (m_connAcked) {
        m_connAcked = false;
        emit errorHappened(QStringLiteral("连接被服务端断开"));
        emit closed();
    }
}
