#include <algorithm>
#include <cstddef>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ids;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.core.document_edit;
import folia.core.document_history;
import folia.core.document_ids;
import folia.core.document_operation_apply;
import folia.markdown.document_symbols;
import folia.markdown.document_text;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.text_edit;

using namespace folia;
using namespace boost::ut;

#include "support/document_edit_test_support.hpp"

suite editing_input_block_prefixes_tests = [] {

"typed_block_markers_apply_local_tree_rules"_test = [] {
    struct Case {
        std::u32string input;
        BlockKind root_kind;
        BlockKind content_kind;
        std::u32string marker;
        std::uint8_t heading_level = 0;
        bool ordered = false;
    };
    const std::vector<Case> cases{
        {U"## ", BlockKind::Heading, BlockKind::Heading, U"## ", 2, false},
        {U"> ", BlockKind::BlockQuote, BlockKind::Paragraph, U"> ", 0, false},
        {U"- ", BlockKind::List, BlockKind::Paragraph, U"- ", 0, false},
        {U"7) ", BlockKind::List, BlockKind::Paragraph, U"7) ", 0, true},
        {U"- [x] ", BlockKind::TaskList, BlockKind::Paragraph, U"- [x] ", 0, false},
    };
    for (const auto& test : cases) {
        auto document = parse_document("");
        normalize_document(document);
        auto selection = caret(first_editable(document));
        auto [after, result] = type_text(std::move(document), selection, test.input);
        const auto& root = after.root.children.front();
        expect(fatal(bool(root.kind == test.root_kind)));
        const BlockNode* content = &root;
        if (root.kind == BlockKind::BlockQuote) content = &root.children.front();
        if (root.kind == BlockKind::List || root.kind == BlockKind::TaskList) content = &root.children.front().children.front();
        expect(fatal(bool(content->kind == test.content_kind)));
        expect(fatal(bool(content->inline_content().source.empty())));
        expect(fatal(bool(result.active.container_id == content->id)));
        expect(fatal(bool(result.active.source_offset == 0u)));
        if (root.kind == BlockKind::Heading) {
            expect(fatal(bool(root.text_special().level == test.heading_level)));
            expect(fatal(bool(root.text_special().opening_marker == test.marker)));
        } else if (root.kind == BlockKind::List || root.kind == BlockKind::TaskList) {
            expect(fatal(bool(root.children.front().item_special().marker == test.marker)));
            expect(fatal(bool(root.list_special().ordered == test.ordered)));
        }
        expect_document_valid(after);
    }
};

"backspace_at_downstream_block_start_removes_one_structural_prefix"_test = [] {
    const std::vector<std::u32string> markers{U"# ", U"> ", U"- ", U"1. ", U"- [ ] "};
    for (const auto& marker : markers) {
        auto document = parse_document("");
        normalize_document(document);
        const auto initial = caret(first_editable(document));
        auto [structured, at_content_start] = type_text(
            std::move(document), initial, marker);
        auto [with_content, after_content] = type_text(
            std::move(structured), at_content_start, U"content");
        auto deletion = test_edit::document_delete_backward(
            with_content,
            TextSelection::caret({
                after_content.active.container_id,
                0,
                TextAffinity::Downstream}));
        expect(fatal(bool(deletion.has_value())));
        if (!deletion) continue;
        expect(fatal(bool(deletion->after.root.children.size() == 1u)));
        expect(fatal(bool(deletion->after.root.children.front().kind == BlockKind::Paragraph)));
        expect(fatal(bool(deletion->after.root.children.front().inline_content().source == U"content")));
        expect(fatal(bool(deletion->selection_after.active.container_id
            == deletion->after.root.children.front().id)));
        expect(fatal(bool(deletion->selection_after.active.source_offset == 0u)));
        expect(fatal(bool(deletion->selection_after.active.affinity == TextAffinity::Downstream)));
        expect_document_valid(deletion->after);
    }
};

"block_start_backspace_semantics_do_not_depend_on_visual_affinity"_test = [] {
    auto document = parse_document("> content");
    const auto owner_id = first_editable(document).id;
    auto deletion = test_edit::document_delete_backward(
        document,
        TextSelection::caret({owner_id, 0, TextAffinity::Upstream}));
    expect(fatal(bool(deletion.has_value())));
    if (!deletion) return;
    expect(fatal(bool(deletion->after.root.children.size() == 1u)));
    expect(fatal(bool(deletion->after.root.children.front().kind == BlockKind::Paragraph)));
    expect(fatal(bool(deletion->after.root.children.front().id == owner_id)));
    expect(fatal(bool(deletion->selection_after.active.affinity == TextAffinity::Downstream)));
    expect_document_valid(deletion->after);
};

"block_start_backspace_peels_nested_prefixes_from_inside_out"_test = [] {
    auto document = parse_document("");
    normalize_document(document);
    const auto initial = caret(first_editable(document));
    auto [quoted, quoted_start] = type_text(
        std::move(document), initial, U"> ");
    auto [heading, heading_start] = type_text(
        std::move(quoted), quoted_start, U"# ");
    auto [with_content, after_content] = type_text(
        std::move(heading), heading_start, U"nested");
    const auto content_id = after_content.active.container_id;

    auto remove_heading = test_edit::document_delete_backward(
        with_content,
        TextSelection::caret({content_id, 0, TextAffinity::Downstream}));
    expect(fatal(bool(remove_heading.has_value())));
    if (!remove_heading) return;
    expect(fatal(bool(remove_heading->after.root.children.front().kind == BlockKind::BlockQuote)));
    expect(fatal(bool(remove_heading->after.root.children.front().children.front().kind == BlockKind::Paragraph)));
    expect(fatal(bool(remove_heading->after.root.children.front().children.front().inline_content().source == U"nested")));

    auto remove_quote = test_edit::document_delete_backward(remove_heading->after, remove_heading->selection_after);
    expect(fatal(bool(remove_quote.has_value())));
    if (!remove_quote) return;
    expect(fatal(bool(remove_quote->after.root.children.size() == 1u)));
    expect(fatal(bool(remove_quote->after.root.children.front().kind == BlockKind::Paragraph)));
    expect(fatal(bool(remove_quote->after.root.children.front().id == content_id)));
    expect(fatal(bool(remove_quote->after.root.children.front().inline_content().source == U"nested")));
    expect_document_valid(remove_quote->after);
};

"backspace_unlists_only_the_direct_first_block_and_preserves_siblings"_test = [] {
    auto document = parse_document("- one\n- two\n- three");
    std::vector<NodeId> editable;
    walk_blocks(document.root, [&](const BlockNode& node) {
        if (node.kind == BlockKind::Paragraph) editable.push_back(node.id);
    });
    expect(fatal(bool(editable.size() == 3u)));
    if (editable.size() != 3) return;
    auto deletion = test_edit::document_delete_backward(
        document,
        TextSelection::caret({editable[1], 0, TextAffinity::Downstream}));
    expect(fatal(bool(deletion.has_value())));
    if (!deletion) return;
    expect(fatal(bool(deletion->after.root.children.size() == 3u)));
    expect(fatal(bool(deletion->after.root.children[0].kind == BlockKind::List)));
    expect(fatal(bool(deletion->after.root.children[1].kind == BlockKind::Paragraph)));
    expect(fatal(bool(deletion->after.root.children[1].id == editable[1])));
    expect(fatal(bool(deletion->after.root.children[1].inline_content().source == U"two")));
    expect(fatal(bool(deletion->after.root.children[2].kind == BlockKind::List)));
    expect(fatal(bool(deletion->after.root.children[0].children.size() == 1u)));
    expect(fatal(bool(deletion->after.root.children[2].children.size() == 1u)));
    expect_document_valid(deletion->after);
};

"backspace_outdents_a_nested_list_item_by_one_tree_level"_test = [] {
    auto document = parse_document("- outer\n  - inner");
    const BlockNode* inner = nullptr;
    walk_blocks(document.root, [&](const BlockNode& node) {
        if (node.kind == BlockKind::Paragraph && node.inline_content().source == U"inner") inner = &node;
    });
    expect(fatal(bool(inner != nullptr)));
    if (!inner) return;
    const auto inner_id = inner->id;
    auto deletion = test_edit::document_delete_backward(
        document,
        TextSelection::caret({inner_id, 0, TextAffinity::Downstream}));
    expect(fatal(bool(deletion.has_value())));
    if (!deletion) return;
    const auto& list = deletion->after.root.children.front();
    expect(fatal(bool(list.kind == BlockKind::List)));
    expect(fatal(bool(list.children.size() == 2u)));
    expect(fatal(bool(list.children[0].children.size() == 1u)));
    expect(fatal(bool(list.children[1].children.front().id == inner_id)));
    expect(fatal(bool(list.children[1].children.front().inline_content().source == U"inner")));
    expect_document_valid(deletion->after);
};

};
