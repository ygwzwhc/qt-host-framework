#pragma once

#include <QByteArray>
#include <QString>

/**
 * 协议解析结果（由各协议解码器产生，UI 直接渲染成报文行）。
 */
struct ParsedPacket
{
    enum Kind {
        Info = 0,   // 正常解析出的一帧
        Warn = 1,   // 可识别但异常（如 CRC 错）
        Debug = 2
    };

    Kind kind = Info;
    quint8 code = 0;      // 功能码/命令字
    QString summary;      // 人类可读的解析摘要
    QByteArray payload;   // 原始负载（可选）
};
