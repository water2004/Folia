// Serializer action visitor; the single block formatting implementation. The public
// serializer facade imports this module and exposes only the established API.
export module folia.markdown.serializer_visitor;
import std;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.container_source;
import folia.markdown.document_text;
import folia.markdown.document;
import folia.markdown.ids;
import folia.markdown.instrumentation;
import folia.markdown.serializer_writer;
import folia.markdown.utf;

export namespace folia::serializer_detail {

inline bool serializer_empty_paragraph(BlockNode const& block) {
    return block.kind == BlockKind::Paragraph && block.inline_content.source.empty();
}

inline std::u32string serializer_html_escape_attribute(std::u32string_view value) {
    std::u32string result;
    result.reserve(value.size());
    for (const auto cp : value) {
        switch (cp) {
            case U'&': result += U"&amp;"; break;
            case U'<': result += U"&lt;"; break;
            case U'>': result += U"&gt;"; break;
            case U'\"': result += U"&quot;"; break;
            case U'\'': result += U"&#39;"; break;
            default: result.push_back(cp); break;
        }
    }
    return result;
}

class SerializerActionVisitor {
public:
    SerializerEventWriter& writer() noexcept { return writer_; }

    void serialize(BlockVec const& blocks, std::size_t begin=0,
        std::size_t end=(std::numeric_limits<std::size_t>::max)()) {
        push_blocks(blocks, begin, end, false, false, true);
        run();
    }

private:
    enum class ActionKind : std::uint8_t {
        VisitBlock,
        VisitBlocks,
        EnterListItem,
        ContinueTableRows,
        EmitTableRow,
        FinishBlock,
        EndPrefixScope,
        EndAnchorBeforeEnding,
        EmitGenerated,
        SiblingSeparator,
    };
    struct Action {
        ActionKind kind = ActionKind::VisitBlock;
        BlockNode const* block = nullptr;
        BlockVec const* blocks = nullptr;
        std::size_t begin = 0;
        std::size_t end = 0;
        std::size_t index = 0;
        bool html_semantic = false;
        bool header_cell = false;
        bool emit_block_anchors = true;
        bool list_item_children = false;
        NodeId owner{};
        NodeId previous_owner{};
        bool previous_empty_paragraph = false;
        bool next_is_list = false;
        std::optional<std::u32string> separator;
        std::u32string text;
        SerializerPrefixPolicy policy{};
    };

    static bool is_list_block(BlockNode const& block) {
        return block.kind == BlockKind::List || block.kind == BlockKind::TaskList;
    }

    void push(Action action) { actions_.push_back(std::move(action)); }

    void push_blocks(BlockVec const& blocks, std::size_t begin, std::size_t end,
        bool html_semantic, bool header_cell, bool emit_block_anchors,
        bool list_item_children = false) {
        Action action;
        action.kind = ActionKind::VisitBlocks;
        action.blocks = &blocks;
        action.begin = begin;
        action.end = (std::min)(end, blocks.size());
        action.index = (std::min)(begin, action.end);
        action.html_semantic = html_semantic;
        action.header_cell = header_cell;
        action.emit_block_anchors = emit_block_anchors;
        action.list_item_children = list_item_children;
        push(std::move(action));
    }

    void push_block(BlockNode const& block, bool html_semantic = false,
        bool header_cell = false, bool emit_block_anchors = true) {
        Action action;
        action.kind = ActionKind::VisitBlock;
        action.block = &block;
        action.html_semantic = html_semantic;
        action.header_cell = header_cell;
        action.emit_block_anchors = emit_block_anchors;
        push(std::move(action));
    }

    void push_anchor_before_end(NodeId owner) {
        Action action;
        action.kind = ActionKind::EndAnchorBeforeEnding;
        action.owner = owner;
        push(std::move(action));
    }

