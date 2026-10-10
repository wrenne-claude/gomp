#pragma once

#include <QObject>
#include <QString>

#include <functional>

namespace gomp {

// Secret Service keyring access through `secret-tool` (libsecret). On KDE Plasma 6, KWallet
// implements the Secret Service API, so this covers GNOME Keyring and KWallet alike.
// Secrets travel over the child's stdin, never argv, and are never logged.
// Items are stored with attributes {service: gomp, provider: <name>}.
class Keyring : public QObject
{
    Q_OBJECT
public:
    using Done = std::function<void(bool ok, const QString &errorOrSecret)>;

    explicit Keyring(QObject *parent = nullptr);

    [[nodiscard]] static bool available();
    void store(const QString &provider, const QString &secret, Done done);
    void lookup(const QString &provider, Done done);
    void remove(const QString &provider, Done done);

    // The `apiKey` value omp runs to fetch this provider's key (omp executes values that start
    // with '!' as shell commands).
    [[nodiscard]] static QString lookupCommand(const QString &provider);
    [[nodiscard]] static bool isGompLookupCommand(const QString &apiKeyValue);

private:
    void run(const QStringList &args, const QByteArray &stdinData, Done done);
};

} // namespace gomp
