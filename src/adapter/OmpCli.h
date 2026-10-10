#pragma once

#include <QJsonDocument>
#include <QObject>
#include <QProcessEnvironment>

#include <functional>

namespace gomp {

// Runs one-shot omp subcommands (`config`, `models`, `plugin`, `skill`, ...) for things that live
// outside a session. Output is parsed by the caller; this class only manages processes,
// timeouts and the environment (keyring-backed keys are injected the same way as for sessions).
class OmpCli : public QObject
{
    Q_OBJECT
public:
    using TextDone = std::function<void(bool ok, const QString &stdoutText, const QString &stderrText)>;
    using JsonDone = std::function<void(bool ok, const QJsonDocument &json, const QString &error)>;

    explicit OmpCli(QObject *parent = nullptr);

    void setProgram(const QString &ompPath) { m_program = ompPath; }
    [[nodiscard]] QString program() const { return m_program; }
    void setEnvironment(const QProcessEnvironment &env) { m_env = env; }

    void run(const QStringList &args, TextDone done, int timeoutMs = 30'000, const QString &workingDir = {});
    void runJson(const QStringList &args, JsonDone done, int timeoutMs = 30'000, const QString &workingDir = {});

private:
    QString m_program;
    QProcessEnvironment m_env = QProcessEnvironment::systemEnvironment();
};

} // namespace gomp
