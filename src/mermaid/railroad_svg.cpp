// Layout from Mermaid 12.2.0 railroadRenderer.ts and styles.ts. MIT.
module folia.mermaid.railroad_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;

namespace folia::mermaid {
namespace {
using View = std::u16string_view;

std::string number(double value) {
    auto formatted = format_number(value);
    if (!formatted) throw std::runtime_error("Invalid Railroad geometry");
    return *formatted;
}

std::string xml(View value, bool attribute = false) {
    auto escaped = escape_svg_text(value, u"Railroad text");
    if (!escaped) throw std::runtime_error("Invalid Railroad SVG text");
    if (!attribute) return *escaped;
    std::string result;
    for (char c : *escaped) {
        if (c == '"') result += "&quot;";
        else result += c;
    }
    return result;
}

std::string attr(std::string_view key, double value) {
    return " " + std::string(key) + "=\"" + number(value) + "\"";
}

std::string attr(std::string_view key, View value) {
    return " " + std::string(key) + "=\"" + xml(value, true) + "\"";
}

std::string point(double x, double y) { return number(x) + " " + number(y); }

std::string transform(double x, double y) {
    return " transform=\"translate(" + number(x) + ", " + number(y) + ")\"";
}

struct Path {
    std::string commands;

    Path(double x, double y) : commands("M " + point(x, y)) {}

    Path& line(double x, double y) {
        commands += " L " + point(x, y);
        return *this;
    }

    Path& arc(double radius, bool sweep, double x, double y) {
        commands += " A " + point(radius, radius) + " 0 0 " + (sweep ? "1 " : "0 ") +
            point(x, y);
        return *this;
    }
};

struct NodeLayout {
    double width{}, up{}, down{};
    std::vector<NodeLayout> children;

    double height() const { return up + down; }
};

class Renderer {
public:
    Renderer(const RailroadRenderInput& input, const SvgTextMeasure& measure)
        : input_(input), measure_(measure) {}

    RailroadSvg render() {
        const auto& theme = input_.theme;
        double width = 0;
        double y = input_.padding;
        for (const auto& rule : input_.document.rules) {
            const auto dimensions = render_rule(rule, y);
            width = std::max(width, dimensions.first);
            y += dimensions.second + input_.vertical_separation;
        }
        auto height = y + input_.padding;
        width += 2 * input_.padding;
        if (input_.document.rules.empty()) {
            width = 200;
            height = 100;
        }

        std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"railroad-diagram\""
            " role=\"graphics-document document\" aria-roledescription=\"" + xml(input_.type, true) +
            "\" viewBox=\"0 0 " + point(width, height) + "\"" +
            attr("font-family", theme.font_family) + attr("font-size", theme.font_size);
        if (input_.use_max_width) {
            svg += " width=\"100%\" style=\"max-width: " + number(width) + "px;\"";
        } else {
            svg += attr("width", width) + attr("height", height);
        }
        const auto& doc = input_.document;
        if (!doc.acc_title.empty()) svg += " aria-labelledby=\"railroad-title\"";
        if (!doc.acc_description.empty()) svg += " aria-describedby=\"railroad-desc\"";
        svg += ">";
        if (!doc.acc_title.empty()) {
            svg += "<title id=\"railroad-title\">" + xml(doc.acc_title) + "</title>";
        }
        if (!doc.acc_description.empty()) {
            svg += "<desc id=\"railroad-desc\">" + xml(doc.acc_description) + "</desc>";
        }
        svg += body_ + "</svg>";
        return {std::move(svg), width, height};
    }

private:
    SvgTextMetrics metrics(View text) {
        if (const auto found = cache_.find(text); found != cache_.end()) return found->second;
        // The upstream getBBox probe always uses regular weight and SVG whitespace.
        std::u16string collapsed;
        bool space = false;
        for (const auto ch : text) {
            if (ch == u' ' || ch == u'\n' || ch == u'\r' || ch == u'\t') {
                space = !collapsed.empty();
            } else {
                if (space) collapsed += u' ';
                space = false;
                collapsed += ch;
            }
        }
        SvgTextMetrics result{};
        if (!collapsed.empty() && input_.theme.font_size != 0) {
            if (!measure_) throw std::runtime_error("Railroad requires real text measurements");
            auto measured = measure_(collapsed, input_.theme.font_size, 400);
            if (!measured) throw std::runtime_error(measured.error());
            result = *measured;
        }
        cache_.emplace(text, result);
        return result;
    }

    static std::u16string label(const RailroadNode& node) {
        if (node.kind == RailroadNodeKind::special) return u"? " + node.text + u" ?";
        return node.text;
    }

