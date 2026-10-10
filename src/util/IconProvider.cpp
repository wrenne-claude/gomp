#include "IconProvider.h"

#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

using namespace Qt::StringLiterals;

namespace gomp {

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QByteArray IconProvider::source(const QString &name)
{
    QMutexLocker lock(&m_mutex);
    if (const auto it = m_sources.constFind(name); it != m_sources.cend())
        return *it;
    QFile f(u":/qt/qml/Gomp/icons/"_s + name + u".svg"_s);
    const auto data = f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray{};
    m_sources.insert(name, data);
    return data;
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const auto name = id.section(u'/', 0, 0);
    auto color = id.section(u'/', 1);
    if (color.isEmpty())
        color = u"#000000"_s;
    auto svg = source(name);
    if (svg.isEmpty())
        svg = source(u"circle-dashed"_s); // visible, recognisable stand-in for a missing icon
    svg.replace("#000000", color.toLatin1());

    const QSize target = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(20, 20);
    QImage image(target, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QSvgRenderer renderer(svg);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(target)));
    painter.end();
    if (size)
        *size = target;
    return image;
}

} // namespace gomp
