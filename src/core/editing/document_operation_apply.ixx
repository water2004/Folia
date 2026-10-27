// folia.core.document_operation_apply — validates and replays reversible
// block-local source edits and unified-tree edits without document snapshots.
export module folia.core.document_operation_apply;
import std;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.core.document_ids;
import folia.core.document_references;
import folia.markdown.document_text;
import folia.core.document_transaction;
import folia.markdown.ids;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.core.inline_source_edit;
import folia.markdown.text_edit;

export namespace folia {

namespace document_operation_detail {

inline InlineDocument* inline_owner(BlockNode& root, NodeId id) {
    auto* block = find_block(root, id);
    return block ? editable_inline_document(*block) : nullptr;
}

inline void apply_payload(BlockNode& target, const BlockNode& shell) {
    auto children = std::move(target.children);
    std::optional<InlineDocument> preserved_inline;
    std::optional<BlockSourceDocument> preserved_block_source;
    if (auto* document = editable_inline_document(target)) {
        preserved_inline = std::move(*document);
    }
    if (editable_raw_block_source(target)) {
        preserved_block_source = std::move(target.block_source);
    }
    target = shell;
    target.children = std::move(children);
    if (preserved_inline) {
        if (auto* document = editable_inline_document(target)) {
            *document = std::move(*preserved_inline);
        }
    }
    if (preserved_block_source && editable_raw_block_source(target)) {
        target.block_source = std::move(*preserved_block_source);
    }
}

inline bool move_child(
    BlockNode& root,
    NodeId from_parent_id,
    std::size_t from_index,
    NodeId to_parent_id,
    std::size_t to_index,
    NodeId moved_id) {
    auto* from_parent = find_block(root, from_parent_id);
    if (!from_parent
        || from_index >= from_parent->children.size()
        || moved_id.v == 0
        || from_parent->children[from_index].id != moved_id) return false;

    if (from_parent_id == to_parent_id) {
        if (to_index >= from_parent->children.size()) return false;
        auto node = remove_block(*from_parent, from_index);
        if (!node) return false;
        if (!insert_block(*from_parent, to_index, std::move(*node))) std::terminate();
        return true;
    }

    auto* to_parent = find_block(root, to_parent_id);
    if (!to_parent || to_index > to_parent->children.size()) return false;
    if (find_block(from_parent->children[from_index], to_parent_id)) return false;

    auto node = remove_block(*from_parent, from_index);
    if (!node) return false;
    to_parent = find_block(root, to_parent_id);
    if (!to_parent || to_index > to_parent->children.size()) {
        const auto restored = insert_block(*from_parent, from_index, std::move(*node));
        if (!restored) std::terminate();
        return false;
    }
    if (!insert_block(*to_parent, to_index, std::move(*node))) std::terminate();
    return true;
}

inline bool apply_tree_edit(BlockNode& root, const DocumentTreeEdit& edit, bool forward) {
    switch (edit.kind) {
        case DocumentTreeEditKind::Insert: {
            auto* parent = find_block(root, edit.parent_id);
            if (!parent) return false;
            if (forward) return insert_block(*parent, edit.index, edit.after);
            if (edit.index >= parent->children.size()
                || parent->children[edit.index].id != edit.after.id) return false;
            return remove_block(*parent, edit.index).has_value();
        }
        case DocumentTreeEditKind::Remove: {
            auto* parent = find_block(root, edit.parent_id);
            if (!parent) return false;
            if (!forward) return insert_block(*parent, edit.index, edit.before);
            if (edit.index >= parent->children.size()
                || parent->children[edit.index].id != edit.before.id) return false;
            return remove_block(*parent, edit.index).has_value();
        }
        case DocumentTreeEditKind::Move:
            return forward
                ? move_child(
                    root,
                    edit.parent_id,
                    edit.index,
                    edit.other_parent_id,
                    edit.other_index,
                    edit.moved_id)
                : move_child(
                    root,
                    edit.other_parent_id,
                    edit.other_index,
                    edit.parent_id,
                    edit.index,
                    edit.moved_id);
        case DocumentTreeEditKind::UpdatePayload: {
            auto* target = find_block(root, edit.before.id);
            if (!target) return false;
            apply_payload(*target, forward ? edit.after : edit.before);
            return true;
        }
    }
    return false;
}

inline bool apply_text_edit(
    EditorDocument& document,
    const DocumentTextOperation& operation,
    bool forward) {
    const auto& edit = forward ? operation.forward : operation.inverse;
    auto* block = find_document_block(document, edit.container_id);
    if (block && editable_inline_document(*block)) {
        auto* owner = editable_inline_document(*block);
        if (!edit.range.valid_for(owner->source.size())) return false;
        const auto& expected=forward ? operation.inverse.replacement : operation.forward.replacement;
        if (std::u32string_view(owner->source).substr(edit.range.start,edit.range.length())!=expected)
            return false;
        auto candidate=*owner;
        const auto allocation_before=document.next_node_id;
        auto context=document_inline_parse_context(document,[&document]{return allocate_document_node_id(document);});
        apply_inline_source_edit(edit.container_id, candidate, edit, context);
        if (operation.inline_identities) {
            const auto& identities=forward ? operation.inline_identities->after : operation.inline_identities->before;
            if (!inline_source_edit_detail::restore_identities(candidate.tree.nodes,identities)) {
                document.next_node_id=allocation_before;
                return false;
            }
        }
        *owner=std::move(candidate);
        return true;
    }
    if (!block) return false;
    auto* source = editable_raw_block_source(*block);
    if (!source || !edit.range.valid_for(source->size())) return false;
    const auto& expected=forward ? operation.inverse.replacement : operation.forward.replacement;
    if (std::u32string_view(*source).substr(edit.range.start,edit.range.length())!=expected)
        return false;
    folia::apply_text_edit(*source, edit);
    const auto& indentation = forward
        ? operation.indentation_after
        : operation.indentation_before;
    if (indentation && block->block_source.storage) {
        block->block_source.storage->indentation_contexts = *indentation;
    }
    reparse_block_source(block->block_source);
    return true;
}

}  // namespace document_operation_detail

inline bool apply_document_operation(
    EditorDocument& document,
    const DocumentOperation& operation,
    bool forward) {
    return std::visit([&](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, DocumentTreeEdit>) {
            const auto applied=document_operation_detail::apply_tree_edit(document.root,value,forward);
            if (applied) document.reference_definitions_dirty=true;
            return applied;
        } else if constexpr (std::is_same_v<T,DocumentTextOperation>) {
            return document_operation_detail::apply_text_edit(document, value, forward);
        } else if constexpr (std::is_same_v<T,DocumentEndingEdit>) {
            const auto& expected=forward ? value.before : value.after;
            if (document.trailing_line_ending!=expected) return false;
            document.trailing_line_ending=forward ? value.after : value.before;
            return true;
        } else {
            const auto& expected=forward ? value.before : value.after;
            if (document.metadata!=expected) return false;
            document.metadata=forward ? value.after : value.before;
            return true;
        }
    }, operation);
}

