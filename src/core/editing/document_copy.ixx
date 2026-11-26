// folia.core.document_copy — lossless Markdown extraction from a structural selection.
export module folia.core.document_copy;
import std;
import folia.markdown.dialect;
import folia.markdown.ids;
import folia.markdown.inline_document;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_parser;
import folia.markdown.serializer;
import folia.core.table_selection;
import folia.core.theme;
import folia.markdown.text_edit;
import folia.markdown.table_metadata;
import folia.markdown.utf;

export namespace folia {

namespace document_copy_detail {

struct OrderedSelection {
    const EditorDocument* document = nullptr;
    TextPosition start;
    TextPosition end;
    std::size_t start_index = 0;
    std::size_t end_index = 0;
};

inline std::optional<std::u32string_view> owner_text(
    const OrderedSelection& selection,
    std::size_t index) {
    if (!selection.document
        || index >= selection.document->cached_editable_order.size()) {
        return std::nullopt;
    }
    const auto* block = find_document_block(
        *selection.document,
        selection.document->cached_editable_order[index]);
    return block ? editable_block_text_view(*block) : std::nullopt;
}

inline std::optional<OrderedSelection> order_selection(
    const EditorDocument& document,
    const TextSelection& selection) {
    OrderedSelection ordered;
    ordered.document = &document;
    const auto anchor = document_editable_order_position(
        document, selection.anchor.container_id);
    const auto active = document_editable_order_position(
        document, selection.active.container_id);
    if (!anchor || !active) return std::nullopt;
    ordered.start = selection.anchor;
    ordered.end = selection.active;
    ordered.start_index = *anchor;
    ordered.end_index = *active;
    if (ordered.end_index < ordered.start_index
        || (ordered.start_index == ordered.end_index
            && ordered.end.source_offset < ordered.start.source_offset)) {
        std::swap(ordered.start, ordered.end);
        std::swap(ordered.start_index, ordered.end_index);
    }
    const auto start_text = owner_text(ordered, ordered.start_index);
    const auto end_text = owner_text(ordered, ordered.end_index);
    if (!start_text || !end_text) return std::nullopt;
    ordered.start.source_offset = (std::min)(
        ordered.start.source_offset, start_text->size());
    ordered.end.source_offset = (std::min)(
        ordered.end.source_offset, end_text->size());
    return ordered;
}

inline SourceRange selected_range(
    const OrderedSelection& selection,
    std::size_t index) {
    if (index < selection.start_index || index > selection.end_index) return {};
    const auto text = owner_text(selection, index);
    if (!text) return {};
    const auto length = text->size();
    const auto start = index == selection.start_index
        ? selection.start.source_offset
        : 0;
    const auto end = index == selection.end_index
        ? selection.end.source_offset
        : length;
    return {(std::min)(start, length), (std::min)(end, length)};
}

inline std::optional<std::u32string> selected_owner_source(
    const BlockNode& owner, SourceRange range) {
    const auto text = editable_block_text_view(owner);
    if (!text || !range.valid_for(text->size())) return std::nullopt;
    if (range.empty()) return std::u32string{};
    if (!editable_inline_document(owner) && !editable_raw_block_source(owner)) {
        // Atomic text projection uses U+FFFC for navigation. Clipboard data
        // must carry the object's actual Markdown, never that placeholder.
        BlockVec fragment;
        fragment.push_back(owner);
        fragment.front().separator_before.reset();
        return serialize_markdown_fragment(fragment);
    }
    return std::u32string(text->substr(range.start, range.length()));
}

inline std::optional<std::pair<std::size_t, std::size_t>> descendant_fragment_span(
    const BlockNode& block,
    const OrderedSelection& selection) {
    if (!selection.document) return std::nullopt;
    const auto first = document_editable_order_position(
        *selection.document, first_editable_subtree_owner(block));
    const auto last = document_editable_order_position(
        *selection.document, last_editable_subtree_owner(block));
    if (!first || !last) return std::nullopt;
    return std::pair{*first, *last};
}

inline bool intersects_selection(
    const std::pair<std::size_t, std::size_t>& span,
    const OrderedSelection& selection) {
    return span.first <= selection.end_index && selection.start_index <= span.second;
}

inline void truncate_owner(
    BlockNode& copy,
    SourceRange range,
    std::size_t full_length,
    const MarkdownDialect& dialect,
    std::uint64_t& next_copy_id) {
    if (auto* document = editable_inline_document(copy)) {
        document->source = document->source.substr(range.start, range.length());
        // A partial heading selection does not include its structural marker:
        // copied text therefore becomes a paragraph. A fully selected heading
        // retains the heading block and its exact original marker spelling.
        if (copy.kind == BlockKind::Heading
            && (range.start != 0 || range.end != full_length)) {
            copy.kind = BlockKind::Paragraph;
            copy.ensure_text_special().level = 0;
            copy.ensure_text_special().opening_marker.clear();
            copy.ensure_text_special().closing_marker.clear();
        }
        InlineParseContext context;
        context.dialect = dialect;
        context.allocate_id = [&next_copy_id] { return NodeId{next_copy_id++}; };
        reparse_inline_document(*document, context);
        return;
    }
    if (auto* source = editable_raw_block_source(copy)) {
        const auto editable = block_source_editable_range(copy.block_source);
        if (range.start == 0 && range.end == full_length) {
            reparse_block_source(copy.block_source);
            return;
        }
        const auto selected_start = (std::clamp)(range.start, editable.start, editable.end);
        const auto selected_end = (std::clamp)(range.end, editable.start, editable.end);
        auto selected = source->substr(
            selected_start,
            selected_end - selected_start);
        if (copy.block_source.tree().kind == BlockSourceKind::IndentedCode) {
            // A partial first line no longer owns the original indentation.
            // Give the copied fragment its own structural prefix; subsequent
            // selected lines already retain their source indentation.
            *source = U"    " + std::move(selected);
        } else {
            auto wrapped = source->substr(0, editable.start);
            wrapped += selected;
            wrapped += source->substr(editable.end);
            *source = std::move(wrapped);
        }
        reparse_block_source(copy.block_source);
    }
}

inline std::optional<BlockNode> slice_block(
    const BlockNode& block,
    const OrderedSelection& selection,
    std::uint64_t& next_copy_id,
    std::unordered_set<std::uint64_t>& boundary_placeholders) {
    const auto span = descendant_fragment_span(block, selection);
    if (!span || !intersects_selection(*span, selection)) return std::nullopt;

    // Clone only this node before visiting selected children. Deep-copying a
    // container and clearing its children first duplicates an entire large
    // list even when the selection contains only two of its leaves.
    BlockNode copy;
    copy.id = block.id;
    copy.kind = block.kind;
    if (block.has_inline_content()) copy.edit_inline_content() = block.inline_content();
    copy.block_source = block.block_source;
    copy.separator_before = block.separator_before;
    copy.payload = block.payload ? std::make_unique<BlockNodeSpecial>(*block.payload) : nullptr;
    bool has_selected_owner = false;
    const auto self = selection.document
        ? document_editable_order_position(*selection.document, block.id)
        : std::nullopt;
    if (self && *self >= selection.start_index && *self <= selection.end_index) {
        const auto range = selected_range(selection, *self);
        const auto text = owner_text(selection, *self);
        if (!text) return std::nullopt;
        const auto endpoint = *self == selection.start_index || *self == selection.end_index;
        if (range.empty() && endpoint) {
            // The interval still crosses this owner's following/preceding
            // boundary. Keep a source-empty placeholder, not the unselected
            // image/fence/heading syntax, so copying only a newline works.
            copy = BlockNode{};
            copy.id = block.id;
            copy.kind = BlockKind::Paragraph;
            copy.separator_before = block.separator_before;
            if (block.kind != BlockKind::Paragraph || !text->empty()) {
                boundary_placeholders.insert(block.id.v);
            }
            has_selected_owner = true;
        } else if (!range.empty() || text->empty()) {
            has_selected_owner = true;
            truncate_owner(
                copy,
                range,
                text->size(),
                selection.document->dialect,
                next_copy_id);
        }
    }

    for (const auto& child : block.children) {
        if (auto selected = slice_block(child, selection, next_copy_id, boundary_placeholders)) {
            copy.children.push_back(std::move(*selected));
        }
    }

    if (!has_selected_owner && copy.children.empty()) return std::nullopt;
    return copy;
}

inline std::optional<BlockNode> slice_block(
    const BlockNode& block,
    const OrderedSelection& selection,
    std::uint64_t& next_copy_id) {
    std::unordered_set<std::uint64_t> boundary_placeholders;
    return slice_block(block, selection, next_copy_id, boundary_placeholders);
}

} // namespace document_copy_detail

struct TableClipboard {
    std::u32string markdown;
    std::u32string html;
    std::u32string tab_separated;
};

std::optional<TableClipboard> document_table_clipboard(
    const EditorDocument& document, TextPosition position,
    std::optional<TableRegion> selected = {}, bool markdown_only = false, const ThemeColors* colors = nullptr);
std::optional<std::u32string> table_clipboard_markdown(std::u32string_view html);
std::optional<std::u32string> table_clipboard_html_table(std::u32string_view html);

// Return the exact source substring for a one-owner selection. Cross-owner
// selections are represented by a recursively pruned block tree, preserving
// list/quote/task/callout/footnote structure at arbitrary nesting depth.
inline std::optional<std::u32string> document_selected_markdown(
    const EditorDocument& document,
    const TextSelection& selection) {
    if (auto cells = document_table_cell_selection(document, selection)) {
        if (auto clipboard = document_table_clipboard(document, selection.active, cells->region))
            return std::move(clipboard->tab_separated);
        return std::nullopt;
    }
    if (auto merged = document_selected_merged_table_text(document, selection)) return merged;
    auto ordered = document_copy_detail::order_selection(document, selection);
    if (!ordered) return std::nullopt;
    if (ordered->start_index == ordered->end_index) {
        const auto range = document_copy_detail::selected_range(
            *ordered,
            ordered->start_index);
        const auto* owner = find_document_block(document, ordered->start.container_id);
        return owner ? document_copy_detail::selected_owner_source(*owner, range)
            : std::nullopt;
    }

    // Any range that crosses a table is an ordinary row-major text range, not
    // an implicit request to copy the whole table. Tabs preserve cell
    // boundaries and newlines preserve row/block boundaries without
    // manufacturing a second table-selection model.
    if (auto slices = document_table_barrier_text_slices(document, selection);
        slices && slices->size() > 1) {
        std::u32string result;
        for (std::size_t index = 0; index < slices->size(); ++index) {
            auto const& slice = (*slices)[index];
            if (index > 0) {
                auto const previous = document_table_cell_address(
                    document, (*slices)[index - 1].container_id);
                auto const current = document_table_cell_address(
                    document, slice.container_id);
                result.push_back(previous && current
                        && previous->table_id == current->table_id
                        && previous->row == current->row
                    ? U'\t' : U'\n');
            }
            auto const* owner = find_document_block(document, slice.container_id);
            auto source = owner ? document_copy_detail::selected_owner_source(
                *owner, slice.source_range) : std::nullopt;
            if (!source) return std::nullopt;
            result += *source;
        }
        return result;
    }

    BlockVec selected;
    selected.reserve(document.root.children.size());
    auto next_copy_id = document.next_node_id;
    std::unordered_set<std::uint64_t> boundary_placeholders;
    for (const auto& block : document.root.children) {
        if (auto copy = document_copy_detail::slice_block(
                block,
                *ordered,
                next_copy_id,
                boundary_placeholders)) {
            selected.push_back(std::move(*copy));
        }
    }
    // Parser-owned separator metadata can split a physical blank line between
    // the preceding block's trailing range and the following block. Once a
    // boundary block is truncated that ownership is no longer valid. Let the
    // fragment serializer choose an unambiguous block separator for every
    // selected top-level sibling instead of accidentally turning the next
    // paragraph into a lazy list continuation.
    for (std::size_t index = 0; index < selected.size(); ++index) {
        auto& block = selected[index];
        const auto original_separator = block.separator_before;
        block.separator_before.reset();
        if (index > 0 && boundary_placeholders.contains(selected[index - 1].id.v)) {
            // An emptied boundary node was not originally a blank paragraph.
            // Its following separator must not shrink to the blank-paragraph
            // default solely because the selected content is empty.
            block.separator_before = original_separator.value_or(U"\n\n");
        }
    }
    return serialize_markdown_fragment(selected);
}

} // namespace folia
