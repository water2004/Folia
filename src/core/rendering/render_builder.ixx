// folia.core.render_builder — derive a local-coordinate RenderModel directly
// from the authoritative block tree and each node's lossless inline source.
export module folia.core.render_builder;
import std;
import folia.core.document_transaction;
import folia.core.theme;
import folia.core.types;
import folia.markdown.ids;
import folia.markdown.dialect;
import folia.markdown.ast;
import folia.markdown.table_metadata;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.callout;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.symbols;
import folia.markdown.outline;
import folia.markdown.diagnostics;
import folia.markdown.utf;
import folia.core.render_model;
import folia.core.render_fragments;
import folia.core.html_inline_presentation;
import folia.core.html_inline_flow;
import folia.core.table_values;

export namespace folia {

inline RenderDiagnostic convert_diagnostic(const Diagnostic& d) {
    RenderDiagnostic r;
    r.severity = (d.severity == DiagnosticSeverity::Hint)   ? RenderDiagnostic::Sev::Info
               : (d.severity == DiagnosticSeverity::Error)   ? RenderDiagnostic::Sev::Error
               : RenderDiagnostic::Sev::Warning;
    r.message = d.message; r.source_span = d.source_span;
    return r;
}

namespace render_key_detail {

struct Hasher {
    std::uint64_t value = 1469598103934665603ull;

    void byte(std::uint8_t input) {
        value ^= input;
        value *= 1099511628211ull;
    }

    template <typename T>
        requires std::is_integral_v<T> || std::is_enum_v<T>
    void scalar(T input) {
        if constexpr (std::is_enum_v<T>) {
            scalar(static_cast<std::underlying_type_t<T>>(input));
        } else if constexpr (std::is_same_v<std::remove_cv_t<T>, bool>) {
            byte(input ? 1u : 0u);
        } else {
            using Unsigned = std::make_unsigned_t<T>;
            auto bits = static_cast<Unsigned>(input);
            for (std::size_t index = 0; index < sizeof(bits); ++index) {
                byte(static_cast<std::uint8_t>(bits & 0xffu));
                bits >>= 8;
            }
        }
    }

    void scalar(float input) { scalar(std::bit_cast<std::uint32_t>(input)); }
    void scalar(NodeId input) { scalar(input.v); }
    void scalar(SourceRange input) { scalar(input.start); scalar(input.end); }
    void scalar(TextSpan input) { scalar(input.container_id); scalar(input.source_range); }

    template <typename Character>
    void text(std::basic_string_view<Character> input) {
        scalar(input.size());
        for (auto character : input) scalar(character);
    }

    template <typename Character>
    void text(std::basic_string<Character> const& input) { text(std::basic_string_view<Character>{input}); }

    template <typename T, typename Append>
    void optional(std::optional<T> const& input, Append append) {
        scalar(input.has_value());
        if (input) append(*input);
    }
};

inline void append(Hasher& hash, InlineCstNode const& node) {
    hash.scalar(node.id); hash.scalar(node.kind); hash.scalar(node.range); hash.scalar(node.status);
    if (auto delim = node.stored_delimiters()) {
        hash.scalar(delim->opening); hash.scalar(delim->content);
        hash.optional(delim->closing, [&](auto value) { hash.scalar(value); });
    }
    hash.scalar(node.children.size());
    for (auto const& child : node.children) append(hash, child);
}

inline void append(Hasher& hash, InlineDocument const& document) {
    hash.text(document.source);
    hash.scalar(document.tree.nodes.size());
    for (auto const& node : document.tree.nodes) append(hash, node);
}

inline void append_metadata(Hasher& hash, BlockNode const& block) {
    hash.scalar(block.id); hash.scalar(block.kind);
    // The block tree stores kind-specific metadata sparsely. Do not serialize
    // dozens of default fields for every ordinary paragraph during a structural
    // index repair, and never inspect its inline source/CST here.
    auto const* payload = block.payload.get();
    auto const* supplemental = payload ? payload->supplemental.get() : nullptr;
    auto field = [&](auto const* value, auto append_value) {
        hash.scalar(value != nullptr);
        if (value) append_value(*value);
    };
    field(payload ? payload->text.get() : nullptr, [&](auto const& text) {
        hash.scalar(text.level); hash.text(text.slug);
        hash.text(text.opening_marker); hash.text(text.closing_marker);
    });
    field(payload ? payload->item.get() : nullptr, [&](auto const& item) {
        hash.text(item.marker); hash.scalar(item.checked);
    });
    field(payload ? payload->list.get() : nullptr, [&](auto const& list) {
        hash.scalar(list.ordered); hash.scalar(list.start); hash.scalar(list.delimiter);
    });
    field(payload ? payload->atomic.get() : nullptr, [&](auto const& atomic) {
        hash.scalar(atomic.code_indented); hash.scalar(atomic.math_delim);
        hash.scalar(atomic.toc_marker); hash.scalar(atomic.fmt);
        hash.scalar(atomic.unsup_reason); hash.text(atomic.ext_name);
    });
    field(payload ? payload->table.get() : nullptr, [&](auto const& table) {
        hash.scalar(table.table_aligns.size());
        for (auto alignment : table.table_aligns) hash.scalar(alignment);
        hash.scalar(table.table_header_row);
        hash.scalar(table.metadata ? table.metadata->source_hash : 0);
    });
    field(supplemental ? supplemental->image.get() : nullptr, [&](auto const& image) {
        hash.text(image.src); hash.text(image.image_alt);
        hash.optional(image.image_title, [&](auto const& value) { hash.text(value); });
        hash.optional(image.image_link, [&](auto const& value) { hash.text(value); });
        hash.optional(image.image_width, [&](auto const& value) {
            hash.scalar(value.value); hash.scalar(value.unit);
        });
        hash.optional(image.image_height, [&](auto const& value) {
            hash.scalar(value.value); hash.scalar(value.unit);
        });
    });
    field(supplemental ? supplemental->container.get() : nullptr, [&](auto const& container) {
        hash.text(container.callout_kind); hash.text(container.footnote_label);
    });
    field(supplemental ? supplemental->html.get() : nullptr, [&](auto const& html) {
        hash.text(html.root_tag); hash.text(html.opening_marker); hash.text(html.closing_marker);
        hash.optional(html.text_alignment, [&](auto value) { hash.scalar(value); });
    });
}

inline void append_header(Hasher& hash, BlockNode const& block) {
    append_metadata(hash, block);
    record_render_source_node(block.inline_content().source.size()
        + block.block_source.source().size() + block.atomic_special().raw.size());
    append(hash, block.inline_content());
    hash.text(block.block_source.source());
    hash.text(block.atomic_special().raw);
    for (const auto& indent : block.block_source.tree().generated_indentation) {
        hash.scalar(indent.content_offset); hash.scalar(indent.source_offset); hash.scalar(indent.columns);
    }
}

inline std::uint64_t context_key(BlockNode const& block, std::uint64_t parent,
    bool first, bool last, std::optional<std::size_t> ordinal) {
    Hasher hash;
    hash.scalar(parent);
    hash.scalar(first);
    hash.scalar(last);
    hash.optional(ordinal, [&](auto value) { hash.scalar(value); });
    append_metadata(hash, block);
    if (!is_fragment_flow_container(block.kind)) {
        // Atomic layout units such as tables retain their complete structure.
        // Source content is invalidated separately by changed_owners.
        auto children = [&](auto& self, BlockNode const& node) -> void {
            hash.scalar(node.children.size());
            for (auto const& child : node.children) {
                append_metadata(hash, child);
                self(self, child);
            }
        };
        children(children, block);
    }
    return hash.value;
}

inline void append(Hasher& hash, BlockNode const& block) {
    append_header(hash, block);
    hash.scalar(block.children.size());
    for (auto const& child : block.children) append(hash, child);
}

inline std::uint64_t source_key(BlockNode const& block, std::uint64_t document_dependency_key) {
    record_render_source_key_derivation();
    Hasher hash;
    hash.scalar(document_dependency_key);
    append(hash, block);
    return hash.value;
}

inline std::uint64_t fragment_key(
    BlockNode const& block, RenderFragment const& fragment,
    std::uint64_t document_dependency_key) {
    if (fragment.first && fragment.last) return source_key(block, document_dependency_key);
    record_render_source_key_derivation();
    Hasher hash;
    hash.scalar(document_dependency_key);
    hash.scalar(fragment.id);
    hash.scalar(fragment.first);
    hash.scalar(fragment.last);
    visit_render_fragment(block, fragment_range(fragment), [&](auto const& node, auto range) {
        append_header(hash, node);
        // Child ordinals affect ordered marker text. Unordered suffixes keep
        // their visual identity when an earlier sibling is inserted/removed.
        if ((node.kind == BlockKind::List || node.kind == BlockKind::TaskList)
            && node.list_special().ordered) {
            hash.scalar(range.begin());
            hash.scalar(range.end(node.children.size()));
        }
        hash.scalar(range.starts_node());
        hash.scalar(range.ends_node(node));
    });
    return hash.value;
}

} // namespace render_key_detail

inline std::size_t block_local_length(const BlockNode& block) {
    if (const auto* document = editable_inline_document(block)) return document->source.size();
    switch (block.kind) {
        case BlockKind::CodeBlock:
        case BlockKind::MathBlock:
            return block.block_source.source().size();
        case BlockKind::Frontmatter:
        case BlockKind::UnsupportedMarkup:
        case BlockKind::LinkDefinition:
            return utf8_to_cps(block.atomic_special().raw).size();
        case BlockKind::ImageBlock:
        case BlockKind::Toc:
        case BlockKind::ThematicBreak:
        case BlockKind::Extension:
            return 1;
        default:
            return 0;
    }
}

inline RenderBlock render_block_base(BlockKind k, NodeId id, std::size_t length, BlockStyle bs) {
    RenderBlock b; b.kind = [&](BlockKind)->RenderBlockKind {
        switch (k) {
            case BlockKind::Paragraph: case BlockKind::Heading: case BlockKind::CalloutTitle:
            case BlockKind::List: case BlockKind::TaskList:
                return RenderBlockKind::Text;
            case BlockKind::BlockQuote: return RenderBlockKind::Quote;
            case BlockKind::CodeBlock: return RenderBlockKind::Code;
            case BlockKind::MathBlock: return RenderBlockKind::Math;
            case BlockKind::Table:     return RenderBlockKind::Table;
            case BlockKind::ImageBlock:return RenderBlockKind::Image;
            case BlockKind::Toc:       return RenderBlockKind::Toc;
            case BlockKind::Callout:   return RenderBlockKind::Callout;
            case BlockKind::FootnoteDefinition: return RenderBlockKind::Footnote;
            case BlockKind::Frontmatter:return RenderBlockKind::Frontmatter;
            case BlockKind::ThematicBreak: return RenderBlockKind::ThematicBreak;
            case BlockKind::LinkDefinition: return RenderBlockKind::Blank;
            case BlockKind::UnsupportedMarkup: return RenderBlockKind::Unsupported;
            case BlockKind::HtmlContainer: return RenderBlockKind::Text;
            case BlockKind::Extension: return RenderBlockKind::Extension;
        }
        return RenderBlockKind::Text;
    }(k);
    b.id = id; b.source_span = {id, {0, length}}; b.block_style = bs;
    b.content_span = b.source_span;
    return b;
}

// Build a generated marker anchored at a boundary in its owner's source.
inline void push_marker(
    std::vector<InlineRenderItem>& out,
    NodeId owner,
    std::size_t& cursor,
    std::u32string text,
    MarkerRole role,
    TextAffinity boundary_affinity,
    std::u32string display_text = {}) {
    InlineRenderItem m; m.kind = InlineRenderItem::Kind::Marker;
    m.source_span = {owner, {cursor, cursor}};
    m.text = std::move(text);
    m.ensure_special().display_text = std::move(display_text);
    m.ensure_special().marker_role = role;
    m.ensure_special().generated_boundary_affinity = boundary_affinity;
    m.ensure_special().marker_style = MarkerStyle{true, {}};
    m.ensure_special().visibility = MarkerVisibility::Always;
    out.push_back(std::move(m));
}

struct Builder {
    explicit Builder(ThemeProfile const& profile) : theme(profile) {}

