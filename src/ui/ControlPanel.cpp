#include "ui/ControlPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include "core/ByteUtils.h"

namespace {
const QString kKeyTransport = QStringLiteral("conn/transport");
const QString kKeyPort      = QStringLiteral("conn/port");
const QString kKeyBaud      = QStringLiteral("conn/baud");
const QString kKeyData      = QStringLiteral("conn/dataBits");
const QString kKeyParity    = QStringLiteral("conn/parity");
const QString kKeyStop      = QStringLiteral("conn/stopBits");
const QString kKeyTcpHost   = QStringLiteral("conn/tcpHost");
const QString kKeyTcpPort   = QStringLiteral("conn/tcpPort");
const QString kKeySrvPort   = QStringLiteral("conn/serverPort");
const QString kKeyUdpLocal  = QStringLiteral("conn/udpLocal");
const QString kKeyUdpHost   = QStringLiteral("conn/udpHost");
const QString kKeyUdpTarget = QStringLiteral("conn/udpTarget");
const QString kKeyMqttHost  = QStringLiteral("conn/mqttHost");
const QString kKeyMqttPort  = QStringLiteral("conn/mqttPort");
const QString kKeyMqttPub   = QStringLiteral("conn/mqttPub");
const QString kKeyMqttSub   = QStringLiteral("conn/mqttSub");
const QString kKeyProtocol  = QStringLiteral("ui/protocol");
const QString kKeyHex       = QStringLiteral("ui/hex");
const QString kKeyCrlf      = QStringLiteral("ui/crlf");
const QString kKeyPeriod    = QStringLiteral("ui/periodMs");
const QString kKeyPoll      = QStringLiteral("ui/pollMs");
} // namespace

