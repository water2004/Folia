#include "support/source_svg_paint.hpp"
#include "support/folia_test.hpp"

namespace {
using namespace boost::ut;
}

suite native_source_svg_paint = [] {
    "Source table CSS is materialized without changing geometry or unrelated SVG bytes"_test = [] {
        const std::string source =
            "<svg><g data-mml-node=\"mtable\" stroke=\"currentColor\" fill=\"currentColor\" stroke-width=\"0\">"
            "<path data-c=\"78\" d=\"M0 0L10 20\"/>"
            "<line data-line=\"v\" class=\"mjx-solid\" x1=\"100\" y1=\"-200\" x2=\"100\" y2=\"400\"/>"
            "<line data-line=\"h\" class=\"mjx-dashed\" x1=\"0\" y1=\"250\" x2=\"500\" y2=\"250\"/>"
            "<rect data-frame=\"true\" class=\"mjx-dotted\" width=\"500\" height=\"600\" x=\"35\" y=\"-200\"/>"
            "<rect width=\"123\" height=\"42\" fill=\"blue\"/>"
            "<line x1=\"0\" y1=\"0\" x2=\"10\" y2=\"20\" stroke-width=\"5\"/>"
            "</g></svg>";
        const std::string expected =
            "<svg><g data-mml-node=\"mtable\" stroke=\"currentColor\" fill=\"currentColor\" stroke-width=\"0\">"
            "<path data-c=\"78\" d=\"M0 0L10 20\"/>"
            "<line data-line=\"v\" class=\"mjx-solid\" x1=\"100\" y1=\"-200\" x2=\"100\" y2=\"400\" fill=\"none\" stroke-width=\"70\"/>"
            "<line data-line=\"h\" class=\"mjx-dashed\" x1=\"0\" y1=\"250\" x2=\"500\" y2=\"250\" fill=\"none\" stroke-width=\"70\" stroke-dasharray=\"140\"/>"
            "<rect data-frame=\"true\" class=\"mjx-dotted\" width=\"500\" height=\"600\" x=\"35\" y=\"-200\" fill=\"none\" stroke-width=\"70\" stroke-linecap=\"round\" stroke-dasharray=\"0 140\"/>"
            "<rect width=\"123\" height=\"42\" fill=\"blue\"/>"
            "<line x1=\"0\" y1=\"0\" x2=\"10\" y2=\"20\" stroke-width=\"5\"/>"
            "</g></svg>";
        expect(source_svg_paint::materialize(source) == expected);
        expect(source_svg_paint::materialize("<svg><path d=\"M0 1L2 3\"/></svg>") ==
               "<svg><path d=\"M0 1L2 3\"/></svg>");
    };

    "Source paint projection rejects unreviewed defaults and nonstandard width attributes"_test = [] {
        for (const auto source : {
                 "<line data-line=\"h\" class=\"mjx-dotted\" stroke-thickness=\"100\"/>",
                 "<line data-line=\"h\" class=\"mjx-solid\" stroke-width=\"0\"/>",
                 "<rect data-frame=\"true\" class=\"mjx-dashed\" style=\"fill:red\"/>",
                 "<line data-line=\"h\" class=\"future-style\"/>"}) {
            bool rejected = false;
            try {
                (void)source_svg_paint::materialize(
                    "<svg><g data-mml-node=\"mtable\">" + std::string(source) + "</g></svg>");
            } catch (std::invalid_argument const&) {
                rejected = true;
            }
            expect(rejected) << source;
        }
    };

    "Source CSS projection respects direct child selectors inline styles and raw titles"_test = [] {
        constexpr std::string_view source =
            "<mjx-container jax=\"SVG\"><svg viewBox=\"0 0 100 200\">"
            "<g data-mml-node=\"merror\"><rect data-background=\"true\" width=\"100\" height=\"200\"/>"
            "<title>Title & \"quoted\" <tag></title>"
            "<g data-mml-node=\"mi\" fill=\"blue\" stroke=\"blue\" transform=\"translate(12,0)\"/>"
            "<g data-mml-node=\"mi\" style=\"fill: green;\" transform=\"scale(1.5)\"/>"
            "<a href=\"https://example.test/\"><g data-mml-node=\"mi\"/></a>"
            "<svg width=\"1\" height=\"2\"><g data-mml-node=\"mi\"/></svg></g>"
            "<g data-mml-node=\"mtable\"><g><svg data-table=\"true\" viewBox=\"0 0 1 2\">"
            "<g><line data-line=\"v\" class=\"mjx-solid\" x1=\"1\" y1=\"0\" x2=\"1\" y2=\"2\"/></g>"
            "</svg></g></g>"
            "<foreignObject><mjx-tool><mjx-tip><svg width=\"1\" height=\"2\"/></mjx-tip></mjx-tool></foreignObject>"
            "</svg></mjx-container>";
        constexpr std::string_view expected =
            "<mjx-container jax=\"SVG\"><svg viewBox=\"0 0 100 200\" overflow=\"visible\">"
            "<g data-mml-node=\"merror\"><rect data-background=\"true\" width=\"100\" height=\"200\" fill=\"yellow\" stroke=\"none\"/>"
            "<title>Title & \"quoted\" <tag></title>"
            "<g data-mml-node=\"mi\" transform=\"translate(12,0)\" fill=\"red\" stroke=\"red\"/>"
            "<g data-mml-node=\"mi\" style=\"fill: green;\" transform=\"scale(1.5)\" fill=\"red\" stroke=\"red\"/>"
            "<a href=\"https://example.test/\" fill=\"blue\" stroke=\"blue\"><g data-mml-node=\"mi\"/></a>"
            "<svg width=\"1\" height=\"2\"><g data-mml-node=\"mi\"/></svg></g>"
            "<g data-mml-node=\"mtable\"><g><svg data-table=\"true\" viewBox=\"0 0 1 2\" overflow=\"visible\">"
            "<g><line data-line=\"v\" class=\"mjx-solid\" x1=\"1\" y1=\"0\" x2=\"1\" y2=\"2\" fill=\"none\" stroke-width=\"70\"/></g>"
            "</svg></g></g>"
            "<foreignObject><mjx-tool><mjx-tip><svg width=\"1\" height=\"2\"/></mjx-tip></mjx-tool></foreignObject>"
            "</svg></mjx-container>";
        expect(source_svg_paint::materialize(source) == expected);
        constexpr std::string_view outside =
            "<svg><line data-line=\"h\" class=\"mjx-solid\"/><rect data-background=\"true\"/>"
            "<g data-mml-node=\"mtable\"><g><line data-line=\"h\" class=\"mjx-dotted\"/></g></g></svg>";
        expect(source_svg_paint::materialize(outside) == outside)
            << "similarly named elements outside the actual selectors are unchanged";
    };

    "Source paint projection preserves foreign HTML and reenters nested SVG context"_test = [] {
        const std::string foreign =
            "<foreignObject><mjx-html><section data-order=\"a&amp;b\">"
            "<!-- comment > and <a href='untouched'> -->"
            "<br><input value=\"x\"><img src=\"literal.png\">"
            "<script>if (x < y) text = '<a href=\"literal\">';</script>"
            "<textarea><g data-mml-node=\"merror\"><g/></g></textarea>"
            "<a href=\"https://example.test/html\"><span>HTML link</span></a>"
            "<g data-mml-node=\"merror\"><g fill=\"green\"/></g>"
            "<svg xmlns=\"http://www.w3.org/2000/svg\"><a href=\"#nested\"><text>SVG</text></a></svg>"
            "</section></mjx-html></foreignObject>";
        const std::string source = "<mjx-container jax=\"SVG\"><svg>" + foreign + "</svg></mjx-container>";
        auto painted_foreign = foreign;
        const auto nested_link = painted_foreign.find("<a href=\"#nested\">");
        painted_foreign.insert(nested_link + std::string_view("<a href=\"#nested\"").size(),
                               " fill=\"blue\" stroke=\"blue\"");
        expect(source_svg_paint::materialize(source) ==
               "<mjx-container jax=\"SVG\"><svg overflow=\"visible\">" + painted_foreign +
                   "</svg></mjx-container>");
        expect(source_svg_paint::materialize(foreign) == foreign)
            << "an SVG link outside a MathJax host has no implicit blue paint";
        for (const auto source : {"<svg><g></svg>", "<svg><foreignObject><section><br></foreignObject></svg>"}) {
            bool rejected = false;
            try {
                (void)source_svg_paint::materialize(source);
            } catch (std::invalid_argument const&) {
                rejected = true;
            }
            expect(rejected) << "unbalanced structure is still rejected";
        }
    };

    "Source glyph blacker only materializes MathJax SVG glyph selectors"_test = [] {
        constexpr std::string_view source =
            "<mjx-container jax=\"SVG\"><svg>"
            "<defs><path id=\"outline\" d=\"M0 0L1 1\"/></defs>"
            "<g stroke-width=\"0\"><path data-c=\"78\" d=\"M0 0L1 1\" transform=\"translate(10,20)\"/>"
            "<use data-c=\"78\" href=\"#outline\"/>"
            "<path data-c=\"79\" d=\"M0 0L2 2\" stroke-width=\"5\" style=\"stroke-width: 9;\"/>"
            "<path d=\"M0 0L3 3\"/><text>Unknown font</text>"
            "<foreignObject><path data-c=\"78\" d=\"HTML payload\"/>"
            "<svg><path data-c=\"78\" d=\"M0 0L1 1\"/></svg></foreignObject>"
            "</g></svg></mjx-container>";
        constexpr std::string_view expected =
            "<mjx-container jax=\"SVG\"><svg overflow=\"visible\">"
            "<defs><path id=\"outline\" d=\"M0 0L1 1\"/></defs>"
            "<g stroke-width=\"0\"><path data-c=\"78\" d=\"M0 0L1 1\" transform=\"translate(10,20)\" stroke-width=\"3\"/>"
            "<use data-c=\"78\" href=\"#outline\" stroke-width=\"3\"/>"
            "<path data-c=\"79\" d=\"M0 0L2 2\" style=\"stroke-width: 9;\" stroke-width=\"3\"/>"
            "<path d=\"M0 0L3 3\"/><text>Unknown font</text>"
            "<foreignObject><path data-c=\"78\" d=\"HTML payload\"/>"
            "<svg><path data-c=\"78\" d=\"M0 0L1 1\" stroke-width=\"3\"/></svg></foreignObject>"
            "</g></svg></mjx-container>";
        expect(source_svg_paint::materialize(source) == expected);
        constexpr std::string_view outside = "<svg><path data-c=\"78\" d=\"M0 0L1 1\"/></svg>";
        expect(source_svg_paint::materialize(outside) == outside);
    };
};
