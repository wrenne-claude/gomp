#pragma once

#include "EffectsProbe.h"
#include "Preferences.h"
#include "adapter/OmpCli.h"
#include "adapter/OmpLocator.h"
#include "config/ConfigStore.h"
#include "config/McpStore.h"
#include "config/ProviderStore.h"
#include "config/SettingsSchema.h"
#include "model/ModelCatalog.h"
#include "model/PaletteModel.h"
#include "model/SessionIndex.h"
#include "model/SettingsModel.h"
#include "model/ShortcutModel.h"
#include "model/WorkspaceManager.h"
#include "util/Notifier.h"
#include "util/Markdown.h"
#include "util/SyntaxHighlighter.h"

#include <QCache>

#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// Root of the state layer, exposed to QML as the `App` singleton. Owns every store and model,
// wires them together, and holds the few app-wide actions QML cannot do itself (clipboard,
// notifications, syntax highlighting, file manager).
class AppController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON
    Q_PROPERTY(gomp::Preferences *prefs READ prefs CONSTANT)
    Q_PROPERTY(gomp::OmpLocator *omp READ locator CONSTANT)
    Q_PROPERTY(gomp::WorkspaceManager *workspaces READ workspaces CONSTANT)
    Q_PROPERTY(gomp::SessionIndex *sessions READ sessions CONSTANT)
    Q_PROPERTY(gomp::SettingsModel *settings READ settings CONSTANT)
    Q_PROPERTY(gomp::ProviderStore *providers READ providers CONSTANT)
    Q_PROPERTY(gomp::McpStore *mcp READ mcp CONSTANT)
    Q_PROPERTY(gomp::ModelCatalog *models READ models CONSTANT)
    Q_PROPERTY(gomp::PaletteModel *palette READ palette CONSTANT)
    Q_PROPERTY(gomp::ShortcutModel *shortcuts READ shortcuts CONSTANT)
    Q_PROPERTY(gomp::EffectsProbe *effects READ effects CONSTANT)
    Q_PROPERTY(bool mockMode READ mockMode NOTIFY mockModeChanged)
    Q_PROPERTY(bool needsFirstRun READ needsFirstRun NOTIFY needsFirstRunChanged)
    Q_PROPERTY(QColor systemAccent READ systemAccent NOTIFY systemPaletteChanged)
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY systemPaletteChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString screenshotDir READ screenshotDir CONSTANT)
    Q_PROPERTY(QString shotFilter READ shotFilter CONSTANT)
    Q_PROPERTY(bool windowActive READ windowActive NOTIFY windowActiveChanged)
public:
    struct Options {
        bool mock = false;
        QString screenshotDir;
        QString openProject;
    };

    explicit AppController(const Options &options, QObject *parent = nullptr);
    ~AppController() override;
    static AppController *create(QQmlEngine *, QJSEngine *);
    static AppController *instance();

    [[nodiscard]] Preferences *prefs() const { return m_prefs; }
    [[nodiscard]] OmpLocator *locator() const { return m_locator; }
    [[nodiscard]] WorkspaceManager *workspaces() const { return m_workspaces; }
    [[nodiscard]] SessionIndex *sessions() const { return m_sessions; }
    [[nodiscard]] SettingsModel *settings() const { return m_settings; }
    [[nodiscard]] ProviderStore *providers() const { return m_providers; }
    [[nodiscard]] McpStore *mcp() const { return m_mcp; }
    [[nodiscard]] ModelCatalog *models() const { return m_models; }
    [[nodiscard]] PaletteModel *palette() const { return m_palette; }
    [[nodiscard]] ShortcutModel *shortcuts() const { return m_shortcuts; }
    [[nodiscard]] EffectsProbe *effects() const { return m_effects; }
    [[nodiscard]] bool mockMode() const { return m_mock; }
    [[nodiscard]] bool needsFirstRun() const;
    [[nodiscard]] QColor systemAccent() const;
    [[nodiscard]] bool systemDark() const;
    [[nodiscard]] QString version() const;
    [[nodiscard]] QString screenshotDir() const { return m_options.screenshotDir; }
    [[nodiscard]] static QString shotFilter() { return qEnvironmentVariable("GOMP_SHOT_FILTER"); }
    [[nodiscard]] bool windowActive() const;

    Q_INVOKABLE void start();
    Q_INVOKABLE int openProject(const QString &dirOrUrl, const QString &initialMode = {});
    Q_INVOKABLE int openSession(const QString &sessionPath, const QString &cwd);
    Q_INVOKABLE int openDemo(const QString &scenario = {});
    Q_INVOKABLE void enterDemoMode();
    Q_INVOKABLE void completeFirstRun();
    Q_INVOKABLE void setProjectScope(const QString &dir);

    Q_INVOKABLE QString highlight(const QString &code, const QString &language) const;
    Q_INVOKABLE void setSyntaxPalette(const QVariantMap &colors);
    Q_INVOKABLE QString renderMarkdown(const QString &markdown) const;
    Q_INVOKABLE QVariantList markdownSegments(const QString &markdown) const;
    Q_INVOKABLE void copyText(const QString &text) const;
    Q_INVOKABLE void openUrl(const QString &url) const;
    Q_INVOKABLE void revealPath(const QString &path) const;
    Q_INVOKABLE QString toLocalPath(const QString &url) const;
    Q_INVOKABLE bool isDirectory(const QString &path) const;
    Q_INVOKABLE QString homePath() const;
    Q_INVOKABLE QString pasteImage() const; // saves a clipboard image to a temp file, returns its path
    // Runs `omp <args>` and calls back with (ok, parsedJson, error). Demo mode answers from samples.
    Q_INVOKABLE void ompJson(const QStringList &args, const QJSValue &callback);
    Q_INVOKABLE void ompRun(const QStringList &args, const QJSValue &callback);

signals:
    void mockModeChanged();
    void needsFirstRunChanged();
    void systemPaletteChanged();
    void windowActiveChanged();
    void raiseRequested();
    void toast(const QString &level, const QString &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void wire();
    void onOmpReady();
    void onAttention(Workspace *w, const QString &kind, const QString &title, const QString &body);

    Options m_options;
    bool m_mock = false;
    bool m_started = false;
    bool m_ompLoaded = false;
    Preferences *m_prefs;
    OmpLocator *m_locator;
    OmpCli *m_cli;
    SettingsSchema m_schema;
    ConfigStore *m_config;
    WorkspaceManager *m_workspaces;
    SessionIndex *m_sessions;
    SettingsModel *m_settings;
    ProviderStore *m_providers;
    McpStore *m_mcp;
    ModelCatalog *m_models;
    PaletteModel *m_palette;
    ShortcutModel *m_shortcuts;
    EffectsProbe *m_effects;
    Notifier *m_notifier;
    syntax::Palette m_syntax;
    md::Style m_mdStyle;
    mutable QCache<QString, QString> m_mdCache{300};
};

} // namespace gomp
