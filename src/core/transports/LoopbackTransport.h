#pragma once

#include "core/ITransport.h"

#include <QTimer>

/**
 * LoopbackTransport —— 回环自测通道（虚拟"短接串口"）。
 * open 后即可收发；写入的字节会在 80ms 后原样回传，
 * 用于无硬件时验证整条 协议解析→界面显示 链路。
 */
class LoopbackTransport : public ITransport
{
    Q_OBJECT
public:
    explicit LoopbackTransport(QObject *parent = nullptr);

    bool open(QString *errMsg = nullptr) override;
    void close() override;
    bool isOpen() const override { return m_open; }
    QString name() const override { return QStringLiteral("回环测试"); }
    QString detail() const override { return QStringLiteral("虚拟自环(80ms 延时)"); }
    void write(const QByteArray &bytes) override;

private:
    bool m_open = false;
    QByteArray m_pending;
    QTimer m_timer;
};
