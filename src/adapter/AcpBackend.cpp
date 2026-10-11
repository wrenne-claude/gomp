#include "AcpBackend.h"

#include <QJsonArray>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcAcp, "gomp.acp")

using namespace Qt::StringLiterals;

namespace gomp {

namespace {
constexpr int MethodNotFound = -32601;
}

AcpBackend::AcpBackend(QObject *parent)
    : AgentBackend(parent)
{
    connect(&m_process, &JsonLineProcess::frameReceived, this, &AcpBackend::onFrame);
    connect(&m_process, &JsonLineProcess::started, this, [this] {
        emit agentEvent(ProcessEvent{ProcessEvent::Kind::Started, 0, {}});
        const QJsonObject caps{
            {"fs"_L1, QJsonObject{{"readTextFile"_L1, false}, {"writeTextFile"_L1, false}}},
            {"terminal"_L1, false},
            {"elicitation"_L1, QJsonObject{{"form"_L1, QJsonObject{}}}},
        };
        request(u"initialize"_s, {{"protocolVersion"_L1, 1}, {"clientCapabilities"_L1, caps}},
                [this](const QJsonValue &, const QJsonObject &error) {
                    if (!error.isEmpty()) {
                        emit agentEvent(ProcessEvent{ProcessEvent::Kind::ProtocolError, 0,
                                                error.value("message"_L1).toString()});
                        return;
                    }
                    openSession();
                });
    });
    connect(&m_process, &JsonLineProcess::failedToStart, this, [this](const QString &msg) {
        emit agentEvent(ProcessEvent{ProcessEvent::Kind::FailedToStart, -1, msg});
    });
    connect(&m_process, &JsonLineProcess::exited, this, [this](int code, bool crashed) {
        m_prompting = false;
        auto pending = std::exchange(m_pending, {});
        for (auto &reply : pending) {
            if (reply)
                reply({}, QJsonObject{{"message"_L1, u"omp exited"_s}});
        }
        emit agentEvent(ProcessEvent{crashed || code != 0 ? ProcessEvent::Kind::Crashed : ProcessEvent::Kind::Exited,
                                code, JsonLineProcess::summarizeError(m_process.stderrTail())});
    });
}

void AcpBackend::start(const LaunchOptions &options)
{
    m_options = options;
    m_sessionId.clear();
    m_stream = {};
    QStringList args{u"acp"_s};
    if (!options.profile.isEmpty())
        args << u"--profile"_s << options.profile;
    m_process.start(options.ompPath, args, options.cwd, options.env);
}

void AcpBackend::openSession()
{
    auto afterOpen = [this](const QJsonValue &result, const QJsonObject &error) {
        if (!error.isEmpty()) {
            emit agentEvent(NoticeEvent{u"error"_s, error.value("message"_L1).toString(), u"acp"_s});
            return;
        }
        const auto obj = result.toObject();
        if (obj.contains("sessionId"_L1))
            m_sessionId = obj.value("sessionId"_L1).toString();
        emit agentEvent(ReadyEvent{1, u"acp"_s});
        if (!m_options.initialMode.isEmpty())
            setMode(m_options.initialMode);
    };
    const QJsonObject base{{"cwd"_L1, m_options.cwd}, {"mcpServers"_L1, QJsonArray{}}};
    if (!m_options.resumeSession.isEmpty()) {
        // `session/resume` attaches without replaying history; gomp already shows the transcript.
        m_sessionId = m_options.resumeSession;
        QJsonObject params = base;
        params.insert("sessionId"_L1, m_sessionId);
        request(u"session/resume"_s, params, [this, base, afterOpen](const QJsonValue &r, const QJsonObject &e) {
            if (e.isEmpty()) {
                afterOpen(r, e);
                return;
            }
            qCInfo(lcAcp) << "resume failed, starting a new plan session:" << e.value("message"_L1).toString();
            request(u"session/new"_s, base, afterOpen);
        });
    } else {
        request(u"session/new"_s, base, afterOpen);
    }
}

void AcpBackend::stop()
{
    m_process.stop();
}

bool AcpBackend::isRunning() const
{
    return m_process.isRunning();
}

void AcpBackend::request(const QString &method, const QJsonObject &params, RpcReply reply)
{
    const auto id = m_nextId++;
    if (reply)
        m_pending.insert(id, std::move(reply));
    m_process.send({{"jsonrpc"_L1, u"2.0"_s}, {"id"_L1, id}, {"method"_L1, method}, {"params"_L1, params}});
}

void AcpBackend::notify(const QString &method, const QJsonObject &params)
{
    m_process.send({{"jsonrpc"_L1, u"2.0"_s}, {"method"_L1, method}, {"params"_L1, params}});
}

void AcpBackend::respond(const QJsonValue &id, const QJsonObject &result)
{
    m_process.send({{"jsonrpc"_L1, u"2.0"_s}, {"id"_L1, id}, {"result"_L1, result}});
}

void AcpBackend::respondError(const QJsonValue &id, int code, const QString &message)
{
    m_process.send({{"jsonrpc"_L1, u"2.0"_s},
                    {"id"_L1, id},
                    {"error"_L1, QJsonObject{{"code"_L1, code}, {"message"_L1, message}}}});
}

void AcpBackend::prompt(const QString &text, const QList<ImageAttachment> &images)
{
    QJsonArray blocks{QJsonObject{{"type"_L1, u"text"_s}, {"text"_L1, text}}};
    for (const auto &img : images) {
        blocks.append(QJsonObject{{"type"_L1, u"image"_s},
                                  {"mimeType"_L1, img.mimeType},
                                  {"data"_L1, QString::fromLatin1(img.data.toBase64())}});
    }
    m_prompting = true;
    emit agentEvent(MessageStartEvent{u"acp-u"_s + QString::number(++m_stream.messageSeq), u"user"_s, text,
                                 static_cast<int>(images.size()), 0});
    emit agentEvent(AgentStartEvent{});
    request(u"session/prompt"_s, {{"sessionId"_L1, m_sessionId}, {"prompt"_L1, blocks}},
            [this](const QJsonValue &result, const QJsonObject &error) {
                m_prompting = false;
                const auto stop = result.toObject().value("stopReason"_L1).toString(u"end_turn"_s);
                for (const auto &ev : acp::finishTurn(m_stream, stop == "cancelled"_L1 ? u"aborted"_s : u"stop"_s))
                    emit agentEvent(ev);
                PromptResultEvent done;
                done.status = !error.isEmpty() ? u"error"_s : stop == "cancelled"_L1 ? u"aborted"_s : u"completed"_s;
                done.errorMessage = error.value("message"_L1).toString();
                emit agentEvent(done);
                emit agentEvent(AgentEndEvent{true});
            });
}

void AcpBackend::steer(const QString &text)
{
    // ACP has no steering queue; deliver as the next prompt.
    followUp(text);
}

void AcpBackend::followUp(const QString &text)
{
    if (!m_prompting) {
        prompt(text);
        return;
    }
    emit agentEvent(NoticeEvent{u"info"_s, u"Plan mode runs one prompt at a time. Send this after the current turn ends."_s,
                           u"gomp"_s});
}

void AcpBackend::abort()
{
    notify(u"session/cancel"_s, {{"sessionId"_L1, m_sessionId}});
}

void AcpBackend::answerUi(const QString &requestId, const QJsonObject &response)
{
    const auto it = m_elicitations.constFind(requestId);
    if (it == m_elicitations.cend())
        return;
    const auto rpcId = it.value().rpcId;
    const auto params = it.value().params;
    m_elicitations.erase(it);
    if (response.value("cancelled"_L1).toBool()) {
        respond(rpcId, {{"action"_L1, u"cancel"_s}});
        return;
    }
    if (response.contains("answers"_L1)) {
        respond(rpcId, {{"action"_L1, u"accept"_s},
                        {"content"_L1, acp::askAnswerContent(params, response.value("answers"_L1).toArray())}});
        return;
    }
    QJsonValue value = response.contains("confirmed"_L1) ? response.value("confirmed"_L1) : response.value("value"_L1);
    respond(rpcId, {{"action"_L1, u"accept"_s}, {"content"_L1, QJsonObject{{"value"_L1, value}}}});
}

void AcpBackend::answerApproval(const QString &requestId, bool approve)
{
    const auto it = m_permissions.constFind(requestId);
    if (it == m_permissions.cend())
        return;
    const auto req = it.value();
    m_permissions.erase(it);
    // Prefer one-shot options; gomp manages persistent policy in Settings › Permissions.
    const QStringList wanted = approve ? QStringList{u"allow_once"_s, u"allow_always"_s}
                                       : QStringList{u"reject_once"_s, u"reject_always"_s};
    for (const auto &kind : wanted) {
        for (const auto &ov : req.options) {
            const auto o = ov.toObject();
            if (o.value("kind"_L1).toString() == kind) {
                respond(req.rpcId, {{"outcome"_L1, QJsonObject{{"outcome"_L1, u"selected"_s},
                                                               {"optionId"_L1, o.value("optionId"_L1)}}}});
                return;
            }
        }
    }
    respond(req.rpcId, {{"outcome"_L1, QJsonObject{{"outcome"_L1, u"cancelled"_s}}}});
}

void AcpBackend::answerPlan(const QString &requestId, const QString &optionId)
{
    answerUi(requestId, {{"value"_L1, optionId}});
}

void AcpBackend::setMode(const QString &mode)
{
    request(u"session/set_mode"_s, {{"sessionId"_L1, m_sessionId}, {"modeId"_L1, mode}},
            [this, mode](const QJsonValue &, const QJsonObject &error) {
                if (!error.isEmpty())
                    emit agentEvent(NoticeEvent{u"error"_s, error.value("message"_L1).toString(), u"acp"_s});
            });
}

void AcpBackend::call(const QString &command, const QJsonObject &params, Reply reply)
{
    // A small subset maps onto ACP; the rest waits until the workspace is back on RPC.
    if (command == "set_model"_L1) {
        const auto value = params.value("provider"_L1).toString() + u'/' + params.value("modelId"_L1).toString();
        request(u"session/set_config_option"_s,
                {{"sessionId"_L1, m_sessionId}, {"configId"_L1, u"model"_s}, {"value"_L1, value}},
                [reply](const QJsonValue &r, const QJsonObject &e) {
                    if (reply)
                        reply(e.isEmpty(), r, e.value("message"_L1).toString());
                });
        return;
    }
    if (reply)
        reply(false, {}, u"Not available while planning. Approve or leave plan mode first."_s);
}

void AcpBackend::onFrame(const QJsonObject &frame)
{
    if (frame.contains("method"_L1)) {
        if (frame.contains("id"_L1)) {
            onServerRequest(frame);
            return;
        }
        if (frame.value("method"_L1).toString() == "session/update"_L1) {
            const auto update = frame.value("params"_L1).toObject().value("update"_L1).toObject();
            for (const auto &ev : acp::parseSessionUpdate(update, m_stream))
                emit agentEvent(ev);
        }
        return;
    }
    const auto id = static_cast<qint64>(frame.value("id"_L1).toDouble());
    auto reply = m_pending.take(id);
    if (reply)
        reply(frame.value("result"_L1), frame.value("error"_L1).toObject());
}

void AcpBackend::onServerRequest(const QJsonObject &frame)
{
    const auto method = frame.value("method"_L1).toString();
    const auto params = frame.value("params"_L1).toObject();
    const auto rpcId = frame.value("id"_L1);
    const auto key = u"acp:"_s + QString::number(rpcId.toDouble(), 'f', 0) + rpcId.toString();

    if (method == "session/request_permission"_L1) {
        m_permissions.insert(key, PermissionRequest{rpcId, params.value("options"_L1).toArray()});
        emit agentEvent(acp::approvalFromPermission(key, params, m_stream));
    } else if (method == "elicitation/create"_L1) {
        m_elicitations.insert(key, Elicitation{rpcId, params});
        if (acp::isPlanElicitation(params))
            emit agentEvent(acp::planProposalFrom(key, params, m_stream));
        else
            emit agentEvent(acp::uiRequestFromElicitation(key, params));
    } else {
        // gomp declares no fs/terminal capabilities, so omp should not ask; refuse cleanly if it does.
        respondError(rpcId, MethodNotFound, u"gomp does not provide "_s + method);
    }
}

} // namespace gomp
