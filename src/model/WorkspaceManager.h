#pragma once

#include "Workspace.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

#include <functional>

namespace gomp {

// The open workspaces (titlebar tabs). Each row exposes its Workspace object plus the fields the
// tab strip needs, so tabs update without binding into every workspace.
class WorkspaceManager : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(gomp::Workspace *current READ current NOTIFY currentChanged)
    Q_PROPERTY(int busyCount READ busyCount NOTIFY busyCountChanged)
public:
    enum Role { WorkspaceRole = Qt::UserRole + 1, NameRole, CwdRole, ActivityRole, ModeRole, BusyRole, MockRole, TitleRole };

    // Creates the backend for a new workspace (real omp or mock) and the launch options.
    using BackendFactory = std::function<AgentBackend *(bool mock)>;
    using LaunchFactory = std::function<LaunchOptions(const QString &cwd)>;

    explicit WorkspaceManager(QObject *parent = nullptr);
    void setFactories(BackendFactory backend, LaunchFactory launch);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int currentIndex() const { return m_current; }
    void setCurrentIndex(int index);
    [[nodiscard]] Workspace *current() const;
    [[nodiscard]] int busyCount() const;
    [[nodiscard]] const QList<Workspace *> &workspaces() const { return m_items; }

    Q_INVOKABLE int open(const QString &cwd, const QString &resumeSession = {}, bool mock = false,
                         const QString &initialMode = {});
    Q_INVOKABLE void close(int index);
    Q_INVOKABLE int indexOfSession(const QString &sessionFile) const;
    Q_INVOKABLE Workspace *at(int index) const;
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();

    // Serialises open workspaces for restore on next launch.
    [[nodiscard]] QVariantList snapshot() const;

signals:
    void countChanged();
    void currentChanged();
    void busyCountChanged();
    void workspaceAdded(gomp::Workspace *workspace);
    void listChanged();

private:
    void rowChanged(Workspace *w, const QList<int> &roles);

    QList<Workspace *> m_items;
    int m_current = -1;
    BackendFactory m_backendFactory;
    LaunchFactory m_launchFactory;
};

} // namespace gomp
