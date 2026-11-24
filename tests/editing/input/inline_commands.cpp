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

suite editing_input_inline_commands_tests = [] {

"backspace_on_a_complete_format_marker_removes_its_pair"_test = [] {
    auto document = parse_document("**alpha**");
    const auto& paragraph = first_editable(document);
    auto transaction = test_edit::document_delete_backward(document, caret(paragraph, 1));
    expect(fatal(bool(transaction.has_value())));
    if (!transaction) return;
    expect(fatal(bool(first_editable(transaction->after).inline_content.source == U"alpha")));
    expect(fatal(bool(transaction->selection_after.active.source_offset == 0u)));
    expect_document_valid(transaction->after);
};

"delete_forward_preserves_unclosed_source_verbatim"_test = [] {
    auto document = parse_document("[title](url)");
    const auto& paragraph = first_editable(document);
    auto transaction = test_edit::document_delete_forward(document, caret(paragraph, 11));
    expect(fatal(bool(transaction.has_value())));
    if (!transaction) return;
    expect(fatal(bool(first_editable(transaction->after).inline_content.source == U"[title](url")));
    expect_document_valid(transaction->after);
};

"format_commands_wrap_and_unwrap_exact_source"_test = [] {
    struct Case { InlineFormat format; std::u32string expected; InlineCstKind kind; };
    const std::vector<Case> cases{
        {InlineFormat::Emphasis, U"*value*", InlineCstKind::Emphasis},
        {InlineFormat::Strong, U"**value**", InlineCstKind::Strong},
        {InlineFormat::Strikethrough, U"~~value~~", InlineCstKind::Strikethrough},
        {InlineFormat::Code, U"`value`", InlineCstKind::CodeSpan},
        {InlineFormat::Math, U"$value$", InlineCstKind::InlineMath},
    };
    for (const auto& test : cases) {
        auto document = parse_document("value");
        const auto& paragraph = first_editable(document);
        auto wrapped = test_edit::document_toggle_inline_format(document, range(paragraph, 0, 5), test.format);
        expect(fatal(bool(wrapped.has_value())));
        if (!wrapped) continue;
        const auto& inline_document = first_editable(wrapped->after).inline_content;
        expect(fatal(bool(inline_document.source == test.expected)));
        expect(fatal(bool(inline_contains_kind(inline_document, test.kind))));
        auto unwrapped = test_edit::document_toggle_inline_format(wrapped->after, wrapped->selection_after, test.format);
        expect(fatal(bool(unwrapped.has_value())));
        if (unwrapped) expect(fatal(bool(first_editable(unwrapped->after).inline_content.source == U"value")));
    }
};

"link_and_image_commands_are_source_edits"_test = [] {
    auto link_document = parse_document("title");
    const auto& link_paragraph = first_editable(link_document);
    auto link = test_edit::document_insert_link(link_document, range(link_paragraph, 0, 5), "url", "name");
    expect(fatal(bool(link.has_value())));
    if (link) {
        const auto& inline_document = first_editable(link->after).inline_content;
        expect(fatal(bool(inline_document.source == U"[title](url \"name\")")));
        expect(fatal(bool(inline_contains_kind(inline_document, InlineCstKind::Link))));
    }
    auto image_document = parse_document("diagram");
    const auto& image_paragraph = first_editable(image_document);
    auto image = test_edit::document_insert_image(image_document, range(image_paragraph, 0, 7), "image.png", "diagram");
    expect(fatal(bool(image.has_value())));
    if (image) {
        const auto& inline_document = first_editable(image->after).inline_content;
        expect(fatal(bool(inline_document.source == U"![diagram](image.png)")));
        expect(fatal(bool(inline_contains_kind(inline_document, InlineCstKind::Image))));
    }
};

"footnote_commands_resolve_labels_and_record_source_plus_tree_edits"_test = [] {
    auto document = parse_document("body[^1]\n\n[^3]: existing");
    const auto& owner = first_editable(document);
    const auto before = range(owner, 0, 4);
    auto inserted = test_edit::document_insert_footnote(document, before);
    expect(fatal(bool(inserted.has_value())));
    if (!inserted) return;

    expect(fatal(bool(inserted->operations.size() == 2u)));
    expect(fatal(bool(std::holds_alternative<DocumentTextOperation>(inserted->operations[0]))));
    expect(fatal(bool(std::holds_alternative<DocumentTreeEdit>(inserted->operations[1]))));
    expect(fatal(bool(inserted->after.root.children.size() == 3u)));
    expect(fatal(bool(inserted->after.root.children[0].inline_content.source == U"body[^2][^1]")));
    const auto& definition = inserted->after.root.children.back();
    expect(fatal(bool(definition.kind == BlockKind::FootnoteDefinition)));
    expect(fatal(bool(definition.container_special().footnote_label == "2")));
    expect(fatal(bool(definition.children.size() == 1u)));
    expect(fatal(bool(inserted->selection_after.active.container_id == definition.children.front().id)));
    expect(fatal(bool(inserted->selection_after.active.source_offset == 0u)));
    expect(fatal(bool(serialize_markdown(inserted->after) ==
        "body[^2][^1]\n\n[^3]: existing\n\n[^2]: ")));

    const auto symbols = build_document_symbol_index(inserted->after);
    expect(fatal(bool(symbols.footnotes.size() == 2u)));
    expect(fatal(bool(symbols.footnote_references.size() == 2u)));
    const auto resolution = resolve_footnote_reference(
        symbols,
        {owner.id, 6, TextAffinity::Downstream});
    expect(fatal(bool(resolution.has_value())));
    if (resolution) {
        expect(fatal(bool(resolution->label == "2")));
        expect(fatal(bool(resolution->definition.has_value())));
    }
    expect_document_valid(inserted->after);
};

"missing_footnote_definitions_are_created_semantically_without_duplicates"_test = [] {
    auto document = parse_document("body[^missing]");
    const auto& owner = first_editable(document);
    const auto before = caret(owner, owner.inline_content.source.size());
    auto created = test_edit::document_create_footnote_definition(document, before, "missing");
    expect(fatal(bool(created.has_value())));
    if (!created) return;
    expect(fatal(bool(created->operations.size() == 1u)));
    expect(fatal(bool(created->after.root.children.size() == 2u)));
    expect(fatal(bool(created->after.root.children.back().kind == BlockKind::FootnoteDefinition)));
    expect(fatal(bool(created->after.root.children.back().container_special().footnote_label == "missing")));
    const auto symbols = build_document_symbol_index(created->after);
    expect(fatal(bool(footnote_definition_target(
        created->after, symbols, "missing").has_value())));
    const auto reference = first_footnote_reference_target(
        symbols, "missing");
    expect(fatal(bool(reference.has_value())));
    if (reference) {
        expect(fatal(bool(reference->container_id == owner.id)));
        expect(fatal(bool(reference->source_offset == 4u)));
    }
    expect(fatal(bool(!test_edit::document_create_footnote_definition(
        created->after, created->selection_after, "missing").has_value())));
    expect_document_valid(created->after);
};

};
