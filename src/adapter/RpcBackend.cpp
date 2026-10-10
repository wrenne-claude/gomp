#include "RpcBackend.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcRpc, "gomp.rpc")

using namespace Qt::StringLiterals;

namespace gomp {

namespace {
constexpr int DefaultTimeoutMs = 30'000;
constexpr int SweepIntervalMs = 1'000;
// State is refetched after turns end; bursts of events collapse into one get_state.
constexpr int StateDebounceMs = 120;
} // namespace

RpcBackend::RpcBackend(QObject *parent)
    : AgentBackend(parent)
{
    connect(&m_process, &JsonLineProcess::frameReceived, this, &RpcBackend::onFrame);
    connect(&m_process, &JsonLineProcess::started, this,
            [this] { emit agentEvent(ProcessEvent{ProcessEvent::Kind::Started, 0, {}}); });
    connect(&m_process, &JsonLineProcess::failedToStart, this, [this](const QString &msg) {
        emit agentEvent(ProcessEvent{ProcessEvent::Kind::FailedToStart, -1, msg});
    });
    connect(&m_process, &JsonLineProcess::malformedLine, this, [](const QByteArray &line) {
        qCWarning(lcRpc) << "non-JSON line from omp:" << line.left(160);
    });
    connect(&m_process, &JsonLineProcess::exited, this, [this](int code, bool crashed) {
        m_ready = false;
        failAllPending(u"omp exited"_s);
        const auto kind = crashed || code != 0 ? ProcessEvent::Kind::Crashed : ProcessEvent::Kind::Exited;
        const auto tail = m_process.stderrTail();
        emit agentEvent(ProcessEvent{kind, code, tail.isEmpty() ? QString{} : tail.constLast()});
    });

    m_timeoutSweep.setInterval(SweepIntervalMs);
    connect(&m_timeoutSweep, &QTimer::timeout, this, &RpcBackend::sweepTimeouts);
    m_stateDebounce.setSingleShot(true);
    m_stateDebounce.setInterval(StateDebounceMs);
    connect(&m_stateDebounce, &QTimer::timeout, this, &RpcBackend::refreshState);
}

RpcBackend::~RpcBackend() = default;

QStringList RpcBackend::buildArguments(const LaunchOptions &o)
{
    QStringList args{u"--mode"_s, u"rpc-ui"_s};
    if (!o.cwd.isEmpty())
        args << u"--cwd"_s << o.cwd;
    if (!o.resumeSession.isEmpty())
        args << u"--resume"_s << o.resumeSession;
    else if (o.continueLatest)
        args << u"--continue"_s;
    if (!o.model.isEmpty())
        args << u"--model"_s << o.model;
    if (!o.approvalMode.isEmpty())
        args << u"--approval-mode"_s << o.approvalMode;
    if (!o.profile.isEmpty())
        args << u"--profile"_s << o.profile;
    // Without this omp would auto-switch a session started in ~ to a temp dir; gomp always passes
    // an explicit project directory the user chose.
    args << u"--allow-home"_s;
    args << o.extraArgs;
    return args;
}

void RpcBackend::start(const LaunchOptions &options)
{
    m_ready = false;
    m_v2 = false;
    m_chunk = {};
    m_state = {};
    m_process.start(options.ompPath, buildArguments(options), options.cwd, options.env);
    m_timeoutSweep.start();
}

void RpcBackend::stop()
{
    m_timeoutSweep.stop();
    m_process.stop();
}

bool RpcBackend::isRunning() const
{
    return m_process.isRunning();
}

void RpcBackend::call(const QString &command, const QJsonObject &params, Reply reply)
{
    if (!m_process.isRunning()) {
        if (reply)
            reply(false, {}, u"omp is not running"_s);
        return;
    }
    const auto id = u"g"_s + QString::number(m_nextId++);
    QJsonObject frame = params;
    frame.insert("type"_L1, command);
    frame.insert("id"_L1, id);
    m_pending.insert(id, Pending{command, std::move(reply),
                                 QDateTime::currentMSecsSinceEpoch() + timeoutFor(command)});
    sendFrame(frame);
}

void RpcBackend::sendFrame(const QJsonObject &frame)
{
    const auto bytes = QJsonDocument(frame).toJson(QJsonDocument::Compact);
    if (!m_v2 || bytes.size() <= MaxFrameBytes) {
        m_process.sendRaw(bytes + '\n');
        return;
    }
    // v2 splits oversized frames (large image prompts) into base64 chunks of 256 KiB raw bytes.
    static quint64 seq = 0;
    const auto chunkId = u"gomp-%1"_s.arg(++seq);
    const auto count = int((bytes.size() + ChunkBytes - 1) / ChunkBytes);
    for (int i = 0; i < count; ++i) {
        const QJsonObject part{{"type"_L1, u"rpc_chunk"_s},
                               {"chunkId"_L1, chunkId},
                               {"index"_L1, i},
                               {"count"_L1, count},
                               {"byteLength"_L1, double(bytes.size())},
                               {"data"_L1, QString::fromLatin1(bytes.mid(i * ChunkBytes, ChunkBytes).toBase64())}};
        m_process.sendRaw(QJsonDocument(part).toJson(QJsonDocument::Compact) + '\n');
    }
}

QJsonValue RpcBackend::unwrap(const QString &command, const QJsonValue &data)
{
    static const QHash<QString, QString> keys{
        {u"get_available_commands"_s, u"commands"_s},   {u"get_subagents"_s, u"subagents"_s},
        {u"cancel_subagent"_s, u"cancelled"_s},         {u"get_available_models"_s, u"models"_s},
        {u"get_available_thinking_levels"_s, u"levels"_s}, {u"get_login_providers"_s, u"providers"_s},
        {u"get_logout_accounts"_s, u"accounts"_s},      {u"export_html"_s, u"path"_s},
        {u"set_slow_mode"_s, u"enabled"_s},             {u"set_ask_dialog"_s, u"enabled"_s},
        {u"set_todos"_s, u"todoPhases"_s},              {u"set_host_tools"_s, u"toolNames"_s},
        {u"set_host_uri_schemes"_s, u"schemes"_s},      {u"set_subagent_subscription"_s, u"level"_s},
        {u"set_event_filter"_s, u"events"_s},           {u"set_cache_warming"_s, u"mode"_s},
        {u"get_last_assistant_text"_s, u"text"_s},      {u"get_messages"_s, u"messages"_s},
        {u"get_branch_messages"_s, u"messages"_s},      {u"login"_s, u"providerId"_s},
        {u"predict_word"_s, u"suffix"_s},               {u"btw"_s, u"record"_s},
        {u"btw_cancel"_s, u"cancelled"_s},              {u"get_btw_history"_s, u"records"_s},
        {u"live_start"_s, u"voice"_s},                  {u"live_mute"_s, u"muted"_s},
    };
    const auto key = keys.value(command);
    if (key.isEmpty() || !data.isObject() || !data.toObject().contains(key))
        return data;
    return data.toObject().value(key);
}

bool RpcBackend::reassemble(const QJsonObject &c)
{
    const auto id = c.value("chunkId"_L1).toString();
    const int index = c.value("index"_L1).toInt(-1);
    const int count = c.value("count"_L1).toInt(0);
    const auto length = static_cast<qint64>(c.value("byteLength"_L1).toDouble());
    if (id.isEmpty() || count < 2 || index < 0 || index >= count || length <= 0 || length > MaxReassembledBytes) {
        m_chunk = {};
        return false;
    }
    if (index == 0)
        m_chunk = Chunked{id, count, 0, length, {}};
    if (m_chunk.id != id || m_chunk.next != index || m_chunk.count != count) {
        m_chunk = {};
        return false;
    }
    m_chunk.data += QByteArray::fromBase64(c.value("data"_L1).toString().toLatin1());
    m_chunk.next++;
    if (m_chunk.next < count)
        return true;
    const auto data = std::exchange(m_chunk, {}).data;
    if (data.size() != length)
        return false;
    const auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return false;
    onFrame(doc.object());
    return true;
}

int RpcBackend::timeoutFor(const QString &command)
{
    // A prompt's ack arrives once admitted; completion is the prompt_result event, not the reply.
    if (command == "login"_L1)
        return 600'000;
    if (command == "compact"_L1 || command == "handoff"_L1 || command == "bash"_L1 || command == "prompt"_L1
        || command == "abort_and_prompt"_L1)
        return 300'000;
    return DefaultTimeoutMs;
}

QJsonArray RpcBackend::encodeImages(const QList<ImageAttachment> &images)
{
    QJsonArray arr;
    for (const auto &img : images) {
        arr.append(QJsonObject{{"type"_L1, u"image"_s},
                               {"data"_L1, QString::fromLatin1(img.data.toBase64())},
                               {"mimeType"_L1, img.mimeType}});
    }
    return arr;
}

void RpcBackend::prompt(const QString &text, const QList<ImageAttachment> &images)
{
    QJsonObject params{{"message"_L1, text}};
    if (!images.isEmpty())
        params.insert("images"_L1, encodeImages(images));
    // While streaming, omp needs to know how to admit a new prompt; queue it as a follow-up.
    if (m_state.isStreaming)
        params.insert("streamingBehavior"_L1, u"followUp"_s);
    call(u"prompt"_s, params, [this](bool ok, const QJsonValue &, const QString &error) {
        if (!ok)
            emit agentEvent(PromptResultEvent{{}, u"error"_s, error, false, true});
    });
}

void RpcBackend::steer(const QString &text)
{
    call(u"steer"_s, {{"message"_L1, text}});
}

void RpcBackend::followUp(const QString &text)
{
    call(u"follow_up"_s, {{"message"_L1, text}});
}

void RpcBackend::abort()
{
    call(u"abort"_s);
}

void RpcBackend::answerUi(const QString &requestId, const QJsonObject &response)
{
    QJsonObject frame = response;
    frame.insert("type"_L1, u"extension_ui_response"_s);
    frame.insert("id"_L1, requestId);
    sendFrame(frame);
}

void RpcBackend::answerApproval(const QString &requestId, bool approve)
{
    answerUi(requestId, {{"value"_L1, approve ? u"Approve"_s : u"Deny"_s}});
}

void RpcBackend::answerPlan(const QString &requestId, const QString &optionId)
{
    answerUi(requestId, {{"value"_L1, optionId}});
}

void RpcBackend::setMode(const QString &mode)
{
    // RPC has no plan mode (docs/omp-integration.md §3); OmpBackend switches transports instead.
    if (mode == "default"_L1)
        emit agentEvent(ModeChangedEvent{mode});
}

void RpcBackend::onReady()
{
    m_ready = true;
    // v2 lifts the 1 MiB frame limit (long transcripts, big tool outputs, image prompts).
    if (m_supportsV2) {
        call(u"negotiate_protocol"_s, {{"protocolVersion"_L1, 2}}, [this](bool ok, const QJsonValue &data, const QString &) {
            m_v2 = ok && data.toObject().value("protocolVersion"_L1).toInt() == 2;
        });
    }
    // Deltas only: gomp accumulates text itself, which keeps long streams O(delta) on the wire.
    call(u"set_event_filter"_s, {{"events"_L1, QJsonValue::Null}, {"messageUpdates"_L1, u"delta"_s}});
    call(u"set_subagent_subscription"_s, {{"level"_L1, u"events"_s}});
    call(u"set_ask_dialog"_s, {{"enabled"_L1, true}});
    call(u"get_available_commands"_s, {}, [this](bool ok, const QJsonValue &data, const QString &) {
        if (ok)
            emit agentEvent(CommandsEvent{rpc::parseCommands(data.toArray())});
    });
    refreshState();
}

void RpcBackend::refreshState()
{
    call(u"get_state"_s, {}, [this](bool ok, const QJsonValue &data, const QString &) {
        if (!ok)
            return;
        m_state = rpc::parseState(data.toObject());
        emit agentEvent(StateEvent{m_state});
    });
    call(u"get_session_stats"_s, {}, [this](bool ok, const QJsonValue &data, const QString &) {
        if (ok)
            emit agentEvent(StatsEvent{rpc::parseStats(data.toObject())});
    });
}

void RpcBackend::onFrame(const QJsonObject &frame)
{
    if (frame.value("type"_L1).toString() == "rpc_chunk"_L1) {
        if (!reassemble(frame))
            qCWarning(lcRpc) << "dropped a malformed chunked frame";
        return;
    }
    if (frame.value("type"_L1).toString() == "ready"_L1) {
        m_supportsV2 = false;
        for (const auto &v : frame.value("supportedProtocolVersions"_L1).toArray())
            m_supportsV2 = m_supportsV2 || v.toInt() == 2;
    }
    auto parsed = rpc::parseFrame(frame);
    if (parsed.response) {
        const auto &r = *parsed.response;
        auto it = m_pending.find(r.id);
        if (it == m_pending.end()) {
            if (!r.success)
                qCWarning(lcRpc) << "uncorrelated failure" << r.command << r.error;
            return;
        }
        auto pending = std::move(it.value());
        m_pending.erase(it);
        if (!r.success)
            qCInfo(lcRpc) << pending.command << "failed:" << r.error;
        if (pending.reply)
            pending.reply(r.success, unwrap(pending.command, r.data), r.error);
        return;
    }
    if (parsed.unknown)
        qCDebug(lcRpc) << "unhandled frame type" << frame.value("type"_L1).toString();

    for (const auto &ev : std::as_const(parsed.events)) {
        if (std::holds_alternative<ReadyEvent>(ev)) {
            emit agentEvent(ev);
            onReady();
            continue;
        }
        if (const auto *s = std::get_if<StateEvent>(&ev))
            m_state = s->state;
        emit agentEvent(ev);
        if (std::holds_alternative<AgentEndEvent>(ev) || std::holds_alternative<PromptResultEvent>(ev)
            || std::holds_alternative<CompactionEvent>(ev) || std::holds_alternative<ModelChangedEvent>(ev)
            || std::holds_alternative<AgentStartEvent>(ev) || std::holds_alternative<MessageEndEvent>(ev))
            m_stateDebounce.start();
    }
}

void RpcBackend::sweepTimeouts()
{
    const auto now = QDateTime::currentMSecsSinceEpoch();
    QList<Pending> expired;
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (it->deadlineMs < now) {
            expired << std::move(it.value());
            it = m_pending.erase(it);
        } else {
            ++it;
        }
    }
    for (auto &p : expired) {
        qCWarning(lcRpc) << p.command << "timed out";
        if (p.reply)
            p.reply(false, {}, u"omp did not answer %1 in time"_s.arg(p.command));
    }
}

void RpcBackend::failAllPending(const QString &reason)
{
    auto pending = std::exchange(m_pending, {});
    for (auto &p : pending) {
        if (p.reply)
            p.reply(false, {}, reason);
    }
}

} // namespace gomp
