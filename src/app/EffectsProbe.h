#pragma once

#include <QObject>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

namespace gomp {

// Detects software rendering (Qt's software backend, llvmpipe, softpipe, SwiftShader) so
// "Reduce effects: Automatic" can drop blur, shaders and particles where they would cost frames.
class EffectsProbe : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(bool softwareRendering READ softwareRendering NOTIFY changed)
    Q_PROPERTY(QString renderer READ renderer NOTIFY changed)
    Q_PROPERTY(QString graphicsApi READ graphicsApi NOTIFY changed)
public:
    explicit EffectsProbe(QObject *parent = nullptr);

    Q_INVOKABLE void attach(QQuickWindow *window);
    [[nodiscard]] bool softwareRendering() const { return m_software; }
    [[nodiscard]] QString renderer() const { return m_renderer; }
    [[nodiscard]] QString graphicsApi() const { return m_api; }

    static bool isSoftwareRendererName(const QString &glRenderer);

signals:
    void changed();

private:
    bool m_software = false;
    QString m_renderer;
    QString m_api;
};

} // namespace gomp
