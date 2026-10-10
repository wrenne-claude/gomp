#include "SessionIndex.h"

#include "adapter/OmpCli.h"
#include "util/Markdown.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QtConcurrent/QtConcurrentRun>

using namespace Qt::StringLiterals;

namespace gomp {

namespace {
constexpr qint64 MaxFullRead = 48LL * 1024 * 1024;
constexpr qint64 HeadTailBytes = 2LL * 1024 * 1024;

QString textOf(const QJsonValue &content)
{
    if (content.isString())
        return content.toString();
    for (const auto &c : content.toArray()) {
        const auto o = c.toObject();
        if (o.value("type"_L1).toString() == "text"_L1 && !o.value("text"_L1).toString().trimmed().isEmpty())
            return o.value("text"_L1).toString();
    }
    return {};
}
} // namespace

SessionIndex::SessionIndex(OmpCli *cli, QObject *parent)
    : QAbstractListModel(parent)
    , m_cli(cli)
{
    connect(&m_watcher, &QFutureWatcher<QList<SessionInfo>>::finished, this, [this] {
        applyScan(m_watcher.result());
        emit scanningChanged();
    });
}

QString SessionIndex::sessionsRoot() const
{
    if (!m_root.isEmpty())
        return m_root;
    const auto env = qEnvironmentVariable("PI_CODING_AGENT_DIR");
    if (!env.isEmpty())
        return env + u"/sessions"_s;
    return QDir::homePath() + u"/.omp/agent/sessions"_s;
}

SessionInfo SessionIndex::scanFile(const QString &path)
{
    SessionInfo info;
    info.path = path;
    const QFileInfo fi(path);
    info.size = fi.size();
    info.modified = fi.lastModified().toMSecsSinceEpoch();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return info;
    QByteArray bytes;
    if (info.size <= MaxFullRead) {
        bytes = f.readAll();
    } else {
        // Huge transcripts: the header and the latest activity are enough for a list row.
        bytes = f.read(HeadTailBytes);
        f.seek(info.size - HeadTailBytes);
        f.readLine(); // resync to a line boundary
        bytes += '\n' + f.readAll();
    }

    QSet<QByteArray> parents;
    qsizetype pos = 0;
    while (pos < bytes.size()) {
        auto nl = bytes.indexOf('\n', pos);
        if (nl < 0)
            nl = bytes.size();
        const auto line = QByteArrayView(bytes).sliced(pos, nl - pos);
        pos = nl + 1;
        if (line.size() < 10)
            continue;
        // Cheap prefilter: only parse the line types the list needs.
        const bool isMessage = line.contains("\"type\":\"message\"");
        if (isMessage) {
            ++info.messages;
            const bool user = line.contains("\"role\":\"user\"");
            const bool assistant = line.contains("\"role\":\"assistant\"");
            if (user)
                ++info.userMessages;
            if ((user && info.firstPrompt.isEmpty()) || assistant) {
                const auto msg = QJsonDocument::fromJson(line.toByteArray()).object().value("message"_L1).toObject();
                const auto text = textOf(msg.value("content"_L1));
                if (user && info.firstPrompt.isEmpty() && !msg.value("synthetic"_L1).toBool())
                    info.firstPrompt = md::plainPreview(text, 200);
                else if (assistant && !text.isEmpty())
                    info.lastReply = text;
                if (assistant && msg.contains("model"_L1))
                    info.model = msg.value("provider"_L1).toString() + u'/' + msg.value("model"_L1).toString();
            }
        } else if (line.contains("\"type\":\"session\"") || line.contains("\"type\":\"title\"")
                   || line.contains("\"type\":\"title_change\"") || line.contains("\"type\":\"model_change\"")) {
            const auto o = QJsonDocument::fromJson(line.toByteArray()).object();
            const auto type = o.value("type"_L1).toString();
            if (type == "session"_L1) {
                info.id = o.value("id"_L1).toString();
                info.cwd = o.value("cwd"_L1).toString();
                info.created = QDateTime::fromString(o.value("timestamp"_L1).toString(), Qt::ISODateWithMs).toMSecsSinceEpoch();
            } else if (type == "title"_L1 || type == "title_change"_L1) {
                info.title = o.value("title"_L1).toString();
            } else if (type == "model_change"_L1 && info.model.isEmpty()) {
                info.model = o.value("model"_L1).toString();
            }
        } else if (line.contains("\"type\":\"compaction\"")) {
            info.compacted = true;
        }
        // Branch detection: an entry id used as a parent twice means the tree forks.
        const auto pidAt = line.indexOf("\"parentId\":\"");
        if (pidAt >= 0) {
            const auto start = pidAt + 12;
            const auto end = line.indexOf('"', start);
            if (end > start) {
                const auto pid = line.sliced(start, end - start).toByteArray();
                if (parents.contains(pid))
                    info.branched = true;
                parents.insert(pid);
            }
        }
    }
    info.lastReply = md::plainPreview(info.lastReply, 200);
    if (info.title.isEmpty())
        info.title = info.firstPrompt.isEmpty() ? u"Untitled session"_s : info.firstPrompt.left(80);
    if (info.created == 0)
        info.created = fi.birthTime().isValid() ? fi.birthTime().toMSecsSinceEpoch() : info.modified;
    return info;
}

void SessionIndex::rescan()
{
    if (m_watcher.isRunning())
        return;
    const auto cache = m_cache;
    const auto root = sessionsRoot();
    m_watcher.setFuture(QtConcurrent::run([root, cache] {
        QList<SessionInfo> out;
        // omp's layout is <root>/<cwd-slug>/<timestamp>_<id>.jsonl; subagent transcripts sit in
        // deeper folders and are not sessions of their own.
        const auto projects = QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto &projectDir : projects) {
          const auto files = QDir(projectDir.absoluteFilePath()).entryInfoList({u"*.jsonl"_s}, QDir::Files);
          for (const auto &file : files) {
            const auto path = file.absoluteFilePath();
            const QFileInfo fi(path);
            const qint64 key = fi.lastModified().toMSecsSinceEpoch() ^ (fi.size() << 20);
            const auto cached = cache.constFind(path);
            if (cached != cache.cend() && cached->first == key) {
                out << cached->second;
                continue;
            }
            auto info = scanFile(path);
            if (info.messages > 0)
                out << info;
          }
        }
        return out;
    }));
    emit scanningChanged();
}

