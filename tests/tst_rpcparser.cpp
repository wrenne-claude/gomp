#include "adapter/RpcBackend.h"
#include "adapter/RpcParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

using namespace gomp;
using namespace Qt::StringLiterals;

namespace {
QJsonObject json(const char *text)
{
    return QJsonDocument::fromJson(QByteArray(text)).object();
}
template<typename T>
const T *first(const QList<AgentEvent> &events)
{
    for (const auto &e : events)
        if (const auto *p = std::get_if<T>(&e))
            return p;
    return nullptr;
}
} // namespace

class TestRpcParser : public QObject
{
    Q_OBJECT
private slots:
    void ready()
    {
        const auto f = rpc::parseFrame(json(R"({"type":"ready","protocolVersion":1,"supportedProtocolVersions":[1,2]})"));
        const auto *r = first<ReadyEvent>(f.events);
        QVERIFY(r);
        QCOMPARE(r->protocolVersion, 1);
        QCOMPARE(r->transport, u"rpc"_s);
    }
    void response()
    {
        const auto f = rpc::parseFrame(json(R"({"type":"response","id":"g7","command":"get_state","success":false,"error":"nope"})"));
        QVERIFY(f.response.has_value());
        QCOMPARE(f.response->id, u"g7"_s);
        QCOMPARE(f.response->command, u"get_state"_s);
        QVERIFY(!f.response->success);
        QCOMPARE(f.response->error, u"nope"_s);
    }
    void streamingDeltas()
    {
        const auto f = rpc::parseFrame(json(R"({"type":"message_update","messageId":"m1",
            "assistantMessageEvent":{"type":"thinking_delta","contentIndex":0,"delta":"hmm"}})"));
        const auto *d = first<BlockDeltaEvent>(f.events);
        QVERIFY(d);
        QCOMPARE(d->messageId, u"m1"_s);
        QCOMPARE(d->kind, BlockKind::Thinking);
        QCOMPARE(d->delta, u"hmm"_s);

        const auto end = rpc::parseFrame(json(R"({"type":"message_update","messageId":"m1",
            "assistantMessageEvent":{"type":"text_end","contentIndex":1,"content":"Done."}})"));
        const auto *e = first<BlockEndEvent>(end.events);
        QVERIFY(e);
        QCOMPARE(e->kind, BlockKind::Text);
        QCOMPARE(e->content, u"Done."_s);
    }
    void messageEndUsage()
    {
        const auto f = rpc::parseFrame(json(R"({"type":"message_end","messageId":"m1","message":{"role":"assistant",
            "stopReason":"error","errorMessage":"429","duration":1200,"usage":{"input":10,"output":20,"cost":{"total":0.5}}}})"));
        const auto *e = first<MessageEndEvent>(f.events);
        QVERIFY(e);
        QCOMPARE(e->stopReason, u"error"_s);
        QCOMPARE(e->errorMessage, u"429"_s);
        QCOMPARE(e->outputTokens, 20);
        QCOMPARE(e->cost, 0.5);
    }
    void hiddenCustomMessagesAreDropped()
    {
        const auto f = rpc::parseFrame(json(R"({"type":"message_start","messageId":"x","message":{"role":"custom","display":false,"content":"plan-mode-context"}})"));
        QVERIFY(f.events.isEmpty());
    }
    void toolLifecycleWithDiff()
    {
        const auto s = rpc::parseFrame(json(R"({"type":"tool_execution_start","toolCallId":"c1","toolName":"edit","args":{"path":"a.ts"},"intent":"Fix it"})"));
        const auto *start = first<ToolStartEvent>(s.events);
        QVERIFY(start);
        QCOMPARE(start->toolName, u"edit"_s);
        QCOMPARE(start->args.value("path"_L1).toString(), u"a.ts"_s);
        QCOMPARE(start->intent, u"Fix it"_s);

        const auto e = rpc::parseFrame(json(R"({"type":"tool_execution_end","toolCallId":"c1","toolName":"edit","isError":false,
            "result":{"content":[{"type":"text","text":"ok"}],"details":{"diff":"-1|a\n+1|b"}}})"));
        const auto *end = first<ToolEndEvent>(e.events);
        QVERIFY(end);
        QCOMPARE(end->output, u"ok"_s);
        QCOMPARE(end->diff, u"-1|a\n+1|b"_s);
    }
    void approvalIsRecognised()
    {
        const auto f = rpc::parseFrame(json(R"({"type":"extension_ui_request","id":"u1","method":"select",
            "title":"Allow tool: bash\nReason: exec tier","options":["Approve","Deny"]})"));
        const auto *a = first<ApprovalRequestEvent>(f.events);
        QVERIFY(a);
        QCOMPARE(a->requestId, u"u1"_s);
        QCOMPARE(a->toolName, u"bash"_s);
        QCOMPARE(a->reason, u"exec tier"_s);
        QVERIFY(!first<UiRequestEvent>(f.events));
    }
    void genericSelectIsAUiRequest()
    {
        const auto f = rpc::parseFrame(json(R"({"type":"extension_ui_request","id":"u2","method":"select","title":"Pick","options":["A","B"]})"));
        const auto *u = first<UiRequestEvent>(f.events);
        QVERIFY(u);
        QCOMPARE(u->kind, UiRequestEvent::Kind::Select);
        QCOMPARE(u->options, (QStringList{u"A"_s, u"B"_s}));
    }
    void presentationRequestsAreIgnored()
    {
        const auto f = rpc::parseFrame(json(R"({"type":"extension_ui_request","id":"w","method":"setWidget","widgetKey":"x"})"));
        QVERIFY(f.events.isEmpty());
        QVERIFY(!f.unknown);
    }
    void subagents()
    {
        const auto l = rpc::parseFrame(json(R"({"type":"subagent_lifecycle","payload":{"id":"A1","agent":"scout","status":"started","index":0,"parentToolCallId":"t"}})"));
        QVERIFY(first<SubagentLifecycleEvent>(l.events));
        const auto p = rpc::parseFrame(json(R"({"type":"subagent_progress","payload":{"index":0,"agent":"scout","task":"Look",
            "progress":{"id":"A1","status":"running","currentTool":"grep","recentTools":["read"],"toolCount":2,"tokens":900,"cost":0.01}}})"));
        const auto *pe = first<SubagentProgressEvent>(p.events);
        QVERIFY(pe);
        QCOMPARE(pe->id, u"A1"_s);
        QCOMPARE(pe->currentTool, u"grep"_s);
        QCOMPARE(pe->toolCount, 2);
        const auto ev = rpc::parseFrame(json(R"({"type":"subagent_event","payload":{"id":"A1","event":{"type":"message_update",
            "assistantMessageEvent":{"type":"text_delta","delta":"found it"}}}})"));
        const auto *out = first<SubagentOutputEvent>(ev.events);
        QVERIFY(out);
        QCOMPARE(out->text, u"found it"_s);
    }
    void promptResult()
    {
        const auto f = rpc::parseFrame(json(R"({"type":"prompt_result","id":"g3","status":"error","agentInvoked":true,"sessionSettled":false,
            "error":{"message":"overloaded","retryable":true}})"));
        const auto *r = first<PromptResultEvent>(f.events);
        QVERIFY(r);
        QCOMPARE(r->requestId, u"g3"_s);
        QVERIFY(r->retryable);
        QVERIFY(!r->settled);
    }
    void state()
    {
        const auto s = rpc::parseState(json(R"({"model":{"provider":"anthropic","id":"m","name":"M","contextWindow":1000,"input":["text","image"],
            "thinking":{"efforts":["low","high"]}},"thinkingLevel":"high","sessionId":"s","isStreaming":true,
            "contextUsage":{"tokens":250,"contextWindow":1000},"queuedMessages":{"steering":["a"],"followUp":["b"]},
            "todoPhases":[{"name":"P","tasks":[{"content":"x","status":"in_progress"}]}],"goal":{"enabled":true,"goal":{"objective":"ship"}}})"));
        QCOMPARE(s.model.selector(), u"anthropic/m"_s);
        QVERIFY(s.model.acceptsImages);
        QCOMPARE(s.model.thinkingLevels, (QStringList{u"low"_s, u"high"_s}));
        QCOMPARE(s.contextTokens, 250);
        QCOMPARE(s.queuedFollowUp, QStringList{u"b"_s});
        QCOMPARE(s.todoPhases.size(), 1);
        QVERIFY(s.goalActive);
        QCOMPARE(s.goalObjective, u"ship"_s);
    }
    void commands()
    {
        const auto cmds = rpc::parseCommands(json(R"({"c":[{"name":"compact","description":"d","input":{"hint":"[x]"},
            "subcommands":[{"name":"soft"}],"source":"builtin"},{"description":"no name"}]})").value("c"_L1).toArray());
        QCOMPARE(cmds.size(), 1);
        QCOMPARE(cmds.first().inputHint, u"[x]"_s);
        QCOMPARE(cmds.first().subcommands, QStringList{u"soft"_s});
    }
    void launchArguments()
    {
        LaunchOptions o;
        o.cwd = u"/p"_s;
        o.resumeSession = u"/s.jsonl"_s;
        o.approvalMode = u"write"_s;
        const auto args = RpcBackend::buildArguments(o);
        QCOMPARE(args.mid(0, 2), (QStringList{u"--mode"_s, u"rpc-ui"_s}));
        QVERIFY(args.contains(u"--resume"_s));
        QVERIFY(args.contains(u"--approval-mode"_s));
        QVERIFY(!args.contains(u"--continue"_s));
    }
};

QTEST_GUILESS_MAIN(TestRpcParser)
#include "tst_rpcparser.moc"
