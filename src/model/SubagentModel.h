#pragma once

#include "adapter/AgentEvents.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// Live subagents of a workspace, fed by omp's subagent_lifecycle / subagent_progress /
// subagent_event frames. Rows keep their order of appearance; finished agents stay until the
// next turn so their output can be inspected.
class SubagentModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by Workspace")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int running READ running NOTIFY statsChanged)
    Q_PROPERTY(int finished READ finished NOTIFY statsChanged)
    Q_PROPERTY(int failed READ failed NOTIFY statsChanged)
    Q_PROPERTY(double totalCost READ totalCost NOTIFY statsChanged)
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        IndexRole,
        AgentRole,
        TaskRole,
        StatusRole,
        ModelRoleRole,
        CurrentToolRole,
        RecentToolsRole,
        ToolCountRole,
        TokensRole,
        CostRole,
        DurationRole,
        OutputRole,
        ParentToolRole,
        StartedAtRole,
    };

    explicit SubagentModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void apply(const AgentEvent &event);
    void clearFinished();
    [[nodiscard]] int running() const;
    [[nodiscard]] int finished() const;
    [[nodiscard]] int failed() const;
    [[nodiscard]] double totalCost() const;
    Q_INVOKABLE int rowOf(const QString &id) const;
    Q_INVOKABLE QVariantMap get(int row) const;

signals:
    void countChanged();
    void statsChanged();

private:
    struct Agent {
        QString id;
        int index = 0;
        QString agent;
        QString task;
        QString status = QStringLiteral("pending"); // pending | running | completed | failed | aborted
        QString modelRole;
        QString currentTool;
        QStringList recentTools;
        int toolCount = 0;
        qint64 tokens = 0;
        double cost = 0;
        qint64 durationMs = 0;
        QString output;
        QString parentToolCallId;
        qint64 startedAt = 0;
    };
    int ensure(const QString &id, int index, const QString &agent);
    void touch(int row);

    QList<Agent> m_agents;
    QTimer m_flush;
    QSet<int> m_dirty;
};

} // namespace gomp
