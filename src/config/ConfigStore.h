#pragma once

#include "SettingsSchema.h"
#include "util/LineDiff.h"
#include "util/YamlLite.h"

#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QTimer>

namespace gomp {

class OmpCli;

// Reads and writes omp's config.yml in two scopes:
//  * Global (~/.omp/agent/config.yml) is written through `omp config set|reset`, so omp validates
//    every value and keeps keys gomp does not know about.
//  * Project (<project>/.omp/config.yml, an overlay omp merges on top) is written by gomp through
//    YamlLite, which preserves unknown keys and key order and refuses YAML it cannot round-trip.
// Edits are staged, previewed as a per-file diff, the file is backed up, then changes apply.
class ConfigStore : public QObject
{
    Q_OBJECT
public:
    enum class Scope { Global, Project };

    ConfigStore(OmpCli *cli, SettingsSchema *schema, QObject *parent = nullptr);

    void setProjectDir(const QString &dir);
    [[nodiscard]] QString projectDir() const { return m_projectDir; }
    void reload();
    // Demo mode: read real files for display, never write them.
    void setDemo(bool demo) { m_demo = demo; }
    [[nodiscard]] bool isLoaded() const { return m_loaded; }

    [[nodiscard]] QJsonValue liveValue(const QString &id) const;    // effective per omp (defaults applied)
    [[nodiscard]] bool isSetIn(Scope scope, const QString &id) const; // explicitly present in that file
    [[nodiscard]] QJsonValue fileValue(Scope scope, const QString &id) const;
    [[nodiscard]] QJsonValue effectiveValue(const QString &id) const; // staged > project > global > default
    // The value as seen from one scope, staged edits included: Global ignores the project overlay;
    // Project shows its override or falls through to the global value.
    [[nodiscard]] QJsonValue scopedValue(const QString &id, Scope scope) const;

    // Staging. Returns a validation message, or empty when staged.
    QString stage(const QString &id, const QJsonValue &value, Scope scope);
    void stageReset(const QString &id, Scope scope);
    void unstage(const QString &id, Scope scope);
    void discardStaged();
    [[nodiscard]] bool isStaged(const QString &id, Scope scope) const;
    [[nodiscard]] int stagedCount() const { return int(m_staged.size()); }
    [[nodiscard]] QStringList stagedIds() const;

    struct FilePreview {
        Scope scope = Scope::Global;
        QString path;
        QList<diff::Line> lines;
        bool hasComments = false;
        QString error;
    };
    [[nodiscard]] QList<FilePreview> preview() const;
    void applyStaged();

    // Raw editor (Settings > Advanced).
    [[nodiscard]] QString filePath(Scope scope) const;
    [[nodiscard]] QString rawText(Scope scope) const;
    [[nodiscard]] QStringList validateRaw(const QString &text) const; // problems, empty when valid
    bool writeRaw(Scope scope, const QString &text, QString *error);

    [[nodiscard]] static QString backupDir();
    static QString backupFile(const QString &path); // returns the backup path, empty on failure

signals:
    void loaded();
    void loadFailed(const QString &message);
    void stagedChanged();
    void applied(bool ok, const QString &message);
    void valuesChanged();

private:
    struct Staged {
        QString id;
        Scope scope;
        QJsonValue value;
        bool reset = false;
    };
    [[nodiscard]] qsizetype stagedIndex(const QString &id, Scope scope) const;
    [[nodiscard]] yaml::ParseResult parseFile(Scope scope) const;
    [[nodiscard]] QString simulate(Scope scope, QString *error, bool *hasComments) const;
    void applyGlobal(QList<Staged> pending, QStringList failures);
    bool applyProject(const QList<Staged> &changes, QString *error);
    void readFiles();
    static QString cliValue(const QJsonValue &value);

    OmpCli *m_cli;
    SettingsSchema *m_schema;
    QString m_projectDir;
    QJsonObject m_live;
    yaml::Node m_globalFile;
    yaml::Node m_projectFile;
    QList<Staged> m_staged;
    bool m_loaded = false;
    bool m_applying = false;
    bool m_demo = false;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadDebounce;
};

} // namespace gomp
