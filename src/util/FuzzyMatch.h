#pragma once

#include <QList>
#include <QString>

namespace gomp::fuzzy {

struct Match {
    int score = -1; // < 0: no match
    QList<int> positions;
    [[nodiscard]] bool matched() const { return score >= 0; }
};

// Subsequence match with bonuses for word starts, camelCase humps, path separators and
// consecutive runs; case-insensitive unless the query has uppercase letters (smart case).
Match match(QStringView query, QStringView candidate);

// Wraps matched characters in <b> for list rows (rich text, escaped).
QString highlight(const QString &candidate, const QList<int> &positions, const QString &color);

} // namespace gomp::fuzzy
