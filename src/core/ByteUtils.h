#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

/**
 * 基础字节/字符串互转工具（头文件实现，UI 层与协议层共用）。
 */
namespace ByteUtils {

// 将 "AA 55 01" / "AA,55" / "AA-55" 之类的文本解析为原始字节。
// 支持的分隔符：空格、逗号、分号、横线、冒号。
// 失败（空串或含非法字符、奇数个字符）时 okOut=false 并返回空数组。
inline QByteArray fromHexText(const QString &input, bool *okOut = nullptr)
{
    QString s = input.simplified();
    s.remove(QLatin1Char(' '));
    s.remove(QLatin1Char(','));
    s.remove(QLatin1Char(';'));
    s.remove(QLatin1Char('-'));
    s.remove(QLatin1Char(':'));

    bool ok = !s.isEmpty() && (s.size() % 2 == 0);
    QByteArray out;
    if (ok) {
        for (int i = 0; i + 1 < s.size(); i += 2) {
            bool byteOk = false;
            const int v = s.mid(i, 2).toInt(&byteOk, 16);
            if (!byteOk || v < 0 || v > 255) {
                ok = false;
                out.clear();
                break;
            }
            out.append(char(v));
        }
    }
    if (okOut)
        *okOut = ok;
    return out;
}

// 把原始字节转为大写十六进制显示文本，如 "AA 55 01"。
inline QString toHexDisplay(const QByteArray &data)
{
    static const char kHex[] = "0123456789ABCDEF";
    QString out;
    out.reserve(data.size() * 3);
    for (int i = 0; i < data.size(); ++i) {
        const quint8 b = quint8(data.at(i));
        out += QLatin1Char(kHex[b >> 4]);
        out += QLatin1Char(kHex[b & 0x0F]);
        out += QLatin1Char(' ');
    }
    if (!out.isEmpty())
        out.chop(1);
    return out;
}

// 可打印化：保留 \r \n \t，其余控制字符显示为 '·'，避免接收区乱码。
inline QString toPrintableText(const QByteArray &data)
{
    QString out;
    out.reserve(data.size());
    for (int i = 0; i < data.size(); ++i) {
        const char c = data.at(i);
        const uchar uc = uchar(c);
        if (c == '\n' || c == '\r' || c == '\t') {
            out += QLatin1Char(c);
        } else if (uc < 0x20 || uc == 0x7F) {
            out += QChar(0x00B7); // ·
        } else {
            out += QLatin1Char(c);
        }
    }
    return out;
}

} // namespace ByteUtils
