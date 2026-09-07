#pragma once

#include "core/ITransport.h"

#include <QList>
#include <QTcpServer>
#include <QTcpSocket>

/**
 * TcpServerTransport —— TCP 服务器（监听模式，向所有已连接客户端广播写入）。
 */
class TcpServerTransport : public ITransport
{
    Q_OBJECT
public:
    TcpServerTransport(quint16 listenPort, QObject *parent = nullptr);

    bool open(QString *errMsg = nullptr) override;
    void close() override;
    bool isOpen() const override;
    QString name() const override { return QStringLiteral("TCP服务器"); }
    QString detail() const override { return QStringLiteral("监听 %1 端口").arg(m_port); }
    void write(const QByteArray &bytes) override;

private:
    QTcpServer *m_server = nullptr;
    QList<QTcpSocket *> m_clients;
    quint16 m_port = 0;
};
