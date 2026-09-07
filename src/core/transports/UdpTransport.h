#pragma once

#include "core/ITransport.h"

#include <QUdpSocket>

/**
 * UdpTransport —— UDP 通道。
 * 本地绑定端口收包；write() 向目标地址单播发送。
 */
class UdpTransport : public ITransport
{
    Q_OBJECT
public:
    UdpTransport(quint16 localPort, const QString &targetHost, quint16 targetPort,
                 QObject *parent = nullptr);

    bool open(QString *errMsg = nullptr) override;
    void close() override;
    bool isOpen() const override;
    QString name() const override { return QStringLiteral("UDP"); }
    QString detail() const override { return QStringLiteral("本地:%1 → %2:%3")
                                                    .arg(m_localPort)
                                                    .arg(m_targetHost)
                                                    .arg(m_targetPort); }
    void write(const QByteArray &bytes) override;

private:
    QUdpSocket *m_socket = nullptr;
    quint16 m_localPort = 0;
    QString m_targetHost;
    quint16 m_targetPort = 0;
};
