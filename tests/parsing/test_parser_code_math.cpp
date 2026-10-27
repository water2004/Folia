#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.serializer;
import folia.markdown.utf;
import folia.markdown.dialect;

using namespace folia;
using namespace boost::ut;

#include "support/parser_test_support.hpp"
#include "support/commonmark_projection.hpp"

suite code_math_leaf_parser = [] {
    "quote_fence_source_drops_only_container_prefixes"_test = [] {
        const std::string input = "> ```cpp\r\n> x\r\n> ```\r\n";
        const auto parsed = parse_text(1, input);
        const auto* code = first_block(parsed.document.root.children, BlockKind::CodeBlock);
        expect(fatal(code != nullptr));
        if (!code) return;
        expect(code->block_source.source() == U"```cpp\r\nx\r\n```");
        expect(serialize_markdown(parsed.document) == input);
    };

    "fence_closing_eol_is_consumed_but_not_owned_by_block_source"_test = [] {
        const std::string input = "```cpp\r\nx\r\n```\r\nnext\n";
        const auto parsed = parse_text(1, input);
        expect(fatal(!parsed.document.root.children.empty()));
        if (parsed.document.root.children.empty()) return;
        const auto& code = parsed.document.root.children.front();
        expect(code.kind == BlockKind::CodeBlock);
        expect(code.block_source.source() == U"```cpp\r\nx\r\n```");
        expect(serialize_markdown(parsed.document) == input);
    };

    "math_close_midline_leaves_suffix_for_the_next_block"_test = [] {
        const std::string input = "$$x$$tail\n";
        const auto parsed = parse_text(1, input);
        expect(fatal(parsed.document.root.children.size() == 2u));
        if (parsed.document.root.children.size() != 2u) return;
        const auto& math = parsed.document.root.children[0];
        expect(math.kind == BlockKind::MathBlock);
        expect(math.block_source.source() == U"$$x$$");
        const auto& suffix = parsed.document.root.children[1];
        expect(suffix.kind == BlockKind::Paragraph);
        expect(suffix.inline_content.source == U"tail");
        expect(serialize_markdown(parsed.document) == input);
    };

    "bracket_math_close_midline_uses_the_same_suffix_handoff"_test = [] {
        const std::string input = "\\[x\\]tail\n";
        const auto parsed = parse_text(1, input);
        expect(fatal(parsed.document.root.children.size() == 2u));
        if (parsed.document.root.children.size() != 2u) return;
        expect(parsed.document.root.children[0].kind == BlockKind::MathBlock);
        expect(parsed.document.root.children[0].block_source.source() == U"\\[x\\]");
        expect(parsed.document.root.children[1].inline_content.source == U"tail");
        expect(serialize_markdown(parsed.document) == input);
    };

    "math_info_fence_keeps_existing_block_kind"_test = [] {
        const std::string input = "~~~math\nx\n~~~\n";
        const auto parsed = parse_text(1, input);
        expect(fatal(parsed.document.root.children.size() == 1u));
        if (parsed.document.root.children.empty()) return;
        const auto& math = parsed.document.root.children.front();
        expect(math.kind == BlockKind::MathBlock);
        expect(math.atomic_special().math_delim == MathDelimiter::FencedMath);
        expect(math.block_source.source() == U"~~~math\nx\n~~~");
        expect(serialize_markdown(parsed.document) == input);
    };

    "unclosed_math_retains_each_original_line_ending"_test = [] {
        const std::string input = "$$\rbody\r";
        const auto parsed = parse_text(1, input);
        expect(fatal(parsed.document.root.children.size() == 1u));
        if (parsed.document.root.children.empty()) return;
        const auto& math = parsed.document.root.children.front();
        expect(math.kind == BlockKind::MathBlock);
        expect(math.block_source.source() == utf8_to_cps(input));
        expect(serialize_markdown(parsed.document) == input);
    };

    "fenced_opening_indent_projects_only_derived_content"_test = [] {
        const std::string input = "  ```\n    x\n  ```\n";
        const auto parsed = parse_text(1, input);
        expect(fatal(!parsed.document.root.children.empty()));
        if (parsed.document.root.children.empty()) return;
        const auto& code = parsed.document.root.children.front();
        expect(code.block_source.source() == U"  ```\n    x\n  ```");
        expect(block_source_code_projection(code.block_source) == U"  x");
        expect(folia::test::commonmark::projection_detail::code_block_text(code.block_source) == "  x\n");
        expect(serialize_markdown(parsed.document) == input);
    };

    "indented_code_trims_blank_projection_without_changing_source"_test = [] {
        const std::string input = "    code\n    \n";
        const auto parsed = parse_text(1, input);
        expect(fatal(parsed.document.root.children.size() == 1u));
        if (parsed.document.root.children.empty()) return;
        const auto& code = parsed.document.root.children.front();
        expect(code.block_source.source() == utf8_to_cps(input));
        expect(block_source_content(code.block_source) == U"code\n");
        expect(serialize_markdown(parsed.document) == input);
    };

    "partial_quote_tab_columns_reach_indented_code_context"_test = [] {
        const auto parsed = parse_text(1, ">\t\tfoo\n", folia::test::commonmark::dialect());
        const auto* code = first_block(parsed.document.root.children, BlockKind::CodeBlock);
        expect(fatal(code != nullptr));
        if (!code) return;
        expect(code->block_source.source() == U"\tfoo\n");
        expect(block_source_code_projection(code->block_source) == U"  foo");
        const auto& contexts = code->block_source.storage->indentation_contexts;
        expect(!contexts.empty());
        if (!contexts.empty()) expect(contexts.front() == BlockLineIndentation{0, 4, 2});
    };
};