    ThemeProfile const& theme;
    std::unordered_map<std::string, std::size_t> footnote_ordinals;

    std::u32string footnote_display_label(std::string_view label) const {
        auto found = footnote_ordinals.find(std::string(label));
        return found == footnote_ordinals.end()
            ? utf8_to_cps(label)
            : utf8_to_cps(std::to_string(found->second));
    }

    static bool owns_text_position(const BlockNode& block) {
        if (editable_inline_document(block)) return true;
        switch (block.kind) {
            case BlockKind::CodeBlock:
            case BlockKind::MathBlock:
            case BlockKind::ImageBlock:
            case BlockKind::Toc:
            case BlockKind::Frontmatter:
            case BlockKind::ThematicBreak:
            case BlockKind::LinkDefinition:
            case BlockKind::UnsupportedMarkup:
            case BlockKind::Extension:
                return true;
            default:
                return false;
        }
    }

    std::optional<NodeId> first_editable_owner(const BlockNode& block) const {
        if (owns_text_position(block)) return block.id;
        for (auto const& child : block.children) {
            if (auto owner = first_editable_owner(child)) return owner;
        }
        return std::nullopt;
    }

    std::optional<TextPosition> last_editable_position(const BlockNode& block) const {
        for (auto child = block.children.rbegin(); child != block.children.rend(); ++child) {
            if (auto position = last_editable_position(*child)) return position;
        }
        if (owns_text_position(block)) {
            return TextPosition{block.id, block_local_length(block), TextAffinity::Upstream};
        }
        return std::nullopt;
    }

    void append_block_break(std::vector<InlineRenderItem>& out, NodeId owner, std::size_t source_offset = 0) {
        std::size_t cursor = source_offset;
        push_marker(out, owner, cursor, U"\n", MarkerRole::Structural, TextAffinity::Upstream);
    }
    void append_generated_indent(std::vector<InlineRenderItem>& out, NodeId owner, std::size_t source_offset, std::size_t columns) {
        if (columns == 0) return;
        InlineRenderItem marker;
        marker.kind = InlineRenderItem::Kind::Marker;
        marker.source_span = {owner, {source_offset, source_offset}};
        marker.ensure_special().display_text = std::u32string(columns, U' ');
        marker.ensure_special().marker_role = MarkerRole::Structural;
        marker.ensure_special().generated_boundary_affinity = TextAffinity::Downstream;
        marker.ensure_special().visibility = MarkerVisibility::Always;
        out.push_back(std::move(marker));
    }

    void append_flow_code_contents(
        std::vector<InlineRenderItem>& out,
        const BlockNode& block,
        std::size_t indent_columns,
        std::size_t emitted_columns) {
        constexpr std::size_t content_padding_columns = 2;
        const auto editable = block_source_editable_range(block.block_source);
        // Reserve one generated header row for the structural code-frame
        // language control. This is presentation-only and therefore has an
        // empty source span; copy/save remain exactly source-backed.
        append_generated_indent(
            out,
            block.id,
            editable.start,
            indent_columns > emitted_columns ? indent_columns - emitted_columns : 0);
        InlineRenderItem header;
        header.kind = InlineRenderItem::Kind::Marker;
        header.source_span = {block.id, {editable.start, editable.start}};
        header.ensure_special().display_text = U"\u200B\n";
        header.ensure_special().marker_role = MarkerRole::Structural;
        header.ensure_special().generated_boundary_affinity = TextAffinity::Downstream;
        header.ensure_special().visibility = MarkerVisibility::Always;
        out.push_back(std::move(header));
        emitted_columns = 0;
        auto append_line_indent = [&](std::size_t source_offset, bool first_line) {
            auto desired = indent_columns + content_padding_columns;
            auto existing = first_line ? emitted_columns : std::size_t{0};
            append_generated_indent(out, block.id, source_offset, desired > existing ? desired - existing : 0);
        };
        const auto code = block_source_content(block.block_source);
        const auto lines = code_presentation_lines(code);
        bool first_line = true;
        for (std::size_t index = 0; index < lines.size(); ++index) {
            const auto& line = lines[index];
            const auto display_range = index + 1 == lines.size()
                ? line.content_range
                : line.range_with_ending;
            const auto source_start = block_source_offset_for_content(
                block.block_source,
                display_range.start);
            for (const auto& generated : block.block_source.tree().generated_indentation) {
                if (generated.content_offset == display_range.start) {
                    append_generated_indent(
                        out, block.id, generated.source_offset, generated.columns);
                }
            }
            append_line_indent(source_start, first_line);
            const auto source_end = block_source_offset_for_content(
                block.block_source,
                display_range.end);
            auto item = InlineRenderItem::plain_text(
                code.substr(
                    display_range.start,
                    display_range.length()),
                {block.id, {source_start, source_end}});
            replace_inline_display_nuls(item.text);
            item.style.code = CodeStyle::Block;
            out.push_back(std::move(item));
            first_line = false;
        }
    }

    struct FlowContext {
        std::size_t indent_columns = 0;
    };

    std::size_t list_marker_columns(const BlockNode& list, std::size_t index) const {
        if (index < list.children.size()
            && list.children[index].kind == BlockKind::TaskListItem) {
            return list.list_special().ordered
                ? std::to_string(list.list_special().start + index).size() + 6
                : 6;
        }
        if (list.list_special().ordered) return std::to_string(list.list_special().start + index).size() + 2;
        return 2;
    }

    NodeId list_item_marker_owner(const BlockNode& item) const {
        return first_editable_owner(item).value_or(item.id);
    }

    BlockStyle flow_container_style(BlockKind kind, std::string_view callout_kind = {}) const {
        switch (kind) {
            case BlockKind::BlockQuote: return BlockStyle::blockquote(theme.layout);
            case BlockKind::Callout: return BlockStyle::callout(callout_kind, theme.layout);
            case BlockKind::FootnoteDefinition: return BlockStyle::footnote(theme.layout);
            case BlockKind::List:
            case BlockKind::TaskList: return BlockStyle::list(theme.layout);
            default: return BlockStyle::paragraph(theme.layout);
        }
    }

    RenderBlock make_flow_container(
        const BlockNode& block,
        FlowContext context,
        std::size_t parent_indent_columns,
        RenderFragmentRange range = {}) {
        auto rendered = render_block_base(
            block.kind,
            block.id,
            block_local_length(block),
            flow_container_style(block.kind, block.container_special().callout_kind));
        rendered.block_style.text_alignment = block.html_special().text_alignment;
        rendered.flow_local_indent_columns = context.indent_columns -
            (std::min)(context.indent_columns, parent_indent_columns);
        auto const* anchor = &block;
        for (auto index : range.first) anchor = &anchor->children[index];
        rendered.flow_anchor_owner_id = first_editable_owner(*anchor).value_or(anchor->id);
        rendered.flow_continues_before = !range.starts_node();
        rendered.flow_continues_after = !range.ends_node(block);
        if (block.kind == BlockKind::Callout) {
            rendered.ensure_special().callout_kind = block.container_special().callout_kind;
        } else if (block.kind == BlockKind::FootnoteDefinition) {
            rendered.ensure_special().footnote_label = block.container_special().footnote_label;
        }
        return rendered;
    }

    void append_missing_indent(
        std::vector<InlineRenderItem>& out,
        NodeId owner,
        std::size_t source_offset,
        std::size_t desired_columns,
        std::size_t emitted_columns) {
        append_generated_indent(
            out,
            owner,
            source_offset,
            desired_columns > emitted_columns ? desired_columns - emitted_columns : 0);
    }

    void append_flow_spacing(
        std::vector<InlineRenderItem>& out, NodeId owner,
        std::size_t offset, float height, bool before) {
        if (height <= 0.0f) return;
        InlineRenderItem space;
        space.kind = InlineRenderItem::Kind::Marker;
        space.source_span = {owner, {offset, offset}};
        auto& payload = space.ensure_special();
        payload.marker_role = MarkerRole::BlockSpacing;
        payload.visibility = MarkerVisibility::Always;
        payload.generated_boundary_affinity = before
            ? TextAffinity::Downstream : TextAffinity::Upstream;
        payload.display_text = before ? U"\uFFFC\n" : U"\n\uFFFC";
        payload.block_spacing_height = height;
        out.push_back(std::move(space));
    }