    NodeLayout layout(const RailroadNode& node) {
        using enum RailroadNodeKind;
        switch (node.kind) {
        case terminal:
        case nonterminal:
        case special: {
            const auto text = metrics(label(node));
            const auto half_height = text.height / 2 + input_.padding;
            return {text.width + 2 * input_.padding, half_height, half_height};
        }
        default:
            break;
        }

        NodeLayout result;
        for (const auto& child : node.children) result.children.push_back(layout(child));
        if (node.kind == sequence) {
            for (const auto& child : result.children) {
                result.width += child.width;
                result.up = std::max(result.up, child.up);
                result.down = std::max(result.down, child.down);
            }
            result.width += (static_cast<double>(result.children.size()) - 1) *
                input_.horizontal_separation;
        } else if (node.kind == choice) {
            double height = 0;
            for (const auto& child : result.children) {
                result.width = std::max(result.width, child.width);
                height += child.height();
            }
            height += (static_cast<double>(result.children.size()) - 1) * input_.vertical_separation;
            result.width += 4 * input_.arc_radius;
            result.up = result.down = height / 2;
        } else {
            if (result.children.size() != 1) throw std::runtime_error("Invalid Railroad unary node");
            const auto& child = result.children.front();
            const auto arc_height = 2 * input_.arc_radius;
            result.width = child.width + 4 * input_.arc_radius;
            result.up = child.up + (node.kind == optional || node.min == 0 ? arc_height : 0);
            result.down = child.down + (node.kind == repetition ? arc_height : 0);
        }
        return result;
    }

    void path(const Path& path) {
        body_ += "<path class=\"railroad-line\" fill=\"none\"" + attr("stroke", input_.theme.line) +
            attr("stroke-width", input_.stroke_width) + " d=\"" + path.commands + "\"/>";
    }

    void marker(std::string_view role, double x, double y) {
        body_ += "<g class=\"railroad-" + std::string(role) + "\"><circle" +
            attr("cx", x) + attr("cy", y) + attr("r", input_.marker_radius) +
            attr("fill", input_.theme.marker) + "/></g>";
    }

    void symbol(const RailroadNode& node, const NodeLayout& box, double x, double y) {
        const auto& theme = input_.theme;
        const bool terminal = node.kind == RailroadNodeKind::terminal;
        const bool special = node.kind == RailroadNodeKind::special;
        const auto& style = terminal ? theme.terminal : special ? theme.special : theme.nonterminal;
        const auto* kind = terminal ? "terminal" : special ? "special" : "nonterminal";
        body_ += std::string("<g class=\"railroad-") + kind + "\"" + transform(x, y) +
            "><rect x=\"0\" y=\"0\"" + attr("width", box.width) + attr("height", box.height()) +
            attr("fill", style.fill) + attr("stroke", style.stroke) + attr("stroke-width", input_.stroke_width);
        if (terminal) body_ += " rx=\"10\" ry=\"10\"";
        if (special) body_ += " stroke-dasharray=\"5,3\"";
        body_ += "/><text text-anchor=\"middle\" dominant-baseline=\"middle\"" +
            attr("x", box.width / 2) + attr("y", box.up) + attr("fill", style.text) +
            ">" + xml(label(node)) + "</text></g>";
    }

    void draw_sequence(const RailroadNode& node, const NodeLayout& box) {
        double x = 0;
        for (std::size_t index = 0; index < box.children.size(); ++index) {
            const auto& child = box.children[index];
            draw(node.children[index], child, x, box.up - child.up);
            x += child.width;
            if (index + 1 < box.children.size()) {
                path(Path(x, box.up).line(x + input_.horizontal_separation, box.up));
                x += input_.horizontal_separation;
            }
        }
    }

    void draw_choice(const RailroadNode& node, const NodeLayout& box) {
        const auto r = input_.arc_radius;
        double y = 0;
        for (std::size_t index = 0; index < box.children.size(); ++index) {
            const auto& child = box.children[index];
            const auto x = (box.width - child.width) / 2;
            const auto center = y + child.up;
            draw(node.children[index], child, x, y);

            Path left(0, box.up);
            Path right(x + child.width, center);
            if (center == box.up) {
                left.line(x, center);
                right.line(box.width, box.up);
            } else {
                const bool below = center > box.up;
                const auto offset = below ? r : -r;
                left.arc(r, below, r, box.up + offset)
                    .line(r, center - offset)
                    .arc(r, !below, 2 * r, center)
                    .line(x, center);
                right.line(box.width - 2 * r, center)
                    .arc(r, !below, box.width - r, center - offset)
                    .line(box.width - r, box.up + offset)
                    .arc(r, below, box.width, box.up);
            }
            path(left);
            path(right);
            y += child.height() + input_.vertical_separation;
        }
    }

