#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QtQml/qqmlregistration.h>

namespace gomp {

class OmpCli;

// Models omp can use (`omp models --json`): provider, context window, output limit, reasoning
// efforts, modalities and pricing. Feeds every model picker; filtered by fuzzy query and provider.
class ModelCatalog : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString provider READ provider WRITE setProvider NOTIFY providerChanged)
    Q_PROPERTY(QStringList providers READ providers NOTIFY loaded)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY loaded)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int total READ total NOTIFY loaded)
public:
    enum Role {
        SelectorRole = Qt::UserRole + 1,
        ProviderRole,
        ModelIdRole,
        NameRole,
        ContextRole,
        MaxTokensRole,
        ReasoningRole,
        EffortsRole,
        ImagesRole,
        CostInRole,
        CostOutRole,
        PricingKnownRole,
        HighlightRole,
    };

    explicit ModelCatalog(OmpCli *cli, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString query() const { return m_query; }
    void setQuery(const QString &q);
    [[nodiscard]] QString provider() const { return m_provider; }
    void setProvider(const QString &p);
    [[nodiscard]] QStringList providers() const;
    [[nodiscard]] bool loading() const { return m_loading; }
    [[nodiscard]] QString error() const { return m_error; }
    [[nodiscard]] int total() const { return int(m_models.size()); }

    Q_INVOKABLE void refresh(bool force = false);
    Q_INVOKABLE QVariantMap get(const QString &selector) const;
    Q_INVOKABLE QVariantMap at(int row) const;
    void setModels(const QJsonArray &models);
    void setHighlightColor(const QString &c) { m_highlight = c; }

signals:
    void queryChanged();
    void providerChanged();
    void loaded();
    void loadingChanged();
    void countChanged();

private:
    struct Entry {
        QString selector;
        QString provider;
        QString id;
        QString name;
        qint64 context = 0;
        qint64 maxTokens = 0;
        bool reasoning = false;
        QStringList efforts;
        bool images = false;
        double costIn = 0;
        double costOut = 0;
        bool pricingKnown = false;
    };
    void refilter();
    QList<Entry> m_models;
    QList<QPair<int, QList<int>>> m_visible; // model index, matched positions in the name
    QString m_query;
    QString m_provider;
    QString m_error;
    QString m_highlight = QStringLiteral("#FFB547");
    bool m_loading = false;
    OmpCli *m_cli;
};

} // namespace gomp
