#include "ProviderLogoProvider.h"

#include <QColor>
#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

using namespace Qt::StringLiterals;

namespace gomp {

ProviderLogoProvider::ProviderLogoProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

bool ProviderLogoProvider::isSingleTone(const QImage &img)
{
    bool anyDark = false;
    bool anyLight = false;
    bool anyOther = false;
    for (int y = 0; y < img.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(line[x]) < 24) // transparent, or the faintest edge of antialiasing
                continue;
            const int hi = std::max({qRed(line[x]), qGreen(line[x]), qBlue(line[x])});
            const int lo = std::min({qRed(line[x]), qGreen(line[x]), qBlue(line[x])});
            if (hi < 72)
                anyDark = true;
            else if (lo > 184)
                anyLight = true;
            else
                anyOther = true;
        }
    }
    return !anyOther && (anyDark != anyLight);
}

void ProviderLogoProvider::tint(QImage &img, const QColor &color)
{
    const QRgb rgb = color.rgb() & 0x00FFFFFF;
    for (int y = 0; y < img.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x)
            line[x] = (line[x] & 0xFF000000) | rgb; // keep alpha, so antialiased edges stay smooth
    }
}

namespace {
bool hasVisiblePixels(const QImage &img)
{
    for (int y = 0; y < img.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(line[x]) >= 24)
                return true;
        }
    }
    return false;
}
} // namespace

QImage ProviderLogoProvider::load(const QString &name, const QSize &target)
{
    const auto base = u":/qt/qml/Gomp/providers/"_s + name;
    if (QFile::exists(base + u".svg"_s)) {
        QImage image(target, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QSvgRenderer renderer(base + u".svg"_s);
        if (renderer.isValid()) {
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            // Fit inside the target, keeping the aspect ratio.
            const auto view = renderer.defaultSize().isEmpty() ? QSizeF(target) : QSizeF(renderer.defaultSize());
            const auto fitted = view.scaled(QSizeF(target), Qt::KeepAspectRatio);
            renderer.render(&painter, QRectF(QPointF((target.width() - fitted.width()) / 2,
                                                     (target.height() - fitted.height()) / 2), fitted));
            painter.end();
            if (hasVisiblePixels(image))
                return image;
        }
    }
    QImage png(base + u".png"_s);
    if (png.isNull())
        return {};
    png = png.convertToFormat(QImage::Format_ARGB32).scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QImage image(target, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.drawImage((target.width() - png.width()) / 2, (target.height() - png.height()) / 2, png);
    painter.end();
    return hasVisiblePixels(image) ? image : QImage{};
}

QImage ProviderLogoProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const auto slash = id.lastIndexOf(u'/');
    const auto name = slash < 0 ? id : id.left(slash);
    const QColor color(slash < 0 ? QString{} : id.mid(slash + 1));
    const QSize target = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(64, 64);

    auto image = load(name, target);
    if (image.isNull()) {
        image = QImage(target, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
    } else if (color.isValid() && isSingleTone(image)) {
        tint(image, color);
    }
    if (size)
        *size = target;
    return image;
}

} // namespace gomp
