#pragma once

#include <QAbstractItemModel>
#include <QMainWindow>

class QAction;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableView;
class ControlPanel;
class HostViewModel;
class LogPanel;
class PacketLogModel;
class PacketFilterProxy;

/**
 * MainWindow —— MVVM 的 View 层。
 * 只负责：布局 / 主题 / 菜单 / 把用户操作转发给 ViewModel、
 * 把 ViewModel 的状态信号渲染到界面。不含任何业务逻辑。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void setupMenus();
    void bindViewModel();
    void applyTheme();
    void appendPacketLine(int kind, const QString &text);
    void appendRawLine(bool tx, const QByteArray &bytes);
    void updateCountLabel(qint64 txBytes, qint64 rxBytes);
    void updatePacketCount();
    QString packetRowsText(const QModelIndexList &proxyRows) const;

    ControlPanel *m_panel = nullptr;
    LogPanel *m_logPanel = nullptr;
    PacketLogModel *m_logModel = nullptr;   // 报文解析数据模型（Model-View）
    PacketFilterProxy *m_filterProxy = nullptr; // 方向+关键字过滤代理（叠在 m_logModel 上）
    QTableView *m_msgView = nullptr;        // 报文解析视图（QTableView，绑定 m_filterProxy）
    QPlainTextEdit *m_rawView = nullptr;    // 原始数据视图
    QComboBox *m_filterCombo = nullptr;     // 报文过滤下拉
    QLineEdit *m_searchEdit = nullptr;      // 报文关键字搜索框（Ctrl+F 聚焦）
    QLabel *m_packetCount = nullptr;        // 报文条数标签

    HostViewModel *m_vm = nullptr;         // ViewModel

    QLabel *m_stateLabel = nullptr;
    QLabel *m_transLabel = nullptr;
    QLabel *m_protoLabel = nullptr;
    QLabel *m_countLabel = nullptr;
    QAction *m_actConnect = nullptr;
    QAction *m_actClear = nullptr;
};
