#include "RpcParser.h"

#include <QJsonArray>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

namespace gomp::rpc {

namespace {

QString str(const QJsonObject &o, QLatin1StringView key)
{
    return o.value(key).toString();
}

qint64 i64(const QJsonObject &o, QLatin1StringView key)
{
    return static_cast<qint64>(o.value(key).toDouble());
}

QStringList stringList(const QJsonValue &v)
{
    QStringList out;
    const auto arr = v.toArray();
    out.reserve(arr.size());
    for (const auto &item : arr) {
        if (item.isString())
            out << item.toString();
        else if (item.isObject())
            out << item.toObject().value("name"_L1).toString(item.toObject().value("label"_L1).toString());
    }
    return out;
}

// Tool results and partial results carry `content: [{type:"text", text}]`, sometimes nested
// under `result`. Anything else is serialised compactly so the card still shows something.
QString toolOutputText(const QJsonValue &result)
{
    if (result.isString())
        return result.toString();
    const auto obj = result.toObject();
    if (obj.contains("content"_L1))
        return contentText(obj.value("content"_L1));
    if (obj.contains("output"_L1))
        return obj.value("output"_L1).toString();
    if (obj.isEmpty())
        return {};
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

BlockKind blockKindFor(const QString &assistantEventType)
{
    if (assistantEventType.startsWith("thinking"_L1))
        return BlockKind::Thinking;
    if (assistantEventType.startsWith("toolcall"_L1))
        return BlockKind::ToolCall;
    return BlockKind::Text;
}

QString messageIdOf(const QJsonObject &frame)
{
    // Empty when the server omits it; the conversation model then targets the message that is
    // currently streaming (omp streams one message at a time per session).
    return str(frame, "messageId"_L1);
}

void parseSessionEvent(const QJsonObject &f, const QString &type, QList<AgentEvent> &out);

UiRequestEvent::Kind uiKind(const QString &method)
{
    if (method == "confirm"_L1)
        return UiRequestEvent::Kind::Confirm;
    if (method == "input"_L1)
        return UiRequestEvent::Kind::Input;
    if (method == "editor"_L1)
        return UiRequestEvent::Kind::Editor;
    if (method == "ask"_L1)
        return UiRequestEvent::Kind::Ask;
    if (method == "open_url"_L1)
        return UiRequestEvent::Kind::OpenUrl;
    if (method == "notify"_L1)
        return UiRequestEvent::Kind::Notify;
    return UiRequestEvent::Kind::Select;
}

void parseUiRequest(const QJsonObject &f, QList<AgentEvent> &out)
{
    const auto method = str(f, "method"_L1);
    const auto id = str(f, "id"_L1);

    // Fire-and-forget presentation methods that gomp reflects elsewhere or ignores.
    if (method == "setStatus"_L1 || method == "setWidget"_L1 || method == "setTitle"_L1
        || method == "set_editor_text"_L1)
        return;
    if (method == "cancel"_L1) {
        out << UiCancelEvent{str(f, "targetId"_L1)};
        return;
    }
    if (method == "notify"_L1) {
        out << NoticeEvent{f.value("notifyType"_L1).toString(u"info"_s), str(f, "message"_L1), u"extension"_s};
        return;
    }
    if (isApprovalRequest(f)) {
        const auto title = str(f, "title"_L1);
        ApprovalRequestEvent ev;
        ev.requestId = id;
        ev.title = title;
        ev.toolName = toolNameFromApprovalTitle(title);
        for (const auto line : QStringView(title).split(u'\n')) {
            if (line.startsWith(u"Reason: "))
                ev.reason = line.mid(8).toString();
        }
        out << ev;
        return;
    }

    UiRequestEvent ev;
    ev.requestId = id;
    ev.kind = uiKind(method);
    ev.title = str(f, "title"_L1);
    ev.message = str(f, "message"_L1);
    ev.options = stringList(f.value("options"_L1));
    for (const auto &d : f.value("optionDetails"_L1).toArray())
        ev.optionDescriptions << d.toObject().value("description"_L1).toString();
    ev.placeholder = str(f, "placeholder"_L1);
    ev.prefill = str(f, "prefill"_L1);
    ev.url = f.value("launchUrl"_L1).toString(str(f, "url"_L1));
    ev.raw = f;
    out << ev;
}

SubagentProgressEvent parseProgress(const QJsonObject &payload)
{
    const auto p = payload.value("progress"_L1).toObject();
    SubagentProgressEvent ev;
    ev.index = payload.value("index"_L1).toInt(p.value("index"_L1).toInt());
    ev.id = p.value("id"_L1).toString(str(payload, "id"_L1));
    ev.agent = str(payload, "agent"_L1);
    ev.task = payload.value("task"_L1).toString(str(p, "task"_L1));
    ev.parentToolCallId = str(payload, "parentToolCallId"_L1);
    ev.modelRole = str(p, "modelRole"_L1);
    ev.status = str(p, "status"_L1);
    ev.currentTool = str(p, "currentTool"_L1);
    ev.recentTools = stringList(p.value("recentTools"_L1));
    ev.recentOutput = stringList(p.value("recentOutput"_L1));
    ev.toolCount = p.value("toolCount"_L1).toInt();
    ev.tokens = i64(p, "tokens"_L1);
    ev.cost = p.value("cost"_L1).toDouble();
    ev.durationMs = i64(p, "durationMs"_L1);
    return ev;
}

void parseSubagentEvent(const QJsonObject &payload, QList<AgentEvent> &out)
{
    const auto id = str(payload, "id"_L1);
    const auto inner = payload.value("event"_L1).toObject();
    const auto type = str(inner, "type"_L1);
    if (type == "message_update"_L1) {
        const auto ame = inner.value("assistantMessageEvent"_L1).toObject();
        if (str(ame, "type"_L1) == "text_delta"_L1)
            out << SubagentOutputEvent{id, str(ame, "delta"_L1), {}};
    } else if (type == "tool_execution_start"_L1) {
        out << SubagentOutputEvent{id, {}, str(inner, "toolName"_L1)};
    }
}

void parseSessionEvent(const QJsonObject &f, const QString &type, QList<AgentEvent> &out)
{
    if (type == "agent_start"_L1) {
        out << AgentStartEvent{};
    } else if (type == "agent_end"_L1) {
        out << AgentEndEvent{f.value("isTerminal"_L1).toBool(true)};
    } else if (type == "message_start"_L1) {
        const auto msg = f.value("message"_L1).toObject();
        MessageStartEvent ev;
        ev.messageId = messageIdOf(f);
        ev.role = str(msg, "role"_L1);
        if (ev.role == "user"_L1 || ev.role == "custom"_L1 || ev.role == "developer"_L1) {
            ev.text = contentText(msg.value("content"_L1));
            ev.imageCount = contentImageCount(msg.value("content"_L1));
        } else if (ev.role == "compactionSummary"_L1 || ev.role == "branchSummary"_L1) {
            ev.text = str(msg, "summary"_L1);
        }
        ev.timestamp = i64(msg, "timestamp"_L1);
        // Hidden custom messages (display:false) are context plumbing such as plan-mode prompts.
        if (ev.role == "custom"_L1 && !msg.value("display"_L1).toBool(false))
            return;
        out << ev;
    } else if (type == "message_update"_L1) {
        const auto ame = f.value("assistantMessageEvent"_L1).toObject();
        const auto aType = str(ame, "type"_L1);
        if (aType.endsWith("_delta"_L1)) {
            out << BlockDeltaEvent{messageIdOf(f), ame.value("contentIndex"_L1).toInt(), blockKindFor(aType),
                                   str(ame, "delta"_L1)};
        } else if (aType == "text_end"_L1 || aType == "thinking_end"_L1) {
            out << BlockEndEvent{messageIdOf(f), ame.value("contentIndex"_L1).toInt(), blockKindFor(aType),
                                 str(ame, "content"_L1)};
        }
    } else if (type == "message_end"_L1) {
        const auto msg = f.value("message"_L1).toObject();
        MessageEndEvent ev;
        ev.messageId = messageIdOf(f);
        ev.role = str(msg, "role"_L1);
        ev.stopReason = str(msg, "stopReason"_L1);
        ev.errorMessage = str(msg, "errorMessage"_L1);
        const auto usage = msg.value("usage"_L1).toObject();
        ev.inputTokens = i64(usage, "input"_L1);
        ev.outputTokens = i64(usage, "output"_L1);
        ev.cost = usage.value("cost"_L1).toObject().value("total"_L1).toDouble();
        ev.durationMs = i64(msg, "duration"_L1);
        out << ev;
    } else if (type == "tool_execution_start"_L1) {
        out << ToolStartEvent{str(f, "toolCallId"_L1), str(f, "toolName"_L1), f.value("args"_L1).toObject(),
                              str(f, "intent"_L1), {}};
    } else if (type == "tool_execution_update"_L1) {
        out << ToolUpdateEvent{str(f, "toolCallId"_L1), toolOutputText(f.value("partialResult"_L1))};
    } else if (type == "tool_execution_end"_L1) {
        const auto result = f.value("result"_L1).toObject();
        ToolEndEvent ev;
        ev.toolCallId = str(f, "toolCallId"_L1);
        ev.toolName = str(f, "toolName"_L1);
        ev.output = toolOutputText(result);
        ev.diff = result.value("details"_L1).toObject().value("diff"_L1).toString();
        ev.isError = f.value("isError"_L1).toBool(false);
        out << ev;
    } else if (type == "auto_compaction_start"_L1) {
        out << CompactionEvent{true, str(f, "reason"_L1), {}, 0, 0, false, {}};
    } else if (type == "auto_compaction_end"_L1) {
        const auto r = f.value("result"_L1).toObject();
        CompactionEvent ev;
        ev.summary = r.value("shortSummary"_L1).toString(str(r, "summary"_L1));
        ev.tokensBefore = i64(r, "tokensBefore"_L1);
        ev.aborted = f.value("aborted"_L1).toBool();
        ev.errorMessage = str(f, "errorMessage"_L1);
        out << ev;
    } else if (type == "auto_retry_start"_L1) {
        RetryEvent ev;
        ev.started = true;
        ev.attempt = f.value("attempt"_L1).toInt();
        ev.maxAttempts = f.value("maxAttempts"_L1).toInt();
        ev.delayMs = i64(f, "delayMs"_L1);
        ev.errorMessage = str(f, "errorMessage"_L1);
        out << ev;
    } else if (type == "auto_retry_end"_L1) {
        RetryEvent ev;
        ev.attempt = f.value("attempt"_L1).toInt();
        ev.success = f.value("success"_L1).toBool();
        ev.errorMessage = str(f, "finalError"_L1);
        out << ev;
    } else if (type == "model_changed"_L1) {
        out << ModelChangedEvent{parseModel(f.value("model"_L1).toObject())};
    } else if (type == "thinking_level_changed"_L1) {
        out << ThinkingLevelEvent{f.value("thinkingLevel"_L1).toString(str(f, "configured"_L1))};
    } else if (type == "notice"_L1) {
        out << NoticeEvent{str(f, "level"_L1), str(f, "message"_L1), str(f, "source"_L1)};
    } else if (type == "queue_update"_L1) {
        const auto q = f.value("queuedMessages"_L1).toObject();
        const auto src = q.isEmpty() ? f : q;
        out << QueueEvent{stringList(src.value("steering"_L1)), stringList(src.value("followUp"_L1))};
    } else if (type == "todo_reminder"_L1 || type == "todo_auto_clear"_L1) {
        // Reminders carry flat todos; the authoritative phased list arrives via get_state.
    } else if (type == "goal_updated"_L1) {
        const bool active = !f.value("state"_L1).isNull() && f.value("state"_L1).toObject().value("enabled"_L1).toBool();
        out << ModeChangedEvent{active ? u"goal"_s : u"default"_s};
    }
}

} // namespace

QString contentText(const QJsonValue &content)
{
    if (content.isString())
        return content.toString();
    QString out;
    for (const auto &part : content.toArray()) {
        const auto o = part.toObject();
        if (str(o, "type"_L1) == "text"_L1) {
            if (!out.isEmpty())
                out += u'\n';
            out += str(o, "text"_L1);
        }
    }
    return out;
}

int contentImageCount(const QJsonValue &content)
{
    int n = 0;
    for (const auto &part : content.toArray())
        n += part.toObject().value("type"_L1).toString() == "image"_L1 ? 1 : 0;
    return n;
}

bool isApprovalRequest(const QJsonObject &uiRequest)
{
    if (uiRequest.value("method"_L1).toString() != "select"_L1)
        return false;
    const auto opts = stringList(uiRequest.value("options"_L1));
    return opts == QStringList{u"Approve"_s, u"Deny"_s};
}

QString toolNameFromApprovalTitle(const QString &title)
{
    const auto first = QStringView(title).split(u'\n').value(0);
    constexpr auto prefix = u"Allow tool: ";
    if (first.startsWith(prefix))
        return first.mid(QStringView(prefix).size()).trimmed().toString();
    return {};
}

ModelInfo parseModel(const QJsonObject &o)
{
    ModelInfo m;
    m.provider = str(o, "provider"_L1);
    m.id = str(o, "id"_L1);
    m.name = o.value("name"_L1).toString(m.id);
    m.contextWindow = i64(o, "contextWindow"_L1);
    m.maxTokens = i64(o, "maxTokens"_L1);
    m.reasoning = o.value("reasoning"_L1).toBool();
    m.acceptsImages = stringList(o.value("input"_L1)).contains("image"_L1);
    const auto cost = o.value("cost"_L1).toObject();
    m.costInputPerMTok = cost.value("input"_L1).toDouble();
    m.costOutputPerMTok = cost.value("output"_L1).toDouble();
    const auto thinking = o.value("thinking"_L1);
    m.thinkingLevels = thinking.isArray() ? stringList(thinking)
                                          : stringList(thinking.toObject().value("efforts"_L1));
    return m;
}

QList<TodoPhase> parseTodoPhases(const QJsonArray &arr)
{
    QList<TodoPhase> phases;
    for (const auto &pv : arr) {
        const auto p = pv.toObject();
        TodoPhase phase{str(p, "name"_L1), {}};
        for (const auto &tv : p.value("tasks"_L1).toArray()) {
            const auto t = tv.toObject();
            phase.tasks << TodoItem{str(t, "content"_L1), t.value("status"_L1).toString(u"pending"_s),
                                    str(t, "blocker"_L1), str(t, "details"_L1)};
        }
        phases << phase;
    }
    return phases;
}

QList<SlashCommand> parseCommands(const QJsonArray &arr)
{
    QList<SlashCommand> out;
    for (const auto &cv : arr) {
        const auto c = cv.toObject();
        SlashCommand cmd;
        cmd.name = str(c, "name"_L1);
        cmd.aliases = stringList(c.value("aliases"_L1));
        cmd.description = str(c, "description"_L1);
        cmd.inputHint = c.value("input"_L1).toObject().value("hint"_L1).toString();
        cmd.subcommands = stringList(c.value("subcommands"_L1));
        cmd.source = c.value("source"_L1).toString(u"builtin"_s);
        if (!cmd.name.isEmpty())
            out << cmd;
    }
    return out;
}

SessionSnapshot parseState(const QJsonObject &o)
{
    SessionSnapshot s;
    s.model = parseModel(o.value("model"_L1).toObject());
    s.thinkingLevel = str(o, "thinkingLevel"_L1);
    s.sessionId = str(o, "sessionId"_L1);
    s.sessionFile = str(o, "sessionFile"_L1);
    s.sessionName = str(o, "sessionName"_L1);
    s.isStreaming = o.value("isStreaming"_L1).toBool();
    s.isCompacting = o.value("isCompacting"_L1).toBool();
    s.autoCompaction = o.value("autoCompactionEnabled"_L1).toBool();
    s.fastMode = o.value("fastModeEnabled"_L1).toBool();
    s.slowModeSupported = o.value("slowModeSupported"_L1).toBool();
    s.slowMode = o.value("slowModeEnabled"_L1).toBool();
    const auto goal = o.value("goal"_L1).toObject();
    s.goalActive = goal.value("enabled"_L1).toBool();
    s.goalObjective = goal.value("goal"_L1).toObject().value("objective"_L1).toString();
    const auto ctx = o.value("contextUsage"_L1).toObject();
    s.contextTokens = i64(ctx, "tokens"_L1);
    s.contextWindow = ctx.contains("contextWindow"_L1) ? i64(ctx, "contextWindow"_L1) : s.model.contextWindow;
    s.tokensPerSecond = o.value("tokensPerSecond"_L1).toDouble();
    s.messageCount = o.value("messageCount"_L1).toInt();
    const auto q = o.value("queuedMessages"_L1).toObject();
    s.queuedSteering = stringList(q.value("steering"_L1));
    s.queuedFollowUp = stringList(q.value("followUp"_L1));
    s.todoPhases = parseTodoPhases(o.value("todoPhases"_L1).toArray());
    return s;
}

SessionStats parseStats(const QJsonObject &o)
{
    SessionStats s;
    const auto t = o.value("tokens"_L1).toObject();
    s.inputTokens = i64(t, "input"_L1);
    s.outputTokens = i64(t, "output"_L1);
    s.reasoningTokens = i64(t, "reasoning"_L1);
    s.cacheReadTokens = i64(t, "cacheRead"_L1);
    s.cacheWriteTokens = i64(t, "cacheWrite"_L1);
    s.cost = o.value("cost"_L1).toDouble();
    s.toolCalls = o.value("toolCalls"_L1).toInt();
    s.userMessages = o.value("userMessages"_L1).toInt();
    s.assistantMessages = o.value("assistantMessages"_L1).toInt();
    return s;
}

ParsedFrame parseFrame(const QJsonObject &f)
{
    ParsedFrame out;
    const auto type = str(f, "type"_L1);

    if (type == "response"_L1) {
        Response r;
        r.id = str(f, "id"_L1);
        r.command = str(f, "command"_L1);
        r.success = f.value("success"_L1).toBool();
        r.data = f.value("data"_L1);
        r.error = str(f, "error"_L1);
        out.response = r;
        return out;
    }
    if (type == "ready"_L1) {
        out.events << ReadyEvent{f.value("protocolVersion"_L1).toInt(1), u"rpc"_s};
    } else if (type == "prompt_result"_L1) {
        PromptResultEvent ev;
        ev.requestId = str(f, "id"_L1);
        ev.status = str(f, "status"_L1);
        const auto err = f.value("error"_L1).toObject();
        ev.errorMessage = str(err, "message"_L1);
        ev.retryable = err.value("retryable"_L1).toBool();
        ev.settled = f.value("sessionSettled"_L1).toBool(true);
        out.events << ev;
    } else if (type == "session_settled"_L1) {
        out.events << AgentEndEvent{true};
    } else if (type == "available_commands_update"_L1) {
        out.events << CommandsEvent{parseCommands(f.value("commands"_L1).toArray())};
    } else if (type == "extension_ui_request"_L1) {
        parseUiRequest(f, out.events);
    } else if (type == "extension_error"_L1) {
        out.events << NoticeEvent{u"error"_s, str(f, "error"_L1), str(f, "extensionPath"_L1)};
    } else if (type == "command_output"_L1) {
        out.events << CommandOutputEvent{str(f, "text"_L1)};
    } else if (type == "session_info_update"_L1) {
        out.events << SessionInfoEvent{str(f, "sessionId"_L1), str(f, "title"_L1)};
    } else if (type == "config_update"_L1) {
        if (f.contains("model"_L1))
            out.events << ModelChangedEvent{parseModel(f.value("model"_L1).toObject())};
        if (f.contains("thinkingLevel"_L1))
            out.events << ThinkingLevelEvent{str(f, "thinkingLevel"_L1)};
    } else if (type == "subagent_lifecycle"_L1) {
        const auto p = f.value("payload"_L1).toObject();
        out.events << SubagentLifecycleEvent{str(p, "id"_L1), str(p, "agent"_L1), str(p, "description"_L1),
                                             str(p, "status"_L1), str(p, "parentToolCallId"_L1),
                                             p.value("index"_L1).toInt(), p.value("detached"_L1).toBool()};
    } else if (type == "subagent_progress"_L1) {
        out.events << parseProgress(f.value("payload"_L1).toObject());
    } else if (type == "subagent_event"_L1) {
        parseSubagentEvent(f.value("payload"_L1).toObject(), out.events);
    } else if (type == "rpc_frame_error"_L1) {
        out.events << NoticeEvent{u"warning"_s, str(f, "error"_L1), u"rpc"_s};
    } else {
        const auto before = out.events.size();
        parseSessionEvent(f, type, out.events);
        out.unknown = out.events.size() == before && !type.startsWith("turn_"_L1) && !type.startsWith("live_"_L1)
                      && !type.startsWith("btw_"_L1) && !type.startsWith("cache_warming"_L1)
                      && !type.startsWith("todo_"_L1) && type != "agent_end"_L1;
    }
    return out;
}

} // namespace gomp::rpc
