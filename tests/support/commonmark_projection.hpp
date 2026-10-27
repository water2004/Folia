#pragma once
import folia.markdown.markdown_html;

// Semantic HTML-shaped projection for the official CommonMark conformance
// tests. This consumes Folia's parsed tree and lossless inline CST; it is not
// a Markdown parser or a serializer.

namespace folia::test::commonmark {

struct Projection {
    std::string strict_tree;
    std::string relaxed_tree;
    // HTML consumed by the shared Python HTMLParser canonicalizer. Markdown
    // text is escaped during projection; raw chunks come only from classified
    // HTML CST nodes or HTML-backed block sources.
    std::string projected_html;
    std::vector<std::string> limitations;
};

struct ProjectionOptions {
    bool table_prefer_style_attributes = false;
    bool full_info_string = false;
};

namespace projection_detail {

struct Node;
struct Child {
    std::string text;
    std::unique_ptr<Node> node;
    Child(std::string value) : text(std::move(value)) {}
    Child(Node value);
    ~Child();
    Child(Child&&) noexcept;
    Child& operator=(Child&&) noexcept;
    Child(Child const&) = delete;
    Child& operator=(Child const&) = delete;
};
struct Node {
    std::string tag;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::vector<Child> children;
    std::optional<std::string> html_override;
    std::string html_prefix;
    std::string html_suffix;
};
inline Child::Child(Node value) : node(std::make_unique<Node>(std::move(value))) {}
inline Child::~Child() = default;
inline Child::Child(Child&&) noexcept = default;
inline Child& Child::operator=(Child&&) noexcept = default;

inline std::string utf8(std::u32string_view value) { return cps_to_utf8(value); }

inline std::string json_string(std::string_view value) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.push_back('"');
    for (unsigned char ch : value) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch < 0x20) {
                    out += "\\u00";
                    out.push_back(hex[ch >> 4]);
                    out.push_back(hex[ch & 0xf]);
                } else {
                    out.push_back(static_cast<char>(ch));
                }
        }
    }
    out.push_back('"');
    return out;
}

inline void normalize_attrs(Node& node) {
    std::ranges::sort(node.attrs, {}, [](auto const& attr) -> auto const& { return attr.first; });
}

inline Node element(std::string tag, std::vector<std::pair<std::string, std::string>> attrs = {}) {
    Node out{std::move(tag), std::move(attrs), {}};
    normalize_attrs(out);
    return out;
}

inline void append_text(Node& node, std::string value) {
    if (value.empty()) return;
    if (!node.children.empty() && !node.children.back().node) {
        node.children.back().text += value;
    } else {
        node.children.emplace_back(std::move(value));
    }
}

inline void append_node(Node& parent, Node child) {
    parent.children.emplace_back(std::move(child));
}

inline Node raw_html_node(std::string value) {
    Node raw{"#raw-html", {}, {}};
    raw.children.emplace_back(std::move(value));
    return raw;
}

inline void append_raw_html(Node& parent, std::string value) {
    if (!value.empty()) append_node(parent, raw_html_node(std::move(value)));
}

inline Node clone(Node const& source) {
    Node out{source.tag, source.attrs, {}};
    for (auto const& child : source.children) {
        if (child.node) out.children.emplace_back(clone(*child.node));
        else out.children.emplace_back(child.text);
    }
    return out;
}

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

inline void limitation(std::vector<std::string>& out, std::string value) {
    if (std::ranges::find(out, value) == out.end()) out.push_back(std::move(value));
}

