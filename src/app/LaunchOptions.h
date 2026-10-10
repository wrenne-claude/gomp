#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QSize>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// Command-line options the UI reads at startup (`Launch` in QML): window size and theme overrides
// for screenshots and testing, and the mock scenario to play first.
class LaunchOptionsObject : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Launch)
    QML_SINGLETON
    Q_PROPERTY(QSize size READ size CONSTANT)
    Q_PROPERTY(QString theme READ theme CONSTANT)
    Q_PROPERTY(QString scenario READ scenario CONSTANT)
    Q_PROPERTY(bool reduceEffects READ reduceEffects CONSTANT)
public:
    explicit LaunchOptionsObject(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    static LaunchOptionsObject *create(QQmlEngine *, QJSEngine *);
    static void configure(QSize size, const QString &theme, const QString &scenario, bool reduceEffects);

    [[nodiscard]] QSize size() const;
    [[nodiscard]] QString theme() const;
    [[nodiscard]] QString scenario() const;
    [[nodiscard]] bool reduceEffects() const;
};

} // namespace gomp
