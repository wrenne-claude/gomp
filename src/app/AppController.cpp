#include "AppController.h"

#include "util/ProviderLogoProvider.h"

#include "adapter/MockBackend.h"
#include "adapter/OmpBackend.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QLoggingCategory>
#include <QMimeData>
#include <QPalette>
#include <QStandardPaths>
#include <QStyleHints>
#include <QUrl>
#include <QUuid>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

Q_LOGGING_CATEGORY(lcApp, "gomp.app")

using namespace Qt::StringLiterals;

namespace gomp {

namespace {
AppController *s_instance = nullptr;
}

AppController::AppController(const Options &options, QObject *parent)
    : QObject(parent)
    , m_options(options)
    , m_mock(options.mock)
    , m_prefs(new Preferences(this))
    , m_locator(new OmpLocator(this))
    , m_cli(new OmpCli(this))
    , m_config(new ConfigStore(m_cli, &m_schema, this))
    , m_workspaces(new WorkspaceManager(this))
    , m_sessions(new SessionIndex(m_cli, this))
    , m_providers(new ProviderStore(m_cli, this))
    , m_mcp(new McpStore(this))
    , m_models(new ModelCatalog(m_cli, this))
    , m_shortcuts(nullptr)
    , m_effects(new EffectsProbe(this))
    , m_notifier(new Notifier(this))
{
    s_instance = this;
    if (!m_options.screenshotDir.isEmpty())
        m_prefs->setVolatile(true); // screenshots must not depend on, or change, the user's setup
    m_schema.loadBundled();
    m_settings = new SettingsModel(m_config, &m_schema, this);
    m_palette = new PaletteModel(&m_schema, m_sessions, m_models, this);
    m_shortcuts = new ShortcutModel(m_prefs, this);
    m_syntax = syntax::Palette{u"#E7ECF3"_s, u"#FFB547"_s, u"#8FA7FF"_s, u"#9FD8A8"_s, u"#F2D35B"_s, u"#74829A"_s,
                               u"#6CB4FF"_s, u"#A3AFBF"_s, u"#5BD49B"_s, u"#FF6B6B"_s, u"#8FA7FF"_s};
    wire();
    qApp->installEventFilter(this);
}

AppController::~AppController()
{
    s_instance = nullptr;
}

AppController *AppController::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

AppController *AppController::instance()
{
    return s_instance;
}

// modelRoles.default without a trailing thinking level ("provider/id:high" -> "provider/id"). Ids can
// contain ':' themselves ("z-ai/glm-5.3-flash:batch"), so only known levels are stripped.
void AppController::applyApprovalPolicy(Workspace *w) const
{
    const auto mode = m_config->liveValue(u"tools.approvalMode"_s).toString();
    w->setApprovalPolicy(mode.isEmpty() ? u"yolo"_s : mode, m_config->liveValue(u"tools.approval"_s).toObject().toVariantMap());
}

QString AppController::defaultModelSelector() const
{
    auto selector = m_config->liveValue(u"modelRoles"_s).toObject().value("default"_L1).toString();
    static const QStringList levels{u"off"_s, u"minimal"_s, u"low"_s, u"medium"_s, u"high"_s,
                                    u"xhigh"_s, u"max"_s, u"auto"_s};
    if (const auto colon = selector.lastIndexOf(u':'); colon > 0 && levels.contains(selector.mid(colon + 1)))
        selector.truncate(colon);
    return selector;
}

void AppController::wire()
{
    m_workspaces->setFactories(
        [](bool mock) -> AgentBackend * {
            if (mock)
                return new MockBackend;
            return new OmpBackend;
        },
        [this](const QString &cwd) {
            LaunchOptions o;
            o.ompPath = m_locator->path();
            o.cwd = cwd;
            o.env = m_providers->environment();
            return o;
        });

    connect(m_workspaces, &WorkspaceManager::workspaceAdded, this, [this](Workspace *w) {
        if (!w->isMock()) {
            w->setExpectedModel(defaultModelSelector());
            applyApprovalPolicy(w);
        }
        connect(w, &Workspace::attention, this,
                [this, w](const QString &kind, const QString &title, const QString &body) { onAttention(w, kind, title, body); });
        connect(w, &Workspace::toast, this, &AppController::toast);
    });
    connect(m_workspaces, &WorkspaceManager::listChanged, this, [this] {
        if (!m_mock && m_options.screenshotDir.isEmpty())
            m_prefs->setOpenWorkspaces(m_workspaces->snapshot());
    });
    connect(m_workspaces, &WorkspaceManager::currentChanged, this, [this] {
        if (auto *w = m_workspaces->current(); w && !w->isMock())
            setProjectScope(w->cwd());
    });

    connect(m_locator, &OmpLocator::changed, this, [this] {
        emit needsFirstRunChanged();
        if (m_locator->status() == OmpLocator::Status::Found || m_locator->status() == OmpLocator::Status::Untested)
            onOmpReady();
    });
    connect(m_providers, &ProviderStore::environmentChanged, this, [this] {
        m_cli->setEnvironment(m_providers->environment());
        m_prefs->setStoredEnvKeys(m_providers->storedEnvKeys());
    });
    connect(m_providers, &ProviderStore::notice, this, &AppController::toast);
    connect(m_sessions, &SessionIndex::notice, this, &AppController::toast);
    connect(m_sessions, &SessionIndex::countChanged, m_palette, &PaletteModel::rebuild);
    connect(m_models, &ModelCatalog::loaded, m_palette, &PaletteModel::rebuild);
    connect(m_settings, &SettingsModel::applied, this, [this](bool ok, const QString &message) {
        emit toast(ok ? u"info"_s : u"error"_s, message);
    });
    connect(m_config, &ConfigStore::loaded, m_palette, &PaletteModel::rebuild);
    connect(m_config, &ConfigStore::loaded, this, [this] {
        for (int i = 0; i < m_workspaces->rowCount(); ++i) {
            if (auto *w = m_workspaces->at(i); w && !w->isMock()) {
                w->setExpectedModel(defaultModelSelector());
                applyApprovalPolicy(w);
            }
        }
    });
    connect(m_notifier, &Notifier::activated, this, [this](const QString &key) {
        bool ok = false;
        const int idx = key.toInt(&ok);
        if (ok)
            m_workspaces->setCurrentIndex(idx);
        emit raiseRequested();
    });
    connect(qGuiApp->styleHints(), &QStyleHints::colorSchemeChanged, this, &AppController::systemPaletteChanged);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, &AppController::windowActiveChanged);
}

