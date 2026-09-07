#include "ui/MainWindow.h"

#include <QAction>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QTextStream>
#include <QTime>
#include <QToolBar>
#include <QVBoxLayout>

#include "core/AppLogger.h"
#include "core/ByteUtils.h"
#include "core/MqttSettings.h"
#include "ui/ControlPanel.h"
#include "ui/LogPanel.h"
#include "vm/HostViewModel.h"
#include "vm/PacketFilterProxy.h"
#include "vm/PacketLogModel.h"

namespace {

QString nowStamp()
{
    return QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"));
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_vm = new HostViewModel(this);

    setupUi();
    setupMenus();
    bindViewModel();
    applyTheme();

    // 与面板中已恢复的设置同步（协议下拉的初值来自 QSettings）
    m_vm->setProtocol(m_panel->protocolIndex());
    m_vm->refreshPorts();

    QSettings s;
    const QByteArray geo = s.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geo.isEmpty())
        restoreGeometry(geo);
    else
        resize(1280, 820);

    // 恢复报文过滤选择（QSettings 持久化；关键字搜索不持久化）
    const int filterIdx = s.value(QStringLiteral("packet/filterIndex"), 0).toInt();
    if (filterIdx > 0 && filterIdx < m_filterCombo->count())
        m_filterCombo->setCurrentIndex(filterIdx);

