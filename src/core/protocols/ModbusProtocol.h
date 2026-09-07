#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>

#include "core/protocols/ProtocolTypes.h"

/**
 * Modbus 协议工具集（RTU 帧 + TCP 帧 的组包与流式解析）。
 *
 * 组包（上位机 → 从站）：
 *   rtuReadHolding(slave,start,count) : 0x03 读保持寄存器
 *   rtuWriteSingle(slave,reg,value)   : 0x06 写单个寄存器
 *   tcp 系列同语义，带 MBAP 头
 * 解析：
 *   RtuCodec / TcpCodec 均为流式解码器，feed() 喂原始字节，
 *   逐帧发 packetParsed()（CRC/长度校验）。
 */
namespace Modbus {

quint16 crc16(const QByteArray &data);

// ---- RTU 组包 ----
QByteArray rtuReadHolding(quint8 slave, quint16 start, quint16 count);
QByteArray rtuWriteSingle(quint8 slave, quint16 reg, quint16 value);

// ---- TCP(MBAP) 组包 ----
QByteArray tcpReadHolding(quint16 transactionId, quint8 unit, quint16 start, quint16 count);
QByteArray tcpWriteSingle(quint16 transactionId, quint8 unit, quint16 reg, quint16 value);

QString fnName(quint8 fn);

} // namespace Modbus

// ============ 流式解码器基类 ============
class ModbusCodecBase : public QObject
{
    Q_OBJECT
public:
    explicit ModbusCodecBase(QObject *parent = nullptr);
    virtual void reset() = 0;
    virtual void feed(const QByteArray &chunk) = 0;

signals:
    void packetParsed(const ParsedPacket &packet);
};

// ---- Modbus RTU 流式解码器 ----
class ModbusRtuCodec : public ModbusCodecBase
{
    Q_OBJECT
public:
    explicit ModbusRtuCodec(QObject *parent = nullptr);
    void reset() override;
    void feed(const QByteArray &chunk) override;

private:
    void tryExtract();
    QByteArray m_buffer;
};

// ---- Modbus TCP(MBAP) 流式解码器 ----
class ModbusTcpCodec : public ModbusCodecBase
{
    Q_OBJECT
public:
    explicit ModbusTcpCodec(QObject *parent = nullptr);
    void reset() override;
    void feed(const QByteArray &chunk) override;

private:
    void tryExtract();
    QByteArray m_buffer;
};
