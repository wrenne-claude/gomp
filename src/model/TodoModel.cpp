#include "TodoModel.h"

#include <QJsonObject>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace gomp {

TodoModel::TodoModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TodoModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_flat.size());
}

QHash<int, QByteArray> TodoModel::roleNames() const
{
    return {{PhaseRole, "phase"},       {ContentRole, "content"},       {StatusRole, "status"},
            {BlockerRole, "blocker"},   {DetailsRole, "details"},       {PhaseIndexRole, "phaseIndex"},
            {TaskIndexRole, "taskIndex"}};
}

QVariant TodoModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_flat.size())
        return {};
    const auto f = m_flat.at(index.row());
    const auto &phase = m_phases.at(f.phase);
    const auto &t = phase.tasks.at(f.task);
    switch (role) {
    case PhaseRole:
        return phase.name;
    case ContentRole:
        return t.content;
    case StatusRole:
        return t.status;
    case BlockerRole:
        return t.blocker;
    case DetailsRole:
        return t.details;
    case PhaseIndexRole:
        return f.phase;
    case TaskIndexRole:
        return f.task;
    default:
        return {};
    }
}

void TodoModel::rebuild()
{
    m_flat.clear();
    for (int p = 0; p < m_phases.size(); ++p) {
        for (int t = 0; t < m_phases.at(p).tasks.size(); ++t)
            m_flat << Flat{p, t};
    }
}

void TodoModel::setPhases(const QList<TodoPhase> &phases)
{
    if (phases == m_phases)
        return;
    beginResetModel();
    m_phases = phases;
    rebuild();
    endResetModel();
    emit changed();
}

QJsonArray TodoModel::toJson() const
{
    QJsonArray out;
    for (const auto &p : m_phases) {
        QJsonArray tasks;
        for (const auto &t : p.tasks) {
            QJsonObject o{{"content"_L1, t.content}, {"status"_L1, t.status}};
            if (!t.blocker.isEmpty())
                o.insert("blocker"_L1, t.blocker);
            if (!t.details.isEmpty())
                o.insert("details"_L1, t.details);
            tasks.append(o);
        }
        out.append(QJsonObject{{"name"_L1, p.name}, {"tasks"_L1, tasks}});
    }
    return out;
}

int TodoModel::completed() const
{
    int n = 0;
    for (const auto &p : m_phases)
        for (const auto &t : p.tasks)
            n += t.status == "completed"_L1 ? 1 : 0;
    return n;
}

int TodoModel::total() const
{
    int n = 0;
    for (const auto &p : m_phases)
        for (const auto &t : p.tasks)
            n += t.status == "abandoned"_L1 ? 0 : 1;
    return n;
}

QString TodoModel::activeTask() const
{
    for (const auto &p : m_phases)
        for (const auto &t : p.tasks)
            if (t.status == "in_progress"_L1)
                return t.content;
    return {};
}

void TodoModel::setContent(int row, const QString &content)
{
    if (row < 0 || row >= m_flat.size() || content.trimmed().isEmpty())
        return;
    const auto f = m_flat.at(row);
    m_phases[f.phase].tasks[f.task].content = content.trimmed();
    emit dataChanged(index(row), index(row), {ContentRole});
    emit changed();
    emit edited();
}

void TodoModel::setStatus(int row, const QString &status)
{
    if (row < 0 || row >= m_flat.size())
        return;
    const auto f = m_flat.at(row);
    m_phases[f.phase].tasks[f.task].status = status;
    emit dataChanged(index(row), index(row), {StatusRole});
    emit changed();
    emit edited();
}

void TodoModel::insertAfter(int row, const QString &content)
{
    beginResetModel();
    if (m_phases.isEmpty())
        m_phases << TodoPhase{u"Plan"_s, {}};
    if (row < 0 || row >= m_flat.size()) {
        m_phases.last().tasks << TodoItem{content, u"pending"_s, {}, {}};
    } else {
        const auto f = m_flat.at(row);
        m_phases[f.phase].tasks.insert(f.task + 1, TodoItem{content, u"pending"_s, {}, {}});
    }
    rebuild();
    endResetModel();
    emit changed();
    emit edited();
}