bool AppController::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)
    if (event->type() == QEvent::ApplicationPaletteChange)
        emit systemPaletteChanged();
    return false;
}

void AppController::start()
{
    if (m_started)
        return;
    m_started = true;
    if (m_mock) {
        const auto demo = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + u"/gomp-demo-sessions"_s;
        QDir(demo).removeRecursively();
        MockBackend::writeDemoSessions(demo);
        m_sessions->setRoot(demo);
        m_sessions->rescan();
        m_config->setDemo(true);
        m_mcp->setDemo(true);
        m_config->reload();
        m_providers->load({});
        m_models->setModels(MockBackend::sampleModels());
        openDemo();
        return;
    }
    m_locator->locate(m_prefs->ompPath());
}

void AppController::onOmpReady()
{
    m_cli->setProgram(m_locator->path());
    if (m_ompLoaded)
        return;
    m_ompLoaded = true;
    m_providers->load(m_prefs->storedEnvKeys());
    m_cli->setEnvironment(m_providers->environment());
    m_config->reload();
    m_models->refresh();
    m_sessions->rescan();
    if (needsFirstRun())
        return;
    // Keyring lookups must finish before spawning omp, or sessions would start without keys.
    auto restore = [this] {
        if (!m_options.openProject.isEmpty()) {
            openProject(m_options.openProject);
            return;
        }
        for (const auto &v : m_prefs->openWorkspaces()) {
            const auto m = v.toMap();
            const auto cwd = m.value(u"cwd"_s).toString();
            if (!QFileInfo(cwd).isDir())
                continue;
            m_workspaces->open(cwd, m.value(u"sessionFile"_s).toString());
        }
        if (m_workspaces->rowCount() > 0)
            m_workspaces->setCurrentIndex(0);
    };
    if (m_providers->ready())
        restore();
    else
        connect(m_providers, &ProviderStore::readyChanged, this, restore, Qt::SingleShotConnection);
}

bool AppController::needsFirstRun() const
{
    if (m_mock)
        return false;
    const auto s = m_locator->status();
    return !m_prefs->firstRunDone() || s == OmpLocator::Status::Missing || s == OmpLocator::Status::Broken;
}

void AppController::completeFirstRun()
{
    m_prefs->setOmpPath(m_locator->path());
    m_prefs->setFirstRunDone(true);
    emit needsFirstRunChanged();
}

void AppController::enterDemoMode()
{
    m_mock = true;
    emit mockModeChanged();
    const auto demo = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + u"/gomp-demo-sessions"_s;
    QDir(demo).removeRecursively();
    MockBackend::writeDemoSessions(demo);
    m_sessions->setRoot(demo);
    m_sessions->rescan();
    m_config->setDemo(true);
    m_mcp->setDemo(true);
    m_config->reload();
    emit needsFirstRunChanged();
    m_models->setModels(MockBackend::sampleModels());
    openDemo();
}

