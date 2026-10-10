#pragma once

#include "AgentEvents.h"

#include <QJsonObject>
#include <QHash>
#include <QSet>

namespace gomp::acp {

// ACP streams chunks without message ids; this tracks message boundaries so chunks map onto the
// same block model RPC produces.
struct StreamState {
    int messageSeq = 0;
    QString currentMessageId;
    BlockKind lastKind = BlockKind::Text;
    int contentIndex = 0;
    bool inAssistantMessage = false;
    QSet<QString> openToolCalls;
    QHash<QString, QString> toolNames;
    QString latestPlanMarkdown; // last content written to a *plan*.md via the write tool
    QString latestPlanPath;
};

QList<AgentEvent> parseSessionUpdate(const QJsonObject &update, StreamState &state);

// Closes an open assistant message (turn ended); returns the MessageEnd event if one was open.
QList<AgentEvent> finishTurn(StreamState &state, const QString &stopReason);

// Plan approval arrives as `elicitation/create` with message "Approve plan \"<title>\" ...".
bool isPlanElicitation(const QJsonObject &params);
PlanProposalEvent planProposalFrom(const QString &requestId, const QJsonObject &params, const StreamState &state);
UiRequestEvent uiRequestFromElicitation(const QString &requestId, const QJsonObject &params);
ApprovalRequestEvent approvalFromPermission(const QString &requestId, const QJsonObject &params);

} // namespace gomp::acp
