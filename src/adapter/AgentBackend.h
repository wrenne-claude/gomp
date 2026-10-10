#pragma once

#include "AgentEvents.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QProcessEnvironment>

#include <functional>

namespace gomp {

struct LaunchOptions {
    QString ompPath;
    QString cwd;
    QString resumeSession; // session file path or id; empty starts a new session
    bool continueLatest = false;
    QString model;         // provider/id selector
    QString approvalMode;  // always-ask | write | yolo; empty keeps config
    QString profile;
    QString initialMode;   // default | plan
    QStringList extraArgs;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
};

struct ImageAttachment {
    QString mimeType;
    QByteArray data; // raw bytes; backends encode as needed
};

// One live omp session. Implementations: RpcBackend (omp --mode rpc-ui), AcpBackend (omp acp,
// used for plan mode), OmpBackend (switches between the two), MockBackend (scripted).
class AgentBackend : public QObject
{
    Q_OBJECT
public:
    using Reply = std::function<void(bool ok, const QJsonValue &data, const QString &error)>;

    using QObject::QObject;

    virtual void start(const LaunchOptions &options) = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool isRunning() const = 0;
    [[nodiscard]] virtual QString transportName() const = 0;
    [[nodiscard]] virtual QStringList diagnostics() const { return {}; }

    virtual void prompt(const QString &text, const QList<ImageAttachment> &images = {}) = 0;
    virtual void steer(const QString &text) = 0;
    virtual void followUp(const QString &text) = 0;
    virtual void abort() = 0;

    // Host UI answers. `response` is one of {value}, {confirmed}, {cancelled:true}, {answers}.
    virtual void answerUi(const QString &requestId, const QJsonObject &response) = 0;
    virtual void answerApproval(const QString &requestId, bool approve) = 0;
    virtual void answerPlan(const QString &requestId, const QString &optionId) = 0;

    virtual void setMode(const QString &mode) = 0; // default | plan

    // RPC-style command pass-through (set_model, compact, fork, get_tree, ...). Backends that
    // cannot serve a command reply with ok=false and a human-readable reason.
    virtual void call(const QString &command, const QJsonObject &params = {}, Reply reply = {}) = 0;

signals:
    void agentEvent(const gomp::AgentEvent &event);
};

} // namespace gomp