    void run() {
        while (!actions_.empty()) {
            auto action = std::move(actions_.back());
            actions_.pop_back();
            switch (action.kind) {
                case ActionKind::VisitBlock:
                    enter_block(*action.block, action.html_semantic,
                        action.header_cell, action.emit_block_anchors);
                    break;
                case ActionKind::VisitBlocks:
                    visit_blocks(action);
                    break;
                case ActionKind::EnterListItem:
                    enter_list_item(*action.block, std::move(action.text));
                    break;
                case ActionKind::ContinueTableRows:
                    continue_table_rows(*action.block, action.index);
                    break;
                case ActionKind::EmitTableRow:
                    emit_table_row(*action.block);
                    break;
                case ActionKind::FinishBlock:
                    writer_.syntax_anchor_before_ending(
                        {action.block->id, ContainerAnchorPart::End, 0});
                    writer_.block_completion(action.block->id);
                    break;
                case ActionKind::EndPrefixScope:
                    writer_.end_prefix_scope(action.block->id);
                    break;
                case ActionKind::EndAnchorBeforeEnding:
                    writer_.syntax_anchor_before_ending({action.owner, ContainerAnchorPart::End, 0});
                    break;
                case ActionKind::EmitGenerated:
                    writer_.generated_span(action.text);
                    break;
                case ActionKind::SiblingSeparator:
                    writer_.sibling_separator(action.previous_owner, action.owner,
                        action.previous_empty_paragraph, action.next_is_list,
                        std::move(action.separator), action.list_item_children);
                    break;
            }
        }
    }

    void visit_blocks(Action const& action) {
        if (action.index >= action.end) return;
        const auto& current = (*action.blocks)[action.index];
        if (action.index + 1 < action.end) {
            Action next = action;
            ++next.index;
            push(std::move(next));
        }
        if (!action.html_semantic && action.index > action.begin) {
            const auto& previous = (*action.blocks)[action.index - 1];
            writer_.sibling_separator(previous.id, current.id,
                serializer_empty_paragraph(previous),
                action.list_item_children && is_list_block(current),
                current.separator_before, action.list_item_children);
        }
        push_block(current, action.html_semantic, action.header_cell,
            action.emit_block_anchors);
    }

    std::u32string list_marker(BlockNode const& list, BlockNode const& item,
        std::size_t index) const {
        const auto& special = item.item_special();
        if (!special.marker.empty()) return special.marker;
        if (item.kind == BlockKind::TaskListItem) {
            return special.checked ? U"- [x] " : U"- [ ] ";
        }
        const auto& list_special = list.list_special();
        if (list_special.ordered) {
            return utf8_to_cps(std::to_string(list_special.start + index))
                + std::u32string(1, list_special.delimiter) + U" ";
        }
        return U"- ";
    }

    void enter_list_item(BlockNode const& item, std::u32string marker) {
        SerializerPrefixPolicy policy;
        policy.rule = SerializerPrefixRule::ListItem;
        policy.marker = std::move(marker);
        writer_.begin_prefix_scope(item.id, item.container_source(), std::move(policy));
        writer_.syntax_anchor({item.id, ContainerAnchorPart::Start, 0});
        Action close;
        close.kind = ActionKind::EndPrefixScope;
        close.block = &item;
        push(std::move(close));
        Action end_anchor;
        end_anchor.kind = ActionKind::EndAnchorBeforeEnding;
        end_anchor.owner = item.id;
        push(std::move(end_anchor));
        push_blocks(item.children, 0, item.children.size(), false, false, true, true);
    }

    void schedule_list(BlockNode const& list) {
        for (std::size_t reverse = list.children.size(); reverse > 0; --reverse) {
            const auto index = reverse - 1;
            Action item;
            item.kind = ActionKind::EnterListItem;
            item.block = &list.children[index];
            item.text = list_marker(list, list.children[index], index);
            push(std::move(item));
            if (index > 0) {
                Action separator;
                separator.kind = ActionKind::SiblingSeparator;
                separator.previous_owner = list.children[index - 1].id;
                separator.owner = list.children[index].id;
                separator.next_is_list = true;
                separator.separator = list.children[index].separator_before.value_or(U"\n");
                separator.list_item_children = true;
                push(std::move(separator));
            }
        }
    }