// A failed replay is rolled back before returning false. This keeps Undo/Redo
// and command-level rollback from leaving a partially applied operation list.
inline bool apply_document_operations(
    EditorDocument& document,
    const std::vector<DocumentOperation>& operations,
    bool forward) {
    const auto allocation_before=document.next_node_id;
    // Public callers can construct operations without identity metadata. Keep
    // transient identities for their rollback too; normal recorded commands
    // already supply these and allocate no additional rollback storage.
    const auto needs_identities=std::ranges::any_of(operations,[](const DocumentOperation& operation) {
        const auto* text=std::get_if<DocumentTextOperation>(&operation);
        return text && !text->inline_identities;
    });
    std::vector<std::optional<InlineIdentityPatch>> transient(needs_identities ? operations.size() : 0);
    auto replay=[&](std::size_t index,bool direction,bool recovering=false) {
        const auto* text=std::get_if<DocumentTextOperation>(&operations[index]);
        if (!text || text->inline_identities) return apply_document_operation(document,operations[index],direction);
        if (recovering && transient[index]) {
            auto recorded=*text;recorded.inline_identities=transient[index];
            return apply_document_operation(document,DocumentOperation{std::move(recorded)},direction);
        }
        auto* block=find_document_block(document,text->forward.container_id);
        auto* owner=block ? editable_inline_document(*block) : nullptr;
        std::optional<InlineIdentityPatch> identities;
        if (owner) {
            identities.emplace();
            (direction ? identities->before : identities->after)=inline_source_edit_detail::node_identities(owner->tree.nodes);
        }
        if (!apply_document_operation(document,operations[index],direction)) return false;
        if (identities) {
            block=find_document_block(document,text->forward.container_id);
            owner=editable_inline_document(*block);
            (direction ? identities->after : identities->before)=inline_source_edit_detail::node_identities(owner->tree.nodes);
            transient[index]=std::move(identities);
        }
        return true;
    };
    if (forward) {
        std::size_t applied = 0;
        for (; applied < operations.size(); ++applied) {
            if (replay(applied,true)) continue;
            while (applied > 0) {
                --applied;
                if (!replay(applied,false,true)) std::terminate();
            }
            document.next_node_id=allocation_before;
            return false;
        }
        return true;
    }

    std::size_t index = operations.size();
    while (index > 0) {
        --index;
        if (replay(index,false)) continue;
        for (auto restore = index + 1; restore < operations.size(); ++restore) {
            if (!replay(restore,true,true)) std::terminate();
        }
        document.next_node_id=allocation_before;
        return false;
    }
    return true;
}

}  // namespace folia
