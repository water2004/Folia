// Reversible block split, join, exit, removal, and pruning operations.
export module folia.core.document_block_edit;
import std;
import folia.markdown.ids;
import folia.markdown.selection;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_document;
import folia.markdown.text_edit;
import folia.markdown.utf;
import folia.core.document_edit_primitives;
import folia.core.document_html_split;
import folia.core.document_container_source;
import folia.core.document_transaction;

export namespace folia::document_edit_detail {

struct RecordedBlockEdit {
    TextPosition target;
    std::vector<DocumentOperation> operations;
};

inline void append_source_operation(
    std::vector<DocumentOperation>& operations,
    DocumentSourceEdit edit) {
    operations.emplace_back(DocumentTextOperation{
        edit.forward,
        edit.inverse,
        edit.indentation_before,
        edit.indentation_after,
        std::move(edit.inline_identities),
    });
    for (auto& metadata_edit : edit.metadata_edits) {
        operations.emplace_back(std::move(metadata_edit));
    }
}

inline std::optional<RecordedBlockEdit> split_direct(
    EditorDocument& document,
    NodeId id,
    std::size_t offset,
    NodeAllocator& allocator) {
    auto path = document_block_path(document, id);
    if (!path || path->empty()) return std::nullopt;
    auto parent_path = *path;
    const auto index = parent_path.back();
    parent_path.pop_back();
    auto* parent = block_at_path(document.root, parent_path);
    auto* block = block_at_path(document.root, *path);
    if (!parent || !block || index >= parent->children.size()) return std::nullopt;
    const auto prefix_snapshots = document_container_source::snapshot_ancestors(document, id);

    RecordedBlockEdit result;
    if (block->kind == BlockKind::CalloutTitle) {
        if (parent->kind != BlockKind::Callout) return std::nullopt;
        offset = (std::min)(offset, block->inline_content().source.size());
        auto right_source = block->inline_content().source.substr(offset);
        const auto syntax_mode = block->inline_content().syntax_mode;
        if (offset == 0) {
            DocumentTreeEdit remove;
            remove.kind = DocumentTreeEditKind::Remove;
            remove.parent_id = parent->id;
            remove.index = index;
            remove.before = *block;
            document_container_source::append_container_tree_operation(document, result.operations, std::move(remove));
            parent->children.erase(
                parent->children.begin() + static_cast<std::ptrdiff_t>(index));
        } else if (offset < block->inline_content().source.size()) {
            auto edit = edit_inline(
                document,
                id,
                {offset, block->inline_content().source.size()},
                {},
                allocator);
            if (!edit) return std::nullopt;
            append_source_operation(result.operations, std::move(*edit));
            parent = find_block(document.root, parent->id);
            if (!parent) return std::nullopt;
        }

        // The old first body is becoming a separate paragraph after the new
        // split body. Its saved header ending is no longer a sibling gap.
        const auto body_index=!parent->children.empty() && parent->children.front().kind==BlockKind::CalloutTitle ? 1u : 0u;
        if (body_index<parent->children.size() && parent->children[body_index].separator_before) {
            auto& body=parent->children[body_index];
            DocumentTreeEdit gap;
            gap.kind=DocumentTreeEditKind::UpdatePayload;
            gap.before=document_transaction_detail::payload_shell(body);
            body.separator_before.reset();
            gap.after=document_transaction_detail::payload_shell(body);
            result.operations.emplace_back(std::move(gap));
        }
        BlockNode right;
        right.id = allocator.allocate();
        right.kind = BlockKind::Paragraph;
        right.edit_inline_content() = make_inline(
            std::move(right_source),
            document,
            allocator,
            syntax_mode);
        result.target = TextPosition{right.id, 0, TextAffinity::Downstream};
        DocumentTreeEdit insert;
        insert.kind = DocumentTreeEditKind::Insert;
        insert.parent_id = parent->id;
        const auto insert_index = offset == 0 ? index : index + 1;
        insert.index = insert_index;
        insert.after = right;
        result.operations.emplace_back(std::move(insert));
        parent->children.insert(
            parent->children.begin() + static_cast<std::ptrdiff_t>(insert_index),
            std::move(right));
        auto prefix_updates = document_container_source::transfer_split_anchors(
            document, prefix_snapshots, id, offset, result.target.container_id);
        for (auto& update : prefix_updates) result.operations.emplace_back(std::move(update));
        return result;
    }

    if (!text_block(block->kind)) return std::nullopt;
    offset = (std::min)(offset, block->inline_content().source.size());
    const auto syntax_mode = block->inline_content().syntax_mode;
    // Within a semantic HTML container, adjacent anonymous inline runs have
    // no serialized boundary. Give both halves an explicit paragraph wrapper
    // so Enter survives saving/reloading, including when either half is empty.
    // A top-level HtmlText source block still uses its normal Markdown gap.
    bool html_parent = false;
    const auto* ancestor = &document.root;
    for (const auto child_index : parent_path) {
        ancestor = &ancestor->children[child_index];
        html_parent = html_parent || ancestor->kind == BlockKind::HtmlContainer
            || ancestor->has_html_source() || ancestor->has_html_element_provenance();
    }
    const bool html_paragraphs = syntax_mode == InlineSyntaxMode::HtmlText && html_parent;
    const auto html_split = html_paragraphs
        ? plan_html_inline_split(block->inline_content(), offset)
        : HtmlInlineSplitPlan{.source_offset = offset};
    offset = html_split.source_offset;
    auto right_source = html_split.right_prefix + block->inline_content().source.substr(offset);
    if (offset < block->inline_content().source.size() || !html_split.left_suffix.empty()) {
        auto edit = edit_inline(
            document,
            id,
            {offset, block->inline_content().source.size()},
            html_split.left_suffix,
            allocator);
        if (!edit) return std::nullopt;
        append_source_operation(result.operations, std::move(*edit));
    }
    auto give_paragraph_wrapper = [](BlockNode& paragraph) {
        auto& html = paragraph.ensure_html_special();
        html.root_tag = "p";
        html.opening_marker = U"<p>";
        html.closing_marker = U"</p>";
    };
    if (html_paragraphs && !block->has_html_element_provenance()) {
        DocumentTreeEdit wrapper;
        wrapper.kind = DocumentTreeEditKind::UpdatePayload;
        wrapper.before = document_transaction_detail::payload_shell(*block);
        give_paragraph_wrapper(*block);
        wrapper.after = document_transaction_detail::payload_shell(*block);
        result.operations.emplace_back(std::move(wrapper));
    }

    BlockNode right;
    right.id = allocator.allocate();
    right.kind = BlockKind::Paragraph;
    right.edit_inline_content() = make_inline(
        std::move(right_source),
        document,
        allocator,
        syntax_mode);
    if (html_paragraphs) give_paragraph_wrapper(right);
    result.target = TextPosition{right.id, html_split.right_caret_offset, TextAffinity::Downstream};
    DocumentTreeEdit insert;
    insert.kind = DocumentTreeEditKind::Insert;
    insert.parent_id = parent->id;
    insert.index = index + 1;
    insert.after = right;
    result.operations.emplace_back(std::move(insert));
    parent->children.insert(
        parent->children.begin() + static_cast<std::ptrdiff_t>(index + 1),
        std::move(right));
    auto prefix_updates = document_container_source::transfer_split_anchors(
        document, prefix_snapshots, id, offset, result.target.container_id, html_split.right_prefix.size());
    for (auto& update : prefix_updates) result.operations.emplace_back(std::move(update));
    return result;
}

inline std::optional<RecordedBlockEdit> join_parent_inline_owner(
    EditorDocument& document,
    NodeId child_id,
    NodeAllocator& allocator) {
    auto path = document_block_path(document, child_id);
    if (!path || path->size() < 2 || path->back() != 0) return std::nullopt;
    auto parent_path = *path;
    parent_path.pop_back();
    auto* parent = block_at_path(document.root, parent_path);
    if (!parent || parent->children.empty()) return std::nullopt;
    auto* parent_inline = editable_inline_document(*parent);
    auto* child_inline = editable_inline_document(parent->children.front());
    if (!parent_inline || !child_inline) return std::nullopt;

    const auto offset = parent_inline->source.size();
    auto child_source = child_inline->source;
    RecordedBlockEdit result;
    result.target = TextPosition{
        parent->id,
        offset,
        offset == 0 ? TextAffinity::Downstream : TextAffinity::Upstream};
    if (!child_source.empty()) {
        auto edit = edit_inline(document, parent->id, {offset, offset}, std::move(child_source), allocator);
        if (!edit) return std::nullopt;
        append_source_operation(result.operations, std::move(*edit));
        parent = find_block(document.root, parent->id);
        if (!parent || parent->children.empty()) return std::nullopt;
    }
    auto prefix_updates = document_container_source::rebind_source_anchors(
        document, parent->id, child_id, 0, parent->id, offset);
    for (auto& update : prefix_updates) result.operations.emplace_back(std::move(update));
    DocumentTreeEdit remove;
    remove.kind = DocumentTreeEditKind::Remove;
    remove.parent_id = parent->id;
    remove.index = 0;
    remove.before = parent->children.front();
    document_container_source::append_container_tree_operation(document, result.operations, std::move(remove));
    parent->children.erase(parent->children.begin());
    return result;
}

inline std::optional<RecordedBlockEdit> join_first_child_into_inline_owner(
    EditorDocument& document,
    NodeId parent_id,
    NodeAllocator& allocator) {
    auto* parent = find_block(document.root, parent_id);
    if (!parent || parent->children.empty()) return std::nullopt;
    auto* parent_inline = editable_inline_document(*parent);
    auto* child_inline = editable_inline_document(parent->children.front());
    if (!parent_inline || !child_inline) return std::nullopt;

    const auto offset = parent_inline->source.size();
    const auto child_id = parent->children.front().id;
    auto child_source = child_inline->source;
    RecordedBlockEdit result;
    result.target = TextPosition{parent->id, offset, TextAffinity::Downstream};
    if (!child_source.empty()) {
        auto edit = edit_inline(document, parent->id, {offset, offset}, std::move(child_source), allocator);
        if (!edit) return std::nullopt;
        append_source_operation(result.operations, std::move(*edit));
        parent = find_block(document.root, parent_id);
        if (!parent || parent->children.empty()) return std::nullopt;
    }
    auto prefix_updates = document_container_source::rebind_source_anchors(
        document, parent->id, child_id, 0, parent->id, offset);
    for (auto& update : prefix_updates) result.operations.emplace_back(std::move(update));
    DocumentTreeEdit remove;
    remove.kind = DocumentTreeEditKind::Remove;
    remove.parent_id = parent->id;
    remove.index = 0;
    remove.before = parent->children.front();
    document_container_source::append_container_tree_operation(document, result.operations, std::move(remove));
    parent->children.erase(parent->children.begin());
    return result;
}

inline std::optional<RecordedBlockEdit> exit_empty_indented_code(
    EditorDocument& document,
    TextPosition position,
    NodeAllocator& allocator) {
    auto path = document_block_path(document, position.container_id);
    if (!path || path->empty()) return std::nullopt;
    auto* block = block_at_path(document.root, *path);
    if (!block || block->kind != BlockKind::CodeBlock || !block->atomic_special().code_indented) return std::nullopt;

    const auto& source = block->block_source.source();
    const auto offset = (std::min)(position.source_offset, source.size());
    auto line_start = offset == 0 ? std::u32string::npos : source.rfind(U'\n', offset - 1);
    line_start = line_start == std::u32string::npos ? 0 : line_start + 1;
    auto line_end = source.find(U'\n', offset);
    if (line_end == std::u32string::npos) line_end = source.size();
    for (auto index = line_start; index < line_end; ++index) {
        if (source[index] != U' ' && source[index] != U'\t') return std::nullopt;
    }

    // The blank line is the exit trigger, not content that should survive in
    // either code block. Remove its preceding line separator as well so the
    // leading block does not retain a visually empty trailing line.
    const auto before_end = line_start > 0 && source[line_start - 1] == U'\n'
        ? line_start - 1
        : line_start;
    auto before = source.substr(0, before_end);
    auto after_start = line_end < source.size() ? line_end + 1 : line_end;
    auto after = source.substr(after_start);
    const auto source_size = source.size();
    auto parent_path = *path;
    const auto block_index = parent_path.back();
    parent_path.pop_back();
    auto* parent = block_at_path(document.root, parent_path);
    if (!parent || block_index >= parent->children.size()) return std::nullopt;

    auto paragraph = empty_paragraph(allocator, document);
    RecordedBlockEdit result;
    result.target = TextPosition{paragraph.id, 0, TextAffinity::Downstream};
    const auto parent_id = parent->id;
    if (before.empty()) {
        if (after.empty()) {
            DocumentTreeEdit remove;
            remove.kind = DocumentTreeEditKind::Remove;
            remove.parent_id = parent_id;
            remove.index = block_index;
            remove.before = parent->children[block_index];
            document_container_source::append_container_tree_operation(document, result.operations, std::move(remove));
            parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(block_index));
        } else {
            auto edit = edit_block_source(
                document,
                position.container_id,
                {0, after_start},
                {},
                allocator);
            if (!edit) return std::nullopt;
            append_source_operation(result.operations, std::move(*edit));
        }

        DocumentTreeEdit insert;
        insert.kind = DocumentTreeEditKind::Insert;
        insert.parent_id = parent_id;
        insert.index = block_index;
        insert.after = paragraph;
        result.operations.emplace_back(std::move(insert));
        parent = find_block(document.root, parent_id);
        if (!parent || !insert_block(*parent, block_index, std::move(paragraph))) return std::nullopt;
        return result;
    }

