#include "McpStore.h"

#include "ConfigStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace gomp {

McpStore::McpStore(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &McpStore::reload);
    reload();
}

QString McpStore::path(const QString &scope) const
{
    if (scope == "project"_L1)
        return m_projectDir.isEmpty() ? QString{} : m_projectDir + u"/.omp/mcp.json"_s;
    const auto env = qEnvironmentVariable("PI_CONFIG_DIR");
    return (env.isEmpty() ? QDir::homePath() + u"/.omp/agent"_s : env) + u"/mcp.json"_s;
}

void McpStore::setProjectDir(const QString &dir)
{
    m_projectDir = dir;
    reload();
}

QJsonObject McpStore::readFile(const QString &scope, QString *error) const
{
    QFile f(path(scope));
    if (!f.exists())
        return {};
    if (!f.open(QIODevice::ReadOnly)) {
        *error = f.errorString();
        return {};
    }
    QJsonParseError pe{};
    const auto doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError) {
        *error = u"%1 is not valid JSON: %2"_s.arg(path(scope), pe.errorString());
        return {};
    }
    return doc.object();
}

void McpStore::reload()
{
    m_servers.clear();
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    for (const auto &scope : {u"global"_s, u"project"_s}) {
        const auto p = path(scope);
        if (p.isEmpty())
            continue;
        if (QFileInfo::exists(p))
            m_watcher.addPath(p);
        QString error;
        const auto root = readFile(scope, &error);
        const auto disabled = root.value("disabledServers"_L1).toArray();
        const auto servers = root.value("mcpServers"_L1).toObject();
        for (auto it = servers.begin(); it != servers.end(); ++it) {
            const auto c = it.value().toObject();
            const auto type = c.value("type"_L1).toString(c.contains("command"_L1) ? u"stdio"_s : u"http"_s);
            QStringList args;
            for (const auto &a : c.value("args"_L1).toArray())
                args << a.toString();
            const bool enabled = c.value("enabled"_L1).toBool(true) && !disabled.contains(QJsonValue(it.key()));
            m_servers << QVariantMap{
                {u"name"_s, it.key()},
                {u"scope"_s, scope},
                {u"type"_s, type},
                {u"command"_s, c.value("command"_L1).toString()},
                {u"args"_s, args},
                {u"url"_s, c.value("url"_L1).toString()},
                {u"enabled"_s, enabled},
                {u"timeout"_s, c.value("timeout"_L1).toDouble()},
                {u"envCount"_s, c.value("env"_L1).toObject().size()},
                {u"headerCount"_s, c.value("headers"_L1).toObject().size()},
                {u"env"_s, c.value("env"_L1).toObject().toVariantMap()},
                {u"headers"_s, c.value("headers"_L1).toObject().toVariantMap()},
                {u"auth"_s, c.contains("auth"_L1) || c.contains("oauth"_L1)},
            };
        }
    }
    emit changed();
}

QString McpStore::validateServer(const QString &name, const QJsonObject &c)
{
    static const QRegularExpression namePattern(u"^[a-zA-Z0-9_.-]{1,100}$"_s);
    if (!namePattern.match(name).hasMatch())
        return u"Names use letters, digits, dot, dash and underscore (up to 100)."_s;
    const auto type = c.value("type"_L1).toString(u"stdio"_s);
    if (type == "stdio"_L1) {
        if (c.value("command"_L1).toString().trimmed().isEmpty())
            return u"A local server needs a command to run."_s;
        if (c.contains("url"_L1))
            return u"A local server cannot also have a URL."_s;
    } else if (type == "http"_L1 || type == "sse"_L1) {
        const QUrl url(c.value("url"_L1).toString());
        if (!url.isValid() || url.host().isEmpty() || !(url.scheme() == "https"_L1 || url.scheme() == "http"_L1))
            return u"Enter the server's full URL, starting with https://."_s;
        if (c.contains("command"_L1))
            return u"A remote server cannot also have a command."_s;
    } else {
        return u"Transport must be stdio, http or sse."_s;
    }
    if (c.contains("timeout"_L1) && c.value("timeout"_L1).toDouble() < 0)
        return u"Timeout cannot be negative."_s;
    return {};
}

QString McpStore::validate(const QVariantMap &server) const
{
    auto obj = QJsonObject::fromVariantMap(server);
    const auto name = obj.take("name"_L1).toString();
    return validateServer(name, obj);
}

