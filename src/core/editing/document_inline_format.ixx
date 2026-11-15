// CST-driven inline formatting over selected block-local authoritative sources.
//
// Formatting never infers an existing construct by comparing marker text.
// It locates the matching lossless CST node, preserves that node's exact
// opening/closing spelling, and records local source TextEdits in one transaction.
export module folia.core.document_inline_format;
import std;
import folia.markdown.ids;
import folia.markdown.selection;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.core.document_edit_primitives;
import folia.core.document_block_edit;
import folia.core.document_edit_validation;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.core.inline_source_edit;
import folia.markdown.text_edit;

export namespace folia {

namespace document_inline_format_detail {

inline InlineCstKind cst_kind(InlineFormat format) {
    switch (format) {
        case InlineFormat::Emphasis: return InlineCstKind::Emphasis;
        case InlineFormat::Strong: return InlineCstKind::Strong;
        case InlineFormat::Strikethrough: return InlineCstKind::Strikethrough;
        case InlineFormat::Code: return InlineCstKind::CodeSpan;
        case InlineFormat::Math: return InlineCstKind::InlineMath;
    }
    return InlineCstKind::Error;
}

inline std::optional<std::pair<std::u32string, std::u32string>> default_markers(
    InlineFormat format,
    InlineSyntaxMode syntax_mode) {
    if (syntax_mode == InlineSyntaxMode::HtmlText) {
        switch (format) {
            case InlineFormat::Emphasis: return std::pair{std::u32string{U"<em>"}, std::u32string{U"</em>"}};
            case InlineFormat::Strong: return std::pair{std::u32string{U"<strong>"}, std::u32string{U"</strong>"}};
            case InlineFormat::Strikethrough: return std::pair{std::u32string{U"<del>"}, std::u32string{U"</del>"}};
            case InlineFormat::Code: return std::pair{std::u32string{U"<code>"}, std::u32string{U"</code>"}};
            case InlineFormat::Math: return std::nullopt;
        }
    }
    switch (format) {
        case InlineFormat::Emphasis: return std::pair{std::u32string{U"*"}, std::u32string{U"*"}};
        case InlineFormat::Strong: return std::pair{std::u32string{U"**"}, std::u32string{U"**"}};
        case InlineFormat::Strikethrough: return std::pair{std::u32string{U"~~"}, std::u32string{U"~~"}};
        case InlineFormat::Code: return std::pair{std::u32string{U"`"}, std::u32string{U"`"}};
        case InlineFormat::Math: return std::pair{std::u32string{U"$"}, std::u32string{U"$"}};
    }
    return {};
}

struct FormatNodeRanges {
    SourceRange full;
    SourceRange opening;
    SourceRange content;
    SourceRange closing;
    InlineCstKind kind = InlineCstKind::Error;
};

inline bool contains_selection(
    const InlineCstNode& node,
    std::size_t start,
    std::size_t end,
    bool caret) {
    const auto& delim = node.delimiter_ranges();
    if (node.status != ParseStatus::Complete || !delim.closing) return false;
    if (!caret && start == node.range.start && end == node.range.end) return true;
    if (caret) return start >= delim.content.start && start <= delim.content.end;
    return start >= delim.content.start && end <= delim.content.end;
}

inline bool matches_format(const InlineCstNode& node, InlineCstKind kind) {
    if (node.kind == kind) return true;
    if (node.kind != InlineCstKind::HtmlElement) return false;
    // Safe HTML remains an HtmlElement in the lossless CST. Its semantic
    // wrappers have the same formatting meaning as their Markdown peers,
    // including the aliases used by HTML presentation. Keep the original
    // element kind/ranges so removal preserves attributes and never treats
    // HTML code content as Markdown code-span padding.
    const auto& tag = node.semantics().html_tag;
    switch (kind) {
        case InlineCstKind::Strong: return tag == "strong" || tag == "b";
        case InlineCstKind::Emphasis: return tag == "em" || tag == "i" || tag == "cite" || tag == "var";
        case InlineCstKind::Strikethrough: return tag == "del" || tag == "s" || tag == "strike";
        case InlineCstKind::CodeSpan: return tag == "code" || tag == "kbd" || tag == "samp" || tag == "tt";
        default: return false;
    }
}

inline std::optional<FormatNodeRanges> find_innermost_format(
    const InlineCstNodes& nodes,
    InlineCstKind kind,
    std::size_t start,
    std::size_t end,
    bool caret) {
    for (const auto& node : nodes) {
        if (auto nested = find_innermost_format(node.children, kind, start, end, caret)) {
            return nested;
        }
        if (matches_format(node, kind) && contains_selection(node, start, end, caret)) {
            return FormatNodeRanges{
                node.range,
                node.delimiter_ranges().opening,
                node.delimiter_ranges().content,
                *node.delimiter_ranges().closing,
                node.kind,
            };
        }
    }
    return std::nullopt;
}

inline TextSelection directed_selection(
    const TextSelection& original,
    NodeId owner,
    std::size_t start,
    std::size_t end) {
    if (original.anchor.source_offset <= original.active.source_offset) {
        return {
            {owner, start, original.anchor.affinity},
            {owner, end, original.active.affinity},
        };
    }
    return {
        {owner, end, original.anchor.affinity},
        {owner, start, original.active.affinity},
    };
}

struct FormatSourceChange {
    SourceRange range;
    std::u32string replacement;
    TextSelection selection_after;
};

inline bool code_space(char32_t value) {
    // Code-span rendering normalizes physical line endings to spaces before
    // deciding whether its one surrounding pair of spaces is padding.
    return value == U' ' || value == U'\r' || value == U'\n';
}

inline bool all_code_spaces(std::u32string_view content) {
    return std::ranges::all_of(content, code_space);
}

inline std::pair<std::u32string, std::u32string> code_markers(
    std::u32string_view content, std::size_t minimum_width = 1) {
    std::size_t longest = 0, run = 0;
    for (auto value : content) {
        run = value == U'`' ? run + 1 : 0;
        longest = (std::max)(longest, run);
    }
    std::u32string opening((std::max)(minimum_width, longest + 1), U'`');
    auto closing = opening;
    const auto pad = !content.empty() && (content.front() == U'`' || content.back() == U'`'
        || (code_space(content.front()) && code_space(content.back()) && !all_code_spaces(content)));
    if (pad) { opening += U' '; closing.insert(closing.begin(), U' '); }
    return {std::move(opening), std::move(closing)};
}

inline FormatNodeRanges without_code_padding(const InlineDocument& document, FormatNodeRanges format) {
    if (format.kind != InlineCstKind::CodeSpan || document.syntax_mode == InlineSyntaxMode::HtmlText
        || format.content.length() < 2) return format;
    const auto content = std::u32string_view(document.source).substr(format.content.start, format.content.length());
    if (content.front() == U' ' && content.back() == U' ' && !all_code_spaces(content)) {
        ++format.opening.end;
        ++format.content.start;
        --format.content.end;
        --format.closing.start;
    }
    return format;
}

inline FormatSourceChange remove_existing_format(
    NodeId owner,
    const InlineDocument& document,
    const TextSelection& selection,
    SourceRange selected,
    const FormatNodeRanges& raw_format) {
    const auto format = without_code_padding(document, raw_format);
    const auto content = inline_source_slice(document, format.content);

    if (selection.is_caret()) {
        const auto offset = format.full.start
            + ((std::clamp)(selection.active.source_offset, format.content.start, format.content.end)
                - format.content.start);
        return {
            format.full,
            content,
            TextSelection::caret({owner, offset, selection.active.affinity}),
        };
    }

    const auto selected_full_node = selected == format.full;
    const auto selected_full_content = selected.start <= format.content.start && selected.end >= format.content.end;
    if (selected_full_node || selected_full_content) {
        return {
            format.full,
            content,
            directed_selection(
                selection,
                owner,
                format.full.start,
                format.full.start + content.size()),
        };
    }

    selected.start = (std::clamp)(selected.start, format.content.start, format.content.end);
    selected.end = (std::clamp)(selected.end, format.content.start, format.content.end);
    const auto wrap_fragment = [&](std::u32string_view fragment) {
        auto opening = inline_source_slice(document, format.opening);
        auto closing = inline_source_slice(document, format.closing);
        if (format.kind == InlineCstKind::CodeSpan && document.syntax_mode != InlineSyntaxMode::HtmlText)
            std::tie(opening, closing) = code_markers(fragment, raw_format.opening.length());
        return opening + std::u32string(fragment) + closing;
    };
    const auto prefix = document.source.substr(
        format.content.start,
        selected.start - format.content.start);
    const auto unformatted = document.source.substr(selected.start, selected.length());
    const auto suffix = document.source.substr(
        selected.end,
        format.content.end - selected.end);

    std::u32string replacement;
    if (!prefix.empty()) replacement += wrap_fragment(prefix);
    const auto selection_start = format.full.start + replacement.size();
    replacement += unformatted;
    const auto selection_end = format.full.start + replacement.size();
    if (!suffix.empty()) replacement += wrap_fragment(suffix);

    return {
        format.full,
        std::move(replacement),
        directed_selection(selection, owner, selection_start, selection_end),
    };
}

inline FormatSourceChange add_format(
    NodeId owner,
    const InlineDocument& document,
    const TextSelection& selection,
    SourceRange selected,
    const std::pair<std::u32string, std::u32string>& markers) {
    auto [opening, closing] = markers;
    const auto content = document.source.substr(selected.start, selected.length());
    if (opening == U"`" && closing == U"`") {
        // Code-span parsing removes one surrounding space when both exist.
        // Pad intentionally so selected spaces/backticks remain content.
        std::tie(opening, closing) = code_markers(content);
    }
    auto replacement = opening + content + closing;
    if (selection.is_caret()) {
        return {
            selected,
            std::move(replacement),
            TextSelection::caret({
                owner,
                selected.start + opening.size(),
                TextAffinity::Downstream,
            }),
        };
    }
    return {
        selected,
        std::move(replacement),
        directed_selection(
            selection,
            owner,
            selected.start + opening.size(),
            selected.end + opening.size()),
    };
}

inline std::optional<FormatNodeRanges> find_boundary_format(
    const InlineCstNodes& nodes,
    InlineCstKind kind,
    SourceRange selected) {
    for (const auto& node : nodes) {
        if (auto nested = find_boundary_format(node.children, kind, selected)) return nested;
        const auto& ranges = node.delimiter_ranges();
        if (!matches_format(node, kind) || node.status != ParseStatus::Complete || !ranges.closing) continue;
        // A cross-owner range includes the opening/closing delimiter at its
        // implicit owner edges. Those markers do not make the selected body
        // unformatted. Keep the single-owner/source-mode detector unchanged.
        if (selected.start >= node.range.start && selected.end <= node.range.end
            && selected.start < ranges.content.end && selected.end > ranges.content.start) {
            return FormatNodeRanges{node.range, ranges.opening, ranges.content, *ranges.closing, node.kind};
        }
    }
    return std::nullopt;
}

struct SelectedInlineOwner {
    NodeId id{};
    const InlineDocument* document = nullptr;
    SourceRange range;
    TextSelection selection;
    std::optional<FormatNodeRanges> existing;
};

inline std::optional<std::vector<SelectedInlineOwner>> selected_inline_owners(
    const EditorDocument& document,
    const TextSelection& selection,
    InlineFormat format) {
    const auto anchor_index = document_editable_order_position(document, selection.anchor.container_id);
    const auto active_index = document_editable_order_position(document, selection.active.container_id);
    if (!anchor_index || !active_index) return std::nullopt;
    const auto* anchor = find_document_block(document, selection.anchor.container_id);
    const auto* active = find_document_block(document, selection.active.container_id);
    const auto anchor_text = anchor ? editable_block_text_view(*anchor) : std::nullopt;
    const auto active_text = active ? editable_block_text_view(*active) : std::nullopt;
    if (!anchor_text || !active_text || selection.anchor.source_offset > anchor_text->size()
        || selection.active.source_offset > active_text->size()) return std::nullopt;

    const auto reversed = *anchor_index > *active_index
        || (*anchor_index == *active_index && selection.anchor.source_offset > selection.active.source_offset);
    const auto& first = reversed ? selection.active : selection.anchor;
    const auto& last = reversed ? selection.anchor : selection.active;
    const auto first_index = (std::min)(*anchor_index, *active_index);
    const auto last_index = (std::max)(*anchor_index, *active_index);
    const auto cross_owner = first_index != last_index;
    std::vector<SelectedInlineOwner> result;
    for (auto index = first_index; index <= last_index; ++index) {
        const auto id = document.cached_editable_order[index];
        const auto* block = find_document_block(document, id);
        const auto* owner = block ? editable_inline_document(*block) : nullptr;
        // Fences and atomic objects remain source islands. Empty boundary
        // ranges do not acquire a pair of markers merely because the range
        // ends at the beginning of the following paragraph or table cell.
        if (!owner) continue;
        const auto start = index == first_index ? first.source_offset : 0;
        const auto end = index == last_index ? last.source_offset : owner->source.size();
        if (start == end && !selection.is_caret()) continue;
        const auto range = SourceRange{start, end};
        if (!range.valid_for(owner->source.size())) return std::nullopt;
        auto local = TextSelection{
            {id, reversed ? end : start, selection.anchor.affinity},
            {id, reversed ? start : end, selection.active.affinity},
        };
        auto existing = find_innermost_format(owner->tree.nodes, cst_kind(format),
            start, end, selection.is_caret());
        if (!existing && cross_owner) existing = find_boundary_format(owner->tree.nodes, cst_kind(format), range);
        result.push_back({id, owner, range, local, existing});
    }
    return result;
}

} // namespace document_inline_format_detail

inline bool document_inline_format_active(
    const EditorDocument& document, const TextSelection& selection, InlineFormat format) {
    const auto selected = document_inline_format_detail::selected_inline_owners(document, selection, format);
    return selected && !selected->empty() && std::ranges::all_of(*selected,
        [](const auto& owner) { return owner.existing.has_value(); });
}

inline std::optional<DocumentTransaction> document_toggle_inline_format(
    EditorDocument& document,
    const TextSelection& selection,
    InlineFormat format) {
    const auto selected = document_inline_format_detail::selected_inline_owners(document, selection, format);
    if (!selected || selected->empty()) return std::nullopt;
    const auto remove = std::ranges::all_of(*selected,
        [](const auto& owner) { return owner.existing.has_value(); });
    std::vector<std::pair<NodeId, document_inline_format_detail::FormatSourceChange>> changes;
    changes.reserve(selected->size());
    // Plan every selected owner before mutation so an unsupported syntax
    // combination cannot leave a half-formatted selection or consume ids.
    for (const auto& owner : *selected) {
        if (remove) {
            changes.emplace_back(owner.id, document_inline_format_detail::remove_existing_format(
                owner.id, *owner.document, owner.selection, owner.range, *owner.existing));
        } else if (!owner.existing) {
            const auto markers = document_inline_format_detail::default_markers(format, owner.document->syntax_mode);
            if (!markers) return std::nullopt;
            changes.emplace_back(owner.id, document_inline_format_detail::add_format(
                owner.id, *owner.document, owner.selection, owner.range, *markers));
        }
    }
    if (changes.empty()) return std::nullopt;

    const auto revision_before = document.revision;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(document, operations, revision_before);
    document_edit_detail::NodeAllocator allocator(document);
    auto after = selection;
    for (auto& [id, change] : changes) {
        auto applied = document_edit_detail::edit_inline(document, id, change.range,
            std::move(change.replacement), allocator);
        if (!applied) return std::nullopt;
        document_edit_detail::append_source_operation(operations, std::move(*applied));
        if (id == selection.anchor.container_id) after.anchor = change.selection_after.anchor;
        if (id == selection.active.container_id) after.active = change.selection_after.active;
    }
    ++document.revision;
    rollback.commit();
    return make_recorded_document_transaction(std::move(operations), selection, after,
        revision_before, document.revision, DocumentTransactionReason::Format);
}

inline std::optional<DocumentTransaction> document_delete_inline_marker(
    EditorDocument& document, const TextSelection& selection, bool backward) {
    if (selection.anchor.container_id != selection.active.container_id) return std::nullopt;
    auto owner = document_edit_detail::find_inline_owner(document, selection.active.container_id);
    if (!owner) return std::nullopt;
    auto start = (std::min)(selection.anchor.source_offset, selection.active.source_offset);
    auto end = (std::max)(selection.anchor.source_offset, selection.active.source_offset);
    if (selection.is_caret()) {
        if (backward && start == 0) return std::nullopt;
        start = backward ? start - 1 : start;
        end = start + 1;
    }
    auto node = inline_format_marker_at(owner->tree.nodes, start);
    if (!node) return std::nullopt;
    const auto& delim = node->delimiter_ranges();
    const auto marker = start < delim.content.start ? delim.opening : *delim.closing;
    if (end > marker.end) return std::nullopt;
    const auto format = document_inline_format_detail::without_code_padding(*owner,
        {node->range, delim.opening, delim.content, *delim.closing, node->kind});
    const auto caret = node->range.start
        + (std::clamp)(selection.active.source_offset, format.content.start, format.content.end) - format.content.start;
    const auto revision = document.revision;
    document_edit_detail::NodeAllocator allocator(document);
    auto applied = document_edit_detail::edit_inline(document, selection.active.container_id,
        node->range, inline_source_slice(*owner, format.content), allocator);
    if (!applied) return std::nullopt;
    ++document.revision;
    return document_edit_detail::source_transaction(document, std::move(*applied), selection,
        TextSelection::caret({selection.active.container_id, caret, selection.active.affinity}),
        revision, DocumentTransactionReason::Delete);
}

} // namespace folia
