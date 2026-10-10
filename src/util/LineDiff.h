#pragma once

#include <QList>
#include <QString>
#include <QVariantList>

namespace gomp::diff {

struct Line {
    enum class Kind { Context, Added, Removed, Gap };
    Kind kind = Kind::Context;
    int oldNo = 0; // 1-based, 0 when not applicable
    int newNo = 0;
    QString text;
};

// Line diff of two texts with `context` lines around changes; unchanged runs collapse to a Gap.
QList<Line> compute(const QString &before, const QString &after, int context = 3);

// Parses omp's edit-tool diff (" 145|  text", "-147|old", "+147|new") or a unified diff.
QList<Line> parseOmpDiff(const QString &diff);

QVariantList toVariant(const QList<Line> &lines);
QString toUnified(const QList<Line> &lines);

struct Stats {
    int added = 0;
    int removed = 0;
};
Stats stats(const QList<Line> &lines);

} // namespace gomp::diff
