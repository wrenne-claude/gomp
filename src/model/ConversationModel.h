#pragma once

#include "adapter/AgentEvents.h"

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QSet>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// The transcript of one workspace as a flat list of rows: user prompts, assistant prose,
// reasoning, tool calls (with approval and diff state), notices, errors, compactions and
// slash-command output. Streaming deltas are applied immediately but announced to views at most
// every 33 ms, and markdown is segmented only on announce, so long streams stay smooth.
class ConversationModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by Workspace")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum class Kind { User, Assistant, Thinking, Tool, Notice, Error, Compaction, CommandOutput };
    Q_ENUM(Kind)

    enum Role {
        KindRole = Qt::UserRole + 1,
        RowIdRole,
        TextRole,
        SegmentsRole,
        StreamingRole,
        TimestampRole,
        DurationRole,
        ToolNameRole,
        ToolTitleRole,
        ToolDetailRole,
        ToolStatusRole,
        ToolArgsRole,
        ToolOutputRole,
        DiffLinesRole,
        DiffAddedRole,
        DiffRemovedRole,
        DiffPathRole,
        ApprovalIdRole,
        LevelRole,
        ImageCountRole,
        TokensBeforeRole,
        TokensAfterRole,
        SubagentCountRole,
        RetryableRole,
    };

    explicit ConversationModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // Event intake (called by Workspace).
    void apply(const AgentEvent &event);
    void attachApproval(const ApprovalRequestEvent &approval);
    void resolveApproval(const QString &approvalId, bool approved);
    void addNotice(const QString &level, const QString &text);
    void addError(const QString &text, bool retryable);
    void loadHistory(const QJsonArray &messages);
    void clear();

    [[nodiscard]] int runningToolCount() const { return int(m_runningTools.size()); }
    [[nodiscard]] bool isThinking() const { return m_lastStreamKind == Kind::Thinking && m_streamingRows > 0; }
    [[nodiscard]] bool isStreamingText() const { return m_lastStreamKind == Kind::Assistant && m_streamingRows > 0; }

    Q_INVOKABLE QString lastAssistantText() const;
    Q_INVOKABLE QString userTextAt(int row) const;

    // Tool presentation helpers; public for tests.
    static QString toolTitle(const QString &name, const QJsonObject &args);
    static QString toolDetail(const QString &name, const QJsonObject &args);
    static QVariantList previewDiff(const QString &name, const QJsonObject &args, QString *path, int *added, int *removed);

signals:
    void countChanged();
    void toolActivityChanged();
    void streamActivityChanged();

private:
    struct Row {
        Kind kind = Kind::Notice;
        QString rowId;
        QString messageId;
        int contentIndex = -1;
        QString text;
        QVariantList segments;
        bool segmentsDirty = true;
        bool streaming = false;
        qint64 timestamp = 0;
        qint64 durationMs = 0;
        qint64 startedMs = 0;
        QString toolCallId;
        QString toolName;
        QString toolTitle;
        QString toolDetail;
        QString toolStatus; // running | awaiting | done | error | denied
        QString toolArgs;
        QString toolOutput;
        QVariantList diffLines;
        int diffAdded = 0;
        int diffRemoved = 0;
        QString diffPath;
        QString approvalId;
        QString level;
        int imageCount = 0;
        qint64 tokensBefore = 0;
        qint64 tokensAfter = 0;
        int subagentCount = 0;
        bool retryable = false;
    };

    int appendRow(Row row);
    int findBlockRow(const QString &messageId, int contentIndex, Kind kind) const;
    int findToolRow(const QString &toolCallId) const;
    void markDirty(int row);
    void flush();
    void ensureSegments(Row &row) const;
    QString resolveMessageId(const QString &id) const;
    void setToolResult(Row &row, const QString &output, const QString &diff, bool isError);

    QList<Row> m_rows;
    QSet<int> m_dirty;
    QTimer m_flushTimer;
    QSet<QString> m_runningTools;
    QString m_currentMessageId;
    Kind m_lastStreamKind = Kind::Assistant;
    int m_streamingRows = 0;
    quint64 m_nextRowId = 1;
    QElapsedTimer m_clock;
};

} // namespace gomp