inline Node placeholder(std::string tag, std::string key, std::string value) {
    return element(std::move(tag), {{std::move(key), std::move(value)}});
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

inline void project_inline_nodes(
    Node& parent,
    InlineDocument const& doc,
    InlineCstNodes const& nodes,
    std::vector<std::string>& limitations) {
    using K = InlineCstKind;
    for (auto const& item : nodes) {
        auto raw = [&] { return utf8(inline_source_slice(doc, item.range)); };
        auto content = [&] { return utf8(inline_source_slice(doc, item.delimiter_ranges().content)); };
        switch (item.kind) {
            case K::Emphasis: {
                auto n = element("em");
                project_inline_nodes(n, doc, item.children, limitations);
                append_node(parent, std::move(n));
                break;
            }
            case K::Strong: {
                auto n = element("strong");
                project_inline_nodes(n, doc, item.children, limitations);
                append_node(parent, std::move(n));
                break;
            }
            case K::Strikethrough: {
                auto n = element("del");
                project_inline_nodes(n, doc, item.children, limitations);
                append_node(parent, std::move(n));
                break;
            }
            case K::CodeSpan: {
                auto n = element("code");
                append_text(n, cps_to_utf8(normalize_inline_code_span(
                    inline_source_slice(doc, item.delimiter_ranges().content), doc.syntax_mode)));
                append_node(parent, std::move(n));
                break;
            }
            case K::Link: {
                auto attrs = std::vector<std::pair<std::string, std::string>>{{"href", item.semantics().href}};
                if (item.semantics().title) attrs.emplace_back("title", *item.semantics().title);
                auto n = element("a", std::move(attrs));
                project_inline_nodes(n, doc, item.children, limitations);
                append_node(parent, std::move(n));
                break;
            }
            case K::Image: {
                auto attrs = std::vector<std::pair<std::string, std::string>>{
                    {"alt", item.semantics().alt}, {"src", item.semantics().href}};
                if (item.semantics().title) attrs.emplace_back("title", *item.semantics().title);
                append_node(parent, element("img", std::move(attrs)));
                break;
            }
            case K::Entity:
                append_text(parent, utf8(decode_inline_entity(inline_source_slice(doc, item.range))));
                break;
            case K::Escape: {
                auto source = inline_source_slice(doc, item.range);
                append_text(parent, source.empty() ? std::string{} : utf8(std::u32string_view(source).substr(source.size() - 1)));
                break;
            }
            case K::SoftBreak:
                if (item.range.end == doc.source.size()) break;
                append_text(parent, "\n");
                break;
            case K::HardBreak:
                if (item.range.end == doc.source.size()) break;
                append_node(parent, element("br"));
                append_text(parent, "\n");
                break;
            case K::Autolink:
                // The CST exposes content and the source range; use semantic href
                // where available and keep the visible spelling from source.
                append_node(parent, element("a", {{"href", item.semantics().href}}));
                if (!parent.children.empty() && parent.children.back().node) {
                    append_text(*parent.children.back().node, content());
                }
                break;
            case K::HtmlElement: {
                const auto& semantic = item.semantics();
                if (item.status != ParseStatus::Complete || semantic.html_tag.empty()) {
                    append_node(parent, placeholder("html-policy/projection", "source", raw()));
                    limitation(limitations, "inline-html-policy/projection");
                    break;
                }
                std::vector<std::pair<std::string, std::string>> attrs;
                attrs.reserve(semantic.html_attributes.size());
                for (const auto& [name, value] : semantic.html_attributes) {
                    attrs.emplace_back(name, value);
                }
                auto n = element(semantic.html_tag, std::move(attrs));
                const auto& ranges = item.delimiter_ranges();
                if (ranges.opening.valid_for(doc.source.size())) {
                    n.html_prefix = utf8(inline_source_slice(doc, ranges.opening));
                }
                if (semantic.html_tag != "img" && semantic.html_tag != "br") {
                    project_inline_nodes(n, doc, item.children, limitations);
                }
                if (ranges.closing && ranges.closing->valid_for(doc.source.size())) {
                    n.html_suffix = utf8(inline_source_slice(doc, *ranges.closing));
                }
                append_node(parent, std::move(n));
                break;
            }
            case K::HtmlComment: {
                const auto& content_range = item.delimiter_ranges().content;
                if (item.status != ParseStatus::Complete || !content_range.valid_for(doc.source.size())) {
                    append_node(parent, placeholder("html-policy/projection", "source", raw()));
                    limitation(limitations, "inline-html-policy/projection");
                    break;
                }
                auto comment = element("comment", {{"text", utf8(inline_source_slice(doc, content_range))}});
                comment.html_override = raw();
                append_node(parent, std::move(comment));
                break;
            }
            case K::Raw: {
                const auto tag = scan_markdown_html_tag(doc.source, item.range.start, item.range.end);
                const auto special = scan_markdown_html_special(doc.source, item.range.start, item.range.end);
                if ((tag && tag->range.end == item.range.end)
                    || (special && special->range.end == item.range.end)) {
                    append_raw_html(parent, raw());
                } else append_text(parent, raw());
                break;
            }
            case K::Incomplete: {
                const auto& semantic = item.semantics();
                const auto& ranges = item.delimiter_ranges();
                if (!semantic.html_tag.empty() && ranges.opening.valid_for(doc.source.size())) {
                    auto n = element(semantic.html_tag);
                    n.html_prefix = utf8(inline_source_slice(doc, ranges.opening));
                    project_inline_nodes(n, doc, item.children, limitations);
                    append_node(parent, std::move(n));
                } else append_text(parent, raw());
                break;
            }
            case K::Error:
                // Preserve the exact incomplete/error spelling as literal text.
                append_text(parent, raw());
                break;
            case K::FootnoteRef: {
                Node ref{"#footnote-ref", {{"label", item.semantics().label}, {"source", raw()}}, {}};
                append_node(parent, std::move(ref));
                break;
            }
            case K::InlineMath:
            case K::WikiLink:
            case K::Extension:
                append_node(parent, placeholder("extension", "kind", std::to_string(static_cast<int>(item.kind))));
                limitation(limitations, "inline-extension:" + std::to_string(static_cast<int>(item.kind)));
                break;
            case K::Text:
            case K::Whitespace:
            case K::Delimiter:
                append_text(parent, raw());
                break;
        }
    }
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

inline InlineCstNodes semantic_inline_nodes(
    InlineDocument const& document,
    InlineBlockWhitespacePolicy policy) {
    auto nodes = document.tree.nodes;
    const auto hidden = inline_block_semantic_whitespace_ranges(document, policy);
    for (const auto& range : hidden) {
        for (auto& node : nodes) {
            if (node.children.empty()
                && (node.kind == InlineCstKind::Text || node.kind == InlineCstKind::Whitespace)
                && range.start >= node.range.start && range.end <= node.range.end) {
                if (range.start == node.range.start) node.range.start = range.end;
                else if (range.end == node.range.end) node.range.end = range.start;
            }
        }
    }
    return nodes;
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
            if (block.inline_content.source.empty()
                || (block.inline_content.syntax_mode == InlineSyntaxMode::Markdown
                    && std::ranges::all_of(block.inline_content.source, [](char32_t ch) {
                        return ch == U' ' || ch == U'\t';
                    }))) return false;
            if (block.inline_content.syntax_mode == InlineSyntaxMode::HtmlText) {
                append_raw_html(parent, utf8(block.inline_content.source) + "\n");
                return true;
            }
            auto n = element("p");
            auto semantic_nodes = semantic_inline_nodes(
                block.inline_content,
                InlineBlockWhitespacePolicy::Paragraph);
            project_inline_nodes(n, block.inline_content, semantic_nodes, limitations);
            append_node(parent, std::move(n));
            return true;
        }
        case BlockKind::Heading: {
            auto const level = static_cast<int>(block.text_special().level);
            auto n = element("h" + std::to_string(level));
            auto semantic_nodes = semantic_inline_nodes(
                block.inline_content,
                InlineBlockWhitespacePolicy::Heading);
            project_inline_nodes(n, block.inline_content, semantic_nodes, limitations);
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
                        if (!child.inline_content.source.empty()) {
                            if (checkbox_pending) append_checkbox(li);
                            if (child.inline_content.syntax_mode == InlineSyntaxMode::HtmlText) {
                                append_raw_html(li, utf8(child.inline_content.source) + "\n");
                            } else {
                                auto semantic_nodes = semantic_inline_nodes(
                                    child.inline_content,
                                    InlineBlockWhitespacePolicy::Paragraph);
                                project_inline_nodes(
                                    li,
                                    child.inline_content,
                                    semantic_nodes,
                                    limitations);
                            }
                        }
                    } else if (!list.tight && child.kind == BlockKind::Paragraph && checkbox_pending) {
                        auto p = element("p");
                        append_checkbox(p);
                        auto semantic_nodes = semantic_inline_nodes(
                            child.inline_content, InlineBlockWhitespacePolicy::Paragraph);
                        project_inline_nodes(p, child.inline_content, semantic_nodes, limitations);
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
                            cell.inline_content, InlineBlockWhitespacePolicy::Paragraph);
                        project_inline_nodes(th, cell.inline_content, semantic_nodes, limitations);
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

inline bool is_container(std::string_view tag) {
    return tag == "root" || tag == "blockquote" || tag == "ul" || tag == "ol" || tag == "li";
}
inline bool is_block(std::string_view tag) {
    return is_container(tag) || tag == "p" || tag == "pre" || tag == "hr"
        || tag == "section"
        || (tag.size() == 2 && tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6');
}
inline bool whitespace_only(std::string const& text) {
    // Match the explicit ASCII whitespace set used by the HTML canonicalizer
    // contract, including vertical tab; UTF-8 non-ASCII bytes stay significant.
    return std::ranges::all_of(text, [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
    });
}

inline void clean(Node& node, bool relax) {
    std::vector<Child> cleaned;
    for (auto& child : node.children) {
        if (!child.node) {
            if (is_container(node.tag) && whitespace_only(child.text)) continue;
            if (!cleaned.empty() && !cleaned.back().node) cleaned.back().text += child.text;
            else cleaned.emplace_back(std::move(child.text));
            continue;
        }
        clean(*child.node, relax);
        if (relax && node.tag == "li" && child.node->tag == "p") {
            for (auto& grandchild : child.node->children) {
                if (!grandchild.node) {
                    if (!cleaned.empty() && !cleaned.back().node) cleaned.back().text += grandchild.text;
                    else cleaned.emplace_back(std::move(grandchild.text));
                } else cleaned.emplace_back(std::move(*grandchild.node));
            }
        } else {
            cleaned.emplace_back(std::move(*child.node));
        }
    }
    node.children = std::move(cleaned);
    if (!is_container(node.tag)) return;
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        auto& child = node.children[i];
        if (child.node) continue;
        auto value = child.text;
        bool previous_block = i == 0;
        bool next_block = i + 1 == node.children.size();
        if (i > 0 && node.children[i - 1].node) previous_block = is_block(node.children[i - 1].node->tag);
        if (i + 1 < node.children.size() && node.children[i + 1].node) next_block = is_block(node.children[i + 1].node->tag);
        if (previous_block) while (!value.empty() && value.front() == '\n') value.erase(value.begin());
        if (next_block) while (!value.empty() && value.back() == '\n') value.pop_back();
        child.text = std::move(value);
    }
    std::erase_if(node.children, [](Child const& child) { return !child.node && child.text.empty(); });
}

inline void encode(Node const& node, std::string& out) {
    out.push_back('[');
    out += json_string(node.tag);
    out += ",[";
    for (std::size_t i = 0; i < node.attrs.size(); ++i) {
        if (i) out.push_back(',');
        out.push_back('[');
        out += json_string(node.attrs[i].first);
        out.push_back(',');
        out += json_string(node.attrs[i].second);
        out.push_back(']');
    }
    out += "],[";
    for (std::size_t i = 0; i < node.children.size(); ++i) {
        if (i) out.push_back(',');
        auto const& child = node.children[i];
        if (child.node) encode(*child.node, out);
        else out += json_string(child.text);
    }
    out += "]]";
}

inline bool inline_lossless_nodes(InlineCstNodes const& nodes, InlineDocument const& doc) {
    for (auto const& node : nodes) {
        if (!node.range.valid_for(doc.source.size()) || node.range.empty()) return false;
        if (!inline_lossless_nodes(node.children, doc)) return false;
    }
    return true;
}

inline void append_html_raw_display(std::string& out, std::string_view value) {
    for (const char ch : value) {
        if (ch == '\0') out += "\xef\xbf\xbd";
        else out.push_back(ch);
    }
}

inline void append_html_text(std::string& out, std::string_view value) {
    for (const char ch : value) {
        switch (ch) {
            case '\0': out += "\xef\xbf\xbd"; break;
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out.push_back(ch); break;
        }
    }
}

inline void append_html_attribute(std::string& out, std::string_view value) {
    for (const char ch : value) {
        switch (ch) {
            case '\0': out += "\xef\xbf\xbd"; break;
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '\"': out += "&quot;"; break;
            default: out.push_back(ch); break;
        }
    }
}

inline void html_start_tag(
    std::string& out,
    std::string_view tag,
    std::vector<std::pair<std::string, std::string>> attrs = {}) {
    std::ranges::sort(attrs, {}, [](auto const& attr) -> auto const& { return attr.first; });
    out.push_back('<');
    out.append(tag);
    for (const auto& [name, value] : attrs) {
        out.push_back(' ');
        out += name;
        out += "=\"";
        append_html_attribute(out, value);
        out.push_back('\"');
    }
    out.push_back('>');
}

inline bool html_void_element(std::string_view tag) {
    return tag == "area" || tag == "base" || tag == "br" || tag == "col"
        || tag == "embed" || tag == "hr" || tag == "img" || tag == "input"
        || tag == "link" || tag == "meta" || tag == "param" || tag == "source"
        || tag == "track" || tag == "wbr";
}

inline void render_html_node(Node const& node, std::string& out) {
    if (node.tag == "#raw-html") {
        for (const auto& child : node.children) {
            if (child.node) render_html_node(*child.node, out);
            else append_html_raw_display(out, child.text);
        }
        return;
    }
    if (node.tag == "root") {
        for (const auto& child : node.children) {
            if (child.node) render_html_node(*child.node, out);
            else append_html_text(out, child.text);
        }
        return;
    }
    if (node.html_override) {
        append_html_raw_display(out, *node.html_override);
        return;
    }
    if (!node.html_prefix.empty()) append_html_raw_display(out, node.html_prefix);
    else html_start_tag(out, node.tag, node.attrs);
    const bool table_structure = node.tag == "table" || node.tag == "thead"
        || node.tag == "tbody" || node.tag == "tr";
    if (table_structure) out.push_back('\n');
    for (const auto& child : node.children) {
        if (child.node) render_html_node(*child.node, out);
        else append_html_text(out, child.text);
    }
    if (!node.html_suffix.empty()) append_html_raw_display(out, node.html_suffix);
    else if (node.html_prefix.empty() && !html_void_element(node.tag)) out += "</" + node.tag + ">";
    // These line endings belong to generated Markdown block markup. They
    // matter inside an open raw HTML element and even within an HTML comment.
    if (is_block(node.tag) || table_structure || node.tag == "td" || node.tag == "th") {
        out.push_back('\n');
    }
}

inline std::string render_html(Node const& root) {
    std::string result;
    render_html_node(root, result);
    return result;
}

inline std::string footnote_id(std::string_view label) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : label) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~'
            || c == '/' || c == '(' || c == ')') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%'); out.push_back(hex[c >> 4]); out.push_back(hex[c & 15]);
        }
    }
    return out;
}

