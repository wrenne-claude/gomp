#include "OmpLocator.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace gomp {

OmpLocator::OmpLocator(QObject *parent)
    : QObject(parent)
{
}

QString OmpLocator::installCommand() const
{
    return !QStandardPaths::findExecutable(u"bun"_s).isEmpty() ? u"bun install -g @oh-my-pi/pi-coding-agent"_s
                                                               : u"npm install -g @oh-my-pi/pi-coding-agent"_s;
}

QList<int> OmpLocator::parseVersion(const QString &out)
{
    static const QRegularExpression re(uR"((\d+)\.(\d+)\.(\d+))"_s);
    const auto m = re.match(out);
    if (!m.hasMatch())
        return {};
    return {m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt()};
}

bool OmpLocator::isSupported(const QList<int> &v)
{
    // The RPC wire protocol gomp speaks (protocol v1 with rpc-ui) is stable across 18.x.
    return v.size() == 3 && v.at(0) == 18;
}

void OmpLocator::locate(const QString &preferred)
{
    QStringList candidates;
    if (!preferred.isEmpty())
        candidates << preferred;
    const auto onPath = QStandardPaths::findExecutable(u"omp"_s);
    if (!onPath.isEmpty())
        candidates << onPath;
    const auto home = QDir::homePath();
    for (const auto &p : {home + u"/.local/bin/omp"_s, home + u"/.bun/bin/omp"_s, home + u"/.npm-global/bin/omp"_s,
                          u"/usr/local/bin/omp"_s, u"/usr/bin/omp"_s}) {
        if (!candidates.contains(p))
            candidates << p;
    }
    m_searched = candidates;
    m_status = Status::Checking;
    emit changed();
    probe(candidates, 0);
}

void OmpLocator::useBinary(const QString &pathOrUrl)
{
    const auto path = pathOrUrl.startsWith("file:"_L1) ? QUrl(pathOrUrl).toLocalFile() : pathOrUrl;
    m_searched = {path};
    m_status = Status::Checking;
    emit changed();
    probe({path}, 0);
}

void OmpLocator::probe(const QStringList &candidates, int index)
{
    while (index < candidates.size() && !QFileInfo(candidates.at(index)).isExecutable())
        ++index;
    if (index >= candidates.size()) {
        m_status = Status::Missing;
        m_path.clear();
        m_version.clear();
        m_message = u"omp was not found on PATH or in the usual install locations."_s;
        emit changed();
        return;
    }
    const auto path = candidates.at(index);
    auto *proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this, proc, path, candidates, index](int code, QProcess::ExitStatus) {
        const auto out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        proc->deleteLater();
        const auto v = parseVersion(out);
        if (code != 0 || v.isEmpty()) {
            if (index + 1 < candidates.size()) {
                probe(candidates, index + 1);
                return;
            }
            m_status = Status::Broken;
            m_path = path;
            m_message = u"%1 did not report a version (exit %2)."_s.arg(path).arg(code);
            emit changed();
            return;
        }
        m_path = QFileInfo(path).absoluteFilePath();
        m_version = u"%1.%2.%3"_s.arg(v.at(0)).arg(v.at(1)).arg(v.at(2));
        m_status = isSupported(v) ? Status::Found : Status::Untested;
        m_message = m_status == Status::Found
                        ? QString{}
                        : u"gomp was built against omp %1.x. omp %2 may behave differently; features that fail will say so."_s
                              .arg(testedVersion().section(u'.', 0, 0), m_version);
        emit changed();
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc, candidates, index](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        proc->deleteLater();
        probe(candidates, index + 1);
    });
    proc->start(path, {u"--version"_s});
}

} // namespace gomp
