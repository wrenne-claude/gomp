#include "ConfigStore.h"

#include "adapter/OmpCli.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>

Q_LOGGING_CATEGORY(lcConfig, "gomp.config")

using namespace Qt::StringLiterals;

namespace gomp {

namespace {
constexpr int MaxBackups = 30;

QString readText(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll());
}

QString ompAgentDir()
{
    const auto env = qEnvironmentVariable("PI_CONFIG_DIR");
    return env.isEmpty() ? QDir::homePath() + u"/.omp/agent"_s : env;
}
} // namespace

ConfigStore::ConfigStore(OmpCli *cli, SettingsSchema *schema, QObject *parent)
    : QObject(parent)
    , m_cli(cli)
    , m_schema(schema)
{
    m_reloadDebounce.setSingleShot(true);
    m_reloadDebounce.setInterval(300);
    connect(&m_reloadDebounce, &QTimer::timeout, this, [this] {
        if (!m_applying)
            reload();
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_reloadDebounce, qOverload<>(&QTimer::start));
}

QString ConfigStore::filePath(Scope scope) const
{
    if (scope == Scope::Global)
        return ompAgentDir() + u"/config.yml"_s;
    return m_projectDir.isEmpty() ? QString{} : m_projectDir + u"/.omp/config.yml"_s;
}

void ConfigStore::setProjectDir(const QString &dir)
{
    if (dir == m_projectDir)
        return;
    m_projectDir = dir;
    // Staged project edits belong to the previous project.
    for (qsizetype i = m_staged.size() - 1; i >= 0; --i) {
        if (m_staged.at(i).scope == Scope::Project)
            m_staged.removeAt(i);
    }
    readFiles();
    emit stagedChanged();
    emit valuesChanged();
}

yaml::ParseResult ConfigStore::parseFile(Scope scope) const
{
    const auto path = filePath(scope);
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        yaml::ParseResult empty;
        empty.ok = true;
        empty.root = yaml::Node::makeMap();
        return empty;
    }
    return yaml::parse(readText(path));
}

void ConfigStore::readFiles()
{
    const auto g = parseFile(Scope::Global);
    m_globalFile = g.ok ? g.root : yaml::Node::makeMap();
    const auto p = parseFile(Scope::Project);
    m_projectFile = p.ok ? p.root : yaml::Node::makeMap();
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    for (const auto scope : {Scope::Global, Scope::Project}) {
        const auto path = filePath(scope);
        if (!path.isEmpty() && QFileInfo::exists(path))
            m_watcher.addPath(path);
    }
}