inline std::string const& attr(Node const& node, std::string_view name) {
    static const std::string empty;
    auto found = std::ranges::find_if(node.attrs, [&](auto const& value) { return value.first == name; });
    return found == node.attrs.end() ? empty : found->second;
}

struct FootnoteState {
    std::unordered_map<std::string, Node> definitions;
    std::unordered_map<std::string, std::size_t> counts;
    std::unordered_map<std::string, std::size_t> ordinals;
    std::vector<std::string> used;
    std::unordered_set<std::string> expanded;
    std::size_t next_ordinal = 1;
};

inline Node footnote_ref(std::string const& label, FootnoteState& state) {
    const auto ordinal = state.ordinals.try_emplace(label, state.next_ordinal).first->second;
    if (ordinal == state.next_ordinal) ++state.next_ordinal;
    const auto occurrence = ++state.counts[label];
    const auto safe = footnote_id(label);
    auto id = "fnref-" + safe + (occurrence == 1 ? "" : "-" + std::to_string(occurrence));
    auto anchor = element("a", {{"data-footnote-ref", ""}, {"href", "#fn-" + safe}, {"id", id}});
    append_text(anchor, std::to_string(ordinal));
    auto sup = element("sup", {{"class", "footnote-ref"}});
    append_node(sup, std::move(anchor));
    if (state.expanded.insert(label).second) {
        state.used.push_back(label);
    }
    return sup;
}

