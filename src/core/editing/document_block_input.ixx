// Non-list block input rules: marker activation, raw blocks, and container unwrapping.
export module folia.core.document_block_input;
import std;
import folia.markdown.selection;
import folia.markdown.ast;
import folia.markdown.container_source;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.core.document_container_source;
import folia.core.document_edit_support;
import folia.markdown.ids;
import folia.markdown.inline_document;
import folia.markdown.text_edit;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.core.document_input_syntax;
import folia.core.document_list_input;

export namespace folia::document_input_rules::detail {

// Atomic owners expose the two sides of one object, not editable characters.
// A caret on either side must still be a place where a flow paragraph can be
// created. Keep this shared by Enter, typing and paste so those commands do
// not invent different ways to escape an image, divider or other object.
inline std::optional<document_edit_detail::RecordedBlockEdit> insert_atomic_boundary_paragraph(
    EditorDocument& document,
    TextPosition position,
    document_edit_detail::NodeAllocator& allocator) {
    const auto path = document_block_path(document, position.container_id);
    if (!path || path->empty()) return std::nullopt;
    auto parent_path = *path;
    const auto index = parent_path.back();
    parent_path.pop_back();
    auto* parent = block_at_path(document.root, parent_path);
    if (!parent || index >= parent->children.size()
        || !document_edit_detail::atomic_block(parent->children[index].kind)) {
        return std::nullopt;
    }

    auto syntax_mode = InlineSyntaxMode::Markdown;
    const auto* context = &document.root;
    for (const auto child_index : parent_path) {
        context = &context->children[child_index];
        if (context->kind == BlockKind::HtmlContainer || context->has_html_source()
            || context->has_html_element_provenance()) {
            syntax_mode = InlineSyntaxMode::HtmlText;
        }
    }
    auto paragraph = document_edit_detail::empty_paragraph(allocator, document, syntax_mode);
    document_edit_detail::RecordedBlockEdit result;
    result.target = {paragraph.id, 0, TextAffinity::Downstream};
    const auto insert_index = index + (position.source_offset == 0 ? 0u : 1u);
    DocumentTreeEdit insert;
    insert.kind = DocumentTreeEditKind::Insert;
    insert.parent_id = parent->id;
    insert.index = insert_index;
    insert.after = paragraph;
    result.operations.emplace_back(std::move(insert));
    if (!insert_block(*parent, insert_index, std::move(paragraph))) return std::nullopt;
    parent_path.push_back(insert_index);
    cache_document_block_path(document, parent_path);
    return result;
}

// Confirm a completed Markdown line on Enter. Spaced dividers share their
// initial spelling with eager list input ("- " / "* "), including nested
// prefixes such as "- - ". Recover only those first-child prefixes that are
// on this same line, and unwrap exactly the matching item chain.
inline std::optional<document_edit_detail::RecordedBlockEdit> activate_thematic_break(
    EditorDocument& document,
    TextPosition position,
    document_edit_detail::NodeAllocator& allocator) {
    const auto* selected = find_document_block(document, position.container_id);
    if (!selected || selected->kind != BlockKind::Paragraph
        || selected->inline_content.syntax_mode != InlineSyntaxMode::Markdown
        || position.source_offset != selected->inline_content.source.size()) {
        return std::nullopt;
    }
    if (selected->inline_content.source.find_first_not_of(U" \t-*_")
        != std::u32string::npos) return std::nullopt;

    auto candidate = selected->inline_content.source;
    std::optional<std::u32string> marker;
    std::size_t matching_prefixes = 0;
    if (recognize_thematic_break(candidate)) marker = candidate;
    auto descendant = selected->id;
    for (std::size_t prefixes = 1;; ++prefixes) {
        const auto* child = find_document_block(document, descendant);
        if (!child || (child->separator_before
                && child->separator_before->find_first_of(U"\r\n") != std::u32string::npos)) {
            break;
        }
        const auto context = direct_list_context(document, descendant);
        if (!context || context->child_index != 0) break;
        const auto* list = block_at_path(document.root, context->list_path);
        if (!list || list->list_special().ordered
            || context->item_index >= list->children.size()) break;
        const auto& item = list->children[context->item_index];
        if (item.kind != BlockKind::ListItem || item.children.size() != 1) break;
        // This item has one child and every already traversed nested list has
        // one item, so the temporary fragment contains exactly the selected
        // leaf and its prefix chain. The serializer owns physical prefix and
        // empty marker-line provenance; concatenating ItemSpecial::marker
        // alone would incorrectly turn "-\n  --" into a divider.
        auto wrapper = list_shell_from(*list, list->id);
        wrapper.children.push_back(item);
        BlockVec fragment;
        fragment.push_back(std::move(wrapper));
        candidate = serialize_markdown_fragment(fragment);
        if (candidate.find_first_of(U"\r\n") != std::u32string::npos) break;
        if (recognize_thematic_break(candidate)) {
            marker = candidate;
            matching_prefixes = prefixes;
        }
        if (list->children.size() != 1) break;
        descendant = list->id;
    }
    if (!marker) return std::nullopt;

    document_edit_detail::RecordedBlockEdit result;
    document_edit_detail::MutationRollback rollback(document, result.operations, document.revision);
    for (std::size_t index = 0; index < matching_prefixes; ++index) {
        const auto context = direct_list_context(document, position.container_id);
        if (!context || context->list_path.empty()) return std::nullopt;
        auto moved_path = context->list_path;
        moved_path.back() += context->item_index > 0 ? 1u : 0u;
        auto unwrapped = remove_top_level_list_prefix(document, *context, position, allocator);
        if (!unwrapped) return std::nullopt;
        result.operations.insert(result.operations.end(),
            std::make_move_iterator(unwrapped->operations.begin()),
            std::make_move_iterator(unwrapped->operations.end()));
        cache_document_block_path(document, moved_path);
    }

    auto* block = find_document_block(document, position.container_id);
    if (!block || block->kind != BlockKind::Paragraph) return std::nullopt;
    auto source_edit = document_edit_detail::edit_inline(document, block->id,
        {0, block->inline_content.source.size()}, {}, allocator);
    if (!source_edit) return std::nullopt;
    document_edit_detail::append_source_operation(result.operations, std::move(*source_edit));
    block = find_document_block(document, position.container_id);
    if (!block) return std::nullopt;
    DocumentTreeEdit update;
    update.kind = DocumentTreeEditKind::UpdatePayload;
    update.before = document_transaction_detail::payload_shell(*block);
    block->kind = BlockKind::ThematicBreak;
    block->ensure_text_special().opening_marker = std::move(*marker);
    update.after = document_transaction_detail::payload_shell(*block);
    result.operations.emplace_back(std::move(update));

    auto following = insert_atomic_boundary_paragraph(document,
        {position.container_id, 1, TextAffinity::Downstream}, allocator);
    if (!following) return std::nullopt;
    result.target = following->target;
    result.operations.insert(result.operations.end(),
        std::make_move_iterator(following->operations.begin()),
        std::make_move_iterator(following->operations.end()));
    rollback.commit();
    return result;
}

inline std::optional<document_edit_detail::RecordedBlockEdit> activate_setext_heading(
    EditorDocument& document,
    TextPosition position,
    document_edit_detail::NodeAllocator& allocator) {
    auto* block = find_document_block(document, position.container_id);
    if (!block || block->kind != BlockKind::Paragraph
        || block->inline_content.syntax_mode != InlineSyntaxMode::Markdown
        || position.source_offset != block->inline_content.source.size()
        || !has_setext_underline(block->inline_content.source)) {
        return std::nullopt;
    }

    // The parser decides whether the preceding lines are valid heading
    // content, including indentation, escapes and other block interruptions.
    // Recognition is bounded to this owner's source and keeps the parser's
    // exact marker spellings instead of rebuilding a canonical ATX heading.
    auto parsed = plan_block_fragment(block->inline_content.source, document.dialect,
        [](const BlockVec& blocks) {
            return blocks.size() == 1 && blocks.front().kind == BlockKind::Heading;
        });
    if (!parsed || parsed->blocks.size() != 1) return std::nullopt;
    const auto& heading = parsed->blocks.front();
    const auto syntax = heading.text_special();
    const auto& content = heading.inline_content.source;
    if (syntax.opening_marker + content + syntax.closing_marker != block->inline_content.source) {
        return std::nullopt;
    }

    document_edit_detail::RecordedBlockEdit result;
    document_edit_detail::MutationRollback rollback(document, result.operations, document.revision);
    const auto content_start = syntax.opening_marker.size();
    const auto content_end = content_start + content.size();
    auto closing = document_edit_detail::edit_inline(document, block->id,
        {content_end, block->inline_content.source.size()}, {}, allocator);
    if (!closing) return std::nullopt;
    document_edit_detail::append_source_operation(result.operations, std::move(*closing));
    if (content_start != 0) {
        auto opening = document_edit_detail::edit_inline(document, position.container_id,
            {0, content_start}, {}, allocator);
        if (!opening) return std::nullopt;
        document_edit_detail::append_source_operation(result.operations, std::move(*opening));
    }
    block = find_document_block(document, position.container_id);
    if (!block) return std::nullopt;
    DocumentTreeEdit update;
    update.kind = DocumentTreeEditKind::UpdatePayload;
    update.before = document_transaction_detail::payload_shell(*block);
    block->kind = BlockKind::Heading;
    block->ensure_text_special() = syntax;
    update.after = document_transaction_detail::payload_shell(*block);
    result.operations.emplace_back(std::move(update));

    const auto at_end = TextPosition{block->id, content.size(), TextAffinity::Downstream};
    auto following = split_list_item(document, at_end, allocator);
    if (!following) {
        following = document_edit_detail::split_direct(document, at_end.container_id,
            at_end.source_offset, allocator);
    }
    if (!following) return std::nullopt;
    result.target = following->target;
    result.operations.insert(result.operations.end(),
        std::make_move_iterator(following->operations.begin()),
        std::make_move_iterator(following->operations.end()));
    rollback.commit();
    return result;
}

inline void record_list_item_boundary(
    EditorDocument& document,
    std::vector<DocumentOperation>& operations,
    NodeId item_id,
    const std::optional<std::u32string>& boundary) {
    if (!boundary) return;
    auto* item = find_block(document.root, item_id);
    if (!item || item->separator_before == boundary) return;
    DocumentTreeEdit update;
    update.kind = DocumentTreeEditKind::UpdatePayload;
    update.before = document_transaction_detail::payload_shell(*item);
    item->separator_before = boundary;
    update.after = document_transaction_detail::payload_shell(*item);
    operations.emplace_back(std::move(update));
}

inline std::optional<document_edit_detail::RecordedBlockEdit> replace_paragraph_with_container(
    EditorDocument& document,
    const BlockPath& path,
    const MarkerMatch& match,
    TextPosition& target,
    document_edit_detail::NodeAllocator& allocator) {
    if (path.empty()) return std::nullopt;
    auto parent_path = path;
    const auto index = parent_path.back();
    parent_path.pop_back();
    auto* parent = block_at_path(document.root, parent_path);
    if (!parent || index >= parent->children.size()) return std::nullopt;
    auto& source_block = parent->children[index];
    if (source_block.kind != BlockKind::Paragraph) return std::nullopt;
    document_edit_detail::RecordedBlockEdit result;
    result.target = target;
    const auto parent_id = parent->id;
    const auto exact_marker = source_block.inline_content.source.substr(0, match.length);
    auto marker_edit = document_edit_detail::edit_inline(
        document, source_block.id, {0, match.length}, {}, allocator);
    if (!marker_edit) return std::nullopt;
    document_edit_detail::append_source_operation(result.operations, std::move(*marker_edit));
    target = {source_block.id, target.source_offset - match.length, TextAffinity::Downstream};
    result.target = target;

    if (match.kind == MarkerMatch::Kind::Heading) {
        DocumentTreeEdit update;
        update.kind = DocumentTreeEditKind::UpdatePayload;
        update.before = document_transaction_detail::payload_shell(source_block);
        source_block.kind = BlockKind::Heading;
        source_block.ensure_text_special().level = match.heading_level;
        source_block.ensure_text_special().opening_marker = exact_marker;
        source_block.ensure_text_special().closing_marker.clear();
        source_block.ensure_text_special().slug.clear();
        update.after = document_transaction_detail::payload_shell(source_block);
        result.operations.emplace_back(std::move(update));
        return result;
    }

    if (match.kind == MarkerMatch::Kind::Quote) {
        BlockNode quote;
        quote.id = allocator.allocate();
        quote.kind = BlockKind::BlockQuote;
        quote.ensure_container_source().lines.push_back({{source_block.id, ContainerAnchorPart::Source, 0}, exact_marker});
        const auto quote_id = quote.id;
        DocumentTreeEdit insert;
        insert.kind = DocumentTreeEditKind::Insert;
        insert.parent_id = parent_id;
        insert.index = index;
        insert.after = quote;
        result.operations.emplace_back(std::move(insert));
        parent->children.insert(
            parent->children.begin() + static_cast<std::ptrdiff_t>(index),
            std::move(quote));

        DocumentTreeEdit move;
        move.kind = DocumentTreeEditKind::Move;
        move.parent_id = parent_id;
        move.index = index + 1;
        move.other_parent_id = quote_id;
        move.other_index = 0;
        move.moved_id = parent->children[index + 1].id;
        auto content = remove_block(*parent, index + 1);
        auto* inserted_quote = find_block(document.root, quote_id);
        if (!content || !inserted_quote || !insert_block(*inserted_quote, 0, std::move(*content))) {
            return std::nullopt;
        }
        document_container_source::append_container_tree_operation(document, result.operations, std::move(move));
        return result;
    }

    BlockNode list;
    list.id = allocator.allocate();
    list.kind = match.kind == MarkerMatch::Kind::TaskList ? BlockKind::TaskList : BlockKind::List;
    // The new list takes the paragraph's root position, so it must also own
    // the paragraph's separator from its preceding root sibling.
    list.separator_before = source_block.separator_before;
    list.ensure_list_special().ordered = match.kind == MarkerMatch::Kind::OrderedList;
    list.ensure_list_special().start = list.list_special().ordered ? match.list_start : 1;
    list.ensure_list_special().delimiter = match.list_delimiter;
    BlockNode item;
    item.id = allocator.allocate();
    item.kind = match.kind == MarkerMatch::Kind::TaskList ? BlockKind::TaskListItem : BlockKind::ListItem;
    item.ensure_item_special().marker = exact_marker;
    item.ensure_item_special().checked = match.checked;
    list.children.push_back(std::move(item));
    const auto list_id = list.id;
    const auto item_id = list.children.front().id;
    DocumentTreeEdit insert;
    insert.kind = DocumentTreeEditKind::Insert;
    insert.parent_id = parent_id;
    insert.index = index;
    insert.after = list;
    result.operations.emplace_back(std::move(insert));
    parent->children.insert(
        parent->children.begin() + static_cast<std::ptrdiff_t>(index),
        std::move(list));

    DocumentTreeEdit move_content;
    move_content.kind = DocumentTreeEditKind::Move;
    move_content.parent_id = parent_id;
    move_content.index = index + 1;
    move_content.other_parent_id = item_id;
    move_content.other_index = 0;
    move_content.moved_id = parent->children[index + 1].id;
    auto content = remove_block(*parent, index + 1);
    auto* inserted_item = find_block(document.root, item_id);
    if (!content || !inserted_item || !insert_block(*inserted_item, 0, std::move(*content))) {
        return std::nullopt;
    }
        document_container_source::append_container_tree_operation(document, result.operations, std::move(move_content));

    auto* inserted_list = find_block(document.root, list_id);
    parent = find_block(document.root, parent_id);
    if (!inserted_list || !parent) return std::nullopt;
    std::size_t effective_index = index;
    if (effective_index > 0 && compatible_lists(parent->children[effective_index - 1], *inserted_list)) {
        auto* previous = &parent->children[effective_index - 1];
        const auto list_boundary = inserted_list->separator_before;
        const auto moved_item_id = inserted_list->children.front().id;
        const auto target_index = previous->children.size();
        DocumentTreeEdit move_item;
        move_item.kind = DocumentTreeEditKind::Move;
        move_item.parent_id = list_id;
        move_item.index = 0;
        move_item.other_parent_id = previous->id;
        move_item.other_index = target_index;
        move_item.moved_id = inserted_list->children.front().id;
        auto moved_item = remove_block(*inserted_list, 0);
        if (!moved_item || !insert_block(*previous, target_index, std::move(*moved_item))) {
            return std::nullopt;
        }
        document_container_source::append_container_tree_operation(document, result.operations, std::move(move_item));
        record_list_item_boundary(document, result.operations, moved_item_id, list_boundary);

        DocumentTreeEdit remove_list;
        remove_list.kind = DocumentTreeEditKind::Remove;
        remove_list.parent_id = parent_id;
        remove_list.index = effective_index;
        remove_list.before = parent->children[effective_index];
        document_container_source::append_container_tree_operation(document, result.operations, std::move(remove_list));
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(effective_index));
        --effective_index;
        inserted_list = &parent->children[effective_index];
    }

