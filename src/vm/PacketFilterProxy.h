#pragma once

#include <QSortFilterProxyModel>

/**
 * PacketFilterProxy —— 报文表格的方向/级别 + 关键字过滤器（Qt 标准代理模型写法）。
 *
 * 叠在 PacketLogModel 之上，QTableView 绑定本代理：
 *   - setKindFilter(-1)  显示全部类型；否则只显示该类型（1=RX 2=TX 0=信息 3=警告 4=错误）
 *   - setTextFilter(s)   内容列包含 s 的子串（不区分大小写），可与类型过滤叠加
 * 行颜色/数据仍由源模型 PacketLogModel 提供，代理只做"显示/隐藏"。
 */
class PacketFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit PacketFilterProxy(QObject *parent = nullptr);

    int kindFilter() const;
    QString textFilter() const;

public slots:
    void setKindFilter(int kind);
    void setTextFilter(const QString &text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    int m_kindFilter = -1;   // -1 = 全部类型
    QString m_textFilter;    // 空 = 不按关键字过滤
};
