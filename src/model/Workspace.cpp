#include "Workspace.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(lcWorkspace, "gomp.workspace")

using namespace Qt::StringLiterals;

namespace gomp {

namespace {
constexpr int RateWindowMs = 250;
constexpr qint64 NotifyAfterTurnMs = 20'000; // long turns earn a "finished" notification
constexpr qint64 MaxImageBytes = 20 * 1024 * 1024;

QString uiKindName(UiRequestEvent::Kind k)
{
    switch (k) {
    case UiRequestEvent::Kind::Select:
        return u"select"_s;
    case UiRequestEvent::Kind::Confirm:
        return u"confirm"_s;
    case UiRequestEvent::Kind::Input:
        return u"input"_s;
    case UiRequestEvent::Kind::Editor:
        return u"editor"_s;
    case UiRequestEvent::Kind::Ask:
        return u"ask"_s;
    case UiRequestEvent::Kind::OpenUrl:
        return u"open_url"_s;
    case UiRequestEvent::Kind::Notify:
        return u"notify"_s;
    }
    return {};
}
} // namespace

Workspace::Workspace(const QString &cwd, AgentBackend *backend, bool mock, QObject *parent)
    : QObject(parent)
    , m_cwd(cwd)
    , m_backend(backend)
    , m_mock(mock)
    , m_conversation(new ConversationModel(this))
    , m_todos(new TodoModel(this))
    , m_subagents(new SubagentModel(this))
    , m_commands(new CommandModel(this))
    , m_git(new GitInfo(this))
{
    backend->setParent(this);
    connect(backend, &AgentBackend::agentEvent, this, &Workspace::onEvent);
    connect(m_conversation, &ConversationModel::toolActivityChanged, this, &Workspace::recomputeActivity);
    connect(m_conversation, &ConversationModel::streamActivityChanged, this, &Workspace::recomputeActivity);
    connect(m_subagents, &SubagentModel::statsChanged, this, &Workspace::activityChanged);
    connect(m_todos, &TodoModel::edited, this, [this] {
        // Plan edits while a proposal is open stay local until the user answers; otherwise sync.
        if (!planPending())
            syncTodos();
    });

    m_rateTimer.setInterval(RateWindowMs);
    connect(&m_rateTimer, &QTimer::timeout, this, [this] {
        // ~4 characters per token is a fair average for code and English prose.
        const double instant = (double(m_charsThisWindow) / 4.0) * (1000.0 / RateWindowMs);
        m_charsThisWindow = 0;
        const double next = m_busy ? (m_tokensPerSecond * 0.6 + instant * 0.4) : 0.0;
        if (std::abs(next - m_tokensPerSecond) > 0.05) {
            m_tokensPerSecond = next < 0.5 ? 0.0 : next;
            emit activityChanged();
        }
        if (!m_busy && m_tokensPerSecond == 0.0)
            m_rateTimer.stop();
    });
    m_errorTimer.setSingleShot(true);
    m_errorTimer.setInterval(2400);
    connect(&m_errorTimer, &QTimer::timeout, this, [this] {
        m_errorFlash = false;
        recomputeActivity();
    });
    m_git->setDirectory(cwd);
}

Workspace::~Workspace()
{
    if (m_backend)
        m_backend->stop();
}

void Workspace::start(const LaunchOptions &options)
{
    m_launch = options;
    m_checkModel = options.resumeSession.isEmpty() && !options.continueLatest && options.model.isEmpty();
    m_historyLoaded = options.resumeSession.isEmpty() && !options.continueLatest;
    setConnection(Connection::Starting);
    m_backend->start(options);
}

QString Workspace::name() const
{
    return QFileInfo(m_cwd).fileName().isEmpty() ? m_cwd : QFileInfo(m_cwd).fileName();
}

void Workspace::checkStartupModel()
{
    // Runs once both omp's first state and the configured default are known, in either order.
    if (!m_checkModel || m_state.model.id.isEmpty() || m_expectedModel.isEmpty())
        return;
    m_checkModel = false;
    // Only a fully qualified selector can be compared; a fuzzy one ("opus") is omp's to resolve.
    const auto slash = m_expectedModel.indexOf(u'/');
    if (slash <= 0)
        return;
    const auto provider = m_expectedModel.left(slash);
    const auto id = m_expectedModel.mid(slash + 1);
    if (m_state.model.provider.compare(provider, Qt::CaseInsensitive) == 0 && m_state.model.id == id)
        return;
    m_conversation->addNotice(
        u"warning"_s,
        u"omp started on %1 instead of your default model, %2, which it could not find. If that provider "
        "lists its models from its endpoint, the list may not have loaded in time: raise discovery.timeoutMs "
        "for it in ~/.omp/agent/models.yml, or pick a model from the header."_s
            .arg(m_state.model.name.isEmpty() ? m_state.model.id : m_state.model.name, m_expectedModel));
}

double Workspace::quietSeconds() const
{
    return m_busy && m_lastOutput.isValid() ? m_lastOutput.elapsed() / 1000.0 : 0.0;
}

double Workspace::turnSeconds() const
{
    return m_busy && m_turnClock.isValid() ? m_turnClock.elapsed() / 1000.0 : 0.0;
}

QStringList Workspace::thinkingLevels() const
{
    QStringList levels{u"off"_s};
    levels << (m_state.model.thinkingLevels.isEmpty()
                   ? QStringList{u"minimal"_s, u"low"_s, u"medium"_s, u"high"_s, u"xhigh"_s}
                   : m_state.model.thinkingLevels);
    levels.removeDuplicates();
    return levels;
}

void Workspace::setConnection(Connection c, const QString &message)
{
    if (c == m_connection && message == m_connectionMessage)
        return;
    m_connection = c;
    m_connectionMessage = message;
    emit connectionChanged();
    recomputeActivity();
}

QStringList Workspace::diagnostics() const
{
    return m_backend ? m_backend->diagnostics() : QStringList{};
}

void Workspace::recomputeActivity()
{
    Activity a = Activity::Idle;
    if (!m_approvals.isEmpty() || planPending() || !m_uiRequests.isEmpty())
        a = Activity::Approval;
    else if (m_errorFlash)
        a = Activity::Error;
    else if (m_busy || m_compacting) {
        if (m_conversation->runningToolCount() > 0)
            a = Activity::Tools;
        else if (m_conversation->isThinking())
            a = Activity::Thinking;
        else if (m_conversation->isStreamingText())
            a = Activity::Streaming;
        else
            a = Activity::Waiting;
    }
    if (a != m_activity) {
        m_activity = a;
        emit activityChanged();
    }
}

void Workspace::onEvent(const AgentEvent &event)
{
    m_lastOutput.start(); // anything omp says counts as signs of life
    m_conversation->apply(event);

    if (const auto *e = std::get_if<ProcessEvent>(&event)) {
        switch (e->kind) {
        case ProcessEvent::Kind::Started:
            break;
        case ProcessEvent::Kind::Exited:
            m_busy = false;
            dropProcessRequests();
            setConnection(Connection::Disconnected, u"omp exited."_s);
            break;
        case ProcessEvent::Kind::Crashed: {
            m_busy = false;
            dropProcessRequests();
            static const QRegularExpression restore(uR"(Could not restore model (\S+))"_s);
            const auto m = restore.match(e->message);
            setConnection(Connection::Failed,
                          m.hasMatch() ? u"omp could not load %1, the model this session last used. Check that provider in "
                                         "Settings, then restart omp."_s.arg(m.captured(1))
                          : e->message.isEmpty() ? u"omp exited with code %1."_s.arg(e->exitCode)
                                                 : u"omp stopped: %1"_s.arg(e->message));
            emit attention(u"error"_s, name(), u"omp stopped unexpectedly. Restart the workspace to continue."_s);
            break;
        }
        case ProcessEvent::Kind::FailedToStart:
            m_busy = false;
            setConnection(Connection::Failed, u"Could not start omp: %1"_s.arg(e->message));
            break;
        case ProcessEvent::Kind::ProtocolError:
            setConnection(Connection::Failed, u"omp spoke a protocol gomp does not understand: %1"_s.arg(e->message));
            break;
        }
        return;
    }
    if (const auto *e = std::get_if<ReadyEvent>(&event)) {
        // A new omp process (start, restart or a plan-mode handoff): requests the previous one
        // made can no longer be answered.
        dropProcessRequests();
        setConnection(Connection::Ready);
        if (e->transport == "rpc"_L1 && !m_historyLoaded)
            loadHistory();
        if (!m_requestedMode.isEmpty() && e->transport == "rpc"_L1 && m_requestedMode == "default"_L1) {
            m_requestedMode.clear();
            m_mode = u"default"_s;
            emit modeChanged();
        }
        emit connectionChanged();
        return;
    }
    if (const auto *e = std::get_if<StateEvent>(&event)) {
        const bool sessionMoved = !m_state.sessionId.isEmpty() && m_state.sessionId != e->state.sessionId;
        m_state = e->state;
        if (!m_state.sessionName.isEmpty())
            m_title = m_state.sessionName;
        if (m_state.goalActive && m_mode != "goal"_L1) {
            m_mode = u"goal"_s;
            emit modeChanged();
        } else if (!m_state.goalActive && m_mode == "goal"_L1) {
            m_mode = u"default"_s;
            emit modeChanged();
        }
        if (!m_state.todoPhases.isEmpty() || !planPending())
            m_todos->setPhases(m_state.todoPhases);
        if (!m_busy && m_state.isStreaming) {
            m_busy = true;
            m_turnClock.start();
        }
        emit stateChanged();
        emit infoChanged();
        checkStartupModel();
        if (sessionMoved)
            emit sessionChanged();
        recomputeActivity();
        return;
    }
    if (const auto *e = std::get_if<StatsEvent>(&event)) {
        m_stats = e->stats;
        emit stateChanged();
        return;
    }
    if (const auto *e = std::get_if<UsageEvent>(&event)) {
        m_state.contextTokens = e->contextTokens;
        if (e->contextWindow > 0)
            m_state.contextWindow = e->contextWindow;
        if (e->cost >= 0)
            m_stats.cost = e->cost;
        emit stateChanged();
        return;
    }
    if (const auto *e = std::get_if<QueueEvent>(&event)) {
        m_state.queuedSteering = e->steering;
        m_state.queuedFollowUp = e->followUp;
        emit stateChanged();
        return;
    }
    if (const auto *e = std::get_if<TodosEvent>(&event)) {
        m_todos->setPhases(e->phases);
        return;
    }
    if (const auto *e = std::get_if<CommandsEvent>(&event)) {
        m_commands->setCommands(e->commands);
        return;
    }
    if (const auto *e = std::get_if<ModelChangedEvent>(&event)) {
        m_state.model = e->model;
        emit stateChanged();
        return;
    }
    if (const auto *e = std::get_if<ThinkingLevelEvent>(&event)) {
        m_state.thinkingLevel = e->level;
        emit stateChanged();
        return;
    }
    if (const auto *e = std::get_if<ModeChangedEvent>(&event)) {
        if (m_mode != e->mode || !m_requestedMode.isEmpty()) {
            m_mode = e->mode;
            // A switch back to default finishes when the RPC transport is ready again.
            if (!(m_requestedMode == "default"_L1 && transport() == "acp"_L1))
                m_requestedMode.clear();
            emit modeChanged();
        }
        return;
    }
    if (const auto *e = std::get_if<SessionInfoEvent>(&event)) {
        if (!e->title.isEmpty()) {
            m_title = e->title;
            emit infoChanged();
        }
        return;
    }
    if (std::holds_alternative<AgentStartEvent>(event)) {
        if (!m_busy) {
            m_busy = true;
            m_turnClock.start();
            m_subagents->clearFinished();
        }
        m_rateTimer.start();
        recomputeActivity();
        return;
    }
    if (const auto *e = std::get_if<BlockDeltaEvent>(&event)) {
        m_charsThisWindow += e->delta.size();
        if (!m_rateTimer.isActive())
            m_rateTimer.start();
        return;
    }
    if (const auto *e = std::get_if<PromptResultEvent>(&event)) {
        if (e->status == "error"_L1) {
            m_errorFlash = true;
            m_errorTimer.start();
        }
        if (e->settled || e->status != "completed"_L1) {
            const auto elapsed = m_turnClock.isValid() ? m_turnClock.elapsed() : 0;
            m_busy = false;
            if (e->status == "completed"_L1 && elapsed > NotifyAfterTurnMs)
                emit attention(u"done"_s, name(), u"Finished after %1 s."_s.arg(elapsed / 1000));
            else if (e->status == "error"_L1)
                emit attention(u"error"_s, name(), e->errorMessage);
        }
        recomputeActivity();
        if (!m_busy && !m_pendingFollowUp.isEmpty())
            send(std::exchange(m_pendingFollowUp, {}));
        return;
    }
    if (const auto *e = std::get_if<AgentEndEvent>(&event)) {
        if (e->terminal && !m_state.isStreaming) {
            m_busy = false;
            m_conversation->settleTools();
            recomputeActivity();
        }
        return;
    }
    if (const auto *e = std::get_if<ApprovalRequestEvent>(&event)) {
        if (e->clientGated && approvalModeAllows(e->toolName)) {
            // Plan mode's transport asks for bash/edit/delete/move whenever approvalMode is not
            // set explicitly; answer as the effective setting would (docs/omp-integration.md).
            m_conversation->resolveApproval(e->requestId, true);
            m_backend->answerApproval(e->requestId, true);
            return;
        }
        m_approvals << e->requestId;
        m_conversation->attachApproval(*e);
        emit attention(u"approval"_s, u"Approval needed in %1"_s.arg(name()),
                       u"%1 wants to run %2."_s.arg(m_state.model.name.isEmpty() ? u"The agent"_s : m_state.model.name,
                                                    e->toolName.isEmpty() ? u"a tool"_s : e->toolName));
        recomputeActivity();
        return;
    }
    if (const auto *e = std::get_if<UiRequestEvent>(&event)) {
        if (e->kind == UiRequestEvent::Kind::Notify) {
            emit toast(u"info"_s, e->message.isEmpty() ? e->title : e->message);
            return;
        }
        QVariantList questions;
        for (const auto &qv : e->raw.value("questions"_L1).toArray()) {
            const auto q = qv.toObject();
            QVariantList options;
            for (const auto &ov : q.value("options"_L1).toArray())
                options << ov.toObject().toVariantMap();
            questions << QVariantMap{{u"id"_s, q.value("id"_L1).toString()},
                                     {u"question"_s, q.value("question"_L1).toString()},
                                     {u"header"_s, q.value("header"_L1).toString()},
                                     {u"multi"_s, q.value("multi"_L1).toBool()},
                                     {u"recommended"_s, q.value("recommended"_L1).toVariant()},
                                     {u"options"_s, options}};
        }
        m_uiRequests << QVariantMap{{u"id"_s, e->requestId},
                                    {u"kind"_s, uiKindName(e->kind)},
                                    {u"title"_s, e->title},
                                    {u"message"_s, e->message},
                                    {u"options"_s, e->options},
                                    {u"optionDescriptions"_s, e->optionDescriptions},
                                    {u"placeholder"_s, e->placeholder},
                                    {u"prefill"_s, e->prefill},
                                    {u"url"_s, e->url},
                                    {u"questions"_s, questions}};
        emit uiRequestsChanged();
        emit attention(u"input"_s, u"%1 is asking a question"_s.arg(name()), e->title);
        recomputeActivity();
        return;
    }
    if (const auto *e = std::get_if<UiCancelEvent>(&event)) {
        m_uiRequests.removeIf([&](const QVariant &v) { return v.toMap().value(u"id"_s) == e->requestId; });
        if (m_approvals.removeAll(e->requestId) > 0)
            m_conversation->resolveApproval(e->requestId, false);
        emit uiRequestsChanged();
        recomputeActivity();
        return;
    }
    if (const auto *e = std::get_if<PlanProposalEvent>(&event)) {
        QVariantList options;
        for (qsizetype i = 0; i < e->optionIds.size(); ++i)
            options << QVariantMap{{u"id"_s, e->optionIds.at(i)}, {u"label"_s, e->optionLabels.value(i)}};
        m_plan = QVariantMap{{u"requestId"_s, e->requestId},
                             {u"title"_s, e->title},
                             {u"markdown"_s, e->planMarkdown},
                             {u"path"_s, e->planFilePath},
                             {u"options"_s, options}};
        // The plan's checklist becomes the editable step list unless omp already sent todos.
        const auto steps = TodoModel::fromMarkdown(e->planMarkdown);
        if (!steps.isEmpty())
            m_todos->setPhases(steps);
        emit planChanged();
        emit attention(u"plan"_s, u"Plan ready in %1"_s.arg(name()), e->title);
        recomputeActivity();
        return;
    }
    if (const auto *e = std::get_if<NoticeEvent>(&event)) {
        if (e->level == "error"_L1 || e->level == "warning"_L1)
            m_conversation->addNotice(e->level, e->message);
        else if (!e->message.startsWith("xd://"_L1)) // tool-mount chatter
            emit toast(u"info"_s, e->message);
        return;
    }
    if (const auto *e = std::get_if<CompactionEvent>(&event)) {
        m_compacting = e->started;
        if (!e->started && !e->aborted && e->errorMessage.isEmpty()) {
            m_lastCompaction = QVariantMap{{u"summary"_s, e->summary},
                                           {u"tokensBefore"_s, double(e->tokensBefore)},
                                           {u"tokensAfter"_s, double(e->tokensAfter)}};
            emit compactionFinished();
        }
        emit stateChanged();
        recomputeActivity();
        return;
    }
    if (std::holds_alternative<SubagentLifecycleEvent>(event) || std::holds_alternative<SubagentProgressEvent>(event)
        || std::holds_alternative<SubagentOutputEvent>(event)) {
        m_subagents->apply(event);
        return;
    }
}

void Workspace::loadHistory()
{
    m_historyLoaded = true;
    m_backend->call(u"get_messages"_s, {}, [this](bool ok, const QJsonValue &data, const QString &error) {
        if (!ok) {
            emit toast(u"warning"_s, u"Could not load the session history: %1"_s.arg(error));
            return;
        }
        const auto messages = data.isArray() ? data.toArray() : data.toObject().value("messages"_L1).toArray();
        m_conversation->loadHistory(messages);
    });
}

void Workspace::send(const QString &text, const QStringList &imagePaths)
{
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty() && imagePaths.isEmpty())
        return;
    QList<ImageAttachment> images;
    QMimeDatabase mimes;
    for (const auto &p : imagePaths) {
        const auto path = p.startsWith("file:"_L1) ? QUrl(p).toLocalFile() : p;
        QFile f(path);
        if (f.size() > MaxImageBytes) {
            emit toast(u"warning"_s, u"%1 is larger than 20 MB and was not attached."_s.arg(QFileInfo(path).fileName()));
            continue;
        }
        if (!f.open(QIODevice::ReadOnly)) {
            emit toast(u"warning"_s, u"Could not read %1."_s.arg(path));
            continue;
        }
        images << ImageAttachment{mimes.mimeTypeForFile(path).name(), f.readAll()};
    }
    if (!trimmed.startsWith(u'/'))
        m_lastPrompt = trimmed;
    m_backend->prompt(trimmed, images);
}

