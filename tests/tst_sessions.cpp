#include "adapter/MockBackend.h"
#include "model/SessionIndex.h"
#include "model/SessionTreeModel.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace gomp;
using namespace Qt::StringLiterals;

class TestSessions : public QObject
{
    Q_OBJECT
    QTemporaryDir dir;
    QString redis;
private slots:
    void initTestCase()
    {
        MockBackend::writeDemoSessions(dir.path());
        redis = dir.filePath(u"-home-dev-Projects-api/api-redis.jsonl"_s);
        QVERIFY(QFile::exists(redis));
    }
    void scanFile()
    {
        const auto info = SessionIndex::scanFile(redis);
        QCOMPARE(info.title, u"Move sessions to Redis"_s);
        QCOMPARE(info.cwd, u"/home/dev/Projects/api"_s);
        QCOMPARE(info.messages, 8);
        QCOMPARE(info.userMessages, 4);
        QVERIFY(info.branched);
        QVERIFY(info.compacted);
        QVERIFY(info.firstPrompt.startsWith(u"Plan a move"_s));
        QVERIFY(info.modified > 0);
    }
    void renameInPlace()
    {
        const auto copy = dir.filePath(u"copy.jsonl"_s);
        QVERIFY(QFile::copy(redis, copy));
        const auto size = QFileInfo(copy).size();
        QVERIFY(SessionIndex::rewriteTitle(copy, u"Sessions on Redis"_s));
        QCOMPARE(QFileInfo(copy).size(), size); // nothing after the first line moved
        QCOMPARE(SessionIndex::scanFile(copy).title, u"Sessions on Redis"_s);
        QVERIFY(!SessionIndex::rewriteTitle(copy, QString(400, u'x'))); // does not fit the pad
    }
    void treeLayout()
    {
        QFile f(redis);
        QVERIFY(f.open(QIODevice::ReadOnly));
        int lanes = 0, branches = 0;
        const auto nodes = SessionTreeModel::layout(f.readAll(), &lanes, &branches);
        QCOMPARE(branches, 1);
        QCOMPARE(lanes, 2);
        int onSide = 0;
        for (const auto &n : nodes) {
            if (n.lane == 1) {
                ++onSide;
                QVERIFY(!n.active);
            }
        }
        QCOMPARE(onSide, 2); // the Postgres alternative and its reply
        QVERIFY(nodes.last().active);
        QCOMPARE(nodes.last().lane, 0);
    }
};

QTEST_GUILESS_MAIN(TestSessions)
#include "tst_sessions.moc"