ControlPanel::ControlPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // ---- 通讯方式 ----
    auto *connGroup = new QGroupBox(QStringLiteral("通讯方式"), this);
    auto *connLayout = new QVBoxLayout(connGroup);
    connLayout->setSpacing(8);

    m_transportCombo = new QComboBox(connGroup);
    m_transportCombo->addItem(QStringLiteral("串口"));
    m_transportCombo->addItem(QStringLiteral("TCP 客户端"));
    m_transportCombo->addItem(QStringLiteral("TCP 服务器"));
    m_transportCombo->addItem(QStringLiteral("UDP"));
    m_transportCombo->addItem(QStringLiteral("回环测试"));
    m_transportCombo->addItem(QStringLiteral("MQTT（消息代理）"));
    connLayout->addWidget(m_transportCombo);

    m_stack = new QStackedWidget(connGroup);
    m_stack->addWidget(createSerialPage());
    m_stack->addWidget(createTcpClientPage());
    m_stack->addWidget(createTcpServerPage());
    m_stack->addWidget(createUdpPage());
    m_stack->addWidget(createLoopbackPage());
    m_stack->addWidget(createMqttPage());
    connLayout->addWidget(m_stack);

    auto *statusRow = new QHBoxLayout;
    m_statusDot = new QLabel(QStringLiteral("●"), connGroup);
    m_statusDot->setStyleSheet(QStringLiteral("color:#6e7681; font-size:16px;"));
    m_statusText = new QLabel(QStringLiteral("未连接"), connGroup);
    m_connectBtn = new QPushButton(QStringLiteral("连  接"), connGroup);
    m_connectBtn->setObjectName("connectBtn");
    statusRow->addWidget(m_statusDot);
    statusRow->addWidget(m_statusText);
    statusRow->addStretch(1);
    connLayout->addLayout(statusRow);
    connLayout->addWidget(m_connectBtn);

    // ---- 协议解析 ----
    auto *protoGroup = new QGroupBox(QStringLiteral("协议解析"), this);
    auto *protoLayout = new QVBoxLayout(protoGroup);
    m_protocolCombo = new QComboBox(protoGroup);
    m_protocolCombo->addItem(QStringLiteral("原始透传（不解析）"));
    m_protocolCombo->addItem(QStringLiteral("自定义帧 AA55+CRC16"));
    m_protocolCombo->addItem(QStringLiteral("Modbus RTU"));
    m_protocolCombo->addItem(QStringLiteral("Modbus TCP"));
    m_protocolCombo->addItem(QStringLiteral("文本行 ASCII"));
    protoLayout->addWidget(m_protocolCombo);

    // ---- 数据发送 ----
    auto *sendGroup = new QGroupBox(QStringLiteral("数据发送"), this);
    auto *sendLayout = new QVBoxLayout(sendGroup);
    auto *optRow = new QHBoxLayout;
    m_hexCheck = new QCheckBox(QStringLiteral("HEX"), sendGroup);
    m_crlfCheck = new QCheckBox(QStringLiteral("追加 CRLF"), sendGroup);
    m_crlfCheck->setChecked(true);
    optRow->addWidget(m_hexCheck);
    optRow->addWidget(m_crlfCheck);
    optRow->addStretch(1);
    m_inputEdit = new QLineEdit(sendGroup);
    m_inputEdit->setPlaceholderText(QStringLiteral("输入要发送的内容"));
    auto *sendRow = new QHBoxLayout;
    auto *sendBtn = new QPushButton(QStringLiteral("发送"), sendGroup);
    sendBtn->setObjectName("sendBtn");
    sendRow->addWidget(m_inputEdit, 1);
    sendRow->addWidget(sendBtn);

    auto *periodRow = new QHBoxLayout;
    m_periodicCheck = new QCheckBox(QStringLiteral("周期发送"), sendGroup);
    m_periodSpin = new QSpinBox(sendGroup);
    m_periodSpin->setRange(50, 60000);
    m_periodSpin->setSingleStep(50);
    m_periodSpin->setValue(1000);
    m_periodSpin->setSuffix(QStringLiteral(" ms"));
    m_periodSpin->setEnabled(false);
    periodRow->addWidget(m_periodicCheck);
    periodRow->addWidget(m_periodSpin, 1);
    sendLayout->addLayout(optRow);
    sendLayout->addLayout(sendRow);
    sendLayout->addLayout(periodRow);

    // ---- 快捷指令 ----
    m_customGroup = new QGroupBox(QStringLiteral("自定义帧指令"), this);
    auto *customLayout = new QHBoxLayout(m_customGroup);
    auto *pingBtn = new QPushButton(QStringLiteral("PING (0x01)"), m_customGroup);
    auto *verBtn = new QPushButton(QStringLiteral("查询版本 (0x02)"), m_customGroup);
    customLayout->addWidget(pingBtn);
    customLayout->addWidget(verBtn);

    m_modbusGroup = new QGroupBox(QStringLiteral("Modbus 指令"), this);
    auto *mbForm = new QFormLayout(m_modbusGroup);
    m_slaveSpin = new QSpinBox(m_modbusGroup);
    m_slaveSpin->setRange(1, 247);
    m_slaveSpin->setValue(1);
    m_regSpin = new QSpinBox(m_modbusGroup);
    m_regSpin->setRange(0, 65535);
    m_countSpin = new QSpinBox(m_modbusGroup);
    m_countSpin->setRange(1, 125);
    m_countSpin->setValue(1);
    m_valueSpin = new QSpinBox(m_modbusGroup);
    m_valueSpin->setRange(0, 65535);
    m_valueSpin->setValue(1);
    mbForm->addRow(QStringLiteral("从站地址"), m_slaveSpin);
    mbForm->addRow(QStringLiteral("寄存器地址"), m_regSpin);
    mbForm->addRow(QStringLiteral("读取数量"), m_countSpin);
    mbForm->addRow(QStringLiteral("写入值(0x06)"), m_valueSpin);
    auto *mbRow = new QHBoxLayout;
    auto *readBtn = new QPushButton(QStringLiteral("读 0x03"), m_modbusGroup);
    auto *writeBtn = new QPushButton(QStringLiteral("写 0x06"), m_modbusGroup);
    mbRow->addWidget(readBtn);
    mbRow->addWidget(writeBtn);
    mbForm->addRow(mbRow);

    auto *pollRow = new QHBoxLayout;
    m_pollCheck = new QCheckBox(QStringLiteral("自动轮询"), m_modbusGroup);
    m_pollSpin = new QSpinBox(m_modbusGroup);
    m_pollSpin->setRange(100, 60000);
    m_pollSpin->setSingleStep(100);
    m_pollSpin->setValue(1000);
    m_pollSpin->setSuffix(QStringLiteral(" ms"));
    m_pollSpin->setEnabled(false);
    pollRow->addWidget(m_pollCheck);
    pollRow->addWidget(m_pollSpin, 1);
    mbForm->addRow(pollRow);

    layout->addWidget(connGroup);
    layout->addWidget(protoGroup);
    layout->addWidget(sendGroup);
    layout->addWidget(m_customGroup);
    layout->addWidget(m_modbusGroup);
    layout->addStretch(1);

    m_sendTimer = new QTimer(this);
    connect(m_transportCombo, &QComboBox::currentIndexChanged, m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_protocolCombo, &QComboBox::currentIndexChanged, this, &ControlPanel::protocolChanged);
    connect(m_protocolCombo, &QComboBox::currentIndexChanged, this, &ControlPanel::updateQuickVisibility);
    connect(m_connectBtn, &QPushButton::clicked, this, &ControlPanel::onConnectClicked);
    connect(sendBtn, &QPushButton::clicked, this, &ControlPanel::onSendClicked);
    connect(m_inputEdit, &QLineEdit::returnPressed, this, &ControlPanel::onSendClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &ControlPanel::refreshPortsRequested);
    connect(pingBtn, &QPushButton::clicked, this, &ControlPanel::quickPingRequested);
    connect(verBtn, &QPushButton::clicked, this, &ControlPanel::quickVersionRequested);
    connect(readBtn, &QPushButton::clicked, this, [this] {
        emit modbusReadRequested(modbusSlave(), modbusReg(), modbusCount());
    });
    connect(writeBtn, &QPushButton::clicked, this, [this] {
        emit modbusWriteRequested(modbusSlave(), modbusReg(), quint16(m_valueSpin->value()));
    });
    connect(m_periodicCheck, &QCheckBox::toggled, this, &ControlPanel::onPeriodicToggled);
    connect(m_sendTimer, &QTimer::timeout, this, &ControlPanel::onPeriodicTimeout);
    connect(m_pollCheck, &QCheckBox::toggled, this, &ControlPanel::onPollToggled);
    connect(m_pollSpin, &QSpinBox::valueChanged, this, [this](int v) {
        if (m_pollCheck->isChecked())
            emit modbusPollToggled(true, v);
    });
    const auto emitModbusParams = [this] {
        emit modbusParamsChanged(modbusSlave(), modbusReg(), modbusCount());
    };
    connect(m_slaveSpin, qOverload<int>(&QSpinBox::valueChanged), this, emitModbusParams);
    connect(m_regSpin, qOverload<int>(&QSpinBox::valueChanged), this, emitModbusParams);
    connect(m_countSpin, qOverload<int>(&QSpinBox::valueChanged), this, emitModbusParams);

    restoreSettings();
    updateQuickVisibility();
}

