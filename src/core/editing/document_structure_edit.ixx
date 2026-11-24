export module folia.core.document_structure_edit;
import std;
import folia.markdown.dialect;
import folia.markdown.selection;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.callout;
import folia.markdown.container_source;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.core.document_container_source;
import folia.core.document_ids;
import folia.markdown.ids;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.core.inline_source_edit;
import folia.markdown.text_edit;
import folia.core.table_selection;
import folia.markdown.utf;
import folia.core.document_edit_support;
import folia.core.document_input_rules;
import folia.core.document_list_input;
import folia.core.document_source_edit;
import folia.core.document_copy;

export namespace folia {

namespace document_structure_detail {

struct DirectRange {
    BlockPath parent_path;
    std::size_t first = 0;
    std::size_t last = 0;
};

inline std::optional<DirectRange> selected_list_items(
    const EditorDocument& document,
    const TextSelection& selection) {
    const auto anchor = document_block_path(document, selection.anchor.container_id);
    const auto active = document_block_path(document, selection.active.container_id);
    if (!anchor || !active) return std::nullopt;
    auto shared = (std::min)(anchor->size(), active->size());
    for (std::size_t index = 0; index < shared; ++index) {
        if ((*anchor)[index] != (*active)[index]) {
            shared = index;
            break;
        }
    }
    for (auto depth = shared; depth > 0; --depth) {
        if (depth >= anchor->size() || depth >= active->size()) continue;
        BlockPath path(anchor->begin(), anchor->begin() + static_cast<std::ptrdiff_t>(depth));
        const auto* list = block_at_path(document.root, path);
        if (!list || (list->kind != BlockKind::List && list->kind != BlockKind::TaskList)) continue;
        const auto anchor_item = (*anchor)[depth];
        const auto active_item = (*active)[depth];
        auto first = (std::min)(anchor_item, active_item);
        auto last = (std::max)(anchor_item, active_item);
        if (last >= list->children.size()) return std::nullopt;
        if (first < last) {
            const auto end = anchor_item < active_item ? selection.active : selection.anchor;
            const auto first_position = document_edit_detail::first_editable_position(list->children[last]);
            // A selection ending just before the next item does not include
            // that item, in either selection direction.
            const auto* end_block = find_document_block(document, end.container_id);
            const auto length = end_block ? document_edit_detail::local_position_length(*end_block) : std::nullopt;
            if (first_position && first_position->container_id == end.container_id
                && first_position->source_offset == end.source_offset
                && !(length == 0 && end.affinity == TextAffinity::Upstream)) --last;
        }
        return DirectRange{std::move(path), first, last};
    }
    return std::nullopt;
}

inline std::optional<NodeId> nearest_container(
    const EditorDocument& document,
    NodeId id,
    std::initializer_list<BlockKind> kinds) {
    auto path = block_path(document.root, id);
    if (!path) return std::nullopt;
    while (!path->empty()) {
        const auto* candidate = block_at_path(document.root, *path);
        if (candidate && std::find(kinds.begin(), kinds.end(), candidate->kind) != kinds.end()) {
            return candidate->id;
        }
        path->pop_back();
    }
    return std::nullopt;
}

inline std::optional<DirectRange> direct_range(
    const EditorDocument& document,
    NodeId first,
    NodeId last) {
    auto first_path = block_path(document.root, first);
    auto last_path = block_path(document.root, last);
    if (!first_path || !last_path || first_path->empty() || last_path->empty()) return std::nullopt;
    const auto first_index = first_path->back();
    const auto last_index = last_path->back();
    first_path->pop_back();
    last_path->pop_back();
    if (*first_path != *last_path) return std::nullopt;
    return DirectRange{
        std::move(*first_path),
        (std::min)(first_index, last_index),
        (std::max)(first_index, last_index)};
}

inline bool insert_container_and_move_range(
    EditorDocument& document,
    const DirectRange& range,
    BlockNode container,
    std::vector<DocumentOperation>& operations) {
    auto* parent = block_at_path(document.root, range.parent_path);
    if (!parent || range.last >= parent->children.size()) return false;
    const auto parent_id = parent->id;
    const auto container_id = container.id;
    DocumentTreeEdit insert;
    insert.kind = DocumentTreeEditKind::Insert;
    insert.parent_id = parent_id;
    insert.index = range.first;
    insert.after = container;
    operations.emplace_back(std::move(insert));
    if (!insert_block(*parent, range.first, std::move(container))) return false;

    const auto count = range.last - range.first + 1;
    for (std::size_t target_index = 0; target_index < count; ++target_index) {
        parent = find_block(document.root, parent_id);
        auto* inserted = find_block(document.root, container_id);
        if (!parent || !inserted || range.first + 1 >= parent->children.size()) return false;
        DocumentTreeEdit move;
        move.kind = DocumentTreeEditKind::Move;
        move.parent_id = parent_id;
        move.index = range.first + 1;
        move.other_parent_id = container_id;
        move.other_index = target_index;
        move.moved_id = parent->children[range.first + 1].id;
        auto child = remove_block(*parent, range.first + 1);
        if (!child || !insert_block(*inserted, target_index, std::move(*child))) return false;
        document_container_source::append_container_tree_operation(document, operations, std::move(move));
    }
    return true;
}

inline void ensure_paragraph_boundary(
    BlockNode& parent, std::size_t index, std::vector<DocumentOperation>& operations) {
    if (index == 0 || index >= parent.children.size()) return;
    auto& current = parent.children[index];
    const auto& previous = parent.children[index - 1];
    if (current.kind != BlockKind::Paragraph || previous.kind != BlockKind::Paragraph
        || !current.separator_before) return;
    const auto separator = *current.separator_before;
    std::size_t endings = 0;
    for (std::size_t offset = 0; offset < separator.size(); ++offset) {
        if (separator[offset] != U'\r' && separator[offset] != U'\n') continue;
        ++endings;
        if (separator[offset] == U'\r' && offset + 1 < separator.size()
            && separator[offset + 1] == U'\n') ++offset;
    }
    if (endings >= 2) return;
    DocumentTreeEdit spacing;
    spacing.kind = DocumentTreeEditKind::UpdatePayload;
    spacing.before = document_transaction_detail::payload_shell(current);
    // A heading/quote marker can interrupt a paragraph after one line ending.
    // Removing that marker needs a second ending, in the original spelling.
    std::u32string updated(separator);
    if (endings == 1) {
        const auto offset = separator.find_first_of(U"\r\n");
        const auto width = separator[offset] == U'\r' && offset + 1 < separator.size()
            && separator[offset + 1] == U'\n' ? 2u : 1u;
        updated.append(separator.substr(offset, width));
    } else updated += U"\n\n";
    current.separator_before.assign(std::move(updated));
    spacing.after = document_transaction_detail::payload_shell(current);
    operations.emplace_back(std::move(spacing));
}

inline bool unwrap_container(
    EditorDocument& document,
    const DirectRange& range,
    std::vector<DocumentOperation>& operations) {
    auto* parent = block_at_path(document.root, range.parent_path);
    if (!parent || range.first != range.last || range.first >= parent->children.size()) return false;
    const auto parent_id = parent->id;
    const auto container_id = parent->children[range.first].id;
    const auto incoming_separator = parent->children[range.first].separator_before;
    const auto child_count = parent->children[range.first].children.size();
    for (std::size_t target_index = 0; target_index < child_count; ++target_index) {
        parent = block_at_path(document.root, range.parent_path);
        const auto index = range.first + target_index;
        if (!parent || index >= parent->children.size()) return false;
        auto* container = &parent->children[index];
        if (container->id != container_id || container->children.empty()) return false;
        auto& first_child = container->children.front();
        if (target_index == 0 && first_child.separator_before != incoming_separator) {
            DocumentTreeEdit boundary;
            boundary.kind = DocumentTreeEditKind::UpdatePayload;
            boundary.before = document_transaction_detail::payload_shell(first_child);
            // The first child's former boundary belonged inside its wrapper
            // (for a callout, it followed the header). It now takes the
            // wrapper's exact incoming boundary in the surrounding flow.
            first_child.separator_before = incoming_separator;
            boundary.after = document_transaction_detail::payload_shell(first_child);
            operations.emplace_back(std::move(boundary));
        }
        DocumentTreeEdit move;
        move.kind = DocumentTreeEditKind::Move;
        move.parent_id = container_id;
        move.index = 0;
        move.other_parent_id = parent_id;
        move.other_index = index;
        move.moved_id = first_child.id;
        auto child = remove_block(*container, 0);
        if (!child || !insert_block(*parent, index, std::move(*child))) return false;
        auto shifted_container_path = range.parent_path;
        shifted_container_path.push_back(index + 1);
        document_container_source::append_container_tree_operation(document, operations,
            std::move(move), shifted_container_path, range.parent_path, &parent->children[index]);
        ensure_paragraph_boundary(*parent, index, operations);
    }
    parent = block_at_path(document.root, range.parent_path);
    const auto container_index = range.first + child_count;
    if (!parent || container_index >= parent->children.size()) return false;
    DocumentTreeEdit remove;
    remove.kind = DocumentTreeEditKind::Remove;
    remove.parent_id = parent_id;
    remove.index = container_index;
    remove.before = parent->children[container_index];
    document_container_source::append_container_tree_operation(
        document, operations, std::move(remove), range.parent_path);
    parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(container_index));
    ensure_paragraph_boundary(*parent, container_index, operations);
    return true;
}

} // namespace document_structure_detail

