// Reversible block split, join, exit, removal, and pruning operations.
export module folia.core.document_block_edit;
import std;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_document;
import folia.markdown.text_edit;
import folia.markdown.utf;
import folia.core.document_edit_primitives;
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
    auto path = block_path(document.root, id);
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
        offset = (std::min)(offset, block->inline_content.source.size());
        auto right_source = block->inline_content.source.substr(offset);
        const auto syntax_mode = block->inline_content.syntax_mode;
        if (offset == 0) {
            DocumentTreeEdit remove;
            remove.kind = DocumentTreeEditKind::Remove;
            remove.parent_id = parent->id;
            remove.index = index;
            remove.before = *block;
            document_container_source::append_container_tree_operation(document, result.operations, std::move(remove));
            parent->children.erase(
                parent->children.begin() + static_cast<std::ptrdiff_t>(index));
        } else if (offset < block->inline_content.source.size()) {
            auto edit = edit_inline(
                document,
                id,
                {offset, block->inline_content.source.size()},
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
        right.inline_content = make_inline(
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
    offset = (std::min)(offset, block->inline_content.source.size());
    auto right_source = block->inline_content.source.substr(offset);
    const auto syntax_mode = block->inline_content.syntax_mode;
    if (offset < block->inline_content.source.size()) {
        auto edit = edit_inline(
            document,
            id,
            {offset, block->inline_content.source.size()},
            {},
            allocator);
        if (!edit) return std::nullopt;
        append_source_operation(result.operations, std::move(*edit));
    }

    BlockNode right;
    right.id = allocator.allocate();
    right.kind = BlockKind::Paragraph;
    right.inline_content = make_inline(
        std::move(right_source),
        document,
        allocator,
        syntax_mode);
    result.target = TextPosition{right.id, 0, TextAffinity::Downstream};
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
        document, prefix_snapshots, id, offset, result.target.container_id);
    for (auto& update : prefix_updates) result.operations.emplace_back(std::move(update));
    return result;
}

inline std::optional<RecordedBlockEdit> join_parent_inline_owner(
    EditorDocument& document,
    NodeId child_id,
    NodeAllocator& allocator) {
    auto path = block_path(document.root, child_id);
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
    auto path = block_path(document.root, position.container_id);
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

inline std::optional<RecordedBlockEdit> exit_complete_raw_block_at_end(
    EditorDocument& document,
    TextPosition position,
    NodeAllocator& allocator) {
    auto path = block_path(document.root, position.container_id);
    if (!path || path->empty()) return std::nullopt;
    auto* block = block_at_path(document.root, *path);
    if (!block || (block->kind != BlockKind::CodeBlock
            && block->kind != BlockKind::MathBlock)
        || !block->block_source.tree().complete_closing) {
        return std::nullopt;
    }

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
    auto path = block_path(document.root, position.container_id);
    if (!path || path->empty()) return std::nullopt;
    auto* selected = block_at_path(document.root, *path);
    if (!selected || selected->kind != BlockKind::Paragraph || !selected->inline_content.source.empty()) return std::nullopt;

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

inline std::optional<RecordedBlockEdit> join_adjacent(
    EditorDocument& document,
    NodeId id,
    bool backward,
    NodeAllocator& allocator) {
    auto path = block_path(document.root, id);
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
        const auto offset = parent->children[index - 1].inline_content.source.size();
        const auto removed_owner = current.id;
        auto source = current.inline_content.source;
        if (!source.empty()) {
            auto edit = edit_inline(document, owner, {offset, offset}, std::move(source), allocator);
            if (!edit) return std::nullopt;
            append_source_operation(result.operations, std::move(*edit));
        }
        auto prefix_updates = document_container_source::rebind_source_anchors(
            document, owner, removed_owner, 0, owner, offset);
        for (auto& update : prefix_updates) result.operations.emplace_back(std::move(update));
        parent = find_block(document.root, parent_id);
        if (!parent || index >= parent->children.size()) return std::nullopt;
        DocumentTreeEdit remove;
        remove.kind = DocumentTreeEditKind::Remove;
        remove.parent_id = parent_id;
        remove.index = index;
        remove.before = parent->children[index];
        document_container_source::append_container_tree_operation(document, result.operations, std::move(remove));
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index));
        result.target = TextPosition{
            owner,
            offset,
            offset == 0 ? TextAffinity::Downstream : TextAffinity::Upstream};
        return result;
    }
    if (backward && index > 0 && current.kind == BlockKind::Paragraph
        && current.inline_content.source.empty()) {
        if (auto previous = last_editable_position(parent->children[index - 1])) {
            result.target = *previous;
            DocumentTreeEdit remove;
            remove.kind = DocumentTreeEditKind::Remove;
            remove.parent_id = parent_id;
            remove.index = index;
            remove.before = current;
            document_container_source::append_container_tree_operation(document, result.operations, std::move(remove));
            parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index));
            return result;
        }
    }
    if (!backward && index + 1 < parent->children.size()
        && text_block(parent->children[index + 1].kind)) {
        const auto owner = current.id;
        const auto offset = current.inline_content.source.size();
        const auto removed_owner = parent->children[index + 1].id;
        auto source = parent->children[index + 1].inline_content.source;
        if (!source.empty()) {
            auto edit = edit_inline(document, owner, {offset, offset}, std::move(source), allocator);
            if (!edit) return std::nullopt;
            append_source_operation(result.operations, std::move(*edit));
        }
        auto prefix_updates = document_container_source::rebind_source_anchors(
            document, owner, removed_owner, 0, owner, offset);
        for (auto& update : prefix_updates) result.operations.emplace_back(std::move(update));
        parent = find_block(document.root, parent_id);
        if (!parent || index + 1 >= parent->children.size()) return std::nullopt;
        DocumentTreeEdit remove;
        remove.kind = DocumentTreeEditKind::Remove;
        remove.parent_id = parent_id;
        remove.index = index + 1;
        remove.before = parent->children[index + 1];
        document_container_source::append_container_tree_operation(document, result.operations, std::move(remove));
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index + 1));
        result.target = TextPosition{owner, offset, TextAffinity::Downstream};
        return result;
    }
    if (!backward && index + 1 < parent->children.size()
        && current.kind == BlockKind::Paragraph && current.inline_content.source.empty()) {
        if (auto next = first_editable_position(parent->children[index + 1])) {
            result.target = *next;
            DocumentTreeEdit remove;
            remove.kind = DocumentTreeEditKind::Remove;
            remove.parent_id = parent_id;
            remove.index = index;
            remove.before = current;
            document_container_source::append_container_tree_operation(document, result.operations, std::move(remove));
            parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index));
            return result;
        }
    }
    return std::nullopt;
}

