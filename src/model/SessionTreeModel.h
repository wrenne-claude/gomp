#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// The branch structure of one session file (entries link by id/parentId), laid out as a timeline:
// rows in append order, each on a lane; the first child continues its parent's lane and each
// further child opens a new lane. Only conversational entries are shown; links skip the rest.
class SessionTreeModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(int laneCount READ laneCount NOTIFY loaded)
    Q_PROPERTY(int branchCount READ branchCount NOTIFY loaded)
    Q_PROPERTY(int count READ rowCount NOTIFY loaded)
public:
    enum Role { EntryIdRole = Qt::UserRole + 1, KindRole, TextRole, TimestampRole, LaneRole, ParentRowRole, ParentLaneRole, ActiveRole, ForkRole };

    explicit SessionTreeModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString path() const { return m_path; }
    void setPath(const QString &path);
    [[nodiscard]] int laneCount() const { return m_lanes; }
    [[nodiscard]] int branchCount() const { return m_branches; }

    struct Node {
        QString id;
        QString parentId;
        QString kind; // user | assistant | compaction | branch | model
        QString text;
        qint64 timestamp = 0;
        int lane = 0;
        int parentRow = -1;
        int parentLane = -1;
        bool active = false;
        bool fork = false; // has more than one child
    };
    // Builds the laid-out nodes from JSONL text; public for tests.
    static QList<Node> layout(const QByteArray &jsonl, int *lanes, int *branches);

signals:
    void pathChanged();
    void loaded();

private:
    QString m_path;
    QList<Node> m_nodes;
    int m_lanes = 0;
    int m_branches = 0;
};

} // namespace gomp