void SessionIndex::applyScan(const QList<SessionInfo> &results)
{
    beginResetModel();
    m_sessions = results;
    m_cache.clear();
    for (const auto &s : results) {
        const QFileInfo fi(s.path);
        m_cache.insert(s.path, {fi.lastModified().toMSecsSinceEpoch() ^ (fi.size() << 20), s});
    }
    endResetModel();
    emit countChanged();
}

int SessionIndex::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_sessions.size());
}

QHash<int, QByteArray> SessionIndex::roleNames() const
{
    return {{PathRole, "path"},       {IdRole, "sessionId"},       {TitleRole, "title"},
            {CwdRole, "cwd"},         {ProjectRole, "project"},    {CreatedRole, "created"},
            {ModifiedRole, "modified"}, {SizeRole, "size"},        {MessagesRole, "messages"},
            {ModelRole, "model"},     {FirstPromptRole, "firstPrompt"}, {LastReplyRole, "lastReply"},
            {BranchedRole, "branched"}, {CompactedRole, "compacted"}};
}

QVariant SessionIndex::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_sessions.size())
        return {};
    const auto &s = m_sessions.at(index.row());
    switch (role) {
    case PathRole:
        return s.path;
    case IdRole:
        return s.id;
    case TitleRole:
        return s.title;
    case CwdRole:
        return s.cwd;
    case ProjectRole:
        return QFileInfo(s.cwd).fileName();
    case CreatedRole:
        return double(s.created);
    case ModifiedRole:
        return double(s.modified);
    case SizeRole:
        return double(s.size);
    case MessagesRole:
        return s.messages;
    case ModelRole:
        return s.model;
    case FirstPromptRole:
        return s.firstPrompt;
    case LastReplyRole:
        return s.lastReply;
    case BranchedRole:
        return s.branched;
    case CompactedRole:
        return s.compacted;
    default:
        return {};
    }
}

QStringList SessionIndex::projects() const
{
    QStringList out;
    for (const auto &s : m_sessions) {
        const auto p = QFileInfo(s.cwd).fileName();
        if (!p.isEmpty() && !out.contains(p))
            out << p;
    }
    out.sort(Qt::CaseInsensitive);
    return out;
}

QVariantMap SessionIndex::get(const QString &path) const
{
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions.at(i).path != path)
            continue;
        QVariantMap out;
        const auto names = roleNames();
        for (auto it = names.cbegin(); it != names.cend(); ++it)
            out.insert(QString::fromUtf8(it.value()), data(index(i), it.key()));
        return out;
    }
    return {};
}

QString SessionIndex::findByTitle(const QString &title) const
{
    for (const auto &s : m_sessions) {
        if (s.title == title)
            return s.path;
    }
    return {};
}

bool SessionIndex::remove(const QString &path)
{
    if (!QFile::moveToTrash(path)) {
        emit notice(u"error"_s, u"Could not move the session to the trash."_s);
        return false;
    }
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions.at(i).path == path) {
            beginRemoveRows({}, i, i);
            m_sessions.removeAt(i);
            endRemoveRows();
            break;
        }
    }
    m_cache.remove(path);
    emit countChanged();
    emit notice(u"info"_s, u"Session moved to the trash. Restore it from there if needed."_s);
    return true;
}

