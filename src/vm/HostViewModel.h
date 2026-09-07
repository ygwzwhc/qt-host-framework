#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>

#include "core/FrameCodec.h"
#include "core/MqttSettings.h"
#include "core/SerialConfig.h"
#include "core/protocols/AsciiLineCodec.h"
#include "core/protocols/ModbusProtocol.h"

class QTimer;
class ITransport;

/**
 * HostViewModel —— MVVM 的 ViewModel 层（不依赖任何 QtWidgets 类型）。
 *
 * 职责：
 *  1. 暴露"命令"（slot，由 View 调用）：连接/断开/选择协议/发送/Modbus 指令/轮询；
 *  2. 暴露"可观察状态"（信号）：连接状态、协议名、端口列表、收发计数、原始字节、解析报文；
 *  3. 持有全部业务对象：传输层(ITransport*)、协议解码器、轮询定时器。
 *
 * Model = core/(传输层 + 协议解码器 + AppLogger)；
 * View  = MainWindow / ControlPanel（只做 UI 布局与信号接线，不含业务判断）。
 *
 * View 侧的 UI 参数（如串口配置、HEX 开关）由 View 自行收集，
 * 通过"标量命令参数"传入，因此本类不引用任何 UI 头文件。
 */
class HostViewModel : public QObject
{
    Q_OBJECT
public:
    // 通道类型：与控制面板下拉顺序一致（Serial=0..Loopback=4）
    enum ChannelType {
        ChannelSerial = 0,
        ChannelTcpClient,
        ChannelTcpServer,
        ChannelUdp,
        ChannelLoopback,
        ChannelMqtt
    };

    // 报文行种类（View 按 kind 决定颜色/图标）
    enum MessageKind {
        KindInfo = 0,   // 一般信息
        KindRx = 1,     // 收到解析帧 / 原始 RX
        KindTx = 2,     // 主动发送帧标签
        KindWarn = 3,   // 协议告警（CRC 错、异常应答…）
        KindError = 4   // 错误
    };

    explicit HostViewModel(QObject *parent = nullptr);

    // ---- 命令（View → VM）----
public slots:
    void openChannel(int channelType, const SerialConfig &serial, const QString &host,
                     int port1, int port2, const MqttSettings &mqtt = MqttSettings());
    void closeChannel();
    void refreshPorts();
    void setProtocol(int index);
    void sendText(const QString &text, bool hex);
    void sendPing();
    void sendVersion();
    void modbusRead(quint8 slave, quint16 start, quint16 count);
    void modbusWrite(quint8 slave, quint16 reg, quint16 value);
    void setModbusParams(quint8 slave, quint16 reg, quint16 count);
    void setPolling(bool enabled, int intervalMs);
    void resetCounters();

    // ---- 可观察状态（VM → View）----
signals:
    void connectionStateChanged(bool connected, const QString &transportName,
                                const QString &detail);
    void protocolNameChanged(const QString &name);
    void portsChanged(const QStringList &ports);
    void countersChanged(qint64 txBytes, qint64 rxBytes);
    void rawLine(bool tx, const QByteArray &bytes);
    void messageLine(int kind, const QString &text);

private:
    void feedCodec(const QByteArray &bytes);
    void performModbusRead();
    void updatePollTimer();
    void logFmt(int kind, const QString &text);
    void onTransportOpened();
    void onTransportClosed();
    void onTransportError(const QString &message);
    void onFrameReady(const Frame &frame);
    void sendBytes(const QByteArray &bytes, const QString &label);
    static QString customCmdName(quint8 cmd);

    ITransport *m_transport = nullptr;

    FrameCodec m_frameCodec;
    ModbusRtuCodec m_rtuCodec;
    ModbusTcpCodec m_tcpCodec;
    AsciiLineCodec m_lineCodec;

    int m_protocolIndex = 0;
    int m_pollIntervalMs = 1000;
    bool m_pollEnabled = false;
    quint16 m_tid = 1;
    qint64 m_txBytes = 0;
    qint64 m_rxBytes = 0;

    // Modbus 当前参数（读/写/轮询共用，由 View 转发）
    quint8 m_modbusSlave = 1;
    quint16 m_modbusReg = 0;
    quint16 m_modbusCount = 1;

    QTimer *m_pollTimer = nullptr;
};