    void continue_table_rows(BlockNode const& table, std::size_t index) {
        const auto& special = table.table_special();
        if (index == 1) {
            if (special.table_internal_line_endings.size() == table.children.size()) {
                writer_.generated_span(special.table_internal_line_endings[0]);
            } else writer_.generated_span(U"\n");
            if (!special.table_separator_source.empty()) {
                writer_.generated_span(special.table_separator_source);
            } else {
                std::u32string separator = U"|";
                const auto width = table.children.front().children.size();
                for (std::size_t cell = 0; cell < width; ++cell) {
                    const auto alignment = cell < special.table_aligns.size()
                        ? special.table_aligns[cell] : TableAlignment::None;
                    if (alignment == TableAlignment::Left) separator += U" :--- |";
                    else if (alignment == TableAlignment::Center) separator += U" :---: |";
                    else if (alignment == TableAlignment::Right) separator += U" ---: |";
                    else separator += U" --- |";
                }
                writer_.generated_owned(std::move(separator));
            }
        }
        if (index >= table.children.size()) return;
        if (special.table_internal_line_endings.size() == table.children.size()) {
            writer_.generated_span(special.table_internal_line_endings[index]);
        } else writer_.generated_span(U"\n");
        Action next;
        next.kind = ActionKind::ContinueTableRows;
        next.block = &table;
        next.index = index + 1;
        push(std::move(next));
        Action row;
        row.kind = ActionKind::EmitTableRow;
        row.block = &table.children[index];
        push(std::move(row));
    }

    bool html_structure_is_original(BlockNode const& root) const {
        std::vector<HtmlBlockShapeEntry> shape;
        std::vector<std::pair<BlockNode const*, std::size_t>> pending{{&root, 0}};
        while (!pending.empty()) {
            const auto [block, depth] = pending.back();
            pending.pop_back();
            shape.push_back({block->kind, depth, block->children.size()});
            for (std::size_t index = block->children.size(); index > 0; --index) {
                pending.push_back({&block->children[index - 1], depth + 1});
            }
        }

        std::vector<NodeId> editable_order;
        std::vector<BlockNode const*> nodes{&root};
        while (!nodes.empty()) {
            const auto* block = nodes.back();
            nodes.pop_back();
            if (editable_inline_document(*block) || editable_raw_block_source(*block)) {
                editable_order.push_back(block->id);
            }
            for (std::size_t index = block->children.size(); index > 0; --index) {
                nodes.push_back(&block->children[index - 1]);
            }
        }
        auto slots = root.html_special().content_slots;
        std::ranges::sort(slots, [](const auto& left, const auto& right) {
            if (left.source_range.start != right.source_range.start) {
                return left.source_range.start < right.source_range.start;
            }
            return left.source_range.end < right.source_range.end;
        });
        std::vector<NodeId> original_order;
        original_order.reserve(slots.size());
        for (const auto& slot : slots) original_order.push_back(slot.owner_id);
        return shape == root.html_special().structure_shape && original_order == editable_order;
    }

    std::unordered_map<std::uint64_t, BlockNode const*> html_owner_index(BlockNode const& root) const {
        std::unordered_map<std::uint64_t, BlockNode const*> owners;
        std::vector<BlockNode const*> pending{&root};
        while (!pending.empty()) {
            const auto* block = pending.back();
            pending.pop_back();
            owners.try_emplace(block->id.v, block);
            for (std::size_t index = block->children.size(); index > 0; --index) {
                pending.push_back(&block->children[index - 1]);
            }
        }
        return owners;
    }