void ConfigStore::reload()
{
    readFiles();
    if (m_demo) {
        m_loaded = true; // values fall back to the schema defaults
        emit loaded();
        emit valuesChanged();
        return;
    }
    // Run in an empty directory so no project overlay leaks into the "global" view.
    auto tmp = std::make_shared<QTemporaryDir>();
    m_cli->runJson(
        {u"config"_s, u"list"_s, u"--json"_s},
        [this, tmp](bool ok, const QJsonDocument &doc, const QString &error) {
            if (!ok) {
                qCWarning(lcConfig) << "config list failed:" << error;
                emit loadFailed(error);
                return;
            }
            m_live = doc.object();
            m_schema->mergeLive(m_live);
            m_loaded = true;
            emit loaded();
            emit valuesChanged();
        },
        30'000, tmp->path());
}

QJsonValue ConfigStore::liveValue(const QString &id) const
{
    const auto entry = m_live.value(id).toObject();
    if (entry.contains("value"_L1))
        return entry.value("value"_L1);
    if (const auto *def = m_schema->find(id))
        return def->defaultValue;
    return QJsonValue::Null;
}

bool ConfigStore::isSetIn(Scope scope, const QString &id) const
{
    const auto &root = scope == Scope::Global ? m_globalFile : m_projectFile;
    return root.atPath(id.split(u'.')) != nullptr;
}

QJsonValue ConfigStore::fileValue(Scope scope, const QString &id) const
{
    const auto &root = scope == Scope::Global ? m_globalFile : m_projectFile;
    const auto *n = root.atPath(id.split(u'.'));
    return n ? n->toJson() : QJsonValue(QJsonValue::Undefined);
}

QJsonValue ConfigStore::effectiveValue(const QString &id) const
{
    for (const auto scope : {Scope::Project, Scope::Global}) {
        const auto idx = stagedIndex(id, scope);
        if (idx >= 0) {
            const auto &s = m_staged.at(idx);
            if (!s.reset)
                return s.value;
            if (scope == Scope::Project)
                continue; // project reset falls through to global
            if (const auto *def = m_schema->find(id))
                return def->defaultValue;
        }
    }
    if (isSetIn(Scope::Project, id))
        return fileValue(Scope::Project, id);
    return liveValue(id);
}

QJsonValue ConfigStore::scopedValue(const QString &id, Scope scope) const
{
    auto globalValue = [&]() -> QJsonValue {
        const auto idx = stagedIndex(id, Scope::Global);
        if (idx >= 0) {
            if (!m_staged.at(idx).reset)
                return m_staged.at(idx).value;
            const auto *def = m_schema->find(id);
            return def ? def->defaultValue : QJsonValue{};
        }
        return liveValue(id);
    };
    if (scope == Scope::Global)
        return globalValue();
    const auto idx = stagedIndex(id, Scope::Project);
    if (idx >= 0)
        return m_staged.at(idx).reset ? globalValue() : m_staged.at(idx).value;
    return isSetIn(Scope::Project, id) ? fileValue(Scope::Project, id) : globalValue();
}

qsizetype ConfigStore::stagedIndex(const QString &id, Scope scope) const
{
    for (qsizetype i = 0; i < m_staged.size(); ++i) {
        if (m_staged.at(i).id == id && m_staged.at(i).scope == scope)
            return i;
    }
    return -1;
}

QString ConfigStore::stage(const QString &id, const QJsonValue &value, Scope scope)
{
    const auto *def = m_schema->find(id);
    if (!def)
        return u"Unknown setting %1."_s.arg(id);
    if (scope == Scope::Project && m_projectDir.isEmpty())
        return u"Open a project to change project settings."_s;
    const auto problem = SettingsSchema::validate(*def, value);
    if (!problem.isEmpty())
        return problem;
    const auto idx = stagedIndex(id, scope);
    // Staging the value already on disk is a no-op: drop any pending change instead.
    const bool sameAsFile = isSetIn(scope, id) && fileValue(scope, id) == value;
    if (sameAsFile) {
        if (idx >= 0)
            m_staged.removeAt(idx);
    } else if (idx >= 0) {
        m_staged[idx] = Staged{id, scope, value, false};
    } else {
        m_staged << Staged{id, scope, value, false};
    }
    emit stagedChanged();
    emit valuesChanged();
    return {};
}

void ConfigStore::stageReset(const QString &id, Scope scope)
{
    const auto idx = stagedIndex(id, scope);
    if (!isSetIn(scope, id)) {
        if (idx >= 0)
            m_staged.removeAt(idx);
    } else if (idx >= 0) {
        m_staged[idx] = Staged{id, scope, {}, true};
    } else {
        m_staged << Staged{id, scope, {}, true};
    }
    emit stagedChanged();
    emit valuesChanged();
}

void ConfigStore::unstage(const QString &id, Scope scope)
{
    const auto idx = stagedIndex(id, scope);
    if (idx < 0)
        return;
    m_staged.removeAt(idx);
    emit stagedChanged();
    emit valuesChanged();
}

void ConfigStore::discardStaged()
{
    if (m_staged.isEmpty())
        return;
    m_staged.clear();
    emit stagedChanged();
    emit valuesChanged();
}

bool ConfigStore::isStaged(const QString &id, Scope scope) const
{
    return stagedIndex(id, scope) >= 0;
}

QStringList ConfigStore::stagedIds() const
{
    QStringList ids;
    for (const auto &s : m_staged)
        ids << s.id;
    return ids;
}

QString ConfigStore::simulate(Scope scope, QString *error, bool *hasComments) const
{
    const auto parsed = parseFile(scope);
    if (!parsed.ok) {
        *error = u"%1 (line %2). Edit it in Settings › Advanced."_s.arg(parsed.error).arg(parsed.errorLine);
        return {};
    }
    *hasComments = parsed.hasComments;
    auto root = parsed.root;
    for (const auto &s : m_staged) {
        if (s.scope != scope)
            continue;
        const auto path = s.id.split(u'.');
        if (s.reset) {
            root.removePath(path);
        } else {
            const auto *prev = root.atPath(path);
            root.setPath(path, yaml::Node::fromJson(s.value, prev));
        }
    }
    return yaml::serialize(root);
}

QList<ConfigStore::FilePreview> ConfigStore::preview() const
{
    QList<FilePreview> out;
    for (const auto scope : {Scope::Global, Scope::Project}) {
        bool any = false;
        for (const auto &s : m_staged)
            any = any || s.scope == scope;
        if (!any)
            continue;
        FilePreview fp;
        fp.scope = scope;
        fp.path = filePath(scope);
        const auto before = readText(fp.path);
        const auto after = simulate(scope, &fp.error, &fp.hasComments);
        if (fp.error.isEmpty()) {
            // Diff on normalised text so the preview shows value changes, not re-indentation.
            const auto normalisedBefore = yaml::serialize(parseFile(scope).root);
            fp.lines = diff::compute(normalisedBefore.isEmpty() ? before : normalisedBefore, after, 2);
        }
        out << fp;
    }
    return out;
}

QString ConfigStore::backupDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + u"/gomp/backups"_s;
}