void ControlPanel::requestConnect()
{
    onConnectClicked();
}

QWidget *ControlPanel::createSerialPage()
{
    auto *page = new QWidget(m_stack);
    auto *grid = new QGridLayout(page);
    grid->setContentsMargins(0, 0, 0, 0);

    m_portCombo = new QComboBox(page);
    m_refreshBtn = new QPushButton(QStringLiteral("刷新"), page);
    m_refreshBtn->setAutoDefault(false);

    m_baudCombo = new QComboBox(page);
    const int bauds[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    for (const int b : bauds)
        m_baudCombo->addItem(QString::number(b), b);
    m_baudCombo->setCurrentIndex(m_baudCombo->findData(115200));

    m_dataCombo = new QComboBox(page);
    for (int bits = 5; bits <= 8; ++bits)
        m_dataCombo->addItem(QString::number(bits), bits);
    m_dataCombo->setCurrentIndex(m_dataCombo->findData(8));

    m_parityCombo = new QComboBox(page);
    m_parityCombo->addItem(QStringLiteral("无"), int(QSerialPort::NoParity));
    m_parityCombo->addItem(QStringLiteral("偶"), int(QSerialPort::EvenParity));
    m_parityCombo->addItem(QStringLiteral("奇"), int(QSerialPort::OddParity));

    m_stopCombo = new QComboBox(page);
    m_stopCombo->addItem(QStringLiteral("1"), int(QSerialPort::OneStop));
    m_stopCombo->addItem(QStringLiteral("2"), int(QSerialPort::TwoStop));

    grid->addWidget(new QLabel(QStringLiteral("串口"), page), 0, 0);
    grid->addWidget(m_portCombo, 0, 1);
    grid->addWidget(m_refreshBtn, 0, 2);
    grid->addWidget(new QLabel(QStringLiteral("波特率"), page), 1, 0);
    grid->addWidget(m_baudCombo, 1, 1, 1, 2);
    grid->addWidget(new QLabel(QStringLiteral("数据位"), page), 2, 0);
    grid->addWidget(m_dataCombo, 2, 1);
    grid->addWidget(new QLabel(QStringLiteral("校验"), page), 2, 2);
    grid->addWidget(m_parityCombo, 2, 3);
    grid->addWidget(new QLabel(QStringLiteral("停止位"), page), 3, 0);
    grid->addWidget(m_stopCombo, 3, 1);
    grid->setColumnStretch(1, 1);
    return page;
}

QWidget *ControlPanel::createTcpClientPage()
{
    auto *page = new QWidget(m_stack);
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);
    m_tcpHostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), page);
    m_tcpPortSpin = new QSpinBox(page);
    m_tcpPortSpin->setRange(1, 65535);
    m_tcpPortSpin->setValue(9000);
    form->addRow(QStringLiteral("服务器地址"), m_tcpHostEdit);
    form->addRow(QStringLiteral("端口"), m_tcpPortSpin);
    return page;
}

