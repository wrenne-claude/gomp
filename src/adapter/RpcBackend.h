#pragma once

#include "AgentBackend.h"
#include "JsonLineProcess.h"
#include "RpcParser.h"

#include <QHash>
#include <QTimer>

namespace gomp {

// `omp --mode rpc-ui`: the primary transport. See docs/omp-integration.md §1.
class RpcBackend : public AgentBackend
{
    Q_OBJECT
public:
    explicit RpcBackend(QObject *parent = nullptr);
    ~RpcBackend() override;

    void start(const LaunchOptions &options) override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] QString transportName() const override { return QStringLiteral("rpc"); }
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

    [[nodiscard]] const SessionSnapshot &lastState() const { return m_state; }

    // Builds the omp argument list; public for tests.
    static QStringList buildArguments(const LaunchOptions &options);
    // Replies wrap single values ({commands: [...]}); this applies the same per-command unwrap
    // omp's generated SDKs use, so callers get the value itself. Public for tests.
    static QJsonValue unwrap(const QString &command, const QJsonValue &data);
    // Protocol v2 transport limits (from omp's rpc wire code).
    static constexpr qsizetype MaxFrameBytes = 1024 * 1024;
    static constexpr qsizetype ChunkBytes = 256 * 1024;
    static constexpr qsizetype MaxReassembledBytes = 64LL * 1024 * 1024;

private:
    struct Pending {
        QString command;
        Reply reply;
        qint64 deadlineMs = 0;
    };

    void onFrame(const QJsonObject &frame);
    void onReady();
    void refreshState();
    void sweepTimeouts();
    void failAllPending(const QString &reason);
    static QJsonArray encodeImages(const QList<ImageAttachment> &images);
    static int timeoutFor(const QString &command);
    void sendFrame(const QJsonObject &frame);
    bool reassemble(const QJsonObject &chunk); // false: protocol violation, chunk state reset

    struct Chunked {
        QString id;
        int count = 0;
        int next = 0;
        qint64 byteLength = 0;
        QByteArray data;
    };

    JsonLineProcess m_process;
    QHash<QString, Pending> m_pending;
    QTimer m_timeoutSweep;
    QTimer m_stateDebounce;
    quint64 m_nextId = 1;
    bool m_ready = false;
    bool m_v2 = false;
    bool m_supportsV2 = false;
    Chunked m_chunk;
    SessionSnapshot m_state;
};

} // namespace gomp