void TodoModel::removeAt(int row)
{
    if (row < 0 || row >= m_flat.size())
        return;
    beginResetModel();
    const auto f = m_flat.at(row);
    m_phases[f.phase].tasks.removeAt(f.task);
    if (m_phases.at(f.phase).tasks.isEmpty())
        m_phases.removeAt(f.phase);
    rebuild();
    endResetModel();
    emit changed();
    emit edited();
}

void TodoModel::move(int from, int to)
{
    if (from == to || from < 0 || to < 0 || from >= m_flat.size() || to >= m_flat.size())
        return;
    const auto a = m_flat.at(from);
    const auto b = m_flat.at(to);
    beginResetModel();
    auto item = m_phases[a.phase].tasks.takeAt(a.task);
    // After removal, b.task already addresses the slot the dragged row should occupy.
    const int target = b.task;
    m_phases[b.phase].tasks.insert(std::min<qsizetype>(target, m_phases[b.phase].tasks.size()), item);
    if (m_phases.at(a.phase).tasks.isEmpty())
        m_phases.removeAt(a.phase);
    rebuild();
    endResetModel();
    emit changed();
    emit edited();
}

QList<TodoPhase> TodoModel::fromMarkdown(const QString &markdown)
{
    static const QRegularExpression heading(uR"(^#{1,4}\s+(.+?)\s*$)"_s);
    static const QRegularExpression check(uR"(^\s*(?:[-*+]|\d+[.)])\s+(?:\[( |x|X|~|-)\]\s+)?(.+?)\s*$)"_s);
    QList<TodoPhase> phases;
    bool inFence = false;
    for (const auto &line : markdown.split(u'\n')) {
        if (line.trimmed().startsWith("```"_L1)) {
            inFence = !inFence;
            continue;
        }
        if (inFence)
            continue;
        if (const auto h = heading.match(line); h.hasMatch()) {
            phases << TodoPhase{h.captured(1), {}};
            continue;
        }
        // Only top-level list items become steps; nested bullets are details of the step above.
        if (line.startsWith(u' ') || line.startsWith(u'\t')) {
            if (!phases.isEmpty() && !phases.last().tasks.isEmpty()) {
                auto &d = phases.last().tasks.last().details;
                d += (d.isEmpty() ? QString{} : u"\n"_s) + line.trimmed();
            }
            continue;
        }
        if (const auto c = check.match(line); c.hasMatch()) {
            if (phases.isEmpty())
                phases << TodoPhase{u"Plan"_s, {}};
            const auto mark = c.captured(1);
            const auto status = (mark == "x"_L1 || mark == "X"_L1) ? u"completed"_s
                                : mark == "~"_L1                     ? u"in_progress"_s
                                : mark == "-"_L1                     ? u"abandoned"_s
                                                                     : u"pending"_s;
            static const QRegularExpression inlineMarks(u"`|\\*\\*"_s);
            phases.last().tasks << TodoItem{c.captured(2).remove(inlineMarks), status, {}, {}};
        }
    }
    // Drop headings that introduced no steps (title, context sections).
    phases.removeIf([](const TodoPhase &p) { return p.tasks.isEmpty(); });
    return phases;
}

QString TodoModel::toMarkdown() const
{
    QString out;
    for (const auto &p : m_phases) {
        out += u"## "_s + p.name + u"\n\n"_s;
        for (const auto &t : p.tasks) {
            const auto mark = t.status == "completed"_L1 ? u"x"_s : t.status == "abandoned"_L1 ? u"-"_s : u" "_s;
            out += u"- ["_s + mark + u"] "_s + t.content + u'\n';
        }
        out += u'\n';
    }
    return out.trimmed() + u'\n';
}

} // namespace gomp
