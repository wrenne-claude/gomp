#include "LineDiff.h"

#include <QRegularExpression>
#include <QVariantMap>

#include <vector>

using namespace Qt::StringLiterals;

namespace gomp::diff {

namespace {

constexpr qsizetype MaxCells = 4'000'000; // LCS table budget; larger inputs fall back to a block diff

QList<Line> rawDiff(const QStringList &a, const QStringList &b)
{
    // Trim common prefix/suffix first; edits are usually local.
    qsizetype pre = 0;
    while (pre < a.size() && pre < b.size() && a.at(pre) == b.at(pre))
        ++pre;
    qsizetype suf = 0;
    while (suf < a.size() - pre && suf < b.size() - pre && a.at(a.size() - 1 - suf) == b.at(b.size() - 1 - suf))
        ++suf;

    QList<Line> out;
    for (qsizetype i = 0; i < pre; ++i)
        out << Line{Line::Kind::Context, int(i + 1), int(i + 1), a.at(i)};

    const qsizetype n = a.size() - pre - suf;
    const qsizetype m = b.size() - pre - suf;
    if (n * m <= MaxCells && n > 0 && m > 0) {
        std::vector<std::vector<int>> lcs(size_t(n + 1), std::vector<int>(size_t(m + 1), 0));
        for (qsizetype i = n - 1; i >= 0; --i)
            for (qsizetype j = m - 1; j >= 0; --j)
                lcs[size_t(i)][size_t(j)] = a.at(pre + i) == b.at(pre + j)
                                                 ? lcs[size_t(i + 1)][size_t(j + 1)] + 1
                                                 : std::max(lcs[size_t(i + 1)][size_t(j)], lcs[size_t(i)][size_t(j + 1)]);
        qsizetype i = 0;
        qsizetype j = 0;
        while (i < n || j < m) {
            if (i < n && j < m && a.at(pre + i) == b.at(pre + j)) {
                out << Line{Line::Kind::Context, int(pre + i + 1), int(pre + j + 1), a.at(pre + i)};
                ++i;
                ++j;
            } else if (i < n && (j == m || lcs[size_t(i + 1)][size_t(j)] >= lcs[size_t(i)][size_t(j + 1)])) {
                // Removals before additions, as in unified diffs.
                out << Line{Line::Kind::Removed, int(pre + i + 1), 0, a.at(pre + i)};
                ++i;
            } else {
                out << Line{Line::Kind::Added, 0, int(pre + j + 1), b.at(pre + j)};
                ++j;
            }
        }
    } else {
        for (qsizetype i = 0; i < n; ++i)
            out << Line{Line::Kind::Removed, int(pre + i + 1), 0, a.at(pre + i)};
        for (qsizetype j = 0; j < m; ++j)
            out << Line{Line::Kind::Added, 0, int(pre + j + 1), b.at(pre + j)};
    }
    for (qsizetype k = 0; k < suf; ++k) {
        const auto ai = a.size() - suf + k;
        const auto bi = b.size() - suf + k;
        out << Line{Line::Kind::Context, int(ai + 1), int(bi + 1), a.at(ai)};
    }
    return out;
}

QList<Line> collapse(const QList<Line> &lines, int context)
{
    QList<bool> keep(lines.size(), false);
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (lines.at(i).kind == Line::Kind::Context)
            continue;
        for (qsizetype k = std::max<qsizetype>(0, i - context); k <= std::min(lines.size() - 1, i + context); ++k)
            keep[k] = true;
    }
    QList<Line> out;
    bool gap = false;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (keep.at(i)) {
            out << lines.at(i);
            gap = false;
        } else if (!gap) {
            out << Line{Line::Kind::Gap, 0, 0, {}};
            gap = true;
        }
    }
    return out;
}

} // namespace

QList<Line> compute(const QString &before, const QString &after, int context)
{
    const auto a = before.isEmpty() ? QStringList{} : before.split(u'\n');
    const auto b = after.isEmpty() ? QStringList{} : after.split(u'\n');
    return collapse(rawDiff(a, b), context);
}

QList<Line> parseOmpDiff(const QString &text)
{
    static const QRegularExpression ompLine(uR"(^([ +\-])\s*(\d+)\|(.*)$)"_s);
    static const QRegularExpression hunk(uR"(^@@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@)"_s);
    QList<Line> out;
    int oldNo = 0;
    int newNo = 0;
    for (const auto &line : text.split(u'\n')) {
        if (line.isEmpty())
            continue;
        if (const auto m = ompLine.match(line); m.hasMatch()) {
            const auto sign = m.captured(1);
            const int no = m.captured(2).toInt();
            if (sign == "+"_L1)
                out << Line{Line::Kind::Added, 0, no, m.captured(3)};
            else if (sign == "-"_L1)
                out << Line{Line::Kind::Removed, no, 0, m.captured(3)};
            else
                out << Line{Line::Kind::Context, no, no, m.captured(3)};
            continue;
        }
        if (const auto m = hunk.match(line); m.hasMatch()) {
            oldNo = m.captured(1).toInt();
            newNo = m.captured(2).toInt();
            if (!out.isEmpty())
                out << Line{Line::Kind::Gap, 0, 0, {}};
            continue;
        }
        if (line.startsWith("+++"_L1) || line.startsWith("---"_L1))
            continue;
        if (line == "..."_L1 || line.trimmed() == "…"_L1) {
            out << Line{Line::Kind::Gap, 0, 0, {}};
            continue;
        }
        if (line.startsWith(u'+'))
            out << Line{Line::Kind::Added, 0, newNo++, line.mid(1)};
        else if (line.startsWith(u'-'))
            out << Line{Line::Kind::Removed, oldNo++, 0, line.mid(1)};
        else
            out << Line{Line::Kind::Context, oldNo++, newNo++, line.startsWith(u' ') ? line.mid(1) : line};
    }
    return out;
}

QVariantList toVariant(const QList<Line> &lines)
{
    QVariantList out;
    out.reserve(lines.size());
    for (const auto &l : lines) {
        QString kind;
        switch (l.kind) {
        case Line::Kind::Context:
            kind = u"context"_s;
            break;
        case Line::Kind::Added:
            kind = u"added"_s;
            break;
        case Line::Kind::Removed:
            kind = u"removed"_s;
            break;
        case Line::Kind::Gap:
            kind = u"gap"_s;
            break;
        }
        out << QVariantMap{{u"kind"_s, kind}, {u"oldNo"_s, l.oldNo}, {u"newNo"_s, l.newNo}, {u"text"_s, l.text}};
    }
    return out;
}

QString toUnified(const QList<Line> &lines)
{
    QString out;
    for (const auto &l : lines) {
        switch (l.kind) {
        case Line::Kind::Context:
            out += u' ' + l.text + u'\n';
            break;
        case Line::Kind::Added:
            out += u'+' + l.text + u'\n';
            break;
        case Line::Kind::Removed:
            out += u'-' + l.text + u'\n';
            break;
        case Line::Kind::Gap:
            out += u"@@\n"_s;
            break;
        }
    }
    return out;
}

Stats stats(const QList<Line> &lines)
{
    Stats s;
    for (const auto &l : lines) {
        s.added += l.kind == Line::Kind::Added ? 1 : 0;
        s.removed += l.kind == Line::Kind::Removed ? 1 : 0;
    }
    return s;
}

} // namespace gomp::diff