    void emit_html_envelope(BlockNode const& block) {
        const auto& html = block.html_special();
        if (html.source.empty()) return;
        if (!html_structure_is_original(block)) {
            push_block(block, true, false, false);
            return;
        }
        auto slots = html.content_slots;
        std::ranges::sort(slots, [](const auto& left, const auto& right) {
            if (left.source_range.start != right.source_range.start) {
                return left.source_range.start < right.source_range.start;
            }
            return left.source_range.end < right.source_range.end;
        });
        std::size_t validated_cursor = 0;
        for (const auto& slot : slots) {
            if (!slot.source_range.valid_for(html.source.size())
                || slot.source_range.start < validated_cursor) {
                writer_.generated_span(html.source);
                return;
            }
            validated_cursor = slot.source_range.end;
        }
        const auto owners = html_owner_index(block);
        std::size_t cursor = 0;
        for (const auto& slot : slots) {
            writer_.generated_span(std::u32string_view(html.source).substr(
                cursor, slot.source_range.start - cursor));
            const auto found = owners.find(slot.owner_id.v);
            const auto* owner = found == owners.end() ? nullptr : found->second;
            const auto* inline_document = owner ? editable_inline_document(*owner) : nullptr;
            const auto* raw_source = owner ? editable_raw_block_source(*owner) : nullptr;
            if (inline_document) writer_.source_span(owner->id, inline_document->source);
            else if (raw_source) writer_.source_span(owner->id, *raw_source);
            else writer_.generated_span(std::u32string_view(html.source).substr(
                slot.source_range.start, slot.source_range.length()));
            cursor = slot.source_range.end;
        }
        writer_.generated_span(std::u32string_view(html.source).substr(cursor));
    }

    bool html_provenance_matches_kind(BlockNode const& block, std::string_view fallback) const {
        if (!block.has_html_element_provenance()) return false;
        const auto& tag = block.html_special().root_tag;
        if (block.kind == BlockKind::HtmlContainer) return true;
        if (block.kind == BlockKind::Paragraph) {
            return tag == "p" || tag == "summary" || tag == "dt" || tag == "dd";
        }
        return tag == fallback;
    }

    void wrap_semantic_html(BlockNode const& block, std::string fallback,
        std::optional<std::pair<bool, bool>> children = std::nullopt,
        std::u32string const* source = nullptr) {
        const auto& html = block.html_special();
        const bool preserve = html_provenance_matches_kind(block, fallback);
        const auto tag = preserve ? html.root_tag : std::move(fallback);
        const auto opening = preserve && !html.opening_marker.empty()
            ? html.opening_marker
            : U"<" + utf8_to_cps(tag) + U">";
        const auto closing = preserve && !html.closing_marker.empty()
            ? html.closing_marker
            : U"</" + utf8_to_cps(tag) + U">";
        writer_.generated_span(opening);
        Action close;
        close.kind = ActionKind::EmitGenerated;
        close.text = closing;
        push(std::move(close));
        if (source != nullptr) {
            writer_.source_span(block.id, *source);
        } else if (children) {
            const auto [html_semantic, header_cell] = *children;
            push_blocks(block.children, 0, block.children.size(), html_semantic,
                header_cell, false);
        }
    }