inline void append_backrefs(Node& definition, std::string const& label, FootnoteState const& state) {
    const auto count = state.counts.at(label);
    auto make_link = [&](std::size_t occurrence) {
        auto safe = footnote_id(label);
        auto id = "fnref-" + safe + (occurrence == 1 ? "" : "-" + std::to_string(occurrence));
        auto index = std::to_string(state.ordinals.at(label))
            + (occurrence == 1 ? "" : "-" + std::to_string(occurrence));
        auto link = element("a", {{"aria-label", "Back to reference " + index},
            {"class", "footnote-backref"}, {"data-footnote-backref", ""},
            {"data-footnote-backref-idx", index}, {"href", "#" + id}});
        append_text(link, "↩");
        if (occurrence > 1) {
            auto sup = element("sup", {{"class", "footnote-ref"}});
            append_text(sup, std::to_string(occurrence));
            append_node(link, std::move(sup));
        }
        return link;
    };
    Node* last_paragraph = nullptr;
    auto find_last = [&](auto&& self, Node& node) -> void {
        for (auto& child : node.children) if (child.node) {
            self(self, *child.node);
            if (child.node->tag == "p") last_paragraph = child.node.get();
        }
    };
    find_last(find_last, definition);
    Node* target = last_paragraph ? last_paragraph : &definition;
    for (std::size_t i = 1; i <= count; ++i) {
        if (!target->children.empty()) append_text(*target, " ");
        append_node(*target, make_link(i));
    }
}

