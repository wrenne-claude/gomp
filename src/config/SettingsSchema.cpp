#include "SettingsSchema.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

using namespace Qt::StringLiterals;

namespace gomp {

QString SettingDef::displayLabel() const
{
    if (!label.isEmpty())
        return label;
    // "task.maxConcurrency" -> "Max concurrency"
    const auto last = id.section(u'.', -1);
    QString out;
    for (qsizetype i = 0; i < last.size(); ++i) {
        const QChar c = last.at(i);
        if (i > 0 && c.isUpper() && !last.at(i - 1).isUpper())
            out += u' ';
        out += i == 0 ? c.toUpper() : (c.isUpper() && i + 1 < last.size() && !last.at(i + 1).isUpper() ? c.toLower() : c);
    }
    return out;
}

bool SettingsSchema::loadBundled()
{
    QFile f(u":/gomp/schema/omp-settings.json"_s);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    return loadFromJson(f.readAll());
}

bool SettingsSchema::loadFromJson(const QByteArray &json)
{
    const auto doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return false;
    m_settings.clear();
    m_index.clear();
    m_generatedFrom = doc.object().value("generatedFrom"_L1).toString();
    for (const auto &sv : doc.object().value("settings"_L1).toArray()) {
        const auto s = sv.toObject();
        SettingDef d;
        d.id = s.value("id"_L1).toString();
        d.type = s.value("type"_L1).toString();
        d.defaultValue = s.value("default"_L1);
        for (const auto &v : s.value("values"_L1).toArray())
            d.values << v.toString();
        for (const auto &ov : s.value("options"_L1).toArray()) {
            const auto o = ov.toObject();
            d.options << SettingOption{o.value("value"_L1).toVariant().toString(), o.value("label"_L1).toString(),
                                       o.value("description"_L1).toString()};
        }
        d.tab = s.value("tab"_L1).toString();
        d.group = s.value("group"_L1).toString();
        d.label = s.value("label"_L1).toString();
        d.description = s.value("description"_L1).toString();
        if (d.id.isEmpty())
            continue;
        m_index.insert(d.id, m_settings.size());
        m_settings << d;
    }
    return !m_settings.isEmpty();
}

void SettingsSchema::mergeLive(const QJsonObject &list)
{
    for (auto it = list.begin(); it != list.end(); ++it) {
        const auto entry = it.value().toObject();
        if (const auto idx = m_index.value(it.key(), -1); idx >= 0) {
            if (m_settings[idx].description.isEmpty())
                m_settings[idx].description = entry.value("description"_L1).toString();
            continue;
        }
        SettingDef d;
        d.id = it.key();
        d.type = entry.value("type"_L1).toString(u"string"_s);
        d.description = entry.value("description"_L1).toString();
        m_index.insert(d.id, m_settings.size());
        m_settings << d;
    }
}

const SettingDef *SettingsSchema::find(const QString &id) const
{
    const auto idx = m_index.value(id, -1);
    return idx < 0 ? nullptr : &m_settings.at(idx);
}

QString SettingsSchema::validate(const SettingDef &def, const QJsonValue &value)
{
    if (value.isNull() || value.isUndefined())
        return {}; // null means "unset"
    if (def.type == "boolean"_L1 && !value.isBool())
        return u"Expected on or off."_s;
    if (def.type == "number"_L1) {
        // Range limits are omp's to enforce; `omp config set` rejects out-of-range values.
        if (!value.isDouble() || !std::isfinite(value.toDouble()))
            return u"Expected a number."_s;
    }
    if (def.type == "enum"_L1) {
        const auto s = value.toVariant().toString();
        if (!def.values.isEmpty() && !def.values.contains(s))
            return u"Choose one of: %1."_s.arg(def.values.join(u", "_s));
    }
    if (def.type == "string"_L1 && !value.isString())
        return u"Expected text."_s;
    if (def.type == "array"_L1 && !value.isArray())
        return u"Expected a list."_s;
    if (def.type == "record"_L1 && !value.isObject())
        return u"Expected a set of key/value pairs."_s;
    return {};
}

QStringList SettingsSchema::tabOrder()
{
    return {u"model"_s,  u"interaction"_s, u"context"_s, u"tools"_s,      u"files"_s,
            u"shell"_s,  u"tasks"_s,       u"memory"_s,  u"providers"_s, u"appearance"_s};
}

QString SettingsSchema::tabTitle(const QString &tab)
{
    static const QHash<QString, QString> titles{
        {u"model"_s, u"Model behaviour"_s}, {u"interaction"_s, u"Interaction"_s},
        {u"context"_s, u"Context"_s},        {u"tools"_s, u"Tools"_s},
        {u"files"_s, u"Files"_s},            {u"shell"_s, u"Shell"_s},
        {u"tasks"_s, u"Tasks and modes"_s},  {u"memory"_s, u"Memory"_s},
        {u"providers"_s, u"Provider options"_s}, {u"appearance"_s, u"Terminal UI"_s},
        {QString{}, u"Other"_s},
    };
    return titles.value(tab, tab);
}

} // namespace gomp