    auto edit = edit_block_source(
        document,
        position.container_id,
        {before_end, source_size},
        {},
        allocator);
    if (!edit) return std::nullopt;
    append_source_operation(result.operations, std::move(*edit));

    DocumentTreeEdit insert_paragraph;
    insert_paragraph.kind = DocumentTreeEditKind::Insert;
    insert_paragraph.parent_id = parent_id;
    insert_paragraph.index = block_index + 1;
    insert_paragraph.after = paragraph;
    result.operations.emplace_back(std::move(insert_paragraph));
    parent = find_block(document.root, parent_id);
    if (!parent || !insert_block(*parent, block_index + 1, std::move(paragraph))) return std::nullopt;
    if (!after.empty()) {
        auto trailing = parent->children[block_index];
        trailing.id = allocator.allocate();
        trailing.block_source = make_block_source(std::move(after), BlockSourceKind::IndentedCode);
        DocumentTreeEdit insert_trailing;
        insert_trailing.kind = DocumentTreeEditKind::Insert;
        insert_trailing.parent_id = parent_id;
        insert_trailing.index = block_index + 2;
        insert_trailing.after = trailing;
        result.operations.emplace_back(std::move(insert_trailing));
        if (!insert_block(*parent, block_index + 2, std::move(trailing))) return std::nullopt;
    }
    return result;
}