bool SessionIndex::rewriteTitle(const QString &path, const QString &title)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite))
        return false;
    const auto first = f.readLine();
    const auto obj = QJsonDocument::fromJson(first).object();
    if (obj.value("type"_L1).toString() != "title"_L1 || !obj.contains("pad"_L1))
        return false;
    // omp pads the first line so the title can be rewritten without moving the rest of the file.
    auto next = obj;
    next.insert("title"_L1, title);
    next.insert("source"_L1, u"user"_s);
    next.insert("updatedAt"_L1, QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    next.insert("pad"_L1, QString{});
    auto line = QJsonDocument(next).toJson(QJsonDocument::Compact);
    const auto budget = first.size() - 1; // excluding '\n'
    if (line.size() > budget)
        return false;
    next.insert("pad"_L1, QString(budget - line.size(), u' '));
    line = QJsonDocument(next).toJson(QJsonDocument::Compact);
    if (line.size() != budget)
        return false;
    f.seek(0);
    return f.write(line) == budget;
}

bool SessionIndex::rename(const QString &path, const QString &title)
{
    const auto t = title.trimmed().left(80);
    if (t.isEmpty() || !rewriteTitle(path, t)) {
        emit notice(u"error"_s, u"This session's title cannot be changed on disk. Open it and rename it there."_s);
        return false;
    }
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions.at(i).path == path) {
            m_sessions[i].title = t;
            emit dataChanged(index(i), index(i), {TitleRole});
        }
    }
    return true;
}

void SessionIndex::exportHtml(const QString &path)
{
    auto tmp = std::make_shared<QTemporaryDir>();
    m_cli->run({u"--export"_s, path},
               [this, tmp](bool ok, const QString &out, const QString &err) {
                   static const QRegularExpression htmlPath(uR"((/\S+\.html))"_s);
                   const auto m = htmlPath.match(out + err);
                   if (ok)
                       emit notice(u"info"_s, m.hasMatch() ? u"Exported to %1"_s.arg(m.captured(1)) : out.trimmed());
                   else
                       emit notice(u"error"_s, u"Export failed: %1"_s.arg(err.trimmed()));
               },
               60'000, QDir::homePath());
}

// ---- SessionFilterModel --------------------------------------------------------------------------

SessionFilterModel::SessionFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    sort(0, Qt::DescendingOrder);
    connect(this, &QAbstractItemModel::rowsInserted, this, &SessionFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &SessionFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &SessionFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &SessionFilterModel::countChanged);
}

void SessionFilterModel::setQuery(const QString &q)
{
    if (q == m_query)
        return;
    beginFilterChange();
    m_query = q;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    emit queryChanged();
    emit countChanged();
}

void SessionFilterModel::setProject(const QString &p)
{
    if (p == m_project)
        return;
    beginFilterChange();
    m_project = p;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    emit projectChanged();
    emit countChanged();
}

void SessionFilterModel::setSortKey(const QString &k)
{
    if (k == m_sortKey)
        return;
    m_sortKey = k;
    sort(0, k == "title"_L1 ? Qt::AscendingOrder : Qt::DescendingOrder);
    invalidate();
    emit sortKeyChanged();
}

bool SessionFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const auto idx = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!m_project.isEmpty() && idx.data(SessionIndex::ProjectRole).toString() != m_project)
        return false;
    if (m_query.trimmed().isEmpty())
        return true;
    // Every word must match somewhere: title, prompt, reply, project or model.
    const auto hay = idx.data(SessionIndex::TitleRole).toString() + u' ' + idx.data(SessionIndex::FirstPromptRole).toString()
                     + u' ' + idx.data(SessionIndex::LastReplyRole).toString() + u' '
                     + idx.data(SessionIndex::CwdRole).toString() + u' ' + idx.data(SessionIndex::ModelRole).toString();
    for (const auto &word : m_query.split(u' ', Qt::SkipEmptyParts)) {
        if (!hay.contains(word, Qt::CaseInsensitive))
            return false;
    }
    return true;
}

bool SessionFilterModel::lessThan(const QModelIndex &l, const QModelIndex &r) const
{
    if (m_sortKey == "title"_L1)
        return QString::localeAwareCompare(l.data(SessionIndex::TitleRole).toString(),
                                           r.data(SessionIndex::TitleRole).toString())
               < 0;
    const int role = m_sortKey == "created"_L1    ? SessionIndex::CreatedRole
                     : m_sortKey == "messages"_L1 ? SessionIndex::MessagesRole
                                                  : SessionIndex::ModifiedRole;
    return l.data(role).toDouble() < r.data(role).toDouble();
}

QVariantMap SessionFilterModel::get(int row) const
{
    QVariantMap out;
    const auto src = mapToSource(index(row, 0));
    if (!src.isValid())
        return out;
    const auto names = sourceModel()->roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        out.insert(QString::fromUtf8(it.value()), src.data(it.key()));
    return out;
}

} // namespace gomp
