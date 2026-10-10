#include "SettingsModel.h"

#include "util/LineDiff.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QVariantMap>

using namespace Qt::StringLiterals;

namespace gomp {

namespace {
QString jsonText(const QJsonValue &v)
{
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Indented)).trimmed();
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Indented)).trimmed();
    if (v.isBool())
        return v.toBool() ? u"true"_s : u"false"_s;
    if (v.isDouble())
        return QString::number(v.toDouble(), 'g', 12);
    return v.toString();
}
} // namespace

SettingsModel::SettingsModel(ConfigStore *store, SettingsSchema *schema, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
    , m_schema(schema)
{
    connect(store, &ConfigStore::valuesChanged, this, [this] {
        if (!m_rows.isEmpty())
            emit dataChanged(index(0), index(int(m_rows.size()) - 1));
        emit valuesChanged();
    });
    connect(store, &ConfigStore::loaded, this, [this] {
        refilter();
        emit valuesChanged();
    });
    connect(store, &ConfigStore::stagedChanged, this, &SettingsModel::stagedChanged);
    connect(store, &ConfigStore::applied, this, &SettingsModel::applied);
}

int SettingsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QHash<int, QByteArray> SettingsModel::roleNames() const
{
    return {{IdRole, "settingId"},     {LabelRole, "label"},         {DescriptionRole, "description"},
            {TypeRole, "type"},        {GroupRole, "group"},         {TabRole, "tab"},
            {ValueRole, "value"},      {ValueTextRole, "valueText"}, {DefaultRole, "defaultValue"},
            {IsDefaultRole, "isDefault"}, {StagedRole, "staged"},    {OverriddenRole, "overridden"},
            {OptionsRole, "options"}};
}

QVariant SettingsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const auto &def = m_schema->settings().at(m_rows.at(index.row()));
    const auto value = m_store->scopedValue(def.id, m_scope);
    switch (role) {
    case IdRole:
        return def.id;
    case LabelRole:
        return def.displayLabel();
    case DescriptionRole:
        return def.description;
    case TypeRole:
        return def.type;
    case GroupRole:
        return def.group.isEmpty() ? SettingsSchema::tabTitle(def.tab) : def.group;
    case TabRole:
        return SettingsSchema::tabTitle(def.tab);
    case ValueRole:
        return value.toVariant();
    case ValueTextRole:
        return jsonText(value);
    case DefaultRole:
        return def.defaultValue.toVariant();
    case IsDefaultRole:
        return value == def.defaultValue || (value.isNull() && def.defaultValue.isNull());
    case StagedRole:
        return m_store->isStaged(def.id, m_scope);
    case OverriddenRole:
        return isOverridden(def.id);
    case OptionsRole: {
        QVariantList opts;
        if (!def.options.isEmpty()) {
            for (const auto &o : def.options)
                opts << QVariantMap{{u"value"_s, o.value}, {u"label"_s, o.label}, {u"description"_s, o.description}};
        } else {
            for (const auto &v : def.values)
                opts << QVariantMap{{u"value"_s, v}, {u"label"_s, v}, {u"description"_s, QString{}}};
        }
        return opts;
    }
    default:
        return {};
    }
}

bool SettingsModel::isOverridden(const QString &id) const
{
    return m_store->isSetIn(ConfigStore::Scope::Project, id) || m_store->isStaged(id, ConfigStore::Scope::Project);
}

void SettingsModel::setSection(const QString &s)
{
    if (s == m_section && m_ids.isEmpty())
        return;
    m_section = s;
    m_ids.clear();
    emit sectionChanged();
    refilter();
}

void SettingsModel::setIds(const QStringList &ids)
{
    m_ids = ids;
    m_section.clear();
    emit sectionChanged();
    refilter();
}

void SettingsModel::setQuery(const QString &q)
{
    if (q == m_query)
        return;
    m_query = q;
    emit queryChanged();
    refilter();
}