    AppLogger::instance().info(QStringLiteral("框架启动完成（v0.8 · MVVM + Model-View），请选择通讯方式并连接。"));
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("Qt 上位机框架  v0.8"));

    m_panel = new ControlPanel(this);
    m_panel->setFixedWidth(340);

    m_logPanel = new LogPanel(this);

    // 报文解析视图：QAbstractTableModel + 过滤代理 + QTableView（Qt Model-View 绑定）
    m_logModel = new PacketLogModel(this);
    m_logModel->setMaxEntries(5000);
    m_filterProxy = new PacketFilterProxy(this);
    m_filterProxy->setSourceModel(m_logModel);
    m_msgView = new QTableView(this);
    m_msgView->setModel(m_filterProxy);
    m_msgView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_msgView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_msgView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_msgView->setAlternatingRowColors(true);
    m_msgView->setShowGrid(false);
    m_msgView->setWordWrap(false);
    m_msgView->verticalHeader()->setVisible(false);
    m_msgView->verticalHeader()->setDefaultSectionSize(24);
    m_msgView->horizontalHeader()->setHighlightSections(false);
    m_msgView->horizontalHeader()->setSectionResizeMode(PacketLogModel::ColTime, QHeaderView::Fixed);
    m_msgView->horizontalHeader()->setSectionResizeMode(PacketLogModel::ColDir, QHeaderView::Fixed);
    m_msgView->horizontalHeader()->setSectionResizeMode(PacketLogModel::ColText, QHeaderView::Stretch);
    m_msgView->setColumnWidth(PacketLogModel::ColTime, 92);
    m_msgView->setColumnWidth(PacketLogModel::ColDir, 52);

    // 过滤条：方向/级别过滤 + 关键字搜索 + 实时条数
    auto *bar = new QWidget(this);
    auto *barLay = new QHBoxLayout(bar);
    barLay->setContentsMargins(0, 0, 0, 4);
    barLay->setSpacing(6);
    auto *paneTitle = new QLabel(QStringLiteral("报文解析"), bar);
    QFont tf = paneTitle->font();
    tf.setBold(true);
    paneTitle->setFont(tf);
    m_filterCombo = new QComboBox(bar);
    m_filterCombo->addItem(QStringLiteral("全部"), -1);
    m_filterCombo->addItem(QStringLiteral("仅接收 RX"), int(HostViewModel::KindRx));
    m_filterCombo->addItem(QStringLiteral("仅发送 TX"), int(HostViewModel::KindTx));
    m_filterCombo->addItem(QStringLiteral("仅信息"), int(HostViewModel::KindInfo));
    m_filterCombo->addItem(QStringLiteral("仅警告"), int(HostViewModel::KindWarn));
    m_filterCombo->addItem(QStringLiteral("仅错误"), int(HostViewModel::KindError));
    m_packetCount = new QLabel(QStringLiteral("0 / 0 条"), bar);
    m_packetCount->setStyleSheet(QStringLiteral("color:#8fa3bd;"));
    m_searchEdit = new QLineEdit(bar);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索内容…(Ctrl+F)"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(190);
    m_searchEdit->setToolTip(QStringLiteral("按内容子串过滤（不区分大小写），可与方向过滤叠加；Esc 清空"));
    barLay->addWidget(paneTitle);
    barLay->addStretch(1);
    barLay->addWidget(new QLabel(QStringLiteral("显示:"), bar));
    barLay->addWidget(m_filterCombo);
    barLay->addWidget(m_packetCount);
    barLay->addWidget(m_searchEdit);

    auto *packetPane = new QWidget(this);
    auto *paneLay = new QVBoxLayout(packetPane);
    paneLay->setContentsMargins(0, 0, 0, 0);
    paneLay->setSpacing(0);
    paneLay->addWidget(bar);
    paneLay->addWidget(m_msgView, 1);

    // 报文表格右键菜单：复制 / 全选 / 清空
    m_msgView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_msgView, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
                QMenu menu(m_msgView);
                QAction *copySel = menu.addAction(QStringLiteral("复制选中行"));
                QAction *copyAll = menu.addAction(QStringLiteral("复制全部（当前过滤）"));
                menu.addSeparator();
                QAction *selAll = menu.addAction(QStringLiteral("全选"));
                menu.addSeparator();
                QAction *clearAct = menu.addAction(QStringLiteral("清空报文"));
                QAction *chosen = menu.exec(m_msgView->viewport()->mapToGlobal(pos));
                if (chosen == copySel) {
                    const QModelIndexList rows = m_msgView->selectionModel()->selectedRows();
                    if (!rows.isEmpty())
                        QGuiApplication::clipboard()->setText(packetRowsText(rows));
                } else if (chosen == copyAll) {
                    QModelIndexList rows;
                    for (int r = 0; r < m_filterProxy->rowCount(); ++r)
                        rows << m_filterProxy->index(r, 0);
                    QGuiApplication::clipboard()->setText(packetRowsText(rows));
                } else if (chosen == selAll) {
                    m_msgView->selectAll();
                } else if (chosen == clearAct) {
                    m_logModel->clearEntries();
                }
            });
    // 过滤下拉 → 代理；选择持久化
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        m_filterProxy->setKindFilter(m_filterCombo->currentData().toInt());
        updatePacketCount();
        QSettings s;
        s.setValue(QStringLiteral("packet/filterIndex"), m_filterCombo->currentIndex());
    });
    // 搜索框 → 代理（不持久化）
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_filterProxy->setTextFilter(text.trimmed());
        updatePacketCount();
    });
    // 条数标签：源模型增删（总数变化）与代理过滤（显示数变化）都要刷新
    connect(m_filterProxy, &QAbstractItemModel::rowsInserted, this, &MainWindow::updatePacketCount);
    connect(m_filterProxy, &QAbstractItemModel::rowsRemoved, this, &MainWindow::updatePacketCount);
    connect(m_filterProxy, &QAbstractItemModel::modelReset, this, &MainWindow::updatePacketCount);
    connect(m_filterProxy, &QAbstractItemModel::layoutChanged, this, &MainWindow::updatePacketCount);
    connect(m_logModel, &QAbstractItemModel::rowsInserted, this, &MainWindow::updatePacketCount);
    connect(m_logModel, &QAbstractItemModel::rowsRemoved, this, &MainWindow::updatePacketCount);
    connect(m_logModel, &QAbstractItemModel::modelReset, this, &MainWindow::updatePacketCount);

    // 快捷键：Ctrl+F 聚焦搜索；Esc 清空搜索（仅当搜索框持有焦点或非空时）
    auto *findShort = new QShortcut(QKeySequence::Find, this);
    connect(findShort, &QShortcut::activated, this, [this] {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });
    auto *escShort = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escShort, &QShortcut::activated, this, [this] {
        if (m_searchEdit->hasFocus() || !m_searchEdit->text().isEmpty())
            m_searchEdit->clear();
    });

    m_rawView = new QPlainTextEdit(this);
    m_rawView->setReadOnly(true);
    m_rawView->setMaximumBlockCount(5000);
    m_rawView->setPlaceholderText(QStringLiteral("原始收发字节（HEX）将显示在这里…"));

    QFont mono(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::TypeWriter);
    m_msgView->setFont(mono);
    m_rawView->setFont(mono);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(packetPane);
    splitter->addWidget(m_rawView);
    splitter->addWidget(m_logPanel);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 2);
    splitter->setSizes({450, 200, 200});

    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);
    layout->addWidget(m_panel);
    layout->addWidget(splitter, 1);
    setCentralWidget(central);

    m_stateLabel = new QLabel(QStringLiteral("○ 未连接"), this);
    m_transLabel = new QLabel(QString(), this);
    m_protoLabel = new QLabel(QString(), this);
    m_countLabel = new QLabel(QStringLiteral("TX 0 B    RX 0 B"), this);
    statusBar()->addWidget(m_stateLabel);
    statusBar()->addWidget(m_transLabel);
    statusBar()->addWidget(m_protoLabel, 1);
    statusBar()->addPermanentWidget(m_countLabel);

    auto *tb = addToolBar(QStringLiteral("main"));
    tb->setMovable(false);
    m_actConnect = new QAction(QStringLiteral("连接"), this);
    m_actClear = new QAction(QStringLiteral("清空视图"), this);
    QAction *actAbout = new QAction(QStringLiteral("关于"), this);
    tb->addAction(m_actConnect);
    tb->addSeparator();
    tb->addAction(m_actClear);
    tb->addSeparator();
    tb->addAction(actAbout);
}