    if (effective_index + 1 < parent->children.size()
        && compatible_lists(*inserted_list, parent->children[effective_index + 1])) {
        const auto next_id = parent->children[effective_index + 1].id;
        const auto list_boundary = parent->children[effective_index + 1].separator_before;
        auto* next = &parent->children[effective_index + 1];
        bool first_moved_item = true;
        while (!next->children.empty()) {
            DocumentTreeEdit move_item;
            move_item.kind = DocumentTreeEditKind::Move;
            move_item.parent_id = next_id;
            move_item.index = 0;
            move_item.other_parent_id = inserted_list->id;
            move_item.other_index = inserted_list->children.size();
            move_item.moved_id = next->children.front().id;
            const auto moved_item_id = move_item.moved_id;
            const auto preserve_boundary = first_moved_item;
            auto moved_item = remove_block(*next, 0);
            first_moved_item = false;
            if (!moved_item || !insert_block(
                    *inserted_list, move_item.other_index, std::move(*moved_item))) {
                return std::nullopt;
            }
        document_container_source::append_container_tree_operation(document, result.operations, std::move(move_item));
            if (preserve_boundary) {
                record_list_item_boundary(document, result.operations, moved_item_id, list_boundary);
            }
        }
        DocumentTreeEdit remove_list;
        remove_list.kind = DocumentTreeEditKind::Remove;
        remove_list.parent_id = parent_id;
        remove_list.index = effective_index + 1;
        remove_list.before = parent->children[effective_index + 1];
        document_container_source::append_container_tree_operation(document, result.operations, std::move(remove_list));
        parent->children.erase(
            parent->children.begin() + static_cast<std::ptrdiff_t>(effective_index + 1));
    }
    return result;
}