void SettingsModel::setScope(const QString &s)
{
    const auto next = s == "project"_L1 ? ConfigStore::Scope::Project : ConfigStore::Scope::Global;
    if (next == m_scope)
        return;
    m_scope = next;
    emit scopeChanged();
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(int(m_rows.size()) - 1));
}

void SettingsModel::refilter()
{
    beginResetModel();
    m_rows.clear();
    const auto &all = m_schema->settings();
    if (!m_query.trimmed().isEmpty()) {
        const auto words = m_query.split(u' ', Qt::SkipEmptyParts);
        for (qsizetype i = 0; i < all.size(); ++i) {
            const auto &d = all.at(i);
            const auto hay = d.displayLabel() + u' ' + d.id + u' ' + d.description + u' ' + d.group;
            bool ok = true;
            for (const auto &w : words)
                ok = ok && hay.contains(w, Qt::CaseInsensitive);
            if (ok)
                m_rows << i;
        }
        // Keep each group contiguous so section headers are not repeated.
        std::stable_sort(m_rows.begin(), m_rows.end(), [&all](qsizetype a, qsizetype b) {
            const auto ga = all.at(a).group.isEmpty() ? SettingsSchema::tabTitle(all.at(a).tab) : all.at(a).group;
            const auto gb = all.at(b).group.isEmpty() ? SettingsSchema::tabTitle(all.at(b).tab) : all.at(b).group;
            return ga < gb;
        });
    } else if (!m_ids.isEmpty()) {
        for (const auto &id : m_ids) {
            for (qsizetype i = 0; i < all.size(); ++i) {
                if (all.at(i).id == id)
                    m_rows << i;
            }
        }
    } else {
        for (qsizetype i = 0; i < all.size(); ++i) {
            const auto &d = all.at(i);
            const auto tab = d.tab.isEmpty() ? u"other"_s : d.tab;
            if (tab == m_section)
                m_rows << i;
        }
        // Group rows so ListView sections are contiguous; keep omp's order within a group.
        std::stable_sort(m_rows.begin(), m_rows.end(), [&all](qsizetype a, qsizetype b) {
            const auto ga = all.at(a).group.isEmpty() ? all.at(a).id.section(u'.', 0, 0) : all.at(a).group;
            const auto gb = all.at(b).group.isEmpty() ? all.at(b).id.section(u'.', 0, 0) : all.at(b).group;
            return ga < gb;
        });
    }
    endResetModel();
    emit countChanged();
}

QVariantList SettingsModel::sections() const
{
    QHash<QString, int> counts;
    for (const auto &d : m_schema->settings())
        counts[d.tab.isEmpty() ? u"other"_s : d.tab] += 1;
    QVariantList out;
    auto tabs = SettingsSchema::tabOrder();
    tabs << u"other"_s;
    for (const auto &t : tabs) {
        if (!counts.contains(t))
            continue;
        out << QVariantMap{{u"id"_s, t},
                           {u"title"_s, SettingsSchema::tabTitle(t == "other"_L1 ? QString{} : t)},
                           {u"count"_s, counts.value(t)}};
    }
    return out;
}

QJsonValue SettingsModel::toJson(const SettingDef &def, const QVariant &value, QString *error)
{
    if (def.type == "boolean"_L1)
        return value.toBool();
    if (def.type == "number"_L1) {
        bool ok = false;
        const double d = value.toDouble(&ok);
        if (!ok) {
            *error = u"Expected a number."_s;
            return {};
        }
        return d;
    }
    if (def.type == "array"_L1 || def.type == "record"_L1)
        return QJsonValue::fromVariant(value);
    return value.toString();
}

QString SettingsModel::setValue(const QString &id, const QVariant &value)
{
    const auto *def = m_schema->find(id);
    if (!def)
        return u"Unknown setting."_s;
    QString error;
    const auto json = toJson(*def, value, &error);
    if (!error.isEmpty())
        return error;
    return m_store->stage(id, json, m_scope);
}

