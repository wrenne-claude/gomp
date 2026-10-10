#include "MockBackend.h"

#include "JsonLineProcess.h"
#include "RpcParser.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace gomp {

namespace {

QJsonObject mockModel(const QString &provider, const QString &id, const QString &name, qint64 ctx, double in,
                      double out, bool images = true)
{
    return {{"provider"_L1, provider},
            {"id"_L1, id},
            {"name"_L1, name},
            {"api"_L1, u"mock"_s},
            {"reasoning"_L1, true},
            {"contextWindow"_L1, ctx},
            {"maxTokens"_L1, 64'000},
            {"input"_L1, images ? QJsonArray{u"text"_s, u"image"_s} : QJsonArray{u"text"_s}},
            {"cost"_L1, QJsonObject{{"input"_L1, in}, {"output"_L1, out}, {"cacheRead"_L1, in / 10}, {"cacheWrite"_L1, in}}},
            {"thinking"_L1, QJsonObject{{"efforts"_L1, QJsonArray{u"minimal"_s, u"low"_s, u"medium"_s, u"high"_s, u"xhigh"_s}}}}};
}

QJsonArray mockModels()
{
    return {mockModel(u"anthropic"_s, u"claude-opus-5-5"_s, u"Claude Opus 5.5"_s, 1'000'000, 5, 25),
            mockModel(u"anthropic"_s, u"claude-sonnet-5-5"_s, u"Claude Sonnet 5.5"_s, 1'000'000, 3, 15),
            mockModel(u"anthropic"_s, u"claude-haiku-5-5"_s, u"Claude Haiku 5.5"_s, 200'000, 1, 5),
            mockModel(u"openai"_s, u"gpt-5.2"_s, u"GPT-5.2"_s, 400'000, 1.25, 10),
            mockModel(u"google"_s, u"gemini-3-pro"_s, u"Gemini 3 Pro"_s, 1'000'000, 2, 12),
            mockModel(u"ollama"_s, u"qwen3-coder:30b"_s, u"Qwen3 Coder 30B"_s, 262'144, 0, 0, false)};
}

QJsonArray mockCommands()
{
    auto cmd = [](const QString &name, const QString &desc, const QString &hint = {}, const QStringList &subs = {},
                  const QString &source = u"builtin"_s) {
        QJsonArray subArr;
        for (const auto &s : subs)
            subArr.append(QJsonObject{{"name"_L1, s}});
        QJsonObject o{{"name"_L1, name}, {"description"_L1, desc}, {"source"_L1, source}};
        if (!hint.isEmpty())
            o.insert("input"_L1, QJsonObject{{"hint"_L1, hint}});
        if (!subArr.isEmpty())
            o.insert("subcommands"_L1, subArr);
        return o;
    };
    return {cmd(u"model"_s, u"Show current model selection"_s),
            cmd(u"effort"_s, u"Set or show reasoning effort"_s, u"<level>"_s,
                {u"off"_s, u"minimal"_s, u"low"_s, u"medium"_s, u"high"_s, u"xhigh"_s}),
            cmd(u"compact"_s, u"Compact the conversation"_s, u"[instructions]"_s, {u"soft"_s, u"remote"_s}),
            cmd(u"handoff"_s, u"Summarize the session into a handoff document and compact in place"_s),
            cmd(u"export"_s, u"Export session to HTML file"_s),
            cmd(u"rename"_s, u"Rename the current session (omit title to generate)"_s, u"[title]"_s),
            cmd(u"context"_s, u"Show context usage"_s),
            cmd(u"tools"_s, u"Show available tools"_s),
            cmd(u"mcp"_s, u"Manage MCP servers"_s, {}, {u"add"_s, u"list"_s, u"remove"_s, u"enable"_s, u"disable"_s}),
            cmd(u"fast"_s, u"Toggle fast mode"_s, {}, {u"on"_s, u"off"_s, u"status"_s}),
            cmd(u"todo"_s, u"Manage todos"_s, {}, {u"edit"_s, u"export"_s, u"import"_s}),
            cmd(u"retry"_s, u"Retry the last failed agent turn"_s),
            cmd(u"review"_s, u"Launch interactive code review"_s, {}, {}, u"custom"_s),
            cmd(u"init"_s, u"Generate AGENTS.md for current codebase"_s, {}, {}, u"file"_s)};
}

QStringList chunkText(const QString &text, int minLen, int maxLen)
{
    QStringList chunks;
    auto *rng = QRandomGenerator::global();
    qsizetype i = 0;
    while (i < text.size()) {
        auto len = static_cast<qsizetype>(rng->bounded(minLen, maxLen + 1));
        // Avoid splitting surrogate pairs.
        if (i + len < text.size() && text.at(i + len).isLowSurrogate())
            ++len;
        chunks << text.mid(i, len);
        i += len;
    }
    return chunks;
}

} // namespace

QJsonArray MockBackend::sampleModels()
{
    QJsonArray out;
    for (const auto &mv : mockModels()) {
        auto m = mv.toObject();
        m.insert("selector"_L1, m.value("provider"_L1).toString() + u'/' + m.value("id"_L1).toString());
        m.insert("kind"_L1, u"chat"_s);
        m.insert("thinking"_L1, m.value("thinking"_L1).toObject().value("efforts"_L1));
        m.insert("pricingStatus"_L1, u"known"_s);
        out.append(m);
    }
    return out;
}

void MockBackend::writeDemoSessions(const QString &dir)
{
    static const QRegularExpression stamp(uR"(\{\{T(?:-(\d+))?\}\})"_s);
    const auto now = QDateTime::currentDateTimeUtc();
    for (const auto &name : QDir(u":/gomp/mock/sessions"_s).entryList({u"*.jsonl"_s})) {
        QFile in(u":/gomp/mock/sessions/"_s + name);
        if (!in.open(QIODevice::ReadOnly))
            continue;
        QString text = QString::fromUtf8(in.readAll());
        qint64 newest = -1;
        QString out;
        qsizetype last = 0;
        auto it = stamp.globalMatch(text);
        while (it.hasNext()) {
            const auto m = it.next();
            const qint64 ago = m.captured(1).toLongLong();
            if (!m.captured(1).isEmpty()) // bare {{T}} marks bookkeeping times, not activity
                newest = newest < 0 ? ago : std::min(newest, ago);
            out += QStringView(text).mid(last, m.capturedStart() - last);
            out += now.addSecs(-ago).toString(Qt::ISODateWithMs);
            last = m.capturedEnd();
        }
        out += QStringView(text).mid(last);
        // The cwd slug directory mirrors omp's layout: one folder per project.
        const auto cwd = QJsonDocument::fromJson(out.section(u'\n', 1, 1).toUtf8()).object().value("cwd"_L1).toString();
        const auto slug = QString(cwd).replace(u'/', u'-');
        QDir().mkpath(dir + u'/' + slug);
        QFile f(dir + u'/' + slug + u'/' + name);
        if (!f.open(QIODevice::WriteOnly))
            continue;
        f.write(out.toUtf8());
        f.close();
        if (f.open(QIODevice::ReadWrite))
            f.setFileTime(QDateTime::currentDateTime().addSecs(-std::max<qint64>(newest, 0)), QFileDevice::FileModificationTime);
    }
}

MockBackend::MockBackend(QObject *parent)
    : AgentBackend(parent)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &MockBackend::pump);
    m_model = mockModels().at(1).toObject();
}

QStringList MockBackend::scenarioNames()
{
    QStringList names;
    for (const auto &f : QDir(u":/gomp/mock"_s).entryList({u"*.json"_s}, QDir::Files))
        names << f.chopped(5);
    return names;
}

QJsonObject MockBackend::loadScenario(const QString &name)
{
    QFile f(u":/gomp/mock/"_s + name + u".json"_s);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

void MockBackend::start(const LaunchOptions &options)
{
    m_running = true;
    m_mode = options.initialMode.isEmpty() ? u"default"_s : options.initialMode;
    QTimer::singleShot(0, this, [this] {
        emit agentEvent(ProcessEvent{ProcessEvent::Kind::Started, 0, {}});
        emitFrame({{"type"_L1, u"ready"_s}, {"protocolVersion"_L1, 1}});
        emitFrame({{"type"_L1, u"available_commands_update"_s}, {"commands"_L1, mockCommands()}});
        emitState();
    });
}

void MockBackend::stop()
{
    m_timer.stop();
    m_queue.clear();
    if (m_running) {
        m_running = false;
        emit agentEvent(ProcessEvent{ProcessEvent::Kind::Exited, 0, {}});
    }
}

void MockBackend::emitFrame(const QJsonObject &frame)
{
    const auto parsed = rpc::parseFrame(frame);
    for (const auto &ev : parsed.events)
        emit agentEvent(ev);
}

void MockBackend::emitState()
{
    const QJsonObject state{
        {"model"_L1, m_model},
        {"thinkingLevel"_L1, m_thinkingLevel},
        {"isStreaming"_L1, m_streaming},
        {"sessionId"_L1, u"01a12665-mock-7c41-9e2d-gomp"_s},
        {"sessionFile"_L1, QDir::homePath() + u"/.omp/agent/sessions/-mock/session.jsonl"_s},
        {"autoCompactionEnabled"_L1, true},
        {"messageCount"_L1, m_seq},
        {"todoPhases"_L1, m_todos},
        {"tokensPerSecond"_L1, m_streaming ? 62.0 : 0.0},
        {"contextUsage"_L1, QJsonObject{{"tokens"_L1, m_contextTokens},
                                        {"contextWindow"_L1, m_model.value("contextWindow"_L1)},
                                        {"percent"_L1, 0}}},
    };
    emit agentEvent(StateEvent{rpc::parseState(state)});
    SessionStats stats;
    stats.outputTokens = m_outputTokens;
    stats.inputTokens = m_contextTokens;
    stats.cost = m_cost;
    emit agentEvent(StatsEvent{stats});
}

QString MockBackend::pickScenario(const QString &prompt) const
{
    const auto p = prompt.toLower();
    if (m_mode == "plan"_L1 || p.contains("plan"_L1))
        return u"plan"_s;
    if (p.contains("parallel"_L1) || p.contains("subagent"_L1) || p.contains("audit"_L1))
        return u"subagents"_s;
    if (p.contains("fix"_L1) || p.contains("edit"_L1) || p.contains("change"_L1))
        return u"edit"_s;
    if (p.contains("crash"_L1))
        return u"crash"_s;
    if (p.contains("fail"_L1) || p.contains("error"_L1) || p.contains("deploy"_L1))
        return u"error"_s;
    return u"explain"_s;
}

void MockBackend::prompt(const QString &text, const QList<ImageAttachment> &images)
{
    if (text.startsWith(u'/')) {
        emitFrame({{"type"_L1, u"command_output"_s},
                   {"text"_L1, u"Mock backend: %1 ran locally."_s.arg(text.section(u' ', 0, 0))}});
        return;
    }
    if (m_streaming) {
        m_queued << text;
        emitFrame({{"type"_L1, u"queue_update"_s},
                   {"queuedMessages"_L1, QJsonObject{{"steering"_L1, QJsonArray{}},
                                                     {"followUp"_L1, QJsonArray::fromStringList(m_queued)}}}});
        return;
    }
    QJsonArray content{QJsonObject{{"type"_L1, u"text"_s}, {"text"_L1, text}}};
    for (qsizetype i = 0; i < images.size(); ++i)
        content.append(QJsonObject{{"type"_L1, u"image"_s}, {"mimeType"_L1, images.at(i).mimeType}, {"data"_L1, u""_s}});
    emitFrame({{"type"_L1, u"message_start"_s},
               {"messageId"_L1, u"mock-u"_s + QString::number(++m_seq)},
               {"message"_L1, QJsonObject{{"role"_L1, u"user"_s},
                                          {"content"_L1, content},
                                          {"timestamp"_L1, QDateTime::currentMSecsSinceEpoch()}}}});
    const auto scenario = pickScenario(text);
    if (!m_titled) {
        m_titled = true;
        const auto title = loadScenario(scenario).value("title"_L1).toString();
        if (!title.isEmpty())
            emitFrame({{"type"_L1, u"session_info_update"_s}, {"title"_L1, title}, {"sessionId"_L1, u"mock"_s}});
    }
    playScenario(scenario, text);
}

void MockBackend::playScenario(const QString &name, const QString &)
{
    const auto scenario = loadScenario(name);
    m_streaming = true;
    m_turnClock.start();
    emitFrame({{"type"_L1, u"agent_start"_s}});
    emitState();
    enqueueOps(scenario.value("ops"_L1).toArray());
    enqueue(0, [this] { finishTurn(u"completed"_s); });
    if (!m_timer.isActive() && !m_paused)
        pump();
}

void MockBackend::enqueue(int delayMs, std::function<void()> action, bool pause)
{
    m_queue.push_back(Step{static_cast<int>(delayMs / m_timeScale), std::move(action), pause});
}

void MockBackend::pump()
{
    if (m_paused || m_queue.empty())
        return;
    auto step = std::move(m_queue.front());
    m_queue.pop_front();
    step.action();
    if (step.pauseAfter) {
        m_paused = true;
        return;
    }
    if (!m_queue.empty())
        m_timer.start(m_queue.front().delayMs);
}

void MockBackend::beginAssistantMessage()
{
    if (!m_messageId.isEmpty())
        return;
    m_messageId = u"mock-a"_s + QString::number(++m_seq);
    m_contentIndex = -1;
    m_lastBlock.clear();
    emitFrame({{"type"_L1, u"message_start"_s},
               {"messageId"_L1, m_messageId},
               {"message"_L1, QJsonObject{{"role"_L1, u"assistant"_s}, {"content"_L1, QJsonArray{}},
                                          {"timestamp"_L1, QDateTime::currentMSecsSinceEpoch()}}}});
}

void MockBackend::endAssistantMessage(const QString &stopReason)
{
    if (m_messageId.isEmpty())
        return;
    const auto outTokens = static_cast<qint64>(180 + QRandomGenerator::global()->bounded(600));
    m_outputTokens += outTokens;
    m_contextTokens += outTokens + 900;
    const double cost = (outTokens * m_model.value("cost"_L1).toObject().value("output"_L1).toDouble()
                         + 900 * m_model.value("cost"_L1).toObject().value("input"_L1).toDouble())
                        / 1e6;
    m_cost += cost;
    emitFrame({{"type"_L1, u"message_end"_s},
               {"messageId"_L1, m_messageId},
               {"message"_L1, QJsonObject{{"role"_L1, u"assistant"_s},
                                          {"stopReason"_L1, stopReason},
                                          {"duration"_L1, m_turnClock.elapsed()},
                                          {"usage"_L1, QJsonObject{{"input"_L1, 900}, {"output"_L1, outTokens},
                                                                   {"cost"_L1, QJsonObject{{"total"_L1, cost}}}}}}}});
    m_messageId.clear();
    emitState();
}

void MockBackend::finishTurn(const QString &status)
{
    endAssistantMessage(status == "aborted"_L1 ? u"aborted"_s : u"stop"_s);
    m_streaming = false;
    emitFrame({{"type"_L1, u"agent_end"_s}, {"messages"_L1, QJsonArray{}}});
    emitFrame({{"type"_L1, u"prompt_result"_s}, {"status"_L1, status}, {"agentInvoked"_L1, true},
               {"sessionSettled"_L1, true}});
    emitState();
    // Deliver queued follow-ups the way omp does: one new turn per queued message.
    if (!m_queued.isEmpty() && status != "aborted"_L1) {
        const auto next = m_queued.takeFirst();
        emitFrame({{"type"_L1, u"queue_update"_s},
                   {"queuedMessages"_L1, QJsonObject{{"steering"_L1, QJsonArray{}},
                                                     {"followUp"_L1, QJsonArray::fromStringList(m_queued)}}}});
        QTimer::singleShot(int(300 / m_timeScale), this, [this, next] { prompt(next); });
    }
}

void MockBackend::enqueueOps(const QJsonArray &ops)
{
    for (const auto &op : ops)
        enqueueOp(op.toObject());
}

void MockBackend::enqueueOp(const QJsonObject &op)
{
    const auto kind = op.value("op"_L1).toString();

    if (kind == "think"_L1 || kind == "say"_L1) {
        const bool thinking = kind == "think"_L1;
        const auto text = op.value("text"_L1).toString();
        const auto blockType = thinking ? u"thinking"_s : u"text"_s;
        enqueue(op.value("delay"_L1).toInt(120), [this, blockType] {
            beginAssistantMessage();
            ++m_contentIndex;
            m_lastBlock = blockType;
            emitFrame({{"type"_L1, u"message_update"_s},
                       {"messageId"_L1, m_messageId},
                       {"assistantMessageEvent"_L1, QJsonObject{{"type"_L1, blockType + u"_start"_s},
                                                                {"contentIndex"_L1, m_contentIndex}}}});
        });
        // ~60 tokens/s for prose, faster for reasoning, like a typical hosted model.
        const int interval = thinking ? 14 : 22;
        for (const auto &chunk : chunkText(text, 3, 9)) {
            enqueue(interval, [this, blockType, chunk] {
                emitFrame({{"type"_L1, u"message_update"_s},
                           {"messageId"_L1, m_messageId},
                           {"assistantMessageEvent"_L1, QJsonObject{{"type"_L1, blockType + u"_delta"_s},
                                                                    {"contentIndex"_L1, m_contentIndex},
                                                                    {"delta"_L1, chunk}}}});
            });
        }
        enqueue(0, [this, blockType, text] {
            emitFrame({{"type"_L1, u"message_update"_s},
                       {"messageId"_L1, m_messageId},
                       {"assistantMessageEvent"_L1, QJsonObject{{"type"_L1, blockType + u"_end"_s},
                                                                {"contentIndex"_L1, m_contentIndex},
                                                                {"content"_L1, text}}}});
        });
        return;
    }

    if (kind == "tool"_L1) {
        const auto id = u"call_"_s + QString::number(QRandomGenerator::global()->generate(), 16);
        const auto name = op.value("name"_L1).toString();
        const auto args = op.value("args"_L1).toObject();
        const auto output = op.value("output"_L1).toString();
        const auto diff = op.value("diff"_L1).toString();
        const bool isError = op.value("error"_L1).toBool();
        const bool needsApproval = op.value("approval"_L1).toBool();
        const int ms = op.value("ms"_L1).toInt(500);
        m_onDeny = op.value("onDeny"_L1).toArray();

        enqueue(op.value("delay"_L1).toInt(160), [this, id, name, args, op] {
            beginAssistantMessage();
            endAssistantMessage(u"toolUse"_s);
            emitFrame({{"type"_L1, u"tool_execution_start"_s}, {"toolCallId"_L1, id}, {"toolName"_L1, name},
                       {"args"_L1, args}, {"intent"_L1, op.value("intent"_L1).toString()}});
        });
        if (needsApproval) {
            enqueue(120, [this, id, name, op] {
                m_pendingToolId = id;
                m_pendingApprovalId = u"ui_"_s + QString::number(++m_seq);
                QString title = u"Allow tool: "_s + name;
                if (op.contains("reason"_L1))
                    title += u"\nReason: "_s + op.value("reason"_L1).toString();
                emitFrame({{"type"_L1, u"extension_ui_request"_s}, {"id"_L1, m_pendingApprovalId},
                           {"method"_L1, u"select"_s}, {"title"_L1, title},
                           {"options"_L1, QJsonArray{u"Approve"_s, u"Deny"_s}}});
            }, true);
        }
        const auto partials = op.value("partials"_L1).toArray();
        for (const auto &pv : partials) {
            const auto partial = pv.toString(); // copy now: the array is a temporary
            enqueue(ms / static_cast<int>(partials.size() + 1), [this, id, name, partial] {
                emitFrame({{"type"_L1, u"tool_execution_update"_s}, {"toolCallId"_L1, id}, {"toolName"_L1, name},
                           {"partialResult"_L1, QJsonObject{{"content"_L1, QJsonArray{QJsonObject{
                                                                {"type"_L1, u"text"_s}, {"text"_L1, partial}}}}}}});
            });
        }
        enqueue(partials.isEmpty() ? ms : ms / static_cast<int>(partials.size() + 1),
                [this, id, name, output, diff, isError] {
                    QJsonObject details;
                    if (!diff.isEmpty())
                        details.insert("diff"_L1, diff);
                    emitFrame({{"type"_L1, u"tool_execution_end"_s}, {"toolCallId"_L1, id}, {"toolName"_L1, name},
                               {"isError"_L1, isError},
                               {"result"_L1, QJsonObject{{"content"_L1, QJsonArray{QJsonObject{{"type"_L1, u"text"_s},
                                                                                              {"text"_L1, output}}}},
                                                         {"details"_L1, details}}}});
                    m_contextTokens += output.size() / 4;
                    emitState();
                });
        return;
    }

    if (kind == "todos"_L1) {
        const auto phases = op.value("phases"_L1).toArray();
        enqueue(op.value("delay"_L1).toInt(80), [this, phases] {
            m_todos = phases;
            emit agentEvent(TodosEvent{rpc::parseTodoPhases(phases)});
        });
        return;
    }

    if (kind == "subagents"_L1) {
        const auto agents = op.value("agents"_L1).toArray();
        const auto parentId = u"call_task_"_s + QString::number(++m_seq);
        enqueue(160, [this, parentId, agents] {
            beginAssistantMessage();
            endAssistantMessage(u"toolUse"_s);
            QJsonArray tasks;
            for (const auto &a : agents)
                tasks.append(QJsonObject{{"agent"_L1, a.toObject().value("agent"_L1)},
                                         {"task"_L1, a.toObject().value("task"_L1)}});
            emitFrame({{"type"_L1, u"tool_execution_start"_s}, {"toolCallId"_L1, parentId}, {"toolName"_L1, u"task"_s},
                       {"args"_L1, QJsonObject{{"tasks"_L1, tasks}}}});
            int index = 0;
            for (const auto &av : agents) {
                const auto a = av.toObject();
                emitFrame({{"type"_L1, u"subagent_lifecycle"_s},
                           {"payload"_L1, QJsonObject{{"id"_L1, a.value("id"_L1)}, {"agent"_L1, a.value("agent"_L1)},
                                                      {"agentSource"_L1, u"bundled"_s},
                                                      {"description"_L1, a.value("task"_L1)},
                                                      {"status"_L1, u"started"_s},
                                                      {"parentToolCallId"_L1, parentId}, {"index"_L1, index++}}}});
            }
        });
        // Interleave progress across agents: tick t advances every agent that still has work.
        int maxTicks = 0;
        for (const auto &av : agents)
            maxTicks = std::max(maxTicks, static_cast<int>(av.toObject().value("tools"_L1).toArray().size()));
        for (int tick = 0; tick <= maxTicks; ++tick) {
            enqueue(op.value("tick"_L1).toInt(650), [this, agents, tick, parentId] {
                int index = 0;
                for (const auto &av : agents) {
                    const auto a = av.toObject();
                    const auto tools = a.value("tools"_L1).toArray();
                    const auto outputs = a.value("output"_L1).toArray();
                    const bool done = tick >= tools.size();
                    QJsonArray recent;
                    for (int i = std::max(0, tick - 3); i < std::min<int>(tick, static_cast<int>(tools.size())); ++i)
                        recent.append(tools.at(i));
                    const auto current = done ? QString{} : tools.at(tick).toString();
                    const QJsonObject progress{
                        {"index"_L1, index}, {"id"_L1, a.value("id"_L1)}, {"agent"_L1, a.value("agent"_L1)},
                        {"modelRole"_L1, a.value("role"_L1).toString(u"task"_s)},
                        {"status"_L1, done ? u"completed"_s : u"running"_s},
                        {"task"_L1, a.value("task"_L1)}, {"currentTool"_L1, current},
                        {"recentTools"_L1, recent}, {"toolCount"_L1, std::min<int>(tick, static_cast<int>(tools.size()))},
                        {"tokens"_L1, 2200 * (tick + 1) + 400 * index}, {"cost"_L1, 0.004 * (tick + 1)},
                        {"durationMs"_L1, 650 * tick}};
                    emitFrame({{"type"_L1, u"subagent_progress"_s},
                               {"payload"_L1, QJsonObject{{"index"_L1, index}, {"agent"_L1, a.value("agent"_L1)},
                                                          {"task"_L1, a.value("task"_L1)},
                                                          {"parentToolCallId"_L1, parentId},
                                                          {"progress"_L1, progress}}}});
                    if (!done) {
                        emitFrame({{"type"_L1, u"subagent_event"_s},
                                   {"payload"_L1, QJsonObject{{"id"_L1, a.value("id"_L1)},
                                                              {"event"_L1, QJsonObject{{"type"_L1, u"tool_execution_start"_s},
                                                                                       {"toolName"_L1, current}}}}}});
                        if (tick < outputs.size()) {
                            emitFrame({{"type"_L1, u"subagent_event"_s},
                                       {"payload"_L1, QJsonObject{
                                                          {"id"_L1, a.value("id"_L1)},
                                                          {"event"_L1, QJsonObject{
                                                                           {"type"_L1, u"message_update"_s},
                                                                           {"assistantMessageEvent"_L1,
                                                                            QJsonObject{{"type"_L1, u"text_delta"_s},
                                                                                        {"delta"_L1, outputs.at(tick).toString() + u"\n"_s}}}}}}}});
                        }
                    } else if (tick == tools.size()) {
                        emitFrame({{"type"_L1, u"subagent_lifecycle"_s},
                                   {"payload"_L1, QJsonObject{{"id"_L1, a.value("id"_L1)}, {"agent"_L1, a.value("agent"_L1)},
                                                              {"agentSource"_L1, u"bundled"_s},
                                                              {"status"_L1, a.value("fails"_L1).toBool() ? u"failed"_s : u"completed"_s},
                                                              {"parentToolCallId"_L1, parentId}, {"index"_L1, index}}}});
                    }
                    ++index;
                }
            });
        }
        enqueue(200, [this, parentId, agents] {
            emitFrame({{"type"_L1, u"tool_execution_end"_s}, {"toolCallId"_L1, parentId}, {"toolName"_L1, u"task"_s},
                       {"result"_L1, QJsonObject{{"content"_L1, QJsonArray{QJsonObject{
                                                                {"type"_L1, u"text"_s},
                                                                {"text"_L1, u"%1 agents finished."_s.arg(agents.size())}}}}}}});
        });
        return;
    }

    if (kind == "plan"_L1) {
        const auto title = op.value("title"_L1).toString();
        const auto markdown = op.value("markdown"_L1).toString();
        m_onApprovePlan = op.value("then"_L1).toArray();
        enqueue(200, [this, title, markdown] {
            endAssistantMessage(u"stop"_s);
            m_pendingPlanId = u"plan_"_s + QString::number(++m_seq);
            emit agentEvent(PlanProposalEvent{m_pendingPlanId, title, markdown, u"local://PLAN.md"_s,
                                         {u"Approve and execute"_s, u"Approve and compact context"_s, u"Refine plan"_s},
                                         {u"Approve and execute"_s, u"Approve and compact context"_s, u"Refine plan"_s}});
        }, true);
        return;
    }

    if (kind == "notice"_L1) {
        const auto level = op.value("level"_L1).toString(u"info"_s);
        const auto text = op.value("text"_L1).toString();
        enqueue(op.value("delay"_L1).toInt(100), [this, level, text] {
            emitFrame({{"type"_L1, u"notice"_s}, {"level"_L1, level}, {"message"_L1, text}, {"source"_L1, u"omp"_s}});
        });
        return;
    }

    if (kind == "error"_L1) {
        const auto text = op.value("text"_L1).toString();
        enqueue(op.value("delay"_L1).toInt(300), [this, text] {
            beginAssistantMessage();
            emitFrame({{"type"_L1, u"message_end"_s}, {"messageId"_L1, m_messageId},
                       {"message"_L1, QJsonObject{{"role"_L1, u"assistant"_s}, {"stopReason"_L1, u"error"_s},
                                                  {"errorMessage"_L1, text}}}});
            m_messageId.clear();
        });
        return;
    }

    // The process dies the way omp does when it cannot restore a session's model: a source
    // excerpt and a stack on stderr, then a non-zero exit.
    if (kind == "crash"_L1) {
        const auto lines = op.value("stderr"_L1).toVariant().toStringList();
        enqueue(op.value("delay"_L1).toInt(300), [this, lines] {
            m_timer.stop();
            m_queue.clear();
            m_streaming = false;
            m_running = false;
            m_stderr = lines;
            emit agentEvent(ProcessEvent{ProcessEvent::Kind::Crashed, 1, JsonLineProcess::summarizeError(lines)});
        });
        return;
    }

    if (kind == "wait"_L1)
        enqueue(op.value("ms"_L1).toInt(300), [] {});
}

void MockBackend::steer(const QString &text)
{
    emitFrame({{"type"_L1, u"queue_update"_s},
               {"queuedMessages"_L1, QJsonObject{{"steering"_L1, QJsonArray{text}}, {"followUp"_L1, QJsonArray{}}}}});
}

void MockBackend::followUp(const QString &text)
{
    prompt(text);
}

void MockBackend::abort()
{
    if (!m_streaming)
        return;
    m_timer.stop();
    m_queue.clear();
    m_paused = false;
    finishTurn(u"aborted"_s);
}

void MockBackend::answerUi(const QString &requestId, const QJsonObject &response)
{
    Q_UNUSED(response)
    if (requestId == m_pendingApprovalId || requestId == m_pendingPlanId) {
        m_paused = false;
        pump();
    }
}

void MockBackend::answerApproval(const QString &requestId, bool approve)
{
    if (requestId != m_pendingApprovalId)
        return;
    m_pendingApprovalId.clear();
    if (!approve) {
        // Drop the pending tool's remaining steps up to its end, emit a denial, then onDeny.
        m_queue.clear();
        emitFrame({{"type"_L1, u"tool_execution_end"_s}, {"toolCallId"_L1, m_pendingToolId}, {"isError"_L1, true},
                   {"result"_L1, QJsonObject{{"content"_L1, QJsonArray{QJsonObject{{"type"_L1, u"text"_s},
                                                                                  {"text"_L1, u"Denied by user"_s}}}}}}});
        enqueueOps(m_onDeny);
        enqueue(0, [this] { finishTurn(u"completed"_s); });
    }
    m_paused = false;
    pump();
}

void MockBackend::answerPlan(const QString &requestId, const QString &optionId)
{
    if (requestId != m_pendingPlanId)
        return;
    m_pendingPlanId.clear();
    m_queue.clear();
    if (optionId.startsWith("Approve"_L1)) {
        m_mode = u"default"_s;
        emit agentEvent(ModeChangedEvent{m_mode});
        if (optionId.contains("compact"_L1)) {
            emit agentEvent(CompactionEvent{true, u"plan approval"_s, {}, 0, 0, false, {}});
            const auto before = m_contextTokens;
            m_contextTokens = m_contextTokens / 5;
            emit agentEvent(CompactionEvent{false, {}, u"Plan approved; earlier exploration summarised."_s, before,
                                       m_contextTokens, false, {}});
        }
        enqueueOps(m_onApprovePlan);
    }
    enqueue(0, [this] { finishTurn(u"completed"_s); });
    m_paused = false;
    pump();
}

void MockBackend::setMode(const QString &mode)
{
    m_mode = mode;
    QTimer::singleShot(static_cast<int>(240 / m_timeScale), this, [this, mode] { emit agentEvent(ModeChangedEvent{mode}); });
}

void MockBackend::call(const QString &command, const QJsonObject &params, Reply reply)
{
    auto ok = [&](const QJsonValue &data) {
        if (reply)
            QTimer::singleShot(static_cast<int>(60 / m_timeScale), this, [reply, data] { reply(true, data, {}); });
    };
    if (command == "get_available_models"_L1) {
        ok(mockModels());
    } else if (command == "set_model"_L1) {
        for (const auto &mv : mockModels()) {
            const auto m = mv.toObject();
            if (m.value("provider"_L1) == params.value("provider"_L1) && m.value("id"_L1) == params.value("modelId"_L1)) {
                m_model = m;
                emitFrame({{"type"_L1, u"model_changed"_s}, {"model"_L1, m}});
                emitState();
                ok(m);
                return;
            }
        }
        if (reply)
            reply(false, {}, u"Unknown model"_s);
    } else if (command == "set_thinking_level"_L1) {
        m_thinkingLevel = params.value("level"_L1).toString();
        emitFrame({{"type"_L1, u"thinking_level_changed"_s}, {"thinkingLevel"_L1, m_thinkingLevel}});
        ok({});
    } else if (command == "get_available_thinking_levels"_L1) {
        ok(QJsonArray{u"off"_s, u"minimal"_s, u"low"_s, u"medium"_s, u"high"_s, u"xhigh"_s});
    } else if (command == "compact"_L1) {
        const auto before = m_contextTokens;
        emit agentEvent(CompactionEvent{true, u"manual"_s, {}, 0, 0, false, {}});
        QTimer::singleShot(static_cast<int>(1400 / m_timeScale), this, [this, before, reply] {
            m_contextTokens = std::max<qint64>(6'000, before / 6);
            const auto summary = u"Kept: goals, open todos, file list for src/adapter and src/model, and the last "
                                 "failing test output. Dropped: exploratory reads and superseded diffs."_s;
            emit agentEvent(CompactionEvent{false, {}, summary, before, m_contextTokens, false, {}});
            emitState();
            if (reply)
                reply(true, QJsonObject{{"summary"_L1, summary}, {"tokensBefore"_L1, before}}, {});
        });
    } else if (command == "set_session_name"_L1) {
        emitFrame({{"type"_L1, u"session_info_update"_s}, {"title"_L1, params.value("name"_L1)}});
        ok({});
    } else if (command == "set_fast_mode"_L1 || command == "set_auto_compaction"_L1) {
        ok(QJsonObject{{"enabled"_L1, params.value("enabled"_L1)}});
    } else if (command == "get_state"_L1) {
        emitState();
        ok({});
    } else if (command == "fork"_L1 || command == "branch"_L1 || command == "switch_session"_L1
               || command == "new_session"_L1 || command == "open_session"_L1) {
        ok(QJsonObject{{"cancelled"_L1, false}});
    } else if (command == "export_html"_L1) {
        ok(params.value("outputPath"_L1).toString(QDir::homePath() + u"/session-export.html"_s));
    } else if (command == "cancel_subagent"_L1) {
        ok(true);
    } else {
        ok({});
    }
}

} // namespace gomp