inline std::optional<document_edit_detail::RecordedBlockEdit> open_raw_block(
    EditorDocument& document,
    TextPosition position,
    document_edit_detail::NodeAllocator& allocator) {
    auto path = document_block_path(document, position.container_id);
    if (!path || path->empty()) return std::nullopt;
    auto* block = block_at_path(document.root, *path);
    if (!block || block->kind != BlockKind::Paragraph
        || block->inline_content.syntax_mode != InlineSyntaxMode::Markdown
        || position.source_offset != block->inline_content.source.size()) return std::nullopt;
    auto opening = recognize_raw_block_opening(block->inline_content.source, document.dialect);
    if (!opening) return std::nullopt;

    document_edit_detail::RecordedBlockEdit result;
    document_edit_detail::MutationRollback rollback(
        document, result.operations, document.revision);
    result.target = TextPosition{position.container_id, 0, TextAffinity::Downstream};
    auto source_edit = document_edit_detail::edit_inline(
        document,
        block->id,
        {0, block->inline_content.source.size()},
        {},
        allocator);
    if (!source_edit) return std::nullopt;
    document_edit_detail::append_source_operation(result.operations, std::move(*source_edit));
    block = find_document_block(document, position.container_id);
    if (!block) return std::nullopt;

    DocumentTreeEdit update;
    update.kind = DocumentTreeEditKind::UpdatePayload;
    update.before = document_transaction_detail::payload_shell(*block);
    BlockNode replacement;
    replacement.id = block->id;
    replacement.kind = opening->block_kind;
    replacement.separator_before = block->separator_before;
    replacement.ensure_atomic_special().math_delim = opening->math_delimiter;
    replacement.block_source.tree().kind = opening->source_kind;
    *block = std::move(replacement);
    update.after = document_transaction_detail::payload_shell(*block);
    result.operations.emplace_back(std::move(update));

    auto block_source_edit = document_edit_detail::edit_block_source(
        document,
        position.container_id,
        {0, 0},
        std::move(opening->source),
        allocator);
    if (!block_source_edit) return std::nullopt;
    document_edit_detail::append_source_operation(result.operations, std::move(*block_source_edit));
    block = find_document_block(document, position.container_id);
    if (!block) return std::nullopt;
    result.target.source_offset = block_source_editable_range(block->block_source).start;
    rollback.commit();
    return result;
}