    RenderBlock append_flow_block(
        std::vector<InlineRenderItem>& out,
        const BlockNode& block,
        FlowContext context,
        std::size_t emitted_columns = 0,
        std::size_t parent_indent_columns = 0,
        RenderFragmentRange range = {}) {
        auto const* anchor = &block;
        for (auto index : range.first) anchor = &anchor->children[index];
        auto owner = first_editable_owner(*anchor).value_or(anchor->id);
        switch (block.kind) {
            case BlockKind::Document:
            case BlockKind::HtmlContainer:
            case BlockKind::ListItem:
            case BlockKind::TaskListItem: {
                auto rendered = make_flow_container(block, context, parent_indent_columns, range);
                for (std::size_t index = range.begin(); index < range.end(block.children.size()); ++index) {
                    if (index > range.begin()) {
                        auto previous = last_editable_position(block.children[index - 1])
                            .value_or(TextPosition{owner, 0, TextAffinity::Upstream});
                        append_block_break(out, previous.container_id, previous.source_offset);
                    }
                    rendered.child_blocks.push_back(append_flow_block(
                        out,
                        block.children[index],
                        FlowContext{context.indent_columns},
                        index == range.begin() ? emitted_columns : 0,
                        context.indent_columns, range.child(index)));
                }
                return rendered;
            }
            case BlockKind::List:
            case BlockKind::TaskList: {
                auto rendered = make_flow_container(block, context, parent_indent_columns, range);
                for (std::size_t index = range.begin(); index < range.end(block.children.size()); ++index) {
                    auto const& item = block.children[index];
                    const bool task_item = item.kind == BlockKind::TaskListItem;
                    auto item_range = range.child(index);
                    auto const* item_anchor = &item;
                    for (auto child : item_range.first) item_anchor = &item_anchor->children[child];
                    auto item_owner = first_editable_owner(*item_anchor).value_or(item_anchor->id);
                    if (index > range.begin()) {
                        auto previous = last_editable_position(block.children[index - 1])
                            .value_or(TextPosition{item_owner, 0, TextAffinity::Upstream});
                        append_block_break(out, previous.container_id, previous.source_offset);
                    }
                    auto marker = item.item_special().marker;
                    if (marker.empty()) {
                        if (task_item) {
                            marker = item.item_special().checked ? U"- [x] " : U"- [ ] ";
                        }
                        else if (block.list_special().ordered) marker = utf8_to_cps(std::to_string(block.list_special().start + index)) + std::u32string(1, block.list_special().delimiter) + U" ";
                        else marker = U"- ";
                    }
                    auto marker_columns = list_marker_columns(block, index);
                    auto missing_indent = context.indent_columns > (index == range.begin() ? emitted_columns : 0)
                        ? context.indent_columns - (index == range.begin() ? emitted_columns : 0)
                        : 0;
                    auto display_marker = task_item
                        ? marker
                        : block.list_special().ordered
                            ? utf8_to_cps(std::to_string(block.list_special().start + index)) + std::u32string(1, block.list_special().delimiter) + U" "
                            : U"\u2022 ";
                    if (item_range.starts_node()) {
                        std::size_t cursor = 0;
                        push_marker(
                            out,
                            item_owner,
                            cursor,
                            marker,
                            task_item ? MarkerRole::TaskCheckbox : block.list_special().ordered ? MarkerRole::ListNumber : MarkerRole::ListBullet,
                            TextAffinity::Downstream,
                            std::u32string(missing_indent, U' ') + display_marker);
                        if (task_item) out.back().ensure_special().task_checked = item.item_special().checked;
                    }

                    FlowContext item_context{context.indent_columns + marker_columns};
                    auto item_rendered = make_flow_container(item, item_context, context.indent_columns, item_range);
                    for (std::size_t child_index = item_range.begin(); child_index < item_range.end(item.children.size()); ++child_index) {
                        if (child_index > item_range.begin()) {
                            auto previous = last_editable_position(item.children[child_index - 1])
                                .value_or(TextPosition{item_owner, 0, TextAffinity::Upstream});
                            append_block_break(out, previous.container_id, previous.source_offset);
                        }
                        item_rendered.child_blocks.push_back(append_flow_block(
                            out,
                            item.children[child_index],
                            item_context,
                            child_index == item_range.begin() && item_range.starts_node()
                                ? context.indent_columns + marker_columns : 0,
                            item_context.indent_columns, item_range.child(child_index)));
                    }
                    rendered.child_blocks.push_back(std::move(item_rendered));
                }
                return rendered;
            }
            case BlockKind::BlockQuote:
            case BlockKind::Callout:
            case BlockKind::FootnoteDefinition: {
                auto rendered = make_flow_container(block, context, parent_indent_columns, range);
                std::u32string footnote_marker;
                std::u32string callout_label;
                if (block.kind == BlockKind::FootnoteDefinition && !block.container_special().footnote_label.empty()) {
                    footnote_marker = footnote_display_label(block.container_special().footnote_label) + U". ";
                }
                if (block.kind == BlockKind::Callout) {
                    callout_label = callout_display_label(block.container_special().callout_kind);
                }
                auto const content_indent = context.indent_columns
                    + (footnote_marker.empty() ? 2u : footnote_marker.size());
                if (!footnote_marker.empty() && range.starts_node()) {
                    append_missing_indent(out, owner, 0, context.indent_columns, emitted_columns);
                    InlineRenderItem label;
                    label.kind = InlineRenderItem::Kind::Marker;
                    label.source_span = {owner, {0, 0}};
                    label.ensure_special().display_text = footnote_marker;
                    label.ensure_special().ensure_semantic().footnote_label = block.container_special().footnote_label;
                    label.ensure_special().marker_role = MarkerRole::FootnoteLabel;
                    label.ensure_special().generated_boundary_affinity = TextAffinity::Downstream;
                    label.ensure_special().visibility = MarkerVisibility::Always;
                    out.push_back(std::move(label));
                }
                if (!callout_label.empty() && range.starts_node()) {
                    append_missing_indent(out, owner, 0, content_indent, emitted_columns);
                    InlineRenderItem label;
                    label.kind = InlineRenderItem::Kind::Marker;
                    label.source_span = {owner, {0, 0}};
                    label.ensure_special().display_text = callout_label;
                    label.style.bold = true;
                    label.ensure_special().marker_role = MarkerRole::Structural;
                    label.ensure_special().generated_boundary_affinity = TextAffinity::Downstream;
                    label.ensure_special().visibility = MarkerVisibility::Always;
                    out.push_back(std::move(label));
                    if (!block.children.empty()) {
                        append_block_break(out, owner, 0);
                    }
                }
                for (std::size_t index = range.begin(); index < range.end(block.children.size()); ++index) {
                    if (index > range.begin()) {
                        auto previous = last_editable_position(block.children[index - 1])
                            .value_or(TextPosition{owner, 0, TextAffinity::Upstream});
                        append_block_break(out, previous.container_id, previous.source_offset);
                    }
                    rendered.child_blocks.push_back(append_flow_block(
                        out,
                        block.children[index],
                        FlowContext{content_indent},
                        index == range.begin() && range.starts_node() && callout_label.empty()
                            ? !footnote_marker.empty()
                                ? context.indent_columns + footnote_marker.size()
                                : emitted_columns
                            : 0,
                        context.indent_columns, range.child(index)));
                }
                return rendered;
            }
            default:
                break;
        }

        auto rendered = build_block(block);
        const auto spaced = block.kind == BlockKind::CodeBlock
            || block.kind == BlockKind::MathBlock;
        if (spaced && rendered.block_style.padding_top > 0.0f) {
            if (emitted_columns > 0) append_block_break(out, owner, 0);
            append_flow_spacing(out, owner, block_source_editable_range(block.block_source).start,
                rendered.block_style.padding_top, true);
            emitted_columns = 0;
        }
        switch (block.kind) {
            case BlockKind::Paragraph:
            case BlockKind::Heading:
            case BlockKind::CalloutTitle:
            case BlockKind::TableCell:
                append_missing_indent(out, owner, 0, context.indent_columns, emitted_columns);
                out.insert(out.end(), rendered.inline_items.begin(), rendered.inline_items.end());
                break;
            case BlockKind::CodeBlock:
                append_flow_code_contents(out, block, context.indent_columns, emitted_columns);
                break;
            case BlockKind::MathBlock: {
                append_missing_indent(out, owner, 0, context.indent_columns, emitted_columns);
                const auto math = block_source_content(block.block_source);
                InlineRenderItem item;
                item.kind = InlineRenderItem::Kind::Math;
                item.id = block.id;
                item.source_span = {block.id, {0, block.block_source.source().size()}};
                item.ensure_special().ensure_semantic().block_source = rendered.payload;
                item.ensure_special().ensure_semantic().continuation_indent_columns = context.indent_columns;
                item.text = math;
                item.ensure_special().display = MathDisplayMode::Block;
                item.ensure_special().math_delim = block.atomic_special().math_delim;
                out.push_back(std::move(item));
                break;
            }
            case BlockKind::ImageBlock: {
                append_missing_indent(out, owner, 0, context.indent_columns, emitted_columns);
                InlineRenderItem item;
                item.kind = InlineRenderItem::Kind::Image;
                item.id = block.id;
                item.source_span = {block.id, {0, block_local_length(block)}};
                auto& semantic = item.ensure_special().ensure_semantic();
                semantic.src = block.image_special().src;
                semantic.alt = block.image_special().image_alt;
                semantic.title = block.image_special().image_title;
                semantic.image_width = block.image_special().image_width;
                semantic.image_height = block.image_special().image_height;
                semantic.block_image = true;
                out.push_back(std::move(item));
                break;
            }
            case BlockKind::Table: {
                append_missing_indent(out, owner, 0, context.indent_columns, emitted_columns);
                bool first_cell = true;
                for (const auto& row : block.children) {
                    for (const auto& cell : row.children) {
                        if (!first_cell) append_block_break(out, cell.id, 0);
                        auto items = build_inline_document(cell.inline_content(), cell.id, InlineStyle::plain());
                        out.insert(out.end(), items.begin(), items.end());
                        first_cell = false;
                    }
                }
                break;
            }
            case BlockKind::Frontmatter:
            case BlockKind::LinkDefinition:
            case BlockKind::UnsupportedMarkup:
            case BlockKind::Extension: {
                append_missing_indent(out, owner, 0, context.indent_columns, emitted_columns);
                auto raw = utf8_to_cps(block.atomic_special().raw);
                out.push_back(InlineRenderItem::plain_text(raw, {block.id, {0, raw.size()}}));
                break;
            }
            case BlockKind::ThematicBreak:
                append_missing_indent(out, owner, 0, context.indent_columns, emitted_columns);
                out.push_back(InlineRenderItem::plain_text(U"\u2014", {block.id, {0, block_local_length(block)}}));
                break;
            case BlockKind::Toc:
                append_missing_indent(out, owner, 0, context.indent_columns, emitted_columns);
                out.push_back(InlineRenderItem::plain_text(U"contents", {block.id, {0, block_local_length(block)}}));
                break;
            default:
                break;
        }
        if (spaced) {
            append_flow_spacing(out, owner, block_source_editable_range(block.block_source).end,
                rendered.block_style.padding_bottom, false);
        }
        rendered.flow_local_indent_columns = context.indent_columns -
            (std::min)(context.indent_columns, parent_indent_columns);
        rendered.flow_anchor_owner_id = owner;
        return rendered;
    }
    std::vector<InlineRenderItem> build_inline_document(
        const InlineDocument& document,
        NodeId owner_id,
        const InlineStyle& base,
        bool preserve_soft_breaks = false,
        InlineBlockWhitespacePolicy whitespace_policy = InlineBlockWhitespacePolicy::None) {
        std::vector<InlineRenderItem> output;
        const auto semantic_whitespace = inline_block_semantic_whitespace_ranges(document, whitespace_policy);
        auto source_span = [&](SourceRange range) {
            return TextSpan{owner_id, range};
        };
        auto delimiter_icon = [&](InlineCstNode const& node, SourceRange range) {
            using M = SyntaxMarker;
            using K = InlineCstKind;
            if (node.status != ParseStatus::Complete || !node.delimiter_ranges().closing || range.empty()) return M::None;
            switch (node.kind) {
                case K::Emphasis: return document.source[range.start] == U'*' ? M::Star : M::Underscore;
                case K::Strong: return document.source[range.start] == U'*' ? M::DoubleStar : M::DoubleUnderscore;
                case K::Strikethrough: return range.length() == 1 ? M::Tilde : M::DoubleTilde;
                case K::Highlight: return M::DoubleEquals;
                case K::CodeSpan:
                    switch (range.length()) {
                        case 1: return M::Backtick;
                        case 2: return M::DoubleBacktick;
                        case 3: return M::TripleBacktick;
                        default: return M::None;
                    }
                case K::InlineMath:
                    if (node.semantics().math_delim == MathDelimiter::InlineDollar) return M::Dollar;
                    return range == node.delimiter_ranges().opening ? M::MathOpen : M::MathClose;
                default: return M::None;
            }
        };
        auto append_marker = [&](std::vector<InlineRenderItem>& target, const InlineCstNode& owner, SourceRange range) {
            if (range.empty()) return;
            InlineRenderItem marker;
            marker.kind = InlineRenderItem::Kind::Marker;
            marker.id = owner.id;
            marker.style.heading_level = base.heading_level;
            marker.ensure_special().marker_owner = owner.id;
            marker.ensure_special().marker_construct = owner.range;
            marker.ensure_special().syntax_marker = delimiter_icon(owner, range);
            marker.source_span = source_span(range);
            marker.text = inline_source_slice(document, range);
            marker.ensure_special().marker_style = MarkerStyle{true, {}};
            // Reveal exact source delimiters when editing their CST construct.
            // Incomplete syntax remains visible so it can be finished normally.
            marker.ensure_special().visibility = owner.status == ParseStatus::Complete
                ? MarkerVisibility::WhenCaretInsideNode : MarkerVisibility::Always;
            target.push_back(std::move(marker));
        };
        std::function<void(const InlineCstNodes&, const InlineStyle&, std::vector<InlineRenderItem>&)> append_nodes;
        append_nodes = [&](const InlineCstNodes& nodes, const InlineStyle& style, std::vector<InlineRenderItem>& target) {
            for (const auto& node : nodes) {
                using K = InlineCstKind;
                const auto& delim = node.delimiter_ranges();
                const auto& semantic = node.semantics();
                auto append_text = [&](std::u32string text, SourceRange range, InlineStyle text_style = InlineStyle::plain()) {
                    InlineRenderItem item = InlineRenderItem::plain_text(std::move(text), source_span(range));
                    item.id = node.id;
                    if (text_style.link && !item.text.empty()
                        && std::ranges::all_of(item.text, [](char32_t ch) {
                            return ch == U' ' || ch == U'\t' || ch == U'\n' || ch == U'\r';
                        })) {
                        text_style.link = false;
                    }
                    item.style = text_style;
                    target.push_back(std::move(item));
                };
                auto append_raw_text_leaf = [&] {
                    if (node.kind != K::Text && node.kind != K::Whitespace) {
                        append_text(inline_source_slice(document, node.range), node.range, style);
                        return;
                    }
                    const auto found = std::find_if(semantic_whitespace.begin(), semantic_whitespace.end(), [&](const auto& range) {
                        return range.start >= node.range.start && range.end <= node.range.end;
                    });
                    if (found == semantic_whitespace.end()) {
                        append_text(inline_source_slice(document, node.range), node.range, style);
                        return;
                    }
                    if (node.range.start < found->start) {
                        const SourceRange prefix{node.range.start, found->start};
                        append_text(inline_source_slice(document, prefix), prefix, style);
                    }
                    InlineRenderItem marker;
                    marker.kind = InlineRenderItem::Kind::Marker;
                    marker.id = node.id;
                    marker.source_span = source_span(*found);
                    marker.text = inline_source_slice(document, *found);
                    marker.style = style;
                    auto& marker_payload = marker.ensure_special();
                    marker_payload.marker_owner = node.id;
                    marker_payload.marker_role = MarkerRole::Syntax;
                    marker_payload.marker_style = MarkerStyle{true, {}};
                    marker_payload.visibility = MarkerVisibility::HiddenButEditable;
                    target.push_back(std::move(marker));
                    if (found->end < node.range.end) {
                        const SourceRange suffix{found->end, node.range.end};
                        append_text(inline_source_slice(document, suffix), suffix, style);
                    }
                };
                auto append_code_span_text = [&](const InlineCstNode& code_node, const InlineStyle& text_style) {
                    struct ProjectedCharacter {
                        char32_t value;
                        SourceRange source_range;
                        bool hidden = false;
                    };
                    const auto content = code_node.delimiter_ranges().content;
                    const auto raw = inline_source_slice(document, content);
                    const auto normalized = normalize_inline_code_span(raw, document.syntax_mode);
                    std::vector<ProjectedCharacter> projected;
                    projected.reserve(raw.size());
                    for (std::size_t offset = 0; offset < raw.size(); ++offset) {
                        const auto source_start = content.start + offset;
                        const auto value = raw[offset];
                        if (document.syntax_mode == InlineSyntaxMode::TableCell
                            && value == U'\\' && offset + 1 < raw.size() && raw[offset + 1] == U'|') {
                            projected.push_back({U'\\', {source_start, source_start + 1}, true});
                            projected.push_back({U'|', {source_start + 1, source_start + 2}});
                            ++offset;
                            continue;
                        }
                        if (value == U'\r') {
                            auto source_end = source_start + 1;
                            if (offset + 1 < raw.size() && raw[offset + 1] == U'\n') {
                                ++offset;
                                ++source_end;
                            }
                            projected.push_back({U' ', {source_start, source_end}});
                        } else if (value == U'\n') {
                            projected.push_back({U' ', {source_start, source_start + 1}});
                        } else {
                            projected.push_back({value, {source_start, source_start + 1}});
                        }
                    }

                    auto content_start = std::size_t{0};
                    auto content_end = projected.size();
                    const auto trim_outer_spaces = projected.size() >= 2
                        && projected.front().value == U' '
                        && projected.back().value == U' '
                        && std::ranges::any_of(projected, [](const auto& character) {
                            return character.value != U' ';
                        });
                    if (trim_outer_spaces) {
                        append_marker(target, code_node, projected.front().source_range);
                        ++content_start;
                        --content_end;
                    }

                    auto cursor = content_start;
                    auto display_cursor = std::size_t{0};
                    while (cursor < content_end) {
                        if (projected[cursor].hidden) {
                            InlineRenderItem marker;
                            marker.kind = InlineRenderItem::Kind::Marker;
                            marker.id = code_node.id;
                            marker.source_span = source_span(projected[cursor].source_range);
                            marker.text = inline_source_slice(document, projected[cursor].source_range);
                            auto& marker_payload = marker.ensure_special();
                            marker_payload.marker_owner = code_node.id;
                            marker_payload.marker_role = MarkerRole::Syntax;
                            marker_payload.marker_style = MarkerStyle{true, {}};
                            marker_payload.visibility = MarkerVisibility::HiddenButEditable;
                            target.push_back(std::move(marker));
                            ++cursor;
                            continue;
                        }
                        if (projected[cursor].source_range.length() != 1) {
                            append_text(
                                normalized.substr(display_cursor, 1),
                                projected[cursor].source_range,
                                text_style);
                            ++cursor;
                            ++display_cursor;
                            continue;
                        }
                        auto run_end = cursor + 1;
                        while (run_end < content_end
                            && !projected[run_end].hidden
                            && projected[run_end].source_range.length() == 1
                            && projected[run_end - 1].source_range.end
                                == projected[run_end].source_range.start) {
                            ++run_end;
                        }
                        auto text = normalized.substr(display_cursor, run_end - cursor);
                        const auto displayed_count = run_end - cursor;
                        append_text(
                            std::move(text),
                            {projected[cursor].source_range.start, projected[run_end - 1].source_range.end},
                            text_style);
                        cursor = run_end;
                        display_cursor += displayed_count;
                    }

                    if (trim_outer_spaces) {
                        append_marker(target, code_node, projected.back().source_range);
                    }
                };
                switch (node.kind) {
                    case K::Strong: {
                        auto child_style = style; child_style.bold = true;
                        append_marker(target, node, delim.opening);
                        append_nodes(node.children, child_style, target);
                        if (delim.closing) append_marker(target, node, *delim.closing);
                        break;
                    }
                    case K::Emphasis: {
                        auto child_style = style; child_style.italic = true;
                        append_marker(target, node, delim.opening);
                        append_nodes(node.children, child_style, target);
                        if (delim.closing) append_marker(target, node, *delim.closing);
                        break;
                    }
                    case K::Strikethrough: {
                        auto child_style = style; child_style.strikethrough = true;
                        append_marker(target, node, delim.opening);
                        append_nodes(node.children, child_style, target);
                        if (delim.closing) append_marker(target, node, *delim.closing);
                        break;
                    }
                    case K::Highlight: {
                        auto child_style = style;
                        auto presentation = std::make_shared<InlinePresentationStyle>(
                            style.presentation ? *style.presentation : InlinePresentationStyle{});
                        presentation->highlight = true;
                        child_style.presentation = std::move(presentation);
                        append_marker(target, node, delim.opening);
                        append_nodes(node.children, child_style, target);
                        if (delim.closing) append_marker(target, node, *delim.closing);
                        break;
                    }
                    case K::CodeSpan: {
                        auto child_style = style; child_style.code = CodeStyle::Inline;
                        child_style.code_span_id = node.id;
                        append_marker(target, node, delim.opening);
                        append_code_span_text(node, child_style);
                        if (delim.closing) append_marker(target, node, *delim.closing);
                        break;
                    }
                    case K::InlineMath: {
                        InlineRenderItem item;
                        item.kind = InlineRenderItem::Kind::Math;
                        item.id = node.id;
                        item.source_span = source_span(node.range);
                        item.text = inline_source_slice(document, delim.content);
                        item.ensure_special().display = MathDisplayMode::Inline;
                        item.ensure_special().math_delim = semantic.math_delim;
                        item.ensure_special().ensure_semantic().content_range = delim.content;
                        item.ensure_special().ensure_semantic().opening_marker = delimiter_icon(node, delim.opening);
                        if (delim.closing) item.ensure_special().ensure_semantic().closing_marker = delimiter_icon(node, *delim.closing);
                        item.style = style;
                        item.style.bold = false;
                        item.style.italic = false;
                        item.style.code = CodeStyle::None;
                        item.style.code_span_id = {};
                        item.style.link = false;
                        target.push_back(std::move(item));
                        break;
                    }
                    case K::Link:
                    case K::Autolink: {
                        InlineRenderItem item;
                        item.kind = InlineRenderItem::Kind::Link;
                        item.id = node.id;
                        item.source_span = source_span(node.range);
                        item.style = style;
                        auto& special = item.ensure_special().ensure_semantic();
                        special.href = semantic.href;
                        special.content_range = delim.content;
                        special.title = semantic.title;
                        auto child_style = style; child_style.link = true;
                        if (node.kind == K::Link) {
                            append_marker(special.children, node, delim.opening);
                            append_nodes(node.children, child_style, special.children);
                            if (delim.closing) append_marker(special.children, node, *delim.closing);
                        } else {
                            append_marker(special.children, node, delim.opening);
                            InlineRenderItem text_item = InlineRenderItem::plain_text(
                                inline_source_slice(document, delim.content), source_span(delim.content));
                            text_item.style = child_style;
                            special.children.push_back(std::move(text_item));
                            if (delim.closing) append_marker(special.children, node, *delim.closing);
                        }
                        target.push_back(std::move(item));
                        break;
                    }
                    case K::Image: {
                        InlineRenderItem item;
                        item.kind = InlineRenderItem::Kind::Image;
                        item.id = node.id;
                        item.source_span = source_span(node.range);
                        item.style = style;
                        item.style.link = false;
                        auto& special = item.ensure_special().ensure_semantic();
                        special.src = semantic.href;
                        special.alt = semantic.alt;
                        special.content_range = delim.content;
                        special.title = semantic.title;
                        special.image_width = semantic.image_width;
                        special.image_height = semantic.image_height;
                        target.push_back(std::move(item));
                        break;
                    }
                    case K::HtmlElement: {
                        if (semantic.html_tag == "img") {
                            InlineRenderItem item;
                            item.kind = InlineRenderItem::Kind::Image;
                            item.id = node.id;
                            item.source_span = source_span(node.range);
                            item.style = style;
                            item.style.link = false;
                            auto& special = item.ensure_special().ensure_semantic();
                            special.src = semantic.href;
                            special.alt = semantic.alt;
                            special.title = semantic.title;
                            special.image_width = semantic.image_width;
                            special.image_height = semantic.image_height;
                            target.push_back(std::move(item));
                            break;
                        }
                        if (semantic.html_tag == "br") {
                            append_text(U"\n", node.range, style);
                            break;
                        }
                        if (semantic.html_tag == "a") {
                            InlineRenderItem item;
                            item.kind = InlineRenderItem::Kind::Link;
                            item.id = node.id;
                            item.source_span = source_span(node.range);
                            auto& special = item.ensure_special().ensure_semantic();
                            special.href = semantic.href;
                            special.title = semantic.title;
                            auto child_style = style;
                            child_style.link = true;
                            append_marker(special.children, node, delim.opening);
                            append_nodes(node.children, child_style, special.children);
                            if (delim.closing) append_marker(special.children, node, *delim.closing);
                            normalize_html_inline_whitespace(
                                special.children,
                                html_inline_media_only(special.children));
                            target.push_back(std::move(item));
                            break;
                        }
                        auto child_style = apply_html_inline_presentation(
                            style,
                            semantic.html_tag,
                            semantic.html_attributes);
                        if (child_style.code == CodeStyle::Inline && style.code != CodeStyle::Inline)
                            child_style.code_span_id = node.id;
                        append_marker(target, node, delim.opening);
                        std::vector<InlineRenderItem> html_children;
                        append_nodes(node.children, child_style, html_children);
                        normalize_html_inline_whitespace(html_children, false);
                        target.insert(
                            target.end(),
                            std::make_move_iterator(html_children.begin()),
                            std::make_move_iterator(html_children.end()));
                        if (delim.closing) append_marker(target, node, *delim.closing);
                        break;
                    }
                    case K::HtmlComment:
                        break;
                    case K::Escape: {
                        const auto raw = inline_source_slice(document, node.range);
                        if (!raw.empty()) {
                            append_marker(target, node, {node.range.start, node.range.start + 1});
                            append_text(raw.substr(1), {node.range.start + 1, node.range.end}, style);
                        }
                        break;
                    }
                    case K::Entity:
                        append_text(decode_inline_entity(inline_source_slice(document, node.range)), node.range, style);
                        break;
                    case K::SoftBreak:
                    case K::HardBreak:
                        if (node.kind == K::SoftBreak) {
                            append_text(preserve_soft_breaks ? U"\n" : U" ", node.range, style);
                            break;
                        }
                        append_text(U"\n", node.range, style);
                        break;
                    case K::FootnoteRef: {
                        InlineRenderItem item;
                        item.kind = InlineRenderItem::Kind::FootnoteReference;
                        item.id = node.id;
                        item.source_span = source_span(node.range);
                        item.style = style;
                        item.text = utf8_to_cps(semantic.label);
                        item.ensure_special().display_text = footnote_display_label(semantic.label);
                        item.ensure_special().ensure_semantic().footnote_label = semantic.label;
                        target.push_back(std::move(item));
                        break;
                    }
                    case K::WikiLink: {
                        auto child_style = style; child_style.link = true;
                        append_text(utf8_to_cps(semantic.alias.value_or(semantic.target)), node.range, child_style);
                        break;
                    }
                    case K::Incomplete:
                        append_marker(target, node, delim.opening);
                        if (!node.children.empty()) append_nodes(node.children, style, target);
                        else if (!delim.content.empty()) append_text(inline_source_slice(document, delim.content), delim.content, style);
                        break;
                    case K::Text:
                    case K::Whitespace:
                        append_raw_text_leaf();
                        break;
                    default:
                        append_text(inline_source_slice(document, node.range), node.range, style);
                        break;
                }
            }
        };
        append_nodes(document.tree.nodes, base, output);
        if (document.syntax_mode == InlineSyntaxMode::HtmlText) {
            normalize_html_inline_whitespace(output, true);
        }
        auto attach_source = [&](auto& self, std::vector<InlineRenderItem>& items) -> void {
            for (auto& item : items) {
                replace_inline_display_nuls(item.text);
                if (item.payload) replace_inline_display_nuls(item.payload->display_text);
                if (item.kind != InlineRenderItem::Kind::Text
                    && item.kind != InlineRenderItem::Kind::Marker) {
                    item.ensure_special().ensure_semantic().source_text =
                        inline_source_slice(document, item.source_span.source_range);
                }
                if (item.payload && item.payload->semantic_payload) {
                    self(self, item.payload->semantic_payload->children);
                }
            }
        };
        attach_source(attach_source, output);
        // Presentation runs need not mirror every CST leaf. Join only linear,
        // contiguous source spans with the same style; collapsed entities,
        // controls and marker ownership retain their individual projections.
        std::vector<InlineRenderItem> runs;
        for (auto& item : output) {
            if (!runs.empty()) {
                auto& previous = runs.back();
                if (previous.kind == InlineRenderItem::Kind::Text && item.kind == previous.kind
                    && !previous.payload && !item.payload && previous.style == item.style
                    && previous.source_syntax == item.source_syntax
                    && previous.source_span.container_id == item.source_span.container_id
                    && previous.source_span.source_range.end == item.source_span.source_range.start
                    && previous.text.size() == previous.source_span.source_range.length()
                    && item.text.size() == item.source_span.source_range.length()) {
                    previous.text.append(item.text);
                    previous.source_span.source_range.end = item.source_span.source_range.end;
                    continue;
                }
            }
            runs.push_back(std::move(item));
        }
        return runs;
    }