void MainWindow::setupMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    QAction *actExport = fileMenu->addAction(QStringLiteral("导出报文为文本(&E)…"));
    connect(actExport, &QAction::triggered, this, [this] {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("导出报文"), QStringLiteral("messages.log"),
            QStringLiteral("文本文件 (*.txt *.log);;所有文件 (*.*)"));
        if (path.isEmpty())
            return;
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&file);
            ts << m_logModel->toPlainText();
            AppLogger::instance().info(QStringLiteral("报文已导出：%1").arg(path));
        } else {
            AppLogger::instance().error(QStringLiteral("导出失败：%1").arg(file.errorString()));
        }
    });
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("退出(&X)"), this, &MainWindow::close);

    QMenu *commMenu = menuBar()->addMenu(QStringLiteral("通讯(&C)"));
    commMenu->addAction(m_actConnect);
    QAction *rescan = commMenu->addAction(QStringLiteral("重新扫描串口"));
    connect(rescan, &QAction::triggered, m_vm, &HostViewModel::refreshPorts);
    commMenu->addSeparator();
    commMenu->addAction(QStringLiteral("清空收发计数"), m_vm, &HostViewModel::resetCounters);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    QAction *actRaw = viewMenu->addAction(QStringLiteral("显示原始数据面板"));
    actRaw->setCheckable(true);
    actRaw->setChecked(true);
    connect(actRaw, &QAction::toggled, m_rawView, &QWidget::setVisible);
    viewMenu->addSeparator();
    viewMenu->addAction(QStringLiteral("清空报文解析"), this,
                        [this] { m_logModel->clearEntries(); });
    viewMenu->addAction(QStringLiteral("清空原始数据"), m_rawView, &QPlainTextEdit::clear);
    viewMenu->addAction(QStringLiteral("清空日志"), m_logPanel, &LogPanel::clear);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actClear);

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    QAction *about = helpMenu->addAction(QStringLiteral("关于…"));
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::information(this, QStringLiteral("关于"),
                                 QStringLiteral("Qt 上位机基础框架 v0.8（MVVM + Model-View）\n\n"
                                                "Model      ：core/（传输层 ITransport + 协议解码器 + 日志）\n"
                                                "             vm/PacketLogModel（报文解析数据模型）\n"
                                                "             vm/PacketFilterProxy（方向+关键字过滤代理模型）\n"
                                                "ViewModel  ：vm/HostViewModel（全部业务命令与状态）\n"
                                                "View       ：MainWindow / ControlPanel / QTableView（纯界面与绑定）\n\n"
                                                "传输：串口 / TCP客户端 / TCP服务器 / UDP / 回环 / MQTT(3.1.1 QoS0)\n"
                                                "协议：自定义帧(AA55+CRC16) / Modbus RTU / Modbus TCP / 文本行\n"
                                                "报文表格：方向过滤 / 关键字搜索(Ctrl+F) / 右键复制 / 导出文本"));
    });

    connect(m_actConnect, &QAction::triggered, m_panel, &ControlPanel::requestConnect);
    connect(m_actClear, &QAction::triggered, this, [this] {
        m_logModel->clearEntries();
        m_rawView->clear();
        m_logPanel->clear();
    });
}

