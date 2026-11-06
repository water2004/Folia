module folia.mermaid.ishikawa_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;
namespace folia::mermaid {
namespace {
const ConfigValue& member(const ConfigValue& value, std::u16string_view key) {
    static const ConfigValue empty{};
    const auto* found = value.find(key);
    return found ? *found : empty;
}
std::u16string string(const ConfigValue& value, std::u16string_view fallback = {}) {
    return value.kind == ConfigKind::string ? value.string : std::u16string(fallback);
}
double number(const ConfigValue& value, double fallback) {
    return value.kind == ConfigKind::number ? value.number : fallback;
}
std::string n(double value) {
    auto formatted = format_number(value);
    if (!formatted) throw std::runtime_error("Invalid Ishikawa geometry");
    return *formatted;
}
std::string esc(std::u16string_view value) {
    auto escaped = escape_svg_text(value, u"Ishikawa label");
    if (!escaped) throw std::runtime_error("Invalid Ishikawa text");
    return *escaped;
}
struct Label {
    std::u16string text;
    double width = 0, height = 0, font_height = 0, baseline = 0;
    double middle_offset = 0, line_height = 0;
};
struct Flat { std::u16string text; int depth = 0, parent = -1, children = 0; };
struct Bounds {
    double left = std::numeric_limits<double>::infinity();
    double top = std::numeric_limits<double>::infinity();
    double right = -std::numeric_limits<double>::infinity();
    double bottom = -std::numeric_limits<double>::infinity();
    void add(double x, double y, double width, double height) {
        left = std::min(left, x);
        top = std::min(top, y);
        right = std::max(right, x + width);
        bottom = std::max(bottom, y + height);
    }
    void add_line(double x1, double y1, double x2, double y2) {
        add(std::min(x1, x2), std::min(y1, y2),
            std::abs(x2 - x1), std::abs(y2 - y1));
    }
};
std::u16string wrap_label(std::u16string_view value, std::size_t limit) {
    if (value.size() <= limit) return std::u16string(value);
    auto is_space = [](char16_t ch) {
        return ch == u' ' || ch == u'\t' || ch == u'\r' || ch == u'\n' ||
            ch == u'\f' || ch == u'\v';
    };
    std::u16string result;
    std::size_t start = 0;
    while (start < value.size()) {
        while (start < value.size() && is_space(value[start])) ++start;
        if (start == value.size()) break;
        auto end = start;
        while (end < value.size() && !is_space(value[end])) ++end;
        const auto word = value.substr(start, end - start);
        const auto newline = result.find_last_of(u'\n');
        const auto line_start = newline == result.npos ? 0 : newline + 1;
        if (!result.empty() && result.size() - line_start + 1 + word.size() <= limit)
            result += u' ';
        else if (!result.empty())
            result += u'\n';
        result += word;
        start = end;
    }
    return result;
}
std::pair<std::vector<Flat>, std::vector<int>> flatten(
    const std::vector<IshikawaNode>& nodes, int direction) {
    std::vector<Flat> entries;
    std::vector<int> y_order;
    auto walk = [&](auto&& self, const std::vector<IshikawaNode>& siblings,
                    int parent, int depth) -> void {
        auto visit = [&](const IshikawaNode& node) {
            const int index = static_cast<int>(entries.size());
            entries.push_back({wrap_label(node.text, 15), depth, parent,
                               static_cast<int>(node.children.size())});
            if (depth % 2 == 0) {
                y_order.push_back(index);
                if (!node.children.empty()) self(self, node.children, index, depth + 1);
            } else {
                if (!node.children.empty()) self(self, node.children, index, depth + 1);
                y_order.push_back(index);
            }
        };
        if (direction < 0) {
            for (auto it = siblings.rbegin(); it != siblings.rend(); ++it) visit(*it);
        } else {
            for (const auto& node : siblings) visit(node);
        }
    };
    walk(walk, nodes, -1, 2);
    return {std::move(entries), std::move(y_order)};
}
void text(std::string& out, std::u16string_view value, double x, double y,
          std::string_view class_name, std::string_view anchor, double size,
          double line_height = 0) {
    std::vector<std::u16string> lines;
    std::size_t from = 0;
    while (from <= value.size()) {
        auto end = value.find(u'\n', from);
        if (end == value.npos) end = value.size();
        lines.emplace_back(value.substr(from, end - from));
        if (end == value.size()) break;
        from = end + 1;
    }
    if (line_height <= 0) line_height = size * 1.05;
    out += "<text class=\"" + std::string(class_name) + "\" text-anchor=\"" +
           std::string(anchor) + "\" x=\"" + n(x) + "\" y=\"" +
           n(y - (lines.size() - 1) * line_height / 2) + "\" font-size=\"" +
           n(size) + "\">";
    for (std::size_t i = 0; i < lines.size(); ++i) {
        out += "<tspan x=\"" + n(x) + "\" dy=\"" +
               (i ? n(line_height) : "0") + "\">" + esc(lines[i]) + "</tspan>";
    }
    out += "</text>";
}
}
std::expected<IshikawaRenderInput, std::string> prepare_ishikawa_render(
    const PreparedDiagram& prepared) {
    auto document = parse_ishikawa(prepared.code.cleaned);
    if (!document) return std::unexpected(document.error());

    IshikawaRenderInput out;
    out.document = std::move(*document);
    const auto& config = member(prepared.effective_config, u"ishikawa");
    const auto& theme = member(prepared.effective_config, u"themeVariables");
    out.theme.font_family = string(member(prepared.effective_config, u"fontFamily"), u"Arial");
    out.theme.font_size = number(member(theme, u"fontSize"), 16);
    auto narrow = [](std::u16string_view value) { return std::string(value.begin(), value.end()); };
    out.theme.line = narrow(string(member(theme, u"lineColor"), u"#333333"));
    out.theme.background = narrow(string(member(theme, u"mainBkg"), u"#ffffff"));
    out.theme.text = narrow(string(member(theme, u"textColor"), u"#333333"));
    out.config.diagram_padding = number(member(config, u"diagramPadding"), 20);
    const auto& max_width = member(config, u"useMaxWidth");
    out.config.use_max_width = max_width.kind == ConfigKind::boolean && max_width.boolean;
    return out;
}
std::expected<IshikawaSvg, std::string> render_ishikawa_svg(
    const IshikawaRenderInput& input, const SvgTextMeasure& measure) {
 try {
    if (!measure) return std::unexpected("Ishikawa rendering requires platform text measurement");
    const auto& root = input.document.root;
    const double font_size = input.theme.font_size;
    auto measure_lines = [&](std::u16string_view label, double size,
                             unsigned weight, double line_height)
        -> std::expected<Label, std::string> {
        Label result;
        result.text = std::u16string(label);
        std::size_t from = 0, line_count = 0;
        while (from <= label.size()) {
            auto end = label.find(u'\n', from);
            if (end == label.npos) end = label.size();
            auto line = label.substr(from, end - from);
            auto measured = measure(line.empty() ? std::u16string_view(u"\u200b") : line,
                                    size, weight);
            if (!measured) return std::unexpected(measured.error());
            if (line_count == 0) {
                result.height = measured->height;
                result.font_height = measured->height;
                result.baseline = measured->baseline;
                result.middle_offset = measured->middle_offset;
                result.line_height = line_height;
            }
            result.width = std::max(result.width, measured->width);
            ++line_count;
            if (end == label.size()) break;
            from = end + 1;
        }
        if (line_count > 1) result.height += (line_count - 1) * line_height;
        return result;
    };
    auto wrap = [](std::u16string_view value, std::size_t limit) {
        if (value.size() <= limit) return std::u16string(value);
        std::u16string result;
        std::size_t start = 0;
        while (start < value.size()) {
            auto is_space = [](char16_t ch) {
                return ch == u' ' || ch == u'\t' || ch == u'\r' || ch == u'\n' ||
                    ch == u'\f' || ch == u'\v';
            };
            while (start < value.size() && is_space(value[start])) ++start;
            if (start == value.size()) break;
            auto end = start;
            while (end < value.size() && !is_space(value[end])) ++end;
            const auto word = value.substr(start, end - start);
            const auto newline = result.find_last_of(u'\n');
            const auto line_start = newline == result.npos ? 0 : newline + 1;
            if (!result.empty() && result.size() - line_start + 1 + word.size() <= limit) {
                result += u' ';
            } else if (!result.empty()) {
                result += u'\n';
            }
            result += word;
            start = end;
        }
        return result;
    };

    const auto max_chars = std::max(6, static_cast<int>(110 / (font_size * 0.6)));
    const auto head = wrap(root.text, static_cast<std::size_t>(max_chars));
    const double head_line_height = font_size * 1.05;
    auto head_metrics = measure_lines(head, 14, 600, head_line_height);
    if (!head_metrics) return std::unexpected(head_metrics.error());
    const double head_width = std::max(60.0, head_metrics->width + 6);
    const double head_height = std::max(40.0, head_metrics->height * 2 + 40);

    constexpr double spine_base = 250;
    constexpr double angle = 82 * std::numbers::pi / 180;
    const double cosine = std::cos(angle), sine = std::sin(angle);
    const auto& causes = root.children;
    auto side_stats = [&](std::size_t parity) {
        std::size_t total = 0, maximum = 0;
        auto count_descendants = [&](auto&& self, const IshikawaNode& node) -> std::size_t {
            std::size_t count = 0;
            for (const auto& child : node.children) count += 1 + self(self, child);
            return count;
        };
        for (std::size_t i = parity; i < causes.size(); i += 2) {
            const auto count = count_descendants(count_descendants, causes[i]);
            total += count;
            maximum = std::max(maximum, count);
        }
        return std::pair{total, maximum};
    };
    const auto upper_stats = side_stats(0), lower_stats = side_stats(1);
    double upper_length = spine_base, lower_length = spine_base;
    const auto descendant_total = upper_stats.first + lower_stats.first;
    if (descendant_total) {
        upper_length = std::max(spine_base * 0.3,
            2 * spine_base * upper_stats.first / descendant_total);
        lower_length = std::max(spine_base * 0.3,
            2 * spine_base * lower_stats.first / descendant_total);
    }
    const double min_spacing = font_size * 2;
    upper_length = std::max(upper_length, upper_stats.second * min_spacing);
    lower_length = std::max(lower_length, lower_stats.second * min_spacing);
    const double spine_y = std::max(upper_length, spine_base);
    double spine_x = causes.empty() ? 0 : -20;

    Bounds bounds;
    std::string out = "<svg xmlns=\"http://www.w3.org/2000/svg\"><defs>"
        "<marker id=\"ishikawa-arrow\" viewBox=\"0 0 10 10\" refX=\"0\" refY=\"5\" "
        "markerWidth=\"6\" markerHeight=\"6\" orient=\"auto\"><path d=\"M10 0L0 5L10 10Z\" "
        "class=\"ishikawa-arrow\" fill=\"" + input.theme.line + "\"/></marker></defs>"
        "<style>.ishikawa .ishikawa-spine,.ishikawa .ishikawa-branch,.ishikawa .ishikawa-sub-branch{stroke:" + input.theme.line +
        ";stroke-width:2;fill:none}.ishikawa .ishikawa-sub-branch{stroke-width:1}.ishikawa .ishikawa-head,.ishikawa .ishikawa-label-box{fill:" + input.theme.background +
        ";stroke:" + input.theme.line + ";stroke-width:2}.ishikawa text{font-family:" + esc(input.theme.font_family) + ";font-size:" + n(font_size) +
        "px;fill:" + input.theme.text + "}.ishikawa .ishikawa-head-label{font-weight:600;text-anchor:middle;dominant-baseline:middle;font-size:14px}.ishikawa .ishikawa-label.cause,.ishikawa .ishikawa-label.align{dominant-baseline:middle}.ishikawa .ishikawa-label.up{dominant-baseline:baseline}.ishikawa .ishikawa-label.down{dominant-baseline:hanging}</style>"
        "<g class=\"ishikawa\" font-family=\"" + esc(input.theme.font_family) +
        "\" fill=\"" + input.theme.text + "\">";
    out += "<g class=\"ishikawa-head-group\" transform=\"translate(0," +
        n(spine_y) + ")\"><path class=\"ishikawa-head\" d=\"M0 " +
        n(-head_height / 2) + " L0 " + n(head_height / 2) + " Q" + n(head_width * 2.4) +
        " 0 0 " + n(-head_height / 2) + "Z\" fill=\"" + input.theme.background +
        "\" stroke=\"" + input.theme.line + "\" stroke-width=\"2\"/>";
    const double head_label_x = head_width / 2 + 3;
    const double head_label_y = head_metrics->baseline - head_metrics->middle_offset -
        head_metrics->font_height / 2;
    text(out, head, head_label_x, head_label_y, "ishikawa-head-label", "middle", 14,
         head_line_height);
    bounds.add(0, spine_y - head_height / 2, head_width * 1.2, head_height);
    bounds.add(head_label_x - head_metrics->width / 2,
        spine_y - head_metrics->height / 2,
        head_metrics->width, head_metrics->height);
    out += "</g>";

    for (std::size_t pair_start = 0; pair_start < causes.size(); pair_start += 2) {
      double pair_left = std::numeric_limits<double>::infinity();
      for (std::size_t cause_index = pair_start;
           cause_index < std::min(pair_start + 2, causes.size()); ++cause_index) {
        const auto direction = cause_index % 2 ? 1 : -1;
        const auto& cause = causes[cause_index];
        const double length = direction < 0 ? upper_length : lower_length;
        const double line_length = cause.children.empty() ? length * 0.2 : length;
        const double dx = -cosine * line_length;
        const double dy = sine * line_length * direction;
        const double end_x = spine_x + dx, end_y = spine_y + dy;
        bounds.add_line(spine_x, spine_y, end_x, end_y);
        out += "<line class=\"ishikawa-branch\" x1=\"" + n(spine_x) + "\" y1=\"" +
            n(spine_y) + "\" x2=\"" + n(end_x) + "\" y2=\"" + n(end_y) +
            "\" stroke=\"" + input.theme.line + "\" stroke-width=\"2\" "
            "marker-start=\"url(#ishikawa-arrow)\"/>";
        const auto& label = cause.text;
        auto label_metrics = measure_lines(label, font_size, 400, font_size * 1.05);
        if (!label_metrics) return std::unexpected(label_metrics.error());
        pair_left = std::min(pair_left, end_x - label_metrics->width / 2);
        const double label_y = end_y + 11 * direction;
        const double label_box_x = end_x - label_metrics->width / 2 - 20;
        const double label_box_y = label_y - (label_metrics->height - label_metrics->font_height) / 2 +
            label_metrics->middle_offset - label_metrics->baseline - 2;
        out += "<rect class=\"ishikawa-label-box\" x=\"" +
            n(label_box_x) + "\" y=\"" + n(label_box_y) + "\" width=\"" +
            n(label_metrics->width + 40) + "\" height=\"" +
            n(label_metrics->height + 4) + "\" fill=\"" + input.theme.background +
            "\" stroke=\"" + input.theme.line + "\" stroke-width=\"2\"/>";
        bounds.add(label_box_x, label_box_y,
                   label_metrics->width + 40, label_metrics->height + 4);
        text(out, label, end_x, label_y, "ishikawa-label cause", "middle", font_size,
             font_size * 1.05);

        auto [entries, y_order] = flatten(cause.children, direction);
        std::vector<double> ys(entries.size());
        for (std::size_t slot = 0; slot < y_order.size(); ++slot) {
            ys[y_order[slot]] = spine_y + dy * (static_cast<double>(slot + 1) /
                static_cast<double>(entries.size() + 1));
        }
        struct Bone { double x0, y0, x1, y1; int child_count, children_drawn = 0; };
        std::map<int, Bone> bones;
        bones[-1] = {spine_x, spine_y, end_x, end_y,
                     static_cast<int>(cause.children.size()), 0};
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            const double y = ys[i];
            auto& parent = bones.at(entry.parent);
            double x0 = 0, y0 = 0, x1 = 0;
            if (entry.depth % 2 == 0) {
                const double parent_dy = parent.y1 - parent.y0;
                const double t = parent_dy ? (y - parent.y0) / parent_dy : 0.5;
                x0 = parent.x0 + (parent.x1 - parent.x0) * t;
                y0 = y;
                x1 = x0 - (entry.children ? 60 + 5 * entry.children : 30);
                bounds.add_line(x0, y, x1, y);
                out += "<line class=\"ishikawa-sub-branch\" x1=\"" + n(x0) +
                    "\" y1=\"" + n(y) + "\" x2=\"" + n(x1) + "\" y2=\"" +
                    n(y) + "\" stroke=\"" + input.theme.line +
                    "\" marker-start=\"url(#ishikawa-arrow)\"/>";
            } else {
                const int child_index = parent.children_drawn++;
                x0 = parent.x0 + (parent.x1 - parent.x0) *
                    (parent.child_count - child_index) / (parent.child_count + 1);
                y0 = parent.y0;
                x1 = x0 - cosine * ((y - y0) / (sine * direction));
                bounds.add_line(x0, y0, x1, y);
                out += "<line class=\"ishikawa-sub-branch\" x1=\"" + n(x0) +
                    "\" y1=\"" + n(y0) + "\" x2=\"" + n(x1) + "\" y2=\"" +
                    n(y) + "\" stroke=\"" + input.theme.line +
                    "\" marker-start=\"url(#ishikawa-arrow)\"/>";
            }
            const auto& child_label = entry.text;
            auto child_metrics = measure_lines(child_label, font_size, 400, font_size * 1.05);
            if (!child_metrics) return std::unexpected(child_metrics.error());
            const bool horizontal_bone = entry.depth % 2 == 0;
            pair_left = std::min(pair_left, x1 - child_metrics->width);
            const double first_line_y = y -
                (child_metrics->height - child_metrics->font_height) / 2;
            const double child_top = horizontal_bone
                ? first_line_y + child_metrics->middle_offset - child_metrics->baseline
                : direction < 0 ? first_line_y - child_metrics->baseline
                                : first_line_y - 0.2 * child_metrics->baseline;
            bounds.add(x1 - child_metrics->width, child_top,
                       child_metrics->width, child_metrics->height);
            const std::string label_class = horizontal_bone ? "ishikawa-label align"
                : (direction < 0 ? "ishikawa-label up" : "ishikawa-label down");
            text(out, child_label, x1, y, label_class, "end", font_size,
                 font_size * 1.05);
            if (entry.children) {
                bones[static_cast<int>(i)] = {
                    x0, y0, x1, y, entry.children, 0};
            }
        }
        pair_left = std::min(pair_left, end_x - label_metrics->width / 2);
      }
      if (std::isfinite(pair_left)) spine_x = pair_left;
    }
    bounds.add_line(spine_x, spine_y, 0, spine_y);
    const auto spine_line = "<line class=\"ishikawa-spine\" x1=\"" + n(spine_x) +
        "\" y1=\"" + n(spine_y) + "\" x2=\"0\" y2=\"" + n(spine_y) +
        "\" stroke=\"" + input.theme.line + "\" stroke-width=\"2\"/>";
    const auto spine_at = out.find("<g class=\"ishikawa-head-group\"");
    if (spine_at == std::string::npos) throw std::runtime_error("Missing Ishikawa head group");
    out.insert(spine_at, spine_line);
    out += "</g></svg>";
    const double padding = input.config.diagram_padding;
    const double width = bounds.right - bounds.left + padding * 2;
    const double height = bounds.bottom - bounds.top + padding * 2;
    const auto view_box = " viewBox=\"" + n(bounds.left - padding) + " " +
        n(bounds.top - padding) + " " + n(width) + " " + n(height) + "\"";
    const auto size = input.config.use_max_width
        ? " width=\"100%\" style=\"max-width:" + n(width) + "px\""
        : " width=\"" + n(width) + "\"";
    out.insert(out.find('>'), size + " height=\"" + n(height) + "\"" + view_box);
    return IshikawaSvg{std::move(out), width, height};
 } catch (const std::exception& error) {
    return std::unexpected(error.what());
 }
}
}
