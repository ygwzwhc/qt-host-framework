#include "core/protocols/ModbusProtocol.h"

#include <QtGlobal>

namespace {

// Modbus CRC16（poly 0xA001, 初值 0xFFFF，低字节在前）
quint16 modbusCrc(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < data.size(); ++i) {
        crc ^= quint16(quint8(data.at(i)));
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x0001)
                crc = quint16((crc >> 1) ^ 0xA001);
            else
                crc = quint16(crc >> 1);
        }
    }
    return crc;
}

quint16 be16(const QByteArray &data, int pos)
{
    return quint16((quint8(data.at(pos)) << 8) | quint8(data.at(pos + 1)));
}

ParsedPacket makePacket(ParsedPacket::Kind kind, quint8 fn, const QString &summary,
                        const QByteArray &payload = {})
{
    ParsedPacket p;
    p.kind = kind;
    p.code = fn;
    p.summary = summary;
    p.payload = payload;
    return p;
}

} // namespace

namespace Modbus {

quint16 crc16(const QByteArray &data)
{
    return modbusCrc(data);
}

QByteArray rtuReadHolding(quint8 slave, quint16 start, quint16 count)
{
    QByteArray pdu;
    pdu.append(char(slave));
    pdu.append(char(0x03));
    pdu.append(char(quint8(start >> 8)));
    pdu.append(char(quint8(start & 0xFF)));
    pdu.append(char(quint8(count >> 8)));
    pdu.append(char(quint8(count & 0xFF)));
    const quint16 crc = modbusCrc(pdu);
    pdu.append(char(quint8(crc & 0xFF)));        // RTU: CRC 低字节在前
    pdu.append(char(quint8(crc >> 8)));
    return pdu;
}

QByteArray rtuWriteSingle(quint8 slave, quint16 reg, quint16 value)
{
    QByteArray pdu;
    pdu.append(char(slave));
    pdu.append(char(0x06));
    pdu.append(char(quint8(reg >> 8)));
    pdu.append(char(quint8(reg & 0xFF)));
    pdu.append(char(quint8(value >> 8)));
    pdu.append(char(quint8(value & 0xFF)));
    const quint16 crc = modbusCrc(pdu);
    pdu.append(char(quint8(crc & 0xFF)));
    pdu.append(char(quint8(crc >> 8)));
    return pdu;
}

QByteArray tcpReadHolding(quint16 tid, quint8 unit, quint16 start, quint16 count)
{
    QByteArray pdu;
    pdu.append(char(0x03));
    pdu.append(char(quint8(start >> 8)));
    pdu.append(char(quint8(start & 0xFF)));
    pdu.append(char(quint8(count >> 8)));
    pdu.append(char(quint8(count & 0xFF)));
    const quint16 len = quint16(pdu.size() + 1); // unit + pdu
    QByteArray adu;
    adu.append(char(quint8(tid >> 8)));
    adu.append(char(quint8(tid & 0xFF)));
    adu.append(char(0));  // protocol id 高字节
    adu.append(char(0));  // protocol id 低字节
    adu.append(char(quint8(len >> 8)));
    adu.append(char(quint8(len & 0xFF)));
    adu.append(char(unit));
    adu.append(pdu);
    return adu;
}

QByteArray tcpWriteSingle(quint16 tid, quint8 unit, quint16 reg, quint16 value)
{
    QByteArray pdu;
    pdu.append(char(0x06));
    pdu.append(char(quint8(reg >> 8)));
    pdu.append(char(quint8(reg & 0xFF)));
    pdu.append(char(quint8(value >> 8)));
    pdu.append(char(quint8(value & 0xFF)));
    const quint16 len = quint16(pdu.size() + 1);
    QByteArray adu;
    adu.append(char(quint8(tid >> 8)));
    adu.append(char(quint8(tid & 0xFF)));
    adu.append(char(0));
    adu.append(char(0));
    adu.append(char(quint8(len >> 8)));
    adu.append(char(quint8(len & 0xFF)));
    adu.append(char(unit));
    adu.append(pdu);
    return adu;
}

QString fnName(quint8 fn)
{
    switch (fn) {
    case 0x01: return QStringLiteral("读线圈");
    case 0x02: return QStringLiteral("读离散输入");
    case 0x03: return QStringLiteral("读保持寄存器");
    case 0x04: return QStringLiteral("读输入寄存器");
    case 0x05: return QStringLiteral("写单线圈");
    case 0x06: return QStringLiteral("写单寄存器");
    case 0x0F: return QStringLiteral("写多线圈");
    case 0x10: return QStringLiteral("写多寄存器");
    default:   return QStringLiteral("功能码0x%1").arg(fn, 2, 16, QLatin1Char('0'));
    }
}

} // namespace Modbus

// ================= RtuCodec =================

ModbusCodecBase::ModbusCodecBase(QObject *parent)
    : QObject(parent)
{
}

ModbusRtuCodec::ModbusRtuCodec(QObject *parent)
    : ModbusCodecBase(parent)
{
}

void ModbusRtuCodec::reset()
{
    m_buffer.clear();
}

void ModbusRtuCodec::feed(const QByteArray &chunk)
{
    m_buffer.append(chunk);
    tryExtract();
}

// 已知功能码的响应帧长度（不含从站地址与 CRC）
static int rtuPduRespLen(quint8 fn, quint8 firstData)
{
    switch (fn) {
    case 0x01: case 0x02: case 0x03: case 0x04:
        return 1 + firstData;              // 字节数 + 数据
    case 0x05: case 0x06:
        return 4;                          // 地址+值
    case 0x0F: case 0x10:
        return 4;                          // 起始地址+数量
    default:
        return -1;                         // 未知
    }
}