    void enter_semantic_html(BlockNode const& block, bool header_cell) {
        using BK = BlockKind;
        switch (block.kind) {
            case BK::Paragraph:
                if (block.has_html_element_provenance()) {
                    wrap_semantic_html(block, "p", std::nullopt, &block.inline_content.source);
                } else writer_.source_span(block.id, block.inline_content.source);
                return;
            case BK::Heading: {
                const auto level = (std::clamp)(block.text_special().level,
                    std::uint8_t{1}, std::uint8_t{6});
                wrap_semantic_html(block, std::string{"h"} + static_cast<char>('0' + level),
                    std::nullopt, &block.inline_content.source);
                return;
            }
            case BK::HtmlContainer:
                if (block.has_html_element_provenance()) wrap_semantic_html(block, "div", std::pair{true, false});
                else push_blocks(block.children, 0, block.children.size(), true, false, false);
                return;
            case BK::BlockQuote:
                wrap_semantic_html(block, "blockquote", std::pair{true, false}); return;
            case BK::List:
            case BK::TaskList:
                wrap_semantic_html(block, block.list_special().ordered ? "ol" : "ul", std::pair{true, false}); return;
            case BK::ListItem:
            case BK::TaskListItem:
                wrap_semantic_html(block, "li", std::pair{true, false}); return;
            case BK::Table:
                wrap_semantic_html(block, "table", std::pair{true, false}); return;
            case BK::TableRow:
                wrap_semantic_html(block, "tr", std::pair{true, block.table_special().table_header_row}); return;
            case BK::TableCell:
                wrap_semantic_html(block, header_cell ? "th" : "td",
                    std::nullopt, &block.inline_content.source); return;
            case BK::ThematicBreak:
                if (!block.html_special().opening_marker.empty()) {
                    writer_.generated_span(block.html_special().opening_marker);
                } else writer_.generated_span(U"<hr>");
                return;
            case BK::CodeBlock:
            case BK::MathBlock:
                writer_.source_span(block.id, block.block_source.source()); return;
            case BK::Callout:
            case BK::FootnoteDefinition:
            case BK::Document:
                push_blocks(block.children, 0, block.children.size(), true, false, false); return;
            case BK::CalloutTitle:
                writer_.source_span(block.id, block.inline_content.source); return;
            case BK::ImageBlock: {
                const auto& image = block.image_special();
                auto value = U"<img src=\"" + serializer_html_escape_attribute(utf8_to_cps(image.src)) + U"\"";
                if (!image.image_alt.empty()) {
                    value += U" alt=\"" + serializer_html_escape_attribute(utf8_to_cps(image.image_alt)) + U"\"";
                }
                writer_.atomic_generated_span(block.id, value + U">");
                return;
            }
            case BK::LinkDefinition:
            case BK::UnsupportedMarkup:
                writer_.atomic_generated_span(block.id, utf8_to_cps(block.atomic_special().raw)); return;
            case BK::Toc:
            case BK::Frontmatter:
            case BK::Extension:
                writer_.source_span(block.id, block.inline_content.source); return;
        }
    }

    bool row_syntax_matches(BlockNode const& row, BlockTableSpecial::RowSyntax const& syntax) const {
        if (syntax.gaps.size() != syntax.cell_owners.size() + 1
            || row.children.size() != syntax.cell_owners.size() + syntax.implicit_cell_owners.size()) return false;
        for (std::size_t index = 0; index < syntax.cell_owners.size(); ++index) {
            if (row.children[index].id != syntax.cell_owners[index]
                || row.children[index].kind != BlockKind::TableCell) return false;
        }
        for (std::size_t index = 0; index < syntax.implicit_cell_owners.size(); ++index) {
            const auto child_index = syntax.cell_owners.size() + index;
            if (row.children[child_index].id != syntax.implicit_cell_owners[index]
                || row.children[child_index].kind != BlockKind::TableCell
                || !row.children[child_index].inline_content.source.empty()) return false;
        }
        return true;
    }

    void emit_table_row(BlockNode const& row) {
        const auto& syntax = row.table_special().row_syntax;
        if (syntax && row_syntax_matches(row, *syntax)) {
            for (std::size_t index = 0; index < syntax->cell_owners.size(); ++index) {
                writer_.generated_span(syntax->gaps[index]);
                const auto& cell = row.children[index];
                writer_.source_span(cell.id, cell.inline_content.source);
            }
            writer_.generated_span(syntax->gaps.back());
            return;
        }
        writer_.generated_span(U"|");
        for (const auto& cell : row.children) {
            writer_.generated_span(U" ");
            writer_.source_span(cell.id, cell.inline_content.source);
            writer_.generated_span(U" |");
        }
    }

