#include "AcpParser.h"

#include "RpcParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace gomp::acp {

namespace {

QString chunkText(const QJsonObject &update)
{
    const auto content = update.value("content"_L1).toObject();
    return content.value("type"_L1).toString() == "text"_L1 ? content.value("text"_L1).toString() : QString{};
}

void ensureAssistantMessage(StreamState &st, BlockKind kind, QList<AgentEvent> &out)
{
    if (!st.inAssistantMessage) {
        st.currentMessageId = u"acp-"_s + QString::number(++st.messageSeq);
        st.inAssistantMessage = true;
        st.contentIndex = 0;
        st.lastKind = kind;
        out << MessageStartEvent{st.currentMessageId, u"assistant"_s, {}, 0, 0};
        return;
    }
    if (st.lastKind != kind) {
        ++st.contentIndex;
        st.lastKind = kind;
    }
}

QString toolOutput(const QJsonArray &content, QString *diffOut)
{
    QString text;
    for (const auto &cv : content) {
        const auto c = cv.toObject();
        const auto type = c.value("type"_L1).toString();
        if (type == "content"_L1) {
            const auto inner = c.value("content"_L1).toObject();
            if (inner.value("type"_L1).toString() == "text"_L1) {
                if (!text.isEmpty())
                    text += u'\n';
                text += inner.value("text"_L1).toString();
            }
        } else if (type == "diff"_L1 && diffOut) {
            // Render ACP's old/new pair as a minimal unified diff; the UI diff view parses +/-.
            const auto oldLines = c.value("oldText"_L1).toString().split(u'\n');
            const auto newLines = c.value("newText"_L1).toString().split(u'\n');
            QString diff = u"--- "_s + c.value("path"_L1).toString() + u'\n';
            for (const auto &l : oldLines)
                diff += u'-' + l + u'\n';
            for (const auto &l : newLines)
                diff += u'+' + l + u'\n';
            *diffOut = diff;
        }
    }
    return text;
}

} // namespace

QList<AgentEvent> parseSessionUpdate(const QJsonObject &update, StreamState &st)
{
    QList<AgentEvent> out;
    const auto kind = update.value("sessionUpdate"_L1).toString();

    if (kind == "agent_message_chunk"_L1 || kind == "agent_thought_chunk"_L1) {
        const auto blockKind = kind == "agent_thought_chunk"_L1 ? BlockKind::Thinking : BlockKind::Text;
        const auto text = chunkText(update);
        if (text.isEmpty())
            return out;
        ensureAssistantMessage(st, blockKind, out);
        out << BlockDeltaEvent{st.currentMessageId, st.contentIndex, blockKind, text};
    } else if (kind == "user_message_chunk"_L1) {
        out << finishTurn(st, u"stop"_s);
        out << MessageStartEvent{u"acp-u"_s + QString::number(++st.messageSeq), u"user"_s, chunkText(update), 0, 0};
    } else if (kind == "tool_call"_L1) {
        const auto id = update.value("toolCallId"_L1).toString();
        const auto raw = update.value("rawInput"_L1).toObject();
        auto name = update.value("_meta"_L1).toObject().value("toolName"_L1).toString();
        if (name.isEmpty())
            name = update.value("kind"_L1).toString(u"tool"_s);
        st.openToolCalls.insert(id);
        st.toolNames.insert(id, name);
        // The plan is written to local://<slug>-plan.md (or PLAN.md); keep its content so the
        // proposal shows the full plan rather than the 12-line preview in the elicitation.
        const auto path = raw.value("path"_L1).toString();
        if (path.startsWith("local://"_L1) && path.endsWith(".md"_L1) && raw.contains("content"_L1)) {
            st.latestPlanMarkdown = raw.value("content"_L1).toString();
            st.latestPlanPath = path;
        }
        ensureAssistantMessage(st, BlockKind::ToolCall, out);
        out << ToolStartEvent{id, name, raw, update.value("title"_L1).toString(), st.currentMessageId};
    } else if (kind == "tool_call_update"_L1) {
        const auto id = update.value("toolCallId"_L1).toString();
        const auto status = update.value("status"_L1).toString();
        QString diff;
        const auto text = toolOutput(update.value("content"_L1).toArray(), &diff);
        if (status == "completed"_L1 || status == "failed"_L1) {
            st.openToolCalls.remove(id);
            out << ToolEndEvent{id, st.toolNames.value(id), text, diff, status == "failed"_L1};
        } else if (!text.isEmpty()) {
            out << ToolUpdateEvent{id, text};
        }
    } else if (kind == "plan"_L1) {
        TodoPhase phase{u"Plan"_s, {}};
        for (const auto &ev : update.value("entries"_L1).toArray()) {
            const auto e = ev.toObject();
            phase.tasks << TodoItem{e.value("content"_L1).toString(), e.value("status"_L1).toString(u"pending"_s),
                                    {}, {}};
        }
        out << TodosEvent{phase.tasks.isEmpty() ? QList<TodoPhase>{} : QList<TodoPhase>{phase}};
    } else if (kind == "current_mode_update"_L1) {
        out << ModeChangedEvent{update.value("currentModeId"_L1).toString()};
    } else if (kind == "available_commands_update"_L1) {
        out << CommandsEvent{rpc::parseCommands(update.value("availableCommands"_L1).toArray())};
    } else if (kind == "session_info_update"_L1) {
        if (update.contains("title"_L1))
            out << SessionInfoEvent{{}, update.value("title"_L1).toString()};
    } else if (kind == "usage_update"_L1) {
        const auto cost = update.value("cost"_L1).toObject();
        out << UsageEvent{static_cast<qint64>(update.value("used"_L1).toDouble()),
                          static_cast<qint64>(update.value("size"_L1).toDouble()),
                          cost.isEmpty() ? -1.0 : cost.value("amount"_L1).toDouble()};
    }
    return out;
}