void MainWindow::bindViewModel()
{
    // ---- View → VM：用户操作即命令 ----
    connect(m_panel, &ControlPanel::connectRequested, this,
            [this](const ControlPanel::ConnParams &p) {
                int port1;
                if (p.type == ControlPanel::Udp)
                    port1 = p.udpLocalPort;
                else if (p.type == ControlPanel::Mqtt)
                    port1 = p.mqttPort;
                else
                    port1 = p.tcpPort;
                MqttSettings mqtt;
                mqtt.host = p.mqttHost;
                mqtt.port = p.mqttPort;
                mqtt.pubTopic = p.mqttPub;
                mqtt.subTopic = p.mqttSub;
                m_vm->openChannel(int(p.type), p.serial, p.tcpHost, port1, p.udpTargetPort, mqtt);
            });
    connect(m_panel, &ControlPanel::disconnectRequested, m_vm, &HostViewModel::closeChannel);
    connect(m_panel, &ControlPanel::protocolChanged, m_vm, &HostViewModel::setProtocol);
    connect(m_panel, &ControlPanel::refreshPortsRequested, m_vm, &HostViewModel::refreshPorts);
    connect(m_panel, &ControlPanel::sendRequested, m_vm, &HostViewModel::sendText);
    connect(m_panel, &ControlPanel::quickPingRequested, m_vm, &HostViewModel::sendPing);
    connect(m_panel, &ControlPanel::quickVersionRequested, m_vm, &HostViewModel::sendVersion);
    connect(m_panel, &ControlPanel::modbusReadRequested, m_vm, &HostViewModel::modbusRead);
    connect(m_panel, &ControlPanel::modbusWriteRequested, m_vm, &HostViewModel::modbusWrite);
    connect(m_panel, &ControlPanel::modbusParamsChanged, m_vm, &HostViewModel::setModbusParams);
    connect(m_panel, &ControlPanel::modbusPollToggled, m_vm, &HostViewModel::setPolling);

    // ---- VM → View：状态渲染 ----
    connect(m_vm, &HostViewModel::connectionStateChanged, this,
            [this](bool connected, const QString &name, const QString &detail) {
                m_panel->setConnected(connected);
                if (connected) {
                    m_stateLabel->setText(QStringLiteral("● 已连接"));
                    m_stateLabel->setStyleSheet(QStringLiteral("color:#3fb950; font-weight:bold;"));
                    m_transLabel->setText(QStringLiteral("%1（%2）").arg(name, detail));
                    m_actConnect->setText(QStringLiteral("断开"));
                } else {
                    m_stateLabel->setText(QStringLiteral("○ 未连接"));
                    m_stateLabel->setStyleSheet(QString());
                    m_transLabel->clear();
                    m_actConnect->setText(QStringLiteral("连接"));
                }
            });
    connect(m_vm, &HostViewModel::protocolNameChanged, this, [this](const QString &name) {
        m_protoLabel->setText(QStringLiteral("协议：%1").arg(name));
    });
    connect(m_vm, &HostViewModel::portsChanged, m_panel, &ControlPanel::setPortList);
    connect(m_vm, &HostViewModel::countersChanged, this, &MainWindow::updateCountLabel);
    connect(m_vm, &HostViewModel::rawLine, this, &MainWindow::appendRawLine);
    connect(m_vm, &HostViewModel::messageLine, this, &MainWindow::appendPacketLine);

    // 全局日志 → 日志面板（AppLogger 属于 Model 层，View 只订阅）
    connect(&AppLogger::instance(), &AppLogger::messageLogged, m_logPanel, &LogPanel::appendLog);
}

