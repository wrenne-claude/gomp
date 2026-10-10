#include "util/YamlLite.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

using namespace gomp;
using namespace Qt::StringLiterals;

class TestYamlLite : public QObject
{
    Q_OBJECT
private slots:
    void parsesOmpConfigShapes()
    {
        const auto r = yaml::parse(u"symbolPreset: nerd\ntheme:\n  dark: dark-cavern\nmodelRoles:\n  default: a/b:high\n"
                                   "cycleOrder:\n  - smol\n  - default\nmemory:\n  backend: \"off\"\nempty: []\nflags: {a: 1, b: x}\n"_s);
        QVERIFY2(r.ok, qPrintable(r.error));
        const auto j = r.root.toJson().toObject();
        QCOMPARE(j.value("theme"_L1).toObject().value("dark"_L1).toString(), u"dark-cavern"_s);
        QCOMPARE(j.value("modelRoles"_L1).toObject().value("default"_L1).toString(), u"a/b:high"_s);
        QCOMPARE(j.value("cycleOrder"_L1).toArray().size(), 2);
        QCOMPARE(j.value("memory"_L1).toObject().value("backend"_L1).toString(), u"off"_s); // quoted stays a string
        QCOMPARE(j.value("flags"_L1).toObject().value("a"_L1).toDouble(), 1.0);
    }
    void roundTripKeepsOrderAndUnknownKeys()
    {
        const auto text = u"zeta: 1\nalpha:\n  b: true\n  a: \"x: y\"\nlist:\n  - name: one\n    url: https://e.com\n"_s;
        auto r = yaml::parse(text);
        QVERIFY(r.ok);
        r.root.setPath({u"alpha"_s, u"c"_s}, yaml::Node{yaml::Node::Type::Scalar, u"new"_s, true, {}, {}});
        const auto out = yaml::serialize(r.root);
        QVERIFY(out.indexOf(u"zeta"_s) < out.indexOf(u"alpha"_s));
        QVERIFY(out.indexOf(u"b: true"_s) < out.indexOf(u"c: new"_s));
        auto again = yaml::parse(out);
        QVERIFY2(again.ok, qPrintable(again.error));
        QCOMPARE(again.root.toJson(), r.root.toJson());
        QCOMPARE(again.root.atPath({u"alpha"_s, u"a"_s})->scalar, u"x: y"_s);
    }
    void quotingAvoidsTypeChanges()
    {
        auto root = yaml::Node::makeMap();
        root.setPath({u"s"_s}, yaml::Node::fromJson(u"true"_s));
        root.setPath({u"n"_s}, yaml::Node::fromJson(u"0123"_s));
        root.setPath({u"cmd"_s}, yaml::Node::fromJson(u"!secret-tool lookup service gomp"_s));
        const auto r = yaml::parse(yaml::serialize(root));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.root.toJson().toObject().value("s"_L1).toString(), u"true"_s);
        QCOMPARE(r.root.toJson().toObject().value("n"_L1).toString(), u"0123"_s);
        QCOMPARE(r.root.toJson().toObject().value("cmd"_L1).toString(), u"!secret-tool lookup service gomp"_s);
    }
    void blockScalars()
    {
        const auto r = yaml::parse(u"prompt: |\n  line one\n  line two\nnext: 1\n"_s);
        QVERIFY(r.ok);
        QCOMPARE(r.root.find(u"prompt"_s)->scalar, u"line one\nline two\n"_s);
        QCOMPARE(r.root.find(u"next"_s)->toJson().toDouble(), 1.0);
    }
    void reportsComments()
    {
        const auto r = yaml::parse(u"# top\na: 1 # trailing\nb: \"#not a comment\"\n"_s);
        QVERIFY(r.ok);
        QVERIFY(r.hasComments);
        QCOMPARE(r.root.find(u"b"_s)->scalar, u"#not a comment"_s);
    }
    void refusesWhatItCannotRoundTrip()
    {
        QVERIFY(!yaml::parse(u"a: &x 1\nb: *x\n"_s).ok);
        QVERIFY(!yaml::parse(u"a: [1, [2]]\n"_s).ok);
        QVERIFY(!yaml::parse(u"a: 1\n---\nb: 2\n"_s).ok);
        QVERIFY(!yaml::parse(u"a: !tag 1\n"_s).ok);
    }
    void removePathPrunesEmptyMaps()
    {
        auto r = yaml::parse(u"tools:\n  approvalMode: write\nother: 1\n"_s);
        QVERIFY(r.root.removePath({u"tools"_s, u"approvalMode"_s}));
        QVERIFY(!r.root.find(u"tools"_s));
        QVERIFY(r.root.find(u"other"_s));
    }
};

QTEST_GUILESS_MAIN(TestYamlLite)
#include "tst_yamllite.moc"
