#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// Branch and dirty state for a workspace directory. Reads .git/HEAD directly (cheap, no process)
// and asks `git status --porcelain` for the change count, refreshing when HEAD or the index moves.
class GitInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by Workspace")
    Q_PROPERTY(bool isRepository READ isRepository NOTIFY changed)
    Q_PROPERTY(QString branch READ branch NOTIFY changed)
    Q_PROPERTY(int changedFiles READ changedFiles NOTIFY changed)
public:
    explicit GitInfo(QObject *parent = nullptr);

    void setDirectory(const QString &dir);
    [[nodiscard]] bool isRepository() const { return !m_gitDir.isEmpty(); }
    [[nodiscard]] QString branch() const { return m_branch; }
    [[nodiscard]] int changedFiles() const { return m_changed; }
    Q_INVOKABLE void refresh();

    static QString findGitDir(const QString &dir);
    static QString branchFromHead(const QString &headContents);

signals:
    void changed();

private:
    QString m_dir;
    QString m_gitDir;
    QString m_branch;
    int m_changed = 0;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};

} // namespace gomp
