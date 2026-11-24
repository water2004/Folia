#pragma once

// Block tree projection, including code metadata and GFM containers.
#include "support/commonmark/inlines.hpp"

namespace folia::test::commonmark::projection_detail {

inline std::string block_kind_name(BlockKind kind) {
    switch (kind) {
        case BlockKind::Document: return "Document";
        case BlockKind::Paragraph: return "Paragraph";
        case BlockKind::Heading: return "Heading";
        case BlockKind::CalloutTitle: return "CalloutTitle";
        case BlockKind::BlockQuote: return "BlockQuote";
        case BlockKind::List: return "List";
        case BlockKind::TaskList: return "TaskList";
        case BlockKind::ListItem: return "ListItem";
        case BlockKind::TaskListItem: return "TaskListItem";
        case BlockKind::CodeBlock: return "CodeBlock";
        case BlockKind::MathBlock: return "MathBlock";
        case BlockKind::Table: return "Table";
        case BlockKind::TableRow: return "TableRow";
        case BlockKind::TableCell: return "TableCell";
        case BlockKind::ImageBlock: return "ImageBlock";
        case BlockKind::Callout: return "Callout";
        case BlockKind::FootnoteDefinition: return "FootnoteDefinition";
        case BlockKind::Toc: return "Toc";
        case BlockKind::Frontmatter: return "Frontmatter";
        case BlockKind::ThematicBreak: return "ThematicBreak";
        case BlockKind::LinkDefinition: return "LinkDefinition";
        case BlockKind::UnsupportedMarkup: return "UnsupportedMarkup";
        case BlockKind::HtmlContainer: return "HtmlContainer";
        case BlockKind::Extension: return "Extension";
    }
    return "Unknown";
}

inline std::string code_block_text(BlockSourceDocument const& source) {
    auto text = utf8(block_source_code_projection(source));
    auto const& tree = source.tree();
    auto const boundary = tree.content_to_source.empty() ? std::size_t{0}
        : tree.content_to_source.back();
    bool separated_ending = std::ranges::any_of(tree.tokens, [&](auto const& token) {
        return token.kind == BlockSourceTokenKind::LineBreak
            && token.source_range.start == boundary;
    });
    // Indented-code normalization trims trailing blank lines but retains their
    // source offsets in the terminal mapping. Fenced code instead keeps the
    // separator before its closing fence as the ending of its final content
    // line, so that ending must remain part of the projected code text.
    if (tree.kind == BlockSourceKind::IndentedCode) {
        if (!text.empty() && text.back() != '\n') text.push_back('\n');
    } else if (separated_ending) {
        // For fenced code this token is the ending of the last content line,
        // including an otherwise empty final code line before the closing fence.
        text.push_back('\n');
    } else if (!text.empty() && text.back() != '\n') {
        text.push_back('\n');
    }
    return text;
}

inline std::u32string decoded_code_meta(std::u32string_view raw) {
    std::u32string decoded;
    for (std::size_t cursor = 0; cursor < raw.size();) {
        if (raw[cursor] == U'&') {
            auto end = cursor + 1;
            while (end < raw.size() && raw[end] != U';'
                && raw[end] != U' ' && raw[end] != U'\t') ++end;
            if (end < raw.size() && raw[end] == U';') {
                if (auto value = decode_html_character_reference(raw.substr(cursor, end - cursor + 1))) {
                    decoded.append(*value);
                    cursor = end + 1;
                    continue;
                }
            }
        }
        if (raw[cursor] == U'\\' && cursor + 1 < raw.size()) {
            const auto next = raw[cursor + 1];
            const bool punctuation = (next >= U'!' && next <= U'/')
                || (next >= U':' && next <= U'@')
                || (next >= U'[' && next <= U'`')
                || (next >= U'{' && next <= U'~');
            if (punctuation) {
                decoded.push_back(next);
                cursor += 2;
                continue;
            }
        }
        decoded.push_back(raw[cursor++]);
    }
    for (auto& ch : decoded) if (ch == U'\0') ch = U'\uFFFD';
    return decoded;
}

inline bool project_block(
    Node& parent,
    BlockNode const& block,
    std::vector<std::string>& limitations,
    ProjectionOptions const& options) {
    // HTML-backed semantic trees retain their original source as the HTML
    // projection input. This test parses fresh documents; editing/save checks
    // remain separate and do not use this rendering convenience.
    if (block.has_html_source()) {
        append_raw_html(parent, utf8(block.html_special().source) + "\n");
        return true;
    }
    auto add_children = [&](Node& node) {
        for (auto const& child : block.children) project_block(node, child, limitations, options);
    };
    switch (block.kind) {
        case BlockKind::Document:
            add_children(parent);
            return false;
        case BlockKind::Paragraph: {
            // Empty editable paragraphs encode preserved blank source lines.
            // They are not Markdown paragraphs in the specification's HTML.
            if (block.inline_content().source.empty()
                || (block.inline_content().syntax_mode == InlineSyntaxMode::Markdown
                    && std::ranges::all_of(block.inline_content().source, [](char32_t ch) {
                        return ch == U' ' || ch == U'\t';
                    }))) return false;
            if (block.inline_content().syntax_mode == InlineSyntaxMode::HtmlText) {
                append_raw_html(parent, utf8(block.inline_content().source) + "\n");
                return true;
            }
            auto n = element("p");
            auto semantic_nodes = semantic_inline_nodes(
                block.inline_content(),
                InlineBlockWhitespacePolicy::Paragraph);
            project_inline_nodes(n, block.inline_content(), semantic_nodes, limitations);
            append_node(parent, std::move(n));
            return true;
        }
        case BlockKind::Heading: {
            auto const level = static_cast<int>(block.text_special().level);
            auto n = element("h" + std::to_string(level));
            auto semantic_nodes = semantic_inline_nodes(
                block.inline_content(),
                InlineBlockWhitespacePolicy::Heading);
            project_inline_nodes(n, block.inline_content(), semantic_nodes, limitations);
            append_node(parent, std::move(n));
            return true;
        }
        case BlockKind::BlockQuote: {
            auto n = element("blockquote");
            add_children(n);
            append_node(parent, std::move(n));
            return true;
        }
        case BlockKind::List:
        case BlockKind::TaskList: {
            auto const& list = block.list_special();
            std::vector<std::pair<std::string, std::string>> attrs;
            if (list.ordered && list.start != 1) attrs.emplace_back("start", std::to_string(list.start));
            auto n = element(list.ordered ? "ol" : "ul", std::move(attrs));
            for (auto const& item : block.children) {
                auto li = element("li");
                const bool task_item = item.kind == BlockKind::TaskListItem;
                bool checkbox_pending = task_item;
                auto checkbox_attrs = std::vector<std::pair<std::string, std::string>>{
                    {"disabled", ""}, {"type", "checkbox"}};
                if (task_item && item.item_special().checked) checkbox_attrs.emplace_back("checked", "");
                auto checkbox = task_item ? element("input", std::move(checkbox_attrs)) : Node{};
                auto append_checkbox = [&](Node& target) {
                    append_node(target, std::move(checkbox));
                    checkbox_pending = false;
                    auto const& marker = item.item_special().marker;
                    if (!marker.empty() && (marker.back() == U' ' || marker.back() == U'\t')) {
                        append_text(target, utf8(std::u32string_view(marker).substr(marker.size() - 1)));
                    }
                };
                for (auto const& child : item.children) {
                    if (list.tight && child.kind == BlockKind::Paragraph) {
                        if (!child.inline_content().source.empty()) {
                            if (checkbox_pending) append_checkbox(li);
                            if (child.inline_content().syntax_mode == InlineSyntaxMode::HtmlText) {
                                append_raw_html(li, utf8(child.inline_content().source) + "\n");
                            } else {
                                auto semantic_nodes = semantic_inline_nodes(
                                    child.inline_content(),
                                    InlineBlockWhitespacePolicy::Paragraph);
                                project_inline_nodes(
                                    li,
                                    child.inline_content(),
                                    semantic_nodes,
                                    limitations);
                            }
                        }
                    } else if (!list.tight && child.kind == BlockKind::Paragraph && checkbox_pending) {
                        auto p = element("p");
                        append_checkbox(p);
                        auto semantic_nodes = semantic_inline_nodes(
                            child.inline_content(), InlineBlockWhitespacePolicy::Paragraph);
                        project_inline_nodes(p, child.inline_content(), semantic_nodes, limitations);
                        append_node(li, std::move(p));
                    } else {
                        project_block(li, child, limitations, options);
                    }
                }
                if (checkbox_pending) append_checkbox(li);
                append_node(n, std::move(li));
            }
            append_node(parent, std::move(n));
            return true;
        }
        case BlockKind::ListItem: {
            auto n = element("li");
            add_children(n);
            append_node(parent, std::move(n));
            return true;
        }
        case BlockKind::CodeBlock: {
            if (block.block_source.tree().kind == BlockSourceKind::HtmlCode) {
                append_raw_html(parent, utf8(block.block_source.source()) + "\n");
                return true;
            }
            auto pre = element("pre");
            std::vector<std::pair<std::string, std::string>> attrs;
            auto const& language = block.block_source.tree().language;
            if (language && !language->empty()) attrs.emplace_back("class", "language-" + *language);
            auto const& source = block.block_source.source();
            auto const& source_tree = block.block_source.tree();
            if (options.full_info_string && source_tree.info_range) {
                auto meta_start = source_tree.language_range
                    ? source_tree.language_range->end : source_tree.info_range->start;
                auto meta_end = source_tree.info_range->end;
                while (meta_start < meta_end
                    && (source[meta_start] == U' ' || source[meta_start] == U'\t')) ++meta_start;
                while (meta_end > meta_start
                    && (source[meta_end - 1] == U' ' || source[meta_end - 1] == U'\t')) --meta_end;
                if (meta_start < meta_end) {
                    attrs.emplace_back("data-meta", utf8(decoded_code_meta(
                        std::u32string_view(source).substr(meta_start, meta_end - meta_start))));
                }
            }
            auto code = element("code", std::move(attrs));
            append_text(code, code_block_text(block.block_source));
            append_node(pre, std::move(code));
            append_node(parent, std::move(pre));
            return true;
        }
        case BlockKind::ThematicBreak:
            append_node(parent, element("hr"));
            return true;
        case BlockKind::LinkDefinition:
            // CommonMark link definitions affect inline resolution but render no node.
            return false;
        case BlockKind::ImageBlock: {
            auto const& image = block.image_special();
            std::vector<std::pair<std::string, std::string>> attrs{
                {"alt", image.image_alt}, {"src", image.src}};
            if (image.image_title) attrs.emplace_back("title", *image.image_title);
            auto p = element("p");
            auto image_node = element("img", std::move(attrs));
            if (image.image_link) {
                auto link = element("a", {{"href", *image.image_link}});
                append_node(link, std::move(image_node));
                append_node(p, std::move(link));
            } else {
                append_node(p, std::move(image_node));
            }
            append_node(parent, std::move(p));
            return true;
        }
        case BlockKind::TaskListItem:
        case BlockKind::CalloutTitle:
        case BlockKind::MathBlock:
        case BlockKind::Table:
        case BlockKind::TableRow:
        case BlockKind::TableCell:
        case BlockKind::Callout:
        case BlockKind::FootnoteDefinition:
        case BlockKind::Toc:
        case BlockKind::Frontmatter:
        case BlockKind::UnsupportedMarkup:
        case BlockKind::HtmlContainer:
        case BlockKind::Extension:
            if (block.kind == BlockKind::UnsupportedMarkup) {
                const auto& atomic = block.atomic_special();
                const auto source = utf8_to_cps(atomic.raw);
                const auto line_end = source.find_first_of(U"\r\n");
                const auto start = scan_markdown_html_block_start(
                    source, 0, line_end == std::u32string::npos ? source.size() : line_end);
                if ((atomic.unsup_reason == UnsupportedMarkupReason::UnsafeHtml
                        || atomic.unsup_reason == UnsupportedMarkupReason::MalformedSyntax)
                    && start) {
                    append_raw_html(parent, atomic.raw + "\n");
                } else {
                    append_node(parent, placeholder("extension", "kind", "UnsupportedMarkup"));
                    limitation(limitations, "block-extension:UnsupportedMarkup");
                }
            } else if (block.kind == BlockKind::Table) {
                auto table = element("table");
                auto const& table_data = block.table_special();
                auto alignment = [&](std::size_t column) -> std::optional<std::string> {
                    if (column >= table_data.table_aligns.size()) return std::nullopt;
                    switch (table_data.table_aligns[column]) {
                        case TableAlignment::Left: return "left";
                        case TableAlignment::Center: return "center";
                        case TableAlignment::Right: return "right";
                        case TableAlignment::None: return std::nullopt;
                    }
                    return std::nullopt;
                };
                for (auto const& row : block.children) {
                    const bool header = row.table_special().table_header_row;
                    auto tr = element("tr");
                    for (std::size_t column = 0; column < row.children.size(); ++column) {
                        auto const& cell = row.children[column];
                        std::vector<std::pair<std::string, std::string>> attrs;
                        if (auto align = alignment(column)) {
                            if (options.table_prefer_style_attributes)
                                attrs.emplace_back("style", "text-align: " + *align);
                            else attrs.emplace_back("align", *align);
                        }
                        auto th = element(header ? "th" : "td", std::move(attrs));
                        auto semantic_nodes = semantic_inline_nodes(
                            cell.inline_content(), InlineBlockWhitespacePolicy::Paragraph);
                        project_inline_nodes(th, cell.inline_content(), semantic_nodes, limitations);
                        append_node(tr, std::move(th));
                    }
                    if (header) {
                        auto thead = element("thead");
                        append_node(thead, std::move(tr));
                        append_node(table, std::move(thead));
                    } else {
                        if (!table.children.empty() && table.children.back().node
                            && table.children.back().node->tag == "tbody") {
                            append_node(*table.children.back().node, std::move(tr));
                        } else {
                            auto tbody = element("tbody");
                            append_node(tbody, std::move(tr));
                            append_node(table, std::move(tbody));
                        }
                    }
                }
                append_node(parent, std::move(table));
            } else if (block.kind == BlockKind::FootnoteDefinition) {
                Node definition{"#footnote-definition", {{"label", block.container_special().footnote_label}}, {}};
                add_children(definition);
                append_node(parent, std::move(definition));
            } else if (block.kind == BlockKind::TaskListItem) {
                append_node(parent, placeholder("extension", "kind", "orphan-task-list-item"));
                limitation(limitations, "block-extension:orphan-task-list-item");
            } else if (block.kind == BlockKind::HtmlContainer) {
                append_node(parent, placeholder("html-policy/projection", "kind", block_kind_name(block.kind)));
                limitation(limitations, "block-html-policy/projection");
            } else {
                append_node(parent, placeholder("extension", "kind", block_kind_name(block.kind)));
                limitation(limitations, "block-extension:" + block_kind_name(block.kind));
            }
            return true;
    }
    append_node(parent, placeholder("extension", "kind", block_kind_name(block.kind)));
    limitation(limitations, "block-extension:" + block_kind_name(block.kind));
    return true;
}


} // namespace folia::test::commonmark::projection_detail