inline std::optional<document_edit_detail::RecordedBlockEdit> remove_heading_prefix(
    EditorDocument& document,
    TextPosition position) {
    auto* block = find_document_block(document, position.container_id);
    if (!block || block->kind != BlockKind::Heading) return std::nullopt;
    document_edit_detail::RecordedBlockEdit result;
    result.target = TextPosition{block->id, 0, TextAffinity::Downstream};
    DocumentTreeEdit update;
    update.kind = DocumentTreeEditKind::UpdatePayload;
    update.before = document_transaction_detail::payload_shell(*block);
    auto& heading = block->ensure_text_special();
    const auto hash = heading.opening_marker.find(U'#');
    if (heading.level > 1 && hash != std::u32string::npos) {
        // The parser already classified this as an ATX heading. Preserve its
        // exact surrounding whitespace while reducing the semantic level.
        --heading.level;
        heading.opening_marker.erase(hash, 1);
    } else {
        block->kind = BlockKind::Paragraph;
        heading.level = 0;
        heading.slug.clear();
        heading.opening_marker.clear();
        heading.closing_marker.clear();
    }
    update.after = document_transaction_detail::payload_shell(*block);
    result.operations.emplace_back(std::move(update));
    return result;
}

inline std::optional<document_edit_detail::RecordedBlockEdit> remove_callout_prefix(
    EditorDocument& document,
    TextPosition position,
    document_edit_detail::NodeAllocator& allocator) {
    auto path = document_block_path(document, position.container_id);
    if (!path || path->size() < 2) return std::nullopt;
    auto* selected = block_at_path(document.root, *path);
    if (!selected) return std::nullopt;
    auto callout_path = *path;
    const auto child_index = callout_path.back();
    callout_path.pop_back();
    auto* callout = block_at_path(document.root, callout_path);
    if (!callout || callout->kind != BlockKind::Callout) return std::nullopt;
    const auto* title = callout_title_block(*callout);
    const auto selected_title = selected->kind == BlockKind::CalloutTitle && child_index == 0;
    if (!selected_title && (title || child_index != 0)) return std::nullopt;

    document_edit_detail::RecordedBlockEdit result;
    result.target = TextPosition{position.container_id, 0, TextAffinity::Downstream};

    if (selected_title && callout->children.size() > 1
        && document_edit_detail::text_block(callout->children[1].kind)) {
        const auto title_id = selected->id;
        const auto offset = selected->inline_content.source.size();
        auto appended = U"\n" + callout->children[1].inline_content.source;
        auto edit = document_edit_detail::edit_inline(
            document,
            title_id,
            {offset, offset},
            std::move(appended),
            allocator);
        if (!edit) return std::nullopt;
        document_edit_detail::append_source_operation(result.operations, std::move(*edit));
        callout = block_at_path(document.root, callout_path);
        if (!callout || callout->children.size() < 2) return std::nullopt;
        DocumentTreeEdit remove_body;
        remove_body.kind = DocumentTreeEditKind::Remove;
        remove_body.parent_id = callout->id;
        remove_body.index = 1;
        remove_body.before = callout->children[1];
        document_container_source::append_container_tree_operation(document, result.operations, std::move(remove_body));
        callout->children.erase(callout->children.begin() + 1);
    }

    if (selected_title) {
        callout = block_at_path(document.root, callout_path);
        if (!callout || callout->children.empty()) return std::nullopt;
        auto* current_title = &callout->children.front();
        DocumentTreeEdit title_update;
        title_update.kind = DocumentTreeEditKind::UpdatePayload;
        title_update.before = document_transaction_detail::payload_shell(*current_title);
        current_title->kind = BlockKind::Paragraph;
        title_update.after = document_transaction_detail::payload_shell(*current_title);
        result.operations.emplace_back(std::move(title_update));
    }

    callout = block_at_path(document.root, callout_path);
    if (!callout) return std::nullopt;
    DocumentTreeEdit callout_update;
    callout_update.kind = DocumentTreeEditKind::UpdatePayload;
    callout_update.before = document_transaction_detail::payload_shell(*callout);
    callout->kind = BlockKind::BlockQuote;
    callout->ensure_container_special().callout_kind.clear();
    callout->ensure_text_special().opening_marker.clear();
    callout->ensure_text_special().closing_marker.clear();
    callout_update.after = document_transaction_detail::payload_shell(*callout);
    result.operations.emplace_back(std::move(callout_update));
    return result;
}