    void enter_block(BlockNode const& block, bool html_semantic,
        bool header_cell, bool emit_block_anchors) {
        if (emit_block_anchors) {
            writer_.block_begin(block.id);
            if (block.kind != BlockKind::BlockQuote || block.has_html_source() || html_semantic) {
                writer_.syntax_anchor({block.id, ContainerAnchorPart::Start, 0});
            }
            Action finish;
            finish.kind = ActionKind::FinishBlock;
            finish.block = &block;
            push(std::move(finish));
        }
        if (!html_semantic && block.has_html_source()) {
            emit_html_envelope(block);
            return;
        }
        if (html_semantic) {
            enter_semantic_html(block, header_cell);
            return;
        }

        using BK = BlockKind;
        switch (block.kind) {
            case BK::Document:
            case BK::HtmlContainer:
                push_blocks(block.children, 0, block.children.size(), false, false, true); return;
            case BK::Paragraph:
                writer_.source_span(block.id, block.inline_content.source); return;
            case BK::Heading: {
                const auto& special = block.text_special();
                if (!special.opening_marker.empty() || !special.closing_marker.empty()) {
                    writer_.generated_span(special.opening_marker);
                } else {
                    writer_.generated_owned(std::u32string(
                        special.level == 0 ? 1 : special.level, U'#') + U" ");
                }
                writer_.source_span(block.id, block.inline_content.source);
                writer_.generated_span(special.closing_marker);
                return;
            }
            case BK::BlockQuote: {
                SerializerPrefixPolicy policy;
                policy.rule = SerializerPrefixRule::BlockQuote;
                policy.explicit_empty_paragraph = !block.children.empty()
                    && serializer_empty_paragraph(block.children.back());
                writer_.begin_prefix_scope(block.id, block.container_source(), std::move(policy));
                writer_.syntax_anchor({block.id, ContainerAnchorPart::Start, 0});
                Action close;
                close.kind = ActionKind::EndPrefixScope;
                close.block = &block;
                push(std::move(close));
                push_anchor_before_end(block.id);
                push_blocks(block.children, 0, block.children.size(), false, false, true);
                return;
            }
            case BK::List:
            case BK::TaskList:
                schedule_list(block); return;
            case BK::ListItem:
            case BK::TaskListItem:
                push_blocks(block.children, 0, block.children.size(), false, false, true, true); return;
            case BK::CodeBlock:
            case BK::MathBlock:
                writer_.source_span(block.id, block.block_source.source()); return;
            case BK::Table:
                if (!block.children.empty()) {
                    Action tail;
                    tail.kind = ActionKind::ContinueTableRows;
                    tail.block = &block;
                    tail.index = 1;
                    push(std::move(tail));
                    Action row;
                    row.kind = ActionKind::EmitTableRow;
                    row.block = &block.children.front();
                    push(std::move(row));
                }
                return;
            case BK::TableRow:
                emit_table_row(block); return;
            case BK::TableCell:
                writer_.source_span(block.id, block.inline_content.source); return;
            case BK::ImageBlock:
                writer_.atomic_source_span(block.id, block.inline_content.source); return;
            case BK::Callout: {
                const auto& marker = block.text_special().opening_marker;
                auto first = marker.empty()
                    ? U"> [!" + utf8_to_cps(block.container_special().callout_kind) + U"]"
                    : marker;
                writer_.generated_span(first);
                const auto* title = callout_title_block(block);
                if (title) {
                    if (marker.empty()) writer_.generated_span(U" ");
                    writer_.source_span(title->id, title->inline_content.source);
                }
                const auto body_begin = callout_body_start(block);
                if (body_begin < block.children.size()) {
                    writer_.generated_span(block.children[body_begin].separator_before.value_or(U"\n"));
                    SerializerPrefixPolicy policy;
                    policy.rule = SerializerPrefixRule::FixedLines;
                    policy.fixed_prefix_on_first_line = true;
                    policy.fixed_prefix_on_empty_line = true;
                    policy.suppress_if_empty_body = true;
                    policy.nonempty_prefix = U"> ";
                    policy.empty_prefix = U">";
                    writer_.begin_prefix_scope(block.id, block.container_source(), std::move(policy));
                    Action close;
                    close.kind = ActionKind::EndPrefixScope;
                    close.block = &block;
                    push(std::move(close));
                    push_blocks(block.children, body_begin, block.children.size(), false, false, true);
                }
                return;
            }
            case BK::FootnoteDefinition: {
                const auto& special = block.text_special();
                const auto& container = block.container_special();
                const auto marker = special.opening_marker.empty()
                    ? U"[^" + utf8_to_cps(container.footnote_label) + U"]: "
                    : special.opening_marker;
                writer_.generated_span(marker);
                SerializerPrefixPolicy policy;
                policy.rule = SerializerPrefixRule::FixedLines;
                policy.fixed_prefix_on_first_line = false;
                policy.nonempty_prefix = std::u32string(4, U' ');
                policy.suppress_if_empty_body = true;
                writer_.begin_prefix_scope(block.id, no_container_prefixes_, std::move(policy));
                Action close;
                close.kind = ActionKind::EndPrefixScope;
                close.block = &block;
                push(std::move(close));
                push_blocks(block.children, 0, block.children.size(), false, false, true);
                return;
            }
            case BK::Toc:
                writer_.atomic_generated_span(block.id,
                    block.atomic_special().toc_marker == TocMarkerKind::WikiToc ? U"[[TOC]]" : U"[TOC]");
                return;
            case BK::Frontmatter: {
                const auto& special = block.atomic_special();
                const auto& syntax=block.text_special();
                auto opening=syntax.opening_marker;
                if (opening.empty()) opening=special.fmt==FrontmatterFormat::Toml ? U"---toml\n"
                    : special.fmt==FrontmatterFormat::Json ? U"---json\n" : U"---\n";
                auto body=utf8_to_cps(special.raw);
                if (syntax.opening_marker.empty() && !body.empty() && body.back()!=U'\n' && body.back()!=U'\r')
                    body.push_back(U'\n');
                writer_.atomic_generated_span(block.id,opening+body+
                    (syntax.closing_marker.empty() ? U"---" : syntax.closing_marker));
                return;
            }
            case BK::ThematicBreak: {
                const auto& marker = block.text_special().opening_marker;
                writer_.atomic_generated_span(block.id, marker.empty() ? U"---" : marker);
                return;
            }
            case BK::LinkDefinition:
            case BK::UnsupportedMarkup:
                writer_.atomic_generated_span(block.id, utf8_to_cps(block.atomic_special().raw));
                return;
            case BK::CalloutTitle:
                writer_.source_span(block.id, block.inline_content.source); return;
            case BK::Extension:
                writer_.atomic_generated_span(block.id,
                    U"[ext:" + utf8_to_cps(block.atomic_special().ext_name) + U"]");
                return;
        }
    }

