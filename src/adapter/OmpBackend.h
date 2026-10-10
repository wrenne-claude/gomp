#pragma once

#include "AcpBackend.h"
#include "RpcBackend.h"

namespace gomp {

// The backend a real workspace uses. Runs omp over RPC and hands the session to an ACP process
// while in plan mode (docs/omp-integration.md §3). Only one omp process owns the session file at
// a time: the outgoing transport is stopped before the incoming one opens the session.
class OmpBackend : public AgentBackend
{
    Q_OBJECT
public:
    explicit OmpBackend(QObject *parent = nullptr);

    void start(const LaunchOptions &options) override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] QString transportName() const override { return active()->transportName(); }
    [[nodiscard]] QStringList diagnostics() const override { return active()->diagnostics(); }

    void prompt(const QString &text, const QList<ImageAttachment> &images = {}) override;
    void steer(const QString &text) override;
    void followUp(const QString &text) override;
    void abort() override;
    void answerUi(const QString &requestId, const QJsonObject &response) override;
    void answerApproval(const QString &requestId, bool approve) override;
    void answerPlan(const QString &requestId, const QString &optionId) override;
    void setMode(const QString &mode) override;
    void call(const QString &command, const QJsonObject &params = {}, Reply reply = {}) override;

private:
    enum class Transport { Rpc, Acp };
    [[nodiscard]] AgentBackend *active() const;
    void forward(Transport from, const AgentEvent &ev);
    void switchTo(Transport target, const QString &sessionId, const QString &mode);

    RpcBackend m_rpc;
    AcpBackend m_acp;
    Transport m_active = Transport::Rpc;
    LaunchOptions m_options;
    bool m_switching = false;
    bool m_returnAfterTurn = false;
    QString m_lastSessionId;
    LaunchOptions m_pendingLaunch; // the in-flight handoff, for the resume fallback
    bool m_retriedWithoutResume = false;
};

} // namespace gomp
