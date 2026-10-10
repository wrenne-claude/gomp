#include "WorkspaceManager.h"

#include <QDir>
#include <QFileInfo>

using namespace Qt::StringLiterals;

namespace gomp {

WorkspaceManager::WorkspaceManager(QObject *parent)
    : QAbstractListModel(parent)
{
}

void WorkspaceManager::setFactories(BackendFactory backend, LaunchFactory launch)
{
    m_backendFactory = std::move(backend);
    m_launchFactory = std::move(launch);
}

int WorkspaceManager::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QHash<int, QByteArray> WorkspaceManager::roleNames() const
{
    return {{WorkspaceRole, "workspace"}, {NameRole, "name"}, {CwdRole, "cwd"},   {ActivityRole, "activity"},
            {ModeRole, "mode"},           {BusyRole, "busy"}, {MockRole, "mock"}, {TitleRole, "title"}};
}

QVariant WorkspaceManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    auto *w = m_items.at(index.row());
    switch (role) {
    case WorkspaceRole:
        return QVariant::fromValue(w);
    case NameRole:
        return w->name();
    case CwdRole:
        return w->cwd();
    case ActivityRole:
        return int(w->activity());
    case ModeRole:
        return w->mode();
    case BusyRole:
        return w->busy();
    case MockRole:
        return w->isMock();
    case TitleRole:
        return w->sessionTitle();
    default:
        return {};
    }
}

void WorkspaceManager::rowChanged(Workspace *w, const QList<int> &roles)
{
    const auto row = m_items.indexOf(w);
    if (row >= 0)
        emit dataChanged(index(int(row)), index(int(row)), roles);
}

Workspace *WorkspaceManager::current() const
{
    return m_current >= 0 && m_current < m_items.size() ? m_items.at(m_current) : nullptr;
}

Workspace *WorkspaceManager::at(int index) const
{
    return index >= 0 && index < m_items.size() ? m_items.at(index) : nullptr;
}

void WorkspaceManager::setCurrentIndex(int index)
{
    if (index == m_current || index < -1 || index >= m_items.size())
        return;
    m_current = index;
    emit currentChanged();
}

int WorkspaceManager::busyCount() const
{
    int n = 0;
    for (auto *w : m_items)
        n += w->busy() ? 1 : 0;
    return n;
}

int WorkspaceManager::indexOfSession(const QString &sessionFile) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (!sessionFile.isEmpty() && m_items.at(i)->sessionFile() == sessionFile)
            return i;
    }
    return -1;
}

int WorkspaceManager::open(const QString &cwd, const QString &resumeSession, bool mock, const QString &initialMode)
{
    if (!m_backendFactory || !m_launchFactory)
        return -1;
    if (const auto existing = indexOfSession(resumeSession); existing >= 0) {
        setCurrentIndex(existing);
        return existing;
    }
    const auto dir = QDir(cwd).absolutePath();
    auto *w = new Workspace(dir, m_backendFactory(mock), mock, this);
    connect(w, &Workspace::activityChanged, this, [this, w] { rowChanged(w, {ActivityRole, BusyRole}); });
    connect(w, &Workspace::activityChanged, this, &WorkspaceManager::busyCountChanged);
    connect(w, &Workspace::modeChanged, this, [this, w] { rowChanged(w, {ModeRole}); });
    connect(w, &Workspace::infoChanged, this, [this, w] {
        rowChanged(w, {NameRole, TitleRole});
        emit listChanged();
    });

    const int row = int(m_items.size());
    beginInsertRows({}, row, row);
    m_items << w;
    endInsertRows();
    emit countChanged();
    emit workspaceAdded(w);

    auto opts = m_launchFactory(dir);
    opts.resumeSession = resumeSession;
    opts.initialMode = initialMode;
    w->start(opts);
    setCurrentIndex(row);
    emit listChanged();
    return row;
}

void WorkspaceManager::close(int index)
{
    if (index < 0 || index >= m_items.size())
        return;
    beginRemoveRows({}, index, index);
    auto *w = m_items.takeAt(index);
    endRemoveRows();
    w->deleteLater();
    emit countChanged();
    emit busyCountChanged();
    if (m_current >= m_items.size())
        m_current = int(m_items.size()) - 1;
    else if (index < m_current)
        --m_current;
    emit currentChanged();
    emit listChanged();
}

void WorkspaceManager::next()
{
    if (!m_items.isEmpty())
        setCurrentIndex((m_current + 1) % int(m_items.size()));
}

void WorkspaceManager::previous()
{
    if (!m_items.isEmpty())
        setCurrentIndex((m_current - 1 + int(m_items.size())) % int(m_items.size()));
}

QVariantList WorkspaceManager::snapshot() const
{
    QVariantList out;
    for (auto *w : m_items) {
        if (w->isMock())
            continue;
        out << QVariantMap{{u"cwd"_s, w->cwd()}, {u"sessionFile"_s, w->sessionFile()}};
    }
    return out;
}

} // namespace gomp
