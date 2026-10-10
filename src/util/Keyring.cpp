#include "Keyring.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace gomp {

namespace {
constexpr int TimeoutMs = 20'000; // the keyring may prompt to unlock

QString shellQuote(const QString &s)
{
    static const QRegularExpression safe(u"^[A-Za-z0-9._-]+$"_s);
    if (safe.match(s).hasMatch())
        return s;
    QString escaped = s;
    escaped.replace(u'\'', u"'\\''"_s);
    return u'\'' + escaped + u'\'';
}
} // namespace

Keyring::Keyring(QObject *parent)
    : QObject(parent)
{
}

bool Keyring::available()
{
#ifdef Q_OS_LINUX
    return !QStandardPaths::findExecutable(u"secret-tool"_s).isEmpty();
#else
    // The keyring path shells out to secret-tool (libsecret), which only exists on Linux. Keys
    // stay in environment variables on other platforms; gomp never writes one to a file.
    return false;
#endif
}

QString Keyring::lookupCommand(const QString &provider)
{
    return u"!secret-tool lookup service gomp provider "_s + shellQuote(provider);
}

bool Keyring::isGompLookupCommand(const QString &apiKeyValue)
{
    return apiKeyValue.startsWith("!secret-tool lookup service gomp provider "_L1);
}

void Keyring::store(const QString &provider, const QString &secret, Done done)
{
    run({u"store"_s, u"--label=gomp: "_s + provider + u" API key"_s, u"service"_s, u"gomp"_s, u"provider"_s, provider},
        secret.toUtf8(), std::move(done));
}

void Keyring::lookup(const QString &provider, Done done)
{
    run({u"lookup"_s, u"service"_s, u"gomp"_s, u"provider"_s, provider}, {}, std::move(done));
}

void Keyring::remove(const QString &provider, Done done)
{
    run({u"clear"_s, u"service"_s, u"gomp"_s, u"provider"_s, provider}, {}, std::move(done));
}

void Keyring::run(const QStringList &args, const QByteArray &stdinData, Done done)
{
    auto *proc = new QProcess(this);
    proc->setProgram(u"secret-tool"_s);
    proc->setArguments(args);
    connect(proc, &QProcess::finished, this, [proc, done](int code, QProcess::ExitStatus status) {
        const auto out = QString::fromUtf8(proc->readAllStandardOutput());
        const auto err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        proc->deleteLater();
        if (status != QProcess::NormalExit || code != 0) {
            done(false, err.isEmpty() ? u"The keyring refused the request (exit %1)."_s.arg(code) : err);
            return;
        }
        done(true, out);
    });
    connect(proc, &QProcess::errorOccurred, this, [proc, done](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        proc->deleteLater();
        done(false, u"secret-tool is not installed. Install libsecret-tools (Fedora: dnf install libsecret)."_s);
    });
    proc->start();
    if (!stdinData.isEmpty())
        proc->write(stdinData);
    proc->closeWriteChannel();
    // Guard against a keyring daemon that never answers; killing makes `finished` report failure.
    QTimer::singleShot(TimeoutMs, proc, [proc] {
        if (proc->state() != QProcess::NotRunning)
            proc->kill();
    });
}

} // namespace gomp
