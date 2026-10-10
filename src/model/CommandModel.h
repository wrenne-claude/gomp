#pragma once

#include "adapter/AgentEvents.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// Slash-command catalog for composer autocomplete, fuzzy-filtered by the text after "/".
class CommandModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by Workspace")
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Role { NameRole = Qt::UserRole + 1, DescriptionRole, HintRole, SourceRole, SubcommandsRole, AliasesRole };

    explicit CommandModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setCommands(const QList<SlashCommand> &commands);
    [[nodiscard]] const QList<SlashCommand> &commands() const { return m_all; }
    [[nodiscard]] QString query() const { return m_query; }
    void setQuery(const QString &query);
    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE QString hintAt(int row) const;

signals:
    void queryChanged();
    void countChanged();

private:
    void refilter();
    QList<SlashCommand> m_all;
    QList<int> m_visible;
    QString m_query;
};

} // namespace gomp
