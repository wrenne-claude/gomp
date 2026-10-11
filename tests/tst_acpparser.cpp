#include "adapter/AcpParser.h"

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
} // namespace

class TestAcpParser : public QObject
{
    Q_OBJECT
private slots:
    void askElicitationRoundTrip()
    {
        // The form omp builds for its `ask` tool over ACP (modes/acp: askDialog).
        const auto params = QJsonDocument::fromJson(R"({
          "mode": "form", "sessionId": "s", "message": "Answer 2 questions",
          "requestedSchema": {"type": "object", "properties": {
            "q0": {"type": "string", "title": "How should the site be built?", "description": "Stack",
                   "oneOf": [{"const": "Static HTML", "title": "Static HTML", "description": "No build step"},
                             {"const": "Astro", "title": "Astro"}],
                   "default": "Astro"},
            "q0__other": {"type": "string", "title": "Other"},
            "q1": {"type": "array", "title": "Which pages?", "items": {"anyOf": [{"const": "Home", "title": "Home"}, {"const": "Docs", "title": "Docs"}]}},
            "q1__other": {"type": "string", "title": "Other"}}}})").object();
        QVERIFY(acp::isAskElicitation(params));
        const auto ev = acp::uiRequestFromElicitation(u"r1"_s, params);
        QCOMPARE(ev.kind, UiRequestEvent::Kind::Ask);
        const auto qs = ev.raw.value("questions"_L1).toArray();
        QCOMPARE(qs.size(), 2);
        const auto q0 = qs.at(0).toObject();
        QCOMPARE(q0.value("id"_L1).toString(), u"q0"_s);
        QCOMPARE(q0.value("question"_L1).toString(), u"How should the site be built?"_s);
        QCOMPARE(q0.value("header"_L1).toString(), u"Stack"_s);
        QCOMPARE(q0.value("recommended"_L1).toInt(), 1);
        QCOMPARE(q0.value("options"_L1).toArray().at(0).toObject().value("description"_L1).toString(), u"No build step"_s);
        QVERIFY(qs.at(1).toObject().value("multi"_L1).toBool());

        const auto content = acp::askAnswerContent(params, QJsonDocument::fromJson(R"([
            {"id": "q0", "selectedOptions": ["Static HTML"], "customInput": ""},
            {"id": "q1", "selectedOptions": ["Home", "Docs"], "customInput": "and a changelog"}])").array());
        QCOMPARE(content.value("q0"_L1).toString(), u"Static HTML"_s);
        QCOMPARE(content.value("q1"_L1).toArray().size(), 2);
        QCOMPARE(content.value("q1__other"_L1).toString(), u"and a changelog"_s);
        QVERIFY(!content.contains("q0__other"_L1));
    }
    void permissionNamesItsToolCall()
    {
        acp::StreamState st;
        acp::parseSessionUpdate(QJsonDocument::fromJson(R"({"sessionUpdate":"tool_call","toolCallId":"t9","title":"rm -rf build","kind":"execute","rawInput":{"command":"rm -rf build"},"_meta":{"toolName":"bash"}})").object(), st);
        const auto ev = acp::approvalFromPermission(u"p1"_s, QJsonDocument::fromJson(R"({"toolCall":{"toolCallId":"t9","title":"rm -rf build","kind":"execute"},"options":[]})").object(), st);
        QCOMPARE(ev.toolCallId, u"t9"_s);
        QCOMPARE(ev.toolName, u"bash"_s);
        QVERIFY(ev.clientGated);
    }
    void chunksBecomeOneMessageWithBlocks()
    {
        acp::StreamState st;
        auto a = acp::parseSessionUpdate(json(R"({"sessionUpdate":"agent_thought_chunk","content":{"type":"text","text":"think"}})"), st);
        QCOMPARE(a.size(), 2); // message start + delta
        QVERIFY(std::holds_alternative<MessageStartEvent>(a.first()));
        auto b = acp::parseSessionUpdate(json(R"({"sessionUpdate":"agent_message_chunk","content":{"type":"text","text":"say"}})"), st);
        QCOMPARE(b.size(), 1);
        const auto &d = std::get<BlockDeltaEvent>(b.first());
        QCOMPARE(d.kind, BlockKind::Text);
        QCOMPARE(d.contentIndex, 1);
        const auto end = acp::finishTurn(st, u"stop"_s);
        QCOMPARE(end.size(), 1);
        QVERIFY(acp::finishTurn(st, u"stop"_s).isEmpty());
    }
    void toolCallCapturesPlanFile()
    {
        acp::StreamState st;
        // Plain literal: moc reads "//" inside raw strings as a comment.
        acp::parseSessionUpdate(json("{\"sessionUpdate\":\"tool_call\",\"toolCallId\":\"t1\",\"title\":\"Write plan\",\"kind\":\"edit\","
                                     "\"rawInput\":{\"path\":\"local://redis-plan.md\",\"content\":\"Plan: one step\"}}"), st);
        QCOMPARE(st.latestPlanPath, u"local://redis-plan.md"_s);
        const auto done = acp::parseSessionUpdate(json(R"({"sessionUpdate":"tool_call_update","toolCallId":"t1","status":"completed",
            "content":[{"type":"content","content":{"type":"text","text":"wrote"}}]})"), st);
        const auto &end = std::get<ToolEndEvent>(done.first());
        QCOMPARE(end.output, u"wrote"_s);
        QVERIFY(!end.isError);
    }
    void planElicitation()
    {
        acp::StreamState st;
        st.latestPlanMarkdown = u"# Full plan"_s;
        // Plain literal: moc mis-reads escaped quotes inside raw strings.
        const auto params = json("{\"mode\":\"form\",\"message\":\"Approve plan \\\"Redis\\\" and start implementation?\\n# Pl\","
                                 "\"requestedSchema\":{\"type\":\"object\",\"properties\":{\"value\":{\"type\":\"string\","
                                 "\"enum\":[\"Approve and execute\",\"Refine plan\"]}}}}");
        QVERIFY(acp::isPlanElicitation(params));
        const auto ev = acp::planProposalFrom(u"r1"_s, params, st);
        QCOMPARE(ev.title, u"Redis"_s);
        QCOMPARE(ev.planMarkdown, u"# Full plan"_s);
        QCOMPARE(ev.optionIds.size(), 2);
    }
    void confirmElicitation()
    {
        const auto params = json(R"({"message":"Delete?\nreally","requestedSchema":{"properties":{"value":{"type":"boolean"}}}})");
        QVERIFY(!acp::isPlanElicitation(params));
        const auto ev = acp::uiRequestFromElicitation(u"r2"_s, params);
        QCOMPARE(ev.kind, UiRequestEvent::Kind::Confirm);
        QCOMPARE(ev.title, u"Delete?"_s);
    }
    void modeAndPlanEntries()
    {
        acp::StreamState st;
        const auto m = acp::parseSessionUpdate(json(R"({"sessionUpdate":"current_mode_update","currentModeId":"plan"})"), st);
        QCOMPARE(std::get<ModeChangedEvent>(m.first()).mode, u"plan"_s);
        const auto p = acp::parseSessionUpdate(json(R"({"sessionUpdate":"plan","entries":[{"content":"a","status":"completed"},{"content":"b"}]})"), st);
        const auto &t = std::get<TodosEvent>(p.first());
        QCOMPARE(t.phases.first().tasks.size(), 2);
        QCOMPARE(t.phases.first().tasks.at(1).status, u"pending"_s);
    }
};

QTEST_GUILESS_MAIN(TestAcpParser)
#include "tst_acpparser.moc"
