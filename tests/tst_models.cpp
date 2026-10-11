#include "model/CommandModel.h"
#include "model/ConversationModel.h"
#include "model/SubagentModel.h"
#include "model/TodoModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>

using namespace gomp;
using namespace Qt::StringLiterals;

class TestModels : public QObject
{
    Q_OBJECT
private slots:
    void conversationStreamsIntoBlocks()
    {
        ConversationModel m;
        m.apply(MessageStartEvent{u"u1"_s, u"user"_s, u"hello"_s, 0, 0});
        m.apply(MessageStartEvent{u"a1"_s, u"assistant"_s, {}, 0, 0});
        m.apply(BlockDeltaEvent{u"a1"_s, 0, BlockKind::Thinking, u"think"_s});
        m.apply(BlockDeltaEvent{{}, 1, BlockKind::Text, u"Hel"_s}); // empty id: current message
        m.apply(BlockDeltaEvent{u"a1"_s, 1, BlockKind::Text, u"lo"_s});
        QCOMPARE(m.rowCount(), 3);
        QVERIFY(m.isStreamingText());
        QCOMPARE(m.data(m.index(2), ConversationModel::TextRole).toString(), u"Hello"_s);
        m.apply(MessageEndEvent{u"a1"_s, u"assistant"_s, u"stop"_s, {}, 0, 0, 0, 0});
        QVERIFY(!m.data(m.index(2), ConversationModel::StreamingRole).toBool());
    }
    void toolApprovalFlow()
    {
        ConversationModel m;
        m.apply(ToolStartEvent{u"c1"_s, u"edit"_s, QJsonDocument::fromJson(R"({"path":"a.ts","edits":[{"old_text":"a","new_text":"b"}]})").object(), {}, {}});
        QCOMPARE(m.runningToolCount(), 1);
        const auto row = m.index(0);
        QCOMPARE(m.data(row, ConversationModel::DiffAddedRole).toInt(), 1);
        m.attachApproval(ApprovalRequestEvent{u"ui1"_s, u"edit"_s, u"Allow tool: edit"_s, {}, {}, false});
        QCOMPARE(m.data(row, ConversationModel::ToolStatusRole).toString(), u"awaiting"_s);
        m.resolveApproval(u"ui1"_s, false);
        QCOMPARE(m.data(row, ConversationModel::ToolStatusRole).toString(), u"denied"_s);
        QCOMPARE(m.runningToolCount(), 0);
    }
    void approvalsNeverLandOnStaleCalls()
    {
        ConversationModel m;
        // A bash call from an earlier turn whose end event never arrived (an interrupted turn).
        m.apply(ToolStartEvent{u"old"_s, u"bash"_s, {{"command"_L1, u"ls"_s}}, {}, {}});
        m.settleTools();
        QCOMPARE(m.data(m.index(0), ConversationModel::ToolStatusRole).toString(), u"stopped"_s);
        QCOMPARE(m.runningToolCount(), 0);
        // A new approval for bash must not reopen it; it gets its own row.
        m.attachApproval(ApprovalRequestEvent{u"a1"_s, u"bash"_s, u"Allow tool: bash"_s, {}, {}, false});
        QCOMPARE(m.data(m.index(0), ConversationModel::ToolStatusRole).toString(), u"stopped"_s);
        QCOMPARE(m.rowCount(), 2);
        QCOMPARE(m.data(m.index(1), ConversationModel::ApprovalIdRole).toString(), u"a1"_s);
    }
    void approvalFindsItsExactCall()
    {
        ConversationModel m;
        m.apply(ToolStartEvent{u"first"_s, u"bash"_s, {{"command"_L1, u"make"_s}}, {}, {}});
        m.apply(ToolStartEvent{u"second"_s, u"bash"_s, {{"command"_L1, u"make test"_s}}, {}, {}});
        m.attachApproval(ApprovalRequestEvent{u"a1"_s, u"bash"_s, u"make"_s, {}, u"first"_s, true});
        QCOMPARE(m.data(m.index(0), ConversationModel::ToolStatusRole).toString(), u"awaiting"_s);
        QCOMPARE(m.data(m.index(1), ConversationModel::ToolStatusRole).toString(), u"running"_s);
    }
    void toolTitles()
    {
        QCOMPARE(ConversationModel::toolTitle(u"bash"_s, {{"command"_L1, u"npm test\nmore"_s}}), u"npm test"_s);
        QCOMPARE(ConversationModel::toolTitle(u"read"_s, {{"path"_L1, u"src/a.ts"_s}}), u"src/a.ts"_s);
        QCOMPARE(ConversationModel::toolTitle(u"task"_s, {{"tasks"_L1, QJsonArray{QJsonObject{}, QJsonObject{}}}}), u"2 parallel tasks"_s);
    }
    void historyLoad()
    {
        ConversationModel m;
        const auto msgs = QJsonDocument::fromJson(R"([
            {"role":"user","content":[{"type":"text","text":"go"}],"timestamp":1},
            {"role":"assistant","content":[{"type":"thinking","thinking":"t"},{"type":"toolCall","id":"c","name":"bash","arguments":{"command":"ls"}},{"type":"text","text":"done"}]},
            {"role":"toolResult","toolCallId":"c","content":[{"type":"text","text":"a\nb"}],"isError":true}])").array();
        m.loadHistory(msgs);
        QCOMPARE(m.rowCount(), 4);
        QCOMPARE(m.data(m.index(2), ConversationModel::ToolStatusRole).toString(), u"error"_s);
        QCOMPARE(m.data(m.index(2), ConversationModel::ToolOutputRole).toString(), u"a\nb"_s);
    }
    void planMarkdownToSteps()
    {
        const auto phases = TodoModel::fromMarkdown(u"# Title\nIntro\n## Prepare\n- [ ] Add `REDIS_URL` **now**\n  - nested detail\n- [x] Done thing\n## Verify\n1. Run tests\n"_s);
        QCOMPARE(phases.size(), 2);
        QCOMPARE(phases[0].tasks.size(), 2);
        QCOMPARE(phases[0].tasks[0].content, u"Add REDIS_URL now"_s);
        QCOMPARE(phases[0].tasks[0].details, u"- nested detail"_s);
        QCOMPARE(phases[0].tasks[1].status, u"completed"_s);
        QCOMPARE(phases[1].tasks[0].content, u"Run tests"_s);
    }
    void todoEditing()
    {
        TodoModel m;
        m.setPhases(TodoModel::fromMarkdown(u"- [ ] a\n- [ ] b\n"_s));
        QSignalSpy edited(&m, &TodoModel::edited);
        m.insertAfter(0, u"x"_s);
        m.setStatus(2, u"completed"_s);
        m.removeAt(1);
        QCOMPARE(edited.count(), 3);
        QCOMPARE(m.toMarkdown(), u"## Plan\n\n- [ ] a\n- [x] b\n"_s);
    }
    void subagentProgress()
    {
        SubagentModel m;
        m.apply(SubagentLifecycleEvent{u"A"_s, u"scout"_s, u"look"_s, u"started"_s, {}, 0, false});
        SubagentProgressEvent p;
        p.id = u"A"_s;
        p.agent = u"scout"_s;
        p.status = u"running"_s;
        p.toolCount = 3;
        m.apply(p);
        m.apply(SubagentOutputEvent{u"A"_s, u"text"_s, {}});
        QTRY_COMPARE(m.running(), 1);
        m.apply(SubagentLifecycleEvent{u"A"_s, u"scout"_s, {}, u"completed"_s, {}, 0, false});
        p.status = u"running"_s; // late progress must not revive a finished agent
        m.apply(p);
        QTRY_COMPARE(m.finished(), 1);
        QCOMPARE(m.get(0).value(u"output"_s).toString(), u"text"_s);
    }
    void commandFilter()
    {
        CommandModel m;
        m.setCommands({SlashCommand{u"compact"_s, {}, {}, {}, {}, u"builtin"_s}, SlashCommand{u"context"_s, {}, {}, {}, {}, u"builtin"_s},
                       SlashCommand{u"review"_s, {}, {}, {}, {}, u"custom"_s}});
        m.setQuery(u"co"_s);
        QCOMPARE(m.rowCount(), 2);
        m.setQuery(u"rev"_s);
        QCOMPARE(m.nameAt(0), u"review"_s);
    }
};

QTEST_MAIN(TestModels)
#include "tst_models.moc"
