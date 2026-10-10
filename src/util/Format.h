#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// Number and time formatting shared by C++ and QML, so every meter, cost and timestamp in the UI
// reads the same way. Exposed to QML as the `Fmt` singleton.
class Format : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Fmt)
    QML_SINGLETON
public:
    using QObject::QObject;

    Q_INVOKABLE static QString tokens(double count);           // 950, 12.4k, 1.20M
    Q_INVOKABLE static QString cost(double usd);               // $0.042, $1.27, —
    Q_INVOKABLE static QString duration(double ms);            // 340 ms, 4.2 s, 3 min 12 s
    Q_INVOKABLE static QString relativeTime(double epochMs);   // just now, 4 min ago, yesterday, 12 Mar
    Q_INVOKABLE static QString percent(double fraction);       // 37%
    Q_INVOKABLE static QString bytes(double count);            // 18 KB
    Q_INVOKABLE static QString shortPath(const QString &path); // ~/Projects/gomp
    Q_INVOKABLE static QString pricePerMTok(double usd);       // $3 / $15 inputs
    Q_INVOKABLE static QString count(double n, const QString &singular, const QString &plural); // 1 message, 3 messages
};

} // namespace gomp
