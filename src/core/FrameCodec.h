#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QObject>

/**
 * 解析后的一帧数据。只有字段、不含逻辑，便于在层间以值传递。
 */
struct Frame
{
    quint8 cmd = 0;          // 命令字
    QByteArray payload;      // 负载（不含帧头/长度/CRC）
};

Q_DECLARE_METATYPE(Frame)

/**
 * FrameCodec —— 二进制帧编解码器。
 *
 * 帧格式（本项目约定，可按你的设备协议替换）：
 *   ┌────────┬────────┬──────┬──────┬──────────────┬──────────┐
 *   │ 0xAA   │ 0x55   │ CMD  │ LEN  │ PAYLOAD[LEN] │ CRC16(2B)│
 *   └────────┴────────┴──────┴──────┴──────────────┴──────────┘
 *   - 帧头  : AA 55（2 字节）
 *   - CMD   : 命令字（1 字节）
 *   - LEN   : 负载长度（1 字节，0~255）
 *   - PAYLOAD: 负载
 *   - CRC16 : 对 CMD+LEN+PAYLOAD 计算，CRC16-CCITT(0x1021)，大端存放
 *
 * 使用方式：把串口裸字节流持续 feed() 进来，它内部做"粘包/半包/错位"
 * 处理，拼出完整合法帧后发出 frameReady()。这样上层业务永远只面对
 * "一帧完整的业务数据"，不需要关心字节边界。
 */
class FrameCodec : public QObject
{
    Q_OBJECT
public:
    static constexpr quint8 kHead0 = 0xAA;
    static constexpr quint8 kHead1 = 0x55;
    // 最小帧长：AA 55 CMD LEN CRC(2) = 6 字节（LEN=0 时）
    static constexpr int kMinFrameSize = 6;

    explicit FrameCodec(QObject *parent = nullptr);

    // 将一段裸字节喂入解码器（可能产生 0..N 个完整帧）
    void feed(const QByteArray &chunk);

    // 复位解码器（换设备 / 清空缓冲时调用）
    void reset();

    // 组帧：把业务数据打包成完整帧字节流，供发送
    static QByteArray pack(quint8 cmd, const QByteArray &payload);

signals:
    void frameReady(const Frame &frame);
    void codecError(const QString &message); // CRC 错误、无效数据等

private:
    int findHeader() const;
    void trimUselessTail();
    static quint16 crc16(const QByteArray &data);

    QByteArray m_buffer;
};
