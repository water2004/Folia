// Literal line indentation after list/table-specific Tab handling. Each edit
// stays in its source owner; raw code/math indentation never touches fences.
export module folia.core.document_indentation;
import std;
import folia.markdown.ids;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_document;
import folia.markdown.parser;
import folia.markdown.selection;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.core.document_block_reparse;
import folia.core.document_edit_support;
import folia.core.document_source_edit;

export namespace folia {

namespace document_indentation_detail {

inline bool indentation_reparse_candidate(const BlockNode& owner) {
    return ((owner.kind == BlockKind::Paragraph || owner.kind == BlockKind::Heading)
            && owner.inline_content.syntax_mode == InlineSyntaxMode::Markdown)
        || (owner.kind == BlockKind::CodeBlock
            && owner.block_source.tree().kind == BlockSourceKind::IndentedCode);
}

// Leading Markdown indentation may turn paragraphs into one indented code
// block; removing the last code indentation has the inverse meaning. Reparse
// only consecutive affected siblings (and adjoining indented code), retaining
// their exact separators. Both endpoints are mapped through this fragment.
inline bool reclassify_owners(EditorDocument& document, std::span<const NodeId> changed,
    TextSelection& selection, document_edit_detail::NodeAllocator& allocator,
    std::vector<DocumentOperation>& operations) {
    struct Region { NodeId first; NodeId last; NodeId parent; std::size_t last_index; };
    std::vector<Region> regions;
    for (const auto id : changed) {
        const auto* owner = find_document_block(document, id);
        if (!owner) return false;
        if (!indentation_reparse_candidate(*owner)) continue;
        auto path = document_block_path(document, id);
        if (!path || path->empty()) return false;
        const auto index = path->back();
        path->pop_back();
        const auto* parent = block_at_path(document.root, *path);
        if (!parent) return false;
        if (!regions.empty() && regions.back().parent == parent->id
            && regions.back().last_index + 1 == index) {
            regions.back().last = id;
            regions.back().last_index = index;
        } else regions.push_back({id, id, parent->id, index});
    }
    for (auto region = regions.rbegin(); region != regions.rend(); ++region) {
        auto first_path = document_block_path(document, region->first);
        const auto last_path = document_block_path(document, region->last);
        if (!first_path || !last_path || first_path->empty() || last_path->empty()) return false;
        auto begin = first_path->back();
        auto end = last_path->back() + 1;
        first_path->pop_back();
        const auto* parent = block_at_path(document.root, *first_path);
        if (!parent || parent->id != region->parent || end > parent->children.size()) return false;
        const auto is_indented_code = [](const BlockNode& block) {
            return block.kind == BlockKind::CodeBlock
                && block.block_source.tree().kind == BlockSourceKind::IndentedCode;
        };
        while (begin != 0 && is_indented_code(parent->children[begin - 1])) --begin;
        while (end < parent->children.size() && is_indented_code(parent->children[end])) ++end;
        const auto previous = serialize_markdown_fragment_projection(parent->children, begin, end);
        auto parsed = plan_block_fragment(previous.text, document.dialect, [&](const BlockVec& blocks) {
            if (blocks.empty()) return false;
            if (blocks.size() != end - begin) return true;
            for (std::size_t index = 0; index < blocks.size(); ++index) {
                const auto& current = parent->children[begin + index];
                const auto& candidate = blocks[index];
                if (current.kind != candidate.kind) return true;
                if (current.kind == BlockKind::Paragraph || current.kind == BlockKind::Heading) {
                    if (current.inline_content.source != candidate.inline_content.source) return true;
                } else if (current.block_source.source() != candidate.block_source.source()
                    || current.block_source.tree().kind != candidate.block_source.tree().kind) return true;
            }
            return false;
        });
        if (!parsed) continue;
        const auto parsed_projection = serialize_markdown_fragment_projection(parsed->blocks);
        if (parsed_projection.text != previous.text) return false;
        const auto endpoint = [&](TextPosition position) {
            const auto* block = find_block(document.root, position.container_id);
            if (!selection.is_caret() && block && block->kind == BlockKind::Heading) {
                const auto range = std::ranges::find(previous.blocks, position.container_id,
                    &SerializedBlockRange::owner);
                if (range != previous.blocks.end()) {
                    if (position.source_offset == 0) return std::optional{range->begin};
                    if (position.source_offset == block->inline_content.source.size()) return std::optional{range->end};
                }
            }
            return serialized_offset_for_source_position(previous, position);
        };
        const auto anchor_offset = endpoint(selection.anchor);
        const auto active_offset = endpoint(selection.active);
        const auto target_offset = active_offset.value_or(anchor_offset.value_or(0));
        const auto target = source_position_for_serialized_offset(parsed_projection, target_offset,
            active_offset ? selection.active.affinity : selection.anchor.affinity);
        if (!target) return false;
        const auto owner_id = parent->children[begin].id;
        const auto parent_id = parent->id;
        const auto count = parsed->blocks.size();
        parsed->blocks.front().separator_before = parent->children[begin].separator_before;
        document_edit_detail::BlockReparsePlan plan{owner_id, std::move(parsed->blocks),
            target->container_id, target->source_offset, end - begin};
        auto result = document_edit_detail::apply_block_reparse_plan(document, std::move(plan), allocator);
        if (!result) return false;
        operations.insert(operations.end(), std::make_move_iterator(result->operations.begin()),
            std::make_move_iterator(result->operations.end()));
        parent = find_block(document.root, parent_id);
        if (!parent) return false;
        const auto projection = serialize_markdown_fragment_projection(parent->children, begin, begin + count);
        const auto remap = [&](TextPosition& position, const std::optional<std::size_t>& offset) {
            if (!offset) return true;
            const auto mapped = source_position_for_serialized_offset(projection, *offset, position.affinity);
            if (!mapped) return false;
            position = *mapped;
            const auto* block = find_block(document.root, position.container_id);
            if (!block) return false;
            position.source_offset = clamp_editable_block_source_offset(*block, position.source_offset);
            return true;
        };
        if (!remap(selection.anchor, anchor_offset) || !remap(selection.active, active_offset)) return false;
    }
    return true;
}

} // namespace document_indentation_detail

inline std::optional<DocumentTransaction> document_indent_text(
    EditorDocument& document, const TextSelection& selection, bool backward) {
    if (!backward && selection.is_caret()) {
        const auto* owner = find_document_block(document, selection.active.container_id);
        const auto heading_line_start = owner && owner->kind == BlockKind::Heading
            && owner->inline_content.syntax_mode == InlineSyntaxMode::Markdown
            && document_navigation_detail::physical_line_range(owner->inline_content.source,
                selection.active.source_offset).start == selection.active.source_offset;
        if (!owner || (owner->kind != BlockKind::Paragraph && !heading_line_start)
            || owner->inline_content.syntax_mode != InlineSyntaxMode::Markdown)
            return document_insert_text(document, selection, U"    ");
    }
    auto first = document_editable_order_position(document, selection.anchor.container_id);
    auto last = document_editable_order_position(document, selection.active.container_id);
    if (!first || !last) return std::nullopt;
    auto begin = selection.anchor;
    auto end = selection.active;
    if (*first > *last || (*first == *last && begin.source_offset > end.source_offset)) {
        std::swap(first, last);
        std::swap(begin, end);
    }
    std::vector<TextEdit> planned;
    std::vector<NodeId> changed_owners;
    std::vector<NodeId> heading_prefixes;
    for (auto index = *first; index <= *last; ++index) {
        const auto id = document.cached_editable_order[index];
        const auto* block = find_document_block(document, id);
        if (!block) return std::nullopt;
        const auto* raw = editable_raw_block_source(*block);
        const auto* inlines = editable_inline_document(*block);
        if (!raw && !inlines) continue;
        if (!selection.is_caret() && index == *last && index != *first
            && clamp_editable_block_source_offset(*block, end.source_offset)
                == editable_block_source_range(*block)->start) continue;
        if (!backward && block->kind == BlockKind::Heading
            && inlines->syntax_mode == InlineSyntaxMode::Markdown) {
            // A heading is one structural unit. Indent its marker and every
            // physical title/Setext-underline line together; spaces after an
            // ATX marker would only become invisible marker padding on reload.
            heading_prefixes.push_back(id);
            auto line = document_navigation_detail::physical_line_range(inlines->source, 0);
            while (line.end < inlines->source.size()) {
                const auto next = document_navigation_detail::after_line_ending(inlines->source, line.end);
                planned.push_back({id, {next, next}, U"    "});
                line = document_navigation_detail::physical_line_range(inlines->source, next);
            }
            changed_owners.push_back(id);
            continue;
        }
        if (!backward && selection.is_caret()) {
            const auto offset = clamp_editable_block_source_offset(*block, begin.source_offset);
            planned.push_back({id, {offset, offset}, U"    "});
            changed_owners.push_back(id);
            continue;
        }
        if (backward && selection.is_caret() && raw
            && block->block_source.tree().kind == BlockSourceKind::IndentedCode
            && begin.source_offset <= document_navigation_detail::physical_line_range(*raw, 0).end) {
            // A heading indented as one owner includes a hidden Setext line.
            // Recognize its complete inverse when Shift+Tab is invoked on the
            // first code line; otherwise ordinary code outdents only that line.
            std::vector<TextEdit> whole_heading;
            auto unindented = *raw;
            for (std::size_t offset = 0; offset < raw->size();) {
                const auto line = document_navigation_detail::physical_line_range(*raw, offset);
                auto cursor = line.start;
                std::size_t columns = 0;
                while (cursor < line.end && columns < 4) {
                    if ((*raw)[cursor] == U' ') ++columns;
                    else if ((*raw)[cursor] == U'\t') columns += 4 - columns % 4;
                    else break;
                    ++cursor;
                }
                if (cursor != line.start) whole_heading.push_back({id, {line.start, cursor}, {}});
                if (line.end == raw->size()) break;
                offset = document_navigation_detail::after_line_ending(*raw, line.end);
            }
            for (auto edit = whole_heading.rbegin(); edit != whole_heading.rend(); ++edit)
                apply_text_edit(unindented, *edit);
            const auto heading = plan_block_fragment(unindented, document.dialect, [](const BlockVec& blocks) {
                return blocks.size() == 1 && blocks.front().kind == BlockKind::Heading;
            });
            if (heading && serialize_markdown_fragment(heading->blocks) == unindented) {
                planned.insert(planned.end(), std::make_move_iterator(whole_heading.begin()),
                    std::make_move_iterator(whole_heading.end()));
                changed_owners.push_back(id);
                continue;
            }
        }
        const auto edits_before = planned.size();
        auto text = raw ? std::u32string_view(block->block_source.tree().content)
            : std::u32string_view(inlines->source);
        auto to_content = [&](std::size_t offset) {
            if (!raw) return (std::min)(offset, text.size());
            const auto& mapping = block->block_source.tree().content_to_source;
            const auto mapped = mapping.lower_bound(offset);
            return mapped == mapping.end() ? text.size()
                : static_cast<std::size_t>(mapped - mapping.begin());
        };
        auto to_source = [&](std::size_t offset) {
            return raw ? block->block_source.tree().content_to_source[offset] : offset;
        };
        const auto local_start = index == *first ? to_content(begin.source_offset) : 0;
        const auto local_end = index == *last ? to_content(end.source_offset) : text.size();
        // An endpoint at the next owner's start does not select that owner.
        if (!selection.is_caret() && index == *last && index != *first && local_end == 0) continue;
        auto line_start = document_navigation_detail::physical_line_range(text, local_start).start;
        while (line_start <= local_end) {
            if (!selection.is_caret() && line_start == local_end && line_start != local_start) break;
            const auto line = document_navigation_detail::physical_line_range(text, line_start);
            if (!backward) {
                const auto at = to_source(line.start);
                planned.push_back({id, {at, at}, U"    "});
            } else {
                // The four required source columns of an indented code block
                // also participate in Shift+Tab. With extra visible indentation
                // this is a literal body outdent; without it the local reparse
                // below converts the affected content back into Markdown.
                const bool indented_code = raw && block->block_source.tree().kind == BlockSourceKind::IndentedCode;
                const auto leading_text = indented_code ? std::u32string_view(*raw) : text;
                const auto leading_line = indented_code
                    ? document_navigation_detail::physical_line_range(leading_text, to_source(line.start)) : line;
                auto cursor = leading_line.start;
                std::size_t columns = 0;
                while (cursor < leading_line.end && columns < 4) {
                    if (leading_text[cursor] == U' ') ++columns;
                    else if (leading_text[cursor] == U'\t') columns += 4 - columns % 4;
                    else break;
                    ++cursor;
                }
                if (cursor != leading_line.start) {
                    const SourceRange range = indented_code ? SourceRange{leading_line.start, cursor}
                        : SourceRange{to_source(line.start), to_source(cursor)};
                    if (!range.empty()) planned.push_back({id, range, {}});
                }
            }
            if (line.end == text.size()) break;
            line_start = document_navigation_detail::after_line_ending(text, line.end);
        }
        if (planned.size() != edits_before) changed_owners.push_back(id);
    }
    if (planned.empty() && heading_prefixes.empty()) return std::nullopt;
    const auto revision = document.revision;
    auto after = selection;
    document_edit_detail::NodeAllocator allocator(document);
    std::vector<DocumentOperation> operations;
    document_edit_detail::MutationRollback rollback(document, operations, revision);
    for (const auto id : heading_prefixes) {
        auto* block = find_document_block(document, id);
        if (!block) return std::nullopt;
        DocumentTreeEdit update;
        update.kind = DocumentTreeEditKind::UpdatePayload;
        update.before = document_transaction_detail::payload_shell(*block);
        auto& syntax = block->ensure_text_special();
        syntax.opening_marker.insert(0, U"    ");
        auto& closing = syntax.closing_marker;
        for (std::size_t offset = 0; offset < closing.size();) {
            const auto line = document_navigation_detail::physical_line_range(closing, offset);
            if (line.end == closing.size()) break;
            offset = document_navigation_detail::after_line_ending(closing, line.end);
            if (offset < closing.size()) {
                closing.insert(offset, U"    ");
                offset += 4;
            }
        }
        update.after = document_transaction_detail::payload_shell(*block);
        operations.emplace_back(std::move(update));
    }
    // Reverse source order keeps every planned block-local range valid and
    // does not require rebuilding document indexes or projecting the document.
    for (auto edit = planned.rbegin(); edit != planned.rend(); ++edit) {
        auto applied = document_edit_detail::edit_block_source(document, edit->container_id,
            edit->range, std::move(edit->replacement), allocator);
        if (!applied) return std::nullopt;
        after = translate_selection(after, applied->forward);
        document_edit_detail::append_source_operation(operations, std::move(*applied));
    }
    if (!backward && selection.is_caret() && heading_prefixes.empty()) {
        after.anchor = after.active = {selection.active.container_id,
            planned.front().range.start + 4, TextAffinity::Downstream};
    }
    if (!document_indentation_detail::reclassify_owners(document, changed_owners, after, allocator, operations))
        return std::nullopt;
    ++document.revision;
    rollback.commit();
    return make_recorded_document_transaction(std::move(operations), selection, after,
        revision, document.revision, DocumentTransactionReason::Format);
}

} // namespace folia