inline std::u32string display_math_line_ending(
    const EditorDocument& document, const BlockSourceDocument& source) {
    const auto fallback = document.trailing_line_ending.empty() ? std::u32string_view{U"\n"}
        : std::u32string_view{document.trailing_line_ending};
    return block_source_line_ending(source, fallback);
}

inline std::optional<InsertResult> insert_raw_line_break(
    EditorDocument& document, BlockNode& block, TextPosition position, NodeAllocator& allocator) {
    const auto& source = block.block_source;
    if (!block_source_has_math_delimiters(source))
        return insert_text(document, position, U"\n", allocator);
    const auto offset = block_source_clamp_editable_offset(source, position.source_offset, position.affinity);
    auto ending = display_math_line_ending(document, source);
    const auto target = offset + ending.size();
    auto edit = edit_block_source(document, block, {offset, offset}, std::move(ending), allocator);
    if (!edit) return std::nullopt;
    return InsertResult{target, TextAffinity::Downstream, std::move(edit)};
}

inline bool display_math_body_is_blank(const BlockSourceDocument& source) {
    return std::ranges::all_of(source.tree().content, [](char32_t character) {
        return character == U' ' || character == U'\t' || character == U'\r' || character == U'\n';
    });
}

inline std::optional<RecordedBlockEdit> enter_at_display_math_boundary(
    EditorDocument& document, TextPosition position, const BlockPath& path, BlockNode& block,
    NodeAllocator& allocator) {
    const auto& source = block.block_source;
    position.source_offset = block_source_clamp_editable_offset(source, position.source_offset, position.affinity);
    const auto opening = block_source_marker_range(source, BlockSourceTokenKind::OpeningMarker);
    const auto closing = block_source_marker_range(source, BlockSourceTokenKind::ClosingMarker);
    const bool before_opening = opening && position.source_offset <= opening->start;
    const bool after_closing = closing && position.source_offset >= closing->end;
    std::optional<SourceRange> trailing_break;
    if (!before_opening && !after_closing) {
        // The last empty body line is distinct from the start of the closer.
        // A second Enter there finishes a formula that already has content.
        if (!closing || position.source_offset >= closing->start
            || position.source_offset != block_source_content_range(source).end) return std::nullopt;
        trailing_break = block_source_trailing_line_break(source);
        if (!trailing_break || display_math_body_is_blank(source)) return std::nullopt;
    }

    auto parent_path = path;
    const auto index = parent_path.back();
    parent_path.pop_back();
    auto* parent = block_at_path(document.root, parent_path);
    if (!parent) return std::nullopt;

    const auto ending = display_math_line_ending(document, source);
    RecordedBlockEdit result;
    if (trailing_break) {
        auto edit = edit_block_source(document, block, *trailing_break, {}, allocator);
        if (!edit) return std::nullopt;
        append_source_operation(result.operations, std::move(*edit));
    }
    auto update_separator = [&](BlockNode& owner) {
        DocumentTreeEdit update;
        update.kind = DocumentTreeEditKind::UpdatePayload;
        update.before = document_transaction_detail::payload_shell(owner);
        owner.separator_before = ending;
        update.after = document_transaction_detail::payload_shell(owner);
        result.operations.emplace_back(std::move(update));
    };
    // A same-line suffix already has a text owner. Enter moves it to the next
    // line and lands at its start, exactly as splitting an ordinary line does.
    if (!before_opening && index + 1 < parent->children.size()) {
        auto& following = parent->children[index + 1];
        if (following.separator_before && (*following.separator_before).empty()) {
            if (auto target = first_editable_position(following)) {
                update_separator(following);
                result.target = *target;
                return result;
            }
        }
    }

    auto paragraph = empty_paragraph(allocator, document);
    if (before_opening) {
        paragraph.separator_before = block.separator_before;
        document_container_source::transfer_leading_anchors(
            document, path, block.id, paragraph.id, result.operations);
        update_separator(block);
        result.target = position;
    } else {
        paragraph.separator_before = ending;
        result.target = {paragraph.id, 0, TextAffinity::Downstream};
    }
    DocumentTreeEdit insert;
    insert.kind = DocumentTreeEditKind::Insert;
    insert.parent_id = parent->id;
    insert.index = before_opening ? index : index + 1;
    insert.after = paragraph;
    const auto insertion = insert.index;
    result.operations.emplace_back(std::move(insert));
    insert_block(*parent, insertion, std::move(paragraph));
    return result;
}