QWidget *ControlPanel::createTcpServerPage()
{
    auto *page = new QWidget(m_stack);
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);
    m_serverPortSpin = new QSpinBox(page);
    m_serverPortSpin->setRange(1, 65535);
    m_serverPortSpin->setValue(9000);
    form->addRow(QStringLiteral("监听端口"), m_serverPortSpin);
    return page;
}

QWidget *ControlPanel::createUdpPage()
{
    auto *page = new QWidget(m_stack);
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);
    m_udpLocalSpin = new QSpinBox(page);
    m_udpLocalSpin->setRange(1, 65535);
    m_udpLocalSpin->setValue(9000);
    m_udpHostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), page);
    m_udpTargetSpin = new QSpinBox(page);
    m_udpTargetSpin->setRange(1, 65535);
    m_udpTargetSpin->setValue(9001);
    form->addRow(QStringLiteral("本地端口"), m_udpLocalSpin);
    form->addRow(QStringLiteral("目标地址"), m_udpHostEdit);
    form->addRow(QStringLiteral("目标端口"), m_udpTargetSpin);
    return page;
}

QWidget *ControlPanel::createLoopbackPage()
{
    auto *page = new QWidget(m_stack);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    auto *hint = new QLabel(QStringLiteral("无硬件自测：写入的数据 80ms 后原样回传，\n可完整验证 协议解析→界面显示 链路。"), page);
    hint->setWordWrap(true);
    lay->addWidget(hint);
    return page;
}

