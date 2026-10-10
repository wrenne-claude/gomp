#pragma once

#include "config/ConfigStore.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// omp settings as rows for the Settings screen: one tab ("tools", "interaction", ...), an explicit
// id list (curated pages such as Permissions), or a global search. Each row carries the label,
// description, type, options, scoped value, default and override state, so one generic delegate
// renders every setting. Edits stage through ConfigStore and apply after a diff review.
class SettingsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(QString section READ section WRITE setSection NOTIFY sectionChanged)
    Q_PROPERTY(QStringList ids READ ids WRITE setIds NOTIFY sectionChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString scope READ scope WRITE setScope NOTIFY scopeChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int stagedCount READ stagedCount NOTIFY stagedChanged)
    Q_PROPERTY(bool hasProject READ hasProject NOTIFY projectChanged)
    Q_PROPERTY(QString projectDir READ projectDir NOTIFY projectChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY valuesChanged)
    Q_PROPERTY(QVariantList sections READ sections NOTIFY valuesChanged)
    Q_PROPERTY(QString schemaVersion READ schemaVersion CONSTANT)
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        LabelRole,
        DescriptionRole,
        TypeRole,
        GroupRole,
        TabRole,
        ValueRole,
        ValueTextRole,
        DefaultRole,
        IsDefaultRole,
        StagedRole,
        OverriddenRole,
        OptionsRole,
    };

    SettingsModel(ConfigStore *store, SettingsSchema *schema, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString section() const { return m_section; }
    void setSection(const QString &s);
    [[nodiscard]] QStringList ids() const { return m_ids; }
    void setIds(const QStringList &ids);
    [[nodiscard]] QString query() const { return m_query; }
    void setQuery(const QString &q);
    [[nodiscard]] QString scope() const { return m_scope == ConfigStore::Scope::Project ? QStringLiteral("project") : QStringLiteral("global"); }
    void setScope(const QString &s);
    [[nodiscard]] int stagedCount() const { return m_store->stagedCount(); }
    [[nodiscard]] bool hasProject() const { return !m_store->projectDir().isEmpty(); }
    [[nodiscard]] QString projectDir() const { return m_store->projectDir(); }
    [[nodiscard]] bool loaded() const { return m_store->isLoaded(); }
    [[nodiscard]] QVariantList sections() const;
    [[nodiscard]] QString schemaVersion() const { return m_schema->generatedFrom(); }

    Q_INVOKABLE QString setValue(const QString &id, const QVariant &value);
    Q_INVOKABLE QString setValueText(const QString &id, const QString &json); // arrays/records
    Q_INVOKABLE void resetValue(const QString &id);
    Q_INVOKABLE void unstage(const QString &id);
    Q_INVOKABLE QVariant value(const QString &id) const;
    Q_INVOKABLE QStringList idsWithPrefix(const QStringList &prefixes, const QStringList &exclude = {}) const;
    Q_INVOKABLE QVariantMap describe(const QString &id) const;
    Q_INVOKABLE bool isOverridden(const QString &id) const;
    Q_INVOKABLE QVariantList preview() const;
    Q_INVOKABLE void apply();
    Q_INVOKABLE void discard();
    Q_INVOKABLE void reload();
    // Raw editor
    Q_INVOKABLE QString rawText(const QString &scope) const;
    Q_INVOKABLE QString rawPath(const QString &scope) const;
    Q_INVOKABLE QStringList validateRaw(const QString &text) const;
    Q_INVOKABLE QVariantList rawDiff(const QString &scope, const QString &text) const;
    Q_INVOKABLE QString writeRaw(const QString &scope, const QString &text);
    Q_INVOKABLE QString backupDir() const { return ConfigStore::backupDir(); }

signals:
    void sectionChanged();
    void queryChanged();
    void scopeChanged();
    void countChanged();
    void stagedChanged();
    void projectChanged();
    void valuesChanged();
    void applied(bool ok, const QString &message);

private:
    void refilter();
    [[nodiscard]] QVariantMap rowMap(const SettingDef &def) const;
    [[nodiscard]] static QJsonValue toJson(const SettingDef &def, const QVariant &value, QString *error);

    ConfigStore *m_store;
    SettingsSchema *m_schema;
    QString m_section;
    QStringList m_ids;
    QString m_query;
    ConfigStore::Scope m_scope = ConfigStore::Scope::Global;
    QList<qsizetype> m_rows; // indices into schema settings
};

} // namespace gomp
