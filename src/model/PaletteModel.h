#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace gomp {

class SettingsSchema;
class SessionIndex;
class ModelCatalog;

// The command palette (Ctrl+K). Actions are registered from QML (where their handlers live);
// settings, sessions and models are added from the C++ catalogs so every setting is reachable.
// Rows are fuzzy-ranked across all sources; an empty query shows actions only.
class PaletteModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString highlightColor READ highlightColor WRITE setHighlightColor NOTIFY queryChanged)
public:
    enum Role { IdRole = Qt::UserRole + 1, KindRole, TitleRole, SubtitleRole, CategoryRole, ShortcutRole, IconRole, HighlightRole, PayloadRole };

    PaletteModel(SettingsSchema *schema, SessionIndex *sessions, ModelCatalog *models, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString query() const { return m_query; }
    void setQuery(const QString &q);
    [[nodiscard]] QString highlightColor() const { return m_color; }
    void setHighlightColor(const QString &c) { m_color = c; }

    // Each action: {id, title, subtitle?, category, shortcut?, icon?}
    Q_INVOKABLE void setActions(const QVariantList &actions);
    Q_INVOKABLE void setCommands(const QStringList &names, const QStringList &descriptions);
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE void rebuild();

signals:
    void queryChanged();
    void countChanged();

private:
    struct Entry {
        QString id;
        QString kind; // action | setting | session | model | command
        QString title;
        QString subtitle;
        QString category;
        QString shortcut;
        QString icon;
        QString payload;
        int boost = 0;
    };
    void refilter();

    SettingsSchema *m_schema;
    SessionIndex *m_sessions;
    ModelCatalog *m_models;
    QList<Entry> m_actions;
    QList<Entry> m_commands;
    QList<Entry> m_entries;
    QList<QPair<int, QList<int>>> m_visible;
    QString m_query;
    QString m_color = QStringLiteral("#FFB547");
};

} // namespace gomp
