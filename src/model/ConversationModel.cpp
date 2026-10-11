#include "ConversationModel.h"

#include "adapter/RpcParser.h"
#include "util/LineDiff.h"
#include "util/Markdown.h"

#include <QDateTime>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace gomp {

namespace {

constexpr int FlushIntervalMs = 33;
constexpr qsizetype MaxToolOutputChars = 64 * 1024; // the card shows a tail; the full text stays in omp
constexpr qsizetype MaxPreviewLines = 400;

QString str(const QJsonObject &o, QLatin1StringView k)
{
    return o.value(k).toString();
}

QString firstLine(const QString &s, int maxChars = 160)
{
    auto line = s.section(u'\n', 0, 0).trimmed();
    if (line.size() > maxChars)
        line = line.left(maxChars - 1) + u'…';
    return line;
}

QString capTail(const QString &s)
{
    if (s.size() <= MaxToolOutputChars)
        return s;
    return u"… (%1 earlier characters hidden)\n"_s.arg(s.size() - MaxToolOutputChars) + s.right(MaxToolOutputChars);
}

} // namespace

ConversationModel::ConversationModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(FlushIntervalMs);
    connect(&m_flushTimer, &QTimer::timeout, this, &ConversationModel::flush);
    m_clock.start();
}

int ConversationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QHash<int, QByteArray> ConversationModel::roleNames() const
{
    return {
        {KindRole, "kind"},
        {RowIdRole, "rowId"},
        {TextRole, "text"},
        {SegmentsRole, "segments"},
        {StreamingRole, "streaming"},
        {TimestampRole, "timestamp"},
        {DurationRole, "durationMs"},
        {ToolNameRole, "toolName"},
        {ToolTitleRole, "toolTitle"},
        {ToolDetailRole, "toolDetail"},
        {ToolStatusRole, "toolStatus"},
        {ToolArgsRole, "toolArgs"},
        {ToolOutputRole, "toolOutput"},
        {DiffLinesRole, "diffLines"},
        {DiffAddedRole, "diffAdded"},
        {DiffRemovedRole, "diffRemoved"},
        {DiffPathRole, "diffPath"},
        {ApprovalIdRole, "approvalId"},
        {LevelRole, "level"},
        {ImageCountRole, "imageCount"},
        {TokensBeforeRole, "tokensBefore"},
        {TokensAfterRole, "tokensAfter"},
        {SubagentCountRole, "subagentCount"},
        {RetryableRole, "retryable"},
    };
}

void ConversationModel::ensureSegments(Row &row) const
{
    if (!row.segmentsDirty)
        return;
    row.segments = md::toVariant(md::split(row.text));
    row.segmentsDirty = false;
}

QVariant ConversationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    auto &row = const_cast<Row &>(m_rows.at(index.row()));
    switch (role) {
    case KindRole:
        return int(row.kind);
    case RowIdRole:
        return row.rowId;
    case TextRole:
        return row.text;
    case SegmentsRole:
        ensureSegments(row);
        return row.segments;
    case StreamingRole:
        return row.streaming;
    case TimestampRole:
        return row.timestamp;
    case DurationRole:
        return row.durationMs;
    case ToolNameRole:
        return row.toolName;
    case ToolTitleRole:
        return row.toolTitle;
    case ToolDetailRole:
        return row.toolDetail;
    case ToolStatusRole:
        return row.toolStatus;
    case ToolArgsRole:
        return row.toolArgs;
    case ToolOutputRole:
        return row.toolOutput;
    case DiffLinesRole:
        return row.diffLines;
    case DiffAddedRole:
        return row.diffAdded;
    case DiffRemovedRole:
        return row.diffRemoved;
    case DiffPathRole:
        return row.diffPath;
    case ApprovalIdRole:
        return row.approvalId;
    case LevelRole:
        return row.level;
    case ImageCountRole:
        return row.imageCount;
    case TokensBeforeRole:
        return row.tokensBefore;
    case TokensAfterRole:
        return row.tokensAfter;
    case SubagentCountRole:
        return row.subagentCount;
    case RetryableRole:
        return row.retryable;
    default:
        return {};
    }
}

int ConversationModel::appendRow(Row row)
{
    row.rowId = QString::number(m_nextRowId++);
    if (row.timestamp == 0)
        row.timestamp = QDateTime::currentMSecsSinceEpoch();
    const int at = int(m_rows.size());
    beginInsertRows({}, at, at);
    m_rows << std::move(row);
    endInsertRows();
    emit countChanged();
    return at;
}