QWidget *ControlPanel::createMqttPage()
{
    auto *page = new QWidget(m_stack);
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 0, 0, 0);
    m_mqttHostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), page);
    m_mqttPortSpin = new QSpinBox(page);
    m_mqttPortSpin->setRange(1, 65535);
    m_mqttPortSpin->setValue(1883);
    m_mqttPubEdit = new QLineEdit(QStringLiteral("qt-host/tx"), page);
    m_mqttPubEdit->setPlaceholderText(QStringLiteral("发布主题（发送数据发到这里）"));
    m_mqttSubEdit = new QLineEdit(QStringLiteral("qt-host/rx"), page);
    m_mqttSubEdit->setPlaceholderText(QStringLiteral("订阅主题（收到数据进解析链路）"));
    form->addRow(QStringLiteral("代理地址"), m_mqttHostEdit);
    form->addRow(QStringLiteral("端口"), m_mqttPortSpin);
    form->addRow(QStringLiteral("发布主题"), m_mqttPubEdit);
    form->addRow(QStringLiteral("订阅主题"), m_mqttSubEdit);
    return page;
}

ControlPanel::ConnParams ControlPanel::currentParams() const
{
    ConnParams p;
    p.type = TransportType(m_transportCombo->currentIndex());
    if (p.type == Serial) {
        p.serial.portName = m_portCombo->currentText();
        p.serial.baudRate = m_baudCombo->currentData().toInt();
        p.serial.dataBits = QSerialPort::DataBits(m_dataCombo->currentData().toInt());
        p.serial.parity = QSerialPort::Parity(m_parityCombo->currentData().toInt());
        p.serial.stopBits = QSerialPort::StopBits(m_stopCombo->currentData().toInt());
    } else if (p.type == TcpClient) {
        p.tcpHost = m_tcpHostEdit->text().trimmed();
        p.tcpPort = quint16(m_tcpPortSpin->value());
    } else if (p.type == TcpServer) {
        p.tcpPort = quint16(m_serverPortSpin->value());
    } else if (p.type == Udp) {
        p.udpLocalPort = quint16(m_udpLocalSpin->value());
        p.tcpHost = m_udpHostEdit->text().trimmed(); // 复用字段存目标地址
        p.udpTargetPort = quint16(m_udpTargetSpin->value());
    } else if (p.type == Mqtt) {
        p.mqttHost = m_mqttHostEdit->text().trimmed();
        p.mqttPort = quint16(m_mqttPortSpin->value());
        p.mqttPub = m_mqttPubEdit->text().trimmed();
        p.mqttSub = m_mqttSubEdit->text().trimmed();
    }
    return p;
}

int ControlPanel::protocolIndex() const
{
    return m_protocolCombo->currentIndex();
}

quint8 ControlPanel::modbusSlave() const { return quint8(m_slaveSpin->value()); }
quint16 ControlPanel::modbusReg() const { return quint16(m_regSpin->value()); }
quint16 ControlPanel::modbusCount() const { return quint16(m_countSpin->value()); }
bool ControlPanel::pollingEnabled() const { return m_pollCheck->isChecked(); }
int ControlPanel::pollingIntervalMs() const { return m_pollSpin->value(); }

