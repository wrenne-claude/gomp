#include "CommandModel.h"

#include "util/FuzzyMatch.h"

#include <algorithm>

using namespace Qt::StringLiterals;

namespace gomp {

CommandModel::CommandModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CommandModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_visible.size());
}

QHash<int, QByteArray> CommandModel::roleNames() const
{
    return {{NameRole, "name"},     {DescriptionRole, "description"}, {HintRole, "hint"},
            {SourceRole, "source"}, {SubcommandsRole, "subcommands"}, {AliasesRole, "aliases"}};
}

QVariant CommandModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_visible.size())
        return {};
    const auto &c = m_all.at(m_visible.at(index.row()));
    switch (role) {
    case NameRole:
        return c.name;
    case DescriptionRole:
        return c.description;
    case HintRole:
        return c.inputHint;
    case SourceRole:
        return c.source;
    case SubcommandsRole:
        return c.subcommands;
    case AliasesRole:
        return c.aliases;
    default:
        return {};
    }
}

void CommandModel::setCommands(const QList<SlashCommand> &commands)
{
    m_all = commands;
    refilter();
}

void CommandModel::setQuery(const QString &query)
{
    if (query == m_query)
        return;
    m_query = query;
    emit queryChanged();
    refilter();
}

void CommandModel::refilter()
{
    beginResetModel();
    m_visible.clear();
    QList<QPair<int, int>> scored;
    for (int i = 0; i < m_all.size(); ++i) {
        const auto &c = m_all.at(i);
        auto best = fuzzy::match(m_query, c.name).score;
        for (const auto &alias : c.aliases)
            best = std::max(best, fuzzy::match(m_query, alias).score - 1);
        if (best < 0)
            continue;
        // Exact prefix beats fuzzy; builtins sort before extension commands on ties.
        if (c.name.startsWith(m_query))
            best += 50;
        scored.append({i, best * 2 + (c.source == "builtin"_L1 ? 1 : 0)});
    }
    std::stable_sort(scored.begin(), scored.end(), [this](const auto &a, const auto &b) {
        if (m_query.isEmpty())
            return m_all.at(a.first).name < m_all.at(b.first).name;
        return a.second > b.second;
    });
    for (const auto &[i, s] : scored)
        m_visible << i;
    endResetModel();
    emit countChanged();
}

QString CommandModel::nameAt(int row) const
{
    return row >= 0 && row < m_visible.size() ? m_all.at(m_visible.at(row)).name : QString{};
}

QString CommandModel::hintAt(int row) const
{
    return row >= 0 && row < m_visible.size() ? m_all.at(m_visible.at(row)).inputHint : QString{};
}

} // namespace gomp
