#pragma once

#include "core/ITransport.h"
#include "core/MqttSettings.h"

#include <QByteArray>
#include <QTimer>

class QTcpSocket;

/**
 * MqttTransport —— MQTT 3.1.1 通道（QoS0，内置协议实现，不依赖外部库）。
 *
 * 作为 ITransport 的"载荷级"实现接入框架：
 *   - 收到订阅主题的 PUBLISH → emit bytesReceived(payload)，照常走协议解码/报文表格；
 *   - write(payload) → 向发布主题发 PUBLISH(QoS0)，emit bytesSent(payload)；
 *   - 连接流程：TCP 连接 → CONNECT → CONNACK(ok) → SUBSCRIBE → opened；
 *   - 保活：keepAlive/2 间隔发 PINGREQ；CONNACK 前发生错误/拒绝 → errorHappened。
 *
 * 主题信息额外经 messageReceived(topic, payload) 暴露，供 VM 写日志。
 */
class MqttTransport : public ITransport
{
    Q_OBJECT
public:
    explicit MqttTransport(const MqttSettings &settings, QObject *parent = nullptr);
    ~MqttTransport() override;

    bool open(QString *errMsg = nullptr) override;
    void close() override;
    bool isOpen() const override;
    QString name() const override { return QStringLiteral("MQTT"); }
    QString detail() const override;
    void write(const QByteArray &bytes) override;

signals:
    // 额外暴露主题（ITransport 之外的能力，VM 可选订阅）
    void messageReceived(const QString &topic, const QByteArray &payload);

private slots:
    void onConnected();
    void onReadyRead();
    void onSocketError();
    void onSocketDisconnected();
    void onKeepAlive();

private:
    void sendConnect();
    void sendSubscribe(const QString &topic, quint16 packetId);
    void sendPingReq();
    void sendDisconnect();
    void writePacket(quint8 header, const QByteArray &variableAndPayload);
    void processBuffer();

    QTcpSocket *m_socket = nullptr;
    MqttSettings m_settings;
    QByteArray m_buffer;
    bool m_tcpConnected = false;   // TCP 已连上
    bool m_connAcked = false;      // CONNACK 成功（=通道可用）
    bool m_closing = false;
    quint16 m_packetId = 1;
    QTimer m_keepAliveTimer;
};