void Workspace::steer(const QString &text)
{
    if (!text.trimmed().isEmpty())
        m_backend->steer(text.trimmed());
}

void Workspace::stop()
{
    m_backend->abort();
}

void Workspace::retry()
{
    if (m_mock && !m_lastPrompt.isEmpty()) {
        send(m_lastPrompt);
        return;
    }
    m_backend->prompt(u"/retry"_s);
}

void Workspace::setApprovalPolicy(const QString &mode, const QVariantMap &toolPolicies)
{
    m_approvalMode = mode;
    m_toolPolicies = toolPolicies;
}

bool Workspace::approvalModeAllows(const QString &toolName) const
{
    // Mirrors omp's own rule: a per-tool policy wins, then the mode's tier. Of the tools ACP gates,
    // bash is exec tier; edit, delete and move are workspace writes.
    const auto policy = m_toolPolicies.value(toolName).toString();
    if (policy == "allow"_L1)
        return true;
    if (policy == "prompt"_L1 || policy == "deny"_L1 || toolName.isEmpty())
        return false;
    if (m_approvalMode == "yolo"_L1)
        return true;
    return m_approvalMode == "write"_L1 && toolName != "bash"_L1;
}

void Workspace::dropProcessRequests()
{
    const bool had = !m_approvals.isEmpty() || !m_uiRequests.isEmpty();
    m_approvals.clear();
    m_uiRequests.clear();
    m_conversation->settleTools();
    if (had) {
        emit uiRequestsChanged();
        recomputeActivity();
    }
}

