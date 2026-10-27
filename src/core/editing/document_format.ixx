// folia.core.document_format — explicit canonical Markdown formatting.
//
// Ordinary save remains lossless. Formatting is an opt-in operation that
// canonicalizes block syntax metadata while preserving every block-local
// inline source and every raw code, math, frontmatter, unsupported, and HTML
// source payload verbatim.
export module folia.core.document_format;
import std;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.dialect;
import folia.markdown.document;
import folia.core.document_transaction;
import folia.markdown.parser;
import folia.markdown.selection;
import folia.markdown.serializer;
import folia.core.source_editor;
import folia.markdown.utf;

export namespace folia {

struct MarkdownFormatOptions {
    bool canonical_headings = true;
    bool canonical_lists = true;
    bool canonical_tables = true;
    bool canonical_containers = true;
    bool canonical_block_spacing = true;
};

struct MarkdownSourceFormatResult {
    std::u32string source;
    SourceSelection selection;
};

namespace document_format_detail {

inline bool needs_local_format(
    BlockNode const& block,
    MarkdownFormatOptions const& options) {
    if (options.canonical_block_spacing && block.separator_before) return true;
    if (block.has_html_source()) return false;
    if (options.canonical_headings && block.kind == BlockKind::Heading) {
        auto const& special = block.text_special();
        if (!special.opening_marker.empty() || !special.closing_marker.empty()) return true;
    }
    if (options.canonical_lists
        && (block.kind == BlockKind::ListItem || block.kind == BlockKind::TaskListItem)
        && !block.item_special().marker.empty()) return true;
    if (options.canonical_lists && block.kind == BlockKind::List
        && block.list_special().ordered && block.list_special().delimiter != U'.') return true;
    if (options.canonical_tables && block.kind == BlockKind::Table) {
        auto const& special = block.table_special();
        if (!special.table_separator_source.empty()
            || !special.table_internal_line_endings.empty()) return true;
    }
    if (options.canonical_tables && block.kind == BlockKind::TableRow
        && block.table_special().row_syntax) return true;
    if (options.canonical_containers
        && (block.kind == BlockKind::Callout || block.kind == BlockKind::FootnoteDefinition)
        && !block.text_special().opening_marker.empty()) return true;
    return false;
}

inline void apply_local_format(
    BlockNode& block,
    MarkdownFormatOptions const& options) {
    if (options.canonical_block_spacing) block.separator_before.reset();
    // A block-level HTML source envelope is independently lossless. Its
    // descendants are semantic presentation slots, not Markdown to reflow.
    if (block.has_html_source()) return;
    if (options.canonical_headings && block.kind == BlockKind::Heading) {
        auto& special = block.ensure_text_special();
        special.opening_marker.clear();
        special.closing_marker.clear();
    }
    if (options.canonical_lists
        && (block.kind == BlockKind::ListItem || block.kind == BlockKind::TaskListItem)) {
        block.ensure_item_special().marker.clear();
        block.ensure_container_source().lines.clear();
    }
    if (options.canonical_lists && block.kind == BlockKind::List
        && block.list_special().ordered) {
        block.ensure_list_special().delimiter = U'.';
    }
    if (options.canonical_tables && block.kind == BlockKind::Table) {
        auto& special = block.ensure_table_special();
        special.table_separator_source.clear();
        special.table_internal_line_endings.clear();
    }
    if (options.canonical_tables && block.kind == BlockKind::TableRow) {
        block.ensure_table_special().row_syntax.reset();
    }
    if (options.canonical_containers
        && (block.kind == BlockKind::Callout || block.kind == BlockKind::FootnoteDefinition)) {
        block.ensure_text_special().opening_marker.clear();
    }
}

inline void canonicalize_tree(
    BlockNode& block,
    MarkdownFormatOptions const& options,
    std::vector<DocumentOperation>* operations) {
    if (needs_local_format(block, options)) {
        DocumentTreeEdit update;
        if (operations) {
            update.kind = DocumentTreeEditKind::UpdatePayload;
            update.before = document_transaction_detail::payload_shell(block);
        }
        apply_local_format(block, options);
        if (operations) {
            update.after = document_transaction_detail::payload_shell(block);
            operations->emplace_back(std::move(update));
        }
    }
    if (block.has_html_source()) return;
    for (auto& child : block.children) canonicalize_tree(child, options, operations);
}

inline std::size_t fallback_mapped_offset(
    std::u32string_view before,
    std::u32string_view after,
    std::size_t offset) {
    offset = (std::min)(offset, before.size());
    std::size_t prefix = 0;
    while (prefix < before.size() && prefix < after.size()
        && before[prefix] == after[prefix]) ++prefix;
    std::size_t suffix = 0;
    while (suffix + prefix < before.size() && suffix + prefix < after.size()
        && before[before.size() - suffix - 1] == after[after.size() - suffix - 1]) ++suffix;
    if (offset <= prefix) return offset;
    if (offset >= before.size() - suffix)
        return after.size() - (before.size() - offset);
    const auto replacement_size = after.size() - prefix - suffix;
    return prefix + (std::min)(offset - prefix, replacement_size);
}

inline std::size_t map_projection_offset(
    SerializedMarkdownProjection const& before,
    SerializedMarkdownProjection const& after,
    std::size_t offset,
    TextAffinity affinity) {
    if (auto position = source_position_for_serialized_offset(before, offset, affinity)) {
        if (auto projected = serialized_offset_for_source_position(after, *position))
            return *projected;
    }
    return fallback_mapped_offset(before.text, after.text, offset);
}

} // namespace document_format_detail

inline EditorDocument canonical_markdown_document(
    EditorDocument document,
    MarkdownFormatOptions const& options = {}) {
    document_format_detail::canonicalize_tree(document.root, options, nullptr);
    return document;
}

inline void canonicalize_markdown_document(
    EditorDocument& document,
    MarkdownFormatOptions const& options = {}) {
    document_format_detail::canonicalize_tree(document.root, options, nullptr);
}

inline std::u32string format_markdown_cps(
    EditorDocument const& document,
    MarkdownFormatOptions const& options = {}) {
    return serialize_markdown_cps(canonical_markdown_document(document, options));
}

inline std::string format_markdown(
    EditorDocument const& document,
    MarkdownFormatOptions const& options = {}) {
    return cps_to_utf8(format_markdown_cps(document, options));
}

inline MarkdownSourceFormatResult format_markdown_source(
    std::u32string_view source,
    SourceSelection selection,
    MarkdownDialect dialect = default_dialect(),
    MarkdownFormatOptions const& options = {}) {
    auto parsed = parse_text(1, cps_to_utf8(source), std::move(dialect));
    auto before = serialize_markdown_projection(parsed.document);
    canonicalize_markdown_document(parsed.document, options);
    auto after = serialize_markdown_projection(parsed.document);
    selection.anchor = document_format_detail::map_projection_offset(
        before, after, selection.anchor, selection.anchor_affinity);
    selection.active = document_format_detail::map_projection_offset(
        before, after, selection.active, selection.active_affinity);
    return {std::move(after.text), selection};
}

inline std::optional<DocumentTransaction> document_format_markdown(
    EditorDocument& document,
    TextSelection const& selection,
    MarkdownFormatOptions const& options = {}) {
    const auto revision_before = document.revision;
    std::vector<DocumentOperation> operations;
    document_format_detail::canonicalize_tree(document.root, options, &operations);
    if (operations.empty()) return std::nullopt;
    ++document.revision;
    return make_recorded_document_transaction(
        std::move(operations),
        selection,
        selection,
        revision_before,
        document.revision,
        DocumentTransactionReason::Format);
}

} // namespace folia
