#include "ProviderStore.h"

#include "ConfigStore.h"
#include "adapter/OmpCli.h"
#include "adapter/RpcBackend.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QVariantMap>

using namespace Qt::StringLiterals;

namespace gomp {

namespace {

QString keySource(const QString &apiKey)
{
    if (apiKey.isEmpty())
        return u"none"_s;
    if (Keyring::isGompLookupCommand(apiKey))
        return u"keyring"_s;
    if (apiKey.startsWith(u'!'))
        return u"command"_s;
    static const QRegularExpression envName(u"^[A-Z][A-Z0-9_]*$"_s);
    if (envName.match(apiKey).hasMatch())
        return u"env"_s;
    return u"plaintext"_s;
}

QString envKeyring(const QString &envVar)
{
    return u"env:"_s + envVar;
}

} // namespace

ProviderStore::ProviderStore(OmpCli *cli, QObject *parent)
    : QObject(parent)
    , m_cli(cli)
{
}

ProviderStore::~ProviderStore()
{
    if (m_auth)
        m_auth->stop();
}

QString ProviderStore::modelsPath()
{
    const auto env = qEnvironmentVariable("PI_CONFIG_DIR");
    return (env.isEmpty() ? QDir::homePath() + u"/.omp/agent"_s : env) + u"/models.yml"_s;
}

QList<QPair<QString, QString>> ProviderStore::parseEnvHelp(const QString &help)
{
    static const QRegularExpression line(uR"(^\s+([A-Z][A-Z0-9_]*_(?:API_KEY|TOKEN|ACCESS_TOKEN))\s+-\s+(.+)$)"_s,
                                         QRegularExpression::MultilineOption);
    QList<QPair<QString, QString>> out;
    auto it = line.globalMatch(help);
    while (it.hasNext()) {
        const auto m = it.next();
        out.append({m.captured(1), m.captured(2).trimmed()});
    }
    return out;
}

void ProviderStore::load(const QStringList &storedEnvKeys)
{
    readModels();
    m_cli->run({u"--help"_s}, [this](bool, const QString &out, const QString &) {
        m_envVars = parseEnvHelp(out);
        // Without omp (demo mode, or an omp that changed its help text) show the major providers.
        if (m_envVars.isEmpty()) {
            m_envVars = {{u"ANTHROPIC_API_KEY"_s, u"Anthropic Claude models"_s},
                         {u"OPENAI_API_KEY"_s, u"OpenAI GPT models"_s},
                         {u"GEMINI_API_KEY"_s, u"Google Gemini models"_s},
                         {u"OPENROUTER_API_KEY"_s, u"OpenRouter aggregated models"_s},
                         {u"XAI_API_KEY"_s, u"xAI Grok models"_s},
                         {u"MISTRAL_API_KEY"_s, u"Mistral models"_s},
                         {u"GROQ_API_KEY"_s, u"Groq models"_s}};
        }
        emit changed();
    });
    m_pendingLookups = int(storedEnvKeys.size());
    if (m_pendingLookups == 0 || !Keyring::available()) {
        m_ready = true;
        emit readyChanged();
        return;
    }
    for (const auto &env : storedEnvKeys) {
        m_keyring.lookup(envKeyring(env), [this, env](bool ok, const QString &secret) {
            if (ok && !secret.isEmpty())
                m_envSecrets.insert(env, secret);
            if (--m_pendingLookups == 0) {
                m_ready = true;
                emit readyChanged();
                emit environmentChanged();
                emit changed();
            }
        });
    }
}

QVariantList ProviderStore::envProviders() const
{
    QVariantList out;
    const auto sys = QProcessEnvironment::systemEnvironment();
    for (const auto &[name, description] : m_envVars) {
        QString source = u"none"_s;
        if (m_envSecrets.contains(name))
            source = u"keyring"_s;
        else if (sys.contains(name))
            source = u"environment"_s;
        out << QVariantMap{{u"envVar"_s, name},
                           {u"description"_s, description},
                           {u"source"_s, source},
                           {u"search"_s, name.contains("SEARCH"_L1) || description.contains("search"_L1)}};
    }
    return out;
}

void ProviderStore::readModels()
{
    m_custom.clear();
    QFile f(modelsPath());
    if (!f.open(QIODevice::ReadOnly)) {
        emit changed();
        return;
    }
    const auto parsed = yaml::parse(QString::fromUtf8(f.readAll()));
    if (!parsed.ok) {
        emit notice(u"warning"_s, u"models.yml could not be read: %1 (line %2)."_s.arg(parsed.error).arg(parsed.errorLine));
        emit changed();
        return;
    }
    const auto *providers = parsed.root.find(u"providers"_s);
    if (providers && providers->type == yaml::Node::Type::Map) {
        for (const auto &[name, node] : providers->map) {
            const auto *key = node.find(u"apiKey"_s);
            const auto *url = node.find(u"baseUrl"_s);
            const auto *api = node.find(u"api"_s);
            // The secret itself never leaves C++; QML only learns where it is kept.
            m_custom << QVariantMap{{u"name"_s, name},
                                    {u"baseUrl"_s, url ? url->scalar : QString{}},
                                    {u"api"_s, api ? api->scalar : QString{}},
                                    {u"keySource"_s, keySource(key ? key->scalar : QString{})},
                                    {u"envVar"_s, key && keySource(key->scalar) == "env"_L1 ? key->scalar : QString{}}};
        }
    }
    emit changed();
}

QVariantList ProviderStore::customProviders() const
{
    return m_custom;
}

int ProviderStore::plaintextKeyCount() const
{
    int n = 0;
    for (const auto &p : m_custom)
        n += p.toMap().value(u"keySource"_s).toString() == "plaintext"_L1 ? 1 : 0;
    return n;
}

QProcessEnvironment ProviderStore::environment() const
{
    auto env = QProcessEnvironment::systemEnvironment();
    for (auto it = m_envSecrets.cbegin(); it != m_envSecrets.cend(); ++it)
        env.insert(it.key(), it.value());
    return env;
}

void ProviderStore::setEnvKey(const QString &envVar, const QString &secret)
{
    const auto trimmed = secret.trimmed();
    if (trimmed.isEmpty())
        return;
    m_keyring.store(envKeyring(envVar), trimmed, [this, envVar, trimmed](bool ok, const QString &err) {
        if (!ok) {
            emit notice(u"error"_s, u"Could not save the key: %1"_s.arg(err));
            return;
        }
        m_envSecrets.insert(envVar, trimmed);
        emit changed();
        emit environmentChanged();
        emit notice(u"info"_s, u"%1 saved to the keyring. New and restarted workspaces use it."_s.arg(envVar));
    });
}

void ProviderStore::clearEnvKey(const QString &envVar)
{
    m_keyring.remove(envKeyring(envVar), [this, envVar](bool, const QString &) {
        m_envSecrets.remove(envVar);
        emit changed();
        emit environmentChanged();
        emit notice(u"info"_s, u"%1 removed from the keyring."_s.arg(envVar));
    });
}

bool ProviderStore::editModels(const std::function<bool(yaml::Node &, QString *)> &edit, QString *error)
{
    const auto path = modelsPath();
    QString text;
    {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            text = QString::fromUtf8(f.readAll());
    }
    auto parsed = yaml::parse(text);
    if (!parsed.ok) {
        *error = u"models.yml uses YAML gomp cannot edit safely (%1, line %2)."_s.arg(parsed.error).arg(parsed.errorLine);
        return false;
    }
    if (!edit(parsed.root, error))
        return false;
    if (ConfigStore::backupFile(path).isEmpty()) {
        *error = u"Could not back up models.yml; nothing was changed."_s;
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        *error = out.errorString();
        return false;
    }
    out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    out.write(yaml::serialize(parsed.root).toUtf8());
    if (!out.commit()) {
        *error = out.errorString();
        return false;
    }
    readModels();
    return true;
}

void ProviderStore::moveKeyToKeyring(const QString &providerName)
{
    QFile f(modelsPath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const auto parsed = yaml::parse(QString::fromUtf8(f.readAll()));
    const auto *key = parsed.ok ? parsed.root.atPath({u"providers"_s, providerName, u"apiKey"_s}) : nullptr;
    if (!key || keySource(key->scalar) != "plaintext"_L1)
        return;
    setCustomKey(providerName, key->scalar);
}

void ProviderStore::setCustomKey(const QString &providerName, const QString &secret)
{
    const auto trimmed = secret.trimmed();
    if (trimmed.isEmpty())
        return;
    m_keyring.store(providerName, trimmed, [this, providerName](bool ok, const QString &err) {
        if (!ok) {
            emit notice(u"error"_s, u"Could not save the key: %1"_s.arg(err));
            return;
        }
        QString error;
        const bool edited = editModels(
            [&](yaml::Node &root, QString *) {
                root.setPath({u"providers"_s, providerName, u"apiKey"_s},
                             yaml::Node{yaml::Node::Type::Scalar, Keyring::lookupCommand(providerName), true, {}, {}});
                return true;
            },
            &error);
        if (edited)
            emit notice(u"info"_s, u"%1's key is in the keyring. models.yml now asks the keyring for it."_s.arg(providerName));
        else
            emit notice(u"error"_s, error);
    });
}

void ProviderStore::addCustomProvider(const QString &name, const QString &baseUrl, const QString &api,
                                      const QString &secret)
{
    static const QRegularExpression valid(u"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$"_s);
    if (!valid.match(name).hasMatch()) {
        emit notice(u"error"_s, u"Provider names use letters, digits, dot, dash and underscore."_s);
        return;
    }
    const QUrl url(baseUrl);
    if (!url.isValid() || !(url.scheme() == "https"_L1 || url.scheme() == "http"_L1)) {
        emit notice(u"error"_s, u"The base URL must start with https:// or http://."_s);
        return;
    }
    QString error;
    const bool ok = editModels(
        [&](yaml::Node &root, QString *err) {
            if (root.atPath({u"providers"_s, name})) {
                *err = u"A provider named %1 already exists."_s.arg(name);
                return false;
            }
            auto scalar = [](const QString &s) { return yaml::Node{yaml::Node::Type::Scalar, s, true, {}, {}}; };
            root.setPath({u"providers"_s, name, u"baseUrl"_s}, scalar(baseUrl));
            root.setPath({u"providers"_s, name, u"api"_s}, scalar(api.isEmpty() ? u"openai-completions"_s : api));
            root.setPath({u"providers"_s, name, u"discovery"_s, u"type"_s}, scalar(u"openai-models-list"_s));
            return true;
        },
        &error);
    if (!ok) {
        emit notice(u"error"_s, error);
        return;
    }
    if (!secret.trimmed().isEmpty())
        setCustomKey(name, secret);
    else
        emit notice(u"info"_s, u"Added %1."_s.arg(name));
}

void ProviderStore::removeCustomProvider(const QString &name)
{
    QString error;
    const bool ok = editModels([&](yaml::Node &root, QString *) { return root.removePath({u"providers"_s, name}); },
                               &error);
    if (!ok) {
        emit notice(u"error"_s, error.isEmpty() ? u"%1 was not found in models.yml."_s.arg(name) : error);
        return;
    }
    m_keyring.remove(name, [](bool, const QString &) {});
    emit notice(u"info"_s, u"Removed %1. A backup of models.yml was kept."_s.arg(name));
}

void ProviderStore::startAuthSession(const std::function<void()> &then)
{
    if (m_auth && m_auth->isRunning()) {
        then();
        return;
    }
    if (m_auth)
        m_auth->deleteLater();
    m_auth = new RpcBackend(this);
    connect(m_auth, &AgentBackend::agentEvent, this, [this, then](const AgentEvent &ev) {
        if (std::holds_alternative<ReadyEvent>(ev)) {
            then();
        } else if (const auto *ui = std::get_if<UiRequestEvent>(&ev)) {
            if (ui->kind == UiRequestEvent::Kind::OpenUrl) {
                m_loginState = u"browser"_s;
                emit loginChanged();
                emit loginUrl(ui->url);
                QDesktopServices::openUrl(QUrl(ui->url));
            } else if (ui->kind == UiRequestEvent::Kind::Input || ui->kind == UiRequestEvent::Kind::Editor) {
                m_loginRequestId = ui->requestId;
                m_loginState = u"input"_s;
                emit loginChanged();
                emit loginPrompt(ui->title.isEmpty() ? ui->message : ui->title, ui->placeholder);
            } else if (ui->kind == UiRequestEvent::Kind::Select && !ui->options.isEmpty()) {
                // Account pickers and similar: take omp's first (default) choice.
                m_auth->answerUi(ui->requestId, {{"value"_L1, ui->options.constFirst()}});
            } else if (ui->kind == UiRequestEvent::Kind::Confirm) {
                m_auth->answerUi(ui->requestId, {{"confirmed"_L1, true}});
            }
        } else if (const auto *p = std::get_if<ProcessEvent>(&ev);
                   p && (p->kind == ProcessEvent::Kind::FailedToStart || p->kind == ProcessEvent::Kind::Crashed)) {
            finishLogin(u"failed"_s);
        }
    });
    auto tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    LaunchOptions o;
    o.ompPath = m_cli->program();
    o.cwd = tmp;
    o.env = environment();
    o.extraArgs = {u"--no-session"_s, u"--no-extensions"_s, u"--no-skills"_s, u"--no-lsp"_s};
    m_auth->start(o);
}

void ProviderStore::refreshOAuth()
{
    startAuthSession([this] {
        m_auth->call(u"get_login_providers"_s, {}, [this](bool ok, const QJsonValue &data, const QString &) {
            if (!ok)
                return;
            m_oauth.clear();
            for (const auto &pv : data.toArray()) {
                const auto p = pv.toObject();
                m_oauth << QVariantMap{{u"id"_s, p.value("id"_L1).toString()},
                                       {u"name"_s, p.value("name"_L1).toString()},
                                       {u"available"_s, p.value("available"_L1).toBool()},
                                       {u"authenticated"_s, p.value("authenticated"_L1).toBool()}};
            }
            emit changed();
        });
    });
}

void ProviderStore::loadDemoOAuth()
{
    QFile f(u":/gomp/mock/providers.json"_s);
    const auto list = f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).array() : QJsonArray{};
    m_oauth.clear();
    for (const auto &v : list) {
        const auto p = v.toObject();
        m_oauth << QVariantMap{{u"id"_s, p.value("id"_L1).toString()},
                               {u"name"_s, p.value("name"_L1).toString()},
                               {u"available"_s, true},
                               {u"authenticated"_s, p.value("authenticated"_L1).toBool()}};
    }
    emit changed();
}

void ProviderStore::login(const QString &providerId)
{
    m_loginProvider = providerId;
    m_loginState = u"starting"_s;
    emit loginChanged();
    startAuthSession([this, providerId] {
        m_auth->call(u"login"_s, {{"providerId"_L1, providerId}}, [this](bool ok, const QJsonValue &, const QString &err) {
            if (!ok)
                emit notice(u"error"_s, u"Login failed: %1"_s.arg(err));
            finishLogin(ok ? u"done"_s : u"failed"_s);
            refreshOAuth();
        });
    });
}

void ProviderStore::answerLogin(const QString &text)
{
    if (!m_auth || m_loginRequestId.isEmpty())
        return;
    m_auth->answerUi(m_loginRequestId, {{"value"_L1, text}});
    m_loginRequestId.clear();
    m_loginState = u"browser"_s;
    emit loginChanged();
}

void ProviderStore::cancelLogin()
{
    if (m_auth && !m_loginRequestId.isEmpty())
        m_auth->answerUi(m_loginRequestId, {{"cancelled"_L1, true}});
    m_loginRequestId.clear();
    finishLogin(u"idle"_s);
}

void ProviderStore::finishLogin(const QString &state)
{
    m_loginState = state;
    emit loginChanged();
}

} // namespace gomp