QString McpStore::writeFile(const QString &scope, const QJsonObject &root)
{
    const auto p = path(scope);
    if (p.isEmpty())
        return u"Open a project to add project servers."_s;
    if (ConfigStore::backupFile(p).isEmpty())
        return u"Could not back up %1; nothing was changed."_s.arg(p);
    QDir().mkpath(QFileInfo(p).absolutePath());
    QSaveFile f(p);
    if (!f.open(QIODevice::WriteOnly))
        return f.errorString();
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit())
        return f.errorString();
    reload();
    return {};
}

QString McpStore::saveServer(const QString &scope, const QString &originalName, const QVariantMap &server)
{
    QJsonObject incoming = QJsonObject::fromVariantMap(server);
    const auto name = incoming.take("name"_L1).toString().trimmed();
    incoming.remove("scope"_L1);
    // Normalise the form's shape into omp's: drop empty optionals, omit the stdio default type.
    if (incoming.value("type"_L1).toString() == "stdio"_L1) {
        incoming.remove("type"_L1);
        incoming.remove("url"_L1);
        incoming.remove("headers"_L1);
    } else {
        incoming.remove("command"_L1);
        incoming.remove("args"_L1);
        incoming.remove("env"_L1);
        incoming.remove("cwd"_L1);
    }
    for (const auto &k : {u"args"_s, u"env"_s, u"headers"_s}) {
        const auto v = incoming.value(k);
        if ((v.isArray() && v.toArray().isEmpty()) || (v.isObject() && v.toObject().isEmpty()))
            incoming.remove(k);
    }
    if (incoming.value("timeout"_L1).toDouble() <= 0)
        incoming.remove("timeout"_L1);
    if (incoming.value("enabled"_L1).toBool(true))
        incoming.remove("enabled"_L1);

    if (const auto err = validateServer(name, incoming); !err.isEmpty())
        return err;
    QString error;
    auto root = readFile(scope, &error);
    if (!error.isEmpty())
        return error;
    auto servers = root.value("mcpServers"_L1).toObject();
    if (name != originalName && servers.contains(name))
        return u"A server named %1 already exists here."_s.arg(name);
    // Keep fields the form does not edit (auth, oauth, instructions, requestIdFormat, ...).
    QJsonObject merged = servers.value(originalName.isEmpty() ? name : originalName).toObject();
    for (const auto &k : {"type"_L1, "command"_L1, "args"_L1, "env"_L1, "cwd"_L1, "url"_L1, "headers"_L1, "timeout"_L1,
                          "enabled"_L1})
        merged.remove(k);
    for (auto it = incoming.begin(); it != incoming.end(); ++it)
        merged.insert(it.key(), it.value());
    if (!originalName.isEmpty() && originalName != name)
        servers.remove(originalName);
    servers.insert(name, merged);
    root.insert("mcpServers"_L1, servers);
    return writeFile(scope, root);
}

QString McpStore::removeServer(const QString &scope, const QString &name)
{
    QString error;
    auto root = readFile(scope, &error);
    if (!error.isEmpty())
        return error;
    auto servers = root.value("mcpServers"_L1).toObject();
    if (!servers.contains(name))
        return u"%1 is not configured here."_s.arg(name);
    servers.remove(name);
    root.insert("mcpServers"_L1, servers);
    return writeFile(scope, root);
}

QString McpStore::setEnabled(const QString &scope, const QString &name, bool enabled)
{
    QString error;
    auto root = readFile(scope, &error);
    if (!error.isEmpty())
        return error;
    auto servers = root.value("mcpServers"_L1).toObject();
    auto c = servers.value(name).toObject();
    if (enabled)
        c.remove("enabled"_L1);
    else
        c.insert("enabled"_L1, false);
    servers.insert(name, c);
    root.insert("mcpServers"_L1, servers);
    // A denylist entry would override the flag; keep the two consistent.
    auto disabled = root.value("disabledServers"_L1).toArray();
    for (qsizetype i = disabled.size() - 1; i >= 0; --i) {
        if (enabled && disabled.at(i).toString() == name)
            disabled.removeAt(i);
    }
    if (root.contains("disabledServers"_L1))
        root.insert("disabledServers"_L1, disabled);
    return writeFile(scope, root);
}

} // namespace gomp