QString ConfigStore::backupFile(const QString &path)
{
    if (!QFileInfo::exists(path))
        return u"(new file)"_s;
    QDir dir(backupDir());
    if (!dir.mkpath(u"."_s))
        return {};
    const QFileInfo fi(path);
    // Name backups after the file and its parent so global and project copies stay distinguishable.
    const auto stem = fi.dir().dirName() + u'-' + fi.completeBaseName();
    const auto target = dir.filePath(stem + u'-' + QDateTime::currentDateTime().toString(u"yyyyMMdd-HHmmss-zzz"_s)
                                     + u'.' + fi.suffix());
    if (!QFile::copy(path, target))
        return {};
    auto old = dir.entryInfoList({stem + u"-*"_s}, QDir::Files, QDir::Time);
    for (qsizetype i = MaxBackups; i < old.size(); ++i)
        QFile::remove(old.at(i).absoluteFilePath());
    return target;
}

QString ConfigStore::cliValue(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Bool:
        return v.toBool() ? u"true"_s : u"false"_s;
    case QJsonValue::Double:
        return QString::fromUtf8(QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact).mid(1).chopped(1));
    case QJsonValue::String:
        return v.toString();
    case QJsonValue::Array:
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    case QJsonValue::Object:
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    default:
        return {};
    }
}

void ConfigStore::applyStaged()
{
    if (m_staged.isEmpty() || m_applying)
        return;
    if (m_demo) {
        m_staged.clear();
        emit stagedChanged();
        emit valuesChanged();
        emit applied(true, u"Demo mode: the change was previewed, but no file was written."_s);
        return;
    }
    m_applying = true;
    QList<Staged> global;
    QList<Staged> project;
    for (const auto &s : std::as_const(m_staged))
        (s.scope == Scope::Global ? global : project) << s;

    QString projectError;
    if (!project.isEmpty() && !applyProject(project, &projectError)) {
        m_applying = false;
        emit applied(false, projectError);
        return;
    }
    for (qsizetype i = m_staged.size() - 1; i >= 0; --i) {
        if (m_staged.at(i).scope == Scope::Project)
            m_staged.removeAt(i);
    }
    if (global.isEmpty()) {
        m_applying = false;
        readFiles();
        emit stagedChanged();
        emit valuesChanged();
        emit applied(true, u"Project settings saved."_s);
        return;
    }
    if (backupFile(filePath(Scope::Global)).isEmpty()) {
        m_applying = false;
        emit applied(false, u"Could not back up %1; nothing was changed."_s.arg(filePath(Scope::Global)));
        return;
    }
    applyGlobal(global, {});
}