int AppController::openProject(const QString &dirOrUrl, const QString &initialMode)
{
    const auto dir = toLocalPath(dirOrUrl);
    if (!QFileInfo(dir).isDir()) {
        emit toast(u"error"_s, u"%1 is not a folder."_s.arg(dir));
        return -1;
    }
    m_prefs->addRecentProject(dir);
    return m_workspaces->open(dir, {}, m_mock, initialMode);
}

int AppController::openSession(const QString &sessionPath, const QString &cwd)
{
    const auto existing = m_workspaces->indexOfSession(sessionPath);
    if (existing >= 0) {
        m_workspaces->setCurrentIndex(existing);
        return existing;
    }
    const auto dir = QFileInfo(cwd).isDir() ? cwd : QDir::homePath();
    return m_workspaces->open(dir, sessionPath, m_mock);
}

int AppController::openDemo(const QString &scenario)
{
    const auto dir = QDir::homePath() + u"/Projects/gomp"_s;
    const int idx = m_workspaces->open(QFileInfo(dir).isDir() ? dir : QDir::homePath(), {}, true);
    if (!scenario.isEmpty()) {
        if (auto *w = m_workspaces->at(idx)) {
            if (auto *mock = qobject_cast<MockBackend *>(w->backend()))
                QMetaObject::invokeMethod(mock, [mock, scenario] { mock->playScenario(scenario); }, Qt::QueuedConnection);
        }
    }
    return idx;
}

void AppController::setProjectScope(const QString &dir)
{
    m_config->setProjectDir(dir);
    m_mcp->setProjectDir(dir);
}

void AppController::onAttention(Workspace *w, const QString &kind, const QString &title, const QString &body)
{
    if (!m_prefs->notifications())
        return;
    if (kind == "done"_L1 && !m_prefs->notifyOnFinish())
        return;
    // Only interrupt when the user is elsewhere; in-app states already show it.
    if (windowActive() && m_workspaces->current() == w)
        return;
    const auto idx = m_workspaces->workspaces().indexOf(w);
    m_notifier->notify(title, body, QString::number(idx), kind == "approval"_L1 || kind == "plan"_L1);
}

QColor AppController::systemAccent() const
{
    return qGuiApp->palette().color(QPalette::Accent);
}

bool AppController::systemDark() const
{
    const auto scheme = qGuiApp->styleHints()->colorScheme();
    if (scheme != Qt::ColorScheme::Unknown)
        return scheme == Qt::ColorScheme::Dark;
    return qGuiApp->palette().color(QPalette::Window).lightness() < 128;
}

QString AppController::version() const
{
    return QCoreApplication::applicationVersion();
}

bool AppController::windowActive() const
{
    return qGuiApp->applicationState() == Qt::ApplicationActive;
}

QString AppController::highlight(const QString &code, const QString &language) const
{
    return syntax::toHtml(code, language, m_syntax);
}

void AppController::setSyntaxPalette(const QVariantMap &c)
{
    auto get = [&c](const char *key, const QString &fallback) { return c.value(QLatin1StringView(key), fallback).toString(); };
    m_syntax = syntax::Palette{get("text", m_syntax.text),         get("keyword", m_syntax.keyword),
                               get("type", m_syntax.type),         get("string", m_syntax.string),
                               get("number", m_syntax.number),     get("comment", m_syntax.comment),
                               get("function", m_syntax.function), get("punctuation", m_syntax.punctuation),
                               get("added", m_syntax.added),       get("removed", m_syntax.removed),
                               get("meta", m_syntax.meta)};
    m_mdStyle = md::Style{get("text", m_mdStyle.text), get("secondary", m_mdStyle.secondary), get("accent", m_mdStyle.link),
                          get("codeText", m_mdStyle.codeText), get("codeBg", m_mdStyle.codeBg),
                          get("hairline", m_mdStyle.hairline), get("headerBg", m_mdStyle.headerBg),
                          m_mdStyle.uiFamily, m_mdStyle.monoFamily};
    m_mdCache.clear();
    const auto accent = get("accent", u"#FFB547"_s);
    m_models->setHighlightColor(accent);
    m_palette->setHighlightColor(accent);
}

QString AppController::renderMarkdown(const QString &markdown) const
{
    if (const auto *hit = m_mdCache.object(markdown))
        return *hit;
    const auto html = md::toHtml(markdown, m_mdStyle);
    m_mdCache.insert(markdown, new QString(html), int(markdown.size() / 64) + 1);
    return html;
}

QVariantList AppController::markdownSegments(const QString &markdown) const
{
    return md::toVariant(md::split(markdown));
}

