#include "support/source_svg_paint.hpp"
#include "support/folia_test.hpp"

namespace {
using namespace boost::ut;
}

suite native_source_svg_paint = [] {
    "Source table CSS is materialized without changing geometry or unrelated SVG bytes"_test = [] {
        const std::string source =
            "<svg><g stroke=\"currentColor\" fill=\"currentColor\" stroke-width=\"0\">"
            "<path data-c=\"78\" d=\"M0 0L10 20\"/>"
            "<line data-line=\"v\" class=\"mjx-solid\" x1=\"100\" y1=\"-200\" x2=\"100\" y2=\"400\"/>"
            "<line data-line=\"h\" class=\"mjx-dashed\" x1=\"0\" y1=\"250\" x2=\"500\" y2=\"250\"/>"
            "<rect data-frame=\"true\" class=\"mjx-dotted\" width=\"500\" height=\"600\" x=\"35\" y=\"-200\"/>"
            "<rect width=\"123\" height=\"42\" fill=\"blue\"/>"
            "<line x1=\"0\" y1=\"0\" x2=\"10\" y2=\"20\" stroke-width=\"5\"/>"
            "</g></svg>";
        const std::string expected =
            "<svg><g stroke=\"currentColor\" fill=\"currentColor\" stroke-width=\"0\">"
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
                (void)source_svg_paint::materialize(source);
            } catch (std::invalid_argument const&) {
                rejected = true;
            }
            expect(rejected) << source;
        }
    };
};