inline std::optional<document_edit_detail::RecordedBlockEdit> remove_quote_prefix(
    EditorDocument& document,
    TextPosition position) {
    auto path = document_block_path(document, position.container_id);
    if (!path || path->size() < 2) return std::nullopt;

    auto quote_path = *path;
    const auto child_index = quote_path.back();
    quote_path.pop_back();
    const auto* quote = block_at_path(document.root, quote_path);
    // Only the first direct child owns the quote's opening structural boundary.
    // At the start of a later child, Backspace must fall through to the uniform
    // sibling join path and stay inside the quote.
    if (!quote || quote->kind != BlockKind::BlockQuote || child_index != 0
        || child_index >= quote->children.size()) {
        return std::nullopt;
    }

    auto parent_path = quote_path;
    const auto quote_index = parent_path.back();
    parent_path.pop_back();
    auto* parent = block_at_path(document.root, parent_path);
    if (!parent || quote_index >= parent->children.size()) return std::nullopt;

    document_edit_detail::RecordedBlockEdit result;
    result.target = TextPosition{position.container_id, 0, TextAffinity::Downstream};
    const auto parent_id = parent->id;
    const auto quote_id = parent->children[quote_index].id;
    DocumentTreeEdit move;
    move.kind = DocumentTreeEditKind::Move;
    move.parent_id = quote_id;
    move.index = 0;
    move.other_parent_id = parent_id;
    move.other_index = quote_index;
    auto* current_quote = find_block(document.root, quote_id);
    if (!current_quote || current_quote->children.empty()) return std::nullopt;
    move.moved_id = current_quote->children.front().id;
    auto selected = current_quote ? remove_block(*current_quote, 0) : std::nullopt;
    if (!selected || !insert_block(*parent, quote_index, std::move(*selected))) return std::nullopt;
        document_container_source::append_container_tree_operation(document, result.operations, std::move(move));

    current_quote = find_block(document.root, quote_id);
    if (current_quote && current_quote->children.empty()) {
        parent = find_block(document.root, parent_id);
        if (!parent || quote_index + 1 >= parent->children.size()) return std::nullopt;
        DocumentTreeEdit remove_quote;
        remove_quote.kind = DocumentTreeEditKind::Remove;
        remove_quote.parent_id = parent_id;
        remove_quote.index = quote_index + 1;
        remove_quote.before = parent->children[quote_index + 1];
        document_container_source::append_container_tree_operation(document, result.operations, std::move(remove_quote));
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(quote_index + 1));
    }
    return result;
}

} // namespace folia::document_input_rules::detail