inline std::optional<RecordedBlockEdit> enter_at_raw_block_boundary(
    EditorDocument& document,
    TextPosition position,
    NodeAllocator& allocator) {
    auto path = document_block_path(document, position.container_id);
    if (!path || path->empty()) return std::nullopt;
    auto* block = block_at_path(document.root, *path);
    if (!block || !editable_raw_block_source(*block)) return std::nullopt;
    if (block_source_has_math_delimiters(block->block_source))
        return enter_at_display_math_boundary(document, position, *path, *block, allocator);
    if (!block->block_source.tree().complete_closing) return std::nullopt;

    const auto editable = block_source_editable_range(block->block_source);
    const auto after_closing = position.source_offset == block->block_source.source().size();
    const auto at_editable_end = position.source_offset == editable.end;
    const auto trailing_break = at_editable_end
        ? block_source_trailing_line_break(block->block_source)
        : std::nullopt;
    const auto empty_content = block->block_source.tree().content.empty();
    if (!after_closing && !(at_editable_end && (empty_content || trailing_break))) {
        return std::nullopt;
    }

    auto parent_path = *path;
    const auto index = parent_path.back();
    parent_path.pop_back();
    auto* parent = block_at_path(document.root, parent_path);
    if (!parent || index >= parent->children.size()) return std::nullopt;

    RecordedBlockEdit result;
    if (trailing_break) {
        auto edit = edit_block_source(
            document,
            position.container_id,
            *trailing_break,
            {},
            allocator);
        if (!edit) return std::nullopt;
        append_source_operation(result.operations, std::move(*edit));
        parent = find_block(document.root, parent->id);
        if (!parent || index >= parent->children.size()) return std::nullopt;
    }

    auto paragraph = empty_paragraph(allocator, document);
    result.target = {paragraph.id, 0, TextAffinity::Downstream};
    DocumentTreeEdit insert;
    insert.kind = DocumentTreeEditKind::Insert;
    insert.parent_id = parent->id;
    insert.index = index + 1;
    insert.after = paragraph;
    result.operations.emplace_back(std::move(insert));
    if (!insert_block(*parent, index + 1, std::move(paragraph))) return std::nullopt;
    return result;
}

