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

} // namespace gomp::md
