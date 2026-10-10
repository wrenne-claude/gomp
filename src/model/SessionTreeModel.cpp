#include "SessionTreeModel.h"

#include "util/Markdown.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

using namespace Qt::StringLiterals;

namespace gomp {

SessionTreeModel::SessionTreeModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SessionTreeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_nodes.size());
}

QHash<int, QByteArray> SessionTreeModel::roleNames() const
{
    return {{EntryIdRole, "entryId"}, {KindRole, "kind"},           {TextRole, "text"},
            {TimestampRole, "timestamp"}, {LaneRole, "lane"},       {ParentRowRole, "parentRow"},
            {ParentLaneRole, "parentLane"}, {ActiveRole, "active"}, {ForkRole, "fork"},
            {ThroughRole, "through"}, {ContinuesRole, "continues"}};
}

QVariant SessionTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_nodes.size())
        return {};
    const auto &n = m_nodes.at(index.row());
    switch (role) {
    case EntryIdRole:
        return n.id;
    case KindRole:
        return n.kind;
    case TextRole:
        return n.text;
    case TimestampRole:
        return double(n.timestamp);
    case LaneRole:
        return n.lane;
    case ParentRowRole:
        return n.parentRow;
    case ParentLaneRole:
        return n.parentLane;
    case ActiveRole:
        return n.active;
    case ForkRole:
        return n.fork;
    case ThroughRole: {
        QVariantList lanes;
        for (const int l : n.through)
            lanes << l;
        return lanes;
    }
    case ContinuesRole:
        return n.continues;
    default:
        return {};
    }
}

QList<SessionTreeModel::Node> SessionTreeModel::layout(const QByteArray &jsonl, int *lanes, int *branches)
{
    struct Raw {
        QString id;
        QString parentId;
        QString kind; // empty: structural entry, hidden
        QString text;
        qint64 ts = 0;
    };
    QList<Raw> raw;
    QHash<QString, qsizetype> rawIndex;
    for (const auto &line : jsonl.split('\n')) {
        if (line.trimmed().isEmpty())
            continue;
        const auto o = QJsonDocument::fromJson(line).object();
        const auto id = o.value("id"_L1).toString();
        if (id.isEmpty())
            continue;
        Raw r;
        r.id = id;
        r.parentId = o.value("parentId"_L1).toString();
        r.ts = QDateTime::fromString(o.value("timestamp"_L1).toString(), Qt::ISODateWithMs).toMSecsSinceEpoch();
        const auto type = o.value("type"_L1).toString();
        if (type == "message"_L1) {
            const auto msg = o.value("message"_L1).toObject();
            const auto role = msg.value("role"_L1).toString();
            if (role == "user"_L1 || role == "assistant"_L1) {
                QString text;
                const auto content = msg.value("content"_L1);
                if (content.isString()) {
                    text = content.toString();
                } else {
                    for (const auto &c : content.toArray()) {
                        const auto co = c.toObject();
                        if (co.value("type"_L1).toString() == "text"_L1)
                            text += co.value("text"_L1).toString() + u' ';
                    }
                }
                if (!text.trimmed().isEmpty() && !msg.value("synthetic"_L1).toBool()) {
                    r.kind = role;
                    r.text = md::plainPreview(text, 120);
                }
            }
        } else if (type == "compaction"_L1) {
            r.kind = u"compaction"_s;
            r.text = u"Context compacted"_s;
        } else if (type == "branch_summary"_L1) {
            r.kind = u"branch"_s;
            r.text = md::plainPreview(o.value("summary"_L1).toString(), 120);
        }
        rawIndex.insert(r.id, raw.size());
        raw << r;
    }

    // Nearest visible ancestor for every entry.
    auto visibleParent = [&](QString pid) {
        QSet<QString> seen;
        while (!pid.isEmpty() && !seen.contains(pid)) {
            seen.insert(pid);
            const auto it = rawIndex.constFind(pid);
            if (it == rawIndex.cend())
                return QString{};
            if (!raw.at(*it).kind.isEmpty())
                return pid;
            pid = raw.at(*it).parentId;
        }
        return QString{};
    };

    // Active path: walk back from the last entry (the current leaf).
    QSet<QString> active;
    if (!raw.isEmpty()) {
        QString cur = raw.last().id;
        while (!cur.isEmpty() && !active.contains(cur)) {
            active.insert(cur);
            const auto it = rawIndex.constFind(cur);
            if (it == rawIndex.cend())
                break;
            cur = raw.at(*it).parentId;
        }
    }

    QList<Node> nodes;
    QHash<QString, int> rowOf;
    QHash<QString, int> childCount;
    for (const auto &r : raw) {
        if (r.kind.isEmpty())
            continue;
        Node n;
        n.id = r.id;
        n.parentId = visibleParent(r.parentId);
        n.kind = r.kind;
        n.text = r.text;
        n.timestamp = r.ts;
        n.active = active.contains(r.id);
        rowOf.insert(n.id, int(nodes.size()));
        childCount[n.parentId] += 1;
        nodes << n;
    }

    // Lanes: active path on lane 0; other branches take the next free lane at their fork point.
    int nextLane = 1;
    QHash<QString, int> continuedFrom; // parent id -> whether a child already continued its lane
    *branches = 0;
    for (auto &n : nodes) {
        const auto parentRow = rowOf.value(n.parentId, -1);
        n.parentRow = parentRow;
        if (parentRow >= 0) {
            auto &p = nodes[parentRow];
            n.parentLane = p.lane;
            p.fork = childCount.value(p.id) > 1;
        }
        if (n.active) {
            n.lane = 0;
        } else if (parentRow >= 0 && !nodes.at(parentRow).active && !continuedFrom.contains(n.parentId)) {
            n.lane = nodes.at(parentRow).lane;
        } else {
            n.lane = nextLane++;
            ++*branches;
        }
        continuedFrom.insert(n.parentId, 1);
    }
    // Connectors: a child's line runs from its parent's row down to it on the child's lane.
    for (qsizetype i = 0; i < nodes.size(); ++i) {
        const auto &n = nodes.at(i);
        if (n.parentRow < 0)
            continue;
        if (n.parentLane == n.lane)
            nodes[n.parentRow].continues = true;
        for (qsizetype r = n.parentRow + 1; r < i; ++r) {
            if (!nodes[r].through.contains(n.lane) && nodes[r].lane != n.lane)
                nodes[r].through << n.lane;
        }
    }
    *lanes = nextLane;
    return nodes;
}

void SessionTreeModel::setPath(const QString &path)
{
    if (path == m_path)
        return;
    m_path = path;
    emit pathChanged();
    beginResetModel();
    m_nodes.clear();
    QFile f(path);
    if (f.open(QIODevice::ReadOnly))
        m_nodes = layout(f.readAll(), &m_lanes, &m_branches);
    else
        m_lanes = m_branches = 0;
    endResetModel();
    emit loaded();
}

} // namespace gomp
