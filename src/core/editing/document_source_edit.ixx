export module folia.core.document_source_edit;
import std;
import folia.markdown.block_source;
import folia.markdown.ids;
import folia.markdown.selection;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.core.table_selection;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.markdown_html;
import folia.core.inline_source_edit;
import folia.markdown.text_edit;
import folia.markdown.utf;
import folia.markdown.serializer;
import folia.core.document_edit_support;
import folia.core.document_editable_fallback;
import folia.core.document_container_source;
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

inline std::optional<DocumentTransaction> document_delete_selection(
    EditorDocument&, const TextSelection&, TextPosition* tracked = nullptr);
inline std::optional<TextSelection> document_move_selection(
    const EditorDocument&, const TextSelection&, DocumentMove, bool);

inline std::optional<DocumentTransaction> document_delete_marker(
    EditorDocument& document, const TextSelection& selection, bool backward,
    TextPosition* tracked = nullptr) {
    if (auto inline_edit = document_delete_inline_marker(document, selection, backward, tracked)) return inline_edit;
    const auto revision = document.revision;
    document_edit_detail::NodeAllocator allocator(document);
    auto block_edit = document_edit_detail::unwrap_fenced_math_marker(document, selection, backward, allocator, tracked);
    if (!block_edit) return std::nullopt;
    ++document.revision;
    return make_recorded_document_transaction(std::move(block_edit->operations), selection,
        TextSelection::caret(block_edit->target), revision, document.revision, DocumentTransactionReason::Delete);
}

namespace document_edit_detail {

inline void track_source_edit(TextPosition* tracked, const TextEdit& edit) {
    if (tracked && tracked->container_id == edit.container_id)
        tracked->source_offset = translate_offset(tracked->source_offset, tracked->affinity, edit);
}

inline void track_source_operations(TextPosition* tracked, std::span<const DocumentOperation> operations) {
    if (!tracked) return;
    for (const auto& operation : operations)
        if (const auto* edit = std::get_if<DocumentTextOperation>(&operation))
            track_source_edit(tracked, edit->forward);
}

inline bool math_without_opener(const BlockNode& block) {
    return block_source_has_math_delimiters(block.block_source)
        && !block.block_source.tree().complete_opening;
}

inline void reclassify_math_without_opener(
    EditorDocument& document, std::span<const NodeId> owners, NodeAllocator& allocator,
    TextPosition& target, TextPosition* tracked, std::vector<DocumentOperation>& operations) {
    // Later replacements cannot invalidate the cached paths of earlier owners.
    for (auto id : std::views::reverse(owners)) {
        const auto position = target.container_id == id ? target : TextPosition{id, 0};
        auto edit = reparse_edited_direct_block(document, position, allocator, nullptr, tracked);
        if (!edit) continue;
        if (target.container_id == id) target = edit->target;
        operations.insert(operations.end(), std::make_move_iterator(edit->operations.begin()),
            std::make_move_iterator(edit->operations.end()));
    }
}

} // namespace document_edit_detail

