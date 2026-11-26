module folia.core.document_copy;
import std;
import folia.core.table_selection;
import folia.core.document_edit_primitives;
import folia.core.theme;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.dialect;
import folia.markdown.html_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_cst;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.utf;

namespace folia {
namespace {

void append_cell_html(std::u32string& output, const InlineDocument& document, const InlineCstNodes& nodes) {
    for (const auto& node : nodes) {
        using Kind = InlineCstKind;
        auto escaped = [&](std::u32string_view text) { output += serialize_html_attribute_value(text); };
        auto wrapped = [&](std::u32string_view tag) {
            output += U"<";
            output += tag;
            output += U">";
            append_cell_html(output, document, node.children);
            output += U"</";
            output += tag;
            output += U">";
        };
        const auto source = std::u32string_view{document.source};
        const auto raw = node.range.valid_for(source.size()) ? source.substr(node.range.start, node.range.length()) : std::u32string_view{};
        switch (node.kind) {
            case Kind::Strong: wrapped(U"strong"); break;
            case Kind::Emphasis: wrapped(U"em"); break;
            case Kind::Strikethrough: wrapped(U"s"); break;
            case Kind::Highlight: wrapped(U"mark"); break;
            case Kind::SoftBreak:
            case Kind::HardBreak: output += U"<br>"; break;
            case Kind::Escape: if (!raw.empty()) escaped(raw.substr(raw.size() - 1)); break;
            case Kind::Entity: escaped(decode_inline_entity(raw)); break;
            case Kind::HtmlComment: break;
            case Kind::HtmlElement: {
                const auto tag = std::string_view{node.semantics().html_tag};
                constexpr std::array<std::string_view, 10> safe_tags{"b", "strong", "i", "em", "u", "s", "del", "mark", "sup", "sub"};
                if (tag == "br") output += U"<br>";
                else if (std::ranges::contains(safe_tags, tag)) wrapped(utf8_to_cps(tag));
                else append_cell_html(output, document, node.children);
                break;
            }
            case Kind::Link:
            case Kind::Autolink: {
                const auto& href = node.semantics().href;
                const auto external = href.starts_with("https://") || href.starts_with("http://") || href.starts_with("mailto:");
                if (external) output += U"<a href=\"" + serialize_html_attribute_value(utf8_to_cps(href)) + U"\">";
                if (node.children.empty()) escaped(inline_source_slice(document, node.delimiter_ranges().content));
                else append_cell_html(output, document, node.children);
                if (external) output += U"</a>";
                break;
            }
            case Kind::CodeSpan:
                output += U"<code>";
                escaped(normalize_inline_code_span(inline_source_slice(document, node.delimiter_ranges().content), document.syntax_mode));
                output += U"</code>";
                break;
            case Kind::Image: escaped(utf8_to_cps(node.semantics().alt)); break;
            default: escaped(raw); break;
        }
    }
}

std::u32string css_hex(Color color) {
    constexpr std::u32string_view hex = U"0123456789abcdef";
    std::u32string output{U'#'};
    for (auto component : {color.r, color.g, color.b}) {
        output += hex[component >> 4];
        output += hex[component & 15];
    }
    return output;
}

}

static void append_table_tsv_cell(std::u32string& output, std::u32string_view source) {
    if (source.find_first_of(U"\t\r\n\"") == source.npos) {
        output += source;
        return;
    }
    output += U'"';
    for (auto cp : source) {
        if (cp == U'"') output += U'"';
        output += cp;
    }
    output += U'"';
}

std::optional<TableClipboard> document_table_clipboard(
    const EditorDocument& document, TextPosition position,
    std::optional<TableRegion> selected, bool markdown_only, const ThemeColors* colors) {
    const auto address = document_table_cell_address(document, position.container_id);
    if (!address) return std::nullopt;
    auto path = document_block_path(document, address->table_id);
    if (!path || path->empty()) return std::nullopt;
    const auto index = path->back();
    path->pop_back();
    const auto* parent = block_at_path(document.root, *path);
    if (!parent || index >= parent->children.size()) return std::nullopt;
    const auto& table = parent->children[index];
    if (table.has_html_source() || table.children.empty() || table.children.front().children.empty()) return std::nullopt;
    const auto& special = table.table_special();
    const auto rows = table.children.size();
    const auto columns = table.children.front().children.size();
    const auto region = selected.value_or(TableRegion{{0, 0}, {rows - 1, columns - 1}});
    const auto* metadata = markdown_only ? nullptr : special.metadata.get();
    const auto& theme = colors ? *colors : default_theme_profile().colors;
    const auto palette = theme.table_palette();
    TableClipboard clipboard;
    if (!selected) {
        clipboard.markdown = serialize_markdown_fragment(parent->children, index, index + 1);
        if (markdown_only && special.metadata && clipboard.markdown.ends_with(special.metadata->source))
            clipboard.markdown.resize(clipboard.markdown.size() - special.metadata->source.size() - special.metadata->separator.size());
    }
    std::u32string html_rows;
    if (region.last.row >= rows || region.last.column >= columns) return std::nullopt;
    for (auto row = region.first.row; row <= region.last.row; ++row) {
        if (row != region.first.row) clipboard.tab_separated += U'\n';
        if (selected) clipboard.markdown += U"|";
        html_rows += U"<tr>";
        const auto header = row < (metadata ? metadata->header_rows : 1);
        for (auto column = region.first.column; column <= region.last.column; ++column) {
            const auto& content = table.children[row].children[column].inline_content();
            const auto& source = content.source;
            if (column != region.first.column) clipboard.tab_separated += U'\t';
            append_table_tsv_cell(clipboard.tab_separated, source);
            if (selected) {
                clipboard.markdown += U" ";
                clipboard.markdown += source;
                clipboard.markdown += U" |";
            }
            const TableCoordinate cell{row, column};
            const auto* merge = table_merge_at(metadata, cell, rows, columns);
            if (merge && (!region.contains(merge->first) || !region.contains(merge->last))) merge = nullptr;
            if (merge && merge->first != cell) continue;
            html_rows += header ? U"<th" : U"<td";
            if (merge) {
                if (merge->first.row != merge->last.row)
                    html_rows += U" rowspan=\"" + utf8_to_cps(std::to_string(merge->last.row - merge->first.row + 1)) + U"\"";
                if (merge->first.column != merge->last.column)
                    html_rows += U" colspan=\"" + utf8_to_cps(std::to_string(merge->last.column - merge->first.column + 1)) + U"\"";
            }
            const auto alignment = column < special.table_aligns.size() ? special.table_aligns[column] : TableAlignment::None;
            html_rows += U" style=\"border:1px solid " + css_hex(theme.table_border) + U";padding:4px 8px;text-align:";
            html_rows += alignment == TableAlignment::Center ? U"center" : alignment == TableAlignment::Right ? U"right" : U"left";
            if (header) html_rows += U";background-color:" + css_hex(theme.table_header_bg);
            if (metadata) {
                if (column < metadata->widths.size() && metadata->widths[column] > 0)
                    html_rows += U";width:" + utf8_to_cps(std::to_string(static_cast<unsigned>(metadata->widths[column]))) + U"px";
                if (column < metadata->vertical_alignments.size()) {
                    const auto vertical = metadata->vertical_alignments[column];
                    html_rows += U";vertical-align:";
                    html_rows += vertical == TableVerticalAlignment::Middle ? U"middle" : vertical == TableVerticalAlignment::Bottom ? U"bottom" : U"top";
                }
                if (const auto appearance = metadata->cells.find(cell); appearance != metadata->cells.end()) {
                    if (const auto fg = appearance->second.foreground)
                        html_rows += U";color:" + css_hex(palette[static_cast<std::size_t>(*fg)]);
                    if (const auto bg = appearance->second.background) {
                        const auto color = palette[static_cast<std::size_t>(*bg)];
                        auto soften = [](auto base, auto accent) { return static_cast<std::uint8_t>((4U * base + accent) / 5U); };
                        html_rows += U";background-color:" + css_hex(Color{soften(theme.bg.r, color.r),
                            soften(theme.bg.g, color.g), soften(theme.bg.b, color.b)});
                    }
                }
            }
            html_rows += U"\">";
            append_cell_html(html_rows, content, content.tree.nodes);
            if (merge) {
                for (auto mergedRow = merge->first.row; mergedRow <= merge->last.row; ++mergedRow)
                    for (auto mergedColumn = merge->first.column; mergedColumn <= merge->last.column; ++mergedColumn) {
                        if (mergedRow == row && mergedColumn == column) continue;
                        const auto& other = table.children[mergedRow].children[mergedColumn].inline_content();
                        if (other.source.empty()) continue;
                        html_rows += U"<br>";
                        append_cell_html(html_rows, other, other.tree.nodes);
                    }
            }
            html_rows += header ? U"</th>" : U"</td>";
        }
        html_rows += U"</tr>";
        if (selected && row == region.first.row) {
            clipboard.markdown += U"\n|";
            for (auto column = region.first.column; column <= region.last.column; ++column) {
                const auto alignment = column < special.table_aligns.size() ? special.table_aligns[column] : TableAlignment::None;
                clipboard.markdown += alignment == TableAlignment::Center ? U" :---: |"
                    : alignment == TableAlignment::Left ? U" :--- |" : alignment == TableAlignment::Right ? U" ---: |" : U" --- |";
            }
        }
        if (selected && row < region.last.row) clipboard.markdown += U'\n';
    }
    if (selected && !markdown_only) {
        if (auto sliced = slice_table_metadata(special.metadata, region)) {
            clipboard.markdown += sliced->separator;
            clipboard.markdown += sliced->source;
        }
    }
    clipboard.html = U"<table data-ggt=\"1\" style=\"border-collapse:collapse;color:"
        + css_hex(theme.fg) + U";background-color:" + css_hex(theme.bg) + U"\"";
    if (!selected || (region.first.row < region.last.row && region.first.column < region.last.column)) {
        clipboard.html += U" data-ggt-src=\"";
        constexpr std::u32string_view hex = U"0123456789ABCDEF";
        for (const unsigned char byte : cps_to_utf8(clipboard.markdown)) {
            if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')
                || (byte >= '0' && byte <= '9') || byte == '-' || byte == '_' || byte == '.') {
                clipboard.html.push_back(static_cast<char32_t>(byte));
            } else {
                clipboard.html.push_back(U'%');
                clipboard.html.push_back(hex[byte >> 4]);
                clipboard.html.push_back(hex[byte & 15]);
            }
        }
        clipboard.html += U'"';
    }
    clipboard.html += U">" + html_rows + U"</table>";
    return clipboard;
}