inline std::optional<DocumentTransaction> document_set_heading(
    EditorDocument& document, const TextSelection& selection, std::uint8_t level) {
    if (level > 6) return std::nullopt;
    for (const auto& position : {selection.anchor, selection.active}) {
        const auto* block = find_document_block(document, position.container_id);
        const auto length = block ? document_edit_detail::local_position_length(*block) : std::nullopt;
        if (!length || position.source_offset > *length) return std::nullopt;
    }
    const auto anchor_index = document_editable_order_position(document, selection.anchor.container_id);
    const auto active_index = document_editable_order_position(document, selection.active.container_id);
    if (!anchor_index || !active_index) return std::nullopt;
    const auto first = (std::min)(*anchor_index, *active_index);
    auto last = (std::max)(*anchor_index, *active_index);
    if (first < last) {
        const auto end = *anchor_index < *active_index ? selection.active : selection.anchor;
        const auto* end_block = find_document_block(document, end.container_id);
        const auto length = end_block ? document_edit_detail::local_position_length(*end_block) : std::nullopt;
        // An explicit upstream end of an empty owner (for example SelectAll)
        // still includes that owner, unlike an endpoint before the next block.
        if (end.source_offset == 0 && !(length == 0 && end.affinity == TextAffinity::Upstream)) --last;
    }
    std::vector<NodeId> owners;
    for (auto index = first; index <= last; ++index) {
        const auto id = document.cached_editable_order[index];
        const auto* block = find_document_block(document, id);
        // Callout titles have a required header role; heading formatting is
        // available on their body paragraphs without replacing that title.
        if (block && (block->kind == BlockKind::Paragraph || block->kind == BlockKind::Heading)) {
            owners.push_back(id);
        }
    }
    if (owners.empty()) return std::nullopt;

    const auto revision_before = document.revision;
    document_edit_detail::NodeAllocator allocator(document);
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(document, operations, revision_before);
    auto target = selection;
    const auto set_kind = [level](BlockNode& block, bool split_source) {
        const auto was_heading = block.kind == BlockKind::Heading;
        block.kind = level == 0 ? BlockKind::Paragraph : BlockKind::Heading;
        auto& special = block.ensure_text_special();
        special.level = level;
        special.opening_marker = level == 0 ? std::u32string{} : std::u32string(level, U'#') + U" ";
        special.closing_marker.clear();
        if (level == 0 || !was_heading || split_source) special.slug.clear();
    };
    struct PhysicalLine {
        std::size_t start;
        std::size_t end;
        std::size_t next;
        NodeId owner;
    };
    // Later owners are split first, so earlier cached paths stay valid while
    // the transaction has not yet repaired the derived document index.
    for (auto owner = owners.rbegin(); owner != owners.rend(); ++owner) {
        auto path = document_block_path(document, *owner);
        if (!path || path->empty()) return std::nullopt;
        auto parent_path = *path;
        const auto block_index = parent_path.back();
        parent_path.pop_back();
        auto* parent = block_at_path(document.root, parent_path);
        auto* block = &parent->children[block_index];
        const auto source = block->inline_content().source;
        const auto syntax = block->inline_content().syntax_mode;
        std::vector<PhysicalLine> lines;
        if (level != 0 && syntax != InlineSyntaxMode::HtmlText) {
            std::size_t start = 0;
            for (std::size_t index = 0; index < source.size(); ++index) {
                if (source[index] != U'\r' && source[index] != U'\n') continue;
                const auto end = index;
                if (source[index] == U'\r' && index + 1 < source.size() && source[index + 1] == U'\n') ++index;
                lines.push_back({start, end, index + 1, {}});
                start = index + 1;
            }
            lines.push_back({start, source.size(), source.size(), {}});
        } else lines.push_back({0, source.size(), source.size(), {}});
        lines.front().owner = *owner;
        std::vector<document_container_source::ContainerSourceSnapshot> snapshots;
        if (lines.size() > 1) {
            snapshots = document_container_source::snapshot_ancestors(document, *owner);
            auto edited = document_edit_detail::edit_inline(document, *owner,
                {lines.front().end, source.size()}, {}, allocator);
            if (!edited) return std::nullopt;
            document_edit_detail::append_source_operation(operations, std::move(*edited));
        }
        DocumentTreeEdit update;
        update.kind = DocumentTreeEditKind::UpdatePayload;
        update.before = document_transaction_detail::payload_shell(*block);
        set_kind(*block, lines.size() > 1);
        update.after = document_transaction_detail::payload_shell(*block);
        operations.emplace_back(std::move(update));

        BlockVec additions;
        additions.reserve(lines.size() - 1);
        for (std::size_t index = 1; index < lines.size(); ++index) {
            auto heading = document_transaction_detail::payload_shell(*block);
            heading.id = allocator.allocate();
            lines[index].owner = heading.id;
            const auto& previous = lines[index - 1];
            heading.separator_before = source.substr(previous.end, previous.next - previous.end);
            heading.edit_inline_content() = document_edit_detail::make_inline(
                source.substr(lines[index].start, lines[index].end - lines[index].start),
                document, allocator, syntax);
            DocumentTreeEdit insert;
            insert.kind = DocumentTreeEditKind::Insert;
            insert.parent_id = parent->id;
            insert.index = block_index + index;
            insert.after = heading;
            operations.emplace_back(std::move(insert));
            additions.push_back(std::move(heading));
        }
        if (!additions.empty()) {
            parent->children.insert(parent->children.begin() + static_cast<std::ptrdiff_t>(block_index + 1),
                std::make_move_iterator(additions.begin()), std::make_move_iterator(additions.end()));
            const auto line_index = [&](std::size_t offset) {
                const auto found = std::upper_bound(lines.begin(), lines.end(), offset,
                    [](std::size_t value, const PhysicalLine& line) { return value < line.start; });
                return static_cast<std::size_t>((found - lines.begin()) - 1);
            };
            const auto map_position = [&](TextPosition& position) {
                if (position.container_id != *owner) return;
                const auto& line = lines[line_index(position.source_offset)];
                position.container_id = line.owner;
                position.source_offset = (std::min)(position.source_offset, line.end) - line.start;
            };
            map_position(target.anchor);
            map_position(target.active);
            // Prefix provenance follows the physical line into its new owner.
            // A previously lazy continuation now needs an explicit container
            // prefix because an ATX heading interrupts paragraph continuation.
            for (const auto& snapshot : snapshots) {
                auto* container = find_document_block(document, snapshot.container_id);
                if (!container || !container->has_container_source()) continue;
                auto next = container->container_source();
                for (const auto& old_line : snapshot.source.lines) {
                    if (old_line.anchor.part != ContainerAnchorPart::Source
                        || old_line.anchor.owner_id != *owner
                        || old_line.anchor.offset < lines[1].start) continue;
                    const auto& line = lines[line_index(old_line.anchor.offset)];
                    auto moved = old_line;
                    moved.anchor.owner_id = line.owner;
                    moved.anchor.offset = (std::min)(old_line.anchor.offset, line.end) - line.start;
                    if (moved.lazy) {
                        moved.text = document_container_source::canonical_prefix(*container);
                        moved.lazy = false;
                    }
                    next.lines.push_back(std::move(moved));
                }
                if (next == container->container_source()) continue;
                DocumentTreeEdit prefix;
                prefix.kind = DocumentTreeEditKind::UpdatePayload;
                prefix.before = document_transaction_detail::payload_shell(*container);
                container->ensure_container_source() = std::move(next);
                prefix.after = document_transaction_detail::payload_shell(*container);
                operations.emplace_back(std::move(prefix));
            }
        }
        if (level == 0) {
            document_structure_detail::ensure_paragraph_boundary(*parent, block_index, operations);
            document_structure_detail::ensure_paragraph_boundary(*parent, block_index + 1, operations);
        }
    }
    document.revision = revision_before + 1;
    rollback.commit();
    return make_recorded_document_transaction(std::move(operations), selection, target,
        revision_before, document.revision, DocumentTransactionReason::Structure);
}

