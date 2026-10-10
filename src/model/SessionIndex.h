#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QFutureWatcher>
#include <QSortFilterProxyModel>
#include <QtQml/qqmlregistration.h>

namespace gomp {

class OmpCli;

struct SessionInfo {
    QString path;
    QString id;
    QString title;
    QString cwd;
    qint64 created = 0;  // ms since epoch
    qint64 modified = 0; // file mtime
    qint64 size = 0;
    int messages = 0;
    int userMessages = 0;
    QString model;
    QString firstPrompt;
    QString lastReply;
    bool branched = false;
    bool compacted = false;
};

// Every omp session on disk (~/.omp/agent/sessions/<cwd-slug>/*.jsonl). Files are scanned on a
// worker thread and cached by mtime+size, so reopening the browser is instant. RPC can only list
// one directory's sessions, which is why gomp reads the files itself (read-only; renames rewrite
// the padded title line in place, deletes go to the trash).
class SessionIndex : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QStringList projects READ projects NOTIFY countChanged)
public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        IdRole,
        TitleRole,
        CwdRole,
        ProjectRole,
        CreatedRole,
        ModifiedRole,
        SizeRole,
        MessagesRole,
        ModelRole,
        FirstPromptRole,
        LastReplyRole,
        BranchedRole,
        CompactedRole,
    };

    explicit SessionIndex(OmpCli *cli, QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool scanning() const { return m_watcher.isRunning(); }
    [[nodiscard]] QStringList projects() const;
    Q_INVOKABLE void rescan();
    Q_INVOKABLE QVariantMap get(const QString &path) const;
    Q_INVOKABLE QString findByTitle(const QString &title) const;
    Q_INVOKABLE bool remove(const QString &path);
    Q_INVOKABLE bool rename(const QString &path, const QString &title);
    Q_INVOKABLE void exportHtml(const QString &path);

    [[nodiscard]] QString sessionsRoot() const;
    void setRoot(const QString &root) { m_root = root; }
    // Parses one session file; public for tests.
    static SessionInfo scanFile(const QString &path);
    // Rewrites the padded first-line title in place; false if the file has no padded title line.
    static bool rewriteTitle(const QString &path, const QString &title);

signals:
    void scanningChanged();
    void countChanged();
    void notice(const QString &level, const QString &message);

private:
    void applyScan(const QList<SessionInfo> &results);
    QList<SessionInfo> m_sessions;
    QHash<QString, QPair<qint64, SessionInfo>> m_cache; // path -> (mtime+size key, info)
    QFutureWatcher<QList<SessionInfo>> m_watcher;
    OmpCli *m_cli;
    QString m_root;
};

// Search, project filter and sort over SessionIndex for the session browser.
class SessionFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString project READ project WRITE setProject NOTIFY projectChanged)
    Q_PROPERTY(QString sortKey READ sortKey WRITE setSortKey NOTIFY sortKeyChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    explicit SessionFilterModel(QObject *parent = nullptr);

    [[nodiscard]] QString query() const { return m_query; }
    void setQuery(const QString &q);
    [[nodiscard]] QString project() const { return m_project; }
    void setProject(const QString &p);
    [[nodiscard]] QString sortKey() const { return m_sortKey; }
    void setSortKey(const QString &k); // modified | created | messages | title
    [[nodiscard]] int count() const { return rowCount(); }
    Q_INVOKABLE QVariantMap get(int row) const;

signals:
    void queryChanged();
    void projectChanged();
    void sortKeyChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QString m_query;
    QString m_project;
    QString m_sortKey = QStringLiteral("modified");
};

} // namespace gomp
