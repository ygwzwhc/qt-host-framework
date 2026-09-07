#include "core/protocols/AsciiLineCodec.h"

AsciiLineCodec::AsciiLineCodec(QObject *parent)
    : QObject(parent)
{
}

void AsciiLineCodec::reset()
{
    m_buffer.clear();
}

void AsciiLineCodec::feed(const QByteArray &chunk)
{
    m_buffer.append(chunk);
    int nl = 0;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        QByteArray line = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);
        if (line.endsWith('\r'))
            line.chop(1);

        ParsedPacket p;
        p.kind = ParsedPacket::Info;
        p.code = 0x00;
        const QString text = QString::fromUtf8(line);
        p.summary = QStringLiteral("文本行: %1").arg(text.isEmpty() ? QStringLiteral("(空行)") : text);
        p.payload = line;
        emit packetParsed(p);
    }
}
