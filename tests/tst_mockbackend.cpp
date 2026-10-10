#include "adapter/MockBackend.h"
#include "model/ConversationModel.h"
#include "model/Workspace.h"

#include <QSignalSpy>
#include <QTest>

using namespace gomp;
using namespace Qt::StringLiterals;

// End to end through the real parser and models: scripted turns drive the workspace through
// streaming, approval and plan review, as the UI would see them.
class TestMockBackend : public QObject
{
    Q_OBJECT
    Workspace *make(MockBackend **backend)
    {
        auto *b = new MockBackend;
        b->setTimeScale(25);
        *backend = b;
        auto *w = new Workspace(QDir::tempPath(), b, true, this);
        w->start({});
        return w;
    }
private slots:
    void noticesAFallbackFromTheDefaultModel()
    {
        MockBackend *b = nullptr;
        auto *w = make(&b);
        QTRY_COMPARE(w->connection(), Workspace::Connection::Ready);
        const int before = w->conversation()->rowCount();
        w->setExpectedModel(u"slowprovider/flash-model"_s); // config loaded after omp's first state
        QCOMPARE(w->conversation()->rowCount(), before + 1);
        const auto text = w->conversation()->data(w->conversation()->index(before), ConversationModel::TextRole).toString();
        QVERIFY(text.contains(u"slowprovider/flash-model"_s));
        w->setExpectedModel(u"other/model"_s); // only once per start
        QCOMPARE(w->conversation()->rowCount(), before + 1);
    }
    void noFallbackNoticeWhenTheDefaultIsUsed()
    {
        MockBackend *b = nullptr;
        auto *w = make(&b);
        w->setExpectedModel(u"anthropic/claude-sonnet-5-5"_s); // known before omp's first state
        QTRY_COMPARE(w->connection(), Workspace::Connection::Ready);
        QCOMPARE(w->conversation()->rowCount(), 0);
    }
    void streamsAnAnswer()
    {
        MockBackend *b = nullptr;
        auto *w = make(&b);
        QTRY_COMPARE(w->connection(), Workspace::Connection::Ready);
        QVERIFY(w->commands()->rowCount() > 0);
        w->send(u"Explain how session expiry works"_s);
        QTRY_VERIFY(w->busy());
        QTRY_VERIFY_WITH_TIMEOUT(!w->busy(), 10000);
        QCOMPARE(w->activity(), Workspace::Activity::Idle);
        QVERIFY(w->conversation()->lastAssistantText().contains(u"sweeper"_s));
        QCOMPARE(w->sessionTitle(), u"How session expiry works"_s);
    }
    void approvalPausesTheTurn()
    {
        MockBackend *b = nullptr;
        auto *w = make(&b);
        QTRY_COMPARE(w->connection(), Workspace::Connection::Ready);
        QSignalSpy attention(w, &Workspace::attention);
        w->send(u"Fix the failing rate limiter test"_s);
        QTRY_VERIFY_WITH_TIMEOUT(w->pendingApprovals() == 1, 10000);
        QCOMPARE(w->activity(), Workspace::Activity::Approval);
        QVERIFY(!attention.isEmpty());
        w->approveAll();
        QTRY_VERIFY_WITH_TIMEOUT(!w->busy(), 10000);
        QVERIFY(w->conversation()->lastAssistantText().startsWith(u"Fixed"_s));
    }
    void planReview()
    {
        MockBackend *b = nullptr;
        auto *w = make(&b);
        QTRY_COMPARE(w->connection(), Workspace::Connection::Ready);
        w->setMode(u"plan"_s);
        QTRY_COMPARE(w->mode(), u"plan"_s);
        w->send(u"Plan the Redis move"_s);
        QTRY_VERIFY_WITH_TIMEOUT(w->planPending(), 10000);
        QVERIFY(w->todos()->total() >= 7);
        w->answerPlan(u"Approve and execute"_s);
        QTRY_COMPARE(w->mode(), u"default"_s);
        QTRY_VERIFY_WITH_TIMEOUT(!w->busy(), 10000);
        QVERIFY(w->todos()->completed() >= 1);
    }
    void queuedFollowUpRuns()
    {
        MockBackend *b = nullptr;
        auto *w = make(&b);
        QTRY_COMPARE(w->connection(), Workspace::Connection::Ready);
        w->send(u"Explain how session expiry works"_s);
        QTRY_VERIFY(w->busy());
        w->send(u"Audit the API in parallel"_s);
        QTRY_COMPARE(w->queuedMessages().size(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(w->subagents()->rowCount() == 4, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!w->busy() && w->subagents()->running() == 0, 15000);
    }
    void stopAborts()
    {
        MockBackend *b = nullptr;
        auto *w = make(&b);
        b->setTimeScale(1);
        QTRY_COMPARE(w->connection(), Workspace::Connection::Ready);
        w->send(u"Explain how session expiry works"_s);
        QTRY_VERIFY(w->busy());
        w->stop();
        QTRY_VERIFY(!w->busy());
    }
};

QTEST_MAIN(TestMockBackend)
#include "tst_mockbackend.moc"
