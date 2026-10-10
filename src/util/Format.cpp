#include "Format.h"

#include <QDateTime>
#include <QDir>
#include <QLocale>

#include <cmath>

using namespace Qt::StringLiterals;

namespace gomp {

QString Format::tokens(double n)
{
    if (n < 1000)
        return QString::number(qint64(std::llround(n)));
    if (n < 100'000)
        return QString::number(n / 1000.0, 'f', 1) + u'k';
    if (n < 1'000'000)
        return QString::number(n / 1000.0, 'f', 0) + u'k';
    return QString::number(n / 1'000'000.0, 'f', n < 10'000'000 ? 2 : 1) + u'M';
}

QString Format::cost(double usd)
{
    if (usd <= 0)
        return u"—"_s;
    if (usd < 0.01)
        return u"<$0.01"_s;
    if (usd < 10)
        return u"$"_s + QString::number(usd, 'f', 2);
    return u"$"_s + QString::number(usd, 'f', 1);
}

QString Format::pricePerMTok(double usd)
{
    if (usd <= 0)
        return u"free"_s;
    const bool whole = std::abs(usd - std::round(usd)) < 1e-9;
    return u"$"_s + QString::number(usd, 'f', whole ? 0 : 2);
}

QString Format::duration(double ms)
{
    if (ms < 1000)
        return QString::number(qint64(std::llround(ms))) + u" ms"_s;
    const double s = ms / 1000.0;
    if (s < 60)
        return QString::number(s, 'f', s < 10 ? 1 : 0) + u" s"_s;
    const auto total = qint64(std::llround(s));
    if (total < 3600)
        return u"%1 min %2 s"_s.arg(total / 60).arg(total % 60);
    return u"%1 h %2 min"_s.arg(total / 3600).arg((total % 3600) / 60);
}

QString Format::relativeTime(double epochMs)
{
    if (epochMs <= 0)
        return {};
    const auto then = QDateTime::fromMSecsSinceEpoch(qint64(epochMs));
    const auto now = QDateTime::currentDateTime();
    const auto secs = then.secsTo(now);
    if (secs < 45)
        return u"just now"_s;
    if (secs < 3600)
        return u"%1 min ago"_s.arg(std::max<qint64>(1, secs / 60));
    if (then.date() == now.date())
        return QLocale().toString(then.time(), QLocale::ShortFormat);
    if (then.date() == now.date().addDays(-1))
        return u"yesterday"_s;
    if (then.date().daysTo(now.date()) < 7)
        return QLocale().dayName(then.date().dayOfWeek(), QLocale::ShortFormat);
    if (then.date().year() == now.date().year())
        return QLocale().toString(then.date(), u"d MMM"_s);
    return QLocale().toString(then.date(), u"d MMM yyyy"_s);
}

QString Format::percent(double fraction)
{
    return QString::number(qint64(std::llround(fraction * 100))) + u'%';
}

QString Format::bytes(double n)
{
    if (n < 1024)
        return QString::number(qint64(n)) + u" B"_s;
    if (n < 1024 * 1024)
        return QString::number(n / 1024, 'f', 0) + u" KB"_s;
    return QString::number(n / (1024 * 1024), 'f', 1) + u" MB"_s;
}

QString Format::shortPath(const QString &path)
{
    const auto home = QDir::homePath();
    if (path == home)
        return u"~"_s;
    if (path.startsWith(home + u'/'))
        return u"~"_s + path.mid(home.size());
    return path;
}

} // namespace gomp
