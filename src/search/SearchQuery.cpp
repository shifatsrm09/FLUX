#include "SearchQuery.h"

#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <vector>

namespace Flux {

namespace {

// "&" reads as "and"; apostrophes disappear so "Ocean's" and "Oceans" are the same word
QString prepare(QString s) {
    s.replace(QLatin1Char('&'), QStringLiteral(" and "));
    s.remove(QLatin1Char('\''));
    s.remove(QChar(0x2018));
    s.remove(QChar(0x2019));
    s.remove(QChar(0x60));
    return s;
}

bool isStopWord(const QString &token) {
    static const QSet<QString> kStop = {
        QStringLiteral("the"), QStringLiteral("a"), QStringLiteral("an"),
        QStringLiteral("of"),  QStringLiteral("and")
    };
    return kStop.contains(token);
}

void addUnique(QStringList &list, const QString &value) {
    const QString v = value.trimmed();
    if (v.isEmpty()) return;
    for (const QString &existing : list) {
        if (existing.compare(v, Qt::CaseInsensitive) == 0) return;
    }
    list.append(v);
}

bool containsCI(const QStringList &list, const QString &value) {
    for (const QString &existing : list) {
        if (existing.compare(value, Qt::CaseInsensitive) == 0) return true;
    }
    return false;
}

// Quality tags and years match thousands of files; they make poor anchors for a candidate fetch
bool isNoiseToken(const QString &token) {
    static const QRegularExpression noise(
        QStringLiteral("^(?:\\d{3,4}p|\\d{1,2}k|(?:19|20)\\d{2}|x26[45]|h26[45]|hevc|bluray|brrip|webrip|webdl"
                       "|hdrip|dvdrip|hdtv|remux|aac|ac3|dts|hdr|uhd|dual|audio|english|hindi|bangla)$"));
    return noise.match(token).hasMatch();
}

// How many typos we forgive for a word/phrase of this length
int allowedEdits(int length) {
    if (length >= 10) return 2;
    if (length >= 5) return 1;
    return 0;
}

} // namespace

QStringList SearchQuery::tokenize(const QString &text) {
    // Decompose so "é" becomes "e" + a combining mark, then drop the marks
    const QString decomposed = prepare(text).normalized(QString::NormalizationForm_D);

    QStringList tokens;
    QString current;
    for (const QChar c : decomposed) {
        if (c.category() == QChar::Mark_NonSpacing) continue;
        if (c.isLetterOrNumber()) {
            current.append(c.toLower());
        } else if (!current.isEmpty()) {
            tokens.append(current);
            current.clear();
        }
    }
    if (!current.isEmpty()) tokens.append(current);
    return tokens;
}

QString SearchQuery::normalize(const QString &text) {
    return tokenize(text).join(QString());
}

SearchQuery::Plan SearchQuery::plan(const QString &query, int maxLiteralQueries) {
    Plan p;
    p.original = query.trimmed();

    const QStringList all = tokenize(p.original);
    p.normalized = all.join(QString());

    for (const QString &t : all) {
        if (!isStopWord(t)) p.tokens.append(t);
    }
    if (p.tokens.isEmpty()) p.tokens = all;   // a query made only of stop words still has to work

    // ---- Literal queries: the query as typed, then the common ways a title is written ----
    addUnique(p.literalQueries, p.original);
    if (all.size() > 1) {
        const QStringList separators{QStringLiteral("."), QStringLiteral("-"), QStringLiteral(" "),
                                     QStringLiteral("_"), QString()};
        for (const QString &sep : separators) {
            addUnique(p.literalQueries, all.join(sep));
        }
    }
    while (p.literalQueries.size() > std::max(1, maxLiteralQueries)) {
        p.literalQueries.removeLast();
    }

    // ---- Anchors: chunks of the longest words, used to pull in candidates ----
    // A separator can hide anywhere inside a word we can't split ("spiderman" is "spider-man"), so
    // for long words we ask for the first and the last part; any real match contains at least one.
    QStringList words;
    for (const QString &t : p.tokens) {
        if (t.size() >= 4 && !isNoiseToken(t)) words.append(t);
    }
    std::stable_sort(words.begin(), words.end(), [](const QString &a, const QString &b) {
        return a.size() > b.size();
    });
    if (words.size() > 2) words = words.mid(0, 2);

    QStringList anchors;
    for (const QString &w : words) {
        if (w.size() >= 6) {
            const int n = std::max(4, (static_cast<int>(w.size()) + 1) / 2);
            addUnique(anchors, w.left(n));
            addUnique(anchors, w.right(n));
        } else {
            addUnique(anchors, w);
        }
    }
    for (const QString &a : anchors) {
        if (!containsCI(p.literalQueries, a) && p.anchorQueries.size() < 4) {
            p.anchorQueries.append(a);
        }
    }
    return p;
}

bool SearchQuery::approxContains(const QString &text, const QString &pattern, int maxEdits) {
    const int m = static_cast<int>(pattern.size());
    if (m == 0) return true;
    if (maxEdits <= 0) return text.contains(pattern);

    // Sellers' algorithm: edit distance of the pattern against the best-fitting substring
    std::vector<int> prev(static_cast<size_t>(m) + 1);
    std::vector<int> cur(static_cast<size_t>(m) + 1);
    for (int i = 0; i <= m; ++i) prev[static_cast<size_t>(i)] = i;

    for (qsizetype j = 0; j < text.size(); ++j) {
        cur[0] = 0;   // a match may start anywhere in the text
        for (int i = 1; i <= m; ++i) {
            const int cost = (pattern.at(i - 1) == text.at(j)) ? 0 : 1;
            cur[static_cast<size_t>(i)] = std::min({prev[static_cast<size_t>(i) - 1] + cost,
                                                    prev[static_cast<size_t>(i)] + 1,
                                                    cur[static_cast<size_t>(i) - 1] + 1});
        }
        if (cur[static_cast<size_t>(m)] <= maxEdits) return true;
        std::swap(prev, cur);
    }
    return false;
}

int SearchQuery::score(const QString &text, const Plan &plan) {
    if (plan.normalized.isEmpty()) return 3;   // nothing to compare (e.g. only punctuation)

    const QString name = normalize(text);

    // Contiguous, ignoring punctuation and case: "spiderman" in "Spider-Man (2002)"
    if (name.contains(plan.normalized)) return 3;

    const QString core = plan.tokens.join(QString());   // same, without "the", "a", "of" ...
    if (!core.isEmpty() && name.contains(core)) return 3;

    // Every word present in any order: "man spider" finds "Spider-Man"
    if (!plan.tokens.isEmpty()) {
        bool allPresent = true;
        for (const QString &t : plan.tokens) {
            if (!name.contains(t)) { allPresent = false; break; }
        }
        if (allPresent) return 2;
    }

    // Typos: "spidermn", "avangers"
    const int edits = allowedEdits(static_cast<int>(core.size()));
    if (edits > 0 && approxContains(name, core, edits)) return 1;

    if (plan.tokens.size() > 1) {
        bool allClose = true;
        for (const QString &t : plan.tokens) {
            if (!approxContains(name, t, allowedEdits(static_cast<int>(t.size())))) { allClose = false; break; }
        }
        if (allClose) return 1;
    }
    return 0;
}

} // namespace Flux