inline std::optional<DocumentTransaction> document_toggle_block_quote(EditorDocument& document, const TextSelection& selection) {
    const auto revision_before = document.revision;
    auto& after = document; document_edit_detail::NodeAllocator allocator(after);
    auto first = selection.anchor.container_id;
    auto last = selection.active.container_id;
    const auto first_quote = document_structure_detail::nearest_container(
        after, first, {BlockKind::BlockQuote});
    const auto last_quote = document_structure_detail::nearest_container(
        after, last, {BlockKind::BlockQuote});
    if (first_quote && first_quote == last_quote) first = last = *first_quote;
    auto range = document_structure_detail::direct_range(
        after, first, last);
    if (!range) return std::nullopt;
    auto* parent = block_at_path(after.root, range->parent_path);
    if (!parent || range->last >= parent->children.size()) return std::nullopt;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(after, operations, revision_before);
    if (range->first == range->last && parent->children[range->first].kind == BlockKind::BlockQuote) {
        if (!document_structure_detail::unwrap_container(after, *range, operations)) return std::nullopt;
    } else {
        BlockNode quote;
        quote.id = allocator.allocate();
        quote.kind = BlockKind::BlockQuote;
        if (!document_structure_detail::insert_container_and_move_range(
                after, *range, std::move(quote), operations)) return std::nullopt;
    }
    ++after.revision;
    rollback.commit();
    return make_recorded_document_transaction(
        std::move(operations), selection, selection,
        revision_before, after.revision, DocumentTransactionReason::Structure);
}

inline std::optional<DocumentTransaction> document_toggle_callout(EditorDocument& document, const TextSelection& selection, std::string kind) {
    const auto normalized = normalize_callout_kind(kind.empty() ? std::string_view{"NOTE"} : kind);
    if (!normalized) return std::nullopt;
    kind = *normalized;
    const auto revision_before = document.revision;
    auto& after = document; document_edit_detail::NodeAllocator allocator(after);
    auto first = selection.anchor.container_id;
    auto last = selection.active.container_id;
    const auto first_callout = document_structure_detail::nearest_container(
        after, first, {BlockKind::Callout});
    const auto last_callout = document_structure_detail::nearest_container(
        after, last, {BlockKind::Callout});
    if (first_callout && first_callout == last_callout) first = last = *first_callout;
    auto range = document_structure_detail::direct_range(
        after, first, last);
    if (!range) return std::nullopt;
    auto* parent = block_at_path(after.root, range->parent_path);
    if (!parent || range->last >= parent->children.size()) return std::nullopt;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(after, operations, revision_before);
    if (range->first == range->last && parent->children[range->first].kind == BlockKind::Callout) {
        const auto callout_id = parent->children[range->first].id;
        auto* callout = find_block(after.root, callout_id);
        if (!callout) return std::nullopt;
        if (callout->container_special().callout_kind != kind) {
            DocumentTreeEdit update;
            update.kind = DocumentTreeEditKind::UpdatePayload;
            update.before = document_transaction_detail::payload_shell(*callout);
            callout->ensure_container_special().callout_kind = kind;
            callout->ensure_text_special().opening_marker = rewrite_callout_opening_marker(
                callout->text_special().opening_marker, kind);
            update.after = document_transaction_detail::payload_shell(*callout);
            operations.emplace_back(std::move(update));
        } else {
            if (auto* title = callout_title_block(*callout)) {
                DocumentTreeEdit update;
                update.kind = DocumentTreeEditKind::UpdatePayload;
                update.before = document_transaction_detail::payload_shell(*title);
                title->kind = BlockKind::Paragraph;
                update.after = document_transaction_detail::payload_shell(*title);
                operations.emplace_back(std::move(update));
            }
            if (!document_structure_detail::unwrap_container(after, *range, operations)) {
                return std::nullopt;
            }
        }
    } else {
        BlockNode callout;
        callout.id = allocator.allocate();
        callout.kind = BlockKind::Callout;
        callout.ensure_container_special().callout_kind = kind;
        if (!document_structure_detail::insert_container_and_move_range(
                after, *range, std::move(callout), operations)) return std::nullopt;
    }
    ++after.revision;
    rollback.commit();
    return make_recorded_document_transaction(
        std::move(operations), selection, selection,
        revision_before, after.revision, DocumentTransactionReason::Structure);
}