void Workspace::approve(const QString &approvalId, bool approved)
{
    if (!m_approvals.removeAll(approvalId))
        return;
    m_backend->answerApproval(approvalId, approved);
    m_conversation->resolveApproval(approvalId, approved);
    recomputeActivity();
}

void Workspace::approveAll()
{
    const auto pending = m_approvals;
    for (const auto &id : pending)
        approve(id, true);
}

void Workspace::answerUi(const QString &requestId, const QVariant &value)
{
    QVariantMap req;
    for (const auto &v : std::as_const(m_uiRequests)) {
        if (v.toMap().value(u"id"_s) == requestId)
            req = v.toMap();
    }
    if (req.isEmpty())
        return;
    const auto kind = req.value(u"kind"_s).toString();
    QJsonObject response;
    if (kind == "confirm"_L1)
        response.insert("confirmed"_L1, value.toBool());
    else if (kind == "ask"_L1)
        response.insert("answers"_L1, QJsonArray::fromVariantList(value.toList()));
    else
        response.insert("value"_L1, value.toString());
    m_backend->answerUi(requestId, response);
    m_uiRequests.removeIf([&](const QVariant &v) { return v.toMap().value(u"id"_s) == requestId; });
    emit uiRequestsChanged();
    recomputeActivity();
}

