#include "adapter/OmpLocator.h"
#include "app/EffectsProbe.h"
#include "config/ProviderStore.h"
#include "util/Format.h"
#include "util/FuzzyMatch.h"
#include "util/GitInfo.h"
#include "util/Keyring.h"
#include "util/LineDiff.h"
#include "util/Markdown.h"
#include "util/SyntaxHighlighter.h"

#include <QTest>

using namespace gomp;
using namespace Qt::StringLiterals;

class TestUtil : public QObject
{
    Q_OBJECT
private slots:
    void lineDiff()
    {
        const auto lines = diff::compute(u"a\nb\nc\nd"_s, u"a\nB\nc\nd\ne"_s, 1);
        const auto st = diff::stats(lines);
        QCOMPARE(st.added, 2);
        QCOMPARE(st.removed, 1);
        // Removals come before additions, as in unified diffs.
        qsizetype rem = -1, add = -1;
        for (qsizetype i = 0; i < lines.size(); ++i) {
            if (lines[i].kind == diff::Line::Kind::Removed && rem < 0) rem = i;
            if (lines[i].kind == diff::Line::Kind::Added && add < 0) add = i;
        }
        QVERIFY(rem < add);
    }
    void ompDiffFormat()
    {
        const auto lines = diff::parseOmpDiff(u" 145|  keep\n-147|old\n+147|new"_s);
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines[1].kind, diff::Line::Kind::Removed);
        QCOMPARE(lines[1].oldNo, 147);
        QCOMPARE(lines[2].text, u"new"_s);
    }
    void markdownSplit()
    {
        const auto segs = md::split(u"Intro\n\n```ts\nconst a = 1\n```\nAfter\n```py\nopen"_s);
        QCOMPARE(segs.size(), 4);
        QCOMPARE(segs[1].language, u"ts"_s);
        QCOMPARE(segs[1].text, u"const a = 1"_s);
        QVERIFY(!segs[1].open);
        QVERIFY(segs[3].open); // still streaming
    }
    void markdownPreview()
    {
        QCOMPARE(md::plainPreview(u"**Fix** the `bug` in styles/*.css, see [docs](http://x)"_s),
                 u"Fix the bug in styles/*.css, see docs"_s);
    }
    void markdownHtml()
    {
        const auto html = md::toHtml(u"Use `get()` and [link](https://e.com)"_s, md::Style{});
        QVERIFY(html.contains(u"JetBrains Mono"_s));
        QVERIFY(html.contains(u"https://e.com"_s));
    }
    void fuzzy()
    {
        QVERIFY(fuzzy::match(u"cmp"_s, u"compact"_s).matched());
        QVERIFY(!fuzzy::match(u"xyz"_s, u"compact"_s).matched());
        // Word starts beat scattered matches; file names beat directories.
        QVERIFY(fuzzy::match(u"rl"_s, u"src/rateLimit.ts"_s).score > fuzzy::match(u"rl"_s, u"src/overall.ts"_s).score);
        QVERIFY(fuzzy::match(u"Comp"_s, u"compact"_s).score < 0 || !fuzzy::match(u"Comp"_s, u"compact"_s).matched()); // smart case
    }
    void highlighting()
    {
        syntax::Palette p{u"t"_s, u"kw"_s, u"ty"_s, u"st"_s, u"nu"_s, u"co"_s, u"fn"_s, u"pu"_s, u"ad"_s, u"rm"_s, u"me"_s};
        const auto html = syntax::toHtml(u"if (x < 3) return \"a\"; // c"_s, u"ts"_s, p);
        QVERIFY(html.contains(u"color:kw\">if"_s));
        QVERIFY(html.contains(u"&lt;"_s));
        QVERIFY(html.contains(u"color:st\">\"a\""_s));
        QVERIFY(html.contains(u"color:co\">// c"_s));
        QCOMPARE(syntax::canonicalLanguage(u"tsx"_s), u"typescript"_s);
    }
    void formatting()
    {
        QCOMPARE(Format::tokens(950), u"950"_s);
        QCOMPARE(Format::tokens(12400), u"12.4k"_s);
        QCOMPARE(Format::tokens(1'000'000), u"1.00M"_s);
        QCOMPARE(Format::cost(0), u"—"_s);
        QCOMPARE(Format::cost(1.234), u"$1.23"_s);
        QCOMPARE(Format::duration(340), u"340 ms"_s);
        QCOMPARE(Format::count(1, u"message"_s, u"messages"_s), u"1 message"_s);
        QCOMPARE(Format::count(3, u"message"_s, u"messages"_s), u"3 messages"_s);
    }
    void versions()
    {
        QCOMPARE(OmpLocator::parseVersion(u"omp/18.8.7"_s), (QList<int>{18, 8, 7}));
        QVERIFY(OmpLocator::isSupported({18, 9, 0}));
        QVERIFY(!OmpLocator::isSupported({19, 0, 0}));
        QVERIFY(OmpLocator::parseVersion(u"nonsense"_s).isEmpty());
    }
    void envHelp()
    {
        const auto vars = ProviderStore::parseEnvHelp(u"  # Core\n  ANTHROPIC_API_KEY          - Anthropic Claude models\n"
                                                      "  COPILOT_GITHUB_TOKEN      - GitHub Copilot\n  FOO_BAR - not a key\n"_s);
        QCOMPARE(vars.size(), 2);
        QCOMPARE(vars.first().first, u"ANTHROPIC_API_KEY"_s);
    }
    void keyringCommand()
    {
        QCOMPARE(Keyring::lookupCommand(u"myprov"_s), u"!secret-tool lookup service gomp provider myprov"_s);
        QCOMPARE(Keyring::lookupCommand(u"env:A B"_s), u"!secret-tool lookup service gomp provider 'env:A B'"_s);
        QVERIFY(Keyring::isGompLookupCommand(Keyring::lookupCommand(u"x"_s)));
    }
    void gitHead()
    {
        QCOMPARE(GitInfo::branchFromHead(u"ref: refs/heads/main\n"_s), u"main"_s);
        QCOMPARE(GitInfo::branchFromHead(u"0123456789abcdef"_s), u"01234567"_s);
    }
    void softwareRenderers()
    {
        QVERIFY(EffectsProbe::isSoftwareRendererName(u"llvmpipe (LLVM 19.1.0, 256 bits)"_s));
        QVERIFY(!EffectsProbe::isSoftwareRendererName(u"NVIDIA GeForce RTX 5060/PCIe/SSE2"_s));
    }
};

QTEST_GUILESS_MAIN(TestUtil)
#include "tst_util.moc"
