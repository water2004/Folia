// folia.core.document_footnotes — semantic footnote indexing, navigation, and
// atomic source/tree transactions. No serialized Markdown or UI state is
// consulted when resolving references and definitions.
export module folia.core.document_footnotes;
import std;
import folia.markdown.selection;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.core.document_edit_support;
import folia.core.document_input_rules;
import folia.markdown.document_text;
import folia.core.document_transaction;
import folia.markdown.inline_document;
import folia.markdown.markdown_html;
import folia.markdown.symbols;
import folia.markdown.text_edit;
import folia.markdown.utf;

export namespace folia {

struct FootnoteResolution {
    std::string label;
    std::optional<FootnoteReferenceSymbol> reference;
    std::optional<FootnoteSymbol> definition;
};

inline const FootnoteSymbol* find_footnote_definition(
    const DocumentSymbolIndex& symbols,
    std::string_view label) {
    const auto found = std::ranges::find_if(symbols.footnotes, [&](const auto& footnote) {
        return footnote.label == label;
    });
    return found == symbols.footnotes.end() ? nullptr : &*found;
}

inline const FootnoteReferenceSymbol* find_footnote_reference(
    const DocumentSymbolIndex& symbols,
    std::string_view label) {
    const auto found = std::ranges::find_if(symbols.footnote_references, [&](const auto& reference) {
        return reference.label == label;
    });
    return found == symbols.footnote_references.end() ? nullptr : &*found;
}

inline std::optional<FootnoteResolution> resolve_footnote_reference(
    const DocumentSymbolIndex& symbols,
    TextPosition position) {
    const auto found = std::ranges::find_if(symbols.footnote_references, [&](const auto& reference) {
        return reference.container_id == position.container_id
            && reference.source_range.covers(position.source_offset);
    });
    if (found == symbols.footnote_references.end()) return std::nullopt;
    FootnoteResolution result;
    result.label = found->label;
    result.reference = *found;
    if (const auto* definition = find_footnote_definition(symbols, found->label)) {
        result.definition = *definition;
    }
    return result;
}

inline std::string next_footnote_label(const DocumentSymbolIndex& symbols) {
    std::unordered_set<std::string> used;
    for (const auto& definition : symbols.footnotes) used.insert(definition.label);
    for (const auto& reference : symbols.footnote_references) used.insert(reference.label);
    for (std::uint64_t candidate = 1;; ++candidate) {
        auto label = std::to_string(candidate);
        if (!used.contains(label)) return label;
    }
}

inline std::optional<TextPosition> footnote_definition_target(
    const EditorDocument& document,
    const DocumentSymbolIndex& symbols,
    std::string_view label) {
    const auto* definition = find_footnote_definition(symbols, label);
    if (!definition) return std::nullopt;
    const auto* block = find_document_block(document, definition->node_id);
    return block ? document_edit_detail::first_editable_position(*block) : std::nullopt;
}

inline std::optional<TextPosition> first_footnote_reference_target(
    const DocumentSymbolIndex& symbols,
    std::string_view label) {
    const auto* reference = find_footnote_reference(symbols, label);
    if (!reference) return std::nullopt;
    return TextPosition{
        reference->container_id,
        reference->source_range.start,
        TextAffinity::Downstream,
    };
}

// A trailing unclosed literal block owns all subsequent source. A definition
// placed after it would disappear into that body when the document is opened
// again. Place the new definition before that block, preserving its fences.
inline std::size_t footnote_definition_insertion_index(const EditorDocument& document) {
    const auto size = document.root.children.size();
    if (size == 0) return 0;
    const auto& last = document.root.children.back();
    if (editable_raw_block_source(last)
        && last.block_source.tree().kind != BlockSourceKind::IndentedCode
        && !last.block_source.tree().complete_closing) return size - 1;
    if (last.kind == BlockKind::UnsupportedMarkup
        && markdown_html_missing_block_terminator(utf8_to_cps(last.atomic_special().raw))) return size - 1;
    return size;
}

namespace document_footnotes_detail {

inline std::optional<TextPosition> reference_insertion_position(
    const EditorDocument& document, const TextSelection& selection) {
    auto first_index = document_editable_order_position(document, selection.anchor.container_id);
    auto last_index = document_editable_order_position(document, selection.active.container_id);
    if (!first_index || !last_index) return std::nullopt;
    auto first = selection.anchor;
    auto last = selection.active;
    if (*first_index > *last_index
        || (*first_index == *last_index && first.source_offset > last.source_offset)) {
        std::swap(first_index, last_index);
        std::swap(first, last);
    }
    const auto* block = find_document_block(document, last.container_id);
    const auto visible = block ? editable_block_source_range(*block) : std::nullopt;
    if (!visible) return std::nullopt;
    last.source_offset = (std::clamp)(last.source_offset, visible->start, visible->end);
    if (*last_index > *first_index && last.source_offset == visible->start) {
        // The final owner at offset zero contributes no selected characters.
        // Attach the reference to the preceding selected text or object.
        block = find_document_block(document, document.cached_editable_order[*last_index - 1]);
        const auto previous = block ? editable_block_source_range(*block) : std::nullopt;
        if (!previous) return std::nullopt;
        last = {block->id, previous->end, TextAffinity::Upstream};
    }
    return last;
}

inline BlockNode make_definition(
    const EditorDocument& owner,
    document_edit_detail::NodeAllocator& allocator,
    std::string label) {
    BlockNode definition;
    definition.id = allocator.allocate();
    definition.kind = BlockKind::FootnoteDefinition;
    definition.ensure_container_special().footnote_label = std::move(label);
    definition.ensure_text_special().opening_marker =
        U"[^" + utf8_to_cps(definition.container_special().footnote_label) + U"]: ";
    definition.children.push_back(document_edit_detail::empty_paragraph(allocator, owner));
    return definition;
}

inline void append_preview_text(std::u32string& preview, std::u32string_view text) {
    if (text.empty()) return;
    if (!preview.empty() && preview.back() != U' ') preview.push_back(U' ');
    for (const auto ch : text) preview.push_back(ch == U'\n' ? U' ' : ch);
}

inline void collect_preview(const BlockNode& block, std::u32string& preview) {
    if (const auto* document = editable_inline_document(block)) {
        append_preview_text(preview, inline_visible_text(*document));
    } else if (block.kind == BlockKind::CodeBlock || block.kind == BlockKind::MathBlock) {
        append_preview_text(preview, block_source_content(block.block_source));
    }
    for (const auto& child : block.children) collect_preview(child, preview);
}

} // namespace document_footnotes_detail

inline std::string footnote_preview(
    const EditorDocument& document,
    const DocumentSymbolIndex& symbols,
    std::string_view label,
    std::size_t maximum_codepoints = 100) {
    const auto* definition = find_footnote_definition(symbols, label);
    if (!definition) return {};
    const auto* block = find_document_block(document, definition->node_id);
    if (!block) return {};
    std::u32string preview;
    document_footnotes_detail::collect_preview(*block, preview);
    if (preview.size() > maximum_codepoints) preview.resize(maximum_codepoints);
    return cps_to_utf8(preview);
}

inline std::optional<DocumentTransaction> document_create_footnote_definition(
    EditorDocument& document,
    const DocumentSymbolIndex& symbols,
    const TextSelection& selection,
    std::string label) {
    if (label.empty()) return std::nullopt;
    if (find_footnote_definition(symbols, label)) return std::nullopt;

    const auto revision_before = document.revision;
    auto& after = document;
    document_edit_detail::NodeAllocator allocator(after);
    auto definition = document_footnotes_detail::make_definition(after, allocator, std::move(label));
    const auto target = document_edit_detail::first_editable_position(definition);
    if (!target) return std::nullopt;

    DocumentTreeEdit insert;
    insert.kind = DocumentTreeEditKind::Insert;
    insert.parent_id = after.root.id;
    insert.index = footnote_definition_insertion_index(after);
    insert.after = definition;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(after, operations, revision_before);
    const auto insertion_index = insert.index;
    operations.emplace_back(std::move(insert));
    after.root.children.insert(after.root.children.begin() + static_cast<std::ptrdiff_t>(insertion_index),
        std::move(definition));
    ++after.revision;
    rollback.commit();
    return make_recorded_document_transaction(
        std::move(operations),
        selection,
        TextSelection::caret(*target),
        revision_before,
        after.revision,
        DocumentTransactionReason::Structure);
}

inline std::optional<DocumentTransaction> document_insert_footnote(
    EditorDocument& document,
    const DocumentSymbolIndex& symbols,
    const TextSelection& selection) {
    auto position = document_footnotes_detail::reference_insertion_position(document, selection);
    if (!position) return std::nullopt;
    const auto revision_before = document.revision;
    auto& after = document;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(after, operations, revision_before);
    document_edit_detail::NodeAllocator allocator(after);
    if (auto boundary = document_input_rules::ensure_atomic_boundary_paragraph(after, *position, allocator)) {
        position = boundary->target;
        operations.insert(operations.end(), std::make_move_iterator(boundary->operations.begin()),
            std::make_move_iterator(boundary->operations.end()));
    }
    const auto label = next_footnote_label(symbols);
    const auto reference_source = U"[^" + utf8_to_cps(label) + U"]";
    auto inserted = document_edit_detail::insert_text(after, *position, reference_source, allocator);
    if (!inserted || !inserted->source_edit) return std::nullopt;
    document_edit_detail::append_source_operation(operations, std::move(*inserted->source_edit));

    auto definition = document_footnotes_detail::make_definition(after, allocator, label);
    const auto target = document_edit_detail::first_editable_position(definition);
    if (!target) return std::nullopt;
    DocumentTreeEdit insert;
    insert.kind = DocumentTreeEditKind::Insert;
    insert.parent_id = after.root.id;
    insert.index = footnote_definition_insertion_index(after);
    insert.after = definition;
    const auto insertion_index = insert.index;
    operations.emplace_back(std::move(insert));
    after.root.children.insert(after.root.children.begin() + static_cast<std::ptrdiff_t>(insertion_index),
        std::move(definition));
    ++after.revision;
    rollback.commit();
    return make_recorded_document_transaction(
        std::move(operations),
        selection,
        TextSelection::caret(*target),
        revision_before,
        after.revision,
        DocumentTransactionReason::Structure);
}

} // namespace folia