    SerializerEventWriter writer_;
    std::vector<Action> actions_;
    ContainerSource no_container_prefixes_;
};

inline SerializerEmission serialize_blocks(BlockVec const& blocks, std::size_t begin=0,
    std::size_t end=(std::numeric_limits<std::size_t>::max)()) {
    SerializerActionVisitor visitor;
    visitor.serialize(blocks,begin,end);
    SerializerPlanArena arena;
    SerializerLinePlanner planner(visitor.writer(), arena);
    const auto output_lines = planner.build();
    auto result=emit_serializer_lines(arena, output_lines, visitor.writer().events());
    std::unordered_map<std::uint64_t,SerializerBlockRange> ranges;
    for (const auto& range:planner.block_ranges()) ranges.emplace(range.owner.v,range);
    end=(std::min)(end,blocks.size());
    for (auto i=begin;i<end;++i) {
        if (const auto found=ranges.find(blocks[i].id.v);found!=ranges.end())
            result.blocks.push_back(found->second);
    }
    return result;
}

inline void append_document_ending(SerializerEmission& result,const EditorDocument& document) {
    const bool trailing_empty_paragraph = !document.root.children.empty()
        && serializer_empty_paragraph(document.root.children.back());
    if (!document.trailing_line_ending.empty()
        && (result.text.empty()
            || (result.text.back() != U'\n' && result.text.back() != U'\r')
            || trailing_empty_paragraph)) {
        result.text.append(document.trailing_line_ending);
    }
}

inline SerializerEmission serialize_document(EditorDocument const& document) {
    record_full_document_serialization();
    auto result = serialize_blocks(document.root.children);
    append_document_ending(result,document);
    return result;
}

} // namespace folia::serializer_detail