    void unary(const RailroadNode& node, const NodeLayout& box) {
        const auto& child = box.children.front();
        const auto r = input_.arc_radius;
        const auto center = box.up;
        const auto right = box.width - 2 * r;
        draw(node.children.front(), child, 2 * r, center - child.up);
        path(Path(0, center).line(2 * r, center));
        path(Path(right, center).line(box.width, center));
        if (node.kind == RailroadNodeKind::repetition) {
            const auto loop = box.height() - r;
            path(Path(right, center)
                .arc(r, true, right + r, center + r)
                .line(right + r, loop)
                .arc(r, true, right, loop + r)
                .line(2 * r, loop + r)
                .arc(r, true, r, loop)
                .line(r, center + r)
                .arc(r, true, 2 * r, center));
        }
        if (node.kind == RailroadNodeKind::optional || node.min == 0) {
            path(Path(0, center)
                .arc(r, false, r, center - r)
                .line(r, r)
                .arc(r, true, 2 * r, 0)
                .line(right, 0)
                .arc(r, true, right + r, r)
                .line(right + r, center - r)
                .arc(r, false, box.width, center));
        }
    }

    void draw(const RailroadNode& node, const NodeLayout& box, double x, double y, bool root = false) {
        using enum RailroadNodeKind;
        if (node.kind == terminal || node.kind == nonterminal || node.kind == special) {
            symbol(node, box, x, y);
            return;
        }
        const auto* kind = node.kind == sequence ? "sequence" : node.kind == choice ? "choice" :
            node.kind == optional ? "optional" : "repetition";
        body_ += std::string("<g class=\"railroad-") + kind + "\"";
        if (!root) body_ += transform(x, y);
        body_ += ">";
        if (node.kind == sequence) draw_sequence(node, box);
        else if (node.kind == choice) draw_choice(node, box);
        else unary(node, box);
        body_ += "</g>";
    }

    std::pair<double, double> render_rule(const RailroadRule& rule, double y) {
        const auto name = rule.name + u" =";
        const auto start = metrics(name).width + 20;
        const auto x = start + 20;
        const auto box = layout(rule.definition);
        const auto baseline = std::max(20.0, box.up);
        const auto top = baseline - box.up;
        const auto end = x + box.width + 10;
        body_ += "<g class=\"railroad-rule\"" + transform(0, y) + "><g" + transform(x, top) + ">";
        draw(rule.definition, box, 0, 0, true);
        body_ += "</g><g class=\"railroad-rule-name-group\"><text class=\"railroad-rule-name\""
            " font-weight=\"bold\" x=\"0\"" + attr("y", baseline) +
            attr("fill", input_.theme.rule_name) + ">" + xml(name) + "</text></g>";
        marker("start", start, baseline);
        marker("end", end, baseline);
        path(Path(start + input_.marker_radius, baseline).line(x, baseline));
        path(Path(x + box.width, baseline).line(end - input_.marker_radius, baseline));
        body_ += "</g>";
        return {end + input_.marker_radius, std::max(40.0, top + box.height() + 2 * input_.padding)};
    }

