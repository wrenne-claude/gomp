#include "PaletteModel.h"

#include "ModelCatalog.h"
#include "SessionIndex.h"
#include "config/SettingsSchema.h"
#include "util/Format.h"
#include "util/FuzzyMatch.h"

#include <QVariantMap>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace gomp {

namespace {
constexpr int MaxRows = 80;
}

PaletteModel::PaletteModel(SettingsSchema *schema, SessionIndex *sessions, ModelCatalog *models, QObject *parent)
    : QAbstractListModel(parent)
    , m_schema(schema)
    , m_sessions(sessions)
    , m_models(models)
{
}

int PaletteModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_visible.size());
}

QHash<int, QByteArray> PaletteModel::roleNames() const
{
    return {{IdRole, "entryId"},       {KindRole, "kind"},         {TitleRole, "title"},
            {SubtitleRole, "subtitle"}, {CategoryRole, "category"}, {ShortcutRole, "shortcut"},
            {IconRole, "icon"},        {HighlightRole, "highlighted"}, {PayloadRole, "payload"}};
}

QVariant PaletteModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_visible.size())
        return {};
    const auto &[ei, positions] = m_visible.at(index.row());
    const auto &e = m_entries.at(ei);
    switch (role) {
    case IdRole:
        return e.id;
    case KindRole:
        return e.kind;
    case TitleRole:
        return e.title;
    case SubtitleRole:
        return e.subtitle;
    case CategoryRole:
        return e.category;
    case ShortcutRole:
        return e.shortcut;
    case IconRole:
        return e.icon;
    case HighlightRole:
        return positions.isEmpty() ? e.title.toHtmlEscaped() : fuzzy::highlight(e.title, positions, m_color);
    case PayloadRole:
        return e.payload;
    default:
        return {};
    }
}

void PaletteModel::setActions(const QVariantList &actions)
{
    m_actions.clear();
    for (const auto &av : actions) {
        const auto a = av.toMap();
        m_actions << Entry{a.value(u"id"_s).toString(),
                           u"action"_s,
                           a.value(u"title"_s).toString(),
                           a.value(u"subtitle"_s).toString(),
                           a.value(u"category"_s).toString(),
                           a.value(u"shortcut"_s).toString(),
                           a.value(u"icon"_s).toString(),
                           a.value(u"id"_s).toString(),
                           20};
    }
    rebuild();
}

void PaletteModel::setCommands(const QStringList &names, const QStringList &descriptions)
{
    m_commands.clear();
    for (qsizetype i = 0; i < names.size(); ++i) {
        m_commands << Entry{u"cmd:"_s + names.at(i), u"command"_s, u'/' + names.at(i), descriptions.value(i),
                            u"Slash command"_s, {}, u"terminal"_s, names.at(i), 0};
    }
    rebuild();
}

void PaletteModel::rebuild()
{
    m_entries = m_actions;
    m_entries << m_commands;
    for (const auto &d : m_schema->settings()) {
        const auto where = d.group.isEmpty() ? SettingsSchema::tabTitle(d.tab) : d.group;
        m_entries << Entry{u"setting:"_s + d.id, u"setting"_s, d.displayLabel(), where + u" · "_s + d.id,
                           u"Setting"_s, {}, u"sliders"_s, d.id, -2};
    }
    for (int i = 0; i < std::min(m_sessions->rowCount(), 200); ++i) {
        const auto idx = m_sessions->index(i);
        m_entries << Entry{u"session:"_s + idx.data(SessionIndex::PathRole).toString(),
                           u"session"_s,
                           idx.data(SessionIndex::TitleRole).toString(),
                           idx.data(SessionIndex::ProjectRole).toString() + u", "_s
                               + Format::relativeTime(idx.data(SessionIndex::ModifiedRole).toDouble()),
                           u"Session"_s,
                           {},
                           u"history"_s,
                           idx.data(SessionIndex::PathRole).toString(),
                           -4};
    }
    for (int i = 0; i < m_models->rowCount(); ++i) {
        const auto m = m_models->at(i);
        m_entries << Entry{u"model:"_s + m.value(u"selector"_s).toString(),
                           u"model"_s,
                           u"Use "_s + m.value(u"name"_s).toString(),
                           m.value(u"selector"_s).toString(),
                           u"Model"_s,
                           {},
                           u"chip"_s,
                           m.value(u"selector"_s).toString(),
                           -6};
    }
    refilter();
}

void PaletteModel::setQuery(const QString &q)
{
    if (q == m_query)
        return;
    m_query = q;
    emit queryChanged();
    refilter();
}

void PaletteModel::refilter()
{
    beginResetModel();
    m_visible.clear();
    if (m_query.trimmed().isEmpty()) {
        for (int i = 0; i < m_entries.size() && m_visible.size() < MaxRows; ++i) {
            if (m_entries.at(i).kind == "action"_L1)
                m_visible.append(QPair<int, QList<int>>{i, {}});
        }
    } else {
        QList<std::tuple<int, int, QList<int>>> scored;
        for (int i = 0; i < m_entries.size(); ++i) {
            const auto &e = m_entries.at(i);
            const auto m = fuzzy::match(m_query, e.title);
            if (m.matched()) {
                scored.append({i, m.score + e.boost, m.positions});
                continue;
            }
            // Fall back to the subtitle (setting ids, model selectors) without title highlights.
            const auto sub = fuzzy::match(m_query, e.subtitle);
            if (sub.matched())
                scored.append({i, sub.score / 2 + e.boost - 4, QList<int>{}});
        }
        std::stable_sort(scored.begin(), scored.end(),
                         [](const auto &a, const auto &b) { return std::get<1>(a) > std::get<1>(b); });
        for (qsizetype i = 0; i < std::min<qsizetype>(MaxRows, scored.size()); ++i)
            m_visible.append(QPair<int, QList<int>>{std::get<0>(scored.at(i)), std::get<2>(scored.at(i))});
    }
    endResetModel();
    emit countChanged();
}

QVariantMap PaletteModel::get(int row) const
{
    if (row < 0 || row >= m_visible.size())
        return {};
    const auto &e = m_entries.at(m_visible.at(row).first);
    return {{u"id"_s, e.id}, {u"kind"_s, e.kind}, {u"title"_s, e.title}, {u"payload"_s, e.payload}};
}

} // namespace gomp
