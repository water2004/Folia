// Prefix provenance transforms for block-local source edits. Prefix records
// reference existing source owners; they never retain body text.
export module folia.core.document_container_source;
import std;
import folia.markdown.ast;
import folia.markdown.ids;
import folia.markdown.block_tree;
import folia.markdown.container_source;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.serializer;
import folia.core.document_transaction;
import folia.markdown.text_edit;

export namespace folia::document_container_source {

struct ContainerSourceSnapshot {
    NodeId container_id{};
    ContainerSource source;
};

inline std::vector<ContainerSourceSnapshot> snapshot_ancestors(
    const EditorDocument& document,
    NodeId descendant_id) {
    std::vector<ContainerSourceSnapshot> snapshots;
    const auto path = document_block_path(document, descendant_id);
    if (!path) return snapshots;
    const auto* node = &document.root;
    for (std::size_t depth = 0; depth < path->size(); ++depth) {
        if (node->has_container_source())
            snapshots.push_back({node->id, node->container_source()});
        const auto index = (*path)[depth];
        if (index >= node->children.size()) break;
        node = &node->children[index];
    }
    return snapshots;
}

inline std::u32string canonical_prefix(const BlockNode& container) {
    if (container.kind == BlockKind::BlockQuote || container.kind == BlockKind::Callout) return U"> ";
    if (container.kind != BlockKind::ListItem && container.kind != BlockKind::TaskListItem) return {};
    return list_continuation_prefix(container.item_special().marker,
        container.kind == BlockKind::TaskListItem);
}

inline bool source_anchor_deleted(const TextEdit& edit, std::size_t offset) {
    return edit.range.start < offset && offset <= edit.range.end;
}

inline std::optional<std::size_t> translate_anchor(const TextEdit& edit, std::size_t offset) {
    if (source_anchor_deleted(edit, offset)) return std::nullopt;
    if (offset <= edit.range.start) return offset;
    if (offset >= edit.range.end) {
        const auto removed = edit.range.length();
        const auto inserted = edit.replacement.size();
        return offset - removed + inserted;
    }
    return std::nullopt;
}

inline bool source_anchors_change(const ContainerSource& source, const TextEdit& edit) {
    if (edit.replacement.find_first_of(U"\r\n") != std::u32string::npos) return true;
    bool changed = false;
    source.visit_owner(edit.container_id, [&](const ContainerLinePrefix& line) {
        if (line.anchor.part != ContainerAnchorPart::Source) return;
        const auto offset = translate_anchor(edit, line.anchor.offset);
        changed = changed || !offset || *offset != line.anchor.offset;
    });
    return changed;
}

inline ContainerSource transform_source_anchors(
    const ContainerSource& source,
    const BlockNode& container,
    const TextEdit& edit) {
    ContainerSource result;
    result.lines.reserve(source.lines.size() + std::ranges::count(edit.replacement, U'\n'));
    const auto fallback = canonical_prefix(container);
    for (const auto& line : source.lines) {
        if (line.anchor.part != ContainerAnchorPart::Source
            || line.anchor.owner_id != edit.container_id) {
            result.lines.push_back(line);
            continue;
        }
        auto translated = translate_anchor(edit, line.anchor.offset);
        if (!translated) continue;
        auto moved = line;
        moved.anchor.offset = *translated;
        result.lines.push_back(std::move(moved));
    }

    // New physical lines inherit a canonical valid continuation prefix. They
    // are anchored after the inserted newline, at the following source line.
    for (std::size_t index = 0; index < edit.replacement.size(); ++index) {
        if (edit.replacement[index] != U'\n' && edit.replacement[index] != U'\r') continue;
        if (edit.replacement[index] == U'\r' && index + 1 < edit.replacement.size() && edit.replacement[index + 1] == U'\n') ++index;
        const auto new_offset = edit.range.start + index + 1;
        const bool already_recorded = std::ranges::any_of(result.lines, [&](const auto& line) {
            return line.anchor.part == ContainerAnchorPart::Source
                && line.anchor.owner_id == edit.container_id
                && line.anchor.offset == new_offset;
        });
        if (already_recorded) continue;
        result.lines.push_back(ContainerLinePrefix{
            ContainerPrefixAnchor{edit.container_id, ContainerAnchorPart::Source, new_offset},
            fallback,
            false});
    }
    std::ranges::sort(result.lines, [](const auto& left, const auto& right) {
        if (left.anchor.owner_id != right.anchor.owner_id)
            return left.anchor.owner_id.v < right.anchor.owner_id.v;
        if (left.anchor.part != right.anchor.part)
            return left.anchor.part < right.anchor.part;
        return left.anchor.offset < right.anchor.offset;
    });
    return result;
}

inline std::vector<DocumentOperation> apply_source_edit_to_ancestors(
    EditorDocument& document,
    const TextEdit& edit) {
    std::vector<DocumentOperation> updates;
    auto path = document_block_path(document, edit.container_id);
    if (!path) return updates;
    auto* node = &document.root;
    for (std::size_t depth = 0; depth < path->size(); ++depth) {
        if (node->has_container_source()
            && source_anchors_change(node->container_source(), edit)) {
            const auto transformed = transform_source_anchors(
                node->container_source(), *node, edit);
            if (!(transformed == node->container_source())) {
                DocumentTreeEdit update;
                update.kind = DocumentTreeEditKind::UpdatePayload;
                update.before = document_transaction_detail::payload_shell(*node);
                node->ensure_container_source() = transformed;
                update.after = document_transaction_detail::payload_shell(*node);
                updates.push_back(std::move(update));
            }
        }
        const auto index = (*path)[depth];
        if (index >= node->children.size()) break;
        node = &node->children[index];
    }
    return updates;
}

inline bool source_end_is_document_tail(const EditorDocument& document, NodeId owner, std::size_t offset) {
    // A retained container prefix can still follow a leaf's final newline.
    const auto path = cached_document_block_path(document, owner);
    if (!path) return false;
    const auto* node = &document.root;
    for (auto index : *path) {
        if (index + 1 != node->children.size()) return false;
        bool has_suffix = false;
        node->container_source().visit_owner(owner, [&](const auto& line) {
            has_suffix |= line.anchor.part == ContainerAnchorPart::Source
                && line.anchor.offset == offset && !line.text.empty();
        });
        if (has_suffix) return false;
        node = &node->children[index];
    }
    return node->id == owner;
}

inline std::vector<DocumentTreeEdit> rebind_source_anchors(
    EditorDocument& document,
    NodeId descendant_id,
    NodeId from_owner,
    std::size_t from_offset,
    NodeId to_owner,
    std::size_t to_offset) {
    std::vector<DocumentTreeEdit> updates;
    const auto path = document_block_path(document, descendant_id);
    if (!path) return updates;
    auto* node = &document.root;
    for (std::size_t depth = 0; depth <= path->size(); ++depth) {
        if (node->has_container_source()) {
            const auto before_source = node->container_source();
            auto after_source = before_source;
            std::erase_if(after_source.lines, [&](const auto& line) {
                return line.anchor.part == ContainerAnchorPart::Source
                    && line.anchor.owner_id == from_owner
                    && line.anchor.offset == from_offset;
            });
            for (auto& line : after_source.lines) {
                if (line.anchor.part != ContainerAnchorPart::Source
                    || line.anchor.owner_id != from_owner
                    || line.anchor.offset <= from_offset) continue;
                line.anchor.owner_id = to_owner;
                line.anchor.offset = to_offset + (line.anchor.offset - from_offset);
            }
            if (!(before_source == after_source)) {
                DocumentTreeEdit update;
                update.kind = DocumentTreeEditKind::UpdatePayload;
                update.before = document_transaction_detail::payload_shell(*node);
                node->ensure_container_source() = after_source;
                update.after = document_transaction_detail::payload_shell(*node);
                updates.push_back(std::move(update));
            }
        }
        if (depth == path->size()) break;
        const auto index = (*path)[depth];
        if (index >= node->children.size()) break;
        node = &node->children[index];
    }
    return updates;
}

inline std::vector<DocumentTreeEdit> transfer_split_anchors(
    EditorDocument& document,
    const std::vector<ContainerSourceSnapshot>& snapshots,
    NodeId old_owner,
    std::size_t split_offset,
    NodeId new_owner,
    std::size_t new_source_offset = 0) {
    std::vector<DocumentTreeEdit> updates;
    for (const auto& snapshot : snapshots) {
        auto* container = find_block(document.root, snapshot.container_id);
        if (!container || !container->has_container_source()) continue;
        auto next = container->container_source();
        std::erase_if(next.lines, [&](const auto& line) {
            return line.anchor.part == ContainerAnchorPart::Source
                && line.anchor.owner_id == old_owner
                && line.anchor.offset >= split_offset;
        });
        for (const auto& old_line : snapshot.source.lines) {
            if (old_line.anchor.part != ContainerAnchorPart::Source
                || old_line.anchor.owner_id != old_owner
                || old_line.anchor.offset < split_offset) continue;
            auto moved = old_line;
            moved.anchor.owner_id = new_owner;
            moved.anchor.offset = new_source_offset + moved.anchor.offset - split_offset;
            if (moved.lazy) {
                moved.text = canonical_prefix(*container);
                moved.lazy = false;
            }
            const bool exists = std::ranges::any_of(next.lines, [&](const auto& line) {
                return line.anchor == moved.anchor;
            });
            if (!exists) next.lines.push_back(std::move(moved));
        }
        const ContainerPrefixAnchor first_new_line{
            new_owner, ContainerAnchorPart::Source, 0};
        const bool has_new_first_line = std::ranges::any_of(next.lines, [&](const auto& line) {
            return line.anchor == first_new_line;
        });
        if (!has_new_first_line) {
            const ContainerLinePrefix* preceding = nullptr;
            for (const auto& old_line : snapshot.source.lines) {
                if (old_line.anchor.part != ContainerAnchorPart::Source
                    || old_line.anchor.owner_id != old_owner
                    || old_line.anchor.offset > split_offset) continue;
                if (!preceding || preceding->anchor.offset < old_line.anchor.offset)
                    preceding = &old_line;
            }
            ContainerLinePrefix seeded;
            seeded.anchor = first_new_line;
            if (preceding && !preceding->lazy && preceding->role == ContainerPrefixRole::Literal) seeded.text = preceding->text;
            else seeded.text = canonical_prefix(*container);
            seeded.lazy = false;
            next.lines.push_back(std::move(seeded));
        }
        if (next == container->container_source()) continue;
        DocumentTreeEdit update;
        update.kind = DocumentTreeEditKind::UpdatePayload;
        update.before = document_transaction_detail::payload_shell(*container);
        container->ensure_container_source() = next;
        update.after = document_transaction_detail::payload_shell(*container);
        updates.push_back(std::move(update));
    }
    return updates;
}

inline void transfer_leading_anchors(
    EditorDocument& document, const BlockPath& owner_path, NodeId from, NodeId to,
    std::vector<DocumentOperation>& operations) {
    const auto precedes_source = [](const ContainerLinePrefix& line) {
        return line.anchor.part == ContainerAnchorPart::Separator
            || (line.anchor.part == ContainerAnchorPart::Source && line.anchor.offset == 0
                && line.role == ContainerPrefixRole::ItemOpening);
    };
    auto* node = &document.root;
    for (auto index : owner_path) {
        if (node->has_container_source()) {
            bool has_leading_prefix = false;
            node->container_source().visit_owner(from, [&](const auto& line) {
                has_leading_prefix |= precedes_source(line);
            });
            if (has_leading_prefix) {
                DocumentTreeEdit update;
                update.kind = DocumentTreeEditKind::UpdatePayload;
                update.before = document_transaction_detail::payload_shell(*node);
                auto& source = node->ensure_container_source();
                for (auto& line : source.lines) {
                    if (line.anchor.owner_id == from && precedes_source(line))
                        line.anchor.owner_id = to;
                }
                source.invalidate_owner_lookup();
                update.after = document_transaction_detail::payload_shell(*node);
                operations.emplace_back(std::move(update));
            }
        }
        node = &node->children[index];
    }
}

// A lossless fragment reparse changes owners but leaves the physical lines
// in place. Carry the recorded container prefixes onto the new owner of each
// line, including blank lines that move between a body and a separator.
inline std::vector<DocumentTreeEdit> rebind_fragment_anchors(
    EditorDocument& document, const std::vector<ContainerSourceSnapshot>& snapshots,
    const SerializedMarkdownProjection& before, const SerializedMarkdownProjection& after) {
    if (before.text != after.text || before.blocks.empty() || after.blocks.empty()) return {};
    auto new_anchor = [&](std::size_t offset) -> std::optional<ContainerPrefixAnchor> {
        for (const auto& map : after.source_maps) {
            const auto at = map.source_to_serialized.lower_bound(offset);
            if (at != map.source_to_serialized.end() && *at == offset)
                return ContainerPrefixAnchor{map.container_id, ContainerAnchorPart::Source,
                    static_cast<std::size_t>(at - map.source_to_serialized.begin())};
        }
        for (std::size_t i = 0; i < after.blocks.size(); ++i) {
            const auto& block = after.blocks[i];
            if (i && offset >= after.blocks[i - 1].end && offset <= block.begin)
                return ContainerPrefixAnchor{block.owner, ContainerAnchorPart::Separator, offset - after.blocks[i - 1].end};
            if (offset == block.begin) return ContainerPrefixAnchor{block.owner, ContainerAnchorPart::Start, 0};
            if (offset == block.end) return ContainerPrefixAnchor{block.owner, ContainerAnchorPart::End, 0};
        }
        return std::nullopt;
    };
    std::vector<DocumentTreeEdit> updates;
    for (const auto& snapshot : snapshots) {
        auto* container = find_block(document.root, snapshot.container_id);
        if (!container) continue;
        ContainerSource next;
        next.lines.reserve(snapshot.source.lines.size());
        for (auto line : snapshot.source.lines) {
            auto& anchor = line.anchor;
            const auto block = std::ranges::find(before.blocks, anchor.owner_id, &SerializedBlockRange::owner);
            if (block == before.blocks.end()) {
                next.lines.push_back(std::move(line));
                continue;
            }
            std::optional<std::size_t> offset;
            switch (anchor.part) {
                case ContainerAnchorPart::Source:
                    offset = serialized_offset_for_source_position(before, {anchor.owner_id, anchor.offset});
                    break;
                case ContainerAnchorPart::Start: offset = block->begin; break;
                case ContainerAnchorPart::End: offset = block->end; break;
                case ContainerAnchorPart::Separator:
                    if (block == before.blocks.begin()) {
                        anchor.owner_id = after.blocks.front().owner;
                        next.lines.push_back(std::move(line));
                        continue;
                    }
                    offset = std::prev(block)->end + anchor.offset;
                    break;
            }
            if (auto replacement = offset ? new_anchor(*offset) : std::nullopt) {
                anchor = *replacement;
                next.lines.push_back(std::move(line));
            }
        }
        if (next == container->container_source()) continue;
        DocumentTreeEdit update;
        update.kind = DocumentTreeEditKind::UpdatePayload;
        update.before = document_transaction_detail::payload_shell(*container);
        container->ensure_container_source() = std::move(next);
        update.after = document_transaction_detail::payload_shell(*container);
        updates.push_back(std::move(update));
    }
    return updates;
}

inline std::unordered_set<std::uint64_t> subtree_ids(const BlockNode& root) {
    std::unordered_set<std::uint64_t> ids;
    walk_blocks(root, [&](const BlockNode& block) { ids.insert(block.id.v); });
    return ids;
}

inline std::vector<DocumentTreeEdit> update_move_remove_prefixes(
    EditorDocument& document,
    const DocumentTreeEdit& edit,
    const BlockPath& old_parent_path,
    const std::optional<BlockPath>& new_parent_path,
    const BlockNode* moved) {
    if (edit.kind != DocumentTreeEditKind::Move
        && edit.kind != DocumentTreeEditKind::Remove) return {};
    if (edit.kind == DocumentTreeEditKind::Remove) moved = &edit.before;
    if (!moved || (edit.kind == DocumentTreeEditKind::Move && !new_parent_path)) return {};
    std::vector<NodeId> new_ids;
    if (new_parent_path) {
        const auto* node = &document.root;
        new_ids.push_back(node->id);
        for (const auto index : *new_parent_path) {
            if (index >= node->children.size()) return {};
            node = &node->children[index];
            new_ids.push_back(node->id);
        }
        if (node->id != edit.other_parent_id) return {};
    }
    std::vector<BlockNode*> containers;
    auto* ancestor = &document.root;
    for (std::size_t depth = 0; depth <= old_parent_path.size(); ++depth) {
        if (ancestor->has_container_source()
            && std::ranges::find(new_ids, ancestor->id) == new_ids.end()) {
            containers.push_back(ancestor);
        }
        if (depth == old_parent_path.size()) break;
        if (old_parent_path[depth] >= ancestor->children.size()) return {};
        ancestor = &ancestor->children[old_parent_path[depth]];
    }
    if (ancestor->id != edit.parent_id || containers.empty()) return {};
    // Most item moves leave all existing prefix scopes intact. Only inspect
    // the moved subtree when a source-only ancestor actually owns provenance.
    const auto owners = subtree_ids(*moved);
    std::vector<DocumentTreeEdit> updates;
    for (auto* container : containers) {
        auto next = container->container_source();
        std::erase_if(next.lines, [&](const auto& line) {
            return owners.contains(line.anchor.owner_id.v);
        });
        if (next == container->container_source()) continue;
        DocumentTreeEdit update;
        update.kind = DocumentTreeEditKind::UpdatePayload;
        update.before = document_transaction_detail::payload_shell(*container);
        container->ensure_container_source() = std::move(next);
        update.after = document_transaction_detail::payload_shell(*container);
        updates.push_back(std::move(update));
    }

    return updates;
}

inline std::vector<DocumentTreeEdit> update_move_remove_prefixes(
    EditorDocument& document,
    const DocumentTreeEdit& edit) {
    if (edit.kind != DocumentTreeEditKind::Move
        && edit.kind != DocumentTreeEditKind::Remove) return {};
    // Callers can run between several tree mutations, before their
    // transaction updates locators. Use a validated cached location when it
    // is still valid, otherwise retain their existing tree lookup without
    // repairing any derived document index in the middle of the operation.
    const auto locate = [&](NodeId id) -> std::optional<BlockPath> {
        if (auto cached = cached_document_block_path(document, id)) {
            const auto* block = block_at_path(document.root, *cached);
            if (block && block->id == id) return cached;
        }
        return block_path(document.root, id);
    };
    const auto old_parent = locate(edit.parent_id);
    if (!old_parent) return {};
    const auto new_parent = edit.kind == DocumentTreeEditKind::Move
        ? locate(edit.other_parent_id) : std::nullopt;
    const BlockNode* moved = &edit.before;
    if (edit.kind == DocumentTreeEditKind::Move) {
        if (!new_parent) return {};
        const auto* parent = block_at_path(document.root, *new_parent);
        moved = parent && edit.other_index < parent->children.size()
            ? &parent->children[edit.other_index] : nullptr;
        if (!moved || moved->id != edit.moved_id) moved = find_block(document.root, edit.moved_id);
    }
    return update_move_remove_prefixes(document, edit, *old_parent, new_parent, moved);
}

inline void append_container_tree_operation(
    EditorDocument& document,
    std::vector<DocumentOperation>& operations,
    DocumentTreeEdit edit) {
    auto metadata = update_move_remove_prefixes(document, edit);
    operations.emplace_back(std::move(edit));
    for (auto& update : metadata) operations.emplace_back(std::move(update));
}

// Structural commands already know the parent paths and moved destination.
// Reuse those locations before the transaction repairs its derived index,
// rather than searching the entire document for newly inserted identities.
inline void append_container_tree_operation(
    EditorDocument& document,
    std::vector<DocumentOperation>& operations,
    DocumentTreeEdit edit,
    const BlockPath& old_parent_path,
    const std::optional<BlockPath>& new_parent_path = std::nullopt,
    const BlockNode* moved = nullptr) {
    auto metadata = update_move_remove_prefixes(
        document, edit, old_parent_path, new_parent_path, moved);
    operations.emplace_back(std::move(edit));
    for (auto& update : metadata) operations.emplace_back(std::move(update));
}

} // namespace folia::document_container_source
