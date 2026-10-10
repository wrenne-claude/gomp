#include "SyntaxHighlighter.h"

#include <QHash>
#include <QSet>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace gomp::syntax {

namespace {

struct Language {
    QSet<QString> keywords;
    QSet<QString> types;
    QStringList lineComments;
    QString blockOpen;
    QString blockClose;
    QString stringQuotes = u"\"'"_s;
    bool backtickStrings = false;
    bool tripleQuotes = false;
    bool keysAsTypes = false; // JSON/YAML keys
    bool caseInsensitive = false;
};

QSet<QString> words(const char *list)
{
    QSet<QString> out;
    for (const auto &w : QString::fromLatin1(list).split(u' ', Qt::SkipEmptyParts))
        out.insert(w);
    return out;
}

const QHash<QString, Language> &languages()
{
    static const QHash<QString, Language> langs = [] {
        QHash<QString, Language> l;
        Language c;
        c.keywords = words("alignas alignof auto break case catch class const constexpr consteval constinit continue "
                           "co_await co_return co_yield decltype default delete do else enum explicit export extern "
                           "false final for friend goto if inline mutable namespace new noexcept nullptr operator "
                           "override private protected public return sizeof static static_assert static_cast struct "
                           "switch template this throw true try typedef typename union using virtual volatile while "
                           "#include #define #pragma #if #ifdef #ifndef #endif #else signals slots emit Q_OBJECT");
        c.types = words("void bool char int long short float double unsigned signed size_t qsizetype qint64 quint64 "
                        "QString QStringList QList QHash QObject QVariant std string vector");
        c.lineComments = {u"//"_s};
        c.blockOpen = u"/*"_s;
        c.blockClose = u"*/"_s;
        l.insert(u"cpp"_s, c);

        Language js;
        js.keywords = words("async await break case catch class const continue debugger default delete do else export "
                            "extends false finally for from function if import in instanceof let new null of return "
                            "static super switch this throw true try typeof undefined var void while yield type "
                            "interface enum implements readonly as satisfies declare namespace keyof");
        js.types = words("string number boolean any unknown never object Array Promise Record Map Set");
        js.lineComments = {u"//"_s};
        js.blockOpen = u"/*"_s;
        js.blockClose = u"*/"_s;
        js.backtickStrings = true;
        l.insert(u"typescript"_s, js);

        Language qml = js;
        qml.keywords += words("property signal readonly required component alias import pragma");
        qml.types += words("int real double bool string var list url color Item Rectangle Text");
        l.insert(u"qml"_s, qml);

        Language py;
        py.keywords = words("and as assert async await break class continue def del elif else except False finally for "
                            "from global if import in is lambda None nonlocal not or pass raise return True try while "
                            "with yield match case self");
        py.types = words("int str float bool list dict set tuple bytes object Any Optional");
        py.lineComments = {u"#"_s};
        py.tripleQuotes = true;
        l.insert(u"python"_s, py);

        Language rust;
        rust.keywords = words("as async await break const continue crate dyn else enum extern false fn for if impl in let "
                              "loop match mod move mut pub ref return self Self static struct super trait true type "
                              "unsafe use where while macro_rules");
        rust.types = words("i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 bool char str String Vec Option "
                           "Result Box Rc Arc HashMap");
        rust.lineComments = {u"//"_s};
        rust.blockOpen = u"/*"_s;
        rust.blockClose = u"*/"_s;
        rust.stringQuotes = u"\""_s;
        l.insert(u"rust"_s, rust);

        Language go;
        go.keywords = words("break case chan const continue default defer else fallthrough for func go goto if import "
                            "interface map package range return select struct switch type var nil true false");
        go.types = words("int int8 int16 int32 int64 uint uint8 uint16 uint32 uint64 float32 float64 string bool byte "
                         "rune error any");
        go.lineComments = {u"//"_s};
        go.blockOpen = u"/*"_s;
        go.blockClose = u"*/"_s;
        go.backtickStrings = true;
        l.insert(u"go"_s, go);

        Language sh;
        sh.keywords = words("if then else elif fi for while until do done case esac in function return local export "
                            "set unset readonly shift exit source echo cd sudo");
        sh.lineComments = {u"#"_s};
        l.insert(u"bash"_s, sh);

        Language json;
        json.keywords = words("true false null");
        json.stringQuotes = u"\""_s;
        json.keysAsTypes = true;
        l.insert(u"json"_s, json);

        Language yaml;
        yaml.keywords = words("true false null yes no on off");
        yaml.lineComments = {u"#"_s};
        yaml.keysAsTypes = true;
        l.insert(u"yaml"_s, yaml);

        Language toml = yaml;
        l.insert(u"toml"_s, toml);

        Language sql;
        sql.keywords = words("select from where insert into values update set delete create table index drop alter add "
                             "join left right inner outer on group by order having limit as and or not null primary key "
                             "references distinct union all case when then else end");
        sql.lineComments = {u"--"_s};
        sql.blockOpen = u"/*"_s;
        sql.blockClose = u"*/"_s;
        sql.caseInsensitive = true;
        l.insert(u"sql"_s, sql);

        Language css;
        css.keywords = words("important media import from to");
        css.blockOpen = u"/*"_s;
        css.blockClose = u"*/"_s;
        l.insert(u"css"_s, css);

        Language java = c;
        java.keywords = words("abstract assert boolean break byte case catch char class const continue default do double "
                              "else enum extends final finally float for if implements import instanceof int interface "
                              "long native new package private protected public return short static super switch "
                              "synchronized this throw throws transient try void volatile while true false null var "
                              "val fun when object companion data sealed");
        l.insert(u"java"_s, java);
        return l;
    }();
    return langs;
}

QString escape(QStringView s)
{
    QString out;
    out.reserve(s.size());
    for (const auto ch : s) {
        if (ch == u'<')
            out += u"&lt;"_s;
        else if (ch == u'>')
            out += u"&gt;"_s;
        else if (ch == u'&')
            out += u"&amp;"_s;
        else
            out += ch;
    }
    return out;
}

void span(QString &out, const QString &color, QStringView text)
{
    if (color.isEmpty()) {
        out += escape(text);
        return;
    }
    out += u"<span style=\"color:"_s + color + u"\">"_s + escape(text) + u"</span>"_s;
}

QString highlightDiff(const QString &code, const Palette &p)
{
    QString out;
    const auto lines = QStringView(code).split(u'\n');
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const auto line = lines.at(i);
        QString color;
        if (line.startsWith(u"+++") || line.startsWith(u"---") || line.startsWith(u"@@"))
            color = p.meta;
        else if (line.startsWith(u'+'))
            color = p.added;
        else if (line.startsWith(u'-'))
            color = p.removed;
        else
            color = p.comment;
        span(out, color, line);
        if (i + 1 < lines.size())
            out += u'\n';
    }
    return out;
}

} // namespace

