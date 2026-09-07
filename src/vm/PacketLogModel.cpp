#include "vm/PacketLogModel.h"

#include <QFont>
#include <QTime>

namespace {

QString dirForKind(int kind)
{
    switch (kind) {
    case 1:  return QStringLiteral("RX");
    case 2:  return QStringLiteral("TX");
    case 3:  return QStringLiteral("WRN");
    case 4:  return QStringLiteral("ERR");
    default: return QStringLiteral("INF");
    }
}

} // namespace

PacketLogModel::PacketLogModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int PacketLogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

int PacketLogModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 3;
}

QVariant PacketLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return QVariant();

    const PacketLogEntry &e = m_entries.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColTime: return e.time;
        case ColDir:  return e.dir;
        case ColText: return e.text;
        default:      return QVariant();
        }
    case KindRole:
        return e.kind;
    case Qt::ForegroundRole:
        switch (index.column()) {
        case ColTime: return QColor(QStringLiteral("#6e7681"));
        case ColDir:
        case ColText: return QColor(colorForKind(e.kind));
        default:      return QVariant();
        }
    case Qt::FontRole:
        if (index.column() == ColDir) {
            QFont f;
            f.setBold(true);
            return f;
        }
        return QVariant();
    case Qt::TextAlignmentRole:
        if (index.column() == ColDir)
            return int(Qt::AlignCenter);
        return QVariant();
    default:
        return QVariant();
    }
}

QVariant PacketLogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case ColTime: return QStringLiteral("时间");
    case ColDir:  return QStringLiteral("方向");
    case ColText: return QStringLiteral("内容");
    default:      return QVariant();
    }
}

void PacketLogModel::addEntry(int kind, const QString &text)
{
    const QString time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"));

    // 超过上限时先裁剪最旧记录（保持行号连续、一次通知一段区间）
    const int overflow = m_entries.size() + 1 - m_maxEntries;
    if (overflow > 0) {
        beginRemoveRows(QModelIndex(), 0, overflow - 1);
        for (int i = 0; i < overflow; ++i)
            m_entries.removeFirst();
        endRemoveRows();
    }

    const int row = m_entries.size();
    beginInsertRows(QModelIndex(), row, row);
    m_entries.append({kind, time, dirForKind(kind), text});
    endInsertRows();
}

void PacketLogModel::clearEntries()
{
    if (m_entries.isEmpty())
        return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

void PacketLogModel::setMaxEntries(int maxEntries)
{
    m_maxEntries = qMax(1, maxEntries);

    // 若现有记录超出新上限，同样裁剪
    const int overflow = m_entries.size() - m_maxEntries;
    if (overflow > 0) {
        beginRemoveRows(QModelIndex(), 0, overflow - 1);
        for (int i = 0; i < overflow; ++i)
            m_entries.removeFirst();
        endRemoveRows();
    }
}

QString PacketLogModel::toPlainText() const
{
    QString out;
    out.reserve(m_entries.size() * 64);
    for (const PacketLogEntry &e : m_entries)
        out += QStringLiteral("%1 [%2] %3\n").arg(e.time, e.dir, e.text);
    return out;
}

QString PacketLogModel::colorForKind(int kind)
{
    switch (kind) {
    case 1:  return QStringLiteral("#58a6ff"); // RX 蓝
    case 2:  return QStringLiteral("#7ee2a8"); // TX 绿
    case 3:  return QStringLiteral("#e6b85c"); // WRN 黄
    case 4:  return QStringLiteral("#ff7b72"); // ERR 红
    default: return QStringLiteral("#8fa3bd"); // INF 灰蓝
    }
}
