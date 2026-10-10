#include "FileIndex.h"

#include "util/FuzzyMatch.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace gomp {

namespace {
constexpr int MaxFiles = 50'000;
constexpr int MaxRows = 60;
} // namespace

FileIndex::FileIndex(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(&m_watcher, &QFutureWatcher<QStringList>::finished, this, [this] {
        m_files = m_watcher.result();
        emit indexingChanged();
        refilter();
    });
}

QStringList FileIndex::listFiles(const QString &root)
{
    QProcess git;
    git.setWorkingDirectory(root);
    git.start(u"git"_s, {u"ls-files"_s, u"-co"_s, u"--exclude-standard"_s});
    if (git.waitForFinished(8000) && git.exitStatus() == QProcess::NormalExit && git.exitCode() == 0) {
        QStringList files = QString::fromUtf8(git.readAllStandardOutput()).split(u'\n', Qt::SkipEmptyParts);
        if (files.size() > MaxFiles)
            files.resize(MaxFiles);
        return files;
    }
    static const QSet<QString> skip{u".git"_s,  u"node_modules"_s, u"build"_s, u"dist"_s, u"target"_s,
                                    u".cache"_s, u"__pycache__"_s, u".venv"_s, u"venv"_s, u".omp"_s};
    QStringList files;
    QDir base(root);
    QDirIterator it(root, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext() && files.size() < MaxFiles) {
        const auto path = it.next();
        const auto rel = base.relativeFilePath(path);
        bool skipped = false;
        for (const auto &part : rel.split(u'/')) {
            if (skip.contains(part)) {
                skipped = true;
                break;
            }
        }
        if (!skipped)
            files << rel;
    }
    return files;
}

void FileIndex::setRoot(const QString &root)
{
    if (root == m_root)
        return;
    m_root = root;
    emit rootChanged();
    m_files.clear();
    refilter();
    if (!root.isEmpty()) {
        m_watcher.setFuture(QtConcurrent::run(&FileIndex::listFiles, root));
        emit indexingChanged();
    }
}

void FileIndex::setQuery(const QString &q)
{
    if (q == m_query)
        return;
    m_query = q;
    emit queryChanged();
    refilter();
}

void FileIndex::refilter()
{
    beginResetModel();
    m_visible.clear();
    QList<std::tuple<int, int, QList<int>>> scored;
    for (int i = 0; i < m_files.size(); ++i) {
        auto m = fuzzy::match(m_query, m_files.at(i));
        if (m.matched())
            scored.append({i, m.score, m.positions});
    }
    const auto n = std::min<qsizetype>(MaxRows, scored.size());
    std::partial_sort(scored.begin(), scored.begin() + n, scored.end(), [this](const auto &a, const auto &b) {
        if (std::get<1>(a) != std::get<1>(b))
            return std::get<1>(a) > std::get<1>(b);
        return m_files.at(std::get<0>(a)).size() < m_files.at(std::get<0>(b)).size();
    });
    for (qsizetype i = 0; i < n; ++i)
        m_visible.append({std::get<0>(scored.at(i)), std::get<2>(scored.at(i))});
    endResetModel();
    emit countChanged();
}

int FileIndex::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_visible.size());
}

QHash<int, QByteArray> FileIndex::roleNames() const
{
    return {{PathRole, "path"}, {NameRole, "name"}, {DirRole, "dir"}, {HighlightRole, "highlighted"}};
}

QVariant FileIndex::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_visible.size())
        return {};
    const auto &[fi, positions] = m_visible.at(index.row());
    const auto &path = m_files.at(fi);
    switch (role) {
    case PathRole:
        return path;
    case NameRole:
        return path.section(u'/', -1);
    case DirRole:
        return path.contains(u'/') ? path.section(u'/', 0, -2) : QString{};
    case HighlightRole:
        return fuzzy::highlight(path, positions, m_color);
    default:
        return {};
    }
}

QString FileIndex::pathAt(int row) const
{
    return row >= 0 && row < m_visible.size() ? m_files.at(m_visible.at(row).first) : QString{};
}

} // namespace gomp