inline void finish_footnotes(Node& root) {
    FootnoteState state;
    auto extract = [&](auto&& self, Node& node) -> void {
        std::vector<Child> body;
        for (auto& child : node.children) {
            if (child.node && child.node->tag == "#footnote-definition") {
                auto label = attr(*child.node, "label");
                state.definitions.emplace(std::move(label), std::move(*child.node));
            } else {
                if (child.node) self(self, *child.node);
                body.emplace_back(std::move(child));
            }
        }
        node.children = std::move(body);
    };
    extract(extract, root);
    auto visit = [&](auto&& self, Node& node) -> void {
        std::vector<Child> children;
        for (auto& child : node.children) {
            if (child.node && child.node->tag == "#footnote-ref") {
                if (state.definitions.contains(attr(*child.node, "label")))
                    children.emplace_back(footnote_ref(attr(*child.node, "label"), state));
                else children.emplace_back(attr(*child.node, "source"));
            } else {
                if (child.node) self(self, *child.node);
                children.emplace_back(std::move(child));
            }
        }
        node.children = std::move(children);
    };
    visit(visit, root);
    for (std::size_t index = 0; index < state.used.size(); ++index) {
        auto& definition = state.definitions.at(state.used[index]);
        visit(visit, definition);
    }
    if (state.used.empty()) return;
    auto section = element("section", {{"class", "footnotes"}, {"data-footnotes", ""}});
    auto ol = element("ol");
    for (auto const& label : state.used) {
        auto li = element("li", {{"id", "fn-" + footnote_id(label)}});
        auto& definition = state.definitions.at(label);
        for (auto& child : definition.children) {
            if (child.node) append_node(li, std::move(*child.node));
            else append_text(li, std::move(child.text));
        }
        append_backrefs(li, label, state);
        append_node(ol, std::move(li));
    }
    append_text(section, "\n");
    append_node(section, std::move(ol));
    append_node(root, std::move(section));
}
} // namespace projection_detail

