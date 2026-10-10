#pragma once

#include "CommandModel.h"
#include "ConversationModel.h"
#include "SubagentModel.h"
#include "TodoModel.h"
#include "adapter/AgentBackend.h"
#include "util/GitInfo.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// One workspace: a project directory with one live omp session. Owns the backend and the models
// the Agent, Plan and Subagent screens bind to, and derives the activity state that drives the
// Corona visualisation.
class Workspace : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by WorkspaceManager")

    Q_PROPERTY(QString name READ name NOTIFY infoChanged)
    Q_PROPERTY(QString cwd READ cwd CONSTANT)
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY infoChanged)
    Q_PROPERTY(QString sessionFile READ sessionFile NOTIFY infoChanged)
    Q_PROPERTY(QString sessionTitle READ sessionTitle NOTIFY infoChanged)
    Q_PROPERTY(bool mock READ isMock CONSTANT)
    Q_PROPERTY(QString transport READ transport NOTIFY connectionChanged)
    Q_PROPERTY(Connection connection READ connection NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionMessage READ connectionMessage NOTIFY connectionChanged)

    Q_PROPERTY(Activity activity READ activity NOTIFY activityChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY activityChanged)
    Q_PROPERTY(double tokensPerSecond READ tokensPerSecond NOTIFY activityChanged)
    Q_PROPERTY(int runningTools READ runningTools NOTIFY activityChanged)
    Q_PROPERTY(int pendingApprovals READ pendingApprovals NOTIFY activityChanged)
    Q_PROPERTY(double turnSeconds READ turnSeconds NOTIFY activityChanged)

    Q_PROPERTY(QString mode READ mode NOTIFY modeChanged)
    Q_PROPERTY(bool modeSwitching READ modeSwitching NOTIFY modeChanged)
    Q_PROPERTY(bool fastMode READ fastMode NOTIFY stateChanged)
    Q_PROPERTY(QString goalObjective READ goalObjective NOTIFY stateChanged)

    Q_PROPERTY(QString modelSelector READ modelSelector NOTIFY stateChanged)
    Q_PROPERTY(QString modelName READ modelName NOTIFY stateChanged)
    Q_PROPERTY(QString modelProvider READ modelProvider NOTIFY stateChanged)
    Q_PROPERTY(bool modelAcceptsImages READ modelAcceptsImages NOTIFY stateChanged)
    Q_PROPERTY(QString thinkingLevel READ thinkingLevel NOTIFY stateChanged)
    Q_PROPERTY(QStringList thinkingLevels READ thinkingLevels NOTIFY stateChanged)
    Q_PROPERTY(double contextTokens READ contextTokens NOTIFY stateChanged)
    Q_PROPERTY(double contextWindow READ contextWindow NOTIFY stateChanged)
    Q_PROPERTY(double cost READ cost NOTIFY stateChanged)
    Q_PROPERTY(double inputTokens READ inputTokens NOTIFY stateChanged)
    Q_PROPERTY(double outputTokens READ outputTokens NOTIFY stateChanged)
    Q_PROPERTY(bool compacting READ compacting NOTIFY stateChanged)
    Q_PROPERTY(QStringList queuedMessages READ queuedMessages NOTIFY stateChanged)

    Q_PROPERTY(QVariantMap plan READ plan NOTIFY planChanged)
    Q_PROPERTY(bool planPending READ planPending NOTIFY planChanged)
    Q_PROPERTY(QVariantList uiRequests READ uiRequests NOTIFY uiRequestsChanged)
    Q_PROPERTY(QVariantMap lastCompaction READ lastCompaction NOTIFY compactionFinished)

    Q_PROPERTY(gomp::ConversationModel *conversation READ conversation CONSTANT)
    Q_PROPERTY(gomp::TodoModel *todos READ todos CONSTANT)
    Q_PROPERTY(gomp::SubagentModel *subagents READ subagents CONSTANT)
    Q_PROPERTY(gomp::CommandModel *commands READ commands CONSTANT)
    Q_PROPERTY(gomp::GitInfo *git READ git CONSTANT)

