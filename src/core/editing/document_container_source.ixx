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
    const auto path = block_path(document.root, descendant_id);
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

inline std::vector<DocumentTreeEdit> apply_source_edit_to_ancestors(
    EditorDocument& document,
    const TextEdit& edit) {
    std::vector<DocumentTreeEdit> updates;
    auto path = block_path(document.root, edit.container_id);
    if (!path) return updates;
    auto* node = &document.root;
    for (std::size_t depth = 0; depth < path->size(); ++depth) {
        if (node->has_container_source()) {
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

inline std::vector<DocumentTreeEdit> rebind_source_anchors(
    EditorDocument& document,
    NodeId descendant_id,
    NodeId from_owner,
    std::size_t from_offset,
    NodeId to_owner,
    std::size_t to_offset) {
    std::vector<DocumentTreeEdit> updates;
    const auto path = block_path(document.root, descendant_id);
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
    NodeId new_owner) {
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
            moved.anchor.offset -= split_offset;
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

// A lossless fragment reparse changes owners but leaves the physical lines
// in place. Carry the recorded container prefixes onto the new owner of each
// line, including blank lines that move between a body and a separator.
inline std::vector<DocumentTreeEdit> rebind_fragment_anchors(
    EditorDocument& document, const std::vector<ContainerSourceSnapshot>& snapshots,
    const SerializedMarkdownProjection& before, const SerializedMarkdownProjection& after) {
    if (before.text != after.text || before.blocks.empty() || after.blocks.empty()) return {};
    auto new_anchor = [&](std::size_t offset) -> std::optional<ContainerPrefixAnchor> {
        for (const auto& map : after.source_maps) {
            const auto at = std::ranges::lower_bound(map.source_to_serialized, offset);
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

inline std::vector<NodeId> ancestor_ids(const BlockNode& root, NodeId id) {
    std::vector<NodeId> ids;
    const auto path = block_path(root, id);
    if (!path) return ids;
    const auto* node = &root;
    ids.push_back(node->id);
    for (const auto index : *path) {
        if (index >= node->children.size()) break;
        node = &node->children[index];
        ids.push_back(node->id);
    }
    return ids;
}

inline std::vector<DocumentTreeEdit> update_move_remove_prefixes(
    EditorDocument& document,
    const DocumentTreeEdit& edit) {
    if (edit.kind != DocumentTreeEditKind::Move
        && edit.kind != DocumentTreeEditKind::Remove) return {};
    const BlockNode* moved = edit.kind == DocumentTreeEditKind::Remove
        ? &edit.before
        : find_block(document.root, edit.moved_id);
    if (!moved) return {};
    const auto owners = subtree_ids(*moved);
    const auto old_ids = ancestor_ids(document.root, edit.parent_id);
    const auto new_ids = edit.kind == DocumentTreeEditKind::Move
        ? ancestor_ids(document.root, edit.other_parent_id)
        : std::vector<NodeId>{};
    const auto is_in = [](const std::vector<NodeId>& ids, NodeId id) {
        return std::ranges::find(ids, id) != ids.end();
    };
    std::vector<DocumentTreeEdit> updates;

    for (const auto id : old_ids) {
        if (edit.kind == DocumentTreeEditKind::Move && is_in(new_ids, id)) continue;
        auto* container = find_block(document.root, id);
        if (!container || !container->has_container_source()) continue;
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

inline void append_container_tree_operation(
    EditorDocument& document,
    std::vector<DocumentOperation>& operations,
    DocumentTreeEdit edit) {
    auto metadata = update_move_remove_prefixes(document, edit);
    operations.emplace_back(std::move(edit));
    for (auto& update : metadata) operations.emplace_back(std::move(update));
}

} // namespace folia::document_container_source