    RenderBlock build_block(const BlockNode& b) {
        using BK = BlockKind;
        auto base = [&](BlockStyle style) {
            style.text_alignment = b.html_special().text_alignment;
            return render_block_base(b.kind, b.id, block_local_length(b), std::move(style));
        };
        switch (b.kind) {
            case BK::Paragraph:
            case BK::CalloutTitle:
            case BK::TableCell: {
                auto rb = base(BlockStyle::paragraph(theme.layout));
                if (b.inline_content().source.empty() && b.text_special().opening_marker.empty() && b.text_special().closing_marker.empty()) {
                    rb.kind = RenderBlockKind::Blank;
                    rb.inline_items.push_back(InlineRenderItem::plain_text({}, rb.content_span));
                    return rb;
                }
                InlineStyle s = InlineStyle::plain();
                std::size_t cursor = 0;
                if (!b.text_special().opening_marker.empty()) {
                    cursor = 0;
                    push_marker(
                        rb.inline_items,
                        b.id,
                        cursor,
                        b.text_special().opening_marker,
                        MarkerRole::Syntax,
                        TextAffinity::Downstream);
                    rb.inline_items.back().ensure_special().marker_owner = b.id;
                }
                auto items = build_inline_document(
                    b.inline_content(),
                    b.id,
                    s,
                    false,
                    b.kind == BK::Paragraph
                        ? InlineBlockWhitespacePolicy::Paragraph
                        : InlineBlockWhitespacePolicy::None);
                for (auto& item : items) rb.inline_items.push_back(std::move(item));
                if (!b.text_special().closing_marker.empty()) {
                    cursor = b.inline_content().source.size();
                    push_marker(
                        rb.inline_items,
                        b.id,
                        cursor,
                        b.text_special().closing_marker,
                        MarkerRole::Syntax,
                        TextAffinity::Upstream);
                    rb.inline_items.back().ensure_special().marker_owner = b.id;
                }
                return rb;
            }
            case BK::Heading: {
                auto rb = base(BlockStyle::heading(b.text_special().level, theme.layout));
                InlineStyle s = InlineStyle::plain(); s.heading_level = b.text_special().level;
                auto append_heading_marker = [&](
                    std::u32string const& text,
                    std::size_t offset,
                    TextAffinity boundary_affinity) {
                    if (text.empty()) return;
                    InlineRenderItem marker;
                    marker.kind = InlineRenderItem::Kind::Marker;
                    marker.source_span = {b.id, {offset, offset}};
                    marker.text = text;
                    marker.style = s;
                    marker.ensure_special().marker_role = MarkerRole::Heading;
                    marker.ensure_special().generated_boundary_affinity = boundary_affinity;
                    marker.ensure_special().marker_owner = b.id;
                    marker.ensure_special().marker_style = MarkerStyle{true, {}};
                    marker.ensure_special().visibility = MarkerVisibility::WhenBlockFocused;
                    rb.inline_items.push_back(std::move(marker));
                };
                append_heading_marker(b.text_special().opening_marker, 0, TextAffinity::Downstream);
                auto items = build_inline_document(
                    b.inline_content(),
                    b.id,
                    s,
                    true,
                    InlineBlockWhitespacePolicy::Heading);
                for (auto& it : items) rb.inline_items.push_back(std::move(it));
                append_heading_marker(
                    b.text_special().closing_marker,
                    b.inline_content().source.size(),
                    TextAffinity::Upstream);
                std::stable_sort(rb.inline_items.begin(), rb.inline_items.end(), [](auto const& left, auto const& right) {
                    return left.source_span.source_range.start < right.source_span.source_range.start;
                });
                return rb;
            }
            case BK::BlockQuote:
            case BK::List:
            case BK::TaskList:
            case BK::Callout:
            case BK::FootnoteDefinition: {
                std::vector<InlineRenderItem> flow_items;
                auto rb = append_flow_block(flow_items, b, FlowContext{0});
                rb.inline_items = std::move(flow_items);
                return rb;
            }
            case BK::HtmlContainer: {
                std::vector<InlineRenderItem> flow_items;
                auto rb = append_flow_block(flow_items, b, FlowContext{0});
                rb.inline_items = std::move(flow_items);
                return rb;
            }
            case BK::CodeBlock: {
                auto rb = base(BlockStyle::code(theme.layout));
                auto& special = rb.ensure_special();
                special.raw_source = b.block_source.source();
                special.content_to_source = b.block_source.tree().content_to_source;
                special.generated_code_indentation = b.block_source.tree().generated_indentation;
                special.language = b.block_source.tree().language;
                special.code_text = block_source_content(b.block_source);
                replace_inline_display_nuls(special.code_text);
                special.code_indented = b.atomic_special().code_indented;
                special.line_count = code_presentation_lines(special.code_text).size();
                if (!special.content_to_source.empty()) {
                    rb.content_span = {b.id, {special.content_to_source.front(), special.content_to_source.back()}};
                }
                return rb;
            }
            case BK::MathBlock: {
                auto rb = base(BlockStyle::math(theme.layout));
                auto& special = rb.ensure_special();
                special.raw_source = b.block_source.source();
                special.content_to_source = b.block_source.tree().content_to_source;
                special.tex = block_source_content(b.block_source);
                special.math_delim = b.atomic_special().math_delim;
                auto const& sourceTree = b.block_source.tree();
                const auto explicit_delimiters = block_source_has_math_delimiters(b.block_source);
                for (auto const& token : sourceTree.tokens) {
                    auto opening = token.kind == BlockSourceTokenKind::OpeningMarker;
                    auto closing = token.kind == BlockSourceTokenKind::ClosingMarker;
                    if (!opening && !closing) continue;
                    auto range = token.source_range;
                    auto icon = SyntaxMarker::None;
                    if (explicit_delimiters || (sourceTree.complete_opening && sourceTree.complete_closing)) {
                        if (!explicit_delimiters && opening) range.start = range.end - 2;
                        icon = special.math_delim == MathDelimiter::BlockDollar ? SyntaxMarker::DoubleDollar
                            : opening ? SyntaxMarker::MathBracketOpen : SyntaxMarker::MathBracketClose;
                    }
                    special.source_markers.push_back({range, icon});
                }
                if (!special.content_to_source.empty()) {
                    rb.content_span = {b.id, {special.content_to_source.front(), special.content_to_source.back()}};
                }
                return rb;
            }
            case BK::Table: {
                auto rb = base(BlockStyle::table(theme.layout));
                auto& special = rb.ensure_special();
                special.table_aligns = b.table_special().table_aligns;
                special.table_metadata = b.table_special().metadata;
                special.table_grid_editable = !b.has_html_source();
                special.table_header_row = !b.children.empty() && b.children.front().table_special().table_header_row;
                special.column_count = b.children.empty() ? 0 : b.children.front().children.size();
                special.row_count = b.children.size();
                auto build_cell = [&](const BlockNode& cell) {
                    const auto& content = cell.inline_content();
                    if (special.table_grid_editable)
                        if (auto checkbox = table_checkbox(content.source))
                            special.table_checkboxes.emplace(special.table_cell_spans.size(), *checkbox);
                    special.table_cell_spans.push_back({cell.id, {0, content.source.size()}});
                    render_key_detail::Hasher key;
                    key.scalar(cell.id);
                    render_key_detail::append(key, content);
                    special.table_cell_keys.push_back(key.value);
                    return build_inline_document(content, cell.id, InlineStyle::plain());
                };
                for (const auto& row : b.children) for (const auto& cell : row.children) special.table_cells.push_back(build_cell(cell));
                return rb;
            }
            case BK::ImageBlock: {
                auto rb = base(BlockStyle::image(theme.layout));
                auto& special = rb.ensure_special();
                special.alt = b.image_special().image_alt; special.src = b.image_special().src; special.title = b.image_special().image_title; special.link = b.image_special().image_link;
                special.image_width = b.image_special().image_width; special.image_height = b.image_special().image_height;
                return rb;
            }
            case BK::Toc: {
                return base(BlockStyle::toc(theme.layout));
            }
            case BK::Frontmatter: {
                auto rb = base(BlockStyle::frontmatter(theme.layout));
                rb.ensure_special().raw = b.atomic_special().raw;
                return rb;
            }
            case BK::ThematicBreak: {
                return base(BlockStyle::thematic_break(theme.layout));
            }
            case BK::LinkDefinition: {
                return base(BlockStyle::paragraph(theme.layout));
            }
            case BK::UnsupportedMarkup: {
                auto rb = base(BlockStyle::unsupported(theme.layout));
                auto& special = rb.ensure_special();
                special.raw = b.atomic_special().raw;
                special.reason_text = unsupported_reason_message(b.atomic_special().unsup_reason);
                return rb;
            }
            case BK::Extension: {
                auto rb = base(BlockStyle::extension(theme.layout));
                rb.ensure_special().extension_name = b.atomic_special().ext_name;
                return rb;
            }
        }
        return base(BlockStyle::paragraph(theme.layout));
    }