QString SettingsModel::setValueText(const QString &id, const QString &text)
{
    const auto *def = m_schema->find(id);
    if (!def)
        return u"Unknown setting."_s;
    QJsonParseError pe{};
    const auto doc = QJsonDocument::fromJson(text.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError)
        return u"Not valid JSON: %1"_s.arg(pe.errorString());
    return m_store->stage(id, doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object()), m_scope);
}

void SettingsModel::resetValue(const QString &id)
{
    m_store->stageReset(id, m_scope);
}

void SettingsModel::unstage(const QString &id)
{
    m_store->unstage(id, m_scope);
}

QVariant SettingsModel::value(const QString &id) const
{
    return m_store->scopedValue(id, m_scope).toVariant();
}

QStringList SettingsModel::idsWithPrefix(const QStringList &prefixes, const QStringList &exclude) const
{
    QStringList out;
    for (const auto &d : m_schema->settings()) {
        if (exclude.contains(d.id))
            continue;
        for (const auto &p : prefixes) {
            if (d.id == p || d.id.startsWith(p)) {
                out << d.id;
                break;
            }
        }
    }
    return out;
}

QVariantMap SettingsModel::describe(const QString &id) const
{
    const auto *def = m_schema->find(id);
    return def ? rowMap(*def) : QVariantMap{};
}

QVariantMap SettingsModel::rowMap(const SettingDef &def) const
{
    QVariantList opts;
    for (const auto &o : def.options)
        opts << QVariantMap{{u"value"_s, o.value}, {u"label"_s, o.label}, {u"description"_s, o.description}};
    if (opts.isEmpty()) {
        for (const auto &v : def.values)
            opts << QVariantMap{{u"value"_s, v}, {u"label"_s, v}, {u"description"_s, QString{}}};
    }
    const auto value = m_store->scopedValue(def.id, m_scope);
    return {{u"settingId"_s, def.id},
            {u"label"_s, def.displayLabel()},
            {u"description"_s, def.description},
            {u"type"_s, def.type},
            {u"value"_s, value.toVariant()},
            {u"valueText"_s, jsonText(value)},
            {u"defaultValue"_s, def.defaultValue.toVariant()},
            {u"isDefault"_s, value == def.defaultValue},
            {u"staged"_s, m_store->isStaged(def.id, m_scope)},
            {u"overridden"_s, isOverridden(def.id)},
            {u"options"_s, opts}};
}

QVariantList SettingsModel::preview() const
{
    QVariantList out;
    for (const auto &fp : m_store->preview()) {
        const auto st = diff::stats(fp.lines);
        out << QVariantMap{{u"scope"_s, fp.scope == ConfigStore::Scope::Project ? u"project"_s : u"global"_s},
                           {u"path"_s, fp.path},
                           {u"lines"_s, diff::toVariant(fp.lines)},
                           {u"added"_s, st.added},
                           {u"removed"_s, st.removed},
                           {u"hasComments"_s, fp.hasComments},
                           {u"error"_s, fp.error}};
    }
    return out;
}

void SettingsModel::apply()
{
    m_store->applyStaged();
}

void SettingsModel::discard()
{
    m_store->discardStaged();
}

void SettingsModel::reload()
{
    m_store->reload();
}

QString SettingsModel::rawText(const QString &scope) const
{
    return m_store->rawText(scope == "project"_L1 ? ConfigStore::Scope::Project : ConfigStore::Scope::Global);
}

QString SettingsModel::rawPath(const QString &scope) const
{
    return m_store->filePath(scope == "project"_L1 ? ConfigStore::Scope::Project : ConfigStore::Scope::Global);
}

QStringList SettingsModel::validateRaw(const QString &text) const
{
    return m_store->validateRaw(text);
}

QVariantList SettingsModel::rawDiff(const QString &scope, const QString &text) const
{
    return diff::toVariant(diff::compute(rawText(scope), text, 2));
}

QString SettingsModel::writeRaw(const QString &scope, const QString &text)
{
    QString error;
    if (!m_store->writeRaw(scope == "project"_L1 ? ConfigStore::Scope::Project : ConfigStore::Scope::Global, text, &error))
        return error;
    return {};
}

} // namespace gomp
