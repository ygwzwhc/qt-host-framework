#pragma once

#include "core/ITransport.h"
#include "core/SerialConfig.h"

#include <QtSerialPort/QSerialPort>

/**
 * SerialTransport —— 串口传输实现。
 */
class SerialTransport : public ITransport
{
    Q_OBJECT
public:
    explicit SerialTransport(const SerialConfig &cfg, QObject *parent = nullptr);
    ~SerialTransport() override;

    bool open(QString *errMsg = nullptr) override;
    void close() override;
    bool isOpen() const override;
    QString name() const override { return QStringLiteral("串口"); }
    QString detail() const override;
    void write(const QByteArray &bytes) override;

    static QStringList availablePortNames();

private:
    QSerialPort *m_port = nullptr;
    SerialConfig m_cfg;
};
