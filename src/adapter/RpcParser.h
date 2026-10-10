#pragma once

#include "AgentEvents.h"

#include <QJsonObject>
#include <QJsonValue>

#include <optional>

namespace gomp::rpc {

// Reply to a command, correlated by id.
struct Response {
    QString id;
    QString command;
    bool success = false;
    QJsonValue data;
    QString error;
};

struct ParsedFrame {
    std::optional<Response> response;
    QList<AgentEvent> events;
    bool unknown = false; // frame type gomp does not handle (logged, never fatal)
};

// Parses one JSON-lines frame from `omp --mode rpc-ui`.
ParsedFrame parseFrame(const QJsonObject &frame);

// Helpers shared with the ACP parser and the session index.
ModelInfo parseModel(const QJsonObject &obj);
SessionSnapshot parseState(const QJsonObject &obj);
SessionStats parseStats(const QJsonObject &obj);
QList<TodoPhase> parseTodoPhases(const QJsonArray &arr);
QList<SlashCommand> parseCommands(const QJsonArray &arr);
QString contentText(const QJsonValue &content); // MessageContent / tool result content -> text
int contentImageCount(const QJsonValue &content);

// omp's approval prompt is a `select` whose options are exactly Approve / Deny.
bool isApprovalRequest(const QJsonObject &uiRequest);
QString toolNameFromApprovalTitle(const QString &title);

} // namespace gomp::rpc