inline std::optional<RecordedBlockEdit> exit_empty_flow_container(
    EditorDocument& document,
    TextPosition position,
    NodeAllocator& allocator) {
    auto path = document_block_path(document, position.container_id);
    if (!path || path->empty()) return std::nullopt;
    auto* selected = block_at_path(document.root, *path);
    if (!selected || selected->kind != BlockKind::Paragraph || !selected->inline_content().source.empty()) return std::nullopt;

    std::optional<std::size_t> container_depth;
    for (std::size_t depth = path->size(); depth > 0; --depth) {
        BlockPath candidate(path->begin(), path->begin() + static_cast<std::ptrdiff_t>(depth));
        auto const* ancestor = block_at_path(document.root, candidate);
        if (ancestor && (ancestor->kind == BlockKind::BlockQuote
            || ancestor->kind == BlockKind::Callout
            || ancestor->kind == BlockKind::FootnoteDefinition)) {
            container_depth = depth;
            break;
        }
    }
    if (!container_depth || path->size() != *container_depth + 1) return std::nullopt;

    BlockPath container_path(path->begin(), path->begin() + static_cast<std::ptrdiff_t>(*container_depth));
    auto parent_path = container_path;
    const auto container_index = parent_path.back();
    parent_path.pop_back();
    auto* parent = block_at_path(document.root, parent_path);
    if (!parent || container_index >= parent->children.size()) return std::nullopt;
    auto& container = parent->children[container_index];
    const auto child_index = (*path)[*container_depth];
    if (child_index >= container.children.size()) return std::nullopt;
    // Quotes and callouts can split around an empty child. A footnote
    // definition is one semantic item and cannot be duplicated with the same
    // label, so it exits only from its trailing empty child.
    if (container.kind == BlockKind::FootnoteDefinition
        && child_index + 1 != container.children.size()) return std::nullopt;

    // Delete the container's empty content node. A fresh paragraph outside
    // the container owns the caret; reparenting the trigger line would
    // preserve container membership instead of exiting it.
    auto paragraph = empty_paragraph(allocator, document);
    RecordedBlockEdit result;
    result.target = TextPosition{paragraph.id, 0, TextAffinity::Downstream};
    const auto parent_id = parent->id;
    const auto container_id = container.id;
    const auto trailing_count = container.children.size() - child_index - 1;

    DocumentTreeEdit insert_paragraph;
    insert_paragraph.kind = DocumentTreeEditKind::Insert;
    insert_paragraph.parent_id = parent_id;
    insert_paragraph.index = container_index + 1;
    insert_paragraph.after = paragraph;
    result.operations.emplace_back(std::move(insert_paragraph));
    if (!insert_block(*parent, container_index + 1, std::move(paragraph))) return std::nullopt;

    std::optional<NodeId> trailing_id;
    if (trailing_count > 0) {
        auto* current_container = find_block(document.root, container_id);
        if (!current_container) return std::nullopt;
        auto trailing = document_transaction_detail::payload_shell(*current_container);
        trailing.id = allocator.allocate();
        if (trailing.kind == BlockKind::Callout) trailing.ensure_text_special().opening_marker.clear();
        trailing_id = trailing.id;
        DocumentTreeEdit insert_trailing;
        insert_trailing.kind = DocumentTreeEditKind::Insert;
        insert_trailing.parent_id = parent_id;
        insert_trailing.index = container_index + 2;
        insert_trailing.after = trailing;
        result.operations.emplace_back(std::move(insert_trailing));
        parent = find_block(document.root, parent_id);
        if (!parent || !insert_block(*parent, container_index + 2, std::move(trailing))) return std::nullopt;

        for (std::size_t target_index = 0; target_index < trailing_count; ++target_index) {
            auto* source_container = find_block(document.root, container_id);
            auto* trailing_container = find_block(document.root, *trailing_id);
            if (!source_container || !trailing_container || child_index + 1 >= source_container->children.size()) {
                return std::nullopt;
            }
            DocumentTreeEdit move;
            move.kind = DocumentTreeEditKind::Move;
            move.parent_id = container_id;
            move.index = child_index + 1;
            move.other_parent_id = *trailing_id;
            move.other_index = target_index;
            move.moved_id = source_container->children[child_index + 1].id;
            auto child = remove_block(*source_container, child_index + 1);
            if (!child || !insert_block(*trailing_container, target_index, std::move(*child))) return std::nullopt;
            document_container_source::append_container_tree_operation(document, result.operations, std::move(move));
        }
    }

    auto* current_container = find_block(document.root, container_id);
    if (!current_container || child_index >= current_container->children.size()) return std::nullopt;
    DocumentTreeEdit remove_trigger;
    remove_trigger.kind = DocumentTreeEditKind::Remove;
    remove_trigger.parent_id = container_id;
    remove_trigger.index = child_index;
    remove_trigger.before = current_container->children[child_index];
    document_container_source::append_container_tree_operation(document, result.operations, std::move(remove_trigger));
    current_container->children.erase(
        current_container->children.begin() + static_cast<std::ptrdiff_t>(child_index));

    if (current_container->children.empty()) {
        parent = find_block(document.root, parent_id);
        if (!parent || container_index >= parent->children.size()) return std::nullopt;
        DocumentTreeEdit remove_container;
        remove_container.kind = DocumentTreeEditKind::Remove;
        remove_container.parent_id = parent_id;
        remove_container.index = container_index;
        remove_container.before = parent->children[container_index];
        document_container_source::append_container_tree_operation(document, result.operations, std::move(remove_container));
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(container_index));
    }
    return result;
}

