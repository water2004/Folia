module folia.mermaid.tree_view;
import std;
import folia.mermaid.config_value;

namespace folia::mermaid {
namespace {
const ConfigValue* tree_option(const ConfigValue& config, std::u16string_view key) {
    const auto* tree = config.find(u"treeView");
    return tree ? tree->find(key) : nullptr;
}

double numeric_option(const ConfigValue& config, std::u16string_view key, double fallback) {
    const auto* value = tree_option(config, key);
    return value && value->kind == ConfigKind::number ? value->number : fallback;
}

bool boolean_option(const ConfigValue& config, std::u16string_view key, bool fallback) {
    const auto* value = tree_option(config, key);
    return value && value->kind == ConfigKind::boolean ? value->boolean : fallback;
}

std::string string_option(const ConfigValue& config, std::u16string_view key,
    std::string fallback) {
    const auto* value = tree_option(config, key);
    if (!value || value->kind != ConfigKind::string) return fallback;
    std::string result;
    for (char16_t character : value->string) result.push_back(static_cast<char>(character));
    return result;
}

std::u16string trim(std::u16string_view text) {
    const auto first = text.find_first_not_of(u" \t\r");
    if (first == text.npos) return {};
    const auto last = text.find_last_not_of(u" \t\r");
    return std::u16string{text.substr(first, last - first + 1)};
}

bool starts_with(std::u16string_view text, std::u16string_view prefix) {
    return text.starts_with(prefix);
}

bool is_box_character(char16_t value) {
    return value == u'─' || value == u'━' || value == u'│' || value == u'┃' ||
        value == u'└' || value == u'┗' || value == u'├' || value == u'┣';
}

bool is_branch(char16_t value) {
    return value == u'└' || value == u'┗' || value == u'├' || value == u'┣';
}

bool is_dash(char16_t value) { return value == u'─' || value == u'━'; }

bool decoration_only(std::u16string_view line) {
    return std::all_of(line.begin(), line.end(), [](char16_t value) {
        return value == u' ' || value == u'\t' || value == u'│' || value == u'┃' ||
            value == u'─' || value == u'━' || value == u'└' || value == u'┗' ||
            value == u'├' || value == u'┣';
    });
}

std::vector<std::u16string> split_lines(std::u16string_view source) {
    std::vector<std::u16string> lines;
    for (std::size_t begin = 0; begin <= source.size();) {
        auto end = source.find(u'\n', begin);
        if (end == source.npos) end = source.size();
        std::u16string line{source.substr(begin, end - begin)};
        if (!line.empty() && line.back() == u'\r') line.pop_back();
        lines.push_back(std::move(line));
        if (end == source.size()) break;
        begin = end + 1;
    }
    return lines;
}

bool metadata_line(std::u16string_view line) {
    const auto value = trim(line);
    const auto is_keyword = [&value](std::u16string_view keyword) {
        return starts_with(value, keyword) &&
            (value.size() == keyword.size() || value[keyword.size()] == u' ' ||
                value[keyword.size()] == u'\t' || value[keyword.size()] == u':' ||
                value[keyword.size()] == u'{');
    };
    return is_keyword(u"title") || is_keyword(u"accTitle") || is_keyword(u"accDescr");
}

std::size_t infer_segment_width(const std::vector<std::u16string>& lines, std::size_t keyword) {
    for (std::size_t line = keyword + 1; line < lines.size(); ++line) {
        for (std::size_t column = 1; column < lines[line].size(); ++column) {
            if (is_branch(lines[line][column])) return column;
        }
    }
    return 4;
}

std::u16string unquote(std::u16string& content, std::size_t line_number) {
    if (content.empty() || (content.front() != u'"' && content.front() != u'\'')) return {};
    const char16_t quote = content.front();
    const auto close = content.find(quote, 1);
    if (close == content.npos) {
        throw std::runtime_error("syntax error: unterminated quoted name at line " +
            std::to_string(line_number));
    }
    auto name = content.substr(1, close - 1);
    content = trim(std::u16string_view(content).substr(close + 1));
    return name;
}

void parse_annotations(TreeViewNode& node, std::u16string content) {
    while (!content.empty()) {
        content = trim(content);
        if (starts_with(content, u":::")) {
            const auto end = content.find_first_of(u" \t");
            node.css_class = content.substr(3,
                end == content.npos ? content.size() - 3 : end - 3);
            content = end == content.npos ? std::u16string{} : content.substr(end);
        } else if (starts_with(content, u"icon(")) {
            const auto end = content.find(u')');
            if (end == content.npos) throw std::runtime_error("syntax error: unterminated icon annotation");
            node.icon = content.substr(5, end - 5);
            if (node.icon.empty()) node.icon = u"none";
            content = content.substr(end + 1);
        } else if (starts_with(content, u"##")) {
            node.description = trim(std::u16string_view(content).substr(2));
            break;
        } else {
            break;
        }
    }
}

std::u16string take_name(std::u16string& content) {
    auto end = content.size();
    for (const auto marker : {std::u16string_view(u" :::"), std::u16string_view(u" icon("),
            std::u16string_view(u" ##")}) {
        const auto position = content.find(marker);
        if (position != content.npos) end = std::min(end, position);
    }
    auto name = trim(std::u16string_view(content).substr(0, end));
    content = std::u16string(std::u16string_view(content).substr(end));
    return name;
}

std::pair<std::size_t, std::u16string> box_drawing_node(
    std::u16string_view source, std::size_t segment_width) {
    std::u16string normalized;
    for (char16_t character : source) {
        if (character == u'\t') normalized.append(4, u' ');
        else normalized.push_back(character);
    }

    std::size_t branch_position = normalized.find_first_of(u"└┗├┣");
    if (branch_position == normalized.npos) return {0, trim(normalized)};
    const std::size_t level = branch_position / segment_width + 1;
    std::size_t content_position = branch_position + 1;
    while (content_position < normalized.size() && is_dash(normalized[content_position])) {
        ++content_position;
    }
    while (content_position < normalized.size() && normalized[content_position] == u' ') {
        ++content_position;
    }
    return {level, trim(std::u16string_view(normalized).substr(content_position))};
}

std::pair<std::size_t, std::u16string> indented_node(std::u16string_view source) {
    std::size_t position = 0;
    std::size_t indent = 0;
    while (position < source.size() && (source[position] == u' ' || source[position] == u'\t')) {
        indent += source[position] == u'\t' ? 4 : 1;
        ++position;
    }
    return {indent, trim(source.substr(position))};
}

} // namespace

std::expected<TreeViewDocument, std::string> parse_tree_view(std::u16string_view source) {
    try {
        auto lines = split_lines(source);
        std::size_t keyword = lines.size();
        for (std::size_t index = 0; index < lines.size(); ++index) {
            if (trim(lines[index]) == u"treeView-beta") {
                keyword = index;
                break;
            }
        }
        if (keyword == lines.size()) return std::unexpected("syntax error: expected treeView-beta");

        bool box_drawing = false;
        for (std::size_t index = keyword + 1; index < lines.size(); ++index) {
            const auto value = trim(lines[index]);
            if (value.empty() || starts_with(value, u"%%") || metadata_line(value) || decoration_only(value)) {
                continue;
            }
            box_drawing = box_drawing || std::any_of(value.begin(), value.end(), is_box_character);
        }
        const auto segment_width = infer_segment_width(lines, keyword);

        TreeViewDocument document;
        TreeViewNode root;
        root.id = 0;
        root.name = u"/";
        root.type = TreeViewNodeType::directory;
        document.nodes.push_back(std::move(root));
        std::vector<std::size_t> parent_indents{std::numeric_limits<std::size_t>::max()};

        for (std::size_t index = keyword + 1; index < lines.size(); ++index) {
            const auto value = trim(lines[index]);
            if (value.empty() || starts_with(value, u"%%")) continue;
            if (starts_with(value, u"title") &&
                (value.size() == 5 || value[5] == u' ' || value[5] == u'\t')) {
                document.title = trim(std::u16string_view(value).substr(5));
                continue;
            }
            if (starts_with(value, u"accTitle")) {
                const auto colon = value.find(u':');
                if (colon != value.npos) document.acc_title = trim(std::u16string_view(value).substr(colon + 1));
                continue;
            }
            if (starts_with(value, u"accDescr")) {
                const auto colon = value.find(u':');
                if (colon != value.npos) {
                    document.acc_description = trim(std::u16string_view(value).substr(colon + 1));
                } else {
                    const auto open = value.find(u'{');
                    const auto close = value.find_last_of(u'}');
                    if (open != value.npos && close > open) {
                        document.acc_description = trim(
                            std::u16string_view(value).substr(open + 1, close - open - 1));
                    }
                }
                continue;
            }

            auto [indent, content] = box_drawing
                ? box_drawing_node(lines[index], segment_width)
                : indented_node(lines[index]);
            if (box_drawing && decoration_only(lines[index])) continue;
            if (content.empty()) {
                return std::unexpected("syntax error: empty tree node at line " + std::to_string(index + 1));
            }

            std::u16string name = unquote(content, index + 1);
            if (name.empty()) name = take_name(content);
            if (name.empty()) return std::unexpected("syntax error: empty tree node name");

            TreeViewNode node;
            node.id = document.nodes.size();
            node.name = std::move(name);
            if (node.name.ends_with(u"/")) {
                node.type = TreeViewNodeType::directory;
                node.name.pop_back();
            }
            parse_annotations(node, std::move(content));

            while (parent_indents.size() > 1 && indent <= parent_indents.back()) {
                parent_indents.pop_back();
            }
            node.level = parent_indents.size();
            parent_indents.push_back(indent);
            document.nodes.push_back(std::move(node));
        }
        return document;
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}

std::expected<TreeViewRenderInput, std::string> prepare_tree_view_render(
    const PreparedDiagram& prepared) {
    auto document = parse_tree_view(prepared.code.with_comments);
    if (!document) return std::unexpected(document.error());

    TreeViewRenderInput result;
    result.document = std::move(*document);
    const auto& config = prepared.effective_config;
    if (const auto* family = config.find(u"fontFamily");
        family && family->kind == ConfigKind::string) {
        result.theme.font_family = family->string;
    }
    result.config.row_indent = numeric_option(config, u"rowIndent", 10);
    result.config.padding_x = numeric_option(config, u"paddingX", 5);
    result.config.padding_y = numeric_option(config, u"paddingY", 5);
    result.config.line_thickness = numeric_option(config, u"lineThickness", 1);
    result.config.show_icons = boolean_option(config, u"showIcons", false);
    result.config.use_max_width = boolean_option(config, u"useMaxWidth", true);
    result.theme.label_color = string_option(config, u"labelColor", "black");
    result.theme.line_color = string_option(config, u"lineColor", "black");
    result.theme.icon_color = string_option(config, u"iconColor", "#546e7a");
    result.theme.description_color = string_option(config, u"descriptionColor", "#6a9955");
    result.theme.highlight_bg = string_option(config, u"highlightBg", "rgba(255, 193, 7, 0.15)");
    result.theme.highlight_stroke = string_option(config, u"highlightStroke", "#ffc107");
    return result;
}
} // namespace folia::mermaid
