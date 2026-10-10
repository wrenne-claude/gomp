#pragma once

#include <QHash>
#include <QObject>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// Desktop notifications through org.freedesktop.Notifications (KDE, GNOME, most others).
// Clicking a notification's default action raises the window and reports `activated`.
class Notifier : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
public:
    explicit Notifier(QObject *parent = nullptr);

    // Returns the notification id, or 0 when the service is unavailable.
    Q_INVOKABLE uint notify(const QString &title, const QString &body, const QString &key = {},
                            bool urgent = false);
    Q_INVOKABLE void close(uint id);

signals:
    void activated(const QString &key);

private slots:
    void onActionInvoked(uint id, const QString &action);

private:
    QHash<uint, QString> m_keys;
};

} // namespace gomp
