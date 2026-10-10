#include "YamlLite.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace gomp::yaml {

namespace {

struct SrcLine {
    int number = 0; // 1-based
    int indent = 0;
    QString text;   // content without indentation and comments
    QString raw;    // original line, for block scalars
};

struct Unsupported {
    QString message;
    int line;
};

// Strips a trailing comment that is outside quotes. Returns true when a comment was found.
bool stripComment(QString &s)
{
    QChar quote;
    for (qsizetype i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (!quote.isNull()) {
            if (c == u'\\' && quote == u'"')
                ++i;
            else if (c == quote)
                quote = QChar();
            continue;
        }
        if (c == u'"' || c == u'\'') {
            quote = c;
        } else if (c == u'#' && (i == 0 || s.at(i - 1).isSpace())) {
            s.truncate(i);
            while (!s.isEmpty() && s.back().isSpace())
                s.chop(1);
            return true;
        }
    }
    return false;
}

QString unquote(const QString &s, bool *quoted)
{
    *quoted = false;
    if (s.size() >= 2 && s.front() == u'"' && s.back() == u'"') {
        *quoted = true;
        QString out;
        for (qsizetype i = 1; i < s.size() - 1; ++i) {
            QChar c = s.at(i);
            if (c == u'\\' && i + 1 < s.size() - 1) {
                const QChar n = s.at(++i);
                if (n == u'n')
                    out += u'\n';
                else if (n == u't')
                    out += u'\t';
                else if (n == u'"' || n == u'\\' || n == u'/')
                    out += n;
                else if (n == u'u' && i + 4 < s.size()) {
                    out += QChar(s.mid(i + 1, 4).toUShort(nullptr, 16));
                    i += 4;
                } else {
                    out += u'\\';
                    out += n;
                }
            } else {
                out += c;
            }
        }
        return out;
    }
    if (s.size() >= 2 && s.front() == u'\'' && s.back() == u'\'') {
        *quoted = true;
        return s.mid(1, s.size() - 2).replace(u"''"_s, u"'"_s);
    }
    return s;
}

// Splits on top-level commas of a single-level flow collection body.
QStringList splitFlow(const QString &body)
{
    QStringList parts;
    QString cur;
    QChar quote;
    for (const QChar c : body) {
        if (!quote.isNull()) {
            cur += c;
            if (c == quote)
                quote = QChar();
            continue;
        }
        if (c == u'"' || c == u'\'') {
            quote = c;
            cur += c;
        } else if (c == u',') {
            parts << cur.trimmed();
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.trimmed().isEmpty())
        parts << cur.trimmed();
    return parts;
}

// Finds the `key: ` separator outside quotes; -1 if the text is not a mapping entry.
qsizetype mappingColon(const QString &s)
{
    QChar quote;
    for (qsizetype i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (!quote.isNull()) {
            if (c == quote)
                quote = QChar();
            continue;
        }
        if ((c == u'"' || c == u'\'') && i == 0) {
            quote = c;
        } else if (c == u':' && (i + 1 == s.size() || s.at(i + 1) == u' ')) {
            return i;
        } else if (c == u'[' || c == u'{') {
            return -1;
        }
    }
    return -1;
}

class Parser
{
public:
    explicit Parser(const QList<SrcLine> &lines)
        : m_lines(lines)
    {
    }

    Node parseDocument()
    {
        if (m_lines.isEmpty())
            return Node::makeMap();
        return parseBlock(m_lines.first().indent);
    }

private:
    Node parseBlock(int indent)
    {
        if (m_pos >= m_lines.size())
            return {};
        const auto &first = m_lines.at(m_pos);
        if (first.text == "-"_L1 || first.text.startsWith("- "_L1))
            return parseSeq(indent);
        return parseMap(indent);
    }

    Node parseMap(int indent)
    {
        Node node = Node::makeMap();
        while (m_pos < m_lines.size()) {
            const auto line = m_lines.at(m_pos);
            if (line.indent < indent)
                break;
            if (line.indent > indent)
                throw Unsupported{u"Unexpected indentation"_s, line.number};
            const auto colon = mappingColon(line.text);
            if (colon < 0)
                throw Unsupported{u"Expected \"key: value\""_s, line.number};
            bool q = false;
            const auto key = unquote(line.text.left(colon).trimmed(), &q);
            const auto rest = line.text.mid(colon + 1).trimmed();
            ++m_pos;
            node.map.append({key, parseValue(rest, indent, line.number)});
        }
        return node;
    }

    Node parseSeq(int indent)
    {
        Node node{Node::Type::Seq, {}, false, {}, {}};
        while (m_pos < m_lines.size()) {
            auto line = m_lines.at(m_pos);
            if (line.indent < indent || !(line.text == "-"_L1 || line.text.startsWith("- "_L1)))
                break;
            if (line.indent > indent)
                throw Unsupported{u"Unexpected indentation"_s, line.number};
            const auto rest = line.text.mid(1).trimmed();
            if (rest.isEmpty()) {
                ++m_pos;
                node.seq << (m_pos < m_lines.size() && m_lines.at(m_pos).indent > indent
                                 ? parseBlock(m_lines.at(m_pos).indent)
                                 : Node{});
            } else if (mappingColon(rest) >= 0 && !rest.startsWith(u'"') && !rest.startsWith(u'\'')) {
                // "- key: value" starts an inline map whose further keys sit at indent + 2.
                const int itemIndent = indent + 2;
                m_lines[m_pos].indent = itemIndent;
                m_lines[m_pos].text = rest;
                node.seq << parseMap(itemIndent);
            } else {
                ++m_pos;
                node.seq << parseScalarOrFlow(rest, line.number);
            }
        }
        return node;
    }

    Node parseValue(const QString &rest, int parentIndent, int lineNo)
    {
        if (rest.isEmpty()) {
            if (m_pos < m_lines.size()) {
                const auto &next = m_lines.at(m_pos);
                // Sequences may sit at the parent's indentation under a key.
                if (next.indent > parentIndent || (next.indent == parentIndent && next.text.startsWith("- "_L1)))
                    return parseBlock(next.indent);
            }
            return {};
        }
        if (rest == "|"_L1 || rest == ">"_L1 || rest.startsWith("|-"_L1) || rest.startsWith(">-"_L1)
            || rest.startsWith("|+"_L1) || rest.startsWith(">+"_L1))
            return parseBlockScalar(rest, parentIndent);
        return parseScalarOrFlow(rest, lineNo);
    }

    Node parseBlockScalar(const QString &header, int parentIndent)
    {
        const bool folded = header.startsWith(u'>');
        const bool strip = header.contains(u'-');
        QStringList body;
        int blockIndent = -1;
        while (m_pos < m_lines.size() && m_lines.at(m_pos).indent > parentIndent) {
            const auto &l = m_lines.at(m_pos);
            if (blockIndent < 0)
                blockIndent = l.indent;
            body << l.raw.mid(blockIndent);
            ++m_pos;
        }
        QString text = folded ? body.join(u' ') : body.join(u'\n');
        if (!strip)
            text += u'\n';
        return Node{Node::Type::Scalar, text, true, {}, {}};
    }

    Node parseScalarOrFlow(const QString &text, int lineNo)
    {
        if (text.startsWith(u'&') || text.startsWith(u'*') || (text.startsWith(u'!') && !text.startsWith("!="_L1)))
            throw Unsupported{u"Anchors, aliases and tags are not supported"_s, lineNo};
        if (text.startsWith(u'[') || text.startsWith(u'{')) {
            const bool isSeq = text.startsWith(u'[');
            if (!text.endsWith(isSeq ? u']' : u'}'))
                throw Unsupported{u"Multi-line flow collections are not supported"_s, lineNo};
            const auto body = text.mid(1, text.size() - 2);
            if (body.contains(u'[') || body.contains(u'{'))
                throw Unsupported{u"Nested flow collections are not supported"_s, lineNo};
            const auto parts = splitFlow(body);
            if (isSeq) {
                Node n{Node::Type::Seq, {}, false, {}, {}};
                for (const auto &p : parts)
                    n.seq << scalar(p);
                return n;
            }
            Node n = Node::makeMap();
            for (const auto &p : parts) {
                const auto c = mappingColon(p);
                if (c < 0)
                    throw Unsupported{u"Malformed flow mapping"_s, lineNo};
                bool q = false;
                n.map.append({unquote(p.left(c).trimmed(), &q), scalar(p.mid(c + 1).trimmed())});
            }
            return n;
        }
        return scalar(text);
    }

    static Node scalar(const QString &text)
    {
        bool quoted = false;
        const auto value = unquote(text, &quoted);
        if (!quoted && (value == "~"_L1 || value == "null"_L1 || value.isEmpty()))
            return {};
        return Node{Node::Type::Scalar, value, quoted, {}, {}};
    }

    QList<SrcLine> m_lines;
    qsizetype m_pos = 0;
};

bool needsQuotes(const QString &s)
{
    if (s.isEmpty() || s.front().isSpace() || s.back().isSpace())
        return true;
    static const QRegularExpression special(uR"(^[-?:,\[\]{}#&*!|>'"%@`]|: | #|[\x00-\x1f])"_s);
    if (special.match(s).hasMatch())
        return true;
    static const QRegularExpression typed(
        uR"(^(true|false|yes|no|on|off|null|~|[-+]?(\d[\d_]*(\.\d*)?|\.\d+)([eE][-+]?\d+)?|0x[0-9a-fA-F]+|\.inf|\.nan)$)"_s,
        QRegularExpression::CaseInsensitiveOption);
    return typed.match(s).hasMatch();
}

QString quoteDouble(const QString &s)
{
    QString out = u"\""_s;
    for (const QChar c : s) {
        if (c == u'"' || c == u'\\') {
            out += u'\\';
            out += c;
        } else if (c == u'\n') {
            out += u"\\n"_s;
        } else if (c == u'\t') {
            out += u"\\t"_s;
        } else if (c.unicode() < 0x20) {
            out += u"\\u%1"_s.arg(int(c.unicode()), 4, 16, QLatin1Char('0'));
        } else {
            out += c;
        }
    }
    return out + u'"';
}

QString scalarText(const Node &n)
{
    if (n.type == Node::Type::Null)
        return u"null"_s;
    // Plain scalars were valid plain in the source, or are JSON numbers/booleans: emit verbatim.
    // String values are quoted only when a plain scalar would change their meaning.
    if (!n.quoted)
        return n.scalar;
    return needsQuotes(n.scalar) || n.scalar.contains(u'\n') ? quoteDouble(n.scalar) : n.scalar;
}

void emitNode(QString &out, const Node &n, int indent);

void emitMapEntries(QString &out, const Node &n, int indent)
{
    const QString pad(indent, u' ');
    for (const auto &[key, value] : n.map) {
        const auto k = needsQuotes(key) ? quoteDouble(key) : key;
        if (value.type == Node::Type::Map && !value.map.isEmpty()) {
            out += pad + k + u":\n"_s;
            emitMapEntries(out, value, indent + 2);
        } else if (value.type == Node::Type::Seq && !value.seq.isEmpty()) {
            out += pad + k + u":\n"_s;
            emitNode(out, value, indent + 2);
        } else if (value.type == Node::Type::Scalar && value.quoted && value.scalar.contains(u'\n')) {
            auto text = value.scalar;
            const bool trailing = text.endsWith(u'\n');
            if (trailing)
                text.chop(1);
            out += pad + k + (trailing ? u": |\n"_s : u": |-\n"_s);
            for (const auto &l : text.split(u'\n'))
                out += QString(indent + 2, u' ') + l + u'\n';
        } else {
            out += pad + k + u": "_s;
            emitNode(out, value, indent + 2);
        }
    }
}

void emitNode(QString &out, const Node &n, int indent)
{
    const QString pad(indent, u' ');
    switch (n.type) {
    case Node::Type::Null:
    case Node::Type::Scalar:
        out += scalarText(n) + u'\n';
        break;
    case Node::Type::Map:
        if (n.map.isEmpty())
            out += u"{}\n"_s;
        else
            emitMapEntries(out, n, indent);
        break;
    case Node::Type::Seq:
        if (n.seq.isEmpty()) {
            out += u"[]\n"_s;
            break;
        }
        for (const auto &item : n.seq) {
            if (item.type == Node::Type::Map && !item.map.isEmpty()) {
                QString inner;
                emitMapEntries(inner, item, indent + 2);
                // First key goes on the dash line.
                out += pad + u"- "_s + inner.mid(indent + 2);
            } else {
                out += pad + u"- "_s;
                emitNode(out, item, indent + 2);
            }
        }
        break;
    }
}

} // namespace

const Node *Node::find(const QString &key) const
{
    for (const auto &[k, v] : map) {
        if (k == key)
            return &v;
    }
    return nullptr;
}

Node *Node::find(const QString &key)
{
    for (auto &[k, v] : map) {
        if (k == key)
            return &v;
    }
    return nullptr;
}

void Node::setPath(const QStringList &path, const Node &value)
{
    if (path.isEmpty()) {
        *this = value;
        return;
    }
    if (type != Type::Map) {
        type = Type::Map;
        map.clear();
    }
    Node *child = find(path.first());
    if (!child) {
        map.append({path.first(), Node::makeMap()});
        child = &map.last().second;
    }
    child->setPath(path.mid(1), value);
}

bool Node::removePath(const QStringList &path)
{
    if (path.isEmpty() || type != Type::Map)
        return false;
    for (qsizetype i = 0; i < map.size(); ++i) {
        if (map.at(i).first != path.first())
            continue;
        if (path.size() == 1) {
            map.removeAt(i);
            return true;
        }
        const bool removed = map[i].second.removePath(path.mid(1));
        // Drop maps emptied by the removal so the file does not accumulate "key: {}".
        if (removed && map.at(i).second.type == Type::Map && map.at(i).second.map.isEmpty())
            map.removeAt(i);
        return removed;
    }
    return false;
}

const Node *Node::atPath(const QStringList &path) const
{
    const Node *n = this;
    for (const auto &p : path) {
        if (n->type != Type::Map)
            return nullptr;
        n = n->find(p);
        if (!n)
            return nullptr;
    }
    return n;
}

QJsonValue Node::toJson() const
{
    switch (type) {
    case Type::Null:
        return QJsonValue::Null;
    case Type::Scalar: {
        if (quoted)
            return scalar;
        if (scalar == "true"_L1)
            return true;
        if (scalar == "false"_L1)
            return false;
        bool ok = false;
        const double d = QString(scalar).remove(u'_').toDouble(&ok);
        if (ok && !scalar.startsWith(u'+'))
            return d;
        return scalar;
    }
    case Type::Map: {
        QJsonObject o;
        for (const auto &[k, v] : map)
            o.insert(k, v.toJson());
        return o;
    }
    case Type::Seq: {
        QJsonArray a;
        for (const auto &v : seq)
            a.append(v.toJson());
        return a;
    }
    }
    return {};
}

Node Node::fromJson(const QJsonValue &value, const Node *previous)
{
    switch (value.type()) {
    case QJsonValue::Null:
    case QJsonValue::Undefined:
        return {};
    case QJsonValue::Bool:
        return Node{Type::Scalar, value.toBool() ? u"true"_s : u"false"_s, false, {}, {}};
    case QJsonValue::Double: {
        const double d = value.toDouble();
        const auto text = d == static_cast<double>(static_cast<qint64>(d)) ? QString::number(static_cast<qint64>(d))
                                                                          : QString::number(d, 'g', 15);
        return Node{Type::Scalar, text, false, {}, {}};
    }
    case QJsonValue::String:
        return Node{Type::Scalar, value.toString(), true, {}, {}};
    case QJsonValue::Array: {
        Node n{Type::Seq, {}, false, {}, {}};
        const auto arr = value.toArray();
        for (qsizetype i = 0; i < arr.size(); ++i) {
            const Node *prevItem = previous && previous->type == Type::Seq && i < previous->seq.size()
                                       ? &previous->seq.at(i)
                                       : nullptr;
            n.seq << fromJson(arr.at(i), prevItem);
        }
        return n;
    }
    case QJsonValue::Object: {
        Node n = makeMap();
        const auto obj = value.toObject();
        if (previous && previous->type == Type::Map) {
            for (const auto &[k, v] : previous->map) {
                if (obj.contains(k))
                    n.map.append({k, fromJson(obj.value(k), &v)});
            }
        }
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (!n.find(it.key()))
                n.map.append({it.key(), fromJson(it.value(), nullptr)});
        }
        return n;
    }
    }
    return {};
}

ParseResult parse(const QString &text)
{
    ParseResult result;
    QList<SrcLine> lines;
    const auto rawLines = text.split(u'\n');
    for (qsizetype i = 0; i < rawLines.size(); ++i) {
        QString raw = rawLines.at(i);
        if (raw.endsWith(u'\r'))
            raw.chop(1);
        if (raw.trimmed() == "---"_L1 && lines.isEmpty())
            continue;
        if (raw.trimmed() == "---"_L1 || raw.trimmed() == "..."_L1) {
            result.error = u"Multiple YAML documents are not supported"_s;
            result.errorLine = int(i + 1);
            return result;
        }
        if (raw.contains(u'\t') && raw.trimmed().size() != raw.size() && raw.at(0) == u'\t') {
            result.error = u"Tabs are not allowed for indentation"_s;
            result.errorLine = int(i + 1);
            return result;
        }
        int indent = 0;
        while (indent < raw.size() && raw.at(indent) == u' ')
            ++indent;
        QString content = raw.mid(indent);
        if (content.startsWith(u'#')) {
            result.hasComments = true;
            continue;
        }
        if (stripComment(content))
            result.hasComments = true;
        if (content.trimmed().isEmpty()) {
            // Blank lines matter only inside block scalars; keep them as zero-content lines there.
            if (!lines.isEmpty() && (lines.last().text.endsWith(u'|') || lines.last().text.endsWith(u'>')))
                continue;
            continue;
        }
        lines << SrcLine{int(i + 1), indent, content, raw};
    }
    try {
        Parser parser(lines);
        result.root = parser.parseDocument();
        if (result.root.type == Node::Type::Null)
            result.root = Node::makeMap();
        result.ok = true;
    } catch (const Unsupported &u) {
        result.error = u.message;
        result.errorLine = u.line;
    }
    return result;
}

QString serialize(const Node &root)
{
    QString out;
    if (root.type == Node::Type::Map && root.map.isEmpty())
        return {};
    emitNode(out, root, 0);
    return out;
}

} // namespace gomp::yaml
