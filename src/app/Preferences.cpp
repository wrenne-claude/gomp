#include "Preferences.h"

#include <QDir>

using namespace Qt::StringLiterals;

namespace gomp {

Preferences::Preferences(QObject *parent)
    : QObject(parent)
    , m_settings(u"gomp"_s, u"gomp"_s)
{
}

QVariant Preferences::get(const QString &key, const QVariant &fallback) const
{
    if (m_memory.contains(key))
        return m_memory.value(key);
    if (m_volatile)
        return fallback;
    return m_settings.value(key, fallback);
}

void Preferences::set(const QString &key, const QVariant &value)
{
    if (get(key, {}) == value)
        return;
    if (m_volatile)
        m_memory.insert(key, value);
    else
        m_settings.setValue(key, value);
    emit changed();
}

QString Preferences::themeMode() const { return get(u"appearance/theme"_s, u"system"_s).toString(); }
void Preferences::setThemeMode(const QString &v) { set(u"appearance/theme"_s, v); }
QString Preferences::accent() const { return get(u"appearance/accent"_s, u"sodium"_s).toString(); }
void Preferences::setAccent(const QString &v) { set(u"appearance/accent"_s, v); }
QString Preferences::effects() const { return get(u"appearance/effects"_s, u"auto"_s).toString(); }
void Preferences::setEffects(const QString &v) { set(u"appearance/effects"_s, v); }
bool Preferences::reduceMotion() const { return get(u"appearance/reduceMotion"_s, false).toBool(); }
void Preferences::setReduceMotion(bool v) { set(u"appearance/reduceMotion"_s, v); }
QString Preferences::density() const { return get(u"appearance/density"_s, u"auto"_s).toString(); }
void Preferences::setDensity(const QString &v) { set(u"appearance/density"_s, v); }
bool Preferences::customTitlebar() const { return get(u"window/customTitlebar"_s, true).toBool(); }
void Preferences::setCustomTitlebar(bool v) { set(u"window/customTitlebar"_s, v); }
bool Preferences::notifications() const { return get(u"notifications/enabled"_s, true).toBool(); }
void Preferences::setNotifications(bool v) { set(u"notifications/enabled"_s, v); }
bool Preferences::notifyOnFinish() const { return get(u"notifications/onFinish"_s, true).toBool(); }
void Preferences::setNotifyOnFinish(bool v) { set(u"notifications/onFinish"_s, v); }
QString Preferences::ompPath() const { return get(u"omp/path"_s, QString{}).toString(); }
void Preferences::setOmpPath(const QString &v) { set(u"omp/path"_s, v); }
bool Preferences::firstRunDone() const { return get(u"app/firstRunDone"_s, false).toBool(); }
void Preferences::setFirstRunDone(bool v) { set(u"app/firstRunDone"_s, v); }
bool Preferences::inspectorOpen() const { return get(u"window/inspectorOpen"_s, true).toBool(); }
void Preferences::setInspectorOpen(bool v) { set(u"window/inspectorOpen"_s, v); }
QString Preferences::lastScreen() const { return get(u"window/lastScreen"_s, u"agent"_s).toString(); }
void Preferences::setLastScreen(const QString &v) { set(u"window/lastScreen"_s, v); }

QStringList Preferences::recentProjects() const
{
    return get(u"app/recentProjects"_s, QStringList{}).toStringList();
}

void Preferences::addRecentProject(const QString &dir)
{
    auto list = recentProjects();
    const auto clean = QDir::cleanPath(dir);
    list.removeAll(clean);
    list.prepend(clean);
    while (list.size() > 12)
        list.removeLast();
    set(u"app/recentProjects"_s, list);
}

QVariantMap Preferences::shortcutOverrides() const { return get(u"shortcuts/overrides"_s, QVariantMap{}).toMap(); }
void Preferences::setShortcutOverrides(const QVariantMap &m) { set(u"shortcuts/overrides"_s, m); }
QStringList Preferences::storedEnvKeys() const { return get(u"providers/keyringEnv"_s, QStringList{}).toStringList(); }
void Preferences::setStoredEnvKeys(const QStringList &keys) { set(u"providers/keyringEnv"_s, keys); }
QVariantList Preferences::openWorkspaces() const { return get(u"app/openWorkspaces"_s, QVariantList{}).toList(); }
void Preferences::setOpenWorkspaces(const QVariantList &list) { set(u"app/openWorkspaces"_s, list); }
QByteArray Preferences::windowGeometry() const { return get(u"window/geometry"_s, QByteArray{}).toByteArray(); }
void Preferences::setWindowGeometry(const QByteArray &g) { set(u"window/geometry"_s, g); }

} // namespace gomp
