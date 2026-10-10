#pragma once

#include "util/Keyring.h"
#include "util/YamlLite.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QProcessEnvironment>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace gomp {

class OmpCli;
class RpcBackend;

// Providers and their credentials.
//  * Built-in providers read keys from env vars (ANTHROPIC_API_KEY, ...). gomp keeps those keys in
//    the keyring and injects them into omp's environment at spawn; they never touch disk.
//  * Custom providers live in ~/.omp/agent/models.yml. A plaintext `apiKey` there can be moved to
//    the keyring in one step: the value becomes `!secret-tool lookup ...`, which omp executes.
//  * OAuth providers log in through omp's own flow over a short-lived RPC session.
class ProviderStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(QVariantList envProviders READ envProviders NOTIFY changed)
    Q_PROPERTY(QVariantList customProviders READ customProviders NOTIFY changed)
    Q_PROPERTY(QVariantList oauthProviders READ oauthProviders NOTIFY changed)
    Q_PROPERTY(bool keyringAvailable READ keyringAvailable CONSTANT)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(int plaintextKeyCount READ plaintextKeyCount NOTIFY changed)
    Q_PROPERTY(QString loginState READ loginState NOTIFY loginChanged)
    Q_PROPERTY(QString loginProvider READ loginProvider NOTIFY loginChanged)
public:
    ProviderStore(OmpCli *cli, QObject *parent = nullptr);
    ~ProviderStore() override;

    void load(const QStringList &storedEnvKeys);
    [[nodiscard]] bool ready() const { return m_ready; }
    [[nodiscard]] QVariantList envProviders() const;
    [[nodiscard]] QVariantList customProviders() const;
    [[nodiscard]] QVariantList oauthProviders() const { return m_oauth; }
    [[nodiscard]] bool keyringAvailable() const { return Keyring::available(); }
    [[nodiscard]] int plaintextKeyCount() const;
    [[nodiscard]] QStringList storedEnvKeys() const { return m_envSecrets.keys(); }

    // Environment for omp processes: system env plus keyring-held keys.
    [[nodiscard]] QProcessEnvironment environment() const;

    Q_INVOKABLE void setEnvKey(const QString &envVar, const QString &secret);
    Q_INVOKABLE void clearEnvKey(const QString &envVar);
    Q_INVOKABLE void moveKeyToKeyring(const QString &providerName);
    Q_INVOKABLE void setCustomKey(const QString &providerName, const QString &secret);
    Q_INVOKABLE void addCustomProvider(const QString &name, const QString &baseUrl, const QString &api,
                                       const QString &secret);
    Q_INVOKABLE void removeCustomProvider(const QString &name);

    Q_INVOKABLE void refreshOAuth();
    Q_INVOKABLE void loadDemoOAuth(); // demo mode: a fixed sample, nothing is contacted
    Q_INVOKABLE void login(const QString &providerId);
    Q_INVOKABLE void answerLogin(const QString &text);
    Q_INVOKABLE void cancelLogin();
    [[nodiscard]] QString loginState() const { return m_loginState; }
    [[nodiscard]] QString loginProvider() const { return m_loginProvider; }

    [[nodiscard]] static QString modelsPath();
    // Parses omp --help "Environment Variables" lines into {envVar, description}. Public for tests.
    static QList<QPair<QString, QString>> parseEnvHelp(const QString &helpText);

signals:
    void changed();
    void readyChanged();
    void environmentChanged();
    void notice(const QString &level, const QString &message);
    void loginChanged();
    void loginPrompt(const QString &title, const QString &placeholder);
    void loginUrl(const QString &url);

private:
    void readModels();
    bool editModels(const std::function<bool(yaml::Node &root, QString *error)> &edit, QString *error);
    void startAuthSession(const std::function<void()> &then);
    void finishLogin(const QString &state);

    OmpCli *m_cli;
    Keyring m_keyring;
    QList<QPair<QString, QString>> m_envVars; // name, description
    QHash<QString, QString> m_envSecrets;     // env var -> secret (memory only)
    QVariantList m_custom;
    QVariantList m_oauth;
    bool m_ready = false;
    int m_pendingLookups = 0;
    QPointer<RpcBackend> m_auth;
    QString m_loginState = QStringLiteral("idle"); // idle | starting | browser | input | done | failed
    QString m_loginProvider;
    QString m_loginRequestId;
};

} // namespace gomp