inline std::optional<RecordedBlockEdit> join_next_list_item(
    EditorDocument& document,
    NodeId id,
    NodeAllocator& allocator) {
    auto path = document_block_path(document, id);
    if (!path || path->size() < 3) return std::nullopt;
    auto item_path = *path;
    const auto child_index = item_path.back();
    item_path.pop_back();
    auto* item = block_at_path(document.root, item_path);
    if (!item || (item->kind != BlockKind::ListItem && item->kind != BlockKind::TaskListItem)
        || child_index + 1 != item->children.size() || !text_block(item->children.back().kind)) {
        return std::nullopt;
    }
    auto list_path = item_path;
    const auto item_index = list_path.back();
    list_path.pop_back();
    auto* list = block_at_path(document.root, list_path);
    if (!list || (list->kind != BlockKind::List && list->kind != BlockKind::TaskList)
        || item_index + 1 >= list->children.size()) return std::nullopt;
    auto& next = list->children[item_index + 1];
    if ((next.kind != BlockKind::ListItem && next.kind != BlockKind::TaskListItem)
        || next.children.empty() || !text_block(next.children.front().kind)) return std::nullopt;

    const auto list_id = list->id;
    const auto item_id = item->id;
    const auto next_id = next.id;
    const auto removed_owner = next.children.front().id;
    const auto offset = item->children.back().inline_content().source.size();
    auto source = next.children.front().inline_content().source;
    RecordedBlockEdit result;
    result.target = {id, offset, TextAffinity::Downstream};
    if (!source.empty()) {
        auto edited = edit_inline(document, id, {offset, offset}, std::move(source), allocator);
        if (!edited) return std::nullopt;
        append_source_operation(result.operations, std::move(*edited));
    }
    auto prefix_updates = document_container_source::rebind_source_anchors(
        document, id, removed_owner, 0, id, offset);
    for (auto& update : prefix_updates) result.operations.emplace_back(std::move(update));
    auto next_item_path = list_path;
    next_item_path.push_back(item_index + 1);
    auto* next_item = block_at_path(document.root, next_item_path);
    if (!next_item || next_item->children.empty()) return std::nullopt;
    DocumentTreeEdit remove_text;
    remove_text.kind = DocumentTreeEditKind::Remove;
    remove_text.parent_id = next_id;
    remove_text.index = 0;
    remove_text.before = next_item->children.front();
    document_container_source::append_container_tree_operation(
        document, result.operations, std::move(remove_text), next_item_path);
    next_item->children.erase(next_item->children.begin());
    for (;;) {
        item = block_at_path(document.root, item_path);
        next_item = block_at_path(document.root, next_item_path);
        if (!item || !next_item) return std::nullopt;
        if (next_item->children.empty()) break;
        DocumentTreeEdit move;
        move.kind = DocumentTreeEditKind::Move;
        move.parent_id = next_id;
        move.index = 0;
        move.other_parent_id = item_id;
        move.other_index = item->children.size();
        move.moved_id = next_item->children.front().id;
        auto moved = remove_block(*next_item, 0);
        if (!moved || !insert_block(*item, move.other_index, std::move(*moved))) return std::nullopt;
        const auto target_index = move.other_index;
        document_container_source::append_container_tree_operation(document, result.operations,
            std::move(move), next_item_path, item_path, &item->children[target_index]);
    }
    list = block_at_path(document.root, list_path);
    if (!list || item_index + 1 >= list->children.size()) return std::nullopt;
    DocumentTreeEdit remove_item;
    remove_item.kind = DocumentTreeEditKind::Remove;
    remove_item.parent_id = list_id;
    remove_item.index = item_index + 1;
    remove_item.before = list->children[remove_item.index];
    document_container_source::append_container_tree_operation(
        document, result.operations, std::move(remove_item), list_path);
    list->children.erase(list->children.begin() + static_cast<std::ptrdiff_t>(item_index + 1));
    return result;
}

inline std::optional<RecordedBlockEdit> join_adjacent(
    EditorDocument& document,
    NodeId id,
    bool backward,
    NodeAllocator& allocator) {
    auto path = document_block_path(document, id);
    if (!path || path->empty()) return std::nullopt;
    auto parent_path = *path;
    const auto index = parent_path.back();
    parent_path.pop_back();
    auto* parent = block_at_path(document.root, parent_path);
    if (!parent || index >= parent->children.size() || !text_block(parent->children[index].kind)) {
        return std::nullopt;
    }
    auto& current = parent->children[index];
    const auto parent_id = parent->id;
    RecordedBlockEdit result;

    if (backward && index > 0 && text_block(parent->children[index - 1].kind)) {
        const auto owner = parent->children[index - 1].id;
        const auto offset = parent->children[index - 1].inline_content().source.size();
        const auto removed_owner = current.id;
        auto source = current.inline_content().source;
        if (!source.empty()) {
            auto edit = edit_inline(document, owner, {offset, offset}, std::move(source), allocator);
            if (!edit) return std::nullopt;
            append_source_operation(result.operations, std::move(*edit));
        }
        auto prefix_updates = document_container_source::rebind_source_anchors(
            document, owner, removed_owner, 0, owner, offset);
        for (auto& update : prefix_updates) result.operations.emplace_back(std::move(update));
        parent = block_at_path(document.root, parent_path);
        if (!parent || index >= parent->children.size()) return std::nullopt;
        DocumentTreeEdit remove;
        remove.kind = DocumentTreeEditKind::Remove;
        remove.parent_id = parent_id;
        remove.index = index;
        remove.before = parent->children[index];
        document_container_source::append_container_tree_operation(
            document, result.operations, std::move(remove), parent_path);
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index));
        result.target = TextPosition{
            owner,
            offset,
            offset == 0 ? TextAffinity::Downstream : TextAffinity::Upstream};
        return result;
    }
    if (backward && index > 0 && current.kind == BlockKind::Paragraph
        && current.inline_content().source.empty()) {
        if (auto previous = last_editable_position(parent->children[index - 1])) {
            result.target = *previous;
            DocumentTreeEdit remove;
            remove.kind = DocumentTreeEditKind::Remove;
            remove.parent_id = parent_id;
            remove.index = index;
            remove.before = current;
            document_container_source::append_container_tree_operation(
            document, result.operations, std::move(remove), parent_path);
            parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index));
            return result;
        }
    }
    if (!backward && index + 1 < parent->children.size()
        && text_block(parent->children[index + 1].kind)) {
        const auto owner = current.id;
        const auto offset = current.inline_content().source.size();
        const auto removed_owner = parent->children[index + 1].id;
        auto source = parent->children[index + 1].inline_content().source;
        if (!source.empty()) {
            auto edit = edit_inline(document, owner, {offset, offset}, std::move(source), allocator);
            if (!edit) return std::nullopt;
            append_source_operation(result.operations, std::move(*edit));
        }
        auto prefix_updates = document_container_source::rebind_source_anchors(
            document, owner, removed_owner, 0, owner, offset);
        for (auto& update : prefix_updates) result.operations.emplace_back(std::move(update));
        parent = block_at_path(document.root, parent_path);
        if (!parent || index + 1 >= parent->children.size()) return std::nullopt;
        DocumentTreeEdit remove;
        remove.kind = DocumentTreeEditKind::Remove;
        remove.parent_id = parent_id;
        remove.index = index + 1;
        remove.before = parent->children[index + 1];
        document_container_source::append_container_tree_operation(
            document, result.operations, std::move(remove), parent_path);
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index + 1));
        result.target = TextPosition{owner, offset, TextAffinity::Downstream};
        return result;
    }
    if (!backward && index + 1 < parent->children.size()
        && current.kind == BlockKind::Paragraph && current.inline_content().source.empty()) {
        if (auto next = first_editable_position(parent->children[index + 1])) {
            result.target = *next;
            DocumentTreeEdit remove;
            remove.kind = DocumentTreeEditKind::Remove;
            remove.parent_id = parent_id;
            remove.index = index;
            remove.before = current;
            document_container_source::append_container_tree_operation(
            document, result.operations, std::move(remove), parent_path);
            parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index));
            return result;
        }
    }
    // A nonempty text owner cannot merge with an image or divider. Deleting
    // its adjacent boundary removes the atomic neighbor while leaving the
    // text owner and its caret intact. Empty paragraphs above retain their
    // existing behavior: remove the paragraph and enter the neighboring block.
    if ((backward && index > 0 && atomic_block(parent->children[index - 1].kind))
        || (!backward && index + 1 < parent->children.size()
            && atomic_block(parent->children[index + 1].kind))) {
        const auto neighbor_index = backward ? index - 1 : index + 1;
        result.target = {current.id, backward ? 0 : current.inline_content().source.size(),
            backward ? TextAffinity::Downstream : TextAffinity::Upstream};
        DocumentTreeEdit remove;
        remove.kind = DocumentTreeEditKind::Remove;
        remove.parent_id = parent_id;
        remove.index = neighbor_index;
        remove.before = parent->children[neighbor_index];
        document_container_source::append_container_tree_operation(
            document, result.operations, std::move(remove), parent_path);
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(neighbor_index));
        return result;
    }
    if (!backward) return join_next_list_item(document, id, allocator);
    return std::nullopt;
}