void ConversationModel::markDirty(int row)
{
    m_dirty.insert(row);
    if (!m_flushTimer.isActive())
        m_flushTimer.start();
}

void ConversationModel::flush()
{
    for (const int r : std::as_const(m_dirty)) {
        if (r < 0 || r >= m_rows.size())
            continue;
        m_rows[r].segmentsDirty = true;
        const auto idx = index(r);
        emit dataChanged(idx, idx);
    }
    m_dirty.clear();
}

int ConversationModel::findBlockRow(const QString &messageId, int contentIndex, Kind kind) const
{
    // Blocks of the streaming message are near the end; search backwards.
    for (auto i = m_rows.size() - 1; i >= 0 && i >= m_rows.size() - 64; --i) {
        const auto &r = m_rows.at(i);
        if (r.messageId == messageId && r.contentIndex == contentIndex && r.kind == kind)
            return int(i);
    }
    return -1;
}

int ConversationModel::findToolRow(const QString &toolCallId) const
{
    for (auto i = m_rows.size() - 1; i >= 0; --i) {
        if (m_rows.at(i).kind == Kind::Tool && m_rows.at(i).toolCallId == toolCallId)
            return int(i);
    }
    return -1;
}

QString ConversationModel::resolveMessageId(const QString &id) const
{
    return id.isEmpty() ? m_currentMessageId : id;
}

QString ConversationModel::toolTitle(const QString &name, const QJsonObject &a)
{
    auto path = str(a, "path"_L1);
    if (path.isEmpty())
        path = str(a, "file_path"_L1);
    if (name == "bash"_L1 || name == "shell"_L1)
        return firstLine(str(a, "command"_L1), 120);
    if (name == "read"_L1 || name == "write"_L1 || name == "edit"_L1 || name == "ast_edit"_L1)
        return path.isEmpty() ? name : path;
    if (name == "grep"_L1 || name == "ast_grep"_L1)
        return str(a, "pattern"_L1);
    if (name == "glob"_L1 || name == "find"_L1)
        return a.value("pattern"_L1).toString(str(a, "query"_L1));
    if (name == "web_search"_L1)
        return str(a, "query"_L1);
    if (name == "fetch"_L1 || name == "browser"_L1)
        return str(a, "url"_L1);
    if (name == "task"_L1) {
        const auto tasks = a.value("tasks"_L1).toArray();
        if (!tasks.isEmpty())
            return tasks.size() == 1 ? firstLine(tasks.at(0).toObject().value("task"_L1).toString(), 100)
                                     : u"%1 parallel tasks"_s.arg(tasks.size());
        return firstLine(str(a, "task"_L1), 100);
    }
    if (name == "todo"_L1 || name == "todo_write"_L1)
        return u"Update the task list"_s;
    for (auto it = a.begin(); it != a.end(); ++it) {
        if (it.value().isString() && !it.key().startsWith(u'_'))
            return firstLine(it.value().toString(), 100);
    }
    return {};
}

QString ConversationModel::toolDetail(const QString &name, const QJsonObject &a)
{
    if (name == "read"_L1) {
        const auto off = a.value("offset"_L1).toInt();
        const auto lim = a.value("limit"_L1).toInt();
        if (off > 0 || lim > 0)
            return lim > 0 ? u"lines %1–%2"_s.arg(off + 1).arg(off + lim) : u"from line %1"_s.arg(off + 1);
    }
    if (name == "grep"_L1 && a.contains("path"_L1))
        return u"in "_s + str(a, "path"_L1);
    return str(a, "i"_L1).isEmpty() ? str(a, "intent"_L1) : str(a, "i"_L1);
}

