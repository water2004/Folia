#include "support/folia_test.hpp"
import folia.markdown.dialect;
import folia.markdown.ids;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

suite inline_math_storage = [] {
    "inline_math_storage_default_semantics_do_not_allocate_per_formula"_test = [] {
        std::u32string source;
        for (std::size_t index = 0; index < 4096; ++index)
            source += U"$x_{" + utf8_to_cps(std::to_string(index)) + U"}$ ";
        InlineParseContext context;
        context.dialect = default_dialect();
        context.next_id = NodeId{1};
        const auto tree = parse_inline(source, context);
        std::size_t formulas = 0;
        for (auto const& node : tree.nodes) {
            if (node.kind != InlineCstKind::InlineMath) continue;
            ++formulas;
            expect(node.semantic == nullptr);
            expect(node.semantics().math_delim == MathDelimiter::InlineDollar);
            expect(node.status == ParseStatus::Complete);
            const auto& ranges = node.delimiter_ranges();
            expect(source.substr(ranges.opening.start, ranges.opening.length()) == U"$");
            expect(fatal(ranges.closing.has_value()));
            expect(source.substr(ranges.closing->start, ranges.closing->length()) == U"$");
        }
        expect(formulas == 4096_u);
        expect(tokens_partition_source(tree, source.size()));
        expect(roots_partition_source(tree, source.size()));
        expect(serialize_lossless(tree, source) == source);
    };

    "inline_math_storage_nondefault_delimiters_remain_distinct"_test = [] {
        const std::u32string source = U"$a$ \\(b\\) $c$";
        InlineParseContext context;
        context.dialect = default_dialect();
        context.dialect.math.inline_paren = true;
        context.next_id = NodeId{1};
        const auto tree = parse_inline(source, context);
        std::vector<MathDelimiter> delimiters;
        for (auto const& node : tree.nodes) {
            if (node.kind == InlineCstKind::InlineMath)
                delimiters.push_back(node.semantics().math_delim);
        }
        expect(delimiters == std::vector<MathDelimiter>{
            MathDelimiter::InlineDollar, MathDelimiter::InlineParen, MathDelimiter::InlineDollar});
        expect(serialize_lossless(tree, source) == source);
    };
};