inline std::optional<RecordedBlockEdit> remove_direct_block(
    EditorDocument& document,
    NodeId id,
    NodeAllocator& allocator) {
    auto path = document_block_path(document, id);
    if (!path || path->empty()) return std::nullopt;
    auto parent_path = *path;
    const auto index = parent_path.back();
    parent_path.pop_back();
    auto* parent = block_at_path(document.root, parent_path);
    if (!parent || index >= parent->children.size()) {
        return std::nullopt;
    }

    RecordedBlockEdit result;
    bool replace = false;
    if (index + 1 < parent->children.size()) {
        if (auto next = first_editable_position(parent->children[index + 1])) result.target = *next;
        else replace = true;
    } else if (index > 0) {
        if (auto previous = last_editable_position(parent->children[index - 1])) result.target = *previous;
        else replace = true;
    } else {
        replace = true;
    }

    DocumentTreeEdit remove;
    remove.kind = DocumentTreeEditKind::Remove;
    remove.parent_id = parent->id;
    remove.index = index;
    remove.before = parent->children[index];
    document_container_source::append_container_tree_operation(document, result.operations, std::move(remove));
    parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index));
    if (replace) {
        auto paragraph = empty_paragraph(allocator, document);
        result.target = {paragraph.id, 0, TextAffinity::Downstream};
        DocumentTreeEdit insert;
        insert.kind = DocumentTreeEditKind::Insert;
        insert.parent_id = parent->id;
        insert.index = index;
        insert.after = paragraph;
        result.operations.emplace_back(std::move(insert));
        if (!insert_block(*parent, index, std::move(paragraph))) return std::nullopt;
    }
    return result;
}

inline std::optional<RecordedBlockEdit> remove_atomic(
    EditorDocument& document,
    NodeId id,
    NodeAllocator& allocator) {
    const auto* block = find_document_block(document, id);
    if (!block || !atomic_block(block->kind)) return std::nullopt;
    return remove_direct_block(document, id, allocator);
}

inline std::optional<RecordedBlockEdit> remove_empty_display_math(
    EditorDocument& document, const BlockNode& block, TextPosition position, NodeAllocator& allocator) {
    const auto& source = block.block_source;
    const auto opening = block_source_marker_range(source, BlockSourceTokenKind::OpeningMarker);
    const auto closing = block_source_marker_range(source, BlockSourceTokenKind::ClosingMarker);
    if (!opening || !closing || closing->end != source.source().size()
        || position.source_offset <= opening->end || position.source_offset >= closing->start
        || !display_math_body_is_blank(source)) return std::nullopt;

    auto path = document_block_path(document, block.id);
    if (!path || path->empty()) return std::nullopt;
    const auto index = path->back();
    path->pop_back();
    auto* parent = block_at_path(document.root, *path);
    if (!parent || index >= parent->children.size()) return std::nullopt;

    BlockNode paragraph;
    paragraph.id = block.id;
    paragraph.kind = BlockKind::Paragraph;
    paragraph.separator_before = block.separator_before;
    paragraph.edit_inline_content() = make_inline({}, document, allocator);
    RecordedBlockEdit result;
    result.target = {paragraph.id, 0, TextAffinity::Downstream};
    // The empty paragraph keeps the original slot and its first-line prefix.
    // The remaining source anchors belonged to the deleted math lines.
    result.operations = document_container_source::apply_source_edit_to_ancestors(
        document, {block.id, {0, source.source().size()}, {}});
    result.operations.emplace_back(DocumentTreeEdit{
        .kind = DocumentTreeEditKind::Remove, .parent_id = parent->id, .index = index, .before = block});
    result.operations.emplace_back(DocumentTreeEdit{
        .kind = DocumentTreeEditKind::Insert, .parent_id = parent->id, .index = index, .after = paragraph});
    parent->children[index] = std::move(paragraph);
    return result;
}

