#pragma once

#include <QCache>
#include <QMutex>
#include <QQuickImageProvider>

namespace gomp {

// Serves the bundled stroke icons as `image://icons/<name>/<#rrggbb>`. The SVGs are drawn with a
// placeholder colour that is replaced before rasterising, so every icon renders crisp at any
// size and tint without a per-icon shader.
class IconProvider : public QQuickImageProvider
{
public:
    IconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QByteArray source(const QString &name);
    QMutex m_mutex;
    QHash<QString, QByteArray> m_sources;
};

} // namespace gomp
