#include "Markdown.h"

#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextTable>
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

QString toHtml(const QString &markdown, const Style &st)
{
    QTextDocument doc;
    QFont base(st.uiFamily);
    base.setPixelSize(14);
    doc.setDefaultFont(base);
    doc.setDocumentMargin(0);
    doc.setMarkdown(markdown, QTextDocument::MarkdownDialectGitHub);

    struct Range {
        int start;
        int end;
        QTextCharFormat fmt;
    };
    QList<Range> ranges;
    for (auto block = doc.begin(); block.isValid(); block = block.next()) {
        QTextCursor bc(block);
        auto bf = block.blockFormat();
        const int level = bf.headingLevel();
        if (level > 0) {
            bf.setTopMargin(level <= 2 ? 10 : 6);
            bf.setBottomMargin(4);
        } else {
            bf.setTopMargin(0);
            bf.setBottomMargin(block.textList() ? 3 : 10);
        }
        bf.setLineHeight(150, QTextBlockFormat::ProportionalHeight);
        bc.setBlockFormat(bf);
        const bool quote = bf.hasProperty(QTextFormat::BlockQuoteLevel);
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto frag = it.fragment();
            const auto cf = frag.charFormat();
            QTextCharFormat nf;
            if (cf.fontFixedPitch()) {
                nf.setFontFamilies({st.monoFamily});
                nf.setProperty(QTextFormat::FontPixelSize, 13);
                nf.setForeground(QColor(st.codeText));
                nf.setBackground(QColor(st.codeBg));
            }
            if (cf.isAnchor()) {
                nf.setForeground(QColor(st.link));
                nf.setFontUnderline(true);
                nf.setUnderlineColor(QColor(st.link));
            }
            if (level > 0) {
                nf.setProperty(QTextFormat::FontPixelSize, level == 1 ? 20 : level == 2 ? 17 : 15);
                nf.setFontWeight(QFont::DemiBold);
            }
            if (quote)
                nf.setForeground(QColor(st.secondary));
            ranges << Range{frag.position(), frag.position() + frag.length(), nf};
        }
    }
    // Apply after collecting: merging formats splits fragments and would invalidate iteration.
    for (const auto &r : std::as_const(ranges)) {
        QTextCursor c(&doc);
        c.setPosition(r.start);
        c.setPosition(r.end, QTextCursor::KeepAnchor);
        c.mergeCharFormat(r.fmt);
    }
    const std::function<void(QTextFrame *)> styleTables = [&](QTextFrame *frame) {
        for (auto *child : frame->childFrames()) {
            if (auto *table = qobject_cast<QTextTable *>(child)) {
                auto tf = table->format();
                tf.setBorder(1);
                tf.setBorderBrush(QColor(st.hairline));
                tf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
                tf.setBorderCollapse(true);
                tf.setCellPadding(6);
                tf.setCellSpacing(0);
                tf.setBottomMargin(10);
                table->setFormat(tf);
                for (int col = 0; col < table->columns(); ++col) {
                    auto cell = table->cellAt(0, col);
                    auto cf = cell.format();
                    cf.setBackground(QColor(st.headerBg));
                    cell.setFormat(cf);
                }
            }
            styleTables(child);
        }
    };
    styleTables(doc.rootFrame());
    return doc.toHtml();
}

} // namespace gomp::md