QVariantList ConversationModel::previewDiff(const QString &name, const QJsonObject &a, QString *path, int *added,
                                            int *removed)
{
    *path = a.value("path"_L1).toString(str(a, "file_path"_L1));
    QList<diff::Line> lines;
    if (name == "edit"_L1) {
        const auto edits = a.value("edits"_L1).toArray();
        auto addPair = [&lines](const QString &oldText, const QString &newText) {
            if (!lines.isEmpty())
                lines << diff::Line{diff::Line::Kind::Gap, 0, 0, {}};
            lines << diff::compute(oldText, newText, 2);
        };
        for (const auto &ev : edits) {
            const auto e = ev.toObject();
            addPair(e.value("old_text"_L1).toString(str(e, "oldText"_L1)), e.value("new_text"_L1).toString(str(e, "newText"_L1)));
        }
        if (edits.isEmpty() && (a.contains("old_string"_L1) || a.contains("oldText"_L1)))
            addPair(a.value("old_string"_L1).toString(str(a, "oldText"_L1)), a.value("new_string"_L1).toString(str(a, "newText"_L1)));
    } else if (name == "write"_L1) {
        int n = 0;
        for (const auto &l : str(a, "content"_L1).split(u'\n')) {
            if (++n > MaxPreviewLines)
                break;
            lines << diff::Line{diff::Line::Kind::Added, 0, n, l};
        }
    }
    const auto st = diff::stats(lines);
    *added = st.added;
    *removed = st.removed;
    return diff::toVariant(lines);
}

void ConversationModel::setToolResult(Row &row, const QString &output, const QString &diffText, bool isError)
{
    row.toolOutput = capTail(output);
    if (row.toolStatus != "denied"_L1)
        row.toolStatus = isError ? u"error"_s : u"done"_s;
    if (row.startedMs > 0)
        row.durationMs = m_clock.elapsed() - row.startedMs;
    if (!diffText.isEmpty()) {
        const auto lines = diff::parseOmpDiff(diffText);
        const auto st = diff::stats(lines);
        row.diffLines = diff::toVariant(lines);
        row.diffAdded = st.added;
        row.diffRemoved = st.removed;
    }
}