QList<AgentEvent> finishTurn(StreamState &st, const QString &stopReason)
{
    QList<AgentEvent> out;
    if (st.inAssistantMessage) {
        MessageEndEvent end;
        end.messageId = st.currentMessageId;
        end.role = u"assistant"_s;
        end.stopReason = stopReason;
        out << end;
        st.inAssistantMessage = false;
    }
    return out;
}

bool isPlanElicitation(const QJsonObject &params)
{
    return params.value("message"_L1).toString().startsWith("Approve plan "_L1);
}

PlanProposalEvent planProposalFrom(const QString &requestId, const QJsonObject &params, const StreamState &st)
{
    const auto message = params.value("message"_L1).toString();
    static const QRegularExpression titleRe(uR"re(^Approve plan "(.*)" and start implementation\?)re"_s);
    PlanProposalEvent ev;
    ev.requestId = requestId;
    const auto m = titleRe.match(message);
    ev.title = m.hasMatch() ? m.captured(1) : u"Plan"_s;
    const auto preview = message.section(u'\n', 1);
    ev.planMarkdown = st.latestPlanMarkdown.isEmpty() ? preview : st.latestPlanMarkdown;
    ev.planFilePath = st.latestPlanPath;
    const auto value = params.value("requestedSchema"_L1).toObject().value("properties"_L1).toObject()
                           .value("value"_L1).toObject();
    for (const auto &opt : value.value("enum"_L1).toArray()) {
        ev.optionIds << opt.toString();
        ev.optionLabels << opt.toString();
    }
    return ev;
}

bool isAskElicitation(const QJsonObject &params)
{
    static const QRegularExpression question(uR"(^q\d+$)"_s);
    const auto props = params.value("requestedSchema"_L1).toObject().value("properties"_L1).toObject();
    for (auto it = props.begin(); it != props.end(); ++it) {
        if (question.match(it.key()).hasMatch())
            return true;
    }
    return false;
}

