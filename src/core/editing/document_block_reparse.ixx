// folia.core.document_block_reparse — local block-structure recognition after
// edits to a direct block's authoritative source. This reparses one block
// fragment only; it never serializes or reparses the complete document.
export module folia.core.document_block_reparse;
import std;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.core.document_container_source;
import folia.core.document_edit_support;
import folia.markdown.document_text;
import folia.markdown.ids;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.text_edit;

export namespace folia::document_edit_detail {

namespace block_reparse_detail {

inline const BlockNode* fragment_block(const BlockVec& blocks, NodeId id) {
    for (const auto& block : blocks) {
        if (block.id == id) return &block;
        if (const auto* found = fragment_block(block.children, id)) return found;
    }
    return nullptr;
}

inline bool contains_structural_reclassification(const BlockNode& block) {
    if (block.kind == BlockKind::CodeBlock || block.kind == BlockKind::MathBlock
        || block.has_html_source()) {
        return true;
    }
    return std::ranges::any_of(block.children, [](const auto& child) {
        return contains_structural_reclassification(child);
    });
}

inline bool contains_structural_reclassification(const BlockVec& blocks) {
    return std::ranges::any_of(blocks, [](const auto& block) {
        return contains_structural_reclassification(block);
    });
}

inline bool same_raw_projection(const BlockNode& current, const BlockVec& parsed) {
    if (parsed.size() != 1) return false;
    const auto& candidate = parsed.front();
    if (candidate.kind != current.kind || candidate.block_source.source() != current.block_source.source()) {
        return false;
    }
    if (candidate.block_source.tree().kind != current.block_source.tree().kind) return false;
    if (candidate.kind == BlockKind::MathBlock
        && candidate.atomic_special().math_delim != current.atomic_special().math_delim) return false;
    return true;
}

inline bool range_overlaps(SourceRange left, SourceRange right) {
    if (left.empty()) return right.covers(left.start);
    return left.start < right.end && right.start < left.end;
}

inline bool edit_touches_raw_structure(
    const BlockNode& current,
    const AppliedSourceEdit& edit) {
    if (edit.forward.container_id != current.id) return false;
    auto previous_source = current.block_source.source();
    apply_text_edit(previous_source, edit.inverse);
    const auto previous = parse_block_source(
        previous_source,
        current.block_source.tree().kind);
    return std::ranges::any_of(previous.tokens, [&](const auto& token) {
        const auto structural = token.kind == BlockSourceTokenKind::OpeningMarker
            || token.kind == BlockSourceTokenKind::Indentation;
        return structural && range_overlaps(edit.forward.range, token.source_range);
    });
}

struct TargetCandidate {
    NodeId parser_id{};
    std::size_t local_offset = 0;
    std::size_t distance = (std::numeric_limits<std::size_t>::max)();
    std::size_t physical_length = (std::numeric_limits<std::size_t>::max)();
};

inline std::optional<TargetCandidate> target_candidate(
    const ParsedBlockFragment& fragment,
    std::size_t physical_offset) {
    std::optional<TargetCandidate> best;
    for (const auto& range : fragment.source_ranges) {
        const auto* block = fragment_block(fragment.blocks, range.node_id);
        if (!block) continue;
        const auto* inline_document = editable_inline_document(*block);
        const auto* raw_source = editable_raw_block_source(*block);
        if (!inline_document && !raw_source) continue;

        const auto physical = raw_source ? range.source_range : range.content_range;
        const auto local_length = raw_source ? raw_source->size() : inline_document->source.size();
        std::size_t distance = 0;
        if (physical_offset < physical.start) distance = physical.start - physical_offset;
        else if (physical_offset > physical.end) distance = physical_offset - physical.end;
        const auto clamped = (std::clamp)(physical_offset, physical.start, physical.end);
        const auto local_offset = (std::min)(clamped - physical.start, local_length);
        const auto candidate = TargetCandidate{
            range.node_id,
            local_offset,
            distance,
            physical.length(),
        };
        if (!best || candidate.distance < best->distance
            || (candidate.distance == best->distance
                && candidate.physical_length < best->physical_length)) {
            best = candidate;
        }
    }
    return best;
}

inline void assign_fresh_ids(
    BlockNode& block,
    NodeId selected_parser_id,
    NodeId preserved_id,
    const EditorDocument& owner,
    NodeAllocator& allocator,
    std::vector<std::pair<NodeId, NodeId>>& remapped_ids) {
    const auto parser_id = block.id;
    block.id = parser_id == selected_parser_id ? preserved_id : allocator.allocate();
    remapped_ids.emplace_back(parser_id, block.id);
    if (auto* inline_document = editable_inline_document(block)) {
        reparse(*inline_document, owner, allocator);
    }
    for (auto& child : block.children) {
        assign_fresh_ids(
            child,
            selected_parser_id,
            preserved_id,
            owner,
            allocator,
            remapped_ids);
    }
    if (block.has_container_source()) {
        auto& lines = block.ensure_container_source().lines;
        for (auto& line : lines) {
            const auto mapping = std::ranges::find(
                remapped_ids,
                line.anchor.owner_id,
                &std::pair<NodeId, NodeId>::first);
            if (mapping != remapped_ids.end()) line.anchor.owner_id = mapping->second;
        }
    }
    if (block.has_html_source()) {
        auto& html = block.ensure_html_special();
        for (auto& slot : html.content_slots) {
            const auto mapping = std::ranges::find(remapped_ids, slot.owner_id, &std::pair<NodeId, NodeId>::first);
            if (mapping != remapped_ids.end()) slot.owner_id = mapping->second;
        }
    }
    if (block.kind == BlockKind::TableRow && block.table_special().row_syntax) {
        auto& syntax = *block.ensure_table_special().row_syntax;
        auto remap_owners = [&](std::vector<NodeId>& owners) {
            for (auto& owner_id : owners) {
                const auto mapping = std::ranges::find(
                    remapped_ids,
                    owner_id,
                    &std::pair<NodeId, NodeId>::first);
                if (mapping != remapped_ids.end()) owner_id = mapping->second;
            }
        };
        remap_owners(syntax.cell_owners);
        remap_owners(syntax.implicit_cell_owners);
    }
}

} // namespace block_reparse_detail

// Reclassify an edited block when its source changes structural meaning. A
// paragraph is reconsidered only for multiline code/math candidates or for a
// cheap '<...>' HTML candidate, so ordinary character input remains a
// one-node inline CST reparse.
enum class BlockReparseScope { SingleBlock, Fragment };

// Parser-local ids belong to this plan until application. Planning neither
// mutates the document nor allocates document ids; application never reparses
// the Markdown fragment.
struct BlockReparsePlan {
    NodeId owner{};
    BlockVec blocks;
    NodeId target_parser_id{};
    std::size_t target_offset = 0;
};

inline std::optional<BlockReparsePlan> plan_block_reparse(
    const EditorDocument& document,
    const BlockNode& current,
    std::u32string_view source,
    std::size_t target_offset,
    BlockReparseScope scope) {

    const auto is_raw = current.kind == BlockKind::CodeBlock
        || current.kind == BlockKind::MathBlock;
    const auto is_multiline_paragraph = current.kind == BlockKind::Paragraph
        && source.find(U'\n') != std::u32string::npos;
    const auto is_html_paragraph = current.kind == BlockKind::Paragraph
        && source.starts_with(U"<") && source.find(U'>') != std::u32string::npos;
    if (!is_raw && !is_multiline_paragraph && !is_html_paragraph) return std::nullopt;
    auto parsed = parse_block_fragment(source, document.dialect);
    if (is_raw && block_reparse_detail::same_raw_projection(current, parsed.blocks)) {
        return std::nullopt;
    }
    if (!is_raw
        && !block_reparse_detail::contains_structural_reclassification(parsed.blocks)) {
        return std::nullopt;
    }

    auto target = block_reparse_detail::target_candidate(parsed, target_offset);
    // A character edit may reclassify one direct block, but it is not an
    // implicit multi-block split command. Structural paste can explicitly
    // replace the owner with a fragment containing sibling blocks.
    const auto exact = (scope == BlockReparseScope::Fragment || parsed.blocks.size() == 1)
        && serialize_markdown_fragment(parsed.blocks) == source;
    if (!exact || !target) {
        if (!is_raw) return std::nullopt;
        parsed.blocks.clear();
        parsed.source_ranges.clear();
        BlockNode paragraph;
        paragraph.kind = BlockKind::Paragraph;
        paragraph.inline_content.source = source;
        parsed.blocks.push_back(std::move(paragraph));
        target = block_reparse_detail::TargetCandidate{
            {},
            (std::min)(target_offset, source.size()),
            0,
            source.size(),
        };
    }
    parsed.blocks.front().separator_before = current.separator_before;
    return BlockReparsePlan{current.id, std::move(parsed.blocks), target->parser_id, target->local_offset};
}

inline std::optional<BlockReparsePlan> plan_edited_direct_block(
    const EditorDocument& document, TextPosition position,
    BlockReparseScope scope = BlockReparseScope::SingleBlock,
    const AppliedSourceEdit* source_edit = nullptr) {
    const auto* current = find_document_block(document, position.container_id);
    if (!current) return std::nullopt;
    const auto* raw = editable_raw_block_source(*current);
    if (raw && source_edit
        && !block_reparse_detail::edit_touches_raw_structure(*current, *source_edit)) return std::nullopt;
    const auto* inlines = editable_inline_document(*current);
    if (!raw && !inlines) return std::nullopt;
    return plan_block_reparse(document, *current, raw ? *raw : inlines->source,
        position.source_offset, scope);
}

// A paste may continue a structure begun in an inline owner. Keep the parsed
// result for application rather than treating recognition as a boolean probe.
inline std::optional<BlockReparsePlan> plan_block_source_insertion(
    const EditorDocument& document, TextPosition position, std::u32string_view text) {
    const auto* current = find_document_block(document, position.container_id);
    if (!current || current->kind != BlockKind::Paragraph || text.find(U'\n') == text.npos)
        return std::nullopt;
    auto source = current->inline_content.source;
    if (position.source_offset > source.size()) return std::nullopt;
    source.insert(position.source_offset, text);
    return plan_block_reparse(document, *current, source,
        position.source_offset + text.size(), BlockReparseScope::Fragment);
}

inline std::optional<RecordedBlockEdit> apply_block_reparse_plan(
    EditorDocument& document, BlockReparsePlan plan, NodeAllocator& allocator) {
    auto path = document_block_path(document, plan.owner);
    if (!path || path->empty() || plan.blocks.empty()) return std::nullopt;

    auto parent_path = *path;
    const auto index = parent_path.back();
    parent_path.pop_back();
    auto* parent = block_at_path(document.root, parent_path);
    if (!parent || index >= parent->children.size()) return std::nullopt;
    const auto parent_id = parent->id;
    for (auto& block : plan.blocks) {
        std::vector<std::pair<NodeId, NodeId>> remapped_ids;
        block_reparse_detail::assign_fresh_ids(block, plan.target_parser_id,
            plan.owner, document, allocator, remapped_ids);
    }

    RecordedBlockEdit result;
    result.target = {
        plan.owner,
        plan.target_offset,
        TextAffinity::Downstream,
    };

    DocumentTreeEdit remove;
    remove.kind = DocumentTreeEditKind::Remove;
    remove.parent_id = parent_id;
    remove.index = index;
    remove.before = parent->children[index];
    document_container_source::append_container_tree_operation(document, result.operations, std::move(remove));
    parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(index));

    for (std::size_t replacement_index = 0;
         replacement_index < plan.blocks.size();
         ++replacement_index) {
        parent = find_block(document.root, parent_id);
        if (!parent) return std::nullopt;
        auto replacement = std::move(plan.blocks[replacement_index]);
        DocumentTreeEdit insert;
        insert.kind = DocumentTreeEditKind::Insert;
        insert.parent_id = parent_id;
        insert.index = index + replacement_index;
        insert.after = replacement;
        result.operations.emplace_back(std::move(insert));
        if (!insert_block(*parent, index + replacement_index, std::move(replacement))) {
            return std::nullopt;
        }
    }
    return result;
}

inline std::optional<RecordedBlockEdit> reparse_edited_direct_block(
    EditorDocument& document, TextPosition position, NodeAllocator& allocator,
    const AppliedSourceEdit* source_edit = nullptr) {
    auto plan = plan_edited_direct_block(document, position, BlockReparseScope::SingleBlock, source_edit);
    if (!plan) return std::nullopt;
    return apply_block_reparse_plan(document, std::move(*plan), allocator);
}

} // namespace folia::document_edit_detail
