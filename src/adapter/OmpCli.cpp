#include "OmpCli.h"

#include <QJsonParseError>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace gomp {

OmpCli::OmpCli(QObject *parent)
    : QObject(parent)
{
}

void OmpCli::run(const QStringList &args, TextDone done, int timeoutMs, const QString &workingDir)
{
    if (m_program.isEmpty()) {
        done(false, {}, u"omp is not configured"_s);
        return;
    }
    auto *proc = new QProcess(this);
    proc->setProcessEnvironment(m_env);
    if (!workingDir.isEmpty())
        proc->setWorkingDirectory(workingDir);
    auto finished = std::make_shared<bool>(false);
    connect(proc, &QProcess::finished, this, [proc, done, finished](int code, QProcess::ExitStatus status) {
        if (*finished)
            return;
        *finished = true;
        const auto out = QString::fromUtf8(proc->readAllStandardOutput());
        const auto err = QString::fromUtf8(proc->readAllStandardError());
        proc->deleteLater();
        done(status == QProcess::NormalExit && code == 0, out, err);
    });
    connect(proc, &QProcess::errorOccurred, this, [proc, done, finished](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart || *finished)
            return;
        *finished = true;
        proc->deleteLater();
        done(false, {}, proc->errorString());
    });
    QTimer::singleShot(timeoutMs, proc, [proc] {
        if (proc->state() != QProcess::NotRunning)
            proc->kill();
    });
    proc->start(m_program, args);
    proc->closeWriteChannel();
}

void OmpCli::runJson(const QStringList &args, JsonDone done, int timeoutMs, const QString &workingDir)
{
    run(
        args,
        [done](bool ok, const QString &out, const QString &err) {
            if (!ok) {
                done(false, {}, err.trimmed().isEmpty() ? u"omp exited with an error"_s : err.trimmed());
                return;
            }
            QJsonParseError pe{};
            // Some subcommands print a notice line before the JSON; parse from the first brace.
            const auto start = out.indexOf(QRegularExpression(u"^[\\[{]"_s, QRegularExpression::MultilineOption));
            const auto doc = QJsonDocument::fromJson(out.mid(std::max<qsizetype>(0, start)).toUtf8(), &pe);
            if (pe.error != QJsonParseError::NoError) {
                done(false, {}, u"Could not read omp's output: "_s + pe.errorString());
                return;
            }
            done(true, doc, {});
        },
        timeoutMs, workingDir);
}

} // namespace gomp