namespace {

// omp's `ask` tool over ACP: one form with q0, q1, ... (a string with oneOf, or an array whose
// items have anyOf, for multi-select) plus a free-text q<N>__other per question. Rebuilt into the
// same question list the RPC transport sends, so one UI serves both.
QJsonArray askQuestions(const QJsonObject &props)
{
    QJsonArray questions;
    for (int i = 0;; ++i) {
        const auto key = u"q"_s + QString::number(i);
        if (!props.contains(key) && !props.contains(key + u"__other"_s))
            break;
        const auto q = props.value(key).toObject();
        const bool multi = q.value("type"_L1).toString() == "array"_L1;
        const auto choices = multi ? q.value("items"_L1).toObject().value("anyOf"_L1).toArray() : q.value("oneOf"_L1).toArray();
        QJsonArray options;
        int recommended = -1;
        for (const auto &cv : choices) {
            const auto c = cv.toObject();
            const auto label = c.value("const"_L1).toString(c.value("title"_L1).toString());
            if (label == q.value("default"_L1).toString())
                recommended = int(options.size());
            QJsonObject option{{"label"_L1, label}};
            if (c.contains("description"_L1))
                option.insert("description"_L1, c.value("description"_L1));
            options.append(option);
        }
        QJsonObject question{{"id"_L1, key},
                             {"question"_L1, q.value("title"_L1).toString(props.value(key + u"__other"_s).toObject().value("title"_L1).toString())},
                             {"header"_L1, q.value("description"_L1).toString()},
                             {"multi"_L1, multi},
                             {"options"_L1, options}};
        if (recommended >= 0)
            question.insert("recommended"_L1, recommended);
        questions.append(question);
    }
    return questions;
}

} // namespace

QJsonObject askAnswerContent(const QJsonObject &params, const QJsonArray &answers)
{
    const auto props = params.value("requestedSchema"_L1).toObject().value("properties"_L1).toObject();
    QJsonObject content;
    for (const auto &av : answers) {
        const auto a = av.toObject();
        const auto key = a.value("id"_L1).toString();
        if (!props.contains(key) && !props.contains(key + u"__other"_s))
            continue;
        const auto selected = a.value("selectedOptions"_L1).toArray();
        if (props.value(key).toObject().value("type"_L1).toString() == "array"_L1)
            content.insert(key, selected);
        else if (!selected.isEmpty())
            content.insert(key, selected.first());
        const auto custom = a.value("customInput"_L1).toString().trimmed();
        if (!custom.isEmpty())
            content.insert(key + u"__other"_s, custom);
    }
    return content;
}

UiRequestEvent uiRequestFromElicitation(const QString &requestId, const QJsonObject &params)
{
    UiRequestEvent ev;
    ev.requestId = requestId;
    const auto message = params.value("message"_L1).toString();
    ev.title = message.section(u'\n', 0, 0);
    ev.message = message.section(u'\n', 1);
    if (isAskElicitation(params)) {
        ev.kind = UiRequestEvent::Kind::Ask;
        const auto questions = askQuestions(params.value("requestedSchema"_L1).toObject().value("properties"_L1).toObject());
        // A single question is the title already; repeating it below would read twice.
        if (questions.size() == 1)
            ev.title = QString{};
        ev.raw = QJsonObject{{"questions"_L1, questions}};
        return ev;
    }
    const auto value = params.value("requestedSchema"_L1).toObject().value("properties"_L1).toObject()
                           .value("value"_L1).toObject();
    const auto type = value.value("type"_L1).toString();
    if (type == "boolean"_L1) {
        ev.kind = UiRequestEvent::Kind::Confirm;
    } else if (value.contains("enum"_L1)) {
        ev.kind = UiRequestEvent::Kind::Select;
        for (const auto &opt : value.value("enum"_L1).toArray())
            ev.options << opt.toString();
    } else {
        ev.kind = UiRequestEvent::Kind::Input;
        ev.placeholder = value.value("description"_L1).toString();
    }
    ev.raw = params;
    return ev;
}

ApprovalRequestEvent approvalFromPermission(const QString &requestId, const QJsonObject &params, const StreamState &st)
{
    const auto toolCall = params.value("toolCall"_L1).toObject();
    ApprovalRequestEvent ev;
    ev.requestId = requestId;
    ev.title = toolCall.value("title"_L1).toString();
    ev.toolCallId = toolCall.value("toolCallId"_L1).toString();
    // The permission request carries no tool name; the matching tool_call update does.
    ev.toolName = st.toolNames.value(ev.toolCallId,
                                     toolCall.value("_meta"_L1).toObject().value("toolName"_L1).toString(
                                         toolCall.value("kind"_L1).toString() == "execute"_L1 ? u"bash"_s : QString{}));
    ev.clientGated = true;
    return ev;
}

} // namespace gomp::acp
