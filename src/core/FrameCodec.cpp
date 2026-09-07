#include "core/FrameCodec.h"

FrameCodec::FrameCodec(QObject *parent)
    : QObject(parent)
{
}

void FrameCodec::feed(const QByteArray &chunk)
{
    m_buffer.append(chunk);

    // 只要缓冲里还有可能构成帧的数据就持续解析
    while (m_buffer.size() >= 4) { // 至少拿到 CMD/LEN 才能算总长
        const int head = findHeader();
        if (head < 0) {
            trimUselessTail(); // 无有效帧头，清掉废数据（保留可能跨包的尾部 AA）
            return;
        }
        if (head > 0)
            m_buffer.remove(0, head); // 丢弃帧头前的垃圾字节

        if (m_buffer.size() < 4) // AA 55 落在缓冲尾部，等下一段数据
            return;

        const quint8 len = quint8(m_buffer.at(3));
        const int total = kMinFrameSize + len; // 6 + len
        if (m_buffer.size() < total)
            return; // 半包，等下一段

        const QByteArray raw = m_buffer.left(total);
        m_buffer.remove(0, total);

        // CRC 校验范围：CMD + LEN + PAYLOAD（去掉 AA 55 与 CRC 本身）
        const quint16 crcCalc = crc16(raw.mid(2, 2 + len));
        const quint16 crcRecv = (quint16(quint8(raw.at(4 + len))) << 8)
                              | quint16(quint8(raw.at(5 + len)));
        if (crcCalc != crcRecv) {
            emit codecError(QStringLiteral("CRC 校验失败，丢弃一帧（期望 %1，收到 %2）")
                                .arg(crcCalc, 4, 16)
                                .arg(crcRecv, 4, 16));
            continue;
        }

        Frame frame;
        frame.cmd = quint8(raw.at(2));
        frame.payload = raw.mid(4, len);
        emit frameReady(frame);
    }

    // 剩余不足 4 字节：只可能是"帧头前缀"，若缓冲恰好是 AA / AA55 则保留，
    // 否则说明是脏数据，直接清掉
    const bool keep =
        (m_buffer.size() == 1 && quint8(m_buffer.at(0)) == kHead0)
        || (m_buffer.size() >= 2 && quint8(m_buffer.at(0)) == kHead0
            && quint8(m_buffer.at(1)) == kHead1);
    if (!keep)
        m_buffer.clear();
}

void FrameCodec::reset()
{
    m_buffer.clear();
}

QByteArray FrameCodec::pack(quint8 cmd, const QByteArray &payload)
{
    QByteArray raw;
    raw.reserve(kMinFrameSize + payload.size());
    raw.append(char(kHead0));
    raw.append(char(kHead1));
    raw.append(char(cmd));
    raw.append(char(quint8(payload.size())));
    raw.append(payload);

    const quint16 crc = crc16(raw.mid(2)); // CMD + LEN + PAYLOAD
    raw.append(char(quint8(crc >> 8)));
    raw.append(char(quint8(crc & 0xFF)));
    return raw;
}

int FrameCodec::findHeader() const
{
    for (int i = 0; i + 1 < m_buffer.size(); ++i) {
        if (quint8(m_buffer.at(i)) == kHead0 && quint8(m_buffer.at(i + 1)) == kHead1)
            return i;
    }
    return -1;
}

void FrameCodec::trimUselessTail()
{
    // 末尾可能是 AA（帧头前半），需要保留 1 字节等待下一段补成 AA 55
    if (!m_buffer.isEmpty() && quint8(m_buffer.at(m_buffer.size() - 1)) == kHead0) {
        const char keep = m_buffer.at(m_buffer.size() - 1);
        m_buffer.clear();
        m_buffer.append(keep);
    } else {
        m_buffer.clear();
    }
}

quint16 FrameCodec::crc16(const QByteArray &data)
{
    // CRC16-CCITT：poly 0x1021, init 0xFFFF
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= quint16(quint8(data.at(i))) << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000)
                crc = quint16((crc << 1) ^ 0x1021);
            else
                crc = quint16(crc << 1);
        }
    }
    return crc;
}
