#pragma once

#include <QJsonValue>
#include <QList>
#include <QString>

#include <utility>

namespace gomp::yaml {

// An order-preserving YAML node for the block-style subset omp's config files use (maps,
// sequences, plain/quoted scalars, single-level flow collections, | and > block scalars).
// Anchors, aliases, tags, multi-document streams and nested flow collections are reported as
// unsupported so gomp falls back to the raw editor instead of corrupting the file.
struct Node {
    enum class Type { Null, Scalar, Map, Seq };
    Type type = Type::Null;
    QString scalar;
    bool quoted = false;
    QList<std::pair<QString, Node>> map;
    QList<Node> seq;

    static Node makeMap() { return Node{Type::Map, {}, false, {}, {}}; }
    [[nodiscard]] const Node *find(const QString &key) const;
    Node *find(const QString &key);
    // Sets a value at a dotted path, creating intermediate maps; keeps existing key order.
    void setPath(const QStringList &path, const Node &value);
    bool removePath(const QStringList &path);
    [[nodiscard]] const Node *atPath(const QStringList &path) const;

    [[nodiscard]] QJsonValue toJson() const;
    // Builds a node from JSON. When `previous` is given, existing keys keep their order and new
    // keys are appended, so rewriting a file changes only what actually changed.
    static Node fromJson(const QJsonValue &value, const Node *previous = nullptr);
};

struct ParseResult {
    Node root;
    bool ok = false;
    QString error;
    int errorLine = 0;
    bool hasComments = false;
};

ParseResult parse(const QString &text);
QString serialize(const Node &root);

} // namespace gomp::yaml
