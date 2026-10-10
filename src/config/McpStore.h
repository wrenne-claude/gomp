#pragma once

#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// MCP servers from ~/.omp/agent/mcp.json (global) and <project>/.omp/mcp.json (project), in
// omp's format (resources/schema/mcp-schema.json). Edits touch only the affected server entry;
// every other key in the file is kept. Files are backed up before each write.
class McpStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(QVariantList servers READ servers NOTIFY changed)
public:
    explicit McpStore(QObject *parent = nullptr);

    void setProjectDir(const QString &dir);
    void setDemo(bool demo) { m_demo = demo; }
    [[nodiscard]] QVariantList servers() const { return m_servers; }

    // Returns an empty string on success, else a message for the form.
    Q_INVOKABLE QString saveServer(const QString &scope, const QString &originalName, const QVariantMap &server);
    Q_INVOKABLE QString removeServer(const QString &scope, const QString &name);
    Q_INVOKABLE QString setEnabled(const QString &scope, const QString &name, bool enabled);
    Q_INVOKABLE QString validate(const QVariantMap &server) const;
    Q_INVOKABLE void reload();

    [[nodiscard]] QString path(const QString &scope) const;
    // Validation core, public for tests.
    static QString validateServer(const QString &name, const QJsonObject &config);

signals:
    void changed();

private:
    QJsonObject readFile(const QString &scope, QString *error) const;
    QString writeFile(const QString &scope, const QJsonObject &root);

    QString m_projectDir;
    bool m_demo = false;
    QVariantList m_servers;
    QFileSystemWatcher m_watcher;
};

} // namespace gomp
