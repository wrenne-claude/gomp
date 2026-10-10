#include "EffectsProbe.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QSGRendererInterface>

using namespace Qt::StringLiterals;

namespace gomp {

EffectsProbe::EffectsProbe(QObject *parent)
    : QObject(parent)
{
    // An explicit software scene graph is known before any window exists.
    if (QQuickWindow::sceneGraphBackend() == "software"_L1) {
        m_software = true;
        m_api = u"software"_s;
    }
}

bool EffectsProbe::isSoftwareRendererName(const QString &r)
{
    const auto lower = r.toLower();
    return lower.contains("llvmpipe"_L1) || lower.contains("softpipe"_L1) || lower.contains("swiftshader"_L1)
           || lower.contains("software rasterizer"_L1) || lower.contains("lavapipe"_L1);
}

void EffectsProbe::attach(QQuickWindow *window)
{
    if (!window)
        return;
    connect(
        window, &QQuickWindow::sceneGraphInitialized, this,
        [this, window] {
            const auto api = window->rendererInterface()->graphicsApi();
            QString renderer;
            QString apiName;
            switch (api) {
            case QSGRendererInterface::Software:
                apiName = u"software"_s;
                break;
            case QSGRendererInterface::OpenGL:
                apiName = u"OpenGL"_s;
                if (auto *ctx = QOpenGLContext::currentContext())
                    renderer = QString::fromLatin1(reinterpret_cast<const char *>(ctx->functions()->glGetString(GL_RENDERER)));
                break;
            case QSGRendererInterface::Vulkan:
                apiName = u"Vulkan"_s;
                break;
            default:
                apiName = u"other"_s;
                break;
            }
            // Runs on the render thread; publish on the GUI thread.
            QMetaObject::invokeMethod(this, [this, api, renderer, apiName] {
                m_api = apiName;
                m_renderer = renderer;
                m_software = api == QSGRendererInterface::Software || isSoftwareRendererName(renderer);
                emit changed();
            });
        },
        Qt::DirectConnection);
}

} // namespace gomp
