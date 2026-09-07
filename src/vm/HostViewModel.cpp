#include "vm/HostViewModel.h"

#include <QTimer>

#include "core/AppLogger.h"
#include "core/ByteUtils.h"
#include "core/transports/LoopbackTransport.h"
#include "core/transports/MqttTransport.h"
#include "core/transports/SerialTransport.h"
#include "core/transports/TcpClientTransport.h"
#include "core/transports/TcpServerTransport.h"
#include "core/transports/UdpTransport.h"

namespace {
const char *kProtocolNames[] = {
    "原始透传", "自定义帧 AA55", "Modbus RTU", "Modbus TCP", "文本行 ASCII"
};
} // namespace

HostViewModel::HostViewModel(QObject *parent)
    : QObject(parent)
{
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &HostViewModel::performModbusRead);

    // 协议解码器信号（常驻连接，由 setProtocol 决定谁消费字节）
    connect(&m_frameCodec, &FrameCodec::frameReady, this, &HostViewModel::onFrameReady);
    connect(&m_frameCodec, &FrameCodec::codecError, this, [this](const QString &msg) {
        emit messageLine(KindWarn, msg);
    });
    connect(&m_rtuCodec, &ModbusCodecBase::packetParsed, this,
            [this](const ParsedPacket &p) { emit messageLine(p.kind == ParsedPacket::Warn ? KindWarn : KindRx, p.summary); });
    connect(&m_tcpCodec, &ModbusCodecBase::packetParsed, this,
            [this](const ParsedPacket &p) { emit messageLine(p.kind == ParsedPacket::Warn ? KindWarn : KindRx, p.summary); });
    connect(&m_lineCodec, &AsciiLineCodec::packetParsed, this,
            [this](const ParsedPacket &p) { emit messageLine(KindRx, p.summary); });
}

// ---------------- 命令 ----------------

void HostViewModel::openChannel(int channelType, const SerialConfig &serial,
                                const QString &host, int port1, int port2,
                                const MqttSettings &mqtt)
{
    closeChannel();

    switch (channelType) {
    case ChannelSerial:
        m_transport = new SerialTransport(serial, this);
        break;
    case ChannelTcpClient:
        m_transport = new TcpClientTransport(host, quint16(port1), this);
        break;
    case ChannelTcpServer:
        m_transport = new TcpServerTransport(quint16(port1), this);
        break;
    case ChannelUdp:
        m_transport = new UdpTransport(quint16(port1), host, quint16(port2), this);
        break;
    case ChannelLoopback:
        m_transport = new LoopbackTransport(this);
        break;
    case ChannelMqtt:
        m_transport = new MqttTransport(mqtt, this);
        break;
    default:
        return;
    }

    // MQTT 主题日志（载荷本身仍走 bytesReceived → 解码链路）
    if (auto *mqttTr = qobject_cast<MqttTransport *>(m_transport)) {
        connect(mqttTr, &MqttTransport::messageReceived, this,
                [](const QString &topic, const QByteArray &payload) {
                    AppLogger::instance().info(
                        QStringLiteral("MQTT 收到 主题=%1 长度=%2").arg(topic).arg(payload.size()));
                });
    }

    connect(m_transport, &ITransport::opened, this, &HostViewModel::onTransportOpened);
    connect(m_transport, &ITransport::closed, this, &HostViewModel::onTransportClosed);
    connect(m_transport, &ITransport::errorHappened, this, &HostViewModel::onTransportError);
    // 收发链路：原始字节 → 计数/监视/解码
    connect(m_transport, &ITransport::bytesReceived, this, [this](const QByteArray &bytes) {
        if (bytes.isEmpty())
            return;
        m_rxBytes += bytes.size();
        emit countersChanged(m_txBytes, m_rxBytes);
        emit rawLine(false, bytes);
        feedCodec(bytes);
    });
    connect(m_transport, &ITransport::bytesSent, this, [this](const QByteArray &bytes) {
        if (bytes.isEmpty())
            return;
        m_txBytes += bytes.size();
        emit countersChanged(m_txBytes, m_rxBytes);
        emit rawLine(true, bytes);
    });

    QString err;
    if (!m_transport->open(&err)) {
        emit messageLine(KindError, QStringLiteral("打开失败：%1").arg(err));
        AppLogger::instance().error(QStringLiteral("打开失败：%1").arg(err));
        closeChannel();
        return;
    }
    AppLogger::instance().info(
        QStringLiteral("正在打开 %1（%2）…").arg(m_transport->name(), m_transport->detail()));
}

void HostViewModel::closeChannel()
{
    if (!m_transport)
        return;
    m_transport->disconnect(this);
    m_transport->close();
    m_transport->deleteLater();
    m_transport = nullptr;
    onTransportClosed();
}

void HostViewModel::refreshPorts()
{
    emit portsChanged(SerialTransport::availablePortNames());
}

void HostViewModel::setProtocol(int index)
{
    m_protocolIndex = index;
    m_frameCodec.reset();
    m_rtuCodec.reset();
    m_tcpCodec.reset();
    m_lineCodec.reset();
    if (index < 0 || index > 4)
        return;
    const QString name = QString::fromUtf8(kProtocolNames[index]);
    emit protocolNameChanged(name);
    AppLogger::instance().info(QStringLiteral("协议解析切换为：%1").arg(name));
    if (index != 2 && index != 3)
        updatePollTimer(); // 非 Modbus 协议自动停轮询
}