void ControlPanel::setConnected(bool connected)
{
    m_connected = connected;
    m_connectBtn->setText(connected ? QStringLiteral("断  开") : QStringLiteral("连  接"));
    m_connectBtn->setProperty("connected", connected);
    m_connectBtn->style()->unpolish(m_connectBtn);
    m_connectBtn->style()->polish(m_connectBtn);
    m_statusDot->setStyleSheet(connected
                                   ? QStringLiteral("color:#3fb950; font-size:16px;")
                                   : QStringLiteral("color:#6e7681; font-size:16px;"));
    m_statusText->setText(connected ? QStringLiteral("已连接") : QStringLiteral("未连接"));

    if (!connected)
        m_sendTimer->stop();
    else if (m_periodicCheck->isChecked())
        m_sendTimer->start(m_periodSpin->value());

    const bool editable = !connected;
    m_portCombo->setEnabled(editable);
    m_refreshBtn->setEnabled(editable);
    m_baudCombo->setEnabled(editable);
    m_dataCombo->setEnabled(editable);
    m_parityCombo->setEnabled(editable);
    m_stopCombo->setEnabled(editable);
    m_tcpHostEdit->setEnabled(editable);
    m_tcpPortSpin->setEnabled(editable);
    m_serverPortSpin->setEnabled(editable);
    m_udpLocalSpin->setEnabled(editable);
    m_udpHostEdit->setEnabled(editable);
    m_udpTargetSpin->setEnabled(editable);
}

void ControlPanel::setPortList(const QStringList &ports)
{
    const QString current = m_portCombo->currentText();
    m_portCombo->clear();
    m_portCombo->addItems(ports);
    if (!current.isEmpty()) {
        const int idx = m_portCombo->findText(current);
        if (idx >= 0)
            m_portCombo->setCurrentIndex(idx);
    }
}

void ControlPanel::onConnectClicked()
{
    saveSettings();
    if (m_connected) {
        emit disconnectRequested();
        return;
    }
    if (currentParams().type == Serial && m_portCombo->currentText().isEmpty()) {
        emit refreshPortsRequested();
        return;
    }
    emit connectRequested(currentParams());
}

void ControlPanel::onSendClicked()
{
    bool ok = false;
    const QByteArray bytes = encodeCurrent(&ok);
    if (!ok) {
        // 编码错误提示由 MainWindow 侧统一记日志；这里静默丢弃
        return;
    }
    if (bytes.isEmpty())
        return;
    emit sendRequested(m_inputEdit->text(), m_hexCheck->isChecked());
}

void ControlPanel::onPeriodicToggled(bool checked)
{
    m_periodSpin->setEnabled(checked);
    if (m_connected) {
        if (checked)
            m_sendTimer->start(m_periodSpin->value());
        else
            m_sendTimer->stop();
    }
}

void ControlPanel::onPeriodicTimeout()
{
    if (!m_connected)
        return;
    bool ok = false;
    const QByteArray bytes = encodeCurrent(&ok);
    if (!ok || bytes.isEmpty())
        return;
    emit sendRequested(m_inputEdit->text(), m_hexCheck->isChecked());
}

void ControlPanel::onPollToggled(bool checked)
{
    m_pollSpin->setEnabled(checked);
    emit modbusPollToggled(checked, m_pollSpin->value());
}

QByteArray ControlPanel::encodeCurrent(bool *ok) const
{
    const QString text = m_inputEdit->text().trimmed();
    if (text.isEmpty()) {
        if (ok) *ok = true;
        return QByteArray();
    }
    if (m_hexCheck->isChecked()) {
        bool hexOk = false;
        const QByteArray bytes = ByteUtils::fromHexText(text, &hexOk);
        if (ok) *ok = hexOk;
        return hexOk ? bytes : QByteArray();
    }
    QByteArray bytes = text.toUtf8();
    if (m_crlfCheck->isChecked())
        bytes.append("\r\n");
    if (ok) *ok = true;
    return bytes;
}

void ControlPanel::updateQuickVisibility()
{
    const int proto = m_protocolCombo->currentIndex();
    m_customGroup->setVisible(proto == 1);
    m_modbusGroup->setVisible(proto == 2 || proto == 3);
}

