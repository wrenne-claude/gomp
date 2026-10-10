#pragma once

#include <QString>

namespace gomp::syntax {

// Colours are supplied by the QML theme so highlighting follows dark/light and the accent.
struct Palette {
    QString text;
    QString keyword;
    QString type;
    QString string;
    QString number;
    QString comment;
    QString function;
    QString punctuation;
    QString added;
    QString removed;
    QString meta;
};

// Lightweight lexical highlighter: keywords, types, strings, numbers, comments and calls for the
// languages agents commonly emit. It never parses; unknown languages render as plain text.
// Returns Qt rich text using `white-space: pre`, safe to put in a Text/TextEdit item.
QString toHtml(const QString &code, const QString &language, const Palette &palette);

// Canonical language id for a fence tag or file extension ("ts" -> "typescript", "h" -> "cpp").
QString canonicalLanguage(const QString &tagOrExtension);

} // namespace gomp::syntax
