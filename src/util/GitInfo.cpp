#include "GitInfo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

using namespace Qt::StringLiterals;

namespace gomp {

GitInfo::GitInfo(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(250);
    connect(&m_debounce, &QTimer::timeout, this, &GitInfo::refresh);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
}

QString GitInfo::findGitDir(const QString &dir)
{
    QDir d(dir);
    while (true) {
        const QFileInfo dotGit(d.filePath(u".git"_s));
        if (dotGit.isDir())
            return dotGit.absoluteFilePath();
        if (dotGit.isFile()) { // worktree or submodule: "gitdir: <path>"
            QFile f(dotGit.absoluteFilePath());
            if (f.open(QIODevice::ReadOnly)) {
                const auto line = QString::fromUtf8(f.readLine()).trimmed();
                if (line.startsWith("gitdir:"_L1))
                    return QDir(d.path()).absoluteFilePath(line.mid(7).trimmed());
            }
        }
        if (!d.cdUp())
            return {};
    }
}

QString GitInfo::branchFromHead(const QString &head)
{
    const auto h = head.trimmed();
    if (h.startsWith("ref: refs/heads/"_L1))
        return h.mid(16);
    if (h.startsWith("ref: "_L1))
        return h.mid(5);
    return h.left(8); // detached HEAD: short hash
}

void GitInfo::setDirectory(const QString &dir)
{
    if (dir == m_dir)
        return;
    m_dir = dir;
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    m_gitDir = findGitDir(dir);
    if (!m_gitDir.isEmpty())
        m_watcher.addPaths({m_gitDir + u"/HEAD"_s, m_gitDir + u"/index"_s});
    refresh();
}

void GitInfo::refresh()
{
    if (m_gitDir.isEmpty()) {
        m_branch.clear();
        m_changed = 0;
        emit changed();
        return;
    }
    QFile head(m_gitDir + u"/HEAD"_s);
    if (head.open(QIODevice::ReadOnly))
        m_branch = branchFromHead(QString::fromUtf8(head.readAll()));
    // Git rewrites files atomically, which drops them from the watcher; re-add.
    for (const auto &p : {m_gitDir + u"/HEAD"_s, m_gitDir + u"/index"_s}) {
        if (!m_watcher.files().contains(p) && QFileInfo::exists(p))
            m_watcher.addPath(p);
    }
    emit changed();

    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(m_dir);
    connect(proc, &QProcess::finished, this, [this, proc](int code, QProcess::ExitStatus) {
        if (code == 0) {
            const auto out = proc->readAllStandardOutput();
            m_changed = int(out.count('\n'));
            emit changed();
        }
        proc->deleteLater();
    });
    connect(proc, &QProcess::errorOccurred, proc, &QObject::deleteLater);
    proc->start(u"git"_s, {u"status"_s, u"--porcelain"_s, u"--untracked-files=normal"_s});
}

} // namespace gomp
