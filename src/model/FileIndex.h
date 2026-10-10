#pragma once

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// Project files for the composer's @file picker. Lists tracked and untracked-but-not-ignored
// files via git when the directory is a repository, otherwise walks the tree skipping common
// build and dependency folders. Results are fuzzy-ranked; only the top matches become rows.
class FileIndex : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString root READ root WRITE setRoot NOTIFY rootChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool indexing READ indexing NOTIFY indexingChanged)
    Q_PROPERTY(QString highlightColor READ highlightColor WRITE setHighlightColor NOTIFY queryChanged)
public:
    enum Role { PathRole = Qt::UserRole + 1, NameRole, DirRole, HighlightRole };

    explicit FileIndex(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString root() const { return m_root; }
    void setRoot(const QString &root);
    [[nodiscard]] QString query() const { return m_query; }
    void setQuery(const QString &q);
    [[nodiscard]] bool indexing() const { return m_watcher.isRunning(); }
    [[nodiscard]] QString highlightColor() const { return m_color; }
    void setHighlightColor(const QString &c) { m_color = c; }
    Q_INVOKABLE QString pathAt(int row) const;

    static QStringList listFiles(const QString &root);

signals:
    void rootChanged();
    void queryChanged();
    void countChanged();
    void indexingChanged();

private:
    void refilter();
    QString m_root;
    QString m_query;
    QString m_color = QStringLiteral("#FFB547");
    QStringList m_files;
    QList<QPair<int, QList<int>>> m_visible;
    QFutureWatcher<QStringList> m_watcher;
};

} // namespace gomp
