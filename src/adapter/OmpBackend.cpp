#include "OmpBackend.h"

#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(lcOmp, "gomp.backend")

using namespace Qt::StringLiterals;

namespace gomp {

OmpBackend::OmpBackend(QObject *parent)
    : AgentBackend(parent)
{
    connect(&m_rpc, &AgentBackend::agentEvent, this, [this](const AgentEvent &ev) { forward(Transport::Rpc, ev); });
    connect(&m_acp, &AgentBackend::agentEvent, this, [this](const AgentEvent &ev) { forward(Transport::Acp, ev); });
}

AgentBackend *OmpBackend::active() const
{
    return m_active == Transport::Rpc ? static_cast<AgentBackend *>(const_cast<RpcBackend *>(&m_rpc))
                                      : static_cast<AgentBackend *>(const_cast<AcpBackend *>(&m_acp));
}

void OmpBackend::start(const LaunchOptions &options)
{
    m_options = options;
    m_switching = false;
    m_returnAfterTurn = false;
    m_retriedWithModel = false;
    m_active = options.initialMode == "plan"_L1 ? Transport::Acp : Transport::Rpc;
    launch(options);
}

void OmpBackend::launch(const LaunchOptions &o)
{
    m_lastLaunch = o;
    m_readySinceStart = false;
    active()->start(o);
}

// omp's RPC startup cannot restore a session's model when the model comes from a discovery-backed
// provider (one whose models are listed by the endpoint), and exits with "Could not restore model
// <provider/id>". Naming the model explicitly works, so retry once with --model.
bool OmpBackend::retryWithSessionModel(const ProcessEvent &p)
{
    static const QRegularExpression restore(uR"(Could not restore model (\S+))"_s);
    const auto m = restore.match(p.message);
    if (!m.hasMatch() || m_readySinceStart || m_retriedWithModel || !m_lastLaunch.model.isEmpty()
        || m_lastLaunch.resumeSession.isEmpty())
        return false;
    m_retriedWithModel = true;
    auto retry = m_lastLaunch;
    retry.model = m.captured(1);
    qCInfo(lcOmp) << "retrying resume with an explicit model" << retry.model;
    launch(retry);
    return true;
}

void OmpBackend::stop()
{
    m_switching = false;
    m_rpc.stop();
    m_acp.stop();
}

bool OmpBackend::isRunning() const
{
    return active()->isRunning();
}

void OmpBackend::prompt(const QString &text, const QList<ImageAttachment> &images)
{
    active()->prompt(text, images);
}
void OmpBackend::steer(const QString &text)
{
    active()->steer(text);
}
void OmpBackend::followUp(const QString &text)
{
    active()->followUp(text);
}
void OmpBackend::abort()
{
    active()->abort();
}
void OmpBackend::answerUi(const QString &requestId, const QJsonObject &response)
{
    active()->answerUi(requestId, response);
}
void OmpBackend::answerApproval(const QString &requestId, bool approve)
{
    active()->answerApproval(requestId, approve);
}
void OmpBackend::answerPlan(const QString &requestId, const QString &optionId)
{
    active()->answerPlan(requestId, optionId);
}
void OmpBackend::call(const QString &command, const QJsonObject &params, Reply reply)
{
    active()->call(command, params, std::move(reply));
}

void OmpBackend::setMode(const QString &mode)
{
    if (m_switching)
        return;
    if (m_active == Transport::Rpc) {
        if (mode != "plan"_L1) {
            m_rpc.setMode(mode);
            return;
        }
        const auto &state = m_rpc.lastState();
        if (state.isStreaming) {
            emit agentEvent(NoticeEvent{u"warning"_s, u"Stop the current turn before switching to plan mode."_s, u"gomp"_s});
            return;
        }
        // A session with no messages has no file yet; plan mode then starts a fresh session.
        switchTo(Transport::Acp, state.messageCount > 0 ? state.sessionId : QString{}, u"plan"_s);
    } else {
        m_acp.setMode(mode); // current_mode_update "default" triggers the hand-back
    }
}

void OmpBackend::switchTo(Transport target, const QString &sessionId, const QString &mode)
{
    qCInfo(lcOmp) << "switching transport to" << (target == Transport::Rpc ? "rpc" : "acp") << "session" << sessionId;
    m_switching = true;
    m_returnAfterTurn = false;
    LaunchOptions next = m_options;
    next.resumeSession = sessionId;
    next.continueLatest = false;
    next.initialMode = mode;
    auto *outgoing = active();
    m_active = target;
    m_pendingLaunch = next;
    m_retriedWithoutResume = false;
    auto startIncoming = [this, next] {
        launch(next);
    };
    if (outgoing->isRunning()) {
        // Wait for the outgoing process to release the session file.
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(outgoing, &AgentBackend::agentEvent, this, [conn, startIncoming](const AgentEvent &ev) {
            if (const auto *p = std::get_if<ProcessEvent>(&ev);
                p && p->kind != ProcessEvent::Kind::Started) {
                QObject::disconnect(*conn);
                startIncoming();
            }
        });
        outgoing->stop();
    } else {
        startIncoming();
    }
}

void OmpBackend::forward(Transport from, const AgentEvent &ev)
{
    const bool fromActive = from == m_active;
    // A handoff whose resume dies before `ready` (the session was never written, e.g. it had no
    // messages) falls back to a fresh session rather than leaving the workspace disconnected.
    if (const auto *p = std::get_if<ProcessEvent>(&ev);
        fromActive && p && p->kind != ProcessEvent::Kind::Started && retryWithSessionModel(*p))
        return;
    if (fromActive && m_switching && !m_retriedWithoutResume && !m_pendingLaunch.resumeSession.isEmpty()) {
        if (const auto *p = std::get_if<ProcessEvent>(&ev); p && p->kind != ProcessEvent::Kind::Started) {
            m_retriedWithoutResume = true;
            auto fresh = m_pendingLaunch;
            fresh.resumeSession.clear();
            emit agentEvent(NoticeEvent{u"warning"_s, u"The session could not be reopened after the mode switch; continuing in a new session."_s, u"gomp"_s});
            launch(fresh);
            return;
        }
    }
    // Lifecycle noise from a transport handoff is internal.
    if (!fromActive || (m_switching && std::holds_alternative<ProcessEvent>(ev)))
        return;

    if (std::holds_alternative<ReadyEvent>(ev)) {
        m_switching = false;
        m_readySinceStart = true;
    }
    if (const auto *s = std::get_if<StateEvent>(&ev))
        m_lastSessionId = s->state.sessionId;

    emit agentEvent(ev);

    if (m_active != Transport::Acp)
        return;
    if (const auto *mode = std::get_if<ModeChangedEvent>(&ev); mode && mode->mode == "default"_L1) {
        // Plan approved (or the user left plan mode). Execution continues inside the current ACP
        // turn; hand back to RPC once it finishes so subagents and the full toolset are visible.
        if (m_acp.isPrompting())
            m_returnAfterTurn = true;
        else
            switchTo(Transport::Rpc, m_acp.sessionId(), u"default"_s);
    }
    if (m_returnAfterTurn && std::holds_alternative<AgentEndEvent>(ev) && !m_acp.isPrompting())
        switchTo(Transport::Rpc, m_acp.sessionId(), u"default"_s);
}

} // namespace gomp
