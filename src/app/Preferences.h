#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// gomp's own preferences (not omp's config): appearance, effects, window and session restore.
// Stored with QSettings in ~/.config/gomp/gomp.conf.
class Preferences : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY changed)         // system | dark | light
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY changed)                  // sodium | blueprint | lock | ion | rose | system
    Q_PROPERTY(QString effects READ effects WRITE setEffects NOTIFY changed)               // auto | full | reduced
    Q_PROPERTY(bool reduceMotion READ reduceMotion WRITE setReduceMotion NOTIFY changed)
    Q_PROPERTY(QString density READ density WRITE setDensity NOTIFY changed)               // auto | comfortable | compact
    Q_PROPERTY(bool customTitlebar READ customTitlebar WRITE setCustomTitlebar NOTIFY changed)
    Q_PROPERTY(bool notifications READ notifications WRITE setNotifications NOTIFY changed)
    Q_PROPERTY(bool notifyOnFinish READ notifyOnFinish WRITE setNotifyOnFinish NOTIFY changed)
    Q_PROPERTY(QString ompPath READ ompPath WRITE setOmpPath NOTIFY changed)
    Q_PROPERTY(bool firstRunDone READ firstRunDone WRITE setFirstRunDone NOTIFY changed)
    Q_PROPERTY(QStringList recentProjects READ recentProjects NOTIFY changed)
    Q_PROPERTY(bool inspectorOpen READ inspectorOpen WRITE setInspectorOpen NOTIFY changed)
    Q_PROPERTY(QString lastScreen READ lastScreen WRITE setLastScreen NOTIFY changed)
public:
    explicit Preferences(QObject *parent = nullptr);
    // For tests and --mock sessions: keep settings in memory only.
    void setVolatile(bool v) { m_volatile = v; }

    [[nodiscard]] QString themeMode() const;
    void setThemeMode(const QString &v);
    [[nodiscard]] QString accent() const;
    void setAccent(const QString &v);
    [[nodiscard]] QString effects() const;
    void setEffects(const QString &v);
    [[nodiscard]] bool reduceMotion() const;
    void setReduceMotion(bool v);
    [[nodiscard]] QString density() const;
    void setDensity(const QString &v);
    [[nodiscard]] bool customTitlebar() const;
    void setCustomTitlebar(bool v);
    [[nodiscard]] bool notifications() const;
    void setNotifications(bool v);
    [[nodiscard]] bool notifyOnFinish() const;
    void setNotifyOnFinish(bool v);
    [[nodiscard]] QString ompPath() const;
    void setOmpPath(const QString &v);
    [[nodiscard]] bool firstRunDone() const;
    void setFirstRunDone(bool v);
    [[nodiscard]] QStringList recentProjects() const;
    Q_INVOKABLE void addRecentProject(const QString &dir);
    [[nodiscard]] bool inspectorOpen() const;
    void setInspectorOpen(bool v);
    [[nodiscard]] QString lastScreen() const;
    void setLastScreen(const QString &v);

    [[nodiscard]] QVariantMap shortcutOverrides() const;
    void setShortcutOverrides(const QVariantMap &m);
    [[nodiscard]] QStringList storedEnvKeys() const;
    void setStoredEnvKeys(const QStringList &keys);
    [[nodiscard]] QVariantList openWorkspaces() const;
    void setOpenWorkspaces(const QVariantList &list);
    [[nodiscard]] QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray &g);

signals:
    void changed();

private:
    [[nodiscard]] QVariant get(const QString &key, const QVariant &fallback) const;
    void set(const QString &key, const QVariant &value);
    QSettings m_settings;
    QVariantMap m_memory;
    bool m_volatile = false;
};

} // namespace gomp
