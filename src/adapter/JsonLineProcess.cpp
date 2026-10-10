#include "JsonLineProcess.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTimer>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcProcess, "gomp.process")

namespace gomp {

namespace {
constexpr int StderrTailLines = 200;
}

JsonLineProcess::JsonLineProcess(QObject *parent)
    : QObject(parent)
{
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &JsonLineProcess::onStdout);
    connect(&m_process, &QProcess::readyReadStandardError, this, &JsonLineProcess::onStderr);
    connect(&m_process, &QProcess::started, this, &JsonLineProcess::started);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            emit failedToStart(m_process.errorString());
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        onStdout(); // drain anything left
        const bool crashed = status == QProcess::CrashExit && !m_stopping;
        emit exited(code, crashed);
        m_stopping = false;
    });
}

JsonLineProcess::~JsonLineProcess()
{
    // Owners are mid-destruction too; finishing the child must not call back into them.
    disconnect(&m_process, nullptr, this, nullptr);
    blockSignals(true);
    if (m_process.state() != QProcess::NotRunning) {
        m_stopping = true;
        m_process.closeWriteChannel();
        m_process.terminate();
        if (!m_process.waitForFinished(1000))
            m_process.kill();
        m_process.waitForFinished(500);
    }
}

void JsonLineProcess::start(const QString &program, const QStringList &args, const QString &workingDir,
                            const QProcessEnvironment &env)
{
    m_buffer.clear();
    m_stderr.clear();
    m_stopping = false;
    m_process.setProgram(program);
    m_process.setArguments(args);
    m_process.setWorkingDirectory(workingDir);
    m_process.setProcessEnvironment(env);
    qCInfo(lcProcess) << "starting" << program << args << "in" << workingDir;
    m_process.start(QIODevice::ReadWrite);
}

void JsonLineProcess::stop(int graceMs)
{
    if (m_process.state() == QProcess::NotRunning)
        return;
    m_stopping = true;
    // omp exits cleanly when stdin closes; escalate if it does not.
    m_process.closeWriteChannel();
    QTimer::singleShot(graceMs, this, [this] {
        if (m_process.state() != QProcess::NotRunning)
            m_process.terminate();
    });
    QTimer::singleShot(graceMs * 2, this, [this] {
        if (m_process.state() != QProcess::NotRunning)
            m_process.kill();
    });
}

bool JsonLineProcess::isRunning() const
{
    return m_process.state() == QProcess::Running;
}

void JsonLineProcess::send(const QJsonObject &frame)
{
    if (m_process.state() != QProcess::Running)
        return;
    QByteArray line = QJsonDocument(frame).toJson(QJsonDocument::Compact);
    line.append('\n');
    m_process.write(line);
}

QString JsonLineProcess::summarizeError(const QStringList &lines)
{
    static const QRegularExpression errorLine(uR"(^\s*(?:error|Error|fatal|Fatal error):\s*(.+)$)"_s);
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        if (const auto m = errorLine.match(*it); m.hasMatch())
            return m.captured(1).trimmed();
    }
    // Otherwise the last line that is not a stack frame, a source excerpt or a caret marker.
    static const QRegularExpression noise(uR"(^\s*(?:at\s|\d+\s*\||\^|$))"_s);
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        if (!noise.match(*it).hasMatch())
            return it->trimmed();
    }
    return {};
}

void JsonLineProcess::sendRaw(const QByteArray &line)
{
    if (m_process.state() == QProcess::Running)
        m_process.write(line);
}

void JsonLineProcess::onStdout()
{
    m_buffer.append(m_process.readAllStandardOutput());
    qsizetype start = 0;
    while (true) {
        const auto nl = m_buffer.indexOf('\n', start);
        if (nl < 0)
            break;
        const auto line = QByteArrayView(m_buffer).sliced(start, nl - start).trimmed();
        start = nl + 1;
        if (line.isEmpty())
            continue;
        QJsonParseError err{};
        const auto doc = QJsonDocument::fromJson(line.toByteArray(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            emit malformedLine(line.left(512).toByteArray());
            continue;
        }
        emit frameReceived(doc.object());
    }
    m_buffer.remove(0, start);
    if (m_buffer.size() > MaxLineBytes) {
        qCWarning(lcProcess) << "dropping oversized partial frame" << m_buffer.size();
        m_buffer.clear();
    }
}

void JsonLineProcess::onStderr()
{
    const auto text = QString::fromUtf8(m_process.readAllStandardError());
    for (const auto &line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        m_stderr << line;
        if (m_stderr.size() > StderrTailLines)
            m_stderr.removeFirst();
    }
}

} // namespace gomp