inline std::optional<DocumentTransaction> document_toggle_list(EditorDocument& document, const TextSelection& selection, document_edit_detail::ListStyle style) {
    const auto revision_before = document.revision;
    auto& after = document; document_edit_detail::NodeAllocator allocator(after);
    auto first = selection.anchor.container_id;
    auto last = selection.active.container_id;
    const auto first_list = document_structure_detail::nearest_container(
        after, first, {BlockKind::List, BlockKind::TaskList});
    const auto last_list = document_structure_detail::nearest_container(
        after, last, {BlockKind::List, BlockKind::TaskList});
    if (first_list && first_list == last_list) first = last = *first_list;
    auto range = document_structure_detail::direct_range(
        after, first, last);
    if (!range) return std::nullopt;
    auto* parent = block_at_path(after.root, range->parent_path);
    if (!parent || range->last >= parent->children.size()) return std::nullopt;
    const auto parent_id = parent->id;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(after, operations, revision_before);
    if (range->first == range->last
        && (parent->children[range->first].kind == BlockKind::List
            || parent->children[range->first].kind == BlockKind::TaskList)) {
        const auto list_id = parent->children[range->first].id;
        auto* list = find_block(after.root, list_id);
        if (!list) return std::nullopt;
        const auto wants_task = style == document_edit_detail::ListStyle::Task;
        const auto wants_ordered = style == document_edit_detail::ListStyle::Ordered;
        const auto item_kind = wants_task ? BlockKind::TaskListItem : BlockKind::ListItem;
        const auto same_style = list->list_special().ordered == wants_ordered
            && std::ranges::all_of(list->children, [&](const BlockNode& item) {
                return item.kind == item_kind;
            });
        if (!same_style) {
            // Selecting a different list command changes this list's style.
            // Only invoking its current style again unwraps it.
            DocumentTreeEdit update_list;
            update_list.kind = DocumentTreeEditKind::UpdatePayload;
            update_list.before = document_transaction_detail::payload_shell(*list);
            if (!wants_ordered || !list->list_special().ordered) {
                list->ensure_list_special().start = 1;
                list->ensure_list_special().delimiter = U'.';
            }
            list->kind = wants_task ? BlockKind::TaskList : BlockKind::List;
            list->ensure_list_special().ordered = wants_ordered;
            update_list.after = document_transaction_detail::payload_shell(*list);
            operations.emplace_back(std::move(update_list));
            for (std::size_t index = 0; index < list->children.size(); ++index) {
                auto& item = list->children[index];
                const auto checked = wants_task && item.kind == BlockKind::TaskListItem
                    && item.item_special().checked;
                BlockNode marker_item;
                marker_item.kind = item_kind;
                marker_item.ensure_item_special().checked = checked;
                const auto marker = document_input_rules::detail::marker_for_destination(
                    *list, marker_item, index);
                DocumentTreeEdit update;
                update.kind = DocumentTreeEditKind::UpdatePayload;
                update.before = document_transaction_detail::payload_shell(item);
                document_input_rules::detail::set_item_marker(item, item_kind, checked, marker);
                update.after = document_transaction_detail::payload_shell(item);
                operations.emplace_back(std::move(update));
            }
            ++after.revision;
            rollback.commit();
            return make_recorded_document_transaction(
                std::move(operations), selection, selection,
                revision_before, after.revision, DocumentTransactionReason::Structure);
        }
        const auto item_count = list->children.size();
        std::size_t parent_index = range->first;
        for (std::size_t item_index = 0; item_index < item_count; ++item_index) {
            list = find_block(after.root, list_id);
            if (!list || list->children.empty()) return std::nullopt;
            const auto item_id = list->children.front().id;
            auto* item = find_block(after.root, item_id);
            if (!item) return std::nullopt;
            const auto child_count = item->children.size();
            for (std::size_t child_index = 0; child_index < child_count; ++child_index) {
                parent = find_block(after.root, parent_id);
                item = find_block(after.root, item_id);
                if (!parent || !item || item->children.empty()) return std::nullopt;
                DocumentTreeEdit move;
                move.kind = DocumentTreeEditKind::Move;
                move.parent_id = item_id;
                move.index = 0;
                move.other_parent_id = parent_id;
                move.other_index = parent_index++;
                move.moved_id = item->children.front().id;
                auto child = remove_block(*item, 0);
                if (!child || !insert_block(*parent, move.other_index, std::move(*child))) return std::nullopt;
                document_container_source::append_container_tree_operation(after, operations, std::move(move));
            }
            list = find_block(after.root, list_id);
            if (!list || list->children.empty()) return std::nullopt;
            DocumentTreeEdit remove_item;
            remove_item.kind = DocumentTreeEditKind::Remove;
            remove_item.parent_id = list_id;
            remove_item.index = 0;
            remove_item.before = list->children.front();
        document_container_source::append_container_tree_operation(after, operations, std::move(remove_item));
            list->children.erase(list->children.begin());
        }
        parent = find_block(after.root, parent_id);
        if (!parent || parent_index >= parent->children.size()) return std::nullopt;
        DocumentTreeEdit remove_list;
        remove_list.kind = DocumentTreeEditKind::Remove;
        remove_list.parent_id = parent_id;
        remove_list.index = parent_index;
        remove_list.before = parent->children[parent_index];
        document_container_source::append_container_tree_operation(after, operations, std::move(remove_list));
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(parent_index));
    } else {
        BlockNode list;
        list.id = allocator.allocate();
        list.kind = style == document_edit_detail::ListStyle::Task
            ? BlockKind::TaskList : BlockKind::List;
        list.ensure_list_special().ordered = style == document_edit_detail::ListStyle::Ordered;
        const auto list_id = list.id;
        DocumentTreeEdit insert_list;
        insert_list.kind = DocumentTreeEditKind::Insert;
        insert_list.parent_id = parent_id;
        insert_list.index = range->first;
        insert_list.after = list;
        operations.emplace_back(std::move(insert_list));
        if (!insert_block(*parent, range->first, std::move(list))) return std::nullopt;

        const auto count = range->last - range->first + 1;
        for (std::size_t item_index = 0; item_index < count; ++item_index) {
            auto* inserted_list = find_block(after.root, list_id);
            parent = find_block(after.root, parent_id);
            if (!inserted_list || !parent || range->first + 1 >= parent->children.size()) {
                return std::nullopt;
            }
            BlockNode item;
            item.id = allocator.allocate();
            item.kind = style == document_edit_detail::ListStyle::Task
                ? BlockKind::TaskListItem : BlockKind::ListItem;
            item.ensure_item_special().marker = style == document_edit_detail::ListStyle::Task ? U"- [ ] "
                : style == document_edit_detail::ListStyle::Ordered ? U"1. " : U"- ";
            const auto item_id = item.id;
            DocumentTreeEdit insert_item;
            insert_item.kind = DocumentTreeEditKind::Insert;
            insert_item.parent_id = list_id;
            insert_item.index = item_index;
            insert_item.after = item;
            operations.emplace_back(std::move(insert_item));
            if (!insert_block(*inserted_list, item_index, std::move(item))) return std::nullopt;

            DocumentTreeEdit move;
            move.kind = DocumentTreeEditKind::Move;
            move.parent_id = parent_id;
            move.index = range->first + 1;
            move.other_parent_id = item_id;
            move.other_index = 0;
            move.moved_id = parent->children[range->first + 1].id;
            auto child = remove_block(*parent, range->first + 1);
            auto* inserted_item = find_block(after.root, item_id);
            if (!child || !inserted_item || !insert_block(*inserted_item, 0, std::move(*child))) {
                return std::nullopt;
            }
        document_container_source::append_container_tree_operation(after, operations, std::move(move));
        }
    }
    ++after.revision;
    rollback.commit();
    return make_recorded_document_transaction(
        std::move(operations), selection, selection,
        revision_before, after.revision, DocumentTransactionReason::Structure);
}
using ListStyle = document_edit_detail::ListStyle;