    const RailroadRenderInput& input_;
    const SvgTextMeasure& measure_;
    std::map<std::u16string, SvgTextMetrics, std::less<>> cache_;
    std::string body_;
};

View trim(View value) {
    const auto first = value.find_first_not_of(u" \r\n\t\f\v");
    if (first == value.npos) return {};
    return value.substr(first, value.find_last_not_of(u" \r\n\t\f\v") - first + 1);
}

double numeric(const ConfigValue* value, double fallback, bool strictly_positive = false) {
    if (!value) return fallback;
    double result = 0;
    if (value->kind == ConfigKind::number) {
        result = value->number;
    } else if (value->kind == ConfigKind::string) {
        auto text = trim(value->string);
        if (text.starts_with(u'+')) text.remove_prefix(1);
        std::string ascii;
        for (auto c : text) {
            if (c > 127) break;
            ascii.push_back(static_cast<char>(c));
        }
        const auto end = ascii.data() + ascii.size();
        if (std::from_chars(ascii.data(), end, result).ec != std::errc{}) return fallback;
    } else {
        return fallback;
    }
    if (!std::isfinite(result) || result < 0 || (strictly_positive && result == 0)) return fallback;
    return result;
}

std::u16string sanitized(const ConfigValue* value, View fallback, const std::regex& pattern) {
    if (value && value->kind == ConfigKind::string) {
        const auto text = trim(value->string);
        if (std::ranges::all_of(text, [](char16_t c) { return c < 128; }) &&
            std::regex_match(std::string(text.begin(), text.end()), pattern)) {
            return std::u16string(text);
        }
    }
    return std::u16string(fallback);
}

const ConfigValue* color_value(const ConfigValue& theme, View key, View secondary) {
    const auto* value = theme.find(key);
    return value && value->kind != ConfigKind::null ? value : theme.find(secondary);
}
}

std::expected<RailroadRenderInput, std::string> prepare_railroad_render(const PreparedDiagram& prepared) {
    auto document = parse_railroad(encode_diagram_source(prepared.code.with_comments));
    if (!document) return std::unexpected(document.error());

    RailroadRenderInput input;
    input.type = prepared.type;
    input.document = std::move(*document);
    auto& theme = input.theme;
    const ConfigValue empty;
    const auto* theme_config = prepared.effective_config.find(u"themeVariables");
    const auto* options = prepared.effective_config.find(u"railroad");
    const auto& vars = theme_config ? *theme_config : empty;
    const auto& config = options ? *options : empty;

    static const std::regex family_pattern(R"([\w "',.\-]+)");
    theme.font_family = sanitized(vars.find(u"fontFamily"), theme.font_family, family_pattern);
    theme.font_family = sanitized(config.find(u"fontFamily"), theme.font_family, family_pattern);
    theme.font_size = numeric(config.find(u"fontSize"), numeric(vars.find(u"fontSize"), 14, true));
    input.padding = numeric(config.find(u"padding"), input.padding);
    input.vertical_separation = numeric(config.find(u"verticalSeparation"), input.vertical_separation);
    input.horizontal_separation = numeric(config.find(u"horizontalSeparation"), input.horizontal_separation);
    input.arc_radius = numeric(config.find(u"arcRadius"), input.arc_radius);
    input.marker_radius = numeric(config.find(u"markerRadius"), input.marker_radius);
    input.stroke_width = numeric(config.find(u"strokeWidth"), input.stroke_width);
    if (const auto* max_width = config.find(u"useMaxWidth");
        max_width && max_width->kind != ConfigKind::null) {
        input.use_max_width = config_truthy(*max_width);
    }

    static const std::regex color_pattern(
        R"(#(?:[\da-f]{3,4}|[\da-f]{6}|[\da-f]{8})|(?:rgb|rgba|hsl|hsla|hwb|lab|lch|oklab|oklch)\([\d\s%+,./-]+\)|[a-z]+)",
        std::regex::icase);
    auto color = [&](View option, View key, View secondary, View fallback) {
        const auto themed = sanitized(color_value(vars, key, secondary), fallback, color_pattern);
        return sanitized(config.find(option), themed, color_pattern);
    };
    theme.terminal.fill = color(u"terminalFill", u"secondBkg", u"secondaryColor", theme.terminal.fill);
    theme.terminal.stroke = color(u"terminalStroke", u"secondaryBorderColor", u"lineColor", theme.terminal.stroke);
    theme.terminal.text = color(u"terminalTextColor", u"secondaryTextColor", u"textColor", theme.terminal.text);
    theme.nonterminal.fill = color(u"nonTerminalFill", u"mainBkg", u"background", theme.nonterminal.fill);
    theme.nonterminal.stroke = color(u"nonTerminalStroke", u"primaryBorderColor", u"lineColor", theme.nonterminal.stroke);
    theme.nonterminal.text = color(u"nonTerminalTextColor", u"primaryTextColor", u"textColor", theme.nonterminal.text);
    theme.special.fill = color(u"specialFill", u"tertiaryColor", u"secondaryColor", theme.special.fill);
    theme.special.stroke = color(u"specialStroke", u"tertiaryBorderColor", u"secondaryBorderColor", theme.special.stroke);
    theme.special.text = theme.nonterminal.text;
    theme.line = color(u"lineColor", u"lineColor", u"lineColor", theme.line);
    theme.marker = color(u"markerFill", u"lineColor", u"lineColor", theme.marker);
    theme.rule_name = color(u"ruleNameColor", u"titleColor", u"textColor", theme.rule_name);
    return input;
}

std::expected<RailroadSvg, std::string> render_railroad_svg(
    const RailroadRenderInput& input, const SvgTextMeasure& measure) {
    try {
        return Renderer(input, measure).render();
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}
}