    RenderBlock build_block_skeleton(const BlockNode& b, RenderFragmentRange range = {}) {
        using BK = BlockKind;
        auto style = [&]() {
            switch (b.kind) {
                case BK::Heading: return BlockStyle::heading(b.text_special().level, theme.layout);
                case BK::BlockQuote: return BlockStyle::blockquote(theme.layout);
                case BK::List:
                case BK::TaskList: return BlockStyle::list(theme.layout);
                case BK::CodeBlock: return BlockStyle::code(theme.layout);
                case BK::MathBlock: return BlockStyle::math(theme.layout);
                case BK::Table: return BlockStyle::table(theme.layout);
                case BK::ImageBlock: return BlockStyle::image(theme.layout);
                case BK::Toc: return BlockStyle::toc(theme.layout);
                case BK::Callout: return BlockStyle::callout(b.container_special().callout_kind, theme.layout);
                case BK::FootnoteDefinition: return BlockStyle::footnote(theme.layout);
                case BK::Frontmatter: return BlockStyle::frontmatter(theme.layout);
                case BK::ThematicBreak: return BlockStyle::thematic_break(theme.layout);
                case BK::UnsupportedMarkup: return BlockStyle::unsupported(theme.layout);
                case BK::Extension: return BlockStyle::extension(theme.layout);
                default: return BlockStyle::paragraph(theme.layout);
            }
        }();
        auto rendered = render_block_base(b.kind, b.id, block_local_length(b), std::move(style));
        rendered.materialized = false;
        if ((b.kind == BK::Paragraph || b.kind == BK::CalloutTitle || b.kind == BK::TableCell)
            && b.inline_content().source.empty()
            && b.text_special().opening_marker.empty()
            && b.text_special().closing_marker.empty()) {
            rendered.kind = RenderBlockKind::Blank;
        }
        if (b.kind == BK::Heading) rendered.text_heading_level = b.text_special().level;
        if (b.kind == BK::CodeBlock || b.kind == BK::MathBlock) {
            auto const& offsets = b.block_source.tree().content_to_source;
            if (!offsets.empty()) {
                rendered.content_span = {b.id, {offsets.front(), offsets.back()}};
            }
        }
        if (b.kind == BK::ImageBlock) {
            auto& special = rendered.ensure_special();
            special.src = b.image_special().src;
            special.alt = b.image_special().image_alt;
            special.image_width = b.image_special().image_width;
            special.image_height = b.image_special().image_height;
        }

        std::uint64_t characters = 0;
        std::uint64_t line_breaks = 0;
        auto count_text = [&](auto const& text) {
            characters += text.size();
            line_breaks += static_cast<std::uint64_t>(std::ranges::count(text, U'\n'));
        };
        auto visit = [&](BlockNode const& block, auto) {
            if (auto const* inline_document = editable_inline_document(block)) {
                count_text(inline_document->source);
                characters += block.text_special().opening_marker.size();
                characters += block.text_special().closing_marker.size();
            } else if (block.kind == BK::CodeBlock || block.kind == BK::MathBlock) {
                count_text(block.block_source.tree().content);
            } else if (block.kind == BK::Frontmatter
                || block.kind == BK::UnsupportedMarkup
                || block.kind == BK::LinkDefinition) {
                characters += block.atomic_special().raw.size();
                line_breaks += static_cast<std::uint64_t>(
                    std::ranges::count(block.atomic_special().raw, '\n'));
            } else if (block.kind == BK::ImageBlock) {
                characters += block.image_special().image_alt.size();
            }
            characters += block.item_special().marker.size();
        };
        visit_render_fragment(b, range, visit);
        if (b.kind == BK::Table) {
            // The height estimator treats this field as a row-count fallback
            // until the full table projection is materialized.
            line_breaks = (std::max)(line_breaks, static_cast<std::uint64_t>(b.children.size()));
        }
        rendered.estimated_characters = static_cast<std::uint32_t>((std::min)(
            characters,
            static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)())));
        rendered.estimated_line_breaks = static_cast<std::uint32_t>((std::min)(
            line_breaks,
            static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)())));
        return rendered;
    }
    RenderBlock build_fragment(BlockNode const& source, RenderFragment const& fragment, bool skeleton) {
        auto range = fragment_range(fragment);
        RenderBlock rendered;
        if (skeleton) {
            rendered = build_block_skeleton(source, range);
        } else if (is_fragment_flow_container(source.kind)) {
            std::vector<InlineRenderItem> flow;
            rendered = append_flow_block(flow, source, FlowContext{}, 0, 0, range);
            rendered.inline_items = std::move(flow);
        } else {
            rendered = build_block(source);
        }
        rendered.id = fragment.id;
        rendered.fragment_continues = !fragment.last;
        if (!fragment.first) {
            rendered.block_style.margin_top = 0.0f;
            rendered.block_style.padding_top = 0.0f;
        }
        if (!fragment.last) {
            rendered.block_style.margin_bottom = 0.0f;
            rendered.block_style.padding_bottom = 0.0f;
        }
        return rendered;
    }

};