void HostViewModel::sendText(const QString &text, bool hex)
{
    if (!m_transport || !m_transport->isOpen()) {
        AppLogger::instance().warn(QStringLiteral("未连接，无法发送"));
        return;
    }
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return;
    if (hex) {
        bool ok = false;
        const QByteArray bytes = ByteUtils::fromHexText(trimmed, &ok);
        if (!ok) {
            AppLogger::instance().warn(QStringLiteral("HEX 格式错误：需偶数个十六进制字符"));
            return;
        }
        sendBytes(bytes, QStringLiteral("HEX数据"));
    } else {
        sendBytes(trimmed.toUtf8(), QStringLiteral("文本"));
    }
}

void HostViewModel::sendPing()
{
    sendBytes(FrameCodec::pack(0x01, "ping"), QStringLiteral("自定义帧 PING"));
}

void HostViewModel::sendVersion()
{
    sendBytes(FrameCodec::pack(0x02, QByteArray()), QStringLiteral("自定义帧 查询版本"));
}

void HostViewModel::modbusRead(quint8 slave, quint16 start, quint16 count)
{
    setModbusParams(slave, start, count);
    performModbusRead();
}

void HostViewModel::modbusWrite(quint8 slave, quint16 reg, quint16 value)
{
    if (m_protocolIndex == 2)
        sendBytes(Modbus::rtuWriteSingle(slave, reg, value), QStringLiteral("Modbus RTU 写单寄存器"));
    else if (m_protocolIndex == 3)
        sendBytes(Modbus::tcpWriteSingle(m_tid++, slave, reg, value), QStringLiteral("Modbus TCP 写单寄存器"));
}

void HostViewModel::setModbusParams(quint8 slave, quint16 reg, quint16 count)
{
    m_modbusSlave = slave;
    m_modbusReg = reg;
    m_modbusCount = count;
}

void HostViewModel::setPolling(bool enabled, int intervalMs)
{
    m_pollEnabled = enabled;
    m_pollIntervalMs = intervalMs;
    updatePollTimer();
}

void HostViewModel::resetCounters()
{
    m_txBytes = 0;
    m_rxBytes = 0;
    emit countersChanged(m_txBytes, m_rxBytes);
}

// ---------------- 内部实现 ----------------

void HostViewModel::feedCodec(const QByteArray &bytes)
{
    switch (m_protocolIndex) {
    case 1: m_frameCodec.feed(bytes); break;
    case 2: m_rtuCodec.feed(bytes); break;
    case 3: m_tcpCodec.feed(bytes); break;
    case 4: m_lineCodec.feed(bytes); break;
    default: break;
    }
}

void HostViewModel::performModbusRead()
{
    if (m_protocolIndex == 2)
        sendBytes(Modbus::rtuReadHolding(m_modbusSlave, m_modbusReg, m_modbusCount),
                  QStringLiteral("Modbus RTU 轮询读保持寄存器"));
    else if (m_protocolIndex == 3)
        sendBytes(Modbus::tcpReadHolding(m_tid++, m_modbusSlave, m_modbusReg, m_modbusCount),
                  QStringLiteral("Modbus TCP 轮询读保持寄存器"));
}

void HostViewModel::updatePollTimer()
{
    const bool shouldRun = m_transport && m_transport->isOpen()
                           && m_pollEnabled
                           && (m_protocolIndex == 2 || m_protocolIndex == 3);
    if (shouldRun) {
        m_pollTimer->start(m_pollIntervalMs);
        AppLogger::instance().info(QStringLiteral("Modbus 轮询已启动，间隔 %1ms").arg(m_pollIntervalMs));
    } else {
        if (m_pollTimer->isActive())
            AppLogger::instance().info(QStringLiteral("Modbus 轮询已停止"));
        m_pollTimer->stop();
    }
}

void HostViewModel::logFmt(int kind, const QString &text)
{
    AppLogger::Level level = AppLogger::LevelInfo;
    if (kind == KindWarn)
        level = AppLogger::LevelWarn;
    else if (kind == KindError)
        level = AppLogger::LevelError;
    AppLogger::instance().log(level, text);
}

void HostViewModel::onTransportOpened()
{
    if (!m_transport)
        return;
    emit connectionStateChanged(true, m_transport->name(), m_transport->detail());
    updatePollTimer();
    AppLogger::instance().info(
        QStringLiteral("已连接：%1 %2").arg(m_transport->name(), m_transport->detail()));
}

void HostViewModel::onTransportClosed()
{
    m_pollTimer->stop();
    emit connectionStateChanged(false, QString(), QString());
    AppLogger::instance().info(QStringLiteral("连接已断开"));
}

void HostViewModel::onTransportError(const QString &message)
{
    emit messageLine(KindError, QStringLiteral("通道错误：%1").arg(message));
    AppLogger::instance().error(QStringLiteral("通道错误：%1").arg(message));
}

void HostViewModel::onFrameReady(const Frame &frame)
{
    const QString payloadHex = ByteUtils::toHexDisplay(frame.payload);
    emit messageLine(KindRx,
                     QStringLiteral("自定义帧 cmd=0x%1(%2) payload=[%3]")
                         .arg(frame.cmd, 2, 16, QLatin1Char('0'))
                         .arg(customCmdName(frame.cmd),
                              payloadHex.isEmpty() ? QStringLiteral("空") : payloadHex));
}

void HostViewModel::sendBytes(const QByteArray &bytes, const QString &label)
{
    if (!m_transport || !m_transport->isOpen()) {
        AppLogger::instance().warn(QStringLiteral("未连接，无法发送"));
        return;
    }
    emit messageLine(KindTx, label);
    m_transport->write(bytes);
}

QString HostViewModel::customCmdName(quint8 cmd)
{
    switch (cmd) {
    case 0x01: return QStringLiteral("回显");
    case 0x02: return QStringLiteral("版本");
    case 0x81: return QStringLiteral("回显应答");
    case 0x82: return QStringLiteral("版本应答");
    default:   return QStringLiteral("自定义");
    }
}
