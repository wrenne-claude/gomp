#pragma once

#include "AcpParser.h"
#include "AgentBackend.h"
#include "JsonLineProcess.h"

#include <QHash>
#include <QJsonArray>

namespace gomp {

// `omp acp`: Agent Client Protocol over stdio (JSON-RPC 2.0). gomp uses it for plan mode, which
// the RPC transport does not expose. Declares `elicitation.form` so omp asks before executing a
// plan instead of auto-approving it.
class AcpBackend : public AgentBackend
{
    Q_OBJECT
public:
    explicit AcpBackend(QObject *parent = nullptr);

    void start(const LaunchOptions &options) override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] QString transportName() const override { return QStringLiteral("acp"); }
    [[nodiscard]] QStringList diagnostics() const override { return m_process.stderrTail(); }

    void prompt(const QString &text, const QList<ImageAttachment> &images = {}) override;
    void steer(const QString &text) override;
    void followUp(const QString &text) override;
    void abort() override;
    void answerUi(const QString &requestId, const QJsonObject &response) override;
    void answerApproval(const QString &requestId, bool approve) override;
    void answerPlan(const QString &requestId, const QString &optionId) override;
    void setMode(const QString &mode) override;
    void call(const QString &command, const QJsonObject &params = {}, Reply reply = {}) override;

    [[nodiscard]] QString sessionId() const { return m_sessionId; }
    [[nodiscard]] bool isPrompting() const { return m_prompting; }

private:
    using RpcReply = std::function<void(const QJsonValue &result, const QJsonObject &error)>;
    struct PermissionRequest {
        QJsonValue rpcId;
        QJsonArray options;
    };

    void request(const QString &method, const QJsonObject &params, RpcReply reply = {});
    void notify(const QString &method, const QJsonObject &params);
    void respond(const QJsonValue &id, const QJsonObject &result);
    void respondError(const QJsonValue &id, int code, const QString &message);
    void onFrame(const QJsonObject &frame);
    void onServerRequest(const QJsonObject &frame);
    void openSession();

    JsonLineProcess m_process;
    LaunchOptions m_options;
    QHash<qint64, RpcReply> m_pending;
    QHash<QString, PermissionRequest> m_permissions;
    struct Elicitation {
        QJsonValue rpcId;
        QJsonObject params;
    };
    QHash<QString, Elicitation> m_elicitations;
    qint64 m_nextId = 1;
    QString m_sessionId;
    bool m_prompting = false;
    acp::StreamState m_stream;
};

} // namespace gomp
