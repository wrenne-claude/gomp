#pragma once

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

namespace gomp {

struct SettingOption {
    QString value;
    QString label;
    QString description;
};

struct SettingDef {
    QString id;          // dotted key, e.g. "tools.approvalMode"
    QString type;        // boolean | number | enum | string | array | record
    QJsonValue defaultValue;
    QStringList values;  // enum values
    QList<SettingOption> options;
    QString tab;         // omp's settings tab id, empty when omp shows it nowhere
    QString group;
    QString label;
    QString description;

    [[nodiscard]] QStringList path() const { return id.split(QLatin1Char('.')); }
    [[nodiscard]] QString displayLabel() const;
};

// omp's settings schema: labels, groups, enum options and defaults extracted from the omp binary
// (tools/extract-omp-schema.mjs), merged at runtime with the keys `omp config list --json`
// reports, so settings added by a newer omp still appear (under their omp type, without labels).
class SettingsSchema
{
public:
    bool loadBundled(); // :/gomp/schema/omp-settings.json
    bool loadFromJson(const QByteArray &json);
    // Adds keys known to the installed omp but missing from the bundled schema.
    void mergeLive(const QJsonObject &configList);

    [[nodiscard]] const QList<SettingDef> &settings() const { return m_settings; }
    [[nodiscard]] const SettingDef *find(const QString &id) const;
    [[nodiscard]] QString generatedFrom() const { return m_generatedFrom; }

    // Validates a candidate value; returns an empty string when valid, else a message for the UI.
    static QString validate(const SettingDef &def, const QJsonValue &value);

    // Ordered tab ids and their display names.
    static QStringList tabOrder();
    static QString tabTitle(const QString &tab);

private:
    QList<SettingDef> m_settings;
    QHash<QString, qsizetype> m_index;
    QString m_generatedFrom;
};

} // namespace gomp
