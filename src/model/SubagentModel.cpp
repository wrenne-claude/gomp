#include "SubagentModel.h"

using namespace Qt::StringLiterals;

namespace gomp {

namespace {
constexpr qsizetype MaxOutputChars = 48 * 1024;
}

SubagentModel::SubagentModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_flush.setSingleShot(true);
    m_flush.setInterval(50);
    connect(&m_flush, &QTimer::timeout, this, [this] {
        for (const int r : std::as_const(m_dirty)) {
            if (r < m_agents.size())
                emit dataChanged(index(r), index(r));
        }
        m_dirty.clear();
        emit statsChanged();
    });
}

int SubagentModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_agents.size());
}

QHash<int, QByteArray> SubagentModel::roleNames() const
{
    return {{IdRole, "agentId"},          {IndexRole, "agentIndex"},     {AgentRole, "agent"},
            {TaskRole, "task"},           {StatusRole, "status"},        {ModelRoleRole, "modelRole"},
            {CurrentToolRole, "currentTool"}, {RecentToolsRole, "recentTools"}, {ToolCountRole, "toolCount"},
            {TokensRole, "tokens"},       {CostRole, "cost"},            {DurationRole, "durationMs"},
            {OutputRole, "output"},       {ParentToolRole, "parentToolCallId"}, {StartedAtRole, "startedAt"}};
}

QVariant SubagentModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_agents.size())
        return {};
    const auto &a = m_agents.at(index.row());
    switch (role) {
    case IdRole:
        return a.id;
    case IndexRole:
        return a.index;
    case AgentRole:
        return a.agent;
    case TaskRole:
        return a.task;
    case StatusRole:
        return a.status;
    case ModelRoleRole:
        return a.modelRole;
    case CurrentToolRole:
        return a.currentTool;
    case RecentToolsRole:
        return a.recentTools;
    case ToolCountRole:
        return a.toolCount;
    case TokensRole:
        return a.tokens;
    case CostRole:
        return a.cost;
    case DurationRole:
        return a.durationMs;
    case OutputRole:
        return a.output;
    case ParentToolRole:
        return a.parentToolCallId;
    case StartedAtRole:
        return a.startedAt;
    default:
        return {};
    }
}

int SubagentModel::rowOf(const QString &id) const
{
    for (int i = 0; i < m_agents.size(); ++i) {
        if (m_agents.at(i).id == id)
            return i;
    }
    return -1;
}

QVariantMap SubagentModel::get(int row) const
{
    QVariantMap out;
    if (row < 0 || row >= m_agents.size())
        return out;
    const auto names = roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        out.insert(QString::fromUtf8(it.value()), data(index(row), it.key()));
    return out;
}

int SubagentModel::ensure(const QString &id, int idx, const QString &agent)
{
    // Progress frames identify agents by id when present, else by spawn index.
    for (int i = 0; i < m_agents.size(); ++i) {
        const auto &a = m_agents.at(i);
        if ((!id.isEmpty() && a.id == id) || (id.isEmpty() && a.index == idx && a.agent == agent))
            return i;
    }
    Agent a;
    a.id = id.isEmpty() ? u"%1-%2"_s.arg(agent).arg(idx) : id;
    a.index = idx;
    a.agent = agent;
    a.startedAt = QDateTime::currentMSecsSinceEpoch();
    const int row = int(m_agents.size());
    beginInsertRows({}, row, row);
    m_agents << a;
    endInsertRows();
    emit countChanged();
    return row;
}

void SubagentModel::touch(int row)
{
    m_dirty.insert(row);
    if (!m_flush.isActive())
        m_flush.start();
}

void SubagentModel::apply(const AgentEvent &event)
{
    if (const auto *e = std::get_if<SubagentLifecycleEvent>(&event)) {
        const int r = ensure(e->id, e->index, e->agent);
        auto &a = m_agents[r];
        if (!e->description.isEmpty() && a.task.isEmpty())
            a.task = e->description;
        a.parentToolCallId = e->parentToolCallId;
        a.status = e->status == "started"_L1 ? u"running"_s : e->status;
        if (e->status != "started"_L1)
            a.currentTool.clear();
        touch(r);
        return;
    }
    if (const auto *e = std::get_if<SubagentProgressEvent>(&event)) {
        const int r = ensure(e->id, e->index, e->agent);
        auto &a = m_agents[r];
        if (!e->task.isEmpty())
            a.task = e->task;
        if (!e->modelRole.isEmpty())
            a.modelRole = e->modelRole;
        // Lifecycle is authoritative for terminal states; progress may lag behind it.
        if (!e->status.isEmpty() && a.status != "completed"_L1 && a.status != "failed"_L1 && a.status != "aborted"_L1)
            a.status = e->status;
        a.currentTool = e->currentTool;
        if (!e->recentTools.isEmpty())
            a.recentTools = e->recentTools;
        a.toolCount = e->toolCount;
        a.tokens = e->tokens;
        a.cost = e->cost;
        a.durationMs = e->durationMs;
        if (!e->parentToolCallId.isEmpty())
            a.parentToolCallId = e->parentToolCallId;
        if (a.output.isEmpty() && !e->recentOutput.isEmpty())
            a.output = e->recentOutput.join(u'\n');
        touch(r);
        return;
    }
    if (const auto *e = std::get_if<SubagentOutputEvent>(&event)) {
        const int r = rowOf(e->id);
        if (r < 0)
            return;
        auto &a = m_agents[r];
        if (!e->tool.isEmpty()) {
            a.currentTool = e->tool;
            a.output += u"\n› "_s + e->tool + u'\n';
        }
        a.output += e->text;
        if (a.output.size() > MaxOutputChars)
            a.output = a.output.right(MaxOutputChars);
        touch(r);
    }
}

void SubagentModel::clearFinished()
{
    beginResetModel();
    m_agents.removeIf([](const Agent &a) { return a.status != "running"_L1 && a.status != "pending"_L1; });
    endResetModel();
    emit countChanged();
    emit statsChanged();
}

int SubagentModel::running() const
{
    int n = 0;
    for (const auto &a : m_agents)
        n += (a.status == "running"_L1 || a.status == "pending"_L1) ? 1 : 0;
    return n;
}

int SubagentModel::finished() const
{
    int n = 0;
    for (const auto &a : m_agents)
        n += a.status == "completed"_L1 ? 1 : 0;
    return n;
}

int SubagentModel::failed() const
{
    int n = 0;
    for (const auto &a : m_agents)
        n += (a.status == "failed"_L1 || a.status == "aborted"_L1) ? 1 : 0;
    return n;
}

double SubagentModel::totalCost() const
{
    double c = 0;
    for (const auto &a : m_agents)
        c += a.cost;
    return c;
}

} // namespace gomp
