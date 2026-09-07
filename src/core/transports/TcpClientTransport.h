#pragma once

#include "core/ITransport.h"

#include <QTcpServer>
#include <QTcpSocket>

/**
 * TcpClientTransport —— TCP 客户端。
 * open() 为异步连接，连接成功后发 opened()。
 */
class TcpClientTransport : public ITransport
{
    Q_OBJECT
public:
    TcpClientTransport(const QString &host, quint16 port, QObject *parent = nullptr);

    bool open(QString *errMsg = nullptr) override;
    void close() override;
    bool isOpen() const override;
    QString name() const override { return QStringLiteral("TCP客户端"); }
    QString detail() const override { return QStringLiteral("%1:%2").arg(m_host).arg(m_port); }
    void write(const QByteArray &bytes) override;

private:
    QTcpSocket *m_socket = nullptr;
    QString m_host;
    quint16 m_port = 0;
};
