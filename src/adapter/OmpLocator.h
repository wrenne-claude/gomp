#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// Finds the omp binary and checks its version. Drives the first-run screen and the
// "omp missing / unexpected version" banners.
class OmpLocator : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(Status status READ status NOTIFY changed)
    Q_PROPERTY(QString path READ path NOTIFY changed)
    Q_PROPERTY(QString version READ version NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QStringList searchedPaths READ searchedPaths NOTIFY changed)
    Q_PROPERTY(QString installCommand READ installCommand CONSTANT)
    Q_PROPERTY(QString testedVersion READ testedVersion CONSTANT)
public:
    enum class Status { Checking, Found, Untested, Missing, Broken };
    Q_ENUM(Status)

    explicit OmpLocator(QObject *parent = nullptr);

    [[nodiscard]] Status status() const { return m_status; }
    [[nodiscard]] QString path() const { return m_path; }
    [[nodiscard]] QString version() const { return m_version; }
    [[nodiscard]] QString message() const { return m_message; }
    [[nodiscard]] QStringList searchedPaths() const { return m_searched; }
    [[nodiscard]] QString installCommand() const;
    [[nodiscard]] static QString testedVersion() { return QStringLiteral("18.8"); }

    // Re-runs discovery. `preferred` (from Preferences) is tried first.
    Q_INVOKABLE void locate(const QString &preferred = {});
    // Validates a user-chosen file; on success becomes the active path.
    Q_INVOKABLE void useBinary(const QString &pathOrUrl);

    // "omp/18.8.7" -> {18, 8, 7}; empty when unparseable. Public for tests.
    static QList<int> parseVersion(const QString &versionOutput);
    static bool isSupported(const QList<int> &version);

signals:
    void changed();

private:
    void probe(const QStringList &candidates, int index);

    Status m_status = Status::Checking;
    QString m_path;
    QString m_version;
    QString m_message;
    QStringList m_searched;
};

} // namespace gomp
