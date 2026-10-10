#include "Notifier.h"

#ifdef GOMP_HAVE_DBUS
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#endif
#include <QVariantMap>

using namespace Qt::StringLiterals;

namespace gomp {

namespace {
#ifdef GOMP_HAVE_DBUS
const auto Service = u"org.freedesktop.Notifications"_s;
const auto Path = u"/org/freedesktop/Notifications"_s;
#endif
} // namespace

Notifier::Notifier(QObject *parent)
    : QObject(parent)
{
#ifdef GOMP_HAVE_DBUS
    QDBusConnection::sessionBus().connect(Service, Path, Service, u"ActionInvoked"_s, this,
                                          SLOT(onActionInvoked(uint, QString)));
#endif
}

uint Notifier::notify(const QString &title, const QString &body, const QString &key, bool urgent)
{
#ifndef GOMP_HAVE_DBUS
    Q_UNUSED(title) Q_UNUSED(body) Q_UNUSED(key) Q_UNUSED(urgent)
    return 0; // no desktop notification service on this platform; the in-app toast still shows
#else
    QDBusInterface iface(Service, Path, Service, QDBusConnection::sessionBus());
    if (!iface.isValid())
        return 0;
    const QVariantMap hints{
        {u"desktop-entry"_s, u"dev.gomp.Gomp"_s},
        {u"urgency"_s, QVariant::fromValue<uchar>(urgent ? 2 : 1)},
        {u"category"_s, urgent ? u"im.received"_s : u"transfer.complete"_s},
    };
    const QDBusReply<uint> reply = iface.call(u"Notify"_s, u"gomp"_s, uint(0), u"dev.gomp.Gomp"_s, title, body,
                                              QStringList{u"default"_s, u"Open"_s}, hints, int(-1));
    if (!reply.isValid())
        return 0;
    if (!key.isEmpty())
        m_keys.insert(reply.value(), key);
    return reply.value();
#endif
}

void Notifier::close(uint id)
{
#ifdef GOMP_HAVE_DBUS
    QDBusInterface iface(Service, Path, Service, QDBusConnection::sessionBus());
    if (iface.isValid())
        iface.call(u"CloseNotification"_s, id);
#endif
    m_keys.remove(id);
}

void Notifier::onActionInvoked(uint id, const QString &action)
{
    if (!m_keys.contains(id))
        return;
    if (action == "default"_L1)
        emit activated(m_keys.value(id));
    m_keys.remove(id);
}

} // namespace gomp