inline std::uint64_t configure_render_dependencies(
    Builder& builder,
    DocumentSymbolIndex const& symbols) {
    std::size_t next_footnote_ordinal = 1;
    render_key_detail::Hasher dependency_hash;
    auto assign_footnote_ordinal = [&](std::string const& label) {
        if (!builder.footnote_ordinals.contains(label)) {
            auto ordinal = next_footnote_ordinal++;
            builder.footnote_ordinals.emplace(label, ordinal);
            dependency_hash.text(label);
            dependency_hash.scalar(ordinal);
        }
    };
    // Number by first reference, as rendered Markdown does. Keep unreferenced
    // definitions visible and stable by assigning their numbers afterwards in
    // source order; their source labels remain authoritative and unchanged.
    for (auto const& reference : symbols.footnote_references) {
        assign_footnote_ordinal(reference.label);
    }
    for (auto const& definition : symbols.footnotes) {
        assign_footnote_ordinal(definition.label);
    }
    return dependency_hash.value;
}

inline void update_render_geometry_hints(RenderBlock& block) {
    std::uint64_t characters = 0;
    std::uint64_t line_breaks = 0;
    std::vector<std::string> image_sources;
    std::optional<std::uint8_t> homogeneous_heading_level;
    bool saw_visible_text = false;
    bool mixed_heading_styles = false;
    auto visit = [&](auto& self, std::vector<InlineRenderItem> const& items) -> void {
        for (auto const& item : items) {
            auto const& text = item.special().display_text.empty()
                ? item.text : item.special().display_text;
            if (!text.empty()) {
                saw_visible_text = true;
                if (!item.style.heading_level) {
                    mixed_heading_styles = true;
                } else if (!homogeneous_heading_level) {
                    homogeneous_heading_level = *item.style.heading_level;
                } else if (*homogeneous_heading_level != *item.style.heading_level) {
                    mixed_heading_styles = true;
                }
            }
            characters += text.size();
            line_breaks += static_cast<std::uint64_t>(std::ranges::count(text, U'\n'));
            if (item.kind == InlineRenderItem::Kind::Image && !item.special().semantic().src.empty()) {
                image_sources.push_back(item.special().semantic().src);
            }
            self(self, item.special().semantic().children);
        }
    };
    visit(visit, block.inline_items);
    if (saw_visible_text) {
        block.text_heading_level = homogeneous_heading_level && !mixed_heading_styles
            ? *homogeneous_heading_level
            : 0;
    }
    block.estimated_characters = static_cast<std::uint32_t>((std::min)(
        characters,
        static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)())));
    block.estimated_line_breaks = static_cast<std::uint32_t>((std::min)(
        line_breaks,
        static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)())));
    if (!image_sources.empty())
        block.ensure_special().inline_image_sources = std::move(image_sources);
}

