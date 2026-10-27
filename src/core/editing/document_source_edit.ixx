export module folia.core.document_source_edit;
import std;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.core.table_selection;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.core.inline_source_edit;
import folia.markdown.text_edit;
import folia.markdown.utf;
import folia.core.document_edit_support;
import folia.core.document_block_reparse;
import folia.core.document_input_rules;
import folia.core.document_inline_format;

export namespace folia {

inline void normalize_document(EditorDocument& document) {
    document_edit_detail::NodeAllocator allocator(document);
    if (document.root.id.v == 0) document.root.id = allocator.allocate();
    document.root.kind = BlockKind::Document;
    if (document.root.children.empty()) document.root.children.push_back(document_edit_detail::empty_paragraph(allocator, document));
    document_edit_detail::normalize_blocks(document.root.children, document, allocator);
}

inline std::vector<DocumentInvariantError> validate_document(const EditorDocument& document) {
    std::vector<DocumentInvariantError> errors;
    std::unordered_set<std::uint64_t> ids;
    if (document.root.id.v == 0) errors.push_back({document.root.id, "document root has no id"});
    else ids.insert(document.root.id.v);
    if (document.root.children.empty()) errors.push_back({{}, "document has no blocks"});
    document_edit_detail::validate_blocks(
        document.root.children,
        BlockKind::Document,
        ids,
        errors);
    const auto maximum_id = ids.empty()
        ? std::uint64_t{0}
        : *std::ranges::max_element(ids);
    if (document.next_node_id == 0 || document.next_node_id <= maximum_id) {
        errors.push_back({
            {},
            "document node id cursor does not follow every owned node id"});
    }
    return errors;
}

inline std::optional<DocumentTransaction> document_delete_selection(EditorDocument&, const TextSelection&);

inline std::optional<DocumentTransaction> document_delete_marker(
    EditorDocument& document, const TextSelection& selection, bool backward) {
    if (auto inline_edit = document_delete_inline_marker(document, selection, backward)) return inline_edit;
    const auto revision = document.revision;
    document_edit_detail::NodeAllocator allocator(document);
    auto block_edit = document_edit_detail::unwrap_math_marker(document, selection, backward, allocator);
    if (!block_edit) return std::nullopt;
    ++document.revision;
    return make_recorded_document_transaction(std::move(block_edit->operations), selection,
        TextSelection::caret(block_edit->target), revision, document.revision, DocumentTransactionReason::Delete);
}

inline DocumentTransaction source_transaction_with_block_reparse(
    EditorDocument& after,
    DocumentSourceEdit edit,
    TextSelection selection_before,
    TextPosition target,
    std::uint64_t revision_before,
    DocumentTransactionReason reason) {
    std::vector<DocumentOperation> operations;
    const auto reparse_edit = edit;
    document_edit_detail::append_source_operation(operations, std::move(edit));
    document_edit_detail::MutationRollback rollback(after, operations, revision_before);
    document_edit_detail::NodeAllocator allocator(after);
    if (auto reclassified = document_edit_detail::reparse_edited_direct_block(
            after, target, allocator, &reparse_edit)) {
        target = reclassified->target;
        operations.insert(
            operations.end(),
            std::make_move_iterator(reclassified->operations.begin()),
            std::make_move_iterator(reclassified->operations.end()));
    }
    rollback.commit();
    return make_recorded_document_transaction(
        std::move(operations),
        selection_before,
        TextSelection::caret(target),
        revision_before,
        after.revision,
        reason);
}

inline std::optional<DocumentTransaction> document_insert_text(EditorDocument& document, const TextSelection& selection, std::u32string_view text) {
    if (text.empty()) return std::nullopt;
    const auto revision_before = document.revision;
    auto& working = document;
    auto current = selection;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(working, operations, revision_before);
    if (!selection.is_caret()) {
        const auto* block = selection.anchor.container_id == selection.active.container_id
            ? find_document_block(working, selection.active.container_id)
            : nullptr;
        if (block && editable_raw_block_source(*block)) {
            auto anchor = selection.anchor;
            auto active = selection.active;
            anchor.source_offset = clamp_editable_block_source_offset(
                *block, anchor.source_offset);
            active.source_offset = clamp_editable_block_source_offset(
                *block, active.source_offset);
            current = {anchor, active};
        }
        if (!current.is_caret()) {
            auto deletion = document_delete_selection(working, current);
            if (!deletion) return std::nullopt;
            operations.insert(
                operations.end(),
                std::make_move_iterator(deletion->operations.begin()),
                std::make_move_iterator(deletion->operations.end()));
            current = deletion->selection_after;
        }
    }
    document_edit_detail::NodeAllocator allocator(working);
    auto inserted = document_edit_detail::insert_text(working, current.active, text, allocator);
    if (!inserted || !inserted->source_edit) return std::nullopt;
    const auto reparse_edit = *inserted->source_edit;
    document_edit_detail::append_source_operation(
        operations, std::move(*inserted->source_edit));
    auto target = current.active; target.source_offset = inserted->offset; target.affinity = inserted->affinity;
    auto structural = document_input_rules::apply_after_text_insert(working, target, allocator);
    if (structural) {
        if (structural->operations.empty()) return std::nullopt;
        target = structural->target;
        operations.insert(
            operations.end(),
            std::make_move_iterator(structural->operations.begin()),
            std::make_move_iterator(structural->operations.end()));
    }
    if (auto reclassified = document_edit_detail::reparse_edited_direct_block(
            working, target, allocator, &reparse_edit)) {
        target = reclassified->target;
        operations.insert(
            operations.end(),
            std::make_move_iterator(reclassified->operations.begin()),
            std::make_move_iterator(reclassified->operations.end()));
    }
    working.revision = revision_before + 1;
    rollback.commit();
    return make_recorded_document_transaction(
        std::move(operations),
        selection,
        TextSelection::caret(target),
        revision_before,
        working.revision,
        DocumentTransactionReason::InsertText);
}

inline std::optional<DocumentTransaction> document_enter(EditorDocument& document, const TextSelection& selection) {
    const auto revision_before = document.revision;
    auto& after = document;
    auto current = selection;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(after, operations, revision_before);
    if (!current.is_caret()) {
        auto deletion = document_delete_selection(after, current);
        if (!deletion) return std::nullopt;
        operations.insert(
            operations.end(),
            std::make_move_iterator(deletion->operations.begin()),
            std::make_move_iterator(deletion->operations.end()));
        current = deletion->selection_after;
    }
    document_edit_detail::NodeAllocator allocator(after);
    TextPosition target;
    std::optional<DocumentSourceEdit> source_edit;
    if (auto exited = document_edit_detail::exit_empty_indented_code(after, current.active, allocator)) {
        target = exited->target;
        operations.insert(
            operations.end(),
            std::make_move_iterator(exited->operations.begin()),
            std::make_move_iterator(exited->operations.end()));
    } else if (auto exited_raw = document_edit_detail::exit_complete_raw_block_at_end(
                   after, current.active, allocator)) {
        target = exited_raw->target;
        operations.insert(
            operations.end(),
            std::make_move_iterator(exited_raw->operations.begin()),
            std::make_move_iterator(exited_raw->operations.end()));
    } else if (auto exited_container = document_edit_detail::exit_empty_flow_container(after, current.active, allocator)) {
        target = exited_container->target;
        operations.insert(
            operations.end(),
            std::make_move_iterator(exited_container->operations.begin()),
            std::make_move_iterator(exited_container->operations.end()));
    } else if (auto handled = document_input_rules::handle_enter(after, current.active, allocator)) {
        target = handled->target;
        operations.insert(
            operations.end(),
            std::make_move_iterator(handled->operations.begin()),
            std::make_move_iterator(handled->operations.end()));
    } else if (auto* block = find_block(after.root, current.active.container_id);
               block && (block->kind == BlockKind::CodeBlock || block->kind == BlockKind::MathBlock)) {
        auto inserted = document_edit_detail::insert_text(after, current.active, U"\n", allocator);
        if (!inserted) return std::nullopt;
        source_edit = std::move(inserted->source_edit);
        target = current.active;
        target.source_offset = inserted->offset;
        target.affinity = inserted->affinity;
    } else if (block && block->kind == BlockKind::TableCell) {
        const auto offset = (std::min)(current.active.source_offset, block->inline_content.source.size());
        source_edit = document_edit_detail::edit_inline(after, block->id, {offset, offset}, U"<br>", allocator);
        if (!source_edit) return std::nullopt;
        target = {block->id, offset + 4, TextAffinity::Downstream};
    } else {
        auto split = document_edit_detail::split_direct(
            after,
            current.active.container_id,
            current.active.source_offset,
            allocator);
        if (!split) return std::nullopt;
        target = split->target;
        operations.insert(
            operations.end(),
            std::make_move_iterator(split->operations.begin()),
            std::make_move_iterator(split->operations.end()));
    }
    after.revision = revision_before + 1;
    if (source_edit) {
        const auto reparse_edit = *source_edit;
        document_edit_detail::append_source_operation(operations, std::move(*source_edit));
        if (auto reclassified = document_edit_detail::reparse_edited_direct_block(
                after, target, allocator, &reparse_edit)) {
            target = reclassified->target;
            operations.insert(
                operations.end(),
                std::make_move_iterator(reclassified->operations.begin()),
                std::make_move_iterator(reclassified->operations.end()));
        }
    }
    if (operations.empty()) return std::nullopt;
    rollback.commit();
    return make_recorded_document_transaction(
        std::move(operations),
        selection,
        TextSelection::caret(target),
        revision_before,
        after.revision,
        DocumentTransactionReason::Structure);
}

inline std::optional<DocumentTransaction> document_insert_link(EditorDocument& document, const TextSelection& selection, std::string href, std::optional<std::string> title) {
    if (selection.anchor.container_id != selection.active.container_id) return std::nullopt;
    const auto revision_before = document.revision;
    auto& after = document; document_edit_detail::NodeAllocator allocator(after);
    auto* owner = document_edit_detail::find_inline_owner(after, selection.active.container_id); if (!owner) return std::nullopt;
    const auto start = (std::min)(selection.anchor.source_offset, selection.active.source_offset);
    const auto end = (std::min)((std::max)(selection.anchor.source_offset, selection.active.source_offset), owner->source.size());
    const auto label = owner->source.substr(start, end - start);
    std::u32string replacement = U"[" + label + U"](" + utf8_to_cps(href);
    if (title) replacement += U" \"" + utf8_to_cps(*title) + U"\"";
    replacement += U")";
    auto applied = document_edit_detail::edit_inline(
        after, selection.active.container_id, {start, end}, std::move(replacement), allocator);
    if (!applied) return std::nullopt;
    ++after.revision;
    const auto target = TextPosition{selection.active.container_id, start + 1 + label.size(), TextAffinity::Downstream};
    return document_edit_detail::source_transaction(
        after, std::move(*applied), selection, TextSelection::caret(target), revision_before, DocumentTransactionReason::Format);
}

inline std::optional<DocumentTransaction> document_insert_image(EditorDocument& document, const TextSelection& selection, std::string path, std::string alt) {
    if (selection.anchor.container_id != selection.active.container_id) return std::nullopt;
    const auto revision_before = document.revision;
    auto& after = document; document_edit_detail::NodeAllocator allocator(after);
    auto* owner = document_edit_detail::find_inline_owner(after, selection.active.container_id); if (!owner) return std::nullopt;
    const auto start = (std::min)(selection.anchor.source_offset, selection.active.source_offset);
    const auto end = (std::min)((std::max)(selection.anchor.source_offset, selection.active.source_offset), owner->source.size());
    auto label = alt.empty() ? owner->source.substr(start, end - start) : utf8_to_cps(alt);
    const auto replacement = U"![" + label + U"](" + utf8_to_cps(path.empty() ? "image.png" : path) + U")";
    auto applied = document_edit_detail::edit_inline(
        after, selection.active.container_id, {start, end}, replacement, allocator);
    if (!applied) return std::nullopt;
    ++after.revision;
    return document_edit_detail::source_transaction(
        after,
        std::move(*applied),
        selection,
        TextSelection::caret({selection.active.container_id, start + 2 + label.size(), TextAffinity::Downstream}),
        revision_before,
        DocumentTransactionReason::Format);
}

inline std::optional<DocumentTransaction> document_insert_soft_break(EditorDocument& document, const TextSelection& selection) {
    return document_insert_text(document, selection, U"\n");
}

inline std::optional<DocumentTransaction> document_set_code_block_language(
    EditorDocument& document,
    const TextSelection& selection,
    std::u32string language) {
    auto* block = find_document_block(document, selection.active.container_id);
    if (!block || block->kind != BlockKind::CodeBlock
        || block->block_source.tree().kind != BlockSourceKind::FencedCode) {
        return std::nullopt;
    }
    const auto horizontal_space = [](char32_t value) {
        return value == U' ' || value == U'\t';
    };
    while (!language.empty() && horizontal_space(language.front())) {
        language.erase(language.begin());
    }
    while (!language.empty() && horizontal_space(language.back())) {
        language.pop_back();
    }
    if (std::ranges::any_of(language, [horizontal_space](char32_t value) {
            return value == U'\r' || value == U'\n'
                || horizontal_space(value);
        })) {
        return std::nullopt;
    }

    const auto& tree = block->block_source.tree();
    if (!tree.info_range) return std::nullopt;
    auto replacement_range = tree.language_range.value_or(
        SourceRange{tree.info_range->start, tree.info_range->start});
    if (language.empty() && tree.language_range) {
        // Without a first token, the remaining info string would immediately
        // become the language. Clearing the selector therefore clears the
        // complete info string rather than silently reclassifying auxiliary
        // metadata.
        replacement_range = *tree.info_range;
    }
    if (block->block_source.source().substr(
            replacement_range.start,
            replacement_range.length()) == language) {
        return std::nullopt;
    }

    const auto revision_before = document.revision;
    document_edit_detail::NodeAllocator allocator(document);
    auto applied = document_edit_detail::edit_block_source(
        document,
        block->id,
        replacement_range,
        std::move(language),
        allocator);
    if (!applied) return std::nullopt;
    const auto selection_after = translate_selection(selection, applied->forward);
    ++document.revision;
    return document_edit_detail::source_transaction(
        document,
        std::move(*applied),
        selection,
        selection_after,
        revision_before,
        DocumentTransactionReason::Format);
}

inline std::optional<DocumentTransaction> document_delete_backward(EditorDocument& document, const TextSelection& selection) {
    if (!selection.is_caret()) return document_delete_selection(document, selection);
    if (auto marker = document_delete_marker(document, selection, true)) return marker;
    const auto revision_before = document.revision;
    auto& after = document; auto target = selection.active; document_edit_detail::NodeAllocator allocator(after);
    std::optional<DocumentSourceEdit> source_edit;
    std::vector<DocumentOperation> operations;
    auto const* selected_block = find_document_block(after, target.container_id);
    if (selected_block && document_edit_detail::atomic_block(selected_block->kind)) {
        auto removed = document_edit_detail::remove_atomic(after, target.container_id, allocator);
        if (!removed) return std::nullopt;
        target = removed->target;
        operations = std::move(removed->operations);
    } else if (selected_block
        && (selected_block->kind == BlockKind::CodeBlock
            || selected_block->kind == BlockKind::MathBlock)) {
        target.source_offset = block_source_clamp_editable_offset(
            selected_block->block_source,
            target.source_offset);
        if (selected_block->block_source.tree().content.empty()) {
            auto removed = document_edit_detail::remove_empty_raw_block(
                after, target.container_id, allocator);
            if (!removed) return std::nullopt;
            target = removed->target;
            operations = std::move(removed->operations);
        } else {
            auto range = block_source_backward_delete_range(
                selected_block->block_source,
                target.source_offset);
            if (!range) return std::nullopt;
            source_edit = document_edit_detail::erase_text(
                after, target.container_id, *range, allocator);
            if (!source_edit) return std::nullopt;
            target.source_offset = range->start;
        }
    } else if (target.source_offset > 0) {
        if (auto* owner = document_edit_detail::find_inline_owner(after, target.container_id)) {
            auto range = inline_backward_delete_range(
                *owner,
                target.source_offset,
                selected_block && selected_block->kind == BlockKind::TableCell);
            if (!range) return std::nullopt;
            source_edit = document_edit_detail::edit_inline(after, target.container_id, *range, {}, allocator);
            if (!source_edit) return std::nullopt;
            target.source_offset = range->start;
        } else {
            source_edit = document_edit_detail::erase_text(
                after, target.container_id, {target.source_offset - 1, target.source_offset}, allocator);
            if (!source_edit) return std::nullopt;
            --target.source_offset;
        }
    } else if (auto unprefixed = document_input_rules::handle_backspace_at_start(after, target, allocator)) {
        target = unprefixed->target;
        operations = std::move(unprefixed->operations);
    } else if (auto joined_parent = document_edit_detail::join_parent_inline_owner(
                   after, target.container_id, allocator)) {
        target = joined_parent->target;
        operations = std::move(joined_parent->operations);
    } else if (auto joined = document_edit_detail::join_adjacent(
                   after, target.container_id, true, allocator)) {
        target = joined->target;
        operations = std::move(joined->operations);
    } else {
        auto removed = document_edit_detail::remove_atomic(after, target.container_id, allocator);
        if (!removed) return std::nullopt;
        target = removed->target;
        operations = std::move(removed->operations);
    }
    ++after.revision;
    if (source_edit) {
        return source_transaction_with_block_reparse(
            after, std::move(*source_edit), selection, target,
            revision_before, DocumentTransactionReason::Delete);
    }
    if (operations.empty()) return std::nullopt;
    return make_recorded_document_transaction(
        std::move(operations),
        selection,
        TextSelection::caret(target),
        revision_before,
        after.revision,
        DocumentTransactionReason::Delete);
}

inline std::optional<DocumentTransaction> document_delete_forward(EditorDocument& document, const TextSelection& selection) {
    if (!selection.is_caret()) return document_delete_selection(document, selection);
    if (auto marker = document_delete_marker(document, selection, false)) return marker;
    const auto revision_before = document.revision;
    auto& after = document; auto target = selection.active; document_edit_detail::NodeAllocator allocator(after);
    std::optional<DocumentSourceEdit> source_edit;
    std::vector<DocumentOperation> operations;
    auto const* selected_block = find_document_block(after, target.container_id);
    if (selected_block && document_edit_detail::atomic_block(selected_block->kind)) {
        auto removed = document_edit_detail::remove_atomic(after, target.container_id, allocator);
        if (!removed) return std::nullopt;
        target = removed->target;
        operations = std::move(removed->operations);
        ++after.revision;
        return make_recorded_document_transaction(
            std::move(operations),
            selection,
            TextSelection::caret(target),
            revision_before,
            after.revision,
            DocumentTransactionReason::Delete);
    }
    if (selected_block
        && (selected_block->kind == BlockKind::CodeBlock
            || selected_block->kind == BlockKind::MathBlock)) {
        target.source_offset = block_source_clamp_editable_offset(
            selected_block->block_source,
            target.source_offset);
        if (selected_block->block_source.tree().content.empty()) {
            auto removed = document_edit_detail::remove_empty_raw_block(
                after, target.container_id, allocator);
            if (!removed) return std::nullopt;
            target = removed->target;
            operations = std::move(removed->operations);
        } else {
            auto range = block_source_forward_delete_range(
                selected_block->block_source,
                target.source_offset);
            if (!range) return std::nullopt;
            source_edit = document_edit_detail::erase_text(
                after, target.container_id, *range, allocator);
            if (!source_edit) return std::nullopt;
        }
        ++after.revision;
        if (source_edit) {
            return source_transaction_with_block_reparse(
                after, std::move(*source_edit), selection, target,
                revision_before, DocumentTransactionReason::Delete);
        }
        return make_recorded_document_transaction(
            std::move(operations),
            selection,
            TextSelection::caret(target),
            revision_before,
            after.revision,
            DocumentTransactionReason::Delete);
    }
    const auto length = document_edit_detail::editable_length(after, target.container_id); if (!length) return std::nullopt;
    if (target.source_offset < *length) {
        if (auto* owner = document_edit_detail::find_inline_owner(after, target.container_id)) {
            auto range = inline_forward_delete_range(
                *owner,
                target.source_offset,
                selected_block && selected_block->kind == BlockKind::TableCell);
            if (!range) return std::nullopt;
            source_edit = document_edit_detail::edit_inline(after, target.container_id, *range, {}, allocator);
            if (!source_edit) return std::nullopt;
        } else {
            source_edit = document_edit_detail::erase_text(
                after, target.container_id, {target.source_offset, target.source_offset + 1}, allocator);
            if (!source_edit) return std::nullopt;
        }
    } else if (auto joined_child = document_edit_detail::join_first_child_into_inline_owner(
                   after, target.container_id, allocator)) {
        target = joined_child->target;
        operations = std::move(joined_child->operations);
    } else if (auto joined = document_edit_detail::join_adjacent(
                   after, target.container_id, false, allocator)) {
        target = joined->target;
        operations = std::move(joined->operations);
    } else {
        return std::nullopt;
    }
    ++after.revision;
    if (source_edit) {
        return source_transaction_with_block_reparse(
            after, std::move(*source_edit), selection, target,
            revision_before, DocumentTransactionReason::Delete);
    }
    if (operations.empty()) return std::nullopt;
    return make_recorded_document_transaction(
        std::move(operations),
        selection,
        TextSelection::caret(target),
        revision_before,
        after.revision,
        DocumentTransactionReason::Delete);
}

inline std::optional<DocumentTransaction> document_delete_selection(EditorDocument& document, const TextSelection& selection) {
    if (selection.is_caret()) return std::nullopt;
    if (auto marker = document_delete_marker(document, selection, false)) return marker;
    const auto revision_before = document.revision;
    auto& after = document; document_edit_detail::NodeAllocator allocator(after);
    if (selection.anchor.container_id == selection.active.container_id) {
        auto start = (std::min)(selection.anchor.source_offset, selection.active.source_offset);
        auto end = (std::max)(selection.anchor.source_offset, selection.active.source_offset);
        if (const auto* block = find_document_block(after, selection.active.container_id);
            block && (block->kind == BlockKind::CodeBlock
                || block->kind == BlockKind::MathBlock)) {
            const auto range = block_source_editable_range(block->block_source);
            start = (std::clamp)(start, range.start, range.end);
            end = (std::clamp)(end, range.start, range.end);
            if (start == end) return std::nullopt;
        }
        auto source_edit = document_edit_detail::erase_text(
            after, selection.active.container_id, {start, end}, allocator);
        if (!source_edit) return std::nullopt;
        ++after.revision;
        const auto target = TextPosition{selection.active.container_id, start, TextAffinity::Downstream};
        return source_transaction_with_block_reparse(
            after, std::move(*source_edit), selection, target,
            revision_before, DocumentTransactionReason::Delete);
    }

    if (auto slices = document_table_barrier_text_slices(after, selection);
        slices && slices->size() > 1) {
        // A cross-cell text selection never implies a structural table edit.
        // Erase only the selected block-local source ranges and leave the
        // rectangular row/cell tree intact.  Explicit table handles are the
        // sole route to deleting rows or columns.
        std::vector<DocumentOperation> operations;
        document_edit_detail::MutationRollback rollback(
            after, operations, revision_before);
        for (auto const& slice : *slices) {
            if (slice.source_range.empty()) continue;
            auto source_edit = document_edit_detail::erase_text(
                after,
                slice.container_id,
                slice.source_range,
                allocator);
            if (!source_edit) return std::nullopt;
            document_edit_detail::append_source_operation(
                operations, std::move(*source_edit));
        }
        if (operations.empty()) return std::nullopt;
        ++after.revision;
        auto const target = TextPosition{
            slices->front().container_id,
            slices->front().source_range.start,
            TextAffinity::Downstream};
        rollback.commit();
        return make_recorded_document_transaction(
            std::move(operations),
            selection,
            TextSelection::caret(target),
            revision_before,
            after.revision,
            DocumentTransactionReason::Delete);
    }

    const auto& order = after.cached_editable_order;
    auto anchor_index = document_editable_order_position(
        after, selection.anchor.container_id);
    auto active_index = document_editable_order_position(
        after, selection.active.container_id);
    if (!anchor_index || !active_index) return std::nullopt;
    auto first = selection.anchor;
    auto last = selection.active;
    if (*anchor_index > *active_index) {
        std::swap(anchor_index, active_index);
        std::swap(first, last);
    }
    auto first_source = document_editable_text(after, first.container_id);
    auto last_source = document_editable_text(after, last.container_id);
    if (!first_source || !last_source) return std::nullopt;
    first.source_offset = (std::min)(first.source_offset, first_source->size());
    last.source_offset = (std::min)(last.source_offset, last_source->size());

    const auto* first_block = find_document_block(after, first.container_id);
    const auto* last_block = find_document_block(after, last.container_id);
    const auto raw_block = [](const BlockNode* block) {
        return block && (block->kind == BlockKind::CodeBlock
            || block->kind == BlockKind::MathBlock);
    };
    if (raw_block(first_block) || raw_block(last_block)) {
        // A raw block is a semantic structure boundary. Cross-block deletion
        // edits only each surviving boundary's visible content and removes
        // fully covered structures; it never splices paragraph source into a
        // fence or appends a math suffix to an unrelated block kind.
        std::vector<DocumentOperation> operations;
        document_edit_detail::MutationRollback rollback(
            after, operations, revision_before);
        std::vector<NodeId> remove_ids;
        for (std::size_t index = *anchor_index + 1; index < *active_index; ++index) {
            remove_ids.push_back(order[index]);
        }

        bool keep_first = true;
        bool keep_last = true;
        const auto first_range = first_block
            ? editable_block_source_range(*first_block)
            : std::nullopt;
        const auto last_range = last_block
            ? editable_block_source_range(*last_block)
            : std::nullopt;
        if (!first_range || !last_range) return std::nullopt;
        first.source_offset = (std::clamp)(
            first.source_offset, first_range->start, first_range->end);
        last.source_offset = (std::clamp)(
            last.source_offset, last_range->start, last_range->end);

        if (raw_block(first_block) && first.source_offset == first_range->start) {
            keep_first = false;
            remove_ids.push_back(first.container_id);
        } else if (first.source_offset < first_range->end) {
            auto edit = document_edit_detail::erase_text(
                after,
                first.container_id,
                {first.source_offset, first_range->end},
                allocator);
            if (!edit) return std::nullopt;
            document_edit_detail::append_source_operation(
                operations, std::move(*edit));
        }

        if (raw_block(last_block) && last.source_offset == last_range->end) {
            keep_last = false;
            remove_ids.push_back(last.container_id);
        } else if (last.source_offset > last_range->start) {
            auto edit = document_edit_detail::erase_text(
                after,
                last.container_id,
                {last_range->start, last.source_offset},
                allocator);
            if (!edit) return std::nullopt;
            document_edit_detail::append_source_operation(
                operations, std::move(*edit));
        }

        std::ranges::sort(remove_ids, [&](NodeId left, NodeId right) {
            return document_editable_order_position(after, left).value_or(0)
                > document_editable_order_position(after, right).value_or(0);
        });
        remove_ids.erase(std::unique(remove_ids.begin(), remove_ids.end()), remove_ids.end());
        for (const auto id : remove_ids) {
            if (!document_edit_detail::remove_node_recorded(after, id, operations)) return std::nullopt;
        }
        if (operations.empty()) return std::nullopt;
        document_edit_detail::prune_empty_containers_recorded(after.root, operations, after);
        if (after.root.children.empty()) {
            auto paragraph = document_edit_detail::empty_paragraph(allocator, after);
            DocumentTreeEdit insert;
            insert.kind = DocumentTreeEditKind::Insert;
            insert.parent_id = after.root.id;
            insert.index = 0;
            insert.after = paragraph;
            operations.emplace_back(std::move(insert));
            after.root.children.push_back(std::move(paragraph));
        }

        TextPosition target;
        if (keep_first) {
            target = first;
        } else if (keep_last) {
            // The surviving boundary owner was validated before structural
            // removal. Its source prefix was erased in place, so its visible
            // start remains the previously computed range start. Re-looking
            // it up here would consult stale child indices after an earlier
            // sibling was removed and force a full-tree locator repair.
            target = {
                last.container_id,
                last_range->start,
                TextAffinity::Downstream};
        } else {
            auto first_position = first_document_text_position(after);
            if (!first_position) return std::nullopt;
            target = *first_position;
        }
        ++after.revision;
        rollback.commit();
        return make_recorded_document_transaction(
            std::move(operations),
            selection,
            TextSelection::caret(target),
            revision_before,
            after.revision,
            DocumentTransactionReason::Delete);
    }

    auto suffix = last_source->substr(last.source_offset);
    auto source_edit = document_edit_detail::edit_block_source(
        after,
        first.container_id,
        {first.source_offset, first_source->size()},
        std::move(suffix),
        allocator);
    if (!source_edit) return std::nullopt;
    const auto reparse_edit = *source_edit;
    std::vector<DocumentOperation> operations;
    document_edit_detail::append_source_operation(operations, std::move(*source_edit));
    document_edit_detail::MutationRollback rollback(after, operations, revision_before);
    std::vector<NodeId> removed_ids;
    removed_ids.reserve(*active_index - *anchor_index);
    for (std::size_t index = *anchor_index + 1; index <= *active_index; ++index) {
        removed_ids.push_back(order[index]);
    }
    // Remove in reverse tree order so cached sibling paths ahead of the
    // mutation remain valid throughout this transaction.
    for (auto id = removed_ids.rbegin(); id != removed_ids.rend(); ++id) {
        if (!document_edit_detail::remove_node_recorded(after, *id, operations)) return std::nullopt;
    }
    document_edit_detail::prune_empty_containers_recorded(after.root, operations, after);
    if (after.root.children.empty()) {
        auto paragraph = document_edit_detail::empty_paragraph(allocator, after);
        DocumentTreeEdit insert;
        insert.kind = DocumentTreeEditKind::Insert;
        insert.parent_id = after.root.id;
        insert.index = 0;
        insert.after = paragraph;
        operations.emplace_back(std::move(insert));
        after.root.children.push_back(std::move(paragraph));
    }
    ++after.revision;
    auto target = TextPosition{first.container_id, first.source_offset, TextAffinity::Downstream};
    if (auto reclassified = document_edit_detail::reparse_edited_direct_block(
            after, target, allocator, &reparse_edit)) {
        target = reclassified->target;
        operations.insert(
            operations.end(),
            std::make_move_iterator(reclassified->operations.begin()),
            std::make_move_iterator(reclassified->operations.end()));
    }
    rollback.commit();
    return make_recorded_document_transaction(
        std::move(operations),
        selection,
        TextSelection::caret(target),
        revision_before,
        after.revision,
        DocumentTransactionReason::Delete);
}

inline std::optional<TextSelection> document_move_selection(const EditorDocument& document, const TextSelection& selection, DocumentMove movement, bool extend) {
    const auto& order = document.cached_editable_order;
    if (order.empty()) return std::nullopt;
    auto block_at = [&](std::size_t index) -> const BlockNode* {
        if (index >= order.size()) return nullptr;
        return find_document_block(document, order[index]);
    };
    auto text_at = [&](std::size_t index) -> std::optional<std::u32string_view> {
        const auto* block = block_at(index);
        return block ? editable_block_text_view(*block) : std::nullopt;
    };
    auto range_at = [&](std::size_t index) -> std::optional<SourceRange> {
        const auto* block = block_at(index);
        return block ? editable_block_source_range(*block) : std::nullopt;
    };
    auto index = document_editable_order_position(
        document, selection.active.container_id);
    if (!index) return std::nullopt;
    auto text = text_at(*index);
    auto range = range_at(*index);
    const auto* active_block = block_at(*index);
    if (!text || !range || !active_block) return std::nullopt;
    auto target = selection.active;
    target.source_offset = clamp_editable_block_source_offset(*active_block, target.source_offset);
    if (!extend && !selection.is_caret()) {
        const auto anchor_index = document_editable_order_position(
            document, selection.anchor.container_id).value_or(*index);
        const bool anchor_first = anchor_index < *index || (anchor_index == *index && selection.anchor.source_offset < selection.active.source_offset);
        target = (movement == DocumentMove::Left || movement == DocumentMove::Up || movement == DocumentMove::LineStart || movement == DocumentMove::DocumentStart)
            ? (anchor_first ? selection.anchor : selection.active) : (anchor_first ? selection.active : selection.anchor);
        return TextSelection::caret(target);
    }
    if (movement == DocumentMove::DocumentStart) {
        auto first_range = range_at(0); if (!first_range) return std::nullopt;
        target = {order.front(), first_range->start, TextAffinity::Downstream};
    }
    else if (movement == DocumentMove::DocumentEnd) {
        auto last_range = range_at(order.size() - 1); if (!last_range) return std::nullopt;
        target = {order.back(), last_range->end, TextAffinity::Downstream};
    }
    else if (movement == DocumentMove::Left) {
        if (editable_raw_block_source(*active_block)) {
            auto const& mapping = active_block->block_source.tree().content_to_source;
            auto current = std::ranges::lower_bound(mapping, target.source_offset);
            if (current != mapping.end() && *current == target.source_offset && current != mapping.begin()) {
                target.source_offset = *std::prev(current);
            } else if (target.source_offset > range->start) {
                target.source_offset = range->start;
            } else if (*index > 0) {
                auto previous = range_at(*index - 1); if (!previous) return std::nullopt;
                target = {order[*index - 1], previous->end, TextAffinity::Upstream};
            }
        }
        else if (target.source_offset > range->start) --target.source_offset;
        else if (*index > 0) {
            auto previous = range_at(*index - 1); if (!previous) return std::nullopt;
            target = {order[*index - 1], previous->end, TextAffinity::Upstream};
        }
    } else if (movement == DocumentMove::Right) {
        if (editable_raw_block_source(*active_block)) {
            auto const& mapping = active_block->block_source.tree().content_to_source;
            auto current = std::ranges::lower_bound(mapping, target.source_offset);
            if (current != mapping.end() && *current == target.source_offset
                && std::next(current) != mapping.end()) {
                target.source_offset = *std::next(current);
            } else if (target.source_offset < range->end) {
                target.source_offset = range->end;
            } else if (*index + 1 < order.size()) {
                auto next = range_at(*index + 1); if (!next) return std::nullopt;
                target = {order[*index + 1], next->start, TextAffinity::Downstream};
            }
        }
        else if (target.source_offset < range->end) ++target.source_offset;
        else if (*index + 1 < order.size()) {
            auto next = range_at(*index + 1); if (!next) return std::nullopt;
            target = {order[*index + 1], next->start, TextAffinity::Downstream};
        }
    } else {
        auto navigation_text = *text;
        auto navigation_offset = target.source_offset;
        auto raw_mapping = editable_raw_block_source(*active_block)
            ? &active_block->block_source.tree().content_to_source
            : nullptr;
        if (raw_mapping) {
            navigation_text = active_block->block_source.tree().content;
            auto mapped = std::ranges::lower_bound(*raw_mapping, target.source_offset);
            navigation_offset = mapped == raw_mapping->end()
                ? navigation_text.size()
                : static_cast<std::size_t>(std::distance(raw_mapping->begin(), mapped));
        }
        const auto before = navigation_offset == 0 ? std::u32string::npos : navigation_text.rfind(U'\n', navigation_offset - 1);
        const auto line_start = before == std::u32string::npos ? 0 : before + 1;
        const auto after = navigation_text.find(U'\n', navigation_offset);
        const auto line_end = after == std::u32string::npos ? navigation_text.size() : after;
        auto destination = navigation_offset;
        if (movement == DocumentMove::LineStart) destination = line_start;
        else if (movement == DocumentMove::LineEnd) destination = line_end;
        else {
            const auto column = navigation_offset - line_start;
            if (movement == DocumentMove::Up && line_start > 0) {
                const auto previous_end = line_start - 1;
                const auto previous_break = previous_end == 0 ? std::u32string::npos : navigation_text.rfind(U'\n', previous_end - 1);
                const auto previous_start = previous_break == std::u32string::npos ? 0 : previous_break + 1;
                destination = previous_start + (std::min)(column, previous_end - previous_start);
            } else if (movement == DocumentMove::Down && line_end < navigation_text.size()) {
                const auto next_start = line_end + 1;
                const auto next_break = navigation_text.find(U'\n', next_start);
                const auto next_end = next_break == std::u32string::npos ? navigation_text.size() : next_break;
                destination = next_start + (std::min)(column, next_end - next_start);
            }
        }
        target.source_offset = raw_mapping
            ? (*raw_mapping)[(std::min)(destination, raw_mapping->size() - 1)]
            : destination;
    }
    if (const auto* owner = document_edit_detail::find_inline_owner(document, target.container_id)) {
        const auto forward = movement == DocumentMove::Right || movement == DocumentMove::Down
            || movement == DocumentMove::LineEnd || movement == DocumentMove::DocumentEnd;
        target.source_offset = inline_format_marker_boundary(*owner, target.source_offset, forward);
    }
    if (target == selection.active) return std::nullopt;
    return extend ? TextSelection{selection.anchor, target} : TextSelection::caret(target);
}

inline std::optional<TextSelection> document_select_all(const EditorDocument& document) {
    const auto& order = document.cached_editable_order;
    if (order.empty()) return std::nullopt;
    const auto* first = find_document_block(document, order.front());
    const auto* last = find_document_block(document, order.back());
    auto first_range = first ? editable_block_source_range(*first) : std::nullopt;
    auto last_range = last ? editable_block_source_range(*last) : std::nullopt;
    if (!first_range || !last_range) return std::nullopt;
    return TextSelection{
        {order.front(), first_range->start, TextAffinity::Downstream},
        {order.back(), last_range->end, TextAffinity::Downstream}};
}

} // namespace folia
