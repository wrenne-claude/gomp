#pragma once

// Typed events emitted by every backend (RPC, ACP, mock). The model layer consumes only these;
// nothing above the adapter ever sees raw omp JSON.

#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

#include <variant>

namespace gomp {

struct ModelInfo {
    QString provider;
    QString id;
    QString name;
    qint64 contextWindow = 0;
    qint64 maxTokens = 0;
    bool reasoning = false;
    bool acceptsImages = false;
    double costInputPerMTok = 0;
    double costOutputPerMTok = 0;
    QStringList thinkingLevels;

    [[nodiscard]] QString selector() const { return provider + QLatin1Char('/') + id; }
    friend bool operator==(const ModelInfo &, const ModelInfo &) = default;
};

struct TodoItem {
    QString content;
    QString status; // pending | in_progress | completed | abandoned | blocked
    QString blocker;
    QString details;
    friend bool operator==(const TodoItem &, const TodoItem &) = default;
};

struct TodoPhase {
    QString name;
    QList<TodoItem> tasks;
    friend bool operator==(const TodoPhase &, const TodoPhase &) = default;
};

struct SlashCommand {
    QString name;
    QStringList aliases;
    QString description;
    QString inputHint;
    QStringList subcommands;
    QString source; // builtin | skill | extension | custom | mcp_prompt | file
};

struct SessionSnapshot {
    ModelInfo model;
    QString thinkingLevel;
    QString sessionId;
    QString sessionFile;
    QString sessionName;
    bool isStreaming = false;
    bool isCompacting = false;
    bool autoCompaction = false;
    bool fastMode = false;
    bool slowModeSupported = false;
    bool slowMode = false;
    bool goalActive = false;
    QString goalObjective;
    qint64 contextTokens = 0;
    qint64 contextWindow = 0;
    double tokensPerSecond = 0;
    int messageCount = 0;
    QStringList queuedSteering;
    QStringList queuedFollowUp;
    QList<TodoPhase> todoPhases;
};

struct SessionStats {
    qint64 inputTokens = 0;
    qint64 outputTokens = 0;
    qint64 reasoningTokens = 0;
    qint64 cacheReadTokens = 0;
    qint64 cacheWriteTokens = 0;
    double cost = 0;
    int toolCalls = 0;
    int userMessages = 0;
    int assistantMessages = 0;
};

// ---- Events --------------------------------------------------------------------------------------

struct ReadyEvent {
    int protocolVersion = 1;
    QString transport; // "rpc" | "acp" | "mock"
};
struct AgentStartEvent {};
struct AgentEndEvent {
    bool terminal = true;
};

struct MessageStartEvent {
    QString messageId;
    QString role; // user | assistant | toolResult | custom | compactionSummary | ...
    QString text; // initial text for user/custom messages
    int imageCount = 0;
    qint64 timestamp = 0;
};

enum class BlockKind { Text, Thinking, ToolCall };

struct BlockDeltaEvent {
    QString messageId;
    int contentIndex = 0;
    BlockKind kind = BlockKind::Text;
    QString delta;
};

struct BlockEndEvent {
    QString messageId;
    int contentIndex = 0;
    BlockKind kind = BlockKind::Text;
    QString content; // final full text for Text/Thinking
};

struct MessageEndEvent {
    QString messageId;
    QString role;
    QString stopReason;   // stop | length | toolUse | error | aborted
    QString errorMessage; // set when stopReason == error
    qint64 inputTokens = 0;
    qint64 outputTokens = 0;
    double cost = 0;
    qint64 durationMs = 0;
};

struct ToolStartEvent {
    QString toolCallId;
    QString toolName;
    QJsonObject args;
    QString intent;
    QString parentMessageId;
};

struct ToolUpdateEvent {
    QString toolCallId;
    QString partialOutput; // accumulated, display-ready
};

struct ToolEndEvent {
    QString toolCallId;
    QString toolName;
    QString output;
    QString diff; // unified-ish diff from edit tool details, when present
    bool isError = false;
};

// A host UI request from omp. Approvals are recognised and surfaced separately.
struct UiRequestEvent {
    enum class Kind { Select, Confirm, Input, Editor, Ask, OpenUrl, Notify };
    QString requestId;
    Kind kind = Kind::Select;
    QString title;
    QString message;
    QStringList options;
    QStringList optionDescriptions;
    QString placeholder;
    QString prefill;
    QString url;
    QJsonObject raw; // ask questions etc. Interpreted by the model layer's typed accessors only.
};

struct ApprovalRequestEvent {
    QString requestId;
    QString toolName;   // best effort, parsed from the title
    QString title;
    QString reason;
    QString toolCallId; // the exact call, when the transport says (ACP does; RPC does not)
    // True when omp asks only because the client protocol requires it (ACP gates bash, edit,
    // delete and move unless tools.approvalMode is explicitly set), not because a policy says so.
    bool clientGated = false;
};

struct UiCancelEvent {
    QString requestId;
};

struct PlanProposalEvent {
    QString requestId;
    QString title;
    QString planMarkdown;
    QString planFilePath;
    QStringList optionIds;
    QStringList optionLabels;
};

struct StateEvent {
    SessionSnapshot state;
};
struct StatsEvent {
    SessionStats stats;
};
struct QueueEvent {
    QStringList steering;
    QStringList followUp;
};
struct TodosEvent {
    QList<TodoPhase> phases;
};
struct CommandsEvent {
    QList<SlashCommand> commands;
};
struct ModelChangedEvent {
    ModelInfo model;
};
struct ThinkingLevelEvent {
    QString level;
};
struct ModeChangedEvent {
    QString mode; // default | plan | goal
};

struct PromptResultEvent {
    QString requestId;
    QString status; // completed | aborted | error
    QString errorMessage;
    bool retryable = false;
    bool settled = true;
};

struct NoticeEvent {
    QString level; // info | warning | error
    QString message;
    QString source;
};

struct CommandOutputEvent {
    QString text;
};

struct CompactionEvent {
    bool started = false;
    QString reason;
    QString summary;
    qint64 tokensBefore = 0;
    qint64 tokensAfter = 0;
    bool aborted = false;
    QString errorMessage;
};

struct RetryEvent {
    bool started = false;
    int attempt = 0;
    int maxAttempts = 0;
    qint64 delayMs = 0;
    QString errorMessage;
    bool success = false;
};

struct SubagentLifecycleEvent {
    QString id;
    QString agent;
    QString description;
    QString status; // started | completed | failed | aborted
    QString parentToolCallId;
    int index = 0;
    bool detached = false;
};

struct SubagentProgressEvent {
    QString id;
    int index = 0;
    QString agent;
    QString task;
    QString modelRole;
    QString status;
    QString currentTool;
    QStringList recentTools;
    QStringList recentOutput;
    int toolCount = 0;
    qint64 tokens = 0;
    double cost = 0;
    qint64 durationMs = 0;
    QString parentToolCallId;
};

struct SubagentOutputEvent {
    QString id;
    QString text;  // assistant text delta from the child session
    QString tool;  // set when the child started a tool
};

struct SessionInfoEvent {
    QString sessionId;
    QString title;
};

struct UsageEvent {
    qint64 contextTokens = 0;
    qint64 contextWindow = 0;
    double cost = -1; // < 0 when not reported
};

struct ProcessEvent {
    enum class Kind { Started, Exited, Crashed, FailedToStart, ProtocolError };
    Kind kind = Kind::Started;
    int exitCode = 0;
    QString message;
};

using AgentEvent = std::variant<ReadyEvent, AgentStartEvent, AgentEndEvent, MessageStartEvent, BlockDeltaEvent,
                                BlockEndEvent, MessageEndEvent, ToolStartEvent, ToolUpdateEvent, ToolEndEvent,
                                UiRequestEvent, ApprovalRequestEvent, UiCancelEvent, PlanProposalEvent, StateEvent,
                                StatsEvent, QueueEvent, TodosEvent, CommandsEvent, ModelChangedEvent,
                                ThinkingLevelEvent, ModeChangedEvent, PromptResultEvent, NoticeEvent,
                                CommandOutputEvent, CompactionEvent, RetryEvent, SubagentLifecycleEvent,
                                SubagentProgressEvent, SubagentOutputEvent, SessionInfoEvent, UsageEvent,
                                ProcessEvent>;

} // namespace gomp

Q_DECLARE_METATYPE(gomp::AgentEvent)
