#include "FuzzyMatch.h"

using namespace Qt::StringLiterals;

namespace gomp::fuzzy {

namespace {

bool isBoundary(QStringView s, qsizetype i)
{
    if (i == 0)
        return true;
    const QChar prev = s.at(i - 1);
    const QChar cur = s.at(i);
    if (prev == u'/' || prev == u'_' || prev == u'-' || prev == u'.' || prev == u' ' || prev == u':')
        return true;
    return prev.isLower() && cur.isUpper();
}

} // namespace

Match match(QStringView query, QStringView candidate)
{
    Match m;
    if (query.isEmpty()) {
        m.score = 0;
        return m;
    }
    bool smartCase = false;
    for (const auto ch : query)
        smartCase = smartCase || ch.isUpper();
    const auto cs = smartCase ? Qt::CaseSensitive : Qt::CaseInsensitive;

    // Greedy forward pass that prefers boundary matches: for each query char, look for a boundary
    // occurrence before the next plain occurrence's run ends.
    int score = 0;
    qsizetype ci = 0;
    int run = 0;
    for (qsizetype qi = 0; qi < query.size(); ++qi) {
        const QChar qc = query.at(qi);
        qsizetype found = -1;
        qsizetype boundaryFound = -1;
        for (qsizetype k = ci; k < candidate.size(); ++k) {
            if (QStringView(&qc, 1).compare(candidate.mid(k, 1), cs) != 0)
                continue;
            if (found < 0)
                found = k;
            if (isBoundary(candidate, k)) {
                boundaryFound = k;
                break;
            }
            if (k > found + 12) // do not wander far looking for a boundary
                break;
        }
        if (found < 0)
            return {};
        const bool useBoundary = boundaryFound >= 0 && (found != ci || run == 0);
        const qsizetype pos = useBoundary ? boundaryFound : found;
        int s = 1;
        if (isBoundary(candidate, pos))
            s += 8;
        if (!m.positions.isEmpty() && m.positions.constLast() == pos - 1) {
            ++run;
            s += 4 + run;
        } else {
            run = 0;
            if (!m.positions.isEmpty())
                s -= std::min<int>(3, int(pos - m.positions.constLast()) / 4);
        }
        if (candidate.at(pos) == qc)
            s += 1; // exact case
        score += s;
        m.positions << int(pos);
        ci = pos + 1;
    }
    // Shorter candidates and matches near the end (file names in paths) rank higher.
    const auto lastSlash = candidate.lastIndexOf(u'/');
    if (lastSlash >= 0 && m.positions.constFirst() > lastSlash)
        score += 10;
    score -= int(candidate.size() / 16);
    m.score = std::max(0, score);
    return m;
}

QString highlight(const QString &candidate, const QList<int> &positions, const QString &color)
{
    QString out;
    qsizetype p = 0;
    for (qsizetype i = 0; i < candidate.size(); ++i) {
        const bool hit = p < positions.size() && positions.at(p) == i;
        QString ch = candidate.mid(i, 1);
        ch.replace(u'&', u"&amp;"_s).replace(u'<', u"&lt;"_s).replace(u'>', u"&gt;"_s);
        if (hit) {
            out += u"<span style=\"color:"_s + color + u"\">"_s + ch + u"</span>"_s;
            ++p;
        } else {
            out += ch;
        }
    }
    return out;
}

} // namespace gomp::fuzzy
