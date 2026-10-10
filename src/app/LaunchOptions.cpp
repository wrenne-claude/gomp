#include "LaunchOptions.h"

#include <QJSEngine>

namespace gomp {

namespace {
struct Values {
    QSize size;
    QString theme;
    QString scenario;
    bool reduceEffects = false;
};
Values &values()
{
    static Values v;
    return v;
}
} // namespace

LaunchOptionsObject *LaunchOptionsObject::create(QQmlEngine *, QJSEngine *)
{
    return new LaunchOptionsObject; // owned by the QML engine
}

void LaunchOptionsObject::configure(QSize size, const QString &theme, const QString &scenario, bool reduceEffects)
{
    values() = Values{size, theme, scenario, reduceEffects};
}

QSize LaunchOptionsObject::size() const { return values().size; }
QString LaunchOptionsObject::theme() const { return values().theme; }
QString LaunchOptionsObject::scenario() const { return values().scenario; }
bool LaunchOptionsObject::reduceEffects() const { return values().reduceEffects; }

} // namespace gomp
