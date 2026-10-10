#pragma once

#include "AgentBackend.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QTimer>

#include <deque>

namespace gomp {

// A scriptable stand-in for omp, used for UI development, screenshots and smoke tests. Scenarios
// live in resources/mock/*.json as a small list of ops (think, say, tool, todos, subagents, plan,
// notice, error, wait). Ops are expanded into genuine RPC frames and fed through rpc::parseFrame,
// so the UI exercises the same parsing path it uses with real omp.
class MockBackend : public AgentBackend
{
    Q_OBJECT
public:
    explicit MockBackend(QObject *parent = nullptr);

    void start(const LaunchOptions &options) override;
    void stop() override;
    [[nodiscard]] QStringList diagnostics() const override { return m_stderr; }
    [[nodiscard]] bool isRunning() const override { return m_running; }
    [[nodiscard]] QString transportName() const override { return QStringLiteral("mock"); }

    void prompt(const QString &text, const QList<ImageAttachment> &images = {}) override;
    void steer(const QString &text) override;
    void followUp(const QString &text) override;
    void abort() override;
    void answerUi(const QString &requestId, const QJsonObject &response) override;
    void answerApproval(const QString &requestId, bool approve) override;
    void answerPlan(const QString &requestId, const QString &optionId) override;
    void setMode(const QString &mode) override;
    void call(const QString &command, const QJsonObject &params = {}, Reply reply = {}) override;

    // Speed multiplier for tests (e.g. 20 = twenty times faster).
    void setTimeScale(double scale) { m_timeScale = scale; }
    // Plays a scenario immediately as if the user had sent `prompt`.
    void playScenario(const QString &name, const QString &prompt = {});
    [[nodiscard]] static QStringList scenarioNames();
    // Sample catalog in `omp models --json` shape, used when no omp binary is available.
    [[nodiscard]] static QJsonArray sampleModels();
    // Copies the demo session transcripts into <dir>/<project>/ with timestamps relative to now.
    static void writeDemoSessions(const QString &dir);

private:
    struct Step {
        int delayMs = 0;
        std::function<void()> action;
        bool pauseAfter = false; // waits for an approval or plan answer
    };

    void enqueueOps(const QJsonArray &ops);
    void enqueueOp(const QJsonObject &op);
    void enqueue(int delayMs, std::function<void()> action, bool pause = false);
    void pump();
    void emitFrame(const QJsonObject &frame);
    void beginAssistantMessage();
    void endAssistantMessage(const QString &stopReason);
    void finishTurn(const QString &status);
    void emitState();
    static QJsonObject loadScenario(const QString &name);
    QString pickScenario(const QString &prompt) const;

    std::deque<Step> m_queue;
    QTimer m_timer;
    bool m_running = false;
    QStringList m_stderr;
    bool m_paused = false;
    bool m_streaming = false;
    double m_timeScale = 1.0;
    int m_seq = 0;
    QString m_messageId;
    int m_contentIndex = -1;
    QString m_lastBlock;
    QString m_mode = QStringLiteral("default");
    QString m_pendingApprovalId;
    QString m_pendingToolId;
    QString m_pendingPlanId;
    QJsonArray m_onDeny;
    QJsonArray m_onApprovePlan;
    QJsonArray m_todos;
    QStringList m_queued;
    bool m_titled = false;
    QJsonObject m_model;
    QString m_thinkingLevel = QStringLiteral("high");
    qint64 m_contextTokens = 18'400;
    double m_cost = 0.0;
    qint64 m_outputTokens = 0;
    QElapsedTimer m_turnClock;
};

} // namespace gomp