void ModbusRtuCodec::tryExtract()
{
    // 最小帧：addr(1)+fn(1)+crc(2) = 4
    while (m_buffer.size() >= 4) {
        const quint8 slave = quint8(m_buffer.at(0));
        const quint8 fnRaw = quint8(m_buffer.at(1));
        const bool isException = (fnRaw & 0x80) != 0;
        const quint8 fn = quint8(fnRaw & 0x7F);

        int total = -1;
        if (isException) {
            total = 5;                     // addr fn(0x80|) excCode crc2
        } else if (fn == 0x03 || fn == 0x04) {
            if (m_buffer.size() < 3)
                return;                    // 等字节计数
            const quint8 bc = quint8(m_buffer.at(2));
            total = 3 + 1 + bc + 2;        // addr fn bc data crc2 -> 3+1+bc+2
        } else {
            const int respLen = rtuPduRespLen(fn, m_buffer.size() > 2 ? quint8(m_buffer.at(2)) : 0);
            if (respLen > 0)
                total = 2 + respLen + 2;
            else
                total = -1;
        }

        if (total < 0) {
            // 未知功能码：等缓冲不再增长再整体告警（简化处理）
            if (m_buffer.size() > 256) {
                emit packetParsed(makePacket(ParsedPacket::Warn, fn,
                                             QStringLiteral("未知功能码，数据过长已丢弃")));
                m_buffer.clear();
            }
            return;
        }
        if (m_buffer.size() < total)
            return;                        // 半包等待

        const QByteArray raw = m_buffer.left(total);
        m_buffer.remove(0, total);

        // CRC 校验（除末尾 2 字节）
        const quint16 calc = modbusCrc(raw.left(total - 2));
        const quint16 recv = quint16(quint8(raw.at(total - 2)))
                           | quint16(quint8(raw.at(total - 1))) << 8;
        if (calc != recv) {
            emit packetParsed(makePacket(ParsedPacket::Warn, fn,
                                         QStringLiteral("CRC 校验失败")));
            continue;
        }

        if (isException) {
            const quint8 exc = quint8(raw.at(2));
            emit packetParsed(makePacket(ParsedPacket::Warn, fn,
                                         QStringLiteral("从站%1 异常响应 代码%2")
                                             .arg(slave).arg(exc),
                                         raw));
            continue;
        }

        QString detail = QStringLiteral("从站%1 %2").arg(slave).arg(Modbus::fnName(fn));
        if (fn == 0x03 || fn == 0x04) {
            const quint8 bc = quint8(raw.at(2));
            QStringList regs;
            for (int i = 0; i + 1 < bc; i += 2)
                regs.append(QString::number(be16(raw, 3 + i)));
            detail += QStringLiteral(" 值=[%1]").arg(regs.join(QStringLiteral(", ")));
        } else if (fn == 0x06) {
            detail += QStringLiteral(" 寄存器%1=%2")
                          .arg(be16(raw, 2))
                          .arg(be16(raw, 4));
        }
        emit packetParsed(makePacket(ParsedPacket::Info, fn, detail, raw));
    }
}

// ================= TcpCodec =================

ModbusTcpCodec::ModbusTcpCodec(QObject *parent)
    : ModbusCodecBase(parent)
{
}

void ModbusTcpCodec::reset()
{
    m_buffer.clear();
}

void ModbusTcpCodec::feed(const QByteArray &chunk)
{
    m_buffer.append(chunk);
    tryExtract();
}

void ModbusTcpCodec::tryExtract()
{
    // MBAP 头 7 字节：tid(2) pid(2) len(2) unit(1)
    while (m_buffer.size() >= 7) {
        if (quint8(m_buffer.at(2)) != 0 || quint8(m_buffer.at(3)) != 0) {
            // 协议标识非 0，丢弃 1 字节重新同步
            m_buffer.remove(0, 1);
            continue;
        }
        const quint16 tid = be16(m_buffer, 0);
        const quint16 len = be16(m_buffer, 4);
        const int total = 6 + len;         // 6 字节头(不含 unit) + len(unit+pdu)
        if (len < 2 || len > 260) {        // 非法长度，逐字节重同步
            m_buffer.remove(0, 1);
            continue;
        }
        if (m_buffer.size() < total)
            return;                        // 半包

        const QByteArray raw = m_buffer.left(total);
        m_buffer.remove(0, total);

        const quint8 unit = quint8(raw.at(6));
        const quint8 fnRaw = quint8(raw.at(7));
        const bool isException = (fnRaw & 0x80) != 0;
        const quint8 fn = quint8(fnRaw & 0x7F);

        if (isException) {
            emit packetParsed(makePacket(ParsedPacket::Warn, fn,
                                         QStringLiteral("unit=%1 异常响应 tid=%2")
                                             .arg(unit).arg(tid),
                                         raw));
            continue;
        }

        QString detail = QStringLiteral("unit=%1 tid=%2 %3").arg(unit).arg(tid).arg(Modbus::fnName(fn));
        if ((fn == 0x03 || fn == 0x04) && raw.size() >= 9) {
            const quint8 bc = quint8(raw.at(8));
            QStringList regs;
            for (int i = 0; i + 1 < bc; i += 2)
                regs.append(QString::number(be16(raw, 9 + i)));
            detail += QStringLiteral(" 值=[%1]").arg(regs.join(QStringLiteral(", ")));
        } else if (fn == 0x06 && raw.size() >= 12) {
            detail += QStringLiteral(" 寄存器%1=%2").arg(be16(raw, 8)).arg(be16(raw, 10));
        }
        emit packetParsed(makePacket(ParsedPacket::Info, fn, detail, raw));
    }
}