inline std::optional<DocumentTransaction> document_toggle_task_checkbox(EditorDocument& document, const TextSelection& selection) {
    const auto revision_before = document.revision;
    auto& after = document;
    auto path = block_path(after.root, selection.active.container_id);
    if (!path) return std::nullopt;
    BlockNode* item = nullptr;
    while (!path->empty()) {
        auto* candidate = block_at_path(after.root, *path);
        if (candidate && candidate->kind == BlockKind::TaskListItem) {
            item = candidate;
            break;
        }
        path->pop_back();
    }
    if (!item) return std::nullopt;
    DocumentTreeEdit update;
    update.kind = DocumentTreeEditKind::UpdatePayload;
    update.before = document_transaction_detail::payload_shell(*item);
    auto& item_special = item->ensure_item_special();
    item_special.checked = !item_special.checked;
    const auto unchecked = item_special.marker.find(U"[ ]");
    const auto checked_lower = item_special.marker.find(U"[x]");
    const auto checked_upper = item_special.marker.find(U"[X]");
    if (item_special.checked && unchecked != std::u32string::npos) item_special.marker[unchecked + 1] = U'x';
    if (!item_special.checked && checked_lower != std::u32string::npos) item_special.marker[checked_lower + 1] = U' ';
    if (!item_special.checked && checked_upper != std::u32string::npos) item_special.marker[checked_upper + 1] = U' ';
    update.after = document_transaction_detail::payload_shell(*item);
    std::vector<DocumentOperation> operations;
    operations.emplace_back(std::move(update));
    ++after.revision;
    return make_recorded_document_transaction(
        std::move(operations),
        selection,
        selection,
        revision_before,
        after.revision,
        DocumentTransactionReason::Structure);
}

inline BlockNode make_code_block(std::optional<std::string> language = std::nullopt) {
    BlockNode block;
    block.kind = BlockKind::CodeBlock;
    auto source = U"```" + (language ? utf8_to_cps(*language) : std::u32string{}) + U"\n\n```";
    block.block_source = make_block_source(std::move(source), BlockSourceKind::FencedCode);
    return block;
}
inline BlockNode make_math_block() {
    BlockNode block;
    block.kind = BlockKind::MathBlock;
    block.ensure_atomic_special().math_delim = MathDelimiter::BlockDollar;
    block.block_source = make_block_source(U"$$\n\n$$", BlockSourceKind::DollarMath);
    return block;
}
inline BlockNode make_toc_block() { BlockNode block; block.kind = BlockKind::Toc; return block; }

inline BlockNode make_table_block(EditorDocument& document, std::size_t rows, std::size_t columns) {
    document_edit_detail::NodeAllocator allocator(document);
    BlockNode table; table.kind = BlockKind::Table; columns = (std::max)(columns, std::size_t{1}); table.ensure_table_special().table_aligns.assign(columns, TableAlignment::None);
    BlockNode header; header.id = allocator.allocate(); header.kind = BlockKind::TableRow; header.ensure_table_special().table_header_row = true;
    for (std::size_t column = 0; column < columns; ++column) {
        BlockNode cell; cell.id = allocator.allocate(); cell.kind = BlockKind::TableCell;
        cell.edit_inline_content() = document_edit_detail::make_inline(
            U"Header", document, allocator, InlineSyntaxMode::TableCell);
        header.children.push_back(std::move(cell));
    }
    table.children.push_back(std::move(header));
    for (std::size_t row_index = 0; row_index < rows; ++row_index) {
        BlockNode row; row.id = allocator.allocate(); row.kind = BlockKind::TableRow;
        for (std::size_t column = 0; column < columns; ++column) {
            BlockNode cell; cell.id = allocator.allocate(); cell.kind = BlockKind::TableCell;
            cell.edit_inline_content() = document_edit_detail::make_inline(
                U"Cell", document, allocator, InlineSyntaxMode::TableCell);
            row.children.push_back(std::move(cell));
        }
        table.children.push_back(std::move(row));
    }
    return table;
}

inline std::optional<DocumentTransaction> document_insert_atomic_block(EditorDocument& document, const TextSelection& selection, BlockNode block) {
    const auto revision_before = document.revision;
    auto& after = document;
    auto current = selection;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(after, operations, revision_before);
    if (!selection.is_caret()) {
        if ((block.kind == BlockKind::CodeBlock || block.kind == BlockKind::MathBlock)
            && block.block_source.tree().content.empty()) {
            const auto selected = document_selected_markdown(after, selection);
            if (!selected) return std::nullopt;
            // Fences must survive the selected literal source, including a
            // closing fence pasted from another code example.
            if (block.kind == BlockKind::CodeBlock || selected->find(U"$$") != std::u32string::npos) {
                std::size_t fence_length = 3;
                std::size_t run = 0;
                for (auto cp : *selected) {
                    run = cp == U'`' ? run + 1 : 0;
                    fence_length = (std::max)(fence_length, run + 1);
                }
                const auto fence = std::u32string(fence_length, U'`');
                const auto language = block.kind == BlockKind::MathBlock ? U"math"
                    : utf8_to_cps(block.block_source.tree().language.value_or(""));
                const auto kind = block.kind == BlockKind::MathBlock
                    ? BlockSourceKind::FencedMath : BlockSourceKind::FencedCode;
                block.block_source = make_block_source(
                    fence + language + U"\n" + *selected + U"\n" + fence, kind);
                if (block.kind == BlockKind::MathBlock) {
                    block.ensure_atomic_special().math_delim = MathDelimiter::FencedMath;
                }
            } else {
                block.block_source = make_block_source(
                    U"$$\n" + *selected + U"\n$$", BlockSourceKind::DollarMath);
                block.ensure_atomic_special().math_delim = MathDelimiter::BlockDollar;
            }
        }
        auto deleted = document_delete_selection(after, current);
        if (!deleted) return std::nullopt;
        current = deleted->selection_after;
        operations.insert(operations.end(),
            std::make_move_iterator(deleted->operations.begin()),
            std::make_move_iterator(deleted->operations.end()));
    }
    document_edit_detail::NodeAllocator allocator(after);
    reserve_document_node_ids(after, block);
    document_edit_detail::assign_missing_ids(block, after, allocator);
    auto path = document_block_path(after, current.active.container_id);
    if (!path || path->empty()) return std::nullopt;
    // A table row may contain only cells. Block insertion from a cell belongs
    // to the table's flow parent, never beside the cell inside its row.
    for (auto depth = path->size(); depth > 0; --depth) {
        BlockPath candidate(path->begin(), path->begin() + static_cast<std::ptrdiff_t>(depth));
        const auto* ancestor = block_at_path(after.root, candidate);
        if (ancestor && ancestor->kind == BlockKind::Table) {
            *path = std::move(candidate);
            break;
        }
    }
    auto* selected_block = block_at_path(after.root, *path);
    if (!selected_block) return std::nullopt;
    const auto replacement_in_text = !selection.is_caret()
        && document_edit_detail::text_block(selected_block->kind);
    const auto source_offset = replacement_in_text
        ? (std::min)(current.active.source_offset, selected_block->inline_content().source.size())
        : 0;
    if (replacement_in_text && source_offset > 0
        && source_offset < selected_block->inline_content().source.size()) {
        auto split = document_edit_detail::split_direct(
            after, selected_block->id, source_offset, allocator);
        if (!split) return std::nullopt;
        operations.insert(operations.end(),
            std::make_move_iterator(split->operations.begin()),
            std::make_move_iterator(split->operations.end()));
    }
    auto parent_path = *path;
    const auto index = parent_path.back();
    parent_path.pop_back();
    auto* parent = block_at_path(after.root, parent_path);
    if (!parent || index >= parent->children.size()) return std::nullopt;
    const auto inserted_id = block.id;
    auto inserted_target = document_edit_detail::first_editable_position(block)
        .value_or(TextPosition{block.id, 0, TextAffinity::Downstream});
    if ((block.kind == BlockKind::CodeBlock || block.kind == BlockKind::MathBlock)
        && !block.block_source.tree().content_to_source.empty()) {
        inserted_target = {
            block.id,
            block.block_source.tree().content_to_source.front(),
            TextAffinity::Downstream,
        };
    }
    const auto& existing = parent->children[index];
    const auto replace_empty = ((existing.kind == BlockKind::Paragraph || replacement_in_text)
            && existing.inline_content().source.empty())
        || (!selection.is_caret()
            && (existing.kind == BlockKind::CodeBlock || existing.kind == BlockKind::MathBlock)
            && existing.block_source.tree().content.empty());
    if (replace_empty) {
        const auto preserved_id = parent->children[index].id;
        block.id = preserved_id;
        if (inserted_target.container_id == inserted_id) {
            inserted_target.container_id = preserved_id;
        }
        DocumentTreeEdit remove;
        remove.kind = DocumentTreeEditKind::Remove;
        remove.parent_id = parent->id;
        remove.index = index;
        remove.before = parent->children[index];
        document_container_source::append_container_tree_operation(after, operations, std::move(remove));
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index));

        DocumentTreeEdit insert;
        insert.kind = DocumentTreeEditKind::Insert;
        insert.parent_id = parent->id;
        insert.index = index;
        insert.after = block;
        operations.emplace_back(std::move(insert));
        if (!insert_block(*parent, index, std::move(block))) return std::nullopt;
    } else {
        const auto insert_index = replacement_in_text && source_offset == 0 ? index : index + 1;
        DocumentTreeEdit insert;
        insert.kind = DocumentTreeEditKind::Insert;
        insert.parent_id = parent->id;
        insert.index = insert_index;
        insert.after = block;
        operations.emplace_back(std::move(insert));
        if (!insert_block(*parent, insert_index, std::move(block))) return std::nullopt;
    }
    after.revision = revision_before + 1;
    rollback.commit();
    return make_recorded_document_transaction(
        std::move(operations),
        selection,
        TextSelection::caret(inserted_target),
        revision_before,
        after.revision,
        DocumentTransactionReason::Structure);
}

