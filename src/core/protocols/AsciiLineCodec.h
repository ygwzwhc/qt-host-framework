#pragma once

#include <QObject>
#include <QByteArray>

#include "core/protocols/ProtocolTypes.h"

/**
 * AsciiLineCodec —— 文本行协议（以 \n 结尾的一行算一帧）。
 * 半包自动缓冲，粘包逐行拆分。
 */
class AsciiLineCodec : public QObject
{
    Q_OBJECT
public:
    explicit AsciiLineCodec(QObject *parent = nullptr);
    void reset();
    void feed(const QByteArray &chunk);

signals:
    void packetParsed(const ParsedPacket &packet);

private:
    QByteArray m_buffer;
};