inline DocumentTransaction source_transaction_with_block_reparse(
    EditorDocument& after,
    DocumentSourceEdit edit,
    TextSelection selection_before,
    TextPosition target,
    std::uint64_t revision_before,
    DocumentTransactionReason reason,
    TextPosition* tracked = nullptr) {
    std::vector<DocumentOperation> operations;
    const auto reparse_edit = edit;
    document_edit_detail::track_source_edit(tracked, edit.forward);
    document_edit_detail::append_source_operation(operations, std::move(edit));
    document_edit_detail::MutationRollback rollback(after, operations, revision_before);
    document_edit_detail::NodeAllocator allocator(after);
    if (auto reclassified = document_edit_detail::reparse_edited_direct_block(
            after, target, allocator, &reparse_edit, tracked)) {
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
    // A rendered atomic owner has two source boundaries but no inline body.
    // Continue typing in a real sibling paragraph at that boundary, recording
    // its creation together with the text edit in the same undo transaction.
    if (auto boundary = document_input_rules::ensure_atomic_boundary_paragraph(
            working, current.active, allocator)) {
        current = TextSelection::caret(boundary->target);
        operations.insert(operations.end(),
            std::make_move_iterator(boundary->operations.begin()),
            std::make_move_iterator(boundary->operations.end()));
    }
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

// Literal block-local source replacement with an explicit resulting selection.
// Unlike typing, this does not apply input rules or insert fence separators.
inline std::optional<DocumentTransaction> document_replace_text(
    EditorDocument& document, TextSelection before, TextSpan range,
    std::u32string_view replacement, TextSelection after) {
    auto block = find_document_block(document, range.container_id);
    if (!block) return std::nullopt;
    auto revision = document.revision;
    document_edit_detail::NodeAllocator allocator(document);
    auto edit = editable_raw_block_source(*block)
        ? document_edit_detail::edit_block_source(document, *block, range.source_range, std::u32string{replacement}, allocator)
        : document_edit_detail::edit_inline(document, range.container_id, range.source_range, std::u32string{replacement}, allocator);
    if (!edit) return std::nullopt;
    std::vector<DocumentOperation> operations;
    document_edit_detail::append_source_operation(operations, std::move(*edit));
    ++document.revision;
    return make_recorded_document_transaction(std::move(operations), before, after,
        revision, document.revision, DocumentTransactionReason::Replace);
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
    } else if (auto exited_raw = document_edit_detail::enter_at_raw_block_boundary(
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
    } else if (auto* block = find_document_block(after, current.active.container_id);
               block && (block->kind == BlockKind::CodeBlock || block->kind == BlockKind::MathBlock)) {
        auto inserted = document_edit_detail::insert_raw_line_break(after, *block, current.active, allocator);
        if (!inserted) return std::nullopt;
        source_edit = std::move(inserted->source_edit);
        target = current.active;
        target.source_offset = inserted->offset;
        target.affinity = inserted->affinity;
    } else if (block && block->kind == BlockKind::TableCell) {
        const auto offset = (std::min)(current.active.source_offset, block->inline_content().source.size());
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

namespace document_inline_insertion_detail {

// Collect source only from the visible range of each selected owner. Image
// replacement uses this for its default alt text, without exposing raw fences
// or the atomic navigation placeholder in generated Markdown.
inline std::optional<std::u32string> selected_label_source(
    const EditorDocument& document, const TextSelection& selection, bool image_alt = false) {
    auto first_index = document_editable_order_position(document, selection.anchor.container_id);
    auto last_index = document_editable_order_position(document, selection.active.container_id);
    if (!first_index || !last_index) return std::nullopt;
    auto first = selection.anchor;
    auto last = selection.active;
    if (*first_index > *last_index
        || (*first_index == *last_index && first.source_offset > last.source_offset)) {
        std::swap(first_index, last_index);
        std::swap(first, last);
    }
    std::u32string result;
    for (auto index = *first_index; index <= *last_index; ++index) {
        const auto* block = find_document_block(document, document.cached_editable_order[index]);
        const auto visible = block ? editable_block_source_range(*block) : std::nullopt;
        const auto source = block ? editable_block_text_view(*block) : std::nullopt;
        if (!block || !visible || !source) return std::nullopt;
        const auto start = index == *first_index
            ? (std::clamp)(first.source_offset, visible->start, visible->end) : visible->start;
        const auto end = index == *last_index
            ? (std::clamp)(last.source_offset, visible->start, visible->end) : visible->end;
        if (index != *first_index) result += U'\n';
        if (start == end) continue;
        if (document_edit_detail::atomic_block(block->kind)) {
            if (image_alt && block->kind == BlockKind::ImageBlock)
                result += utf8_to_cps(block->image_special().image_alt);
            else {
                BlockVec fragment;
                fragment.push_back(*block);
                fragment.front().separator_before.reset();
                result += serialize_markdown_fragment(fragment);
            }
        } else {
            result.append(source->substr(start, end - start));
        }
    }
    return result;
}

inline std::u32string single_line_label(std::u32string_view source) {
    std::u32string result;
    result.reserve(source.size());
    for (std::size_t index = 0; index < source.size(); ++index) {
        if (source[index] == U'\r' || source[index] == U'\n') {
            if (source[index] == U'\r' && index + 1 < source.size() && source[index + 1] == U'\n') ++index;
            result += U' ';
        } else result += source[index];
    }
    return result;
}

// Generated inline syntax edits one local source. A selected atomic block is
// replaced in its own flow position; an atomic caret obtains a neighboring
// paragraph. Raw owners receive literal syntax inside their content ranges.
template<class BuildSource>
inline std::optional<DocumentTransaction> insert(
    EditorDocument& document, const TextSelection& selection, BuildSource build_source) {
    if (selection.anchor.container_id != selection.active.container_id) return std::nullopt;
    const auto label = selected_label_source(document, selection);
    if (!label) return std::nullopt;
    const auto revision_before = document.revision;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(document, operations, revision_before);
    document_edit_detail::NodeAllocator allocator(document);
    auto current = selection;
    auto* selected_block = find_document_block(document, selection.active.container_id);
    if (!selected_block) return std::nullopt;
    if (!selection.is_caret() && document_edit_detail::atomic_block(selected_block->kind)) {
        auto path = document_block_path(document, selected_block->id);
        if (!path || path->empty()) return std::nullopt;
        const auto index = path->back();
        path->pop_back();
        auto* parent = block_at_path(document.root, *path);
        if (!parent) return std::nullopt;
        auto syntax_mode = InlineSyntaxMode::Markdown;
        const auto* context = &document.root;
        for (const auto child_index : *path) {
            context = &context->children[child_index];
            if (context->kind == BlockKind::HtmlContainer || context->has_html_source()
                || context->has_html_element_provenance()) syntax_mode = InlineSyntaxMode::HtmlText;
        }
        auto paragraph = document_edit_detail::empty_paragraph(allocator, document, syntax_mode);
        paragraph.id = selected_block->id;
        paragraph.separator_before = selected_block->separator_before;
        DocumentTreeEdit remove;
        remove.kind = DocumentTreeEditKind::Remove;
        remove.parent_id = parent->id;
        remove.index = index;
        remove.before = *selected_block;
        document_container_source::append_container_tree_operation(document, operations, std::move(remove));
        DocumentTreeEdit insert;
        insert.kind = DocumentTreeEditKind::Insert;
        insert.parent_id = parent->id;
        insert.index = index;
        insert.after = paragraph;
        operations.emplace_back(std::move(insert));
        parent->children[index] = std::move(paragraph);
        current = TextSelection::caret({selection.active.container_id, 0, TextAffinity::Downstream});
    }
    if (auto boundary = document_input_rules::ensure_atomic_boundary_paragraph(document, current.active, allocator)) {
        current = TextSelection::caret(boundary->target);
        operations.insert(operations.end(), std::make_move_iterator(boundary->operations.begin()),
            std::make_move_iterator(boundary->operations.end()));
    }
    auto* block = find_document_block(document, current.active.container_id);
    const auto visible = block ? editable_block_source_range(*block) : std::nullopt;
    const auto* owner = block ? editable_inline_document(*block) : nullptr;
    if (!block || !visible || (!owner && !editable_raw_block_source(*block))) return std::nullopt;
    const auto start = (std::clamp)((std::min)(current.anchor.source_offset, current.active.source_offset),
        visible->start, visible->end);
    const auto end = (std::clamp)((std::max)(current.anchor.source_offset, current.active.source_offset),
        visible->start, visible->end);
    auto [replacement, caret] = build_source(owner ? owner->syntax_mode : InlineSyntaxMode::Markdown,
        std::u32string_view{*label});
    auto caret_offset = start + caret;
    if (start == end && !owner) {
        const auto inserted_length = replacement.size();
        auto inserted = document_edit_detail::insert_text(document, current.active, replacement, allocator);
        if (!inserted || !inserted->source_edit) return std::nullopt;
        caret_offset = inserted->offset - inserted_length + caret;
        document_edit_detail::append_source_operation(operations, std::move(*inserted->source_edit));
    } else {
        auto applied = document_edit_detail::edit_block_source(document, block->id,
            {start, end}, std::move(replacement), allocator);
        if (!applied) return std::nullopt;
        document_edit_detail::append_source_operation(operations, std::move(*applied));
    }
    document.revision = revision_before + 1;
    rollback.commit();
    return make_recorded_document_transaction(std::move(operations), selection,
        TextSelection::caret({current.active.container_id, caret_offset, TextAffinity::Downstream}),
        revision_before, document.revision, DocumentTransactionReason::Format);
}

inline bool link_node(const InlineCstNode& node) {
    return node.status == ParseStatus::Complete && (node.kind == InlineCstKind::Autolink
        || (node.delimiter_ranges().closing && (node.kind == InlineCstKind::Link
            || (node.kind == InlineCstKind::HtmlElement && node.semantics().html_tag == "a"))));
}

template<class Visit>
inline void visit_links(const InlineCstNodes& nodes, Visit visit, bool include_image_labels = false) {
    std::vector<const InlineCstNode*> pending;
    for (const auto& node : nodes) pending.push_back(&node);
    while (!pending.empty()) {
        const auto* node = pending.back();
        pending.pop_back();
        if (link_node(*node)) visit(*node);
        // Caret targeting treats an image as a unit. Constructing an outer
        // link must also remove link wrappers from image alt labels, keeping
        // their text and image source without creating forbidden nested links.
        if (node->kind != InlineCstKind::Image || include_image_labels)
            for (const auto& child : node->children) pending.push_back(&child);
    }
}

inline const InlineCstNode* containing_link(const InlineDocument& owner, SourceRange selected) {
    const InlineCstNode* result = nullptr;
    visit_links(owner.tree.nodes, [&](const InlineCstNode& node) {
        const auto contains = selected.empty()
            ? node.range.start < selected.start && selected.start < node.range.end
            : node.range.start <= selected.start && selected.end <= node.range.end;
        if (contains && (!result || node.range.length() < result->range.length())) result = &node;
    });
    return result;
}

inline SourceRange expand_link_boundaries(const InlineDocument& owner, SourceRange selected) {
    visit_links(owner.tree.nodes, [&](const InlineCstNode& node) {
        if (node.range.start < selected.end && selected.start < node.range.end) {
            selected.start = (std::min)(selected.start, node.range.start);
            selected.end = (std::max)(selected.end, node.range.end);
        }
    });
    return selected;
}

inline std::u32string label_without_links(const InlineDocument& owner, SourceRange selected) {
    std::vector<SourceRange> wrappers;
    visit_links(owner.tree.nodes, [&](const InlineCstNode& node) {
        if (node.range.start < selected.start || node.range.end > selected.end) return;
        const auto& delimiters = node.delimiter_ranges();
        if (!delimiters.opening.empty()) wrappers.push_back(delimiters.opening);
        if (delimiters.closing && !delimiters.closing->empty()) wrappers.push_back(*delimiters.closing);
    }, true);
    std::ranges::sort(wrappers, std::greater{}, &SourceRange::start);
    auto result = owner.source.substr(selected.start, selected.length());
    for (const auto wrapper : wrappers)
        result.erase(wrapper.start - selected.start, wrapper.length());
    return result;
}

inline std::optional<DocumentTransaction> retarget_link(
    EditorDocument& document, const TextSelection& selection, const InlineDocument& owner,
    const InlineCstNode& node, std::u32string_view href, const std::optional<std::u32string>& title) {
    const auto id = selection.active.container_id;
    auto caret = node.delimiter_ranges().content.end;
    std::vector<TextEdit> edits;
    std::vector<TextEdit> appended_fields;
    const auto same_href = cps_to_utf8(href) == node.semantics().href;
    const auto same_title = title && node.semantics().title
        && cps_to_utf8(*title) == *node.semantics().title;
    if (const auto ranges = inline_link_target_ranges(owner.source, node)) {
        if (!same_href) {
            const auto angled = !ranges->destination.empty() && owner.source[ranges->destination.start] == U'<';
            auto destination = serialize_link_destination(href, angled);
            if (!destination.empty() && ranges->destination.empty() && ranges->title
                && ranges->title->start == ranges->destination.end) destination += U' ';
            edits.push_back({id, ranges->destination, std::move(destination)});
        }
        if (title && !same_title) {
            if (ranges->title) edits.push_back({id, *ranges->title, serialize_link_title(*title)});
            else appended_fields.push_back({id, {ranges->destination.end, ranges->destination.end},
                U" " + serialize_link_title(*title)});
        }
    } else if (node.kind == InlineCstKind::HtmlElement && node.semantics().html_tag == "a") {
        const auto opening = node.delimiter_ranges().opening;
        const auto tag = scan_markdown_html_tag(owner.source, opening.start, opening.end);
        if (!tag) return std::nullopt;
        const auto attribute_edit = [&](std::u32string_view name, std::u32string_view value) {
            const auto escaped = serialize_html_attribute_value(value);
            auto found = false;
            for (const auto& attribute : tag->attributes) {
                auto source_name = owner.source.substr(attribute.name.start, attribute.name.length());
                for (auto& ch : source_name) if (ch >= U'A' && ch <= U'Z') ch += U'a' - U'A';
                if (source_name != name) continue;
                found = true;
                if (!attribute.value) {
                    edits.push_back({id, {attribute.name.end, attribute.name.end}, U"=\"" + escaped + U"\""});
                } else {
                    const auto range = *attribute.value;
                    const auto quoted = range.start > 0 && range.end < owner.source.size()
                        && (owner.source[range.start - 1] == U'\'' || owner.source[range.start - 1] == U'\"')
                        && owner.source[range.start - 1] == owner.source[range.end];
                    edits.push_back({id, range, quoted ? escaped : U"\"" + escaped + U"\""});
                }
            }
            if (!found) {
                const auto at = opening.end - (tag->self_closing ? 2u : 1u);
                appended_fields.push_back({id, {at, at}, U" " + std::u32string(name) + U"=\"" + escaped + U"\""});
            }
        };
        if (!same_href || !node.semantics().html_attributes.contains("href")) attribute_edit(U"href", href);
        if (title && !same_title) attribute_edit(U"title", *title);
    } else return std::nullopt;

    // Apply later fields first. All ranges remain in the original local
    // source coordinate. New attributes/titles at a field's empty boundary
    // follow that field's value, regardless of which attribute was missing.
    std::ranges::reverse(appended_fields);
    edits.insert(edits.begin(), std::make_move_iterator(appended_fields.begin()),
        std::make_move_iterator(appended_fields.end()));
    std::ranges::stable_sort(edits, [](const auto& left, const auto& right) {
        return left.range.start != right.range.start ? left.range.start > right.range.start
            : left.range.end > right.range.end;
    });
    std::erase_if(edits, [&](const auto& edit) {
        return owner.source.substr(edit.range.start, edit.range.length()) == edit.replacement;
    });
    const auto revision_before = document.revision;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(document, operations, revision_before);
    document_edit_detail::NodeAllocator allocator(document);
    for (auto& edit : edits) {
        caret = translate_offset(caret, TextAffinity::Upstream, edit);
        auto applied = document_edit_detail::edit_inline(document, id, edit.range, std::move(edit.replacement), allocator);
        if (!applied) return std::nullopt;
        document_edit_detail::append_source_operation(operations, std::move(*applied));
    }
    if (!operations.empty()) ++document.revision;
    rollback.commit();
    return make_recorded_document_transaction(std::move(operations), selection,
        TextSelection::caret({id, caret, TextAffinity::Downstream}),
        revision_before, document.revision, DocumentTransactionReason::Format);
}

} // namespace document_inline_insertion_detail

inline std::optional<DocumentTransaction> document_insert_link(EditorDocument& document, const TextSelection& selection, std::string href, std::optional<std::string> title) {
    if (selection.anchor.container_id != selection.active.container_id) {
        auto first_index = document_editable_order_position(document, selection.anchor.container_id);
        auto last_index = document_editable_order_position(document, selection.active.container_id);
        if (!first_index || !last_index) return std::nullopt;
        auto first = selection.anchor;
        auto last = selection.active;
        if (*first_index > *last_index) { std::swap(first_index, last_index); std::swap(first, last); }
        std::vector<TextSelection> selected;
        for (auto index = *first_index; index <= *last_index; ++index) {
            const auto id = document.cached_editable_order[index];
            const auto* block = find_document_block(document, id);
            const auto visible = block ? editable_block_source_range(*block) : std::nullopt;
            if (!visible) return std::nullopt;
            const auto start = index == *first_index
                ? (std::clamp)(first.source_offset, visible->start, visible->end) : visible->start;
            const auto end = index == *last_index
                ? (std::clamp)(last.source_offset, visible->start, visible->end) : visible->end;
            if (start != end) selected.push_back({{id, start}, {id, end}});
        }
        if (selected.empty()) selected.push_back(TextSelection::caret(first));
        const auto revision_before = document.revision;
        std::vector<DocumentOperation> operations;
        document_edit_detail::MutationRollback rollback(document, operations, revision_before);
        auto after = selection;
        for (const auto& local : selected) {
            auto insertion = document_insert_link(document, local, href, title);
            if (!insertion) return std::nullopt;
            after = insertion->selection_after;
            operations.insert(operations.end(), std::make_move_iterator(insertion->operations.begin()),
                std::make_move_iterator(insertion->operations.end()));
        }
        document.revision = revision_before + (operations.empty() ? 0u : 1u);
        rollback.commit();
        return make_recorded_document_transaction(std::move(operations), selection, after,
            revision_before, document.revision, DocumentTransactionReason::Format);
    }

    const auto source_href = utf8_to_cps(href);
    const auto source_title = title ? std::optional{utf8_to_cps(*title)} : std::nullopt;
    auto local = selection;
    std::optional<std::u32string> label_source;
    const auto* block = find_document_block(document, selection.active.container_id);
    const auto* owner = block ? editable_inline_document(*block) : nullptr;
    if (owner) {
        auto selected = SourceRange{(std::min)(selection.anchor.source_offset, selection.active.source_offset),
            (std::max)(selection.anchor.source_offset, selection.active.source_offset)};
        if (!selected.valid_for(owner->source.size())) return std::nullopt;
        if (const auto* existing = document_inline_insertion_detail::containing_link(*owner, selected)) {
            if (auto edit = document_inline_insertion_detail::retarget_link(
                    document, selection, *owner, *existing, source_href, source_title)) return edit;
            selected = existing->range;
        }
        if (!selected.empty()) {
            selected = document_inline_insertion_detail::expand_link_boundaries(*owner, selected);
            label_source = document_inline_insertion_detail::label_without_links(*owner, selected);
            local = {{block->id, selected.start}, {block->id, selected.end}};
        }
    } else if (block && block->kind == BlockKind::ImageBlock && !selection.is_caret()
        && !block->inline_content().source.empty()) {
        label_source = document_inline_insertion_detail::label_without_links(block->inline_content(),
            {0, block->inline_content().source.size()});
    }
    auto insertion = document_inline_insertion_detail::insert(document, local,
        [&](InlineSyntaxMode syntax_mode, std::u32string_view selected_source) {
            const auto label = label_source ? std::u32string_view{*label_source} : selected_source;
            const auto html = syntax_mode == InlineSyntaxMode::HtmlText;
            auto replacement = html ? serialize_html_link_source(source_href, label, source_title)
                : serialize_link_source(source_href, label, source_title);
            const auto caret = html ? replacement.size() - std::u32string_view{U"</a>"}.size()
                : 1 + serialize_link_label(label).size();
            return std::pair{std::move(replacement), caret};
        });
    if (insertion) insertion->selection_before = selection;
    return insertion;
}

inline std::optional<DocumentTransaction> document_insert_image(EditorDocument& document, const TextSelection& selection, std::string path, std::string alt) {
    const auto selected = alt.empty()
        ? document_inline_insertion_detail::selected_label_source(document, selection, true)
        : std::optional<std::u32string>{utf8_to_cps(alt)};
    if (!selected) return std::nullopt;
    const auto label = document_inline_insertion_detail::single_line_label(*selected);
    const auto destination = utf8_to_cps(path.empty() ? "image.png" : path);
    const auto build_source = [&](InlineSyntaxMode syntax_mode, std::u32string_view) {
        auto replacement = syntax_mode == InlineSyntaxMode::HtmlText
            ? serialize_html_image_source(destination, label)
            : serialize_image_source(destination, label, false);
        const auto caret = replacement.size();
        return std::pair{std::move(replacement), caret};
    };
    if (selection.anchor.container_id == selection.active.container_id)
        return document_inline_insertion_detail::insert(document, selection, build_source);

    const auto revision_before = document.revision;
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(document, operations, revision_before);
    auto deletion = document_delete_selection(document, selection);
    if (!deletion) return std::nullopt;
    operations = std::move(deletion->operations);
    auto insertion = document_inline_insertion_detail::insert(document, deletion->selection_after, build_source);
    if (!insertion) return std::nullopt;
    operations.insert(operations.end(), std::make_move_iterator(insertion->operations.begin()),
        std::make_move_iterator(insertion->operations.end()));
    document.revision = revision_before + 1;
    rollback.commit();
    return make_recorded_document_transaction(std::move(operations), selection,
        insertion->selection_after, revision_before, document.revision, DocumentTransactionReason::Format);
}

inline std::optional<DocumentTransaction> document_insert_soft_break(EditorDocument& document, const TextSelection& selection) {
    const auto* block = selection.anchor.container_id == selection.active.container_id
        ? find_document_block(document, selection.active.container_id) : nullptr;
    if (block && block->kind == BlockKind::TableCell) {
        return document_insert_text(document, selection, U"<br>");
    }
    return document_insert_text(document, selection, U"\n");
}

inline std::optional<TextEdit> plan_code_block_language_edit(
    const BlockNode& block,
    std::u32string language) {
    if (block.kind != BlockKind::CodeBlock
        || block.block_source.tree().kind != BlockSourceKind::FencedCode) {
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

    const auto& tree = block.block_source.tree();
    if (!tree.info_range) return std::nullopt;
    const auto& source = block.block_source.source();
    const auto marker = tree.info_range->start != 0 ? source[tree.info_range->start - 1] : U'\0';
    if (marker == U'`' && language.find(U'`') != std::u32string::npos) return std::nullopt;
    auto replacement_range = tree.language_range.value_or(
        SourceRange{tree.info_range->start, tree.info_range->start});
    if (language.empty() && tree.language_range) {
        // Without a first token, the remaining info string would immediately
        // become the language. Clearing the selector therefore clears the
        // complete info string rather than silently reclassifying auxiliary
        // metadata.
        replacement_range = *tree.info_range;
    }
    // A leading marker character must not extend the opening fence when the
    // original info token starts immediately after it (for example ~~~cpp).
    if (!language.empty() && language.front() == marker
        && replacement_range.start == tree.info_range->start) language.insert(language.begin(), U' ');
    if (source.substr(
            replacement_range.start,
            replacement_range.length()) == language) {
        return std::nullopt;
    }
    return TextEdit{block.id, replacement_range, std::move(language)};
}

inline std::optional<DocumentTransaction> document_set_code_block_language(
    EditorDocument& document,
    const TextSelection& selection,
    std::u32string language) {
    const auto* block = find_document_block(document, selection.active.container_id);
    if (!block) return std::nullopt;
    auto edit = plan_code_block_language_edit(*block, std::move(language));
    if (!edit) return std::nullopt;

    const auto revision_before = document.revision;
    document_edit_detail::NodeAllocator allocator(document);
    auto applied = document_edit_detail::edit_block_source(
        document,
        edit->container_id,
        edit->range,
        std::move(edit->replacement),
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

// Local prefix and sibling rules run first. When their direct parent has no
// matching neighbor, the next editable leaf may belong to another flow
// container. Delete that boundary through the same tree transaction used for
// an explicitly selected boundary; never flatten the document to find it.
// Raw bodies, tables and different inline syntaxes retain their structure and
// expose the neighboring edge instead of swallowing the deletion command.
inline std::optional<DocumentTransaction> document_delete_flow_boundary(
    EditorDocument& document, const TextSelection& selection, bool backward) {
    const auto index = document_editable_order_position(document, selection.active.container_id);
    const auto& order = document.cached_editable_order;
    if (!index || (backward ? *index == 0 : *index + 1 >= order.size())) return std::nullopt;
    const auto neighbor_index = backward ? *index - 1 : *index + 1;
    const auto* current = find_document_block(document, selection.active.container_id);
    const auto* neighbor = find_document_block(document, order[neighbor_index]);
    if (!current || !neighbor) return std::nullopt;
    const auto current_range = editable_block_source_range(*current);
    const auto neighbor_range = editable_block_source_range(*neighbor);
    if (!current_range || !neighbor_range) return std::nullopt;
    const auto current_edge = backward ? current_range->start : current_range->end;
    if (clamp_editable_block_source_offset(*current, selection.active.source_offset) != current_edge)
        return std::nullopt;

    const auto target = TextPosition{neighbor->id,
        backward ? neighbor_range->end : neighbor_range->start,
        backward ? TextAffinity::Upstream : TextAffinity::Downstream};
    if (document_edit_detail::text_block(current->kind)
        && document_edit_detail::text_block(neighbor->kind)
        && current->inline_content().syntax_mode == neighbor->inline_content().syntax_mode) {
        auto transaction = document_delete_selection(document,
            TextSelection{selection.active, target});
        if (transaction) transaction->selection_before = selection;
        return transaction;
    }
    return make_recorded_document_transaction({}, selection, TextSelection::caret(target),
        document.revision, document.revision, DocumentTransactionReason::Delete);
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
        if (target.source_offset == 0) {
            auto previous = document_move_selection(after, selection, DocumentMove::Left, false);
            if (!previous) return std::nullopt;
            return make_recorded_document_transaction({}, selection, *previous,
                revision_before, revision_before, DocumentTransactionReason::Delete);
        }
        auto removed = document_edit_detail::remove_atomic(after, target.container_id, allocator);
        if (!removed) return std::nullopt;
        target = removed->target;
        operations = std::move(removed->operations);
    } else if (selected_block
        && (selected_block->kind == BlockKind::CodeBlock
            || selected_block->kind == BlockKind::MathBlock)) {
        target.source_offset = block_source_clamp_editable_offset(
            selected_block->block_source,
            target.source_offset, target.affinity);
        if (auto removed = document_edit_detail::remove_empty_raw_block_at_caret(
                after, *selected_block, target, allocator)) {
            target = removed->target;
            operations = std::move(removed->operations);
        } else {
            auto range = block_source_backward_delete_range(
                selected_block->block_source,
                target.source_offset);
            if (!range) return document_delete_flow_boundary(after, selection, true);
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
        return document_delete_flow_boundary(after, selection, true);
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
        if (target.source_offset != 0) {
            auto next = document_move_selection(after, selection, DocumentMove::Right, false);
            if (!next) return std::nullopt;
            return make_recorded_document_transaction({}, selection, *next,
                revision_before, revision_before, DocumentTransactionReason::Delete);
        }
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
            target.source_offset, target.affinity);
        if (auto removed = document_edit_detail::remove_empty_raw_block_at_caret(
                after, *selected_block, target, allocator)) {
            target = removed->target;
            operations = std::move(removed->operations);
        } else {
            auto range = block_source_forward_delete_range(
                selected_block->block_source,
                target.source_offset);
            if (!range) return document_delete_flow_boundary(after, selection, false);
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
        return document_delete_flow_boundary(after, selection, false);
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

inline std::optional<DocumentTransaction> document_delete_selection(
    EditorDocument& document, const TextSelection& requested, TextPosition* tracked) {
    const auto selection = snap_display_math_selection(document, requested);
    if (selection.is_caret()) return std::nullopt;
    if (selection.selects_cells()) {
        const auto grid = document_table_cell_selection(document, selection);
        if (!grid) return std::nullopt;
        const auto* table = find_document_block(document, grid->table);
        if (!table) return std::nullopt;
        const auto revision = document.revision;
        std::vector<DocumentOperation> operations;
        document_edit_detail::MutationRollback rollback(document, operations, revision);
        document_edit_detail::NodeAllocator allocator(document);
        for (auto row = grid->region.first.row; row <= grid->region.last.row; ++row) {
            for (auto column = grid->region.first.column; column <= grid->region.last.column; ++column) {
                if (table_cell_is_computed(*table, {row, column})) continue;
                const auto& cell = table->children[row].children[column];
                const auto size = cell.inline_content().source.size();
                if (size == 0) continue;
                auto edit = document_edit_detail::edit_inline(document, cell.id, {0, size}, {}, allocator);
                if (!edit) return std::nullopt;
                document_edit_detail::append_source_operation(operations, std::move(*edit));
            }
        }
        document_edit_detail::track_source_operations(tracked, operations);
        if (!operations.empty()) ++document.revision;
        rollback.commit();
        return make_recorded_document_transaction(std::move(operations), selection, selection,
            revision, document.revision, DocumentTransactionReason::Delete);
    }
    if (auto marker = document_delete_marker(document, selection, false, tracked)) return marker;
    const auto revision_before = document.revision;
    auto& after = document; document_edit_detail::NodeAllocator allocator(after);
    if (selection.anchor.container_id == selection.active.container_id) {
        auto start = (std::min)(selection.anchor.source_offset, selection.active.source_offset);
        auto end = (std::max)(selection.anchor.source_offset, selection.active.source_offset);
        const auto* selected_block = find_document_block(after, selection.active.container_id);
        if (selected_block && document_edit_detail::atomic_block(selected_block->kind)) {
            if (start != 0 || end != 1) return std::nullopt;
            auto removed = document_edit_detail::remove_atomic(after, selected_block->id, allocator);
            if (!removed) return std::nullopt;
            ++after.revision;
            return make_recorded_document_transaction(std::move(removed->operations), selection,
                TextSelection::caret(removed->target), revision_before, after.revision,
                DocumentTransactionReason::Delete);
        }
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
            revision_before, DocumentTransactionReason::Delete, tracked);
    }

    if (auto slices = document_table_barrier_text_slices(after, selection);
        slices && slices->size() > 1) {
        // Preserve the table's rectangular cell tree while editing the
        // selected visible source. Non-table atomic/raw owners are removable
        // when fully covered; passing their frame source to erase_text would
        // reject an otherwise legal mixed selection (or expose hidden fences).
        std::vector<DocumentOperation> operations;
        document_edit_detail::MutationRollback rollback(
            after, operations, revision_before);
        std::vector<NodeId> removed_ids;
        std::vector<NodeId> math_without_openers;
        std::optional<TextPosition> target;
        for (auto const& slice : *slices) {
            const auto* block = find_document_block(after, slice.container_id);
            const auto visible = block ? editable_block_source_range(*block) : std::nullopt;
            if (!block || !visible) return std::nullopt;
            const auto structural = document_edit_detail::atomic_block(block->kind)
                || block->kind == BlockKind::CodeBlock || block->kind == BlockKind::MathBlock;
            if (structural && slice.source_range == *visible) {
                removed_ids.push_back(slice.container_id);
                continue;
            }
            if (!target) target = TextPosition{
                slice.container_id, slice.source_range.start, TextAffinity::Downstream};
            if (slice.source_range.empty()) continue;
            auto source_edit = document_edit_detail::erase_text(
                after, slice.container_id, slice.source_range, allocator);
            if (!source_edit) return std::nullopt;
            document_edit_detail::append_source_operation(operations, std::move(*source_edit));
            if (document_edit_detail::math_without_opener(*block)) math_without_openers.push_back(block->id);
        }
        // Every table contributes at least one surviving cell. Select it (or
        // an earlier surviving owner) before removing nodes and invalidating
        // their cached paths.
        if (!target) return std::nullopt;
        document_edit_detail::track_source_operations(tracked, operations);
        document_edit_detail::reclassify_math_without_opener(
            after, math_without_openers, allocator, *target, tracked, operations);
        for (auto id = removed_ids.rbegin(); id != removed_ids.rend(); ++id) {
            if (!document_edit_detail::remove_node_recorded(after, *id, operations)) return std::nullopt;
        }
        if (!removed_ids.empty()) {
            document_edit_detail::prune_empty_containers_recorded(after.root, operations, after);
        }
        if (!operations.empty()) ++after.revision;
        rollback.commit();
        return make_recorded_document_transaction(
            std::move(operations), selection, TextSelection::caret(*target),
            revision_before, after.revision, DocumentTransactionReason::Delete);
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
    const auto structure_boundary = [](const BlockNode* block) {
        return block && (block->kind == BlockKind::CodeBlock
            || block->kind == BlockKind::MathBlock
            || document_edit_detail::atomic_block(block->kind));
    };
    if (structure_boundary(first_block) || structure_boundary(last_block)) {
        // Raw and atomic blocks are semantic structure boundaries. Cross-block deletion
        // edits only each surviving boundary's visible content and removes
        // fully covered structures; it never splices paragraph source into a
        // fence or appends a math suffix to an unrelated block kind.
        std::vector<DocumentOperation> operations;
        document_edit_detail::MutationRollback rollback(
            after, operations, revision_before);
        std::vector<NodeId> remove_ids;
        std::vector<NodeId> math_without_openers;
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
        const auto hidden_marker_gap = editable_raw_block_source(*first_block)
            && editable_raw_block_source(*last_block)
            && first.source_offset > first_range->end
            && last.source_offset < last_range->start;
        first.source_offset = (std::clamp)(
            first.source_offset, first_range->start, first_range->end);
        last.source_offset = (std::clamp)(
            last.source_offset, last_range->start, last_range->end);

        if (structure_boundary(first_block) && first.source_offset == first_range->start) {
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
            if (document_edit_detail::math_without_opener(*first_block)) math_without_openers.push_back(first.container_id);
        }

        if (structure_boundary(last_block) && last.source_offset == last_range->end) {
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
            if (document_edit_detail::math_without_opener(*last_block)) math_without_openers.push_back(last.container_id);
        }

        std::ranges::sort(remove_ids, [&](NodeId left, NodeId right) {
            return document_editable_order_position(after, left).value_or(0)
                > document_editable_order_position(after, right).value_or(0);
        });
        remove_ids.erase(std::unique(remove_ids.begin(), remove_ids.end()), remove_ids.end());
        TextPosition target;
        if (keep_first) target = first;
        else if (keep_last) target = {last.container_id, last_range->start, TextAffinity::Downstream};
        document_edit_detail::track_source_operations(tracked, operations);
        document_edit_detail::reclassify_math_without_opener(
            after, math_without_openers, allocator, target, tracked, operations);
        for (const auto id : remove_ids) {
            if (!document_edit_detail::remove_node_recorded(after, id, operations)) return std::nullopt;
        }
        if (operations.empty()) {
            if (hidden_marker_gap) return std::nullopt;
            // A range can select only a protected structural boundary. Its
            // deletion still collapses the selection, allowing typing, paste
            // or Enter to continue at the surviving insertion position.
            rollback.commit();
            return make_recorded_document_transaction({}, selection, TextSelection::caret(first),
                revision_before, revision_before, DocumentTransactionReason::Delete);
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

        if (target.container_id.v == 0) {
            auto first_position = first_document_text_position(after);
            if (!first_position) first_position = append_document_editable_destination(after, &operations);
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
    if (tracked && tracked->container_id == last.container_id
        && tracked->source_offset >= last.source_offset) {
        tracked->container_id = first.container_id;
        tracked->source_offset = first.source_offset + (tracked->source_offset - last.source_offset);
    }
    if (auto reclassified = document_edit_detail::reparse_edited_direct_block(
            after, target, allocator, &reparse_edit, tracked)) {
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

namespace document_navigation_detail {

// The editable source preserves physical line endings. A CRLF is one line
// boundary, and a lone CR is a boundary as well; neither belongs to a line's
// reachable Home/End columns.
inline SourceRange physical_line_range(std::u32string_view text, std::size_t offset) {
    offset = (std::min)(offset, text.size());
    const auto previous = offset == 0 ? std::u32string_view::npos
        : text.find_last_of(U"\r\n", offset - 1);
    const auto next = text.find_first_of(U"\r\n", offset);
    return {previous == std::u32string_view::npos ? 0 : previous + 1,
        next == std::u32string_view::npos ? text.size() : next};
}

inline std::size_t after_line_ending(std::u32string_view text, std::size_t offset) {
    if (offset >= text.size()) return text.size();
    return offset + (text[offset] == U'\r' && offset + 1 < text.size()
        && text[offset + 1] == U'\n' ? 2u : 1u);
}

inline std::size_t before_line_ending(std::u32string_view text, std::size_t offset) {
    if (offset == 0) return 0;
    return offset - (text[offset - 1] == U'\n' && offset >= 2
        && text[offset - 2] == U'\r' ? 2u : 1u);
}

} // namespace document_navigation_detail

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
    target.source_offset = clamp_editable_block_source_offset(*active_block, target.source_offset, target.affinity);
    if (!extend && !selection.is_caret()
        && (movement == DocumentMove::Left || movement == DocumentMove::Right)) {
        const auto anchor_index = document_editable_order_position(
            document, selection.anchor.container_id).value_or(*index);
        const bool anchor_first = anchor_index < *index || (anchor_index == *index && selection.anchor.source_offset < selection.active.source_offset);
        target = movement == DocumentMove::Left
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
    else if (movement == DocumentMove::Left || movement == DocumentMove::Right) {
        const bool backward = movement == DocumentMove::Left;
        const auto direction = backward ? TextAffinity::Upstream : TextAffinity::Downstream;
        std::optional<std::size_t> adjacent;
        if (editable_raw_block_source(*active_block)) {
            adjacent = block_source_adjacent_caret_offset(active_block->block_source, target.source_offset, direction);
        } else if (backward ? target.source_offset > range->start : target.source_offset < range->end) {
            adjacent = backward ? prev_grapheme_boundary_char(*text, target.source_offset)
                : next_grapheme_boundary_char(*text, target.source_offset);
        }
        if (adjacent) target.source_offset = (std::clamp)(*adjacent, range->start, range->end);
        else if (backward ? *index > 0 : *index + 1 < order.size()) {
            const auto neighbor = backward ? *index - 1 : *index + 1;
            auto edge = range_at(neighbor); if (!edge) return std::nullopt;
            target = {order[neighbor], backward ? edge->end : edge->start, direction};
        }
    } else {
        auto navigation_text = *text;
        auto navigation_offset = target.source_offset;
        auto raw_mapping = editable_raw_block_source(*active_block)
            ? block_source_editable_projection(active_block->block_source)
            : nullptr;
        if (raw_mapping) {
            navigation_text = active_block->block_source.tree().content;
            auto mapped = raw_mapping->lower_bound(target.source_offset);
            navigation_offset = mapped == raw_mapping->end()
                ? navigation_text.size()
                : static_cast<std::size_t>(mapped - raw_mapping->begin());
        }
        // External selections can arrive inside CRLF. Resolve the invisible
        // interior by affinity before calculating a physical line column.
        if (navigation_offset > 0 && navigation_offset < navigation_text.size()
            && navigation_text[navigation_offset - 1] == U'\r'
            && navigation_text[navigation_offset] == U'\n') {
            if (target.affinity == TextAffinity::Downstream) ++navigation_offset;
            else --navigation_offset;
        }
        const auto line = document_navigation_detail::physical_line_range(navigation_text, navigation_offset);
        auto destination = navigation_offset;
        if (movement == DocumentMove::LineStart) destination = line.start;
        else if (movement == DocumentMove::LineEnd) destination = line.end;
        else {
            const auto backward = movement == DocumentMove::Up;
            const auto column = navigation_offset - line.start;
            std::optional<SourceRange> destination_line;
            if (backward && line.start > 0) {
                destination_line = document_navigation_detail::physical_line_range(navigation_text,
                    document_navigation_detail::before_line_ending(navigation_text, line.start));
            } else if (!backward && line.end < navigation_text.size()) {
                destination_line = document_navigation_detail::physical_line_range(navigation_text,
                    document_navigation_detail::after_line_ending(navigation_text, line.end));
            } else if (backward ? *index > 0 : *index + 1 < order.size()) {
                const auto* neighbor = block_at(backward ? *index - 1 : *index + 1);
                const auto neighbor_text = neighbor ? editable_block_text_view(*neighbor) : std::nullopt;
                if (!neighbor_text) return std::nullopt;
                raw_mapping = editable_raw_block_source(*neighbor)
                    ? block_source_editable_projection(neighbor->block_source) : nullptr;
                navigation_text = raw_mapping ? std::u32string_view{neighbor->block_source.tree().content}
                    : *neighbor_text;
                target.container_id = neighbor->id;
                target.affinity = backward ? TextAffinity::Upstream : TextAffinity::Downstream;
                destination_line = document_navigation_detail::physical_line_range(navigation_text,
                    backward ? navigation_text.size() : 0);
            }
            destination = destination_line
                ? destination_line->start + (std::min)(column, destination_line->length())
                : backward ? 0 : navigation_text.size();
        }
        // A line column may land inside a combining/emoji cluster on the new
        // line. Keep the same source coordinate unit while snapping to a
        // reachable grapheme boundary before mapping raw content back.
        if (destination > 0 && destination < navigation_text.size()) {
            const auto before_cluster = prev_grapheme_boundary_char(navigation_text, destination);
            if (next_grapheme_boundary_char(navigation_text, before_cluster) != destination)
                destination = before_cluster;
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
    const auto result = snap_display_math_selection(document,
        extend ? TextSelection{selection.anchor, target} : TextSelection::caret(target));
    if (result == selection) return std::nullopt;
    return result;
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