void ConversationModel::apply(const AgentEvent &event)
{
    if (const auto *e = std::get_if<MessageStartEvent>(&event)) {
        if (e->role == "assistant"_L1) {
            m_currentMessageId = e->messageId;
            return;
        }
        Row row;
        if (e->role == "user"_L1) {
            row.kind = Kind::User;
        } else if (e->role == "compactionSummary"_L1 || e->role == "branchSummary"_L1) {
            row.kind = Kind::Compaction;
        } else if (e->role == "custom"_L1 || e->role == "developer"_L1) {
            if (e->text.trimmed().isEmpty())
                return;
            row.kind = Kind::Notice;
            row.level = u"info"_s;
        } else {
            return; // toolResult and others are reflected on their tool rows
        }
        row.messageId = e->messageId;
        row.text = e->text;
        row.imageCount = e->imageCount;
        row.timestamp = e->timestamp;
        appendRow(std::move(row));
        return;
    }
    if (const auto *e = std::get_if<BlockDeltaEvent>(&event)) {
        if (e->kind == BlockKind::ToolCall)
            return; // tool rows come from tool_execution_start with parsed arguments
        const auto kind = e->kind == BlockKind::Thinking ? Kind::Thinking : Kind::Assistant;
        const auto msgId = resolveMessageId(e->messageId);
        int r = findBlockRow(msgId, e->contentIndex, kind);
        if (r < 0) {
            Row row;
            row.kind = kind;
            row.messageId = msgId;
            row.contentIndex = e->contentIndex;
            row.streaming = true;
            row.startedMs = m_clock.elapsed();
            r = appendRow(std::move(row));
            ++m_streamingRows;
            m_lastStreamKind = kind;
            emit streamActivityChanged();
        } else if (m_lastStreamKind != kind) {
            m_lastStreamKind = kind;
            emit streamActivityChanged();
        }
        m_rows[r].text += e->delta;
        markDirty(r);
        return;
    }
    if (const auto *e = std::get_if<BlockEndEvent>(&event)) {
        if (e->kind == BlockKind::ToolCall)
            return;
        const auto kind = e->kind == BlockKind::Thinking ? Kind::Thinking : Kind::Assistant;
        const int r = findBlockRow(resolveMessageId(e->messageId), e->contentIndex, kind);
        if (r < 0)
            return;
        auto &row = m_rows[r];
        if (!e->content.isEmpty())
            row.text = e->content;
        if (row.streaming) {
            row.streaming = false;
            row.durationMs = m_clock.elapsed() - row.startedMs;
            m_streamingRows = std::max(0, m_streamingRows - 1);
            emit streamActivityChanged();
        }
        markDirty(r);
        return;
    }
    if (const auto *e = std::get_if<MessageEndEvent>(&event)) {
        if (e->role != "assistant"_L1)
            return;
        const auto msgId = resolveMessageId(e->messageId);
        for (auto i = m_rows.size() - 1; i >= 0 && i >= m_rows.size() - 64; --i) {
            auto &row = m_rows[i];
            if (row.messageId != msgId || !row.streaming)
                continue;
            row.streaming = false;
            row.durationMs = m_clock.elapsed() - row.startedMs;
            m_streamingRows = std::max(0, m_streamingRows - 1);
            markDirty(int(i));
        }
        emit streamActivityChanged();
        if (e->stopReason == "error"_L1)
            addError(e->errorMessage.isEmpty() ? u"The model returned an error."_s : e->errorMessage, true);
        else if (e->stopReason == "length"_L1)
            addNotice(u"warning"_s, u"The reply hit the model's output limit and was cut short."_s);
        return;
    }
    if (const auto *e = std::get_if<ToolStartEvent>(&event)) {
        if (const int existing = findToolRow(e->toolCallId); existing >= 0)
            return;
        Row row;
        row.kind = Kind::Tool;
        row.toolCallId = e->toolCallId;
        row.toolName = e->toolName;
        row.toolTitle = toolTitle(e->toolName, e->args);
        row.toolDetail = e->intent.isEmpty() ? toolDetail(e->toolName, e->args) : e->intent;
        row.toolStatus = u"running"_s;
        row.toolArgs = QString::fromUtf8(QJsonDocument(e->args).toJson(QJsonDocument::Indented));
        row.startedMs = m_clock.elapsed();
        row.diffLines = previewDiff(e->toolName, e->args, &row.diffPath, &row.diffAdded, &row.diffRemoved);
        if (e->toolName == "task"_L1)
            row.subagentCount = std::max<int>(1, int(e->args.value("tasks"_L1).toArray().size()));
        m_runningTools.insert(e->toolCallId);
        appendRow(std::move(row));
        emit toolActivityChanged();
        return;
    }
    if (const auto *e = std::get_if<ToolUpdateEvent>(&event)) {
        const int r = findToolRow(e->toolCallId);
        if (r < 0)
            return;
        m_rows[r].toolOutput = capTail(e->partialOutput);
        markDirty(r);
        return;
    }
    if (const auto *e = std::get_if<ToolEndEvent>(&event)) {
        int r = findToolRow(e->toolCallId);
        if (r < 0) {
            // A tool we never saw start (e.g. resumed mid-run): record its result anyway.
            Row row;
            row.kind = Kind::Tool;
            row.toolCallId = e->toolCallId;
            row.toolName = e->toolName;
            row.toolTitle = e->toolName;
            r = appendRow(std::move(row));
        }
        setToolResult(m_rows[r], e->output, e->diff, e->isError);
        m_runningTools.remove(e->toolCallId);
        markDirty(r);
        emit toolActivityChanged();
        return;
    }
    if (const auto *e = std::get_if<CompactionEvent>(&event)) {
        if (e->started)
            return;
        if (e->aborted || !e->errorMessage.isEmpty()) {
            addNotice(u"warning"_s, e->errorMessage.isEmpty() ? u"Compaction was cancelled."_s
                                                              : u"Compaction failed: "_s + e->errorMessage);
            return;
        }
        Row row;
        row.kind = Kind::Compaction;
        row.text = e->summary;
        row.tokensBefore = e->tokensBefore;
        row.tokensAfter = e->tokensAfter;
        appendRow(std::move(row));
        return;
    }
    if (const auto *e = std::get_if<CommandOutputEvent>(&event)) {
        Row row;
        row.kind = Kind::CommandOutput;
        row.text = e->text;
        appendRow(std::move(row));
        return;
    }
    if (const auto *e = std::get_if<RetryEvent>(&event)) {
        if (e->started)
            addNotice(u"warning"_s, u"Retrying (attempt %1 of %2): %3"_s.arg(e->attempt).arg(e->maxAttempts).arg(e->errorMessage));
        else if (!e->success && !e->errorMessage.isEmpty())
            addError(u"Gave up after %1 attempts: %2"_s.arg(e->attempt).arg(e->errorMessage), true);
        return;
    }
    if (std::holds_alternative<AgentEndEvent>(event)) {
        // Close anything left streaming (aborts can skip block ends).
        for (qsizetype i = 0; i < m_rows.size(); ++i) {
            auto &row = m_rows[i];
            if (row.streaming) {
                row.streaming = false;
                markDirty(int(i));
            }
            if (row.kind == Kind::Tool && (row.toolStatus == "running"_L1 || row.toolStatus == "awaiting"_L1)
                && !m_runningTools.contains(row.toolCallId)) {
                row.toolStatus = u"done"_s;
                markDirty(int(i));
            }
        }
        m_streamingRows = 0;
        emit streamActivityChanged();
        return;
    }
    if (const auto *e = std::get_if<PromptResultEvent>(&event)) {
        if (e->status == "aborted"_L1) {
            for (qsizetype i = 0; i < m_rows.size(); ++i) {
                auto &row = m_rows[i];
                if (row.kind == Kind::Tool && m_runningTools.contains(row.toolCallId)) {
                    row.toolStatus = u"error"_s;
                    row.toolOutput += u"\nStopped."_s;
                    markDirty(int(i));
                }
            }
            m_runningTools.clear();
            emit toolActivityChanged();
            addNotice(u"info"_s, u"Stopped."_s);
        } else if (e->status == "error"_L1 && !e->errorMessage.isEmpty()) {
            addError(e->errorMessage, e->retryable);
        }
    }
}

