#pragma once

#include "adapter/AgentEvents.h"

#include <QAbstractListModel>
#include <QJsonArray>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// The agent's phased task list (omp `todoPhases`), flattened for a ListView with phase headers.
// Doubles as the editable plan checklist: edits are pushed back to omp with `set_todos`.
class TodoModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by Workspace")
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
    Q_PROPERTY(int completed READ completed NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)
    Q_PROPERTY(QString activeTask READ activeTask NOTIFY changed)
public:
    enum Role { PhaseRole = Qt::UserRole + 1, ContentRole, StatusRole, BlockerRole, DetailsRole, PhaseIndexRole, TaskIndexRole };

    explicit TodoModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setPhases(const QList<TodoPhase> &phases);
    [[nodiscard]] const QList<TodoPhase> &phases() const { return m_phases; }
    [[nodiscard]] QJsonArray toJson() const;
    [[nodiscard]] int completed() const;
    [[nodiscard]] int total() const;
    [[nodiscard]] QString activeTask() const;

    // Editing (plan checklist). Each emits `edited` so the workspace can sync to omp.
    Q_INVOKABLE void setContent(int row, const QString &content);
    Q_INVOKABLE void setStatus(int row, const QString &status);
    Q_INVOKABLE void insertAfter(int row, const QString &content);
    Q_INVOKABLE void removeAt(int row);
    Q_INVOKABLE void move(int from, int to);
    // Parses a markdown checklist ("- [ ] step", "## Phase") into phases; used for plan proposals.
    static QList<TodoPhase> fromMarkdown(const QString &markdown);
    Q_INVOKABLE QString toMarkdown() const;

signals:
    void changed();
    void edited();

private:
    struct Flat {
        int phase;
        int task;
    };
    void rebuild();
    QList<TodoPhase> m_phases;
    QList<Flat> m_flat;
};

} // namespace gomp