inline std::optional<DocumentTransaction> document_indent_list_item(EditorDocument& document, const TextSelection& selection) {
    const auto revision_before = document.revision;
    auto& after = document;
    const auto range = document_structure_detail::selected_list_items(after, selection);
    if (!range) return std::nullopt;
    const auto item_index = range->first;
    const auto count = range->last - range->first + 1;
    auto* list = block_at_path(after.root, range->parent_path);
    if (!list || (list->kind != BlockKind::List && list->kind != BlockKind::TaskList) || item_index == 0) return std::nullopt;
    document_edit_detail::NodeAllocator allocator(after);
    const auto list_id = list->id;
    const auto previous_id = list->children[item_index - 1].id;
    auto* previous = &list->children[item_index - 1];
    auto nested_path = range->parent_path;
    nested_path.push_back(item_index - 1);
    BlockNode* nested = nullptr;
    if (!previous->children.empty()
        && document_input_rules::detail::same_list_style(previous->children.back(), *list)) {
        nested = &previous->children.back();
    }
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(after, operations, revision_before);
    if (!nested) {
        BlockNode created;
        created.id = allocator.allocate();
        created.kind = list->kind;
        created.ensure_list_special().ordered = list->list_special().ordered;
        // A new nested ordered list must begin at one to interrupt the
        // preceding item paragraph in CommonMark. Reusing the outer list's
        // start (for example 5) would serialize as a lazy continuation.
        created.ensure_list_special().start = 1;
        created.ensure_list_special().delimiter = list->list_special().delimiter;
        created.ensure_list_special().tight = list->list_special().tight;
        DocumentTreeEdit insert;
        insert.kind = DocumentTreeEditKind::Insert;
        insert.parent_id = previous_id;
        insert.index = previous->children.size();
        insert.after = created;
        operations.emplace_back(std::move(insert));
        previous->children.push_back(std::move(created));
        nested = &previous->children.back();
    }
    if (!nested) return std::nullopt;
    nested_path.push_back(previous->children.size() - 1);
    const auto nested_id = nested->id;
    for (std::size_t index = 0; index < count; ++index) {
        list = block_at_path(after.root, range->parent_path);
        nested = block_at_path(after.root, nested_path);
        if (!list || !nested || item_index >= list->children.size()) return std::nullopt;
        const auto target_index = nested->children.size();
        const auto marker = document_input_rules::detail::marker_for_destination(
            *nested, list->children[item_index], target_index);
        if (list->children[item_index].item_special().marker != marker) {
            auto& item = list->children[item_index];
            DocumentTreeEdit update;
            update.kind = DocumentTreeEditKind::UpdatePayload;
            update.before = document_transaction_detail::payload_shell(item);
            document_input_rules::detail::set_item_marker(
                item, item.kind, item.item_special().checked, marker);
            update.after = document_transaction_detail::payload_shell(item);
            operations.emplace_back(std::move(update));
        }
        DocumentTreeEdit move;
        move.kind = DocumentTreeEditKind::Move;
        move.parent_id = list_id;
        move.index = item_index;
        move.other_parent_id = nested_id;
        move.other_index = target_index;
        move.moved_id = list->children[item_index].id;
        auto item = remove_block(*list, item_index);
        if (!item || !insert_block(*nested, target_index, std::move(*item))) return std::nullopt;
        document_container_source::append_container_tree_operation(after, operations,
            std::move(move), range->parent_path, nested_path, &nested->children[target_index]);
    }
    ++after.revision;
    rollback.commit();
    return make_recorded_document_transaction(
        std::move(operations), selection, selection,
        revision_before, after.revision, DocumentTransactionReason::Structure);
}

inline std::optional<DocumentTransaction> document_outdent_list_item(EditorDocument& document, const TextSelection& selection) {
    const auto revision_before = document.revision;
    auto& after = document;
    const auto range = document_structure_detail::selected_list_items(after, selection);
    if (!range) return std::nullopt;
    const auto* list = block_at_path(after.root, range->parent_path);
    if (!list || range->last >= list->children.size()) return std::nullopt;
    std::vector<NodeId> items;
    for (auto index = range->first; index <= range->last; ++index) items.push_back(list->children[index].id);
    document_edit_detail::NodeAllocator allocator(after);
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(after, operations, revision_before);
    // Process the range from the end so only the last selected item carries
    // the following unselected siblings, preserving order and nesting.
    for (auto item = items.rbegin(); item != items.rend(); ++item) {
        auto recorded = document_input_rules::outdent_list_item(after, *item, allocator);
        if (!recorded || recorded->operations.empty()) return std::nullopt;
        operations.insert(operations.end(),
            std::make_move_iterator(recorded->operations.begin()),
            std::make_move_iterator(recorded->operations.end()));
    }
    ++after.revision;
    rollback.commit();
    return make_recorded_document_transaction(
        std::move(operations),
        selection,
        selection,
        revision_before,
        after.revision,
        DocumentTransactionReason::Structure);
}