void ConfigStore::applyGlobal(QList<Staged> pending, QStringList failures)
{
    if (pending.isEmpty()) {
        m_applying = false;
        emit stagedChanged();
        reload();
        if (failures.isEmpty())
            emit applied(true, u"Settings saved."_s);
        else
            emit applied(false, failures.join(u'\n'));
        return;
    }
    const auto next = pending.takeFirst();
    const QStringList args = next.reset ? QStringList{u"config"_s, u"reset"_s, next.id}
                                        : QStringList{u"config"_s, u"set"_s, next.id, cliValue(next.value)};
    auto tmp = std::make_shared<QTemporaryDir>();
    m_cli->run(
        args,
        [this, next, pending, failures, tmp](bool ok, const QString &, const QString &err) mutable {
            if (ok) {
                const auto idx = stagedIndex(next.id, Scope::Global);
                if (idx >= 0)
                    m_staged.removeAt(idx);
            } else {
                // omp prints "Error: Invalid value: ... Valid values: ..." — show it verbatim.
                failures << u"%1: %2"_s.arg(next.id, err.trimmed().remove(u"Error: "_s));
            }
            applyGlobal(pending, failures);
        },
        15'000, tmp->path());
}

bool ConfigStore::applyProject(const QList<Staged> &changes, QString *error)
{
    const auto path = filePath(Scope::Project);
    const auto parsed = parseFile(Scope::Project);
    if (!parsed.ok) {
        *error = u"%1 uses YAML gomp cannot edit safely (%2, line %3). Use Settings › Advanced."_s.arg(
            path, parsed.error).arg(parsed.errorLine);
        return false;
    }
    auto root = parsed.root;
    for (const auto &s : changes) {
        const auto p = s.id.split(u'.');
        if (s.reset)
            root.removePath(p);
        else
            root.setPath(p, yaml::Node::fromJson(s.value, root.atPath(p)));
    }
    if (backupFile(path).isEmpty()) {
        *error = u"Could not back up %1; nothing was changed."_s.arg(path);
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        *error = f.errorString();
        return false;
    }
    f.write(yaml::serialize(root).toUtf8());
    if (!f.commit()) {
        *error = f.errorString();
        return false;
    }
    return true;
}

QString ConfigStore::rawText(Scope scope) const
{
    return readText(filePath(scope));
}

QStringList ConfigStore::validateRaw(const QString &text) const
{
    QStringList problems;
    const auto parsed = yaml::parse(text);
    if (!parsed.ok) {
        problems << u"Line %1: %2"_s.arg(parsed.errorLine).arg(parsed.error);
        return problems;
    }
    // Check every known leaf; unknown keys are allowed (omp ignores or a newer omp understands them).
    for (const auto &def : m_schema->settings()) {
        const auto *n = parsed.root.atPath(def.path());
        if (!n)
            continue;
        const auto msg = SettingsSchema::validate(def, n->toJson());
        if (!msg.isEmpty())
            problems << u"%1: %2"_s.arg(def.id, msg);
    }
    return problems;
}

bool ConfigStore::writeRaw(Scope scope, const QString &text, QString *error)
{
    if (m_demo) {
        *error = u"Demo mode: files are not written."_s;
        return false;
    }
    const auto problems = validateRaw(text);
    if (!problems.isEmpty()) {
        *error = problems.join(u'\n');
        return false;
    }
    const auto path = filePath(scope);
    if (path.isEmpty()) {
        *error = u"No project is open."_s;
        return false;
    }
    if (backupFile(path).isEmpty()) {
        *error = u"Could not back up %1; nothing was changed."_s.arg(path);
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(text.toUtf8()) < 0 || !f.commit()) {
        *error = f.errorString();
        return false;
    }
    reload();
    return true;
}

} // namespace gomp