void Workspace::dismissUi(const QString &requestId)
{
    m_backend->answerUi(requestId, {{"cancelled"_L1, true}});
    m_uiRequests.removeIf([&](const QVariant &v) { return v.toMap().value(u"id"_s) == requestId; });
    emit uiRequestsChanged();
    recomputeActivity();
}

void Workspace::setMode(const QString &mode)
{
    if (mode == m_mode && m_requestedMode.isEmpty())
        return;
    if (mode == "goal"_L1)
        return; // goal mode needs an objective: startGoal()
    if (m_mode == "goal"_L1)
        stopGoal();
    m_requestedMode = mode;
    emit modeChanged();
    m_backend->setMode(mode);
}

void Workspace::startGoal(const QString &objective)
{
    if (objective.trimmed().isEmpty())
        return;
    m_backend->call(u"goal"_s, {{"op"_L1, u"create"_s}, {"objective"_L1, objective.trimmed()}},
                    [this](bool ok, const QJsonValue &, const QString &error) {
                        if (!ok) {
                            emit toast(u"error"_s, error);
                            return;
                        }
                        m_mode = u"goal"_s;
                        emit modeChanged();
                        m_backend->call(u"get_state"_s);
                    });
}

void Workspace::stopGoal()
{
    m_backend->call(u"goal"_s, {{"op"_L1, u"drop"_s}}, [this](bool ok, const QJsonValue &, const QString &) {
        if (ok && m_mode == "goal"_L1) {
            m_mode = u"default"_s;
            emit modeChanged();
        }
    });
}

