#pragma once

#include <QList>
#include <QString>
#include <QVariantList>

namespace gomp::md {

// Assistant markdown is split into prose and fenced code so code blocks get their own delegate
// (highlighting, copy, horizontal scroll) while prose goes through Qt's CommonMark renderer.
struct Segment {
    enum class Kind { Prose, Code };
    Kind kind = Kind::Prose;
    QString text;
    QString language;
    bool open = false; // fence not yet closed (still streaming)
};

QList<Segment> split(const QString &markdown);
QVariantList toVariant(const QList<Segment> &segments);

// Short one-line preview of markdown for list rows: strips fences, emphasis and links.
QString plainPreview(const QString &markdown, int maxChars = 140);

// Colours and fonts for rendered prose, supplied by the QML theme.
struct Style {
    QString text = QStringLiteral("#E7ECF3");
    QString secondary = QStringLiteral("#A3AFBF");
    QString link = QStringLiteral("#FFB547");
    QString codeText = QStringLiteral("#D8DFEA");
    QString codeBg = QStringLiteral("#1C2430");
    QString hairline = QStringLiteral("#263142");
    QString headerBg = QStringLiteral("#18202B");
    QString uiFamily = QStringLiteral("Barlow");
    QString monoFamily = QStringLiteral("JetBrains Mono");
};

// Renders a prose segment (no fenced code) to Qt rich text: Qt's CommonMark parser, then the
// document is restyled from the theme (inline code, links, headings, tables, line height).
QString toHtml(const QString &markdown, const Style &style);

} // namespace gomp::md
