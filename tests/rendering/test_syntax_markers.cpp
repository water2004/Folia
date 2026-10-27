#include <algorithm>
#include <string>
#include <vector>
#include "support/folia_test.hpp"
import folia.markdown.parser;
import folia.markdown.inline_parser;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.core.render_builder;
import folia.core.render_model;
import folia.core.syntax_marker;
import folia.core.theme;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace { suite syntax_marker_tests = [] {

"syntax marker projection follows complete CST constructs rather than character spelling"_test = [] {
    struct Example { const char* text; SyntaxMarker icon; };
    for (auto example : {
        Example{"*", SyntaxMarker::None}, {"**", SyntaxMarker::None}, {"*x", SyntaxMarker::None},
        {"**x", SyntaxMarker::None}, {"_", SyntaxMarker::None}, {"__x", SyntaxMarker::None},
        {"~~", SyntaxMarker::None}, {"~~x", SyntaxMarker::None}, {"`", SyntaxMarker::None},
        {"`x", SyntaxMarker::None}, {"$", SyntaxMarker::None}, {"$x", SyntaxMarker::None},
        {"\\*x\\*", SyntaxMarker::None}, {"a_b_c", SyntaxMarker::None},
        {"*x*", SyntaxMarker::Star}, {"_x_", SyntaxMarker::Underscore},
        {"**x**", SyntaxMarker::DoubleStar}, {"__x__", SyntaxMarker::DoubleUnderscore},
        {"~~x~~", SyntaxMarker::DoubleTilde}, {"`*x*`", SyntaxMarker::Backtick},
        {"``x``", SyntaxMarker::DoubleBacktick}, {"$x$", SyntaxMarker::Dollar}
    }) {
        auto parsed = parse_text(1, std::string(example.text) + "\n");
        auto model = build_render_model(parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
        std::vector<SyntaxMarker> icons;
        for (auto const& block : model.blocks)
        for (auto const& item : block.inline_items) {
            if (item.special().syntax_marker != SyntaxMarker::None) icons.push_back(item.special().syntax_marker);
            auto const& semantic = item.special().semantic();
            if (semantic.opening_marker != SyntaxMarker::None) icons.push_back(semantic.opening_marker);
            if (semantic.closing_marker != SyntaxMarker::None) icons.push_back(semantic.closing_marker);
        }
        if (example.icon == SyntaxMarker::None) expect(icons.empty()) << example.text;
        else {
            expect(icons.size() == 2_u) << example.text;
            expect(std::ranges::all_of(icons, [&](auto icon) { return icon == example.icon; })) << example.text;
        }
    }
    for (auto source : {U"*", U"**", U"*x", U"`x", U"$x"}) {
        InlineParseContext context;
        context.next_id = NodeId{1000};
        auto tree = parse_inline(source, context);
        expect(static_cast<bool>(flatten_tokens(tree, source) == std::u32string{source}));
        expect(std::ranges::any_of(tree.nodes, [](auto const& node) {
            return node.kind == InlineCstKind::Incomplete && node.status == ParseStatus::MissingCloser;
        }));
    }
};

"display math marker projection consumes block source tokens and completeness"_test = [] {
    for (auto text : {"$$\nx\n$$\n", "\\[\nx\n\\]\n", "$$\nx", "\\[\nx"}) {
        auto parsed = parse_text(1, text);
        auto model = build_render_model(parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
        expect(model.blocks.size() == 1_u);
        if (model.blocks.empty()) continue;
        auto const& block = model.blocks.front();
        expect(block.kind == RenderBlockKind::Math);
        auto const& markers = block.special().source_markers;
        auto complete = std::string_view{text}.ends_with("\n");
        expect(markers.size() == (complete ? 2u : 1u));
        for (auto const& marker : markers) {
            expect(marker.range.end <= block.special().raw_source.size());
            expect((marker.icon != SyntaxMarker::None) == complete);
            if (complete) expect(marker.range.length() == 2_u);
        }
    }
};
"incomplete formatting preserves independently parsed math in its content"_test = [] {
    for (auto prefix : {"~", "~~", "*", "**"})
    for (auto container : {"", "1. ", "> ", "- "}) {
        auto source = std::string{container} + "微观粒子质量" + prefix
            + "原子质量单位 $1u=1.6605655\\timesz10^{-27}kg$\n";
        auto parsed = parse_text(1, source);
        auto model = build_render_model(parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
        auto count_cst = [&](auto& self, const auto& block) -> std::size_t {
            auto inline_count = [&](auto& visit, const InlineCstNodes& nodes) -> std::size_t {
                std::size_t count = 0;
                for (const auto& node : nodes) {
                    count += node.kind == InlineCstKind::InlineMath ? 1 : 0;
                    count += visit(visit, node.children);
                }
                return count;
            };
            std::size_t count = inline_count(inline_count, block.inline_content.tree.nodes);
            for (const auto& child : block.children) count += self(self, child);
            return count;
        };
        expect(count_cst(count_cst, parsed.document.root) == 1_u) << prefix << container;
        auto count_math = [&](auto& self, const std::vector<InlineRenderItem>& items) -> std::size_t {
            std::size_t count = 0;
            for (const auto& item : items) {
                count += item.kind == InlineRenderItem::Kind::Math ? 1 : 0;
                count += self(self, item.special().semantic().children);
            }
            return count;
        };
        std::size_t math_count = 0;
        for (const auto& block : model.blocks) math_count += count_math(count_math, block.inline_items);
        expect(math_count == 1_u) << prefix << container;
    }
};
}; }