void Workspace::answerPlan(const QString &option, const QString &editedMarkdown)
{
    const auto requestId = m_plan.value(u"requestId"_s).toString();
    if (requestId.isEmpty())
        return;
    const auto original = m_plan.value(u"markdown"_s).toString();
    const bool edited = !editedMarkdown.trimmed().isEmpty() && editedMarkdown.trimmed() != original.trimmed();
    if (edited && option.startsWith("Approve"_L1)) {
        // omp executes the plan file it wrote; send the edits back as a refinement so the agent
        // rewrites the file and proposes the edited plan for a final approval.
        m_backend->answerPlan(requestId, u"Refine plan"_s);
        m_pendingFollowUp = u"I edited the plan. Rewrite the plan file to match this version exactly, then propose "
                            "it again:\n\n"_s
                            + editedMarkdown;
    } else {
        m_backend->answerPlan(requestId, option);
    }
    m_plan.insert(u"requestId"_s, QString{});
    m_plan.insert(u"answered"_s, option);
    emit planChanged();
    recomputeActivity();
}

void Workspace::rejectPlan(const QString &feedback)
{
    const auto requestId = m_plan.value(u"requestId"_s).toString();
    if (requestId.isEmpty())
        return;
    m_backend->answerPlan(requestId, u"Refine plan"_s);
    if (!feedback.trimmed().isEmpty())
        m_pendingFollowUp = feedback.trimmed();
    m_plan.insert(u"requestId"_s, QString{});
    m_plan.insert(u"answered"_s, u"Refine plan"_s);
    emit planChanged();
    recomputeActivity();
}