void MainWindow::applyTheme()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QDialog { background-color: #10131a; }
        QWidget { color: #d7dde6; font-family: "Microsoft YaHei UI"; font-size: 12px; }
        QGroupBox {
            background-color: #171b23; border: 1px solid #262c36; border-radius: 8px;
            margin-top: 12px; padding: 8px 6px 6px 6px; font-weight: 500;
        }
        QGroupBox::title {
            subcontrol-origin: margin; left: 10px; top: 0px;
            color: #8fa3bd; background: transparent;
        }
        QLineEdit, QComboBox, QSpinBox {
            background-color: #0d1017; border: 1px solid #30363d; border-radius: 6px;
            padding: 4px 8px; selection-background-color: #2b6cb8;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: #3d7eff; }
        QComboBox::drop-down { border: none; width: 18px; }
        QComboBox QAbstractItemView {
            background-color: #171b23; border: 1px solid #30363d;
            selection-background-color: #2b6cb8;
        }
        QPushButton {
            background-color: #21262d; border: 1px solid #30363d; border-radius: 6px;
            padding: 5px 14px;
        }
        QPushButton:hover { background-color: #2b313a; border-color: #3d444d; }
        QPushButton:pressed { background-color: #38404b; }
        QPushButton:disabled { color: #566070; }
        QPushButton#connectBtn { background-color: #1f4f8f; border-color: #2b6cb8; font-weight: 500; }
        QPushButton#connectBtn:hover { background-color: #2661ad; }
        QPushButton#connectBtn[connected="true"] { background-color: #8f2f2f; border-color: #c0392b; }
        QPushButton#connectBtn[connected="true"]:hover { background-color: #a83a32; }
        QPushButton#sendBtn { background-color: #1d5f46; border-color: #2ea36f; }
        QPushButton#sendBtn:hover { background-color: #247a58; }
        QPlainTextEdit {
            background-color: #0b0e13; border: 1px solid #262c36; border-radius: 8px;
            color: #d5dbe3;
        }
        QTableView {
            background-color: #0b0e13; alternate-background-color: #10141d;
            border: 1px solid #262c36; border-radius: 8px;
            color: #d5dbe3; gridline-color: transparent;
            selection-background-color: #1f4f8f; selection-color: #ffffff;
        }
        QTableView::item { padding-left: 6px; padding-right: 6px; border: none; }
        QTableView::item:selected { background-color: #1f4f8f; }
        QHeaderView::section {
            background-color: #171b23; color: #8fa3bd;
            border: none; border-right: 1px solid #262c36; border-bottom: 1px solid #262c36;
            padding: 4px 8px; font-weight: 600;
        }
        QTableCornerButton::section { background-color: #171b23; border: none; }
        QMenuBar { background-color: #10131a; border-bottom: 1px solid #262c36; }
        QMenuBar::item { padding: 5px 12px; background: transparent; }
        QMenuBar::item:selected { background-color: #21262d; border-radius: 4px; }
        QMenu { background-color: #171b23; border: 1px solid #30363d; padding: 4px; }
        QMenu::item { padding: 5px 26px 5px 16px; border-radius: 4px; }
        QMenu::item:selected { background-color: #2b6cb8; }
        QMenu::separator { height: 1px; background: #262c36; margin: 4px 6px; }
        QToolBar { background-color: #10131a; border: none; spacing: 6px; padding: 4px; }
        QToolBar QToolButton {
            background-color: #21262d; border: 1px solid #30363d; border-radius: 6px; padding: 4px 12px;
        }
        QToolBar QToolButton:hover { background-color: #2b313a; }
        QStatusBar { background-color: #12151c; border-top: 1px solid #262c36; }
        QStatusBar QLabel { padding: 0 8px; }
        QSplitter::handle { background-color: #10131a; }
    )"));
}

void MainWindow::updatePacketCount()
{
    const int shown = m_filterProxy->rowCount();
    const int total = m_logModel->rowCount();
    m_packetCount->setText(QStringLiteral("%1 / %2 条").arg(shown).arg(total));
}

QString MainWindow::packetRowsText(const QModelIndexList &proxyRows) const
{
    QStringList lines;
    for (const QModelIndex &idx : proxyRows) {
        const int r = idx.row();
        lines << QStringLiteral("%1 [%2] %3")
                     .arg(m_filterProxy->index(r, PacketLogModel::ColTime)
                              .data(Qt::DisplayRole)
                              .toString(),
                          m_filterProxy->index(r, PacketLogModel::ColDir)
                              .data(Qt::DisplayRole)
                              .toString(),
                          m_filterProxy->index(r, PacketLogModel::ColText)
                              .data(Qt::DisplayRole)
                              .toString());
    }
    return lines.join(QLatin1Char('\n'));
}

void MainWindow::appendPacketLine(int kind, const QString &text)
{
    // View 层只做"追加 + 自动滚动"，数据与配色全部由 PacketLogModel 提供
    m_logModel->addEntry(kind, text);
    m_msgView->scrollToBottom();
}

void MainWindow::appendRawLine(bool tx, const QByteArray &bytes)
{
    const QString color = tx ? QStringLiteral("#7ee2a8") : QStringLiteral("#58a6ff");
    const QString tag = tx ? QStringLiteral("TX") : QStringLiteral("RX");
    m_rawView->appendHtml(QStringLiteral("<span style='color:#6e7681'>%1</span> "
                                         "<span style='color:%2'>[%3]</span> "
                                         "<span style='color:#9aa4b2'>%4</span>")
                              .arg(nowStamp(), color, tag, ByteUtils::toHexDisplay(bytes)));
}

void MainWindow::updateCountLabel(qint64 txBytes, qint64 rxBytes)
{
    m_countLabel->setText(QStringLiteral("TX %1 B    RX %2 B").arg(txBytes).arg(rxBytes));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings s;
    s.setValue(QStringLiteral("window/geometry"), saveGeometry());
    if (m_panel)
        m_panel->saveSettings();
    if (m_vm)
        m_vm->closeChannel();
    event->accept();
}