std::optional<std::u32string> table_clipboard_markdown(std::u32string_view html) {
    if (!html.contains(U"data-ggt-src")) return std::nullopt;
    auto parsed = parse_html_cst(html);
    std::vector<const HtmlCstNode*> pending;
    for (const auto& node : parsed.nodes) pending.push_back(&node);
    while (!pending.empty()) {
        const auto& node = *pending.back();
        pending.pop_back();
        if (node.tag_name != "table") {
            for (const auto& child : node.children) pending.push_back(&child);
            continue;
        }
        auto attribute = [&](std::string_view name) {
            const auto found = std::ranges::find(node.attributes, name, &HtmlAttribute::name);
            if (found == node.attributes.end() || !found->value_range) return std::u32string_view{};
            const auto range = *found->value_range;
            return html.substr(range.start, range.length());
        };
        if (attribute("data-ggt") != U"1") continue;
        const auto encoded = attribute("data-ggt-src");
        if (encoded.empty()) continue;
        std::string decoded;
        decoded.reserve(encoded.size());
        auto nibble = [](char32_t cp) -> int {
            if (cp >= U'0' && cp <= U'9') return static_cast<int>(cp - U'0');
            if (cp >= U'A' && cp <= U'F') return static_cast<int>(cp - U'A') + 10;
            if (cp >= U'a' && cp <= U'f') return static_cast<int>(cp - U'a') + 10;
            return -1;
        };
        for (std::size_t i = 0; i < encoded.size(); ++i) {
            auto cp = encoded[i];
            if (cp == U'%') {
                if (encoded.size() - i < 3) return std::nullopt;
                const auto high = nibble(encoded[i + 1]);
                const auto low = nibble(encoded[i + 2]);
                if (high < 0 || low < 0) return std::nullopt;
                cp = static_cast<char32_t>((high << 4) | low);
                i += 2;
            } else if (cp > 127) return std::nullopt;
            decoded.push_back(static_cast<char>(cp));
        }
        return utf8_to_cps(decoded);
    }
    return std::nullopt;
}

