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

    // True when every visible pixel is near-black or every visible pixel is near-white.
    [[nodiscard]] static bool isSingleTone(const QImage &argb32);
    static void tint(QImage &argb32, const QColor &color);
};

} // namespace gomp
