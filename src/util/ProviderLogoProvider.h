#pragma once

#include <QQuickImageProvider>

namespace gomp {

// Serves resources/providers/<id>.svg|png as `image://providerlogo/<id>/<#rrggbb>`.
//
// Logos keep their own colours, with one exception: a logo drawn in a single flat tone, all
// near-black or all near-white, would vanish on one of the themes, so it is recoloured to the tint
// the caller passes (the theme's text colour). Anything with real colour is left untouched.
class ProviderLogoProvider : public QQuickImageProvider
{
public:
    ProviderLogoProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    // The logo for a provider id at `target`, from <id>.svg or, when the SVG is missing or draws
    // nothing (Qt's SVG renderer ignores masks and filters), <id>.png. Null when neither works.
    [[nodiscard]] static QImage load(const QString &name, const QSize &target);

    // True when every visible pixel is near-black or every visible pixel is near-white.
    [[nodiscard]] static bool isSingleTone(const QImage &argb32);
    static void tint(QImage &argb32, const QColor &color);
};

} // namespace gomp
