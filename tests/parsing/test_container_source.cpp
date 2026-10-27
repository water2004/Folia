#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.document;
import folia.markdown.ids;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

const BlockNode* find_kind(const BlockNode& block, BlockKind kind) {
    if (block.kind == kind) return &block;
    for (const auto& child : block.children) {
        if (const auto* found = find_kind(child, kind)) return found;
    }
    return nullptr;
}

const BlockNode* find_paragraph_with(const BlockNode& block, std::u32string_view text) {
    if (block.kind == BlockKind::Paragraph && block.inline_content.source == text) return &block;
    for (const auto& child : block.children) {
        if (const auto* found = find_paragraph_with(child, text)) return found;
    }
    return nullptr;
}

bool inline_sources_are_lossless(const BlockNode& block) {
    if (is_editable_block_owner(block.kind)
        && !block.inline_content.source.empty()) {
        const auto& document = block.inline_content;
        if (!tokens_partition_source(document.tree, document.source.size())
            || !roots_partition_source(document.tree, document.source.size())
            || flatten_tokens(document.tree, document.source) != document.source
            || serialize_lossless(document.tree, document.source) != document.source) {
            return false;
        }
    }
    for (const auto& child : block.children) {
        if (!inline_sources_are_lossless(child)) return false;
    }
    return true;
}

void append_signature(std::string& out, const BlockNode& block) {
    out += std::to_string(static_cast<int>(block.kind));
    out.push_back('(');
    if (block.kind == BlockKind::Paragraph || block.kind == BlockKind::Heading
        || block.kind == BlockKind::CalloutTitle || block.kind == BlockKind::TableCell) {
        out += cps_to_utf8(block.inline_content.source);
    } else if (block.kind == BlockKind::CodeBlock || block.kind == BlockKind::MathBlock) {
        out += cps_to_utf8(block.block_source.source());
    } else if (block.kind == BlockKind::List) {
        const auto& list = block.list_special();
        out += list.ordered ? "ordered" : "bullet";
        out.push_back(':');
        out += std::to_string(list.start);
        out += cps_to_utf8(std::u32string(1, list.delimiter));
    } else if (block.kind == BlockKind::ListItem || block.kind == BlockKind::TaskListItem) {
        out += cps_to_utf8(block.item_special().marker);
    }
    for (const auto& child : block.children) append_signature(out, child);
    out += ")";
}

std::string signature(const EditorDocument& document) {
    std::string result;
    append_signature(result, document.root);
    return result;
}

void expect_parse_save_reload_exact(std::string_view label, const std::string& source) {
    const auto parsed = parse_text(1, source);
    expect(bool(inline_sources_are_lossless(parsed.document.root))) << label << " inline source/CST";
    const auto saved = serialize_markdown(parsed.document);
    expect(bool(saved == source)) << label << " first save differs; actual=" << saved << "; expected=" << source;

    const auto reloaded = parse_text(2, saved);
    expect(bool(inline_sources_are_lossless(reloaded.document.root))) << label << " reload inline source/CST";
    expect(bool(signature(reloaded.document) == signature(parsed.document)))
        << label << " reload changed block structure or block-local source";
    expect(bool(serialize_markdown(reloaded.document) == source)) << label << " reload save differs";
}

} // namespace