inline std::optional<RecordedBlockEdit> remove_direct_block(
    EditorDocument& document,
    NodeId id,
    NodeAllocator& allocator) {
    auto path = block_path(document.root, id);
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

inline std::optional<RecordedBlockEdit> remove_empty_raw_block(
    EditorDocument& document,
    NodeId id,
    NodeAllocator& allocator) {
    const auto* block = find_document_block(document, id);
    if (!block
        || (block->kind != BlockKind::CodeBlock && block->kind != BlockKind::MathBlock)
        || !block->block_source.tree().content.empty()) {
        return std::nullopt;
    }
    return remove_direct_block(document, id, allocator);
}

inline std::optional<RecordedBlockEdit> unwrap_math_marker(
    EditorDocument& document, const TextSelection& selection, bool backward,
    NodeAllocator& allocator) {
    if (selection.anchor.container_id != selection.active.container_id) return std::nullopt;
    auto path = block_path(document.root, selection.active.container_id);
    if (!path || path->empty()) return std::nullopt;
    auto* block = block_at_path(document.root, *path);
    if (!block || block->kind != BlockKind::MathBlock) return std::nullopt;
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
    paragraph.inline_content = make_inline(tree.content, document, allocator);
    RecordedBlockEdit result;
    result.target = {paragraph.id,
        (std::clamp)(selection.active.source_offset, content.start, content.end) - content.start,
        selection.active.affinity};
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
    auto path = block_path(document.root, id);
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