QString canonicalLanguage(const QString &tag)
{
    static const QHash<QString, QString> aliases{
        {u"c"_s, u"cpp"_s},          {u"h"_s, u"cpp"_s},        {u"hpp"_s, u"cpp"_s},     {u"cc"_s, u"cpp"_s},
        {u"cxx"_s, u"cpp"_s},        {u"c++"_s, u"cpp"_s},      {u"js"_s, u"typescript"_s}, {u"javascript"_s, u"typescript"_s},
        {u"ts"_s, u"typescript"_s},  {u"tsx"_s, u"typescript"_s}, {u"jsx"_s, u"typescript"_s}, {u"mjs"_s, u"typescript"_s},
        {u"py"_s, u"python"_s},      {u"rs"_s, u"rust"_s},      {u"sh"_s, u"bash"_s},     {u"shell"_s, u"bash"_s},
        {u"zsh"_s, u"bash"_s},       {u"console"_s, u"bash"_s}, {u"yml"_s, u"yaml"_s},    {u"kt"_s, u"java"_s},
        {u"kotlin"_s, u"java"_s},    {u"patch"_s, u"diff"_s},   {u"jsonc"_s, u"json"_s},  {u"scss"_s, u"css"_s},
        {u"golang"_s, u"go"_s},
    };
    const auto lower = tag.toLower();
    return aliases.value(lower, lower);
}