inline RenderModel finish_render_model(
    const EditorDocument& doc,
    const Outline& outline,
    std::vector<RenderBlock> blocks) {
    std::vector<RenderDiagnostic> diags;
    for (const auto& d : doc.diagnostics) diags.push_back(convert_diagnostic(d));
    RenderModel m; m.revision = doc.revision; m.blocks = std::move(blocks);
    m.outline = outline; m.diagnostics = std::move(diags);
    m.editable_order = doc.cached_editable_order;
    m.editable_index = doc.cached_editable_index;
    return m;
}

// A physical slot is resolved through its semantic root and range. No host
// assumes that the render-model index is a document.root.children index.
inline RenderBlock build_render_fragment(
    Builder& builder, EditorDocument const& doc, RenderFragment const& fragment,
    std::uint64_t dependency_key, bool skeleton,
    std::optional<std::uint64_t> known_key = std::nullopt) {
    auto const* source = find_document_block(doc, fragment.source_root);
    if (!source) return {};
    auto rendered = builder.build_fragment(*source, fragment, skeleton);
    if (!skeleton) {
        update_render_geometry_hints(rendered);
        rendered.source_key = known_key ? *known_key
            : render_key_detail::fragment_key(*source, fragment, dependency_key);
    }
    render_key_detail::Hasher identity;
    identity.scalar(dependency_key);
    identity.scalar(fragment.id);
    rendered.presentation_key = skeleton ? identity.value : rendered.source_key;
    return rendered;
}

inline RenderModel build_render_model_with_fragments(
    const EditorDocument& doc,
    const Outline& outline,
    DocumentSymbolIndex const& symbols,
    ThemeProfile const& theme,
    bool virtualized) {
    Builder builder(theme);
    auto dependency_key = configure_render_dependencies(builder, symbols);
    auto fragments = plan_render_fragments(doc, render_key_detail::context_key);
    std::vector<RenderBlock> blocks;
    blocks.reserve(fragments.size());
    for (auto const& fragment : fragments)
        blocks.push_back(build_render_fragment(builder, doc, fragment, dependency_key, virtualized));
    auto model = finish_render_model(doc, outline, std::move(blocks));
    model.owner_intervals = std::make_shared<const std::vector<RenderOwnerInterval>>(
        index_render_fragment_owners(doc, fragments));
    model.fragments = std::move(fragments);
    model.document_dependency_key = dependency_key;
    model.virtualized = virtualized;
    model.rebuilt_block_count = virtualized ? 0 : model.blocks.size();
    model.reused_block_count = virtualized ? model.blocks.size() : 0;
    return model;
}

inline RenderModel build_render_model(
    const EditorDocument& doc,
    const Outline& outline,
    DocumentSymbolIndex const& symbols,
    ThemeProfile const& theme) {
    return build_render_model_with_fragments(doc, outline, symbols, theme, false);
}

inline RenderModel build_virtualized_render_model(
    const EditorDocument& doc,
    const Outline& outline,
    DocumentSymbolIndex const& symbols,
    ThemeProfile const& theme) {
    return build_render_model_with_fragments(doc, outline, symbols, theme, true);
}

inline void materialize_render_model_range(
    RenderModel& model,
    const EditorDocument& doc,
    DocumentSymbolIndex const& symbols,
    ThemeProfile const& theme,
    std::size_t begin,
    std::size_t end) {
    if (!model.virtualized || model.blocks.empty()) return;
    begin = (std::min)(begin, model.blocks.size());
    end = (std::min)((std::max)(begin, end), model.blocks.size());
    Builder builder(theme);
    auto dependency_key = configure_render_dependencies(builder, symbols);
    for (auto index = begin; index < end; ++index) {
        if (model.blocks[index].materialized) continue;
        model.blocks[index] = build_render_fragment(
            builder, doc, model.fragments[index], dependency_key, false);
        model.materialized_block_indices.insert(index);
    }
}

inline void release_render_model_blocks_outside(
    RenderModel& model,
    const EditorDocument& doc,
    ThemeProfile const& theme,
    std::size_t retain_begin,
    std::size_t retain_end) {
    if (!model.virtualized || model.materialized_block_indices.empty()) return;
    retain_begin = (std::min)(retain_begin, model.blocks.size());
    retain_end = (std::min)((std::max)(retain_begin, retain_end), model.blocks.size());
    Builder builder(theme);
    auto active = std::vector<std::size_t>(
        model.materialized_block_indices.begin(),
        model.materialized_block_indices.end());
    for (auto index : active) {
        if (index >= retain_begin && index < retain_end) continue;
        if (index >= model.fragments.size()) {
            model.materialized_block_indices.erase(index);
            continue;
        }
        auto const key = model.blocks[index].source_key;
        auto const presentation = model.blocks[index].presentation_key;
        auto skeleton = build_render_fragment(
            builder, doc, model.fragments[index], model.document_dependency_key, true);
        // Eviction is not a content change. Retain the exact fingerprint so
        // detached measured geometry remains reusable after a viewport move.
        skeleton.source_key = key;
        skeleton.presentation_key = presentation;
        model.blocks[index] = std::move(skeleton);
        model.materialized_block_indices.erase(index);
    }
}

