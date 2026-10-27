#include <algorithm>
#include <ranges>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

std::optional<std::u32string> nested_quote_paragraph(const BlockVec& blocks) {
    if (blocks.size() != 1 || blocks.front().kind != BlockKind::BlockQuote) return std::nullopt;
    const auto& outer_quote = blocks.front();
    if (outer_quote.children.size() != 1 || outer_quote.children.front().kind != BlockKind::List) {
        return std::nullopt;
    }
    const auto& list = outer_quote.children.front();
    if (list.children.size() != 1 || list.children.front().children.size() != 1
        || list.children.front().children.front().kind != BlockKind::BlockQuote) {
        return std::nullopt;
    }
    const auto& inner_quote = list.children.front().children.front();
    if (inner_quote.children.size() != 1 || inner_quote.children.front().kind != BlockKind::Paragraph) {
        return std::nullopt;
    }
    return inner_quote.children.front().inline_content.source;
}

bool contains_kind(const BlockNode& block, BlockKind kind) {
    if (block.kind == kind) return true;
    return std::ranges::any_of(block.children, [&](const BlockNode& child) {
        return contains_kind(child, kind);
    });
}

} // namespace

suite parser_nested_lazy_continuation_tests = [] {

"nested_ordinary_lazy_continuations_survive_save_and_reload"_test = [] {
    for (const auto& source : std::vector<std::string>{
            "> 1. > Blockquote\ncontinued here.\n",
            "> 1. > Blockquote\n> continued here.\n"}) {
        const auto parsed = parse_text(1, source);
        const auto paragraph = nested_quote_paragraph(parsed.document.root.children);
        expect(fatal(bool(paragraph.has_value()))) << source;
        if (!paragraph) continue;
        expect(fatal(bool(*paragraph == U"Blockquote\ncontinued here."))) << source;

        const auto saved = serialize_markdown(parsed.document);
        const auto reloaded = parse_text(2, saved);
        const auto reloaded_paragraph = nested_quote_paragraph(reloaded.document.root.children);
        expect(fatal(bool(reloaded_paragraph.has_value()))) << saved;
        if (reloaded_paragraph) {
            expect(fatal(bool(*reloaded_paragraph == U"Blockquote\ncontinued here."))) << saved;
        }
        expect(fatal(bool(serialize_markdown(reloaded.document) == saved))) << saved;
    }
};

"nested_leaf_interrupts_remain_blocks_after_save_and_reload"_test = [] {
    for (const auto& source : std::vector<std::string>{
            "> 1. > Blockquote\n# Outside\n",
            "> 1. > Blockquote\n---\n"}) {
        const auto parsed = parse_text(1, source);
        expect(fatal(bool(parsed.document.root.children.size() == 2u))) << source;
        if (parsed.document.root.children.size() != 2) continue;
        expect(fatal(bool(parsed.document.root.children[0].kind == BlockKind::BlockQuote))) << source;
        expect(fatal(bool(parsed.document.root.children[1].kind
            == (source.find("# Outside") != std::string::npos
                ? BlockKind::Heading : BlockKind::ThematicBreak)))) << source;
        const auto paragraph = nested_quote_paragraph(BlockVec{parsed.document.root.children[0]});
        expect(fatal(bool(paragraph && *paragraph == U"Blockquote"))) << source;

        const auto saved = serialize_markdown(parsed.document);
        const auto reloaded = parse_text(2, saved);
        expect(fatal(bool(reloaded.document.root.children.size() == 2u))) << saved;
        if (reloaded.document.root.children.size() == 2) {
            expect(fatal(bool(reloaded.document.root.children[1].kind
                == parsed.document.root.children[1].kind))) << saved;
            const auto reloaded_paragraph = nested_quote_paragraph(
                BlockVec{reloaded.document.root.children[0]});
            expect(fatal(bool(reloaded_paragraph && *reloaded_paragraph == U"Blockquote"))) << saved;
        }
        expect(fatal(bool(serialize_markdown(reloaded.document) == saved))) << saved;
    }
};

"list_marker_padding_over_four_columns_is_not_leaf_paragraph_content"_test = [] {
    const auto parsed = parse_text(1, "- paragraph\n  1.     code\n");
    expect(fatal(bool(parsed.document.root.children.size() == 1u)));
    if (parsed.document.root.children.size() == 1) {
        expect(fatal(bool(parsed.document.root.children.front().kind == BlockKind::List)));
        expect(fatal(bool(contains_kind(parsed.document.root.children.front(), BlockKind::CodeBlock))));
    }
};

"indented_code_inside_quote_is_not_lazy_paragraph_content"_test = [] {
    const auto parsed = parse_text(1, "> paragraph\n>\n>     code\n");
    expect(fatal(bool(parsed.document.root.children.size() == 1u)));
    if (parsed.document.root.children.size() == 1) {
        const auto& quote = parsed.document.root.children.front();
        expect(fatal(bool(quote.kind == BlockKind::BlockQuote)));
        expect(fatal(bool(quote.children.size() == 2u)));
        if (quote.children.size() == 2) {
            expect(fatal(bool(quote.children[0].kind == BlockKind::Paragraph)));
            expect(fatal(bool(quote.children[1].kind == BlockKind::CodeBlock)));
        }
    }
};

"task_prefixes_are_descended_only_when_task_lists_are_enabled"_test = [] {
    const std::string source = "> - [ ] > Blockquote\ncontinued here.\n";
    const auto task_enabled = parse_text(1, source);
    expect(fatal(bool(task_enabled.document.root.children.size() == 1u)));
    if (task_enabled.document.root.children.size() == 1) {
        const auto& outer_quote = task_enabled.document.root.children.front();
        expect(fatal(bool(outer_quote.kind == BlockKind::BlockQuote)));
        expect(fatal(bool(outer_quote.children.size() == 1u)));
        if (outer_quote.children.size() == 1) {
            const auto& task_list = outer_quote.children.front();
            expect(fatal(bool(task_list.kind == BlockKind::TaskList)));
            expect(fatal(bool(task_list.children.size() == 1u)));
            if (task_list.children.size() == 1) {
                const auto& item = task_list.children.front();
                expect(fatal(bool(item.children.size() == 1u)));
                if (item.children.size() == 1) {
                    const auto& inner_quote = item.children.front();
                    expect(fatal(bool(inner_quote.kind == BlockKind::BlockQuote)));
                    expect(fatal(bool(inner_quote.children.size() == 1u)));
                    if (inner_quote.children.size() == 1) {
                        expect(fatal(bool(inner_quote.children.front().inline_content.source
                            == U"Blockquote\ncontinued here.")));
                    }
                }
            }
        }
    }

    auto no_tasks = default_dialect();
    no_tasks.gfm.task_lists = false;
    const auto task_disabled = parse_text(2, source, no_tasks);
    expect(fatal(bool(task_disabled.document.root.children.size() == 1u)));
    if (task_disabled.document.root.children.size() == 1) {
        const auto& outer_quote = task_disabled.document.root.children.front();
        expect(fatal(bool(outer_quote.children.size() == 1u)));
        if (outer_quote.children.size() == 1) {
            const auto& list = outer_quote.children.front();
            expect(fatal(bool(list.kind == BlockKind::List)));
            expect(fatal(bool(list.children.size() == 1u)));
            if (list.children.size() == 1 && list.children.front().children.size() == 1) {
                expect(fatal(bool(list.children.front().children.front().kind == BlockKind::Paragraph)));
                expect(fatal(bool(list.children.front().children.front().inline_content.source
                    == U"[ ] > Blockquote\ncontinued here.")));
            }
        }
    }
};


"list_code_retains_real_tabs_in_its_lossless_source"_test = [] {
    const std::string source = "-\t\tfoo\n";
    const auto parsed = parse_text(1, source);
    bool found = false;
    auto inspect = [&](auto&& self, const BlockNode& block) -> void {
        if (block.kind == BlockKind::CodeBlock) {
            found = true;
            const auto& raw = block.block_source.source();
            expect(raw.find(U'\t') != std::u32string::npos);
            expect(raw.find(U' ') == std::u32string::npos);
            expect(block_source_tokens_partition(block.block_source));
            expect(flatten_block_source_tokens(block.block_source) == raw);
        }
        for (const auto& child : block.children) self(self, child);
    };
    inspect(inspect, parsed.document.root);
    expect(found);
    expect(serialize_markdown(parsed.document) == source);
};

}; // suite parser_nested_lazy_continuation_tests