inline std::string json_string(std::string_view value) {
    return projection_detail::json_string(value);
}

inline bool inline_document_lossless(BlockNode const& block) {
    auto const& doc = block.inline_content;
    if (!(tokens_partition_source(doc.tree, doc.source.size())
        && roots_partition_source(doc.tree, doc.source.size())
        && flatten_tokens(doc.tree, doc.source) == doc.source
        && serialize_lossless(doc.tree, doc.source) == doc.source
        && projection_detail::inline_lossless_nodes(doc.tree.nodes, doc))) return false;
    for (auto const& child : block.children) {
        if (!inline_document_lossless(child)) return false;
    }
    return true;
}

inline bool inline_lossless(BlockNode const& block) {
    return inline_document_lossless(block);
}

inline MarkdownDialect dialect() {
    MarkdownDialect d{};
    d.commonmark = true;
    d.gfm = {false, false, false, false};
    d.math = {false, false, false, false, false, MathRenderBackend::PlainTextFallback};
    d.toc = {false, false, false};
    d.frontmatter = {false, false, false};
    d.footnotes = false;
    d.definition_lists = false;
    d.callouts = false;
    d.wiki_links = false;
    d.tables = {false, false};
    d.diagrams = {false, false, false};
    return d;
}

inline MarkdownDialect gfm_dialect() {
    auto d = dialect();
    d.gfm = {true, true, true, true};
    d.tables = {true, true};
    return d;
}

inline Projection project(BlockNode const& block, ProjectionOptions const& options = {}) {
    using namespace projection_detail;
    Node strict = element("root");
    Projection result;
    if (block.kind == BlockKind::Document) {
        for (auto const& child : block.children) project_block(strict, child, result.limitations, options);
    } else {
        project_block(strict, block, result.limitations, options);
    }
    finish_footnotes(strict);
    result.projected_html = render_html(strict);
    Node relaxed = clone(strict);
    clean(strict, false);
    clean(relaxed, true);
    encode(strict, result.strict_tree);
    encode(relaxed, result.relaxed_tree);
    std::ranges::sort(result.limitations);
    return result;
}

} // namespace folia::test::commonmark