inline std::optional<RecordedBlockEdit> remove_empty_raw_block_at_caret(
    EditorDocument& document, const BlockNode& block, TextPosition position, NodeAllocator& allocator) {
    if (block_source_has_math_delimiters(block.block_source))
        return remove_empty_display_math(document, block, position, allocator);
    if (!block.block_source.tree().content.empty()) return std::nullopt;
    return remove_direct_block(document, block.id, allocator);
}

inline std::optional<RecordedBlockEdit> unwrap_fenced_math_marker(
    EditorDocument& document, const TextSelection& selection, bool backward,
    NodeAllocator& allocator, TextPosition* tracked = nullptr) {
    if (selection.anchor.container_id != selection.active.container_id) return std::nullopt;
    auto path = document_block_path(document, selection.active.container_id);
    if (!path || path->empty()) return std::nullopt;
    auto* block = block_at_path(document.root, *path);
    if (!block || block->kind != BlockKind::MathBlock
        || block->block_source.tree().kind != BlockSourceKind::FencedMath) return std::nullopt;
    const auto& tree = block->block_source.tree();
    if (!tree.complete_opening || !tree.complete_closing) return std::nullopt;
    auto start = (std::min)(selection.anchor.source_offset, selection.active.source_offset);
    auto end = (std::max)(selection.anchor.source_offset, selection.active.source_offset);
    const auto content = block_source_editable_range(block->block_source);
    bool touches_marker = false;
    if (selection.is_caret()) {
        touches_marker = backward ? start == content.start : start == content.end;
        if (backward && start > 0) --start;
        else if (backward) return std::nullopt;
        end = start + 1;
    }
    for (const auto& token : tree.tokens) {
        if (token.kind != BlockSourceTokenKind::OpeningMarker
            && token.kind != BlockSourceTokenKind::ClosingMarker) continue;
        touches_marker = touches_marker || (start >= token.source_range.start && end <= token.source_range.end);
    }
    if (!touches_marker) return std::nullopt;
    BlockNode paragraph;
    paragraph.id = block->id;
    paragraph.kind = BlockKind::Paragraph;
    paragraph.separator_before = block->separator_before;
    paragraph.edit_inline_content() = make_inline(tree.content, document, allocator);
    RecordedBlockEdit result;
    result.target = {paragraph.id,
        (std::clamp)(selection.active.source_offset, content.start, content.end) - content.start,
        selection.active.affinity};
    if (tracked && tracked->container_id == paragraph.id)
        tracked->source_offset = (std::clamp)(tracked->source_offset, content.start, content.end) - content.start;
    const auto index = path->back();
    path->pop_back();
    auto* parent = block_at_path(document.root, *path);
    DocumentTreeEdit remove;
    remove.kind = DocumentTreeEditKind::Remove;
    remove.parent_id = parent->id;
    remove.index = index;
    remove.before = *block;
    document_container_source::append_container_tree_operation(document, result.operations, std::move(remove));
    DocumentTreeEdit insert;
    insert.kind = DocumentTreeEditKind::Insert;
    insert.parent_id = parent->id;
    insert.index = index;
    insert.after = paragraph;
    result.operations.emplace_back(std::move(insert));
    parent->children[index] = std::move(paragraph);
    return result;
}

inline bool remove_node_recorded(
    EditorDocument& document,
    NodeId id,
    std::vector<DocumentOperation>& operations) {
    auto path = document_block_path(document, id);
    if (!path || path->empty()) return false;
    auto parent_path = *path;
    const auto index = parent_path.back();
    parent_path.pop_back();
    auto* parent = block_at_path(document.root, parent_path);
    if (!parent || index >= parent->children.size()) return false;
    DocumentTreeEdit remove;
    remove.kind = DocumentTreeEditKind::Remove;
    remove.parent_id = parent->id;
    remove.index = index;
    remove.before = parent->children[index];
    document_container_source::append_container_tree_operation(document, operations, std::move(remove));
    if (!parent || index >= parent->children.size()) return false;
    parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

inline bool prunable_empty_container(const BlockNode& block) {
    if (!block.children.empty()) return false;
    switch (block.kind) {
        case BlockKind::BlockQuote:
        case BlockKind::FootnoteDefinition:
        case BlockKind::List:
        case BlockKind::TaskList:
        case BlockKind::ListItem:
        case BlockKind::TaskListItem:
        case BlockKind::TableRow:
        case BlockKind::Callout:
            return true;
        default:
            return false;
    }
}

inline void prune_empty_containers_recorded(
    BlockNode& parent,
    std::vector<DocumentOperation>& operations,
    EditorDocument& document) {
    std::size_t index = 0;
    while (index < parent.children.size()) {
        prune_empty_containers_recorded(parent.children[index], operations, document);
        if (!prunable_empty_container(parent.children[index])) {
            ++index;
            continue;
        }
        DocumentTreeEdit remove;
        remove.kind = DocumentTreeEditKind::Remove;
        remove.parent_id = parent.id;
        remove.index = index;
        remove.before = parent.children[index];
        document_container_source::append_container_tree_operation(document, operations, std::move(remove));
        parent.children.erase(parent.children.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

} // namespace folia::document_edit_detail