inline std::optional<DocumentTransaction> document_edit_table(
    EditorDocument& document,
    const TextSelection& selection,
    DocumentTableEdit edit,
    TableAlignment alignment = TableAlignment::None,
    std::size_t argument = 0) {
    const auto revision_before = document.revision;
    auto& after = document;
    // Navigation starts at the active cell, including when a text range spans
    // several cells. Structural row/column edits still require one cell (or
    // an explicit handle that collapsed the selection to its source cell).
    const auto navigation = edit == DocumentTableEdit::MoveCellNext
        || edit == DocumentTableEdit::MoveCellPrevious;
    auto const target_cell = document_table_edit_target(after,
        navigation ? TextSelection::caret(selection.active) : selection);
    if (!target_cell) return std::nullopt;
    const auto cell_id = target_cell->cell_id;
    const auto column = target_cell->column;
    const auto row_index = target_cell->row;
    auto* table = find_block(after.root, target_cell->table_id);
    if (!table || table->kind != BlockKind::Table || table->children.empty()) return std::nullopt;
    document_edit_detail::NodeAllocator allocator(after);
    const auto column_count = table->children.front().children.size();
    if (column_count == 0) return std::nullopt;
    const auto header_rows = table->children.front().table_special().table_header_row
        ? std::size_t{1}
        : std::size_t{0};
    const auto cell_syntax_mode = table->has_html_source()
        ? InlineSyntaxMode::HtmlText
        : InlineSyntaxMode::TableCell;
    auto make_cell = [&] {
        BlockNode cell;
        cell.id = allocator.allocate();
        cell.kind = BlockKind::TableCell;
        cell.edit_inline_content() = document_edit_detail::make_inline(
            {},
            after,
            allocator,
            cell_syntax_mode);
        return cell;
    };
    auto make_row = [&] {
        BlockNode row;
        row.id = allocator.allocate();
        row.kind = BlockKind::TableRow;
        for (std::size_t index = 0; index < column_count; ++index) row.children.push_back(make_cell());
        return row;
    };
    auto target_id = cell_id;
    const auto table_id = table->id;
    const auto table_payload_before = document_transaction_detail::payload_shell(*table);
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(after, operations, revision_before);
    bool payload_changed = false;
    auto insert_node = [&](NodeId parent_id, std::size_t index, BlockNode node) {
        auto* owner = find_block(after.root, parent_id);
        if (!owner || index > owner->children.size()) return false;
        DocumentTreeEdit insert;
        insert.kind = DocumentTreeEditKind::Insert;
        insert.parent_id = parent_id;
        insert.index = index;
        insert.after = node;
        operations.emplace_back(std::move(insert));
        return insert_block(*owner, index, std::move(node));
    };
    auto insert_row = [&](std::size_t index) {
        auto row = make_row();
        if (index == 0 && header_rows > 0) {
            auto* current = find_block(after.root, table_id);
            if (!current || current->children.empty()) return false;
            auto& old_header = current->children.front();
            DocumentTreeEdit update;
            update.kind = DocumentTreeEditKind::UpdatePayload;
            update.before = document_transaction_detail::payload_shell(old_header);
            old_header.ensure_table_special().table_header_row = false;
            update.after = document_transaction_detail::payload_shell(old_header);
            operations.emplace_back(std::move(update));
            row.ensure_table_special().table_header_row = true;
        }
        return insert_node(table_id, index, std::move(row));
    };
    auto remove_node = [&](NodeId parent_id, std::size_t index) {
        auto* owner = find_block(after.root, parent_id);
        if (!owner || index >= owner->children.size()) return false;
        DocumentTreeEdit remove;
        remove.kind = DocumentTreeEditKind::Remove;
        remove.parent_id = parent_id;
        remove.index = index;
        remove.before = owner->children[index];
        document_container_source::append_container_tree_operation(after, operations, std::move(remove));
        return remove_block(*owner, index).has_value();
    };
    auto move_node = [&](NodeId parent_id, std::size_t from, std::size_t to) {
        auto* owner = find_block(after.root, parent_id);
        if (!owner || from >= owner->children.size()) return false;
        auto node = remove_block(*owner, from);
        if (!node || to > owner->children.size()) return false;
        DocumentTreeEdit move;
        move.kind = DocumentTreeEditKind::Move;
        move.parent_id = parent_id;
        move.index = from;
        move.other_parent_id = parent_id;
        move.other_index = to;
        move.moved_id = node->id;
        if (!insert_block(*owner, to, std::move(*node))) return false;
        document_container_source::append_container_tree_operation(after, operations, std::move(move));
        return true;
    };
    bool changed = true;
    switch (edit) {
        case DocumentTableEdit::MoveCellNext: {
            if (column + 1 < table->children[row_index].children.size()) target_id = table->children[row_index].children[column + 1].id;
            else if (row_index + 1 < table->children.size()) target_id = table->children[row_index + 1].children.front().id;
            else {
                // Match the established desktop-table convention: Tab from
                // the final cell appends a row and enters its first cell.
                auto row = make_row();
                target_id = row.children.front().id;
                if (!insert_node(table->id, table->children.size(), std::move(row))) {
                    return std::nullopt;
                }
            }
            // Navigation is selection-only; appending the terminal row is a
            // real tree transaction and therefore participates in history.
            changed = !operations.empty();
            break;
        }
        case DocumentTableEdit::MoveCellPrevious: {
            if (column > 0) target_id = table->children[row_index].children[column - 1].id;
            else if (row_index > 0) target_id = table->children[row_index - 1].children.back().id;
            else {
                const auto first = document_editable_order_position(after, cell_id);
                if (!first) return std::nullopt;
                if (*first > 0) {
                    const auto previous_id = after.cached_editable_order[*first - 1];
                    const auto* previous = find_document_block(after, previous_id);
                    const auto range = previous ? editable_block_source_range(*previous) : std::nullopt;
                    if (!range) return std::nullopt;
                    rollback.commit();
                    return make_recorded_document_transaction({}, selection,
                        TextSelection::caret({previous_id, range->end, TextAffinity::Upstream}),
                        revision_before, revision_before, DocumentTransactionReason::Structure);
                }

                // A table at the start of its document still needs an exit.
                // Create a real flow paragraph before the table in the same
                // parent, so Shift+Tab never falls through to text outdent.
                auto path = document_block_path(after, table_id);
                if (!path || path->empty()) return std::nullopt;
                const auto table_index = path->back();
                path->pop_back();
                auto* parent = block_at_path(after.root, *path);
                if (!parent) return std::nullopt;
                auto syntax_mode = InlineSyntaxMode::Markdown;
                const auto* context = &after.root;
                for (const auto child_index : *path) {
                    context = &context->children[child_index];
                    if (context->kind == BlockKind::HtmlContainer || context->has_html_source()
                        || context->has_html_element_provenance()) syntax_mode = InlineSyntaxMode::HtmlText;
                }
                auto paragraph = document_edit_detail::empty_paragraph(allocator, after, syntax_mode);
                target_id = paragraph.id;
                if (!insert_node(parent->id, table_index, std::move(paragraph))) return std::nullopt;
                ++after.revision;
                rollback.commit();
                return make_recorded_document_transaction(std::move(operations), selection,
                    TextSelection::caret({target_id, 0, TextAffinity::Downstream}),
                    revision_before, after.revision, DocumentTransactionReason::Structure);
            }
            changed = false;
            break;
        }
        case DocumentTableEdit::InsertRowAbove:
            if (!insert_row(row_index)) return std::nullopt;
            break;
        case DocumentTableEdit::InsertRowBelow:
            if (!insert_row(row_index + 1)) return std::nullopt;
            break;
        case DocumentTableEdit::InsertRowAt: {
            const auto index = (std::min)(argument, table->children.size());
            if (!insert_row(index)) return std::nullopt;
            break;
        }
        case DocumentTableEdit::DeleteRow:
            if (row_index < header_rows || table->children.size() <= 1) return std::nullopt;
            if (!remove_node(table_id, row_index)) return std::nullopt;
            table = find_block(after.root, table_id);
            if (!table) return std::nullopt;
            target_id = table->children[
                row_index == 0
                    ? 0
                    : (std::min)(row_index - 1, table->children.size() - 1)]
                .children[(std::min)(column, column_count - 1)].id;
            break;
        case DocumentTableEdit::MoveRowUp:
            if (row_index <= header_rows) return std::nullopt;
            if (!move_node(table_id, row_index, row_index - 1)) return std::nullopt;
            break;
        case DocumentTableEdit::MoveRowDown:
            if (row_index < header_rows || row_index + 1 >= table->children.size()) return std::nullopt;
            if (!move_node(table_id, row_index, row_index + 1)) return std::nullopt;
            break;
        case DocumentTableEdit::MoveRowTo: {
            if (row_index < header_rows || argument < header_rows
                || argument >= table->children.size()) return std::nullopt;
            if (!move_node(table_id, row_index, argument)) return std::nullopt;
            break;
        }
        case DocumentTableEdit::InsertColumnLeft:
        case DocumentTableEdit::InsertColumnRight:
        case DocumentTableEdit::InsertColumnAt: {
            const auto index = edit == DocumentTableEdit::InsertColumnLeft ? column
                : edit == DocumentTableEdit::InsertColumnRight ? column + 1
                : (std::min)(argument, column_count);
            std::vector<NodeId> row_ids;
            for (const auto& row : table->children) row_ids.push_back(row.id);
            for (auto row_id : row_ids) {
                if (!insert_node(row_id, index, make_cell())) return std::nullopt;
            }
            table = find_block(after.root, table_id);
            if (!table) return std::nullopt;
            auto& aligns = table->ensure_table_special().table_aligns;
            aligns.insert(aligns.begin() + static_cast<std::ptrdiff_t>((std::min)(index, aligns.size())), TableAlignment::None);
            payload_changed = true;
            break;
        }
        case DocumentTableEdit::DeleteColumn: {
            if (column_count <= 1) return std::nullopt;
            {
                std::vector<NodeId> row_ids;
                for (const auto& row : table->children) row_ids.push_back(row.id);
                for (auto row_id : row_ids) {
                    if (!remove_node(row_id, column)) return std::nullopt;
                }
            }
            table = find_block(after.root, table_id);
            if (!table) return std::nullopt;
            auto& aligns = table->ensure_table_special().table_aligns;
            if (column < aligns.size()) aligns.erase(aligns.begin() + static_cast<std::ptrdiff_t>(column));
            payload_changed = true;
            target_id = table->children[row_index].children[(std::min)(column, column_count - 2)].id;
            break;
        }
        case DocumentTableEdit::MoveColumnLeft: {
            if (column == 0) return std::nullopt;
            {
                std::vector<NodeId> row_ids;
                for (const auto& row : table->children) row_ids.push_back(row.id);
                for (auto row_id : row_ids) {
                    if (!move_node(row_id, column, column - 1)) return std::nullopt;
                }
            }
            table = find_block(after.root, table_id);
            if (!table) return std::nullopt;
            auto& aligns = table->ensure_table_special().table_aligns;
            if (column < aligns.size()) std::swap(aligns[column], aligns[column - 1]);
            payload_changed = true;
            break;
        }
        case DocumentTableEdit::MoveColumnRight: {
            if (column + 1 >= column_count) return std::nullopt;
            {
                std::vector<NodeId> row_ids;
                for (const auto& row : table->children) row_ids.push_back(row.id);
                for (auto row_id : row_ids) {
                    if (!move_node(row_id, column, column + 1)) return std::nullopt;
                }
            }
            table = find_block(after.root, table_id);
            if (!table) return std::nullopt;
            auto& aligns = table->ensure_table_special().table_aligns;
            if (column + 1 < aligns.size()) std::swap(aligns[column], aligns[column + 1]);
            payload_changed = true;
            break;
        }
        case DocumentTableEdit::MoveColumnTo: {
            if (argument >= column_count || argument == column) return std::nullopt;
            std::vector<NodeId> row_ids;
            for (const auto& row : table->children) row_ids.push_back(row.id);
            for (auto row_id : row_ids) {
                if (!move_node(row_id, column, argument)) return std::nullopt;
            }
            table = find_block(after.root, table_id);
            if (!table) return std::nullopt;
            auto& aligns = table->ensure_table_special().table_aligns;
            auto value = aligns[column];
            aligns.erase(aligns.begin() + static_cast<std::ptrdiff_t>(column));
            aligns.insert(aligns.begin() + static_cast<std::ptrdiff_t>(argument), value);
            payload_changed = true;
            break;
        }
        case DocumentTableEdit::SetColumnAlignment:
            if (column >= table->table_special().table_aligns.size()) {
                table->ensure_table_special().table_aligns.resize(column_count, TableAlignment::None);
            }
            table->ensure_table_special().table_aligns[column] = alignment;
            payload_changed = true;
            break;
        case DocumentTableEdit::Normalize: {
            auto columns = (std::max)(std::size_t{1}, column_count);
            std::vector<NodeId> row_ids;
            for (const auto& row : table->children) row_ids.push_back(row.id);
            for (auto row_id : row_ids) {
                auto* row = find_block(after.root, row_id);
                if (!row) return std::nullopt;
                while (row->children.size() < columns) {
                    if (!insert_node(row_id, row->children.size(), make_cell())) return std::nullopt;
                    row = find_block(after.root, row_id);
                    if (!row) return std::nullopt;
                }
                while (row->children.size() > columns) {
                    if (!remove_node(row_id, row->children.size() - 1)) return std::nullopt;
                    row = find_block(after.root, row_id);
                    if (!row) return std::nullopt;
                }
            }
            table = find_block(after.root, table_id);
            if (!table) return std::nullopt;
            table->ensure_table_special().table_aligns.resize(columns, TableAlignment::None);
            payload_changed = true;
            break;
        }
    }
    if (changed) {
        table = find_block(after.root, table_id);
        if (!table) return std::nullopt;
        auto& table_special = table->ensure_table_special();
        if (!table_special.table_separator_source.empty()
            || !table_special.table_internal_line_endings.empty()) {
            table_special.table_separator_source.clear();
            table_special.table_internal_line_endings.clear();
            payload_changed = true;
        }
    }
    if (payload_changed) {
        table = find_block(after.root, table_id);
        if (!table) return std::nullopt;
        DocumentTreeEdit update;
        update.kind = DocumentTreeEditKind::UpdatePayload;
        update.before = table_payload_before;
        update.after = document_transaction_detail::payload_shell(*table);
        operations.emplace_back(std::move(update));
    }
    if (changed) ++after.revision;
    const auto target = TextSelection::caret(TextPosition{target_id, 0, TextAffinity::Downstream});
    rollback.commit();
    return make_recorded_document_transaction(
        std::move(operations),
        selection,
        target,
        revision_before,
        after.revision,
        DocumentTransactionReason::Structure);
}


} // namespace folia