public:
    enum class Connection { Starting, Ready, Switching, Disconnected, Failed };
    Q_ENUM(Connection)
    enum class Activity { Idle, Waiting, Thinking, Streaming, Tools, Approval, Error };
    Q_ENUM(Activity)

    Workspace(const QString &cwd, AgentBackend *backend, bool mock, QObject *parent = nullptr);
    ~Workspace() override;

    void start(const LaunchOptions &options);
    // The configured default model ("provider/id", from modelRoles.default). A new session that
    // starts on another model gets a notice saying so, since omp falls back silently.
    void setExpectedModel(const QString &selector)
    {
        m_expectedModel = selector;
        checkStartupModel();
    }

    [[nodiscard]] QString name() const;
    [[nodiscard]] QString cwd() const { return m_cwd; }
    [[nodiscard]] QString sessionId() const { return m_state.sessionId; }
    [[nodiscard]] QString sessionFile() const { return m_state.sessionFile; }
    [[nodiscard]] QString sessionTitle() const { return m_title; }
    [[nodiscard]] bool isMock() const { return m_mock; }
    [[nodiscard]] QString transport() const { return m_backend ? m_backend->transportName() : QString{}; }
    [[nodiscard]] Connection connection() const { return m_connection; }
    [[nodiscard]] QString connectionMessage() const { return m_connectionMessage; }

    [[nodiscard]] Activity activity() const { return m_activity; }
    [[nodiscard]] bool busy() const { return m_busy; }
    [[nodiscard]] double tokensPerSecond() const { return m_tokensPerSecond; }
    [[nodiscard]] int runningTools() const { return m_conversation->runningToolCount(); }
    [[nodiscard]] int pendingApprovals() const { return int(m_approvals.size()); }
    [[nodiscard]] double turnSeconds() const;

    [[nodiscard]] QString mode() const { return m_mode; }
    [[nodiscard]] bool modeSwitching() const { return !m_requestedMode.isEmpty(); }
    [[nodiscard]] bool fastMode() const { return m_state.fastMode; }
    [[nodiscard]] QString goalObjective() const { return m_state.goalObjective; }

    [[nodiscard]] QString modelSelector() const { return m_state.model.id.isEmpty() ? QString{} : m_state.model.selector(); }
    [[nodiscard]] QString modelName() const { return m_state.model.name; }
    [[nodiscard]] QString modelProvider() const { return m_state.model.provider; }
    [[nodiscard]] bool modelAcceptsImages() const { return m_state.model.acceptsImages; }
    [[nodiscard]] QString thinkingLevel() const { return m_state.thinkingLevel; }
    [[nodiscard]] QStringList thinkingLevels() const;
    [[nodiscard]] double contextTokens() const { return double(m_state.contextTokens); }
    [[nodiscard]] double contextWindow() const { return double(m_state.contextWindow); }
    [[nodiscard]] double cost() const { return m_stats.cost; }
    [[nodiscard]] double inputTokens() const { return double(m_stats.inputTokens); }
    [[nodiscard]] double outputTokens() const { return double(m_stats.outputTokens); }
    [[nodiscard]] bool compacting() const { return m_state.isCompacting || m_compacting; }
    [[nodiscard]] QStringList queuedMessages() const { return m_state.queuedSteering + m_state.queuedFollowUp; }

    [[nodiscard]] QVariantMap plan() const { return m_plan; }
    [[nodiscard]] bool planPending() const { return !m_plan.value(QStringLiteral("requestId")).toString().isEmpty(); }
    [[nodiscard]] QVariantList uiRequests() const { return m_uiRequests; }
    [[nodiscard]] QVariantMap lastCompaction() const { return m_lastCompaction; }

    [[nodiscard]] ConversationModel *conversation() const { return m_conversation; }
    [[nodiscard]] TodoModel *todos() const { return m_todos; }
    [[nodiscard]] SubagentModel *subagents() const { return m_subagents; }
    [[nodiscard]] CommandModel *commands() const { return m_commands; }
    [[nodiscard]] GitInfo *git() const { return m_git; }
    [[nodiscard]] AgentBackend *backend() const { return m_backend; }

    // Composer
    Q_INVOKABLE void send(const QString &text, const QStringList &imagePaths = {});
    Q_INVOKABLE void steer(const QString &text);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void retry();

    // Approvals and host UI
    Q_INVOKABLE void approve(const QString &approvalId, bool approved);
    Q_INVOKABLE void approveAll();
    Q_INVOKABLE void answerUi(const QString &requestId, const QVariant &value);
    Q_INVOKABLE void dismissUi(const QString &requestId);

    // Plan mode
    Q_INVOKABLE void setMode(const QString &mode);
    Q_INVOKABLE void answerPlan(const QString &option, const QString &editedMarkdown = {});
    Q_INVOKABLE void rejectPlan(const QString &feedback);
    Q_INVOKABLE void startGoal(const QString &objective);
    Q_INVOKABLE void stopGoal();

    // Session and model
    Q_INVOKABLE void setModel(const QString &selector);
    Q_INVOKABLE void setThinkingLevel(const QString &level);
    Q_INVOKABLE void setFastMode(bool enabled);
    Q_INVOKABLE void compact(const QString &instructions = {});
    Q_INVOKABLE void rename(const QString &title);
    Q_INVOKABLE void fork();
    Q_INVOKABLE void branchAt(const QString &entryId);
    Q_INVOKABLE void newSession();
    Q_INVOKABLE void exportHtml(const QString &path);
    Q_INVOKABLE void cancelSubagent(const QString &id);
    Q_INVOKABLE void steerSubagent(const QString &id, const QString &message);
    Q_INVOKABLE void restart();
    Q_INVOKABLE void syncTodos();
    Q_INVOKABLE QStringList diagnostics() const;