std::optional<std::u32string> table_clipboard_html_table(std::u32string_view html) {
    auto tree = parse_html_cst(html);
    std::vector<const HtmlCstNode*> pending;
    for (const auto& item : tree.nodes | std::views::reverse) pending.push_back(&item);
    const HtmlCstNode* table = nullptr;
    while (!pending.empty()) {
        const auto& item = *pending.back();
        pending.pop_back();
        if (item.tag_name == "table" && item.range.valid_for(html.size())) {
            if (table) return std::nullopt;
            table = &item;
            continue;
        }
        if (item.kind == HtmlCstKind::Text && item.range.valid_for(html.size())
            && html.substr(item.range.start, item.range.length()).find_first_not_of(U" \t\r\n") != html.npos)
            return std::nullopt;
        if (item.tag_name == "head" || item.tag_name == "style" || item.tag_name == "script") continue;
        for (const auto& child : item.children | std::views::reverse) pending.push_back(&child);
    }
    if (!table) return std::nullopt;
    const auto source = html.substr(table->range.start, table->range.length());
    const auto fragment = parse_block_fragment(source);
    if (fragment.blocks.size() != 1 || fragment.blocks.front().kind != BlockKind::Table) return std::nullopt;
    const auto& rows = fragment.blocks.front().children;
    if (rows.size() < 2) return std::nullopt;
    const auto width = std::ranges::max(rows | std::views::transform([](const auto& row) { return row.children.size(); }));
    if (width < 2) return std::nullopt;
    std::u32string markdown;
    for (std::size_t row = 0; row < rows.size(); ++row) {
        markdown += U'|';
        for (std::size_t column = 0; column < width; ++column) {
            markdown += U' ';
            if (column < rows[row].children.size()) {
                const auto& cell = rows[row].children[column].inline_content().source;
                markdown += document_edit_detail::table_cell_input_source({}, 0, cell);
            }
            markdown += U" |";
        }
        if (row == 0) {
            markdown += U"\n|";
            for (std::size_t column = 0; column < width; ++column) markdown += U" --- |";
        }
        if (row + 1 < rows.size()) markdown += U'\n';
    }
    return markdown;
}

}
