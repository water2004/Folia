module folia.mermaid.tree_view_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;

namespace folia::mermaid {
namespace {
std::string number(double value) {
    auto formatted = format_number(value);
    if (!formatted) throw std::runtime_error("Invalid treeView geometry");
    return *formatted;
}

std::string escape(std::u16string_view text) {
    auto escaped = escape_svg_text(text, u"treeView text");
    if (!escaped) throw std::runtime_error("Invalid treeView SVG text");
    return *escaped;
}

std::string attribute(std::string_view name, double value) {
    return " " + std::string(name) + "=\"" + number(value) + "\"";
}

std::string text_element(double x, double y, std::u16string_view value,
    std::string_view css_class, std::string_view color, unsigned weight, bool italic = false) {
    return "<text" + attribute("x", x) + attribute("y", y) +
        " dominant-baseline=\"middle\" xml:space=\"preserve\" class=\"" + std::string(css_class) +
        "\" fill=\"" + std::string(color) + "\" font-weight=\"" +
        std::to_string(weight) + "\"" + (italic ? " font-style=\"italic\"" : "") +
        ">" + escape(value) + "</text>";
}

struct Row {
    const TreeViewNode* node{};
    double x{}, y{}, width{}, height{}, label_right{}, center_y{};
    std::string icon_kind;
    double highlight_width{};
    bool highlighted{};
};

std::string icon_svg(std::string_view kind, double x, double y, std::string_view color) {
    constexpr std::string_view folder =
        "<path fill=\"currentColor\" d=\"M10.59 4.59A2 2 0 0 0 9.17 4H4a2 2 0 0 0-2 2v12a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.17z\"/>";
    constexpr std::string_view file =
        "<path fill=\"currentColor\" fill-rule=\"evenodd\" d=\"M6 2a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8.83a2 2 0 0 0-.59-1.42l-4.82-4.82A2 2 0 0 0 13.17 2H6Zm7.5 1.9l4.6 4.6h-3.6a1 1 0 0 1-1-1V3.9Z\" clip-rule=\"evenodd\"/>";
    auto const body = kind == "folder" ? folder : file;
    return "<svg class=\"treeView-node-icon\" x=\"" + number(x) + "\" y=\"" +
        number(y) + "\" width=\"14\" height=\"14\" viewBox=\"0 0 24 24\" color=\"" +
        std::string(color) + "\">" + std::string(body) + "</svg>";
}

std::string node_class(const TreeViewNode& node) {
    std::string result = "treeView-node-label";
    if (node.type == TreeViewNodeType::directory) result += " treeView-node-dir";
    if (!node.css_class.empty()) result += " " + std::string(node.css_class.begin(), node.css_class.end());
    return result;
}

} // namespace

std::expected<TreeViewSvg, std::string> render_tree_view_svg(
    const TreeViewRenderInput& input, const SvgTextMeasure& measure) {
    if (!measure) return std::unexpected("treeView requires real text measurements");
    try {
        const auto& config = input.config;
        const auto& theme = input.theme;
        std::vector<Row> rows;
        double total_height = 0;
        double total_width = 0;
        double max_label_right = 0;

        for (const auto& node : input.document.nodes) {
            const unsigned weight = node.type == TreeViewNodeType::directory ? 700 : 400;
            auto metrics = measure(node.name, 16, weight);
            if (!metrics) return std::unexpected(metrics.error());

            std::string icon_kind;
            if (node.icon.empty() && config.show_icons) {
                icon_kind = node.type == TreeViewNodeType::directory ? "folder" : "file";
            } else if (node.icon == u"folder" || node.icon == u"mermaid-treeview:folder") {
                icon_kind = "folder";
            } else if (node.icon == u"file" || node.icon == u"mermaid-treeview:file") {
                icon_kind = "file";
            }
            const bool show_icon = !icon_kind.empty();
            const double x = static_cast<double>(node.level) *
                (config.row_indent + config.padding_x);
            const double height = metrics->height + config.padding_y * 2;
            const double width = metrics->width + config.padding_x * 2 + (show_icon ? 18 : 0);
            const double center_y = total_height + height / 2;
            rows.push_back({
                .node = &node, .x = x, .y = total_height, .width = width, .height = height,
                .label_right = x + config.padding_x + (show_icon ? 18 : 0) + metrics->width,
                .center_y = center_y, .icon_kind = std::move(icon_kind),
                .highlighted = node.css_class.find(u"highlight") != std::u16string::npos});
            max_label_right = std::max(max_label_right, rows.back().label_right);
            total_width = std::max(total_width, x + width);
            total_height += height;
        }

        const bool has_descriptions = std::any_of(rows.begin(), rows.end(),
            [](const Row& row) { return !row.node->description.empty(); });
        const double description_x = max_label_right + 16;
        if (has_descriptions) {
            for (const auto& row : rows) {
                if (row.node->description.empty()) continue;
                auto metrics = measure(row.node->description, 16, 400);
                if (!metrics) return std::unexpected(metrics.error());
                total_width = std::max(total_width,
                    description_x + metrics->width + config.padding_x);
            }
        }

        for (auto& row : rows) {
            if (!row.highlighted) continue;
            row.highlight_width = total_width - row.x + 8;
            total_width = std::max(total_width, row.x + row.highlight_width + 2);
        }

        std::string body;
        for (const auto& row : rows) {
            const auto& node = *row.node;
            body += "<line" + attribute("x1", row.x - config.row_indent) +
                attribute("y1", row.center_y) + attribute("x2", row.x) +
                attribute("y2", row.center_y) + attribute("stroke-width", config.line_thickness) +
                " class=\"treeView-node-line\" stroke=\"" + theme.line_color + "\"/>";

            if (row.highlighted) {
                body += "<rect" + attribute("x", row.x) + attribute("y", row.y + 1) +
                    attribute("width", std::max(0.0, row.highlight_width)) +
                    attribute("height", row.height - 2) + " rx=\"3\" fill=\"" +
                    theme.highlight_bg + "\" stroke=\"" + theme.highlight_stroke +
                    "\" class=\"treeView-highlight-bg\"/>";
            }

            if (!row.icon_kind.empty()) {
                body += icon_svg(row.icon_kind, row.x + config.padding_x,
                    row.y + config.padding_y, theme.icon_color);
            }

            const double label_x = row.x + config.padding_x + (row.icon_kind.empty() ? 0 : 18);
            const unsigned weight = node.type == TreeViewNodeType::directory ? 700 : 400;
            body += text_element(label_x, row.center_y, node.name,
                node_class(node), theme.label_color, weight);

            if (!node.description.empty()) {
                body += text_element(description_x, row.center_y, node.description,
                    "treeView-node-description", theme.description_color, 400, true);
            }
        }

        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto& row = rows[i];
            std::size_t last_child = i;
            for (std::size_t next = i + 1; next < rows.size() &&
                rows[next].node->level > row.node->level; ++next) {
                if (rows[next].node->level == row.node->level + 1) last_child = next;
            }
            if (last_child == i) continue;
            body += "<line" + attribute("x1", row.x + config.padding_x) +
                attribute("y1", row.y + row.height) +
                attribute("x2", row.x + config.padding_x) +
                attribute("y2", rows[last_child].center_y + config.line_thickness / 2) +
                attribute("stroke-width", config.line_thickness) +
                " class=\"treeView-node-line\" stroke=\"" + theme.line_color + "\"/>";
        }

        const double view_x = -config.line_thickness / 2;
        const double width = std::max(1.0, total_width);
        const double height = std::max(1.0, total_height);
        std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" font-family=\"" +
            escape(theme.font_family) + "\" font-size=\"16\"" +
            attribute("width", width) + attribute("height", height) +
            " viewBox=\"" + number(view_x) + " 0 " + number(width) + " " +
            number(height) + "\"><g class=\"tree-view\">" + body + "</g></svg>";
        return TreeViewSvg{std::move(svg), width, height};
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}
} // namespace folia::mermaid
