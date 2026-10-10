#pragma once

#include <QString>
#include <QStringList>

namespace Flux {

// Forgiving search on top of a server that only does literal substring matching.
//
// The media server answers a query like "spiderman" with nothing when the file is called
// "Spider-Man (2002).mkv", because it compares characters, not words. This class works around
// that on our side in two steps:
//
//   1. plan():  turns what the user typed into
//        - literalQueries: the query plus the usual ways titles are written
//                          ("spider man" -> spider-man, spider.man, spider_man, spiderman)
//        - anchorQueries:  short chunks that must appear somewhere in a matching name
//                          ("spiderman" -> "spide", "erman"), used to fetch candidates when
//                          the literal queries found little
//   2. score(): ranks (and filters) candidates by comparing normalised text, so punctuation,
//               case, accents, word order, "&" vs "and" and small typos stop mattering.
class SearchQuery {
public:
    struct Plan {
        QString original;            // what the user typed (trimmed)
        QString normalized;          // letters/digits only, lowercase, accents removed
        QStringList tokens;          // significant words ("the", "a", "of" ... removed)
        QStringList literalQueries;  // sent to the server first
        QStringList anchorQueries;   // sent only when the literal queries found few results
    };

    // Lowercase letters and digits only: "Spider-Man: Far From Home" -> "spidermanfarfromhome"
    static QString normalize(const QString &text);

    // Words: "Ocean's Eleven (2001)" -> ["oceans", "eleven", "2001"]
    static QStringList tokenize(const QString &text);

    static Plan plan(const QString &query, int maxLiteralQueries = 6);

    // 3 = the whole query appears contiguously (ignoring punctuation/case)
    // 2 = every significant word appears (any order)
    // 1 = close match (small typo)
    // 0 = no match
    static int score(const QString &text, const Plan &plan);

    // True if `pattern` occurs in `text` with at most `maxEdits` insertions/deletions/substitutions
    static bool approxContains(const QString &text, const QString &pattern, int maxEdits);
};

} // namespace Flux
