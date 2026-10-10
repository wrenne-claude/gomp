#include "Markdown.h"

#include <QRegularExpression>
#include <QVariantMap>

using namespace Qt::StringLiterals;

namespace gomp::md {

namespace {

// Returns the fence marker (``` or ~~~, possibly longer) if `line` opens or closes a fence.
QStringView fenceOf(QStringView line)
{
    const auto trimmed = line.trimmed();
    qsizetype indent = 0;
    while (indent < line.size() && line.at(indent) == u' ')
        ++indent;
    if (indent > 3) // indented code, not a fence (CommonMark allows up to three spaces)
        return {};
    for (const auto ch : {u'`', u'~'}) {
        qsizetype n = 0;
        while (n < trimmed.size() && trimmed.at(n) == ch)
            ++n;
        if (n >= 3)
            return trimmed.left(n);
    }
    return {};
}

} // namespace

QList<Segment> split(const QString &markdown)
{
    QList<Segment> out;
    Segment current;
    QString fence;
    const auto lines = QStringView(markdown).split(u'\n');

    auto flush = [&] {
        if (current.kind == Segment::Kind::Prose) {
            while (current.text.endsWith(u'\n'))
                current.text.chop(1);
            if (current.text.trimmed().isEmpty()) {
                current = {};
                return;
            }
        }
        out << current;
        current = {};
    };

    for (qsizetype i = 0; i < lines.size(); ++i) {
        const auto line = lines.at(i);
        const auto marker = fenceOf(line);
        if (fence.isEmpty()) {
            if (!marker.isEmpty()) {
                flush();
                fence = marker.toString();
                current.kind = Segment::Kind::Code;
                current.language = line.trimmed().mid(marker.size()).trimmed().toString().section(u' ', 0, 0).toLower();
                current.open = true;
                continue;
            }
            current.text += line;
            if (i + 1 < lines.size())
                current.text += u'\n';
        } else {
            // A closing fence uses the same character and is at least as long, with nothing after it.
            if (!marker.isEmpty() && marker.at(0) == fence.at(0) && marker.size() >= fence.size()
                && line.trimmed().size() == marker.size()) {
                if (current.text.endsWith(u'\n'))
                    current.text.chop(1);
                current.open = false;
                flush();
                fence.clear();
                continue;
            }
            current.text += line;
            current.text += u'\n';
        }
    }
    if (current.kind == Segment::Kind::Code && current.text.endsWith(u'\n'))
        current.text.chop(1);
    flush();
    return out;
}

QVariantList toVariant(const QList<Segment> &segments)
{
    QVariantList list;
    list.reserve(segments.size());
    for (const auto &s : segments) {
        list << QVariantMap{{u"kind"_s, s.kind == Segment::Kind::Code ? u"code"_s : u"prose"_s},
                            {u"text"_s, s.text},
                            {u"language"_s, s.language},
                            {u"open"_s, s.open}};
    }
    return list;
}

QString plainPreview(const QString &markdown, int maxChars)
{
    static const QRegularExpression fences(u"```[\\s\\S]*?(```|$)"_s);
    static const QRegularExpression links(u"\\[([^\\]]*)\\]\\([^)]*\\)"_s);
    static const QRegularExpression marks(u"[*_`#>]+"_s);
    static const QRegularExpression spaces(u"\\s+"_s);
    QString s = markdown;
    s.replace(fences, u" "_s);
    s.replace(links, u"\\1"_s);
    s.replace(marks, QString{});
    s.replace(spaces, u" "_s);
    s = s.trimmed();
    if (s.size() > maxChars)
        s = s.left(maxChars - 1).trimmed() + u'…';
    return s;
}

} // namespace gomp::md