namespace render_fragment_update_detail {

struct Repair {
    std::vector<RenderFragment> fragments;
};

inline bool same_semantic_payload(DocumentTreeDelta const& delta) {
    if (!delta.before_payload || !delta.after_payload || delta.source_payload_changed) return false;
    render_key_detail::Hasher before;
    render_key_detail::Hasher after;
    render_key_detail::append_metadata(before, *delta.before_payload);
    render_key_detail::append_metadata(after, *delta.after_payload);
    return before.value == after.value;
}

inline std::optional<Repair> repair_plan(
    EditorDocument const& document, RenderModel const& previous,
    RenderModelUpdate const& update) {
    if (update.tree_deltas.empty() || previous.fragments.empty()
        || previous.fragments.size() != previous.blocks.size()) return std::nullopt;
    struct Root {
        NodeId id{};
        std::size_t begin = 0;
        std::size_t end = 0;
        std::size_t dirty_begin = 0;
        std::size_t dirty_end = 0;
        bool affected = false;
        std::vector<BlockPath> insertions;
    };
    std::vector<Root> roots;
    std::unordered_map<std::uint64_t, std::size_t> root_index;
    for (std::size_t index = 0; index < previous.fragments.size(); ++index) {
        auto const& fragment = previous.fragments[index];
        if (roots.empty() || roots.back().id != fragment.source_root) {
            if (root_index.contains(fragment.source_root.v)) return std::nullopt;
            root_index.emplace(fragment.source_root.v, roots.size());
            roots.push_back({fragment.source_root, index, index + 1, previous.fragments.size(), 0});
        } else roots.back().end = index + 1;
        if (fragment.first_leaf_id.v == 0 || fragment.last_leaf_id.v == 0) return std::nullopt;
    }
    std::unordered_set<std::uint64_t> replaced_roots;
    for (auto const& delta : update.tree_deltas)
        if (delta.kind == DocumentTreeEditKind::Insert || delta.kind == DocumentTreeEditKind::Remove)
            replaced_roots.insert(delta.root_id.v);
    auto mark = [&](Root& root, std::size_t begin, std::size_t end) {
        root.affected = true;
        root.dirty_begin = (std::min)(root.dirty_begin, begin);
        root.dirty_end = (std::max)(root.dirty_end, end);
    };
    for (auto const& delta : update.tree_deltas) {
        if (delta.kind == DocumentTreeEditKind::UpdatePayload) {
            if (!same_semantic_payload(delta)) return std::nullopt;
            continue;
        }
        // Renumbering and moving between ancestry contexts have a wider real
        // presentation dependency. They use the general structural planner.
        if (delta.kind == DocumentTreeEditKind::Move || delta.kind == DocumentTreeEditKind::ReorderChildren)
            return std::nullopt;
        auto parent_path = cached_document_block_path(document, delta.parent_id);
        if (!parent_path || parent_path->empty()) return std::nullopt;
        auto const* parent = block_at_path(document.root, *parent_path);
        if (!parent || parent->id != delta.parent_id || parent->children.empty()
            || !is_fragment_flow_container(parent->kind)
            || ((parent->kind == BlockKind::List || parent->kind == BlockKind::TaskList)
                && parent->list_special().ordered)) return std::nullopt;
        auto const* ancestor = &document.root;
        for (auto index : *parent_path) {
            ancestor = &ancestor->children[index];
            if (replaced_roots.contains(ancestor->id.v)) return std::nullopt;
        }
        auto root_id = document.root.children[parent_path->front()].id;
        auto found_root = root_index.find(root_id.v);
        if (found_root == root_index.end()) return std::nullopt;
        auto& root = roots[found_root->second];
        root.affected = true;
        if (delta.kind == DocumentTreeEditKind::Insert) {
            auto path = cached_document_block_path(document, delta.root_id);
            if (!path || path->size() != parent_path->size() + 1
                || !std::equal(parent_path->begin(), parent_path->end(), path->begin())) return std::nullopt;
            auto const* node = block_at_path(document.root, *path);
            if (!node || node->id != delta.root_id || delta.first_owner.v == 0 || delta.last_owner.v == 0)
                return std::nullopt;
            path->erase(path->begin());
            root.insertions.push_back(std::move(*path));
        } else {
            if (document.cached_block_locators.contains(delta.root_id.v)) return std::nullopt;
            auto first = previous.fragment_for_owner(delta.first_owner);
            auto last = previous.fragment_for_owner(delta.last_owner);
            if (!first || !last || *first < root.begin || *last >= root.end || *first > *last)
                return std::nullopt;
            // Include neighbors for closure changes and local underflow merges.
            mark(root, *first > root.begin ? *first - 1 : root.begin,
                (std::min)(root.end, *last + 2));
        }
    }
    for (auto owner : update.changed_owners) {
        auto found = previous.fragment_for_owner(owner);
        if (!found) continue;
        auto index = *found;
        if (index >= previous.fragments.size()) return std::nullopt;
        auto& root = roots[root_index.at(previous.fragments[index].source_root.v)];
        mark(root, index, index + 1);
    }
    auto repaired = previous.fragments;
    for (auto& root : roots) {
        if (!root.affected) continue;
        for (auto index = root.begin; index < root.end; ++index) {
            auto& fragment = repaired[index];
            auto first = render_fragment_detail::leaf_path(document, root.id, fragment.first_leaf_id);
            auto last = render_fragment_detail::leaf_path(document, root.id, fragment.last_leaf_id);
            if (!first || !last) {
                mark(root, index > root.begin ? index - 1 : root.begin, (std::min)(root.end, index + 2));
                continue;
            }
            if (*first > *last) return std::nullopt;
            fragment.first_leaf = std::move(*first);
            fragment.last_leaf = std::move(*last);
        }
        for (auto const& insertion : root.insertions) {
            auto index = root.begin;
            for (auto candidate = root.begin; candidate < root.end; ++candidate) {
                // Missing endpoints already lie in the dirty splice; skip
                // their stale paths while locating an inserted subtree.
                if (!document.cached_block_locators.contains(repaired[candidate].first_leaf_id.v)) continue;
                if (repaired[candidate].first_leaf > insertion) break;
                index = candidate;
            }
            mark(root, index, index + 1);
        }
    }
    Repair result;
    result.fragments.reserve(previous.fragments.size() + update.tree_deltas.size());
    auto append_unchanged = [&](std::size_t begin, std::size_t end) {
        for (auto index = begin; index < end; ++index) {
            auto& fragment = repaired[index];
            if (fragment.first && fragment.last) {
                fragment.first_leaf.clear();
                fragment.last_leaf.clear();
            }
            result.fragments.push_back(std::move(fragment));
        }
    };
    for (auto const& root : roots) {
        if (root.dirty_begin >= root.dirty_end) {
            append_unchanged(root.begin, root.end);
            continue;
        }
        auto path = cached_document_block_path(document, root.id);
        if (!path || path->size() != 1) return std::nullopt;
        auto const* source = block_at_path(document.root, *path);
        if (!source || source->id != root.id) return std::nullopt;
        auto first = root.dirty_begin == root.begin
            ? std::optional{render_fragment_detail::edge_leaf_path(*source, false)}
            : render_fragment_detail::adjacent_leaf_path(*source, repaired[root.dirty_begin - 1].last_leaf, true);
        auto last = root.dirty_end == root.end
            ? std::optional{render_fragment_detail::edge_leaf_path(*source, true)}
            : render_fragment_detail::adjacent_leaf_path(*source, repaired[root.dirty_end].first_leaf, false);
        if (!first || !last || *first > *last) return std::nullopt;
        auto context_key = render_key_detail::context_key;
        auto replacement = render_fragment_detail::plan_interval(*source, context_key,
            {*first, *last}, {}, true);
        if (replacement.empty()) return std::nullopt;
        append_unchanged(root.begin, root.dirty_begin);
        result.fragments.insert(result.fragments.end(), std::make_move_iterator(replacement.begin()),
            std::make_move_iterator(replacement.end()));
        append_unchanged(root.dirty_end, root.end);
    }
    return result;
}

} // namespace render_fragment_update_detail

inline RenderModel build_render_model_incremental(
    const EditorDocument& doc,
    const Outline& outline,
    DocumentSymbolIndex const& symbols,
    ThemeProfile const& theme,
    RenderModel previous,
    RenderModelUpdate const& update) {
    Builder builder(theme);
    auto dependency_key = configure_render_dependencies(builder, symbols);
    bool local = !update.structural
        && dependency_key == previous.document_dependency_key
        && previous.fragments.size() == previous.blocks.size();
    std::vector<std::size_t> changed;
    if (local) {
        for (auto owner : update.changed_owners) {
            auto found = previous.fragment_for_owner(owner);
            if (!found) { local = false; break; }
            changed.push_back(*found);
        }
    }
    if (local) {
        std::ranges::sort(changed);
        changed.erase(std::unique(changed.begin(), changed.end()), changed.end());
        for (auto index : changed) {
            auto was_materialized = previous.blocks[index].materialized;
            previous.blocks[index] = build_render_fragment(
                builder, doc, previous.fragments[index], dependency_key,
                previous.virtualized && !was_materialized);
            if (previous.blocks[index].materialized && previous.virtualized)
                previous.materialized_block_indices.insert(index);
            // A skeleton also needs a new revision key for offscreen edits.
            if (!previous.blocks[index].materialized) {
                render_key_detail::Hasher key;
                key.scalar(previous.blocks[index].presentation_key);
                key.scalar(doc.revision);
                previous.blocks[index].presentation_key = key.value;
            }
        }
        previous.revision = doc.revision;
        if (previous.outline.content_key == outline.content_key) {
            previous.outline.revision = outline.revision;
            previous.outline.content_revision = outline.content_revision;
        } else {
            previous.outline = outline;
        }
        previous.rebuilt_block_count = changed.size();
        previous.reused_block_count = previous.blocks.size() - changed.size();
        previous.incremental_update = true;
        previous.changed_block_indices = std::move(changed);
        return previous;
    }

    changed.clear();
    const bool trusted_structural_locality = update.structural
        && update.structural_locality_known
        && (!update.changed_owners.empty() || !update.structural_anchors.empty())
        && dependency_key == previous.document_dependency_key;
    std::unordered_set<std::uint64_t> changed_roots;
    if (trusted_structural_locality) {
        auto note_root = [&](NodeId owner) {
            if (auto root = document_top_level_block_id(doc, owner)) changed_roots.insert(root->v);
            if (auto found = previous.fragment_for_owner(owner))
                changed_roots.insert(previous.fragments[*found].source_root.v);
        };
        for (auto owner : update.changed_owners) note_root(owner);
        for (auto anchor : update.structural_anchors) note_root(anchor);
    }

    // Structural operations reconcile physical identities. Hash only bounded
    // fragments, never the entire list as a single subtree. Unchanged visual
    // objects (including their exact measured geometry in the host) survive.
    auto repair = trusted_structural_locality
        ? render_fragment_update_detail::repair_plan(doc, previous, update)
        : std::nullopt;
    std::unordered_map<std::uint64_t, std::size_t> previous_by_id;
    previous_by_id.reserve(previous.blocks.size());
    for (std::size_t index = 0; index < previous.blocks.size(); ++index)
        previous_by_id.emplace(previous.blocks[index].id.v, index);
    auto fragments = repair ? std::move(repair->fragments)
        : plan_render_fragments(doc, render_key_detail::context_key, previous.fragments);
    auto owner_intervals = index_render_fragment_owners(doc, fragments);
    std::unordered_set<std::size_t> dirty_sources;
    auto mark_source = [&](NodeId owner) {
        auto found = doc.cached_editable_index.find(owner.v);
        if (found != doc.cached_editable_index.end()) {
            if (auto fragment = render_fragment_at(owner_intervals, found->second))
                dirty_sources.insert(*fragment);
        }
    };
    for (auto owner : update.changed_owners) mark_source(owner);
    // A payload-only change can modify an atomic raw source without a TextEdit.
    // Editable anchors name exactly that unit; container anchors are accounted
    // for by the source-free structure/context fingerprint instead.
    for (auto anchor : update.structural_anchors) mark_source(anchor);
    std::vector<RenderBlock> blocks;
    blocks.reserve(fragments.size());
    std::size_t reused = 0;
    std::size_t rebuilt = 0;
    bool same_slots = fragments.size() == previous.fragments.size();
    for (std::size_t index = 0; index < fragments.size(); ++index) {
        auto const& fragment = fragments[index];
        if (same_slots && (fragment.id != previous.fragments[index].id
            || fragment.source_root != previous.fragments[index].source_root)) same_slots = false;
        auto found = previous_by_id.find(fragment.id.v);
        if (trusted_structural_locality && !changed_roots.contains(fragment.source_root.v)
            && found != previous_by_id.end() && !previous.blocks[found->second].source_mode) {
            blocks.push_back(std::move(previous.blocks[found->second]));
            ++reused;
            continue;
        }
        if (trusted_structural_locality && !dirty_sources.contains(index)
            && found != previous_by_id.end() && found->second < previous.fragments.size()) {
            auto const& old_fragment = previous.fragments[found->second];
            if (old_fragment.source_root == fragment.source_root
                && old_fragment.structure_key == fragment.structure_key
                && old_fragment.first == fragment.first && old_fragment.last == fragment.last
                && !previous.blocks[found->second].source_mode) {
                blocks.push_back(std::move(previous.blocks[found->second]));
                ++reused;
                continue;
            }
        }
        auto const* source = find_document_block(doc, fragment.source_root);
        auto key = source ? render_key_detail::fragment_key(*source, fragment, dependency_key) : 0;
        if (found != previous_by_id.end()
            && previous.blocks[found->second].source_key == key
            && !previous.blocks[found->second].source_mode) {
            blocks.push_back(std::move(previous.blocks[found->second]));
            ++reused;
        } else {
            auto rendered = build_render_fragment(builder, doc, fragment, dependency_key, previous.virtualized, key);
            rendered.source_key = key;
            rendered.presentation_key = key;
            blocks.push_back(std::move(rendered));
            changed.push_back(index);
            ++rebuilt;
        }
    }
    auto model = finish_render_model(doc, outline, std::move(blocks));
    model.owner_intervals = std::make_shared<const std::vector<RenderOwnerInterval>>(std::move(owner_intervals));
    model.fragments = std::move(fragments);
    model.document_dependency_key = dependency_key;
    model.rebuilt_block_count = rebuilt;
    model.reused_block_count = reused;
    model.incremental_update = same_slots;
    model.virtualized = previous.virtualized;
    if (model.virtualized) {
        for (std::size_t index = 0; index < model.blocks.size(); ++index)
            if (model.blocks[index].materialized) model.materialized_block_indices.insert(index);
    }
    if (same_slots) model.changed_block_indices = std::move(changed);
    return model;
}

} // namespace folia