suite container_source_parser_tests = [] {

"commonmark_lazy_quote_cases_keep_semantics_and_the_absent_prefix"_test = [] {
    {
        // CommonMark #93: the setext-looking line continues the open paragraph
        // lazily and remains literal paragraph text inside the quote.
        constexpr std::string_view source = "> foo\nbar\n===\n";
        const auto parsed = parse_text(1, std::string(source));
        const auto& roots = parsed.document.root.children;
        expect(bool(roots.size() == 1u));
        if (roots.size() != 1u) return;
        expect(bool(roots.front().kind == BlockKind::BlockQuote));
        if (roots.front().kind != BlockKind::BlockQuote) return;
        expect(bool(roots.front().children.size() == 1u));
        if (roots.front().children.size() != 1u) return;
        const auto& paragraph = roots.front().children.front();
        expect(bool(paragraph.kind == BlockKind::Paragraph));
        expect(bool(paragraph.inline_content.source == U"foo\nbar\n==="));
        expect_parse_save_reload_exact("CommonMark #93", std::string(source));
    }
    {
        // CommonMark #238: four physical spaces here are still the lazy
        // continuation of the quote paragraph, not an outside code block.
        constexpr std::string_view source = "> foo\n    - bar\n";
        const auto parsed = parse_text(1, std::string(source));
        const auto& roots = parsed.document.root.children;
        expect(bool(roots.size() == 1u));
        if (roots.size() != 1u) return;
        expect(bool(roots.front().kind == BlockKind::BlockQuote));
        if (roots.front().kind != BlockKind::BlockQuote) return;
        expect(bool(roots.front().children.size() == 1u));
        if (roots.front().children.size() != 1u) return;
        expect(bool(roots.front().children.front().kind == BlockKind::Paragraph));
        expect(bool(roots.front().children.front().inline_content.source == U"foo\n- bar"));
        expect_parse_save_reload_exact("CommonMark #238", std::string(source));
    }
    {
        // CommonMark #312: the final list marker is lazy paragraph text in
        // item d; indentation columns alone must not turn it into code.
        constexpr std::string_view source = "- a\n - b\n  - c\n   - d\n    - e\n";
        const auto parsed = parse_text(1, std::string(source));
        expect(bool(parsed.document.root.children.size() == 1u));
        if (parsed.document.root.children.size() != 1u) return;
        const auto* paragraph = find_paragraph_with(parsed.document.root, U"d\n- e");
        expect(bool(paragraph != nullptr));
        expect_parse_save_reload_exact("CommonMark #312", std::string(source));
    }
};

"tab_column_cases_preserve_visual_structure_and_original_bytes"_test = [] {
    // These official stimuli share the same contract: physical Tabs remain
    // Tabs in source/save while their expanded columns determine containment
    // and residual indentation in code content.
    struct TabCase {
        std::string_view label;
        std::string_view source;
        std::u32string_view expected_raw_code;
        std::u32string_view expected_code_projection;
    };
    const std::vector<TabCase> exact_cases{
        {"CommonMark #4", "  - foo\n\n\tbar\n", U"", U""},
        {"CommonMark #5", "- foo\n\n\t\tbar\n", U"bar", U"  bar"},
        {"CommonMark #6", ">\t\tfoo\n", U"foo", U"  foo"},
        {"CommonMark #7", "-\t\tfoo\n", U"foo", U"  foo"},
        {"CommonMark #9", " - foo\n   - bar\n\t - baz\n", U"", U""},
    };
    for (const auto& test : exact_cases) {
        const auto label = test.label;
        const std::string source(test.source);
        const auto parsed = parse_text(1, source);
        expect(bool(inline_sources_are_lossless(parsed.document.root))) << label;
        expect(bool(serialize_markdown(parsed.document) == source)) << label << " exact save";
        const auto reloaded = parse_text(2, serialize_markdown(parsed.document));
        expect(bool(signature(reloaded.document) == signature(parsed.document))) << label << " reload tree";
        expect(bool(serialize_markdown(reloaded.document) == source)) << label << " reload bytes";
        if (!test.expected_raw_code.empty()) {
            const auto* code = find_kind(parsed.document.root, BlockKind::CodeBlock);
            expect(bool(code != nullptr)) << label << " code block";
            if (code) {
                expect(bool(block_source_content(code->block_source)
                    == test.expected_raw_code)) << label << " exact raw code content";
                const auto projection = block_source_code_projection(code->block_source);
                expect(bool(projection == test.expected_code_projection))
                    << label << " semantic code indentation";
                expect(bool(code->block_source.source().find(U'\t') != std::u32string::npos))
                    << label << " raw Tab retained";
            }
        }
        if (label == "CommonMark #9") {
            const auto& roots = parsed.document.root.children;
            expect(bool(roots.size() == 1u)) << label << " root list";
            if (roots.size() == 1u) {
                expect(bool(roots.front().kind == BlockKind::List)) << label << " outer list";
                expect(bool(!roots.front().children.empty())) << label << " outer item";
                if (!roots.front().children.empty()) {
                    const auto* nested = find_kind(roots.front().children.front(), BlockKind::List);
                    expect(bool(nested != nullptr)) << label << " list under foo";
                    if (nested && !nested->children.empty()) {
                        const auto* deeper = find_kind(nested->children.front(), BlockKind::List);
                        expect(bool(deeper != nullptr)) << label << " baz nested under bar";
                        if (deeper && !deeper->children.empty()) {
                            const auto* baz = find_paragraph_with(deeper->children.front(), U"baz");
                            expect(bool(baz != nullptr)) << label << " innermost baz paragraph";
                        }
                    }
                }
            }
        }
        if (label == "CommonMark #4") {
            const auto& roots = parsed.document.root.children;
            expect(bool(roots.size() == 1u)) << label << " list root";
            if (roots.size() == 1u) {
                expect(bool(roots.front().kind == BlockKind::List)) << label << " list kind";
                expect(bool(roots.front().children.size() == 1u)) << label << " one list item";
                if (roots.front().children.size() == 1u) {
                    const auto& blocks = roots.front().children.front().children;
                    expect(bool(blocks.size() == 2u)) << label << " two paragraph children";
                    if (blocks.size() == 2u) {
                        expect(bool(blocks[0].kind == BlockKind::Paragraph && blocks[1].kind == BlockKind::Paragraph));
                        expect(bool(blocks[0].inline_content.source == U"foo"));
                        expect(bool(blocks[1].inline_content.source == U"bar"));
                    }
                }
            }
        }
    }
};

"marker_only_list_items_blank_gaps_and_original_prefixes_survive_reload"_test = [] {
    const std::vector<std::string> cases{
        // CommonMark #278: marker-only items bracket paragraph/code content.
        "-\n  foo\n-\n  ```\n  bar\n  ```\n-\n      baz\n",
        // CommonMark #279: spaces after a marker are padding, not content.
        "-   \n  foo\n",
        "-   foo\n",
        // Distinct physical quote prefixes, lazy lines, and nested list gaps.
        "># Foo\n>\tbar\nplain continuation\n",
        "* a\n  > b\n  >\n* c\n",
        "- parent\n  > child\n  >\n  - nested\n\n- sibling\n",
        // Exact line endings are syntax source too, including lone CR.
        ">one\r\n>  two\r\n>\r\n>three\r\n",
        "- first\r- second\r",
    };
    for (std::size_t index = 0; index < cases.size(); ++index) {
        expect_parse_save_reload_exact("container prefix case " + std::to_string(index), cases[index]);
    }

    const auto marker_only = parse_text(1, cases.front());
    const auto* list = find_kind(marker_only.document.root, BlockKind::List);
    const auto* code = find_kind(marker_only.document.root, BlockKind::CodeBlock);
    expect(bool(list != nullptr));
    expect(bool(code != nullptr));
    if (list) {
        expect(bool(list->children.size() == 3u));
        if (list->children.size() == 3u) {
            expect(bool(find_kind(list->children[0], BlockKind::Paragraph) != nullptr));
            expect(bool(find_kind(list->children[1], BlockKind::CodeBlock) != nullptr));
            expect(bool(find_kind(list->children[2], BlockKind::CodeBlock) != nullptr));
        }
    }

    const auto padded_marker = parse_text(1, cases[1]);
    const auto* padded_list = find_kind(padded_marker.document.root, BlockKind::List);
    expect(bool(padded_list != nullptr));
    if (padded_list && padded_list->children.size() == 1u) {
        const auto* paragraph = find_kind(padded_list->children.front(), BlockKind::Paragraph);
        expect(bool(paragraph != nullptr));
        if (paragraph) expect(bool(paragraph->inline_content.source == U"foo"));
    } else if (padded_list) {
        expect(bool(padded_list->children.size() == 1u));
    }
};

}; // suite container_source_parser_tests