void ConversationModel::attachApproval(const ApprovalRequestEvent &a)
{
    auto attach = [&](qsizetype i) {
        auto &row = m_rows[i];
        row.approvalId = a.requestId;
        row.toolStatus = u"awaiting"_s;
        markDirty(int(i));
        flush();
    };
    // The exact call when the transport names it.
    if (!a.toolCallId.isEmpty()) {
        if (const int r = findToolRow(a.toolCallId); r >= 0 && m_rows[r].approvalId.isEmpty()) {
            attach(r);
            return;
        }
    }
    // Otherwise the newest call of that tool that is live in this turn. Rows from earlier turns
    // never qualify: a card left "running" by an interrupted turn must not collect new approvals.
    for (auto i = m_rows.size() - 1; i >= 0; --i) {
        const auto &row = m_rows[i];
        if (row.kind == Kind::Tool && row.toolStatus == "running"_L1 && row.approvalId.isEmpty()
            && m_runningTools.contains(row.toolCallId) && (a.toolName.isEmpty() || row.toolName == a.toolName)) {
            attach(i);
            return;
        }
    }
    Row row;
    row.kind = Kind::Tool;
    row.toolName = a.toolName;
    row.toolTitle = a.title.section(u'\n', 1).trimmed().isEmpty() ? a.title : a.title.section(u'\n', 1).trimmed();
    row.toolDetail = a.reason;
    row.toolStatus = u"awaiting"_s;
    row.approvalId = a.requestId;
    row.toolCallId = a.toolCallId;
    row.startedMs = m_clock.elapsed();
    appendRow(std::move(row));
}

void ConversationModel::settleTools(const QString &status)
{
    // Called when a turn or process ends: nothing can still be running or waiting for approval.
    bool changed = false;
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        auto &row = m_rows[i];
        if (row.kind != Kind::Tool || (row.toolStatus != "running"_L1 && row.toolStatus != "awaiting"_L1))
            continue;
        row.toolStatus = status;
        row.approvalId.clear();
        if (row.startedMs > 0 && row.durationMs <= 0)
            row.durationMs = m_clock.elapsed() - row.startedMs;
        markDirty(int(i));
        changed = true;
    }
    if (changed || !m_runningTools.isEmpty()) {
        m_runningTools.clear();
        flush();
        emit toolActivityChanged();
    }
}

void ConversationModel::resolveApproval(const QString &approvalId, bool approved)
{
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        auto &row = m_rows[i];
        if (row.approvalId != approvalId)
            continue;
        row.approvalId.clear();
        row.toolStatus = approved ? u"running"_s : u"denied"_s;
        if (!approved)
            m_runningTools.remove(row.toolCallId);
        markDirty(int(i));
        flush();
        emit toolActivityChanged();
        return;
    }
}

void ConversationModel::addNotice(const QString &level, const QString &text)
{
    Row row;
    row.kind = Kind::Notice;
    row.level = level;
    row.text = text;
    appendRow(std::move(row));
}

void ConversationModel::addError(const QString &text, bool retryable)
{
    Row row;
    row.kind = Kind::Error;
    row.text = text;
    row.retryable = retryable;
    appendRow(std::move(row));
}

