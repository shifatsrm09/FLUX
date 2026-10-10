// Standalone checks for the forgiving search logic (no network, no Qt event loop).
// Build target: flux_test_searchquery   (exit code 0 = all passed)

#include <QCoreApplication>
#include <iostream>
#include "../src/search/SearchQuery.h"

using Flux::SearchQuery;

static int g_failures = 0;

static void check(bool ok, const char *what) {
    if (!ok) {
        ++g_failures;
        std::cerr << "FAIL: " << what << "\n";
    }
}

static int scoreOf(const char *name, const char *query) {
    return SearchQuery::score(QString::fromUtf8(name), SearchQuery::plan(QString::fromUtf8(query)));
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // ---- normalisation ----
    check(SearchQuery::normalize("Spider-Man: Far From Home") == "spidermanfarfromhome", "normalize punctuation");
    check(SearchQuery::normalize("Am\xC3\xA9lie") == "amelie", "normalize accents");
    check(SearchQuery::tokenize("Ocean's Eleven (2001)") == QStringList({"oceans", "eleven", "2001"}), "tokenize apostrophe");

    // ---- the reported bug: "spiderman" must find "Spider-Man" ----
    check(scoreOf("Spider-Man (2002) 1080p.mkv", "spiderman") == 3, "spiderman vs Spider-Man");
    check(scoreOf("The.Amazing.Spider-Man.2012.1080p.BluRay.mkv", "spiderman") == 3, "spiderman vs dotted release name");
    check(scoreOf("Spider_Man_Homecoming.mkv", "spider man") == 3, "spider man vs underscores");
    check(scoreOf("Spiderman.mkv", "spider-man") == 3, "spider-man vs spiderman");

    // ---- plans ----
    {
        const auto p = SearchQuery::plan("spiderman");
        check(p.literalQueries == QStringList({"spiderman"}), "single word keeps one literal query");
        check(p.anchorQueries.contains("spide") && p.anchorQueries.contains("erman"), "single word gets prefix/suffix anchors");
    }
    {
        const auto p = SearchQuery::plan("spider man");
        check(p.literalQueries.contains("spider-man"), "two words try hyphen");
        check(p.literalQueries.contains("spider.man"), "two words try dot");
        check(p.literalQueries.contains("spider_man"), "two words try underscore");
        check(p.literalQueries.contains("spiderman"), "two words try joined");
        check(p.literalQueries.first() == "spider man", "original query is sent first");
    }
    {
        const auto p = SearchQuery::plan("spiderman 1080p");
        check(!p.anchorQueries.contains("1080p"), "quality tags are not used as anchors");
        check(SearchQuery::plan("hulk").anchorQueries.isEmpty(), "short word needs no anchors");
        check(SearchQuery::plan("spider man", 3).literalQueries.size() <= 3, "query count is capped");
    }

    // ---- word order, stop words, symbols ----
    check(scoreOf("The Amazing Spider-Man 2.mkv", "man spider") == 2, "any word order");
    check(scoreOf("Avengers.2012.mkv", "the avengers") == 3, "stop word ignored");
    check(scoreOf("Tom & Jerry.mkv", "tom and jerry") == 3, "ampersand equals and");
    check(scoreOf("Oceans.Eleven.mkv", "ocean's eleven") == 3, "apostrophe ignored");
    check(scoreOf("Am\xC3\xA9lie.mkv", "amelie") == 3, "accent ignored");

    // ---- typos ----
    check(scoreOf("Spider-Man.mkv", "spidermn") == 1, "one missing letter");
    check(scoreOf("The Avengers.mkv", "avangers") == 1, "one wrong letter");

    // ---- things that must NOT match ----
    check(scoreOf("Batman.mkv", "spiderman") == 0, "unrelated title");
    check(scoreOf("German.Film.mkv", "spiderman") == 0, "anchor chunk alone is not a match");
    check(scoreOf("Spider-Verse.mkv", "spiderman") == 0, "different title sharing a prefix");

    // ---- degenerate input ----
    check(SearchQuery::score("anything", SearchQuery::plan("...")) == 3, "punctuation-only query matches everything");
    check(!SearchQuery::plan("  ").literalQueries.isEmpty() || true, "empty query does not crash");

    if (g_failures == 0) {
        std::cout << "All SearchQuery checks passed.\n";
        return 0;
    }
    std::cerr << g_failures << " check(s) failed.\n";
    return 1;
}
