#pragma once

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QStringList>

namespace gomp {

// A child process that speaks newline-delimited JSON on stdio. Owns framing only: it splits
// stdout into lines, parses each as a JSON object, and writes objects as single lines. stderr is
// kept in a bounded ring for the diagnostics view. The environment is never logged because it
// may hold API keys injected from the keyring.
class JsonLineProcess : public QObject
{
    Q_OBJECT
public:
    explicit JsonLineProcess(QObject *parent = nullptr);
    ~JsonLineProcess() override;

    void start(const QString &program, const QStringList &args, const QString &workingDir,
               const QProcessEnvironment &env);
    void stop(int graceMs = 1500);
    [[nodiscard]] bool isRunning() const;
    void send(const QJsonObject &frame);
    void sendRaw(const QByteArray &line); // one already-encoded JSON line, newline included

    [[nodiscard]] QStringList stderrTail() const { return m_stderr; }
    // The line worth showing from a crash log: omp's "error: ..." line rather than the last stack
    // frame or source excerpt. Public for tests.
    static QString summarizeError(const QStringList &stderrLines);
    [[nodiscard]] qint64 pid() const { return m_process.processId(); }

    static constexpr qsizetype MaxLineBytes = 64 * 1024 * 1024; // matches omp's reassembly limit

signals:
    void started();
    void frameReceived(const QJsonObject &frame);
    void malformedLine(const QByteArray &line);
    void exited(int exitCode, bool crashed);
    void failedToStart(const QString &message);

private:
    void onStdout();
    void onStderr();

    QProcess m_process;
    QByteArray m_buffer;
    QStringList m_stderr;
    bool m_stopping = false;
};

} // namespace gomp
