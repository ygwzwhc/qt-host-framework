#include "core/transports/LoopbackTransport.h"

LoopbackTransport::LoopbackTransport(QObject *parent)
    : ITransport(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(80);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        emit bytesReceived(m_pending);
        m_pending.clear();
    });
}

bool LoopbackTransport::open(QString *)
{
    close();
    m_open = true;
    emit opened();
    return true;
}

void LoopbackTransport::close()
{
    if (m_open) {
        m_open = false;
        m_timer.stop();
        m_pending.clear();
        emit closed();
    }
}

void LoopbackTransport::write(const QByteArray &bytes)
{
    if (!m_open)
        return;
    emit bytesSent(bytes);
    m_pending.append(bytes);
    m_timer.start(); // 每次写入重启定时器，模拟设备稍后回包
}