void AppController::copyText(const QString &text) const
{
    QGuiApplication::clipboard()->setText(text);
}

void AppController::openUrl(const QString &url) const
{
    QDesktopServices::openUrl(QUrl(url));
}

void AppController::revealPath(const QString &path) const
{
    const QFileInfo fi(toLocalPath(path));
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath()));
}

QString AppController::providerLogo(const QString &providerId) const
{
    if (providerId.isEmpty() || providerId.contains(u'/') || providerId.contains(u'\\'))
        return {};
    // Cached: a logo that exists but draws nothing (an SVG that relies on masks or filters) is
    // treated as missing, so the tile shows initials instead of a blank square.
    static QHash<QString, bool> drawable;
    auto it = drawable.find(providerId);
    if (it == drawable.end())
        it = drawable.insert(providerId, !ProviderLogoProvider::load(providerId, QSize(32, 32)).isNull());
    return it.value() ? providerId : QString{};
}

QString AppController::toLocalPath(const QString &url) const
{
    if (url.startsWith("file:"_L1))
        return QUrl(url).toLocalFile();
    if (url.startsWith(u'~'))
        return QDir::homePath() + url.mid(1);
    return url;
}

bool AppController::isDirectory(const QString &path) const
{
    return QFileInfo(toLocalPath(path)).isDir();
}

QString AppController::homePath() const
{
    return QDir::homePath();
}

void AppController::ompJson(const QStringList &args, const QJSValue &callback)
{
    auto cb = callback;
    auto *engine = qjsEngine(this);
    if (m_mock) {
        QJsonDocument doc;
        if (args.value(0) == "plugin"_L1) {
            doc = QJsonDocument(QJsonObject{
                {"npm"_L1, QJsonArray{QJsonObject{{"name"_L1, u"@omp/plugin-github"_s}, {"version"_L1, u"2.3.0"_s}, {"enabled"_L1, true},
                                                  {"description"_L1, u"Pull requests, issues and checks as tools"_s}},
                                      QJsonObject{{"name"_L1, u"@omp/plugin-sentry"_s}, {"version"_L1, u"0.8.1"_s}, {"enabled"_L1, false},
                                                  {"description"_L1, u"Read Sentry issues and stack traces"_s}}}},
                {"marketplace"_L1, QJsonArray{}}});
        } else if (args.value(0) == "skill"_L1) {
            doc = QJsonDocument(QJsonObject{
                {"skills"_L1, QJsonArray{QJsonObject{{"name"_L1, u"pdf-tools"_s}, {"version"_L1, u"1.4.0"_s},
                                                     {"description"_L1, u"Extract text and tables from PDFs"_s}},
                                         QJsonObject{{"name"_L1, u"release-notes"_s}, {"version"_L1, u"0.3.2"_s},
                                                     {"description"_L1, u"Draft release notes from merged pull requests"_s}}}},
                {"warnings"_L1, QJsonArray{}}});
        }
        QMetaObject::invokeMethod(this, [cb, doc, engine]() mutable {
            if (cb.isCallable() && engine)
                cb.call({true, engine->toScriptValue(doc.isObject() ? doc.object().toVariantMap() : QVariantMap{}), QString{}});
        }, Qt::QueuedConnection);
        return;
    }
    m_cli->runJson(args, [cb, engine](bool ok, const QJsonDocument &doc, const QString &error) mutable {
        if (!cb.isCallable() || !engine)
            return;
        const QVariant data = doc.isArray() ? QVariant(doc.array().toVariantList()) : QVariant(doc.object().toVariantMap());
        cb.call({ok, engine->toScriptValue(data), error});
    });
}

void AppController::ompRun(const QStringList &args, const QJSValue &callback)
{
    auto cb = callback;
    if (m_mock) {
        QMetaObject::invokeMethod(this, [cb]() mutable {
            if (cb.isCallable())
                cb.call({true, u"Done (demo mode: nothing was changed)."_s});
        }, Qt::QueuedConnection);
        return;
    }
    m_cli->run(args, [cb](bool ok, const QString &out, const QString &err) mutable {
        if (cb.isCallable())
            cb.call({ok, (ok ? out : err).trimmed()});
    });
}

QString AppController::pasteImage() const
{
    const auto *mime = QGuiApplication::clipboard()->mimeData();
    if (!mime || !mime->hasImage())
        return {};
    const auto image = qvariant_cast<QImage>(mime->imageData());
    if (image.isNull())
        return {};
    const auto dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/pasted"_s;
    QDir().mkpath(dir);
    const auto path = dir + u'/' + QUuid::createUuid().toString(QUuid::WithoutBraces) + u".png"_s;
    return image.save(path, "PNG") ? path : QString{};
}

} // namespace gomp