void ControlPanel::restoreSettings()
{
    QSettings s;
    m_transportCombo->setCurrentIndex(s.value(kKeyTransport, 0).toInt());

    const QString port = s.value(kKeyPort).toString();
    if (!port.isEmpty())
        m_portCombo->addItem(port);

    int idx = m_baudCombo->findData(s.value(kKeyBaud, 115200).toInt());
    if (idx >= 0) m_baudCombo->setCurrentIndex(idx);
    idx = m_dataCombo->findData(s.value(kKeyData, 8).toInt());
    if (idx >= 0) m_dataCombo->setCurrentIndex(idx);
    idx = m_parityCombo->findData(s.value(kKeyParity, int(QSerialPort::NoParity)).toInt());
    if (idx >= 0) m_parityCombo->setCurrentIndex(idx);
    idx = m_stopCombo->findData(s.value(kKeyStop, int(QSerialPort::OneStop)).toInt());
    if (idx >= 0) m_stopCombo->setCurrentIndex(idx);

    m_tcpHostEdit->setText(s.value(kKeyTcpHost, QStringLiteral("127.0.0.1")).toString());
    m_tcpPortSpin->setValue(s.value(kKeyTcpPort, 9000).toInt());
    m_serverPortSpin->setValue(s.value(kKeySrvPort, 9000).toInt());
    m_udpLocalSpin->setValue(s.value(kKeyUdpLocal, 9000).toInt());
    m_udpHostEdit->setText(s.value(kKeyUdpHost, QStringLiteral("127.0.0.1")).toString());
    m_udpTargetSpin->setValue(s.value(kKeyUdpTarget, 9001).toInt());

    m_mqttHostEdit->setText(s.value(kKeyMqttHost, QStringLiteral("127.0.0.1")).toString());
    m_mqttPortSpin->setValue(s.value(kKeyMqttPort, 1883).toInt());
    m_mqttPubEdit->setText(s.value(kKeyMqttPub, QStringLiteral("qt-host/tx")).toString());
    m_mqttSubEdit->setText(s.value(kKeyMqttSub, QStringLiteral("qt-host/rx")).toString());

    m_protocolCombo->setCurrentIndex(s.value(kKeyProtocol, 1).toInt());
    m_hexCheck->setChecked(s.value(kKeyHex, false).toBool());
    m_crlfCheck->setChecked(s.value(kKeyCrlf, true).toBool());
    m_periodSpin->setValue(s.value(kKeyPeriod, 1000).toInt());
    m_pollSpin->setValue(s.value(kKeyPoll, 1000).toInt());
}

void ControlPanel::saveSettings()
{
    QSettings s;
    s.setValue(kKeyTransport, m_transportCombo->currentIndex());
    s.setValue(kKeyPort, m_portCombo->currentText());
    s.setValue(kKeyBaud, m_baudCombo->currentData());
    s.setValue(kKeyData, m_dataCombo->currentData());
    s.setValue(kKeyParity, m_parityCombo->currentData());
    s.setValue(kKeyStop, m_stopCombo->currentData());
    s.setValue(kKeyTcpHost, m_tcpHostEdit->text());
    s.setValue(kKeyTcpPort, m_tcpPortSpin->value());
    s.setValue(kKeySrvPort, m_serverPortSpin->value());
    s.setValue(kKeyUdpLocal, m_udpLocalSpin->value());
    s.setValue(kKeyUdpHost, m_udpHostEdit->text());
    s.setValue(kKeyUdpTarget, m_udpTargetSpin->value());
    s.setValue(kKeyMqttHost, m_mqttHostEdit->text());
    s.setValue(kKeyMqttPort, m_mqttPortSpin->value());
    s.setValue(kKeyMqttPub, m_mqttPubEdit->text());
    s.setValue(kKeyMqttSub, m_mqttSubEdit->text());
    s.setValue(kKeyProtocol, m_protocolCombo->currentIndex());
    s.setValue(kKeyHex, m_hexCheck->isChecked());
    s.setValue(kKeyCrlf, m_crlfCheck->isChecked());
    s.setValue(kKeyPeriod, m_periodSpin->value());
    s.setValue(kKeyPoll, m_pollSpin->value());
}
