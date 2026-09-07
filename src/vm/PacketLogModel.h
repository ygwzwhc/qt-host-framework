#pragma once

#include <QAbstractTableModel>
#include <QColor>
#include <QList>

/**
 * PacketLogEntry —— 一行报文记录的纯数据（Model 层数据项）。
 */
struct PacketLogEntry
{
    int kind = 0;         // 与 HostViewModel::MessageKind 对齐：0信息 1RX 2TX 3警告 4错误
    QString time;         // HH:mm:ss.zzz
    QString dir;          // 方向/类型标签 TX / RX / INF / WRN / ERR
    QString text;         // 解析摘要
};

/**
 * PacketLogModel —— 报文记录的 QAbstractTableModel。
 * View(QTableView) 直接以本模型为数据源，列：时间 | 方向 | 内容；
 * 行颜色由 ForegroundRole 返回，随方向/级别变化（蓝=收 绿=发 黄=警告 红=错误）。
 */
class PacketLogModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        ColTime = 0,
        ColDir,
        ColText
    };

    // 自定义角色：行类型（供过滤代理 PacketFilterProxy 使用）
    enum Roles {
        KindRole = Qt::UserRole + 1
    };

    explicit PacketLogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    // 追加一行；超过上限自动裁剪最旧记录
    void addEntry(int kind, const QString &text);
    void clearEntries();
    void setMaxEntries(int maxEntries);

    // 按"时间 [方向] 内容"导出文本（供导出/另存）
    QString toPlainText() const;

    static QString colorForKind(int kind);

private:
    QList<PacketLogEntry> m_entries;
    int m_maxEntries = 5000;
};