void Workspace::setModel(const QString &selector)
{
    const auto slash = selector.indexOf(u'/');
    if (slash <= 0)
        return;
    m_backend->call(u"set_model"_s, {{"provider"_L1, selector.left(slash)}, {"modelId"_L1, selector.mid(slash + 1)}},
                    [this, selector](bool ok, const QJsonValue &, const QString &error) {
                        if (!ok)
                            emit toast(u"error"_s, u"Could not switch to %1: %2"_s.arg(selector, error));
                        else
                            m_backend->call(u"get_state"_s);
                    });
}

void Workspace::setThinkingLevel(const QString &level)
{
    m_state.thinkingLevel = level;
    emit stateChanged();
    m_backend->call(u"set_thinking_level"_s, {{"level"_L1, level}}, [this](bool ok, const QJsonValue &, const QString &e) {
        if (!ok)
            emit toast(u"error"_s, e);
    });
}

void Workspace::setFastMode(bool enabled)
{
    m_backend->call(u"set_fast_mode"_s, {{"enabled"_L1, enabled}}, [this, enabled](bool ok, const QJsonValue &, const QString &e) {
        if (!ok) {
            emit toast(u"error"_s, e);
            return;
        }
        m_state.fastMode = enabled;
        emit stateChanged();
    });
}

