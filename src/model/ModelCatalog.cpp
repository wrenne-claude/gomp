#include "ModelCatalog.h"

#include "adapter/OmpCli.h"
#include "util/FuzzyMatch.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace gomp {

ModelCatalog::ModelCatalog(OmpCli *cli, QObject *parent)
    : QAbstractListModel(parent)
    , m_cli(cli)
{
}

int ModelCatalog::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_visible.size());
}

QHash<int, QByteArray> ModelCatalog::roleNames() const
{
    return {{SelectorRole, "selector"}, {ProviderRole, "provider"},   {ModelIdRole, "modelId"},
            {NameRole, "name"},         {ContextRole, "contextWindow"}, {MaxTokensRole, "maxTokens"},
            {ReasoningRole, "reasoning"}, {EffortsRole, "efforts"},   {ImagesRole, "images"},
            {CostInRole, "costIn"},     {CostOutRole, "costOut"},     {PricingKnownRole, "pricingKnown"},
            {HighlightRole, "highlighted"}};
}

QVariant ModelCatalog::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_visible.size())
        return {};
    const auto &[mi, positions] = m_visible.at(index.row());
    const auto &m = m_models.at(mi);
    switch (role) {
    case SelectorRole:
        return m.selector;
    case ProviderRole:
        return m.provider;
    case ModelIdRole:
        return m.id;
    case NameRole:
        return m.name;
    case ContextRole:
        return double(m.context);
    case MaxTokensRole:
        return double(m.maxTokens);
    case ReasoningRole:
        return m.reasoning;
    case EffortsRole:
        return m.efforts;
    case ImagesRole:
        return m.images;
    case CostInRole:
        return m.costIn;
    case CostOutRole:
        return m.costOut;
    case PricingKnownRole:
        return m.pricingKnown;
    case HighlightRole:
        return positions.isEmpty() ? m.name.toHtmlEscaped() : fuzzy::highlight(m.name, positions, m_highlight);
    default:
        return {};
    }
}

void ModelCatalog::setQuery(const QString &q)
{
    if (q == m_query)
        return;
    m_query = q;
    emit queryChanged();
    refilter();
}

void ModelCatalog::setProvider(const QString &p)
{
    if (p == m_provider)
        return;
    m_provider = p;
    emit providerChanged();
    refilter();
}

QStringList ModelCatalog::providers() const
{
    QStringList out;
    for (const auto &m : m_models) {
        if (!out.contains(m.provider))
            out << m.provider;
    }
    return out;
}

void ModelCatalog::refresh(bool force)
{
    if (m_loading)
        return;
    m_loading = true;
    emit loadingChanged();
    QStringList args{u"models"_s};
    if (force)
        args << u"refresh"_s;
    args << u"--json"_s;
    m_cli->runJson(
        args,
        [this, force](bool ok, const QJsonDocument &doc, const QString &error) {
            m_loading = false;
            emit loadingChanged();
            if (!ok) {
                m_error = error;
                emit loaded();
                return;
            }
            m_error.clear();
            const auto models = doc.isArray() ? doc.array() : doc.object().value("models"_L1).toArray();
            setModels(models);
            // `models refresh --json` may print a summary rather than the list; re-read the catalog.
            if (force && models.isEmpty())
                refresh(false);
        },
        force ? 120'000 : 30'000);
}

void ModelCatalog::setModels(const QJsonArray &models)
{
    m_models.clear();
    for (const auto &mv : models) {
        const auto o = mv.toObject();
        if (o.value("kind"_L1).toString(u"chat"_s) != "chat"_L1)
            continue;
        Entry e;
        e.provider = o.value("provider"_L1).toString();
        e.id = o.value("id"_L1).toString();
        e.selector = o.value("selector"_L1).toString(e.provider + u'/' + e.id);
        e.name = o.value("name"_L1).toString(e.id);
        e.context = static_cast<qint64>(o.value("contextWindow"_L1).toDouble());
        e.maxTokens = static_cast<qint64>(o.value("maxTokens"_L1).toDouble());
        e.reasoning = o.value("reasoning"_L1).toBool();
        for (const auto &t : o.value("thinking"_L1).toArray())
            e.efforts << t.toString();
        for (const auto &i : o.value("input"_L1).toArray())
            e.images = e.images || i.toString() == "image"_L1;
        const auto cost = o.value("cost"_L1).toObject();
        e.costIn = cost.value("input"_L1).toDouble();
        e.costOut = cost.value("output"_L1).toDouble();
        // omp marks prices it could not verify; zero there means "unknown", not "free".
        e.pricingKnown = o.value("pricingStatus"_L1).toString() != "unknown"_L1 && (e.costIn > 0 || e.costOut > 0);
        m_models << e;
    }
    emit loaded();
    refilter();
}

void ModelCatalog::refilter()
{
    beginResetModel();
    m_visible.clear();
    QList<QPair<int, int>> scored;
    QHash<int, QList<int>> positions;
    for (int i = 0; i < m_models.size(); ++i) {
        const auto &m = m_models.at(i);
        if (!m_provider.isEmpty() && m.provider != m_provider)
            continue;
        const auto nameMatch = fuzzy::match(m_query, m.name);
        const auto selMatch = fuzzy::match(m_query, m.selector);
        if (!nameMatch.matched() && !selMatch.matched())
            continue;
        positions.insert(i, nameMatch.matched() ? nameMatch.positions : QList<int>{});
        scored.append({i, std::max(nameMatch.score, selMatch.score)});
    }
    std::stable_sort(scored.begin(), scored.end(), [this](const auto &a, const auto &b) {
        if (!m_query.isEmpty() && a.second != b.second)
            return a.second > b.second;
        const auto &ma = m_models.at(a.first);
        const auto &mb = m_models.at(b.first);
        return ma.provider == mb.provider ? ma.name.localeAwareCompare(mb.name) < 0 : ma.provider < mb.provider;
    });
    for (const auto &[i, s] : scored)
        m_visible.append({i, m_query.isEmpty() ? QList<int>{} : positions.value(i)});
    endResetModel();
    emit countChanged();
}

QVariantMap ModelCatalog::get(const QString &selector) const
{
    for (const auto &m : m_models) {
        if (m.selector == selector || m.selector == selector.section(u':', 0, 0)) {
            return {{u"selector"_s, m.selector}, {u"provider"_s, m.provider},     {u"modelId"_s, m.id},
                    {u"name"_s, m.name},         {u"contextWindow"_s, double(m.context)},
                    {u"maxTokens"_s, double(m.maxTokens)}, {u"reasoning"_s, m.reasoning},
                    {u"efforts"_s, m.efforts},   {u"images"_s, m.images},     {u"costIn"_s, m.costIn},
                    {u"costOut"_s, m.costOut},   {u"pricingKnown"_s, m.pricingKnown}};
        }
    }
    return {};
}

QVariantMap ModelCatalog::at(int row) const
{
    if (row < 0 || row >= m_visible.size())
        return {};
    return get(m_models.at(m_visible.at(row).first).selector);
}

} // namespace gomp
