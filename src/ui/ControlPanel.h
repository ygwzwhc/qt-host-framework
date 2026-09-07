#pragma once

#include <QWidget>

#include "core/SerialConfig.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTimer;

/**
 * 左侧控制面板：通讯方式选择 + 参数配置 + 协议选择 + 发送/快捷指令。
 * 所有连接参数通过 QSettings 持久化。
 */
class ControlPanel : public QWidget
{
    Q_OBJECT
public:
    enum TransportType {
        Serial = 0,
        TcpClient,
        TcpServer,
        Udp,
        Loopback,
        Mqtt
    };

    struct ConnParams
    {
        TransportType type = Serial;
        SerialConfig serial;          // 串口参数
        QString tcpHost = QStringLiteral("127.0.0.1");
        quint16 tcpPort = 9000;       // TCP 客户端目标端口 / 服务器监听端口
        quint16 udpLocalPort = 9000;
        quint16 udpTargetPort = 9001;
        // MQTT
        QString mqttHost = QStringLiteral("127.0.0.1");
        quint16 mqttPort = 1883;
        QString mqttPub = QStringLiteral("qt-host/tx");
        QString mqttSub = QStringLiteral("qt-host/rx");
    };

    explicit ControlPanel(QWidget *parent = nullptr);

    // 供工具栏"连接"动作复用面板的连接逻辑
    void requestConnect();

    ConnParams currentParams() const;
    int protocolIndex() const;
    void setConnected(bool connected);
    void setPortList(const QStringList &ports);

    // Modbus 轮询参数读取（MainWindow 定时器使用）
    quint8 modbusSlave() const;
    quint16 modbusReg() const;
    quint16 modbusCount() const;
    bool pollingEnabled() const;
    int pollingIntervalMs() const;

    // 持久化当前面板配置（成功连接 / 窗口关闭时调用）
    void saveSettings();

signals:
    void connectRequested(const ControlPanel::ConnParams &params);
    void disconnectRequested();
    void protocolChanged(int index);
    void refreshPortsRequested();
    void sendRequested(const QString &text, bool isHex);
    void quickPingRequested();
    void quickVersionRequested();
    void modbusReadRequested(quint8 slave, quint16 start, quint16 count);
    void modbusWriteRequested(quint8 slave, quint16 reg, quint16 value);
    void modbusPollToggled(bool enabled, int intervalMs);
    void modbusParamsChanged(quint8 slave, quint16 reg, quint16 count);

private:
    QWidget *createSerialPage();
    QWidget *createTcpClientPage();
    QWidget *createTcpServerPage();
    QWidget *createUdpPage();
    QWidget *createLoopbackPage();
    QWidget *createMqttPage();

private slots:
    void onConnectClicked();
    void onSendClicked();
    void onPeriodicToggled(bool checked);
    void onPeriodicTimeout();
    void onPollToggled(bool checked);
    void updateQuickVisibility();

private:
    void restoreSettings();
    QByteArray encodeCurrent(bool *ok) const;

    QComboBox *m_transportCombo = nullptr;
    QStackedWidget *m_stack = nullptr;

    // 串口页
    QComboBox *m_portCombo = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_stopCombo = nullptr;

    // TCP/UDP 页
    QLineEdit *m_tcpHostEdit = nullptr;
    QSpinBox *m_tcpPortSpin = nullptr;
    QSpinBox *m_serverPortSpin = nullptr;
    QSpinBox *m_udpLocalSpin = nullptr;
    QLineEdit *m_udpHostEdit = nullptr;
    QSpinBox *m_udpTargetSpin = nullptr;

    // MQTT 页
    QLineEdit *m_mqttHostEdit = nullptr;
    QSpinBox *m_mqttPortSpin = nullptr;
    QLineEdit *m_mqttPubEdit = nullptr;
    QLineEdit *m_mqttSubEdit = nullptr;

    // 状态
    QPushButton *m_connectBtn = nullptr;
    QLabel *m_statusDot = nullptr;
    QLabel *m_statusText = nullptr;
    bool m_connected = false;

    // 协议
    QComboBox *m_protocolCombo = nullptr;

    // 发送
    QLineEdit *m_inputEdit = nullptr;
    QCheckBox *m_hexCheck = nullptr;
    QCheckBox *m_crlfCheck = nullptr;
    QCheckBox *m_periodicCheck = nullptr;
    QSpinBox *m_periodSpin = nullptr;
    QTimer *m_sendTimer = nullptr;

    // 快捷指令
    QGroupBox *m_customGroup = nullptr;
    QGroupBox *m_modbusGroup = nullptr;
    QSpinBox *m_slaveSpin = nullptr;
    QSpinBox *m_regSpin = nullptr;
    QSpinBox *m_countSpin = nullptr;
    QSpinBox *m_valueSpin = nullptr;
    QCheckBox *m_pollCheck = nullptr;
    QSpinBox *m_pollSpin = nullptr;
};