void Workspace::compact(const QString &instructions)
{
    QJsonObject params;
    if (!instructions.trimmed().isEmpty())
        params.insert("customInstructions"_L1, instructions.trimmed());
    const auto before = m_state.contextTokens;
    m_compacting = true;
    emit stateChanged();
    recomputeActivity();
    m_backend->call(u"compact"_s, params, [this, before](bool ok, const QJsonValue &data, const QString &error) {
        m_compacting = false;
        if (!ok) {
            emit toast(u"error"_s, u"Compaction failed: %1"_s.arg(error));
            emit stateChanged();
            recomputeActivity();
            return;
        }
        const auto r = data.toObject();
        // Mock and some omp versions also emit compaction events; the reply is authoritative.
        m_backend->call(u"get_state"_s, {}, [this, r, before](bool, const QJsonValue &state, const QString &) {
            const auto after = state.toObject().value("contextUsage"_L1).toObject().value("tokens"_L1).toDouble(
                double(m_state.contextTokens));
            m_lastCompaction = QVariantMap{
                {u"summary"_s, r.value("shortSummary"_L1).toString(r.value("summary"_L1).toString())},
                {u"tokensBefore"_s, r.contains("tokensBefore"_L1) ? r.value("tokensBefore"_L1).toDouble() : double(before)},
                {u"tokensAfter"_s, after}};
            emit compactionFinished();
            emit stateChanged();
            recomputeActivity();
        });
    });
}