QString toHtml(const QString &code, const QString &language, const Palette &p)
{
    const auto langId = canonicalLanguage(language);
    QString body;
    if (langId == "diff"_L1) {
        body = highlightDiff(code, p);
    } else if (!languages().contains(langId)) {
        body = escape(code);
    } else {
        const auto &lang = languages()[langId];
        body.reserve(code.size() * 2);
        const qsizetype n = code.size();
        qsizetype i = 0;
        auto startsWithAt = [&](const QString &tok) { return !tok.isEmpty() && QStringView(code).mid(i).startsWith(tok); };
        while (i < n) {
            const QChar ch = code.at(i);
            // Comments
            bool handled = false;
            for (const auto &lc : lang.lineComments) {
                if (startsWithAt(lc) && !(lc == "#"_L1 && langId == "cpp"_L1)) {
                    auto end = code.indexOf(u'\n', i);
                    if (end < 0)
                        end = n;
                    span(body, p.comment, QStringView(code).mid(i, end - i));
                    i = end;
                    handled = true;
                    break;
                }
            }
            if (handled)
                continue;
            if (startsWithAt(lang.blockOpen)) {
                auto end = code.indexOf(lang.blockClose, i + lang.blockOpen.size());
                end = end < 0 ? n : end + lang.blockClose.size();
                span(body, p.comment, QStringView(code).mid(i, end - i));
                i = end;
                continue;
            }
            // Strings
            if (lang.tripleQuotes && (startsWithAt(u"\"\"\""_s) || startsWithAt(u"'''"_s))) {
                const auto quote = code.mid(i, 3);
                auto end = code.indexOf(quote, i + 3);
                end = end < 0 ? n : end + 3;
                span(body, p.string, QStringView(code).mid(i, end - i));
                i = end;
                continue;
            }
            if (lang.stringQuotes.contains(ch) || (lang.backtickStrings && ch == u'`')) {
                qsizetype j = i + 1;
                while (j < n && code.at(j) != ch && !(code.at(j) == u'\n' && ch != u'`')) {
                    if (code.at(j) == u'\\')
                        ++j;
                    ++j;
                }
                j = std::min(j + 1, n);
                const auto text = QStringView(code).mid(i, j - i);
                // JSON/YAML keys: a string followed by ':' renders as a key.
                qsizetype k = j;
                while (k < n && code.at(k) == u' ')
                    ++k;
                const bool isKey = lang.keysAsTypes && k < n && code.at(k) == u':';
                span(body, isKey ? p.type : p.string, text);
                i = j;
                continue;
            }
            // Numbers
            if (ch.isDigit() && (i == 0 || !(code.at(i - 1).isLetterOrNumber() || code.at(i - 1) == u'_'))) {
                qsizetype j = i;
                while (j < n && (code.at(j).isLetterOrNumber() || code.at(j) == u'.' || code.at(j) == u'_'
                                 || code.at(j) == u'\''))
                    ++j;
                span(body, p.number, QStringView(code).mid(i, j - i));
                i = j;
                continue;
            }
            // Identifiers, keywords, types, calls. '#' prefixes C preprocessor directives.
            if (ch.isLetter() || ch == u'_' || ch == u'$' || (ch == u'#' && langId == "cpp"_L1) || ch == u'@') {
                qsizetype j = i + 1;
                while (j < n && (code.at(j).isLetterOrNumber() || code.at(j) == u'_' || code.at(j) == u'$'
                                 || (langId == "bash"_L1 && code.at(j) == u'-')))
                    ++j;
                const auto word = code.mid(i, j - i);
                const auto lookup = lang.caseInsensitive ? word.toLower() : word;
                qsizetype k = j;
                while (k < n && code.at(k) == u' ')
                    ++k;
                if (lang.keysAsTypes && k < n && code.at(k) == u':' && langId == "yaml"_L1)
                    span(body, p.type, word);
                else if (lang.keywords.contains(lookup))
                    span(body, p.keyword, word);
                else if (lang.types.contains(word) || (word.at(0).isUpper() && langId != "bash"_L1 && word.size() > 1))
                    span(body, p.type, word);
                else if (k < n && code.at(k) == u'(')
                    span(body, p.function, word);
                else
                    span(body, {}, word);
                i = j;
                continue;
            }
            if (QStringLiteral("{}[]();,.:=<>+-*/%!&|^~?").contains(ch))
                span(body, p.punctuation, QStringView(code).mid(i, 1));
            else
                body += escape(QStringView(code).mid(i, 1));
            ++i;
        }
    }
    return u"<div style=\"white-space:pre; color:"_s + p.text + u"\">"_s + body + u"</div>"_s;
}

} // namespace gomp::syntax
