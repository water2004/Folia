#include "support/folia_test.hpp"
import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.dialect;
import folia.markdown.serializer;
import folia.markdown.block_tree;
import folia.markdown.document_text;
import folia.markdown.inline_cst;
import folia.markdown.instrumentation;

using namespace folia;
using namespace boost::ut;

namespace {
suite block_fragment_plan_tests = [] {
    "rejected block discovery builds no discarded inline CST"_test = [] {
        std::u32string source;
        for (int i = 0; i < 10000; ++i) source += U"Sample paragraph with words.\n";
        reset_core_operation_counters();
        bool inspected = false;
        auto plan = plan_block_fragment(source, default_dialect(), [&](auto const& blocks) {
            inspected = true;
            expect(fatal(blocks.size() == 1_u));
            expect(blocks.front().inline_content.tree.nodes.empty());
            return false;
        });
        auto counters = read_core_operation_counters();
        expect(inspected && !plan);
        expect(counters.block_fragment_parses == 1_u);
        expect(counters.inline_reparses == 0_u);
        expect(counters.inline_scanned_characters == 0_u);
    };

    "accepted block discovery finalizes the canonical fragment including container prefixes"_test = [] {
        for (auto source : {U"before **bold**\n\n$$\nx+1\n$$\n\nafter",
            U"> before **bold**\n> $$\n> x+1\n> $$", U"- before\n\n  ```cpp\n  int x;\n  ```"}) {
            auto expected = parse_block_fragment(source);
            auto plan = plan_block_fragment(source, default_dialect(), [](auto const&) { return true; });
            expect(fatal(plan.has_value()));
            expect(bool(serialize_markdown_fragment(plan->blocks) == serialize_markdown_fragment(expected.blocks)));
            bool inlines = false;
            for (auto const& root : plan->blocks) walk_blocks(root, [&](auto const& block) {
                if (auto document = editable_inline_document(block)) {
                    inlines = true;
                    expect(!document->tree.nodes.empty() || document->source.empty());
                    expect(bool(flatten_tokens(document->tree, document->source) == document->source));
                }
            });
            expect(inlines);
        }
    };
};
}