void Workspace::rename(const QString &title)
{
    const auto t = title.trimmed();
    if (t.isEmpty())
        return;
    m_title = t;
    emit infoChanged();
    m_backend->call(u"set_session_name"_s, {{"name"_L1, t}});
}

void Workspace::fork()
{
    m_backend->call(u"fork"_s, {}, [this](bool ok, const QJsonValue &, const QString &error) {
        if (!ok) {
            emit toast(u"error"_s, u"Fork failed: %1"_s.arg(error));
            return;
        }
        emit toast(u"info"_s, u"Forked into a new session. The original is unchanged."_s);
        m_backend->call(u"get_state"_s);
    });
}

void Workspace::branchAt(const QString &entryId)
{
    m_backend->call(u"branch"_s, {{"entryId"_L1, entryId}}, [this](bool ok, const QJsonValue &data, const QString &error) {
        if (!ok) {
            emit toast(u"error"_s, u"Could not branch: %1"_s.arg(error));
            return;
        }
        // omp returns the branched user message so it can be edited before resending.
        const auto text = data.toObject().value("text"_L1).toString();
        emit toast(u"info"_s, u"Branched. The conversation continues from that message."_s);
        loadHistory();
        if (!text.isEmpty())
            emit branchTextReady(text);
    });
}

void Workspace::newSession()
{
    m_backend->call(u"new_session"_s, {}, [this](bool ok, const QJsonValue &, const QString &error) {
        if (!ok) {
            emit toast(u"error"_s, error);
            return;
        }
        m_conversation->clear();
        m_todos->setPhases({});
        m_title.clear();
        emit infoChanged();
        m_backend->call(u"get_state"_s);
    });
}

void Workspace::exportHtml(const QString &path)
{
    const auto local = path.startsWith("file:"_L1) ? QUrl(path).toLocalFile() : path;
    QJsonObject params;
    if (!local.isEmpty())
        params.insert("outputPath"_L1, local);
    m_backend->call(u"export_html"_s, params, [this](bool ok, const QJsonValue &data, const QString &error) {
        if (ok)
            emit toast(u"info"_s, u"Exported to %1"_s.arg(data.toString()));
        else
            emit toast(u"error"_s, u"Export failed: %1"_s.arg(error));
    });
}

void Workspace::cancelSubagent(const QString &id)
{
    m_backend->call(u"cancel_subagent"_s, {{"subagentId"_L1, id}}, [this](bool ok, const QJsonValue &data, const QString &e) {
        if (!ok)
            emit toast(u"error"_s, e);
        else if (!data.toBool(true))
            emit toast(u"info"_s, u"That agent had already finished."_s);
    });
}

void Workspace::steerSubagent(const QString &id, const QString &message)
{
    if (message.trimmed().isEmpty())
        return;
    m_backend->call(u"steer_subagent"_s, {{"subagentId"_L1, id}, {"message"_L1, message.trimmed()}});
}

void Workspace::restart()
{
    auto opts = m_launch;
    if (!m_state.sessionFile.isEmpty()) {
        opts.resumeSession = m_state.sessionFile;
        opts.continueLatest = false;
    }
    m_historyLoaded = true; // the transcript on screen is still current
    m_backend->stop();
    setConnection(Connection::Starting);
    QTimer::singleShot(400, this, [this, opts] { m_backend->start(opts); });
}

void Workspace::syncTodos()
{
    m_backend->call(u"set_todos"_s, {{"phases"_L1, m_todos->toJson()}});
}

} // namespace gomp
