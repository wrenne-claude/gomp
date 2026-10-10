#include "adapter/OmpBackend.h"
#include "adapter/OmpLocator.h"

#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace gomp;
using namespace Qt::StringLiterals;

// Talks to the installed omp without ever prompting a model: handshake, state, commands,
// models, and the plan-mode transport handoff (RPC to ACP and back). Opt in with
// GOMP_TEST_REAL_OMP=1; it uses your omp configuration but sends nothing to a provider.
class TestRealOmp : public QObject
{
    Q_OBJECT
    QString omp;
    QTemporaryDir project;
private slots:
    void initTestCase()
    {
        if (qEnvironmentVariable("GOMP_TEST_REAL_OMP") != "1"_L1)
            QSKIP("set GOMP_TEST_REAL_OMP=1 to run against the installed omp");
        OmpLocator locator;
        QSignalSpy changed(&locator, &OmpLocator::changed);
        locator.locate();
        QTRY_VERIFY_WITH_TIMEOUT(locator.status() != OmpLocator::Status::Checking, 15000);
        if (locator.status() != OmpLocator::Status::Found && locator.status() != OmpLocator::Status::Untested)
            QSKIP("omp is not installed");
        omp = locator.path();
    }
    void handshakeAndPlanHandoff()
    {
        OmpBackend backend;
        QList<AgentEvent> events;
        connect(&backend, &AgentBackend::agentEvent, this, [&](const AgentEvent &e) { events << e; });
        LaunchOptions o;
        o.ompPath = omp;
        o.cwd = project.path();
        o.extraArgs = {u"--no-session"_s};
        backend.start(o);
        auto has = [&](auto pred) { return std::any_of(events.cbegin(), events.cend(), pred); };
        QTRY_VERIFY_WITH_TIMEOUT(has([](const AgentEvent &e) { return std::holds_alternative<ReadyEvent>(e); }), 30000);
        QTRY_VERIFY_WITH_TIMEOUT(has([](const AgentEvent &e) { return std::holds_alternative<StateEvent>(e); }), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(has([](const AgentEvent &e) {
            const auto *c = std::get_if<CommandsEvent>(&e);
            return c && !c->commands.isEmpty();
        }), 15000);
        QCOMPARE(backend.transportName(), u"rpc"_s);

        bool modelsOk = false;
        backend.call(u"get_available_models"_s, {}, [&](bool ok, const QJsonValue &data, const QString &) {
            modelsOk = ok && data.isArray();
        });
        QTRY_VERIFY_WITH_TIMEOUT(modelsOk, 15000);

        events.clear();
        backend.setMode(u"plan"_s);
        QTRY_VERIFY_WITH_TIMEOUT(has([](const AgentEvent &e) {
            const auto *m = std::get_if<ModeChangedEvent>(&e);
            return m && m->mode == "plan"_L1;
        }), 45000);
        QCOMPARE(backend.transportName(), u"acp"_s);

        events.clear();
        backend.setMode(u"default"_s);
        QTRY_VERIFY_WITH_TIMEOUT(backend.transportName() == "rpc"_L1 && has([](const AgentEvent &e) {
            return std::holds_alternative<ReadyEvent>(e);
        }), 45000);
        backend.stop();
    }
};

QTEST_GUILESS_MAIN(TestRealOmp)
#include "tst_realomp.moc"
