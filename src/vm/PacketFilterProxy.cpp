#include "vm/PacketFilterProxy.h"

#include "vm/PacketLogModel.h"

PacketFilterProxy::PacketFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

int PacketFilterProxy::kindFilter() const
{
    return m_kindFilter;
}

QString PacketFilterProxy::textFilter() const
{
    return m_textFilter;
}

void PacketFilterProxy::setKindFilter(int kind)
{
    if (m_kindFilter == kind)
        return;
    m_kindFilter = kind;
    invalidateFilter();
}

void PacketFilterProxy::setTextFilter(const QString &text)
{
    if (m_textFilter == text)
        return;
    m_textFilter = text;
    invalidateFilter();
}

bool PacketFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    // 1) 类型过滤
    if (m_kindFilter >= 0) {
        const QModelIndex kindIdx =
            sourceModel()->index(sourceRow, PacketLogModel::ColText, sourceParent);
        if (kindIdx.data(PacketLogModel::KindRole).toInt() != m_kindFilter)
            return false;
    }
    // 2) 关键字过滤（内容列子串，不区分大小写）
    if (!m_textFilter.isEmpty()) {
        const QModelIndex textIdx =
            sourceModel()->index(sourceRow, PacketLogModel::ColText, sourceParent);
        const QString text = textIdx.data(Qt::DisplayRole).toString();
        if (!text.contains(m_textFilter, Qt::CaseInsensitive))
            return false;
    }
    return true;
}
