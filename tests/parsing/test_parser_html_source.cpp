#include "support/folia_test.hpp"
import folia.markdown.parser_source_session;
import folia.markdown.parser_leaf_line_view;
import folia.markdown.parser_html_source_view;

using namespace boost::ut;
using namespace folia::parser_source_detail;

static LeafLine line(std::initializer_list<PhysicalSourceRange> content,
                     PhysicalSourceRange ending) {
    LeafLine result;
    result.content_spans.assign(content);
    result.ending = ending;
    return result;
}

suite html_source_view_contract = [] {
    "HTML materializes accepted leaf once and maps local ranges"_test = [] {
        RootPhysicalSource root(U"> <div>\r\n>   <p>x</p>\r\n> </div>\r\n");
        LeafSourceAccumulator leaf(root);
        leaf.append_line(line({{2, 7}}, {7, 9}), true);
        leaf.append_line(line({{13, 21}}, {21, 23}), true);
        leaf.append_content(line({{25, 31}}, {31, 33}));
        HtmlSourceView view(leaf);

        expect(view.source() == U"<div>\r\n<p>x</p>\r\n</div>");
        expect(view.size() == leaf.size());
        expect(view.physical_offset(0) == 2u);
        expect(view.physical_offset(7) == 13u);
        expect(view.physical_offset(view.size()) == 31u);
        const auto range = view.physical_range(7, 15);
        expect(range.has_value());
        if (range) {
            expect(range->begin == 13u);
            expect(range->end == 21u);
        }
        const auto inner = view.slice(7, 15);
        expect(inner.has_value());
        if (inner) expect(*inner == U"<p>x</p>");
    };

    "empty HTML ranges map to the leaf physical boundary"_test = [] {
        RootPhysicalSource root(U">\r\n");
        LeafSourceAccumulator leaf(root);
        leaf.append_content(line({{1, 1}}, {1, 3}));
        HtmlSourceView view(leaf);
        expect(view.source().empty());
        expect(view.physical_offset(0) == 1u);
        const auto empty = view.physical_range(0, 0);
        expect(empty.has_value());
        if (empty) expect(empty->begin == empty->end);
    };

    "out-of-range local spans are rejected"_test = [] {
        RootPhysicalSource root(U"<p>x</p>");
        LeafSourceAccumulator leaf(root);
        leaf.append_content(line({{0, 8}}, {8, 8}));
        HtmlSourceView view(leaf);
        expect(!view.physical_offset(9));
        expect(!view.physical_range(4, 3));
        expect(!view.slice(0, 9));
    };
};