void ConversationModel::clear()
{
    beginResetModel();
    m_rows.clear();
    m_dirty.clear();
    m_runningTools.clear();
    m_streamingRows = 0;
    endResetModel();
    emit countChanged();
}

void ConversationModel::loadHistory(const QJsonArray &messages)
{
    beginResetModel();
    m_rows.clear();
    m_dirty.clear();
    m_runningTools.clear();
    m_streamingRows = 0;
    QHash<QString, int> toolRows;
    for (const auto &mv : messages) {
        const auto m = mv.toObject();
        const auto role = str(m, "role"_L1);
        const auto ts = static_cast<qint64>(m.value("timestamp"_L1).toDouble());
        Row base;
        base.timestamp = ts;
        if (role == "user"_L1) {
            Row row = base;
            row.kind = Kind::User;
            row.text = rpc::contentText(m.value("content"_L1));
            row.imageCount = rpc::contentImageCount(m.value("content"_L1));
            row.rowId = QString::number(m_nextRowId++);
            m_rows << row;
        } else if (role == "assistant"_L1) {
            const auto content = m.value("content"_L1).toArray();
            for (qsizetype i = 0; i < content.size(); ++i) {
                const auto c = content.at(i).toObject();
                const auto type = str(c, "type"_L1);
                Row row = base;
                row.rowId = QString::number(m_nextRowId++);
                row.contentIndex = int(i);
                if (type == "text"_L1 && !str(c, "text"_L1).trimmed().isEmpty()) {
                    row.kind = Kind::Assistant;
                    row.text = str(c, "text"_L1);
                    m_rows << row;
                } else if (type == "thinking"_L1 && !str(c, "thinking"_L1).trimmed().isEmpty()) {
                    row.kind = Kind::Thinking;
                    row.text = str(c, "thinking"_L1);
                    m_rows << row;
                } else if (type == "toolCall"_L1) {
                    const auto args = c.value("arguments"_L1).toObject();
                    row.kind = Kind::Tool;
                    row.toolCallId = str(c, "id"_L1);
                    row.toolName = str(c, "name"_L1);
                    row.toolTitle = toolTitle(row.toolName, args);
                    row.toolDetail = toolDetail(row.toolName, args);
                    row.toolArgs = QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Indented));
                    row.toolStatus = u"done"_s;
                    row.diffLines = previewDiff(row.toolName, args, &row.diffPath, &row.diffAdded, &row.diffRemoved);
                    toolRows.insert(row.toolCallId, int(m_rows.size()));
                    m_rows << row;
                }
            }
            if (str(m, "stopReason"_L1) == "error"_L1) {
                Row row = base;
                row.rowId = QString::number(m_nextRowId++);
                row.kind = Kind::Error;
                row.text = str(m, "errorMessage"_L1);
                m_rows << row;
            }
        } else if (role == "toolResult"_L1) {
            const auto r = toolRows.value(str(m, "toolCallId"_L1), -1);
            if (r >= 0) {
                setToolResult(m_rows[r], rpc::contentText(m.value("content"_L1)),
                              m.value("details"_L1).toObject().value("diff"_L1).toString(),
                              m.value("isError"_L1).toBool());
                m_rows[r].durationMs = 0;
            }
        } else if (role == "compactionSummary"_L1) {
            Row row = base;
            row.rowId = QString::number(m_nextRowId++);
            row.kind = Kind::Compaction;
            row.text = m.value("shortSummary"_L1).toString(str(m, "summary"_L1));
            row.tokensBefore = static_cast<qint64>(m.value("tokensBefore"_L1).toDouble());
            row.tokensAfter = static_cast<qint64>(m.value("tokensAfter"_L1).toDouble());
            m_rows << row;
        }
    }
    endResetModel();
    emit countChanged();
    emit toolActivityChanged();
    emit streamActivityChanged();
}

QString ConversationModel::lastAssistantText() const
{
    for (auto i = m_rows.size() - 1; i >= 0; --i) {
        if (m_rows.at(i).kind == Kind::Assistant)
            return m_rows.at(i).text;
    }
    return {};
}

QString ConversationModel::userTextAt(int row) const
{
    return row >= 0 && row < m_rows.size() && m_rows.at(row).kind == Kind::User ? m_rows.at(row).text : QString{};
}

} // namespace gomp
