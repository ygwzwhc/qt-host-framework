#include "core/transports/SerialTransport.h"

#include <QtSerialPort/QSerialPortInfo>

SerialTransport::SerialTransport(const SerialConfig &cfg, QObject *parent)
    : ITransport(parent)
    , m_cfg(cfg)
{
    m_port = new QSerialPort(this);
    connect(m_port, &QSerialPort::readyRead, this, [this] {
        const QByteArray bytes = m_port->readAll();
        if (!bytes.isEmpty())
            emit bytesReceived(bytes);
    });
    connect(m_port, &QSerialPort::errorOccurred, this,
            [this](QSerialPort::SerialPortError err) {
                if (err == QSerialPort::NoError)
                    return;
                emit errorHappened(m_port->errorString());
                if (!m_port->isOpen()) {
                    m_port->close();
                    emit closed();
                }
            });
}

SerialTransport::~SerialTransport()
{
    close();
}

bool SerialTransport::open(QString *errMsg)
{
    close();
    if (m_cfg.portName.isEmpty()) {
        if (errMsg)
            *errMsg = QStringLiteral("未选择串口");
        return false;
    }
    m_port->setPortName(m_cfg.portName);
    m_port->setBaudRate(m_cfg.baudRate);
    m_port->setDataBits(m_cfg.dataBits);
    m_port->setParity(m_cfg.parity);
    m_port->setStopBits(m_cfg.stopBits);
    m_port->setFlowControl(m_cfg.flowControl);
    if (!m_port->open(QIODevice::ReadWrite)) {
        if (errMsg)
            *errMsg = m_port->errorString();
        return false;
    }
    emit opened();
    return true;
}

void SerialTransport::close()
{
    if (m_port->isOpen()) {
        m_port->close();
        emit closed();
    }
}

bool SerialTransport::isOpen() const
{
    return m_port->isOpen();
}

QString SerialTransport::detail() const
{
    return QStringLiteral("%1 @ %2").arg(m_cfg.portName).arg(m_cfg.baudRate);
}

void SerialTransport::write(const QByteArray &bytes)
{
    if (m_port->isOpen()) {
        m_port->write(bytes);
        emit bytesSent(bytes);
    }
}

QStringList SerialTransport::availablePortNames()
{
    QStringList names;
    const auto infos = QSerialPortInfo::availablePorts();
    names.reserve(infos.size());
    for (const QSerialPortInfo &info : infos)
        names.append(info.portName());
    return names;
}