signals:
    void infoChanged();
    void connectionChanged();
    void activityChanged();
    void modeChanged();
    void stateChanged();
    void planChanged();
    void uiRequestsChanged();
    void compactionFinished();
    void toast(const QString &level, const QString &message);
    // Something the user should look at; the app notifies when the window is unfocused.
    void attention(const QString &kind, const QString &title, const QString &body);
    void sessionChanged();
    void branchTextReady(const QString &text);

private:
    void onEvent(const AgentEvent &event);
    void recomputeActivity();
    void setConnection(Connection c, const QString &message = {});
    void loadHistory();

    QString m_cwd;
    QPointer<AgentBackend> m_backend;
    bool m_mock;
    LaunchOptions m_launch;
    ConversationModel *m_conversation;
    TodoModel *m_todos;
    SubagentModel *m_subagents;
    CommandModel *m_commands;
    GitInfo *m_git;

    SessionSnapshot m_state;
    SessionStats m_stats;
    QString m_title;
    QString m_mode = QStringLiteral("default");
    QString m_requestedMode;
    Connection m_connection = Connection::Starting;
    QString m_connectionMessage;
    Activity m_activity = Activity::Idle;
    bool m_busy = false;
    bool m_compacting = false;
    bool m_errorFlash = false;
    double m_tokensPerSecond = 0;
    QStringList m_approvals;
    QVariantMap m_plan;
    QVariantList m_uiRequests;
    QVariantMap m_lastCompaction;
    QString m_lastPrompt;
    QString m_pendingFollowUp; // sent when the current turn ends (plan refinement feedback)
    QElapsedTimer m_turnClock;
    QTimer m_rateTimer;
    QTimer m_errorTimer;
    qint64 m_charsThisWindow = 0;
    bool m_historyLoaded = false;
    QString m_expectedModel;
    bool m_checkModel = false;
    void checkStartupModel();
};

} // namespace gomp
