module folia.mermaid.cynefin_svg;
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

std::u16string string_value(const ConfigValue& value, std::u16string fallback = {}) {
    return value.kind == ConfigKind::string ? value.string : std::move(fallback);
}

double number_value(const ConfigValue& value, double fallback) {
    return value.kind == ConfigKind::number ? value.number : fallback;
}

bool boolean_value(const ConfigValue& value, bool fallback) {
    return value.kind == ConfigKind::boolean ? value.boolean : fallback;
}

std::string escape_text(std::u16string_view value) {
    auto escaped = escape_svg_text(value, u"Cynefin SVG text");
    if (!escaped) throw std::runtime_error("Invalid Cynefin SVG text");
    return *escaped;
}

std::string number(double value) {
    auto formatted = format_number(value);
    if (!formatted) throw std::runtime_error("Invalid Cynefin geometry");
    return *formatted;
}

std::string attribute(std::string_view key, std::string_view value) {
    std::string result = " " + std::string(key) + "=\"";
    for (const char ch : value) {
        if (ch == '&') result += "&amp;";
        else if (ch == '"') result += "&quot;";
        else if (ch == '<') result += "&lt;";
        else result += ch;
    }
    return result + "\"";
}

std::string attribute(std::string_view key, std::u16string_view value) {
    return attribute(key, escape_text(value));
}

double seeded_random(std::uint32_t seed) {
    // Mulberry32 with JavaScript's 32-bit unsigned arithmetic.
    std::uint32_t value = seed + 0x6d2b79f5u;
    value = (value ^ (value >> 15)) * (value | 1u);
    value ^= value + (value ^ (value >> 7)) * (value | 61u);
    return static_cast<double>(value ^ (value >> 14)) / 4294967296.0;
}

std::uint32_t hash_string(std::string_view value) {
    std::uint32_t hash = 0;
    for (const auto ch : value) hash = hash * 31u + static_cast<unsigned char>(ch);
    return hash;
}

std::string boundary_path(double width, double height, std::uint32_t seed,
                          double amplitude, bool horizontal) {
    constexpr unsigned segments = 7;
    double previous_x = 0;
    double previous_y = 0;
    std::string path;

    for (unsigned i = 0; i <= segments; ++i) {
        const double x = horizontal
            ? width * i / segments
            : width / 2 + (seeded_random(seed + i * 17) * 2 - 1) * amplitude;
        const double y = horizontal
            ? height / 2 + (seeded_random(seed + i * 23) * 2 - 1) * amplitude
            : height * i / segments;

        if (i == 0) {
            path = "M" + number(x) + "," + number(y);
        } else {
            const unsigned segment = i - 1;
            const double direction = segment % 2 ? -1 : 1;
            const double offset = amplitude * 1.5 * direction * seeded_random(
                seed + (horizontal ? segment * 37 + 11 : segment * 31 + 7));
            const double middle_x = (previous_x + x) / 2;
            const double middle_y = (previous_y + y) / 2;
            if (horizontal) {
                path += " C" + number(middle_x) + "," + number(previous_y + offset) + " " +
                    number(middle_x) + "," + number(y - offset) + " " + number(x) + "," + number(y);
            } else {
                path += " C" + number(previous_x + offset) + "," + number(middle_y) + " " +
                    number(x - offset) + "," + number(middle_y) + " " + number(x) + "," + number(y);
            }
        }
        previous_x = x;
        previous_y = y;
    }
    return path;
}

std::string cliff_path(double width, double height) {
    const double center_x = width / 2;
    const double top_y = height / 2;
    const double amplitude = width * .03;
    const double half_height = height - top_y;
    return "M" + number(center_x) + "," + number(top_y) +
        " C" + number(center_x + amplitude) + "," + number(top_y + half_height * .2) +
        " " + number(center_x - amplitude * 1.5) + "," + number(top_y + half_height * .55) +
        " " + number(center_x + amplitude * .5) + "," + number(top_y + half_height * .75) +
        " C" + number(center_x - amplitude) + "," + number(top_y + half_height * .85) +
        " " + number(center_x + amplitude * .3) + "," + number(top_y + half_height * .95) +
        " " + number(center_x) + "," + number(height);
}

struct DomainBox {
    double center_x, center_y, x, y, width, height;
};

std::size_t domain_index(CynefinDomainName domain) {
    return static_cast<std::size_t>(domain);
}

std::array<DomainBox, 5> domain_boxes(double width, double height) {
    const double half_width = width / 2;
    const double half_height = height / 2;
    return {{
        {width * .25, height * .25, 0, 0, half_width, half_height},
        {width * .75, height * .25, half_width, 0, half_width, half_height},
        {width * .75, height * .75, half_width, half_height, half_width, half_height},
        {width * .25, height * .75, 0, half_height, half_width, half_height},
        {width / 2, height / 2, width * .35, height * .35, width * .3, height * .3},
    }};
}

std::array<std::string, 5> domain_colors(const CynefinTheme& theme) {
    return {theme.complex_bg, theme.complicated_bg, theme.clear_bg,
            theme.chaotic_bg, theme.confusion_bg};
}

void append_domain_backgrounds(std::string& svg, const std::array<DomainBox, 5>& boxes,
                               const std::array<std::string, 5>& colors) {
    svg += "<g class=\"cynefin-backgrounds\">";
    for (const auto domain : {CynefinDomainName::complex, CynefinDomainName::complicated,
                              CynefinDomainName::chaotic, CynefinDomainName::clear}) {
        const auto index = domain_index(domain);
        const auto& box = boxes[index];
        svg += "<rect class=\"cynefinDomain\" x=\"" + number(box.x) + "\" y=\"" +
            number(box.y) + "\" width=\"" + number(box.width) + "\" height=\"" +
            number(box.height) + "\" fill=\"" + colors[index] +
            "\" fill-opacity=\"0.4\" stroke=\"none\"/>";
    }
    svg += "</g>";
}

void append_boundaries(std::string& svg, const CynefinConfig& config, const CynefinTheme& theme) {
    const auto seed = config.seed ? config.seed : hash_string("mermaid-cynefin");
    const auto vertical_amplitude = config.boundary_amplitude;
    const auto horizontal_amplitude = config.boundary_amplitude;
    svg += "<g class=\"cynefin-boundaries\"><path class=\"cynefinBoundary\" d=\"" +
        boundary_path(config.width, config.height, seed, vertical_amplitude, false) +
        "\" fill=\"none\" stroke=\"" + theme.boundary_color + "\" stroke-width=\"" +
        number(theme.boundary_width) + "\" stroke-dasharray=\"6 3\"/><path class=\"cynefinBoundary\" d=\"" +
        boundary_path(config.width, config.height, seed + 100, horizontal_amplitude, true) +
        "\" fill=\"none\" stroke=\"" + theme.boundary_color + "\" stroke-width=\"" +
        number(theme.boundary_width) + "\" stroke-dasharray=\"6 3\"/><path class=\"cynefinCliff\" d=\"" +
        cliff_path(config.width, config.height) + "\" fill=\"none\" stroke=\"" + theme.cliff_color +
        "\" stroke-width=\"" + number(theme.cliff_width) + "\"/></g>";
}

void append_confusion_shape(std::string& svg, double width, double height,
                            const std::string& color, const CynefinTheme& theme) {
    const double rx = width * .15;
    const double ry = height * .15;
    svg += "<path class=\"cynefinConfusion\" d=\"M" + number(width / 2 - rx) + "," +
        number(height / 2) + " A" + number(rx) + "," + number(ry) + " 0 1,1 " +
        number(width / 2 + rx) + "," + number(height / 2) + " A" + number(rx) + "," +
        number(ry) + " 0 1,1 " + number(width / 2 - rx) + "," + number(height / 2) +
        " Z\" fill=\"" + color + "\" fill-opacity=\"0.5\" stroke=\"" + theme.boundary_color +
        "\" stroke-width=\"1.5\" stroke-dasharray=\"4 2\"/>";
}

std::string text_element(
    std::u16string_view value, double x, double y, double size, unsigned weight,
    std::string_view class_name, std::string_view color, std::string_view baseline,
    const SvgTextMeasure& measure) {
    auto metrics = measure(value, size, weight);
    if (!metrics) throw std::runtime_error(metrics.error());
    return "<text class=\"" + std::string(class_name) + "\" x=\"" + number(x) +
        "\" y=\"" + number(y) + "\" text-anchor=\"middle\" dominant-baseline=\"" +
        std::string(baseline) + "\" font-size=\"" + number(size) + "\" font-weight=\"" +
        std::to_string(weight) + "\" fill=\"" + std::string(color) + "\">" +
        escape_text(value) + "</text>";
}

std::expected<void, std::string> append_domain_labels(
    std::string& svg, const CynefinConfig& config, const CynefinTheme& theme,
    const std::array<DomainBox, 5>& boxes, const SvgTextMeasure& measure) {
    constexpr std::array<std::size_t, 4> order{0, 1, 3, 2};
    constexpr std::array<std::u16string_view, 4> labels{u"Complex", u"Complicated", u"Clear", u"Chaotic"};
    constexpr std::array<std::u16string_view, 4> models{
        u"Probe → Sense → Respond", u"Sense → Analyse → Respond",
        u"Sense → Categorise → Respond", u"Act → Sense → Respond"};
    constexpr std::array<std::u16string_view, 4> practices{
        u"Emergent Practices", u"Good Practices", u"Best Practices", u"Novel Practices"};
    const bool descriptions = config.show_domain_descriptions;
    svg += "<g class=\"cynefin-labels\">";
    for (const auto i : order) {
        const auto& box = boxes[i];
        svg += text_element(labels[i], box.center_x,
            descriptions ? box.center_y - 30 : box.center_y, theme.domain_font_size, 700,
            "cynefinDomainLabel", theme.label_color, "middle", measure);
    }
    svg += text_element(u"Confusion", config.width / 2,
        descriptions ? config.height / 2 - 10 : config.height / 2,
        theme.domain_font_size, 700, "cynefinDomainLabel", theme.label_color, "middle", measure);
    if (descriptions) {
        for (const auto i : order) {
            const auto& box = boxes[i];
            svg += text_element(models[i], box.center_x, box.center_y - 10,
                theme.item_font_size - 1, 400, "cynefinSubtitle", theme.text_color, "middle", measure);
            svg += text_element(practices[i], box.center_x, box.center_y + 5,
                theme.item_font_size - 1, 400, "cynefinSubtitle", theme.text_color, "middle", measure);
        }
        svg += text_element(u"Disorder", config.width / 2, config.height / 2 + 8,
            theme.item_font_size - 1, 400, "cynefinSubtitle", theme.text_color, "middle", measure);
    }
    svg += "</g>";
    return {};
}

std::u16string overflow_label(std::size_t count) {
    const auto digits = std::to_string(count);
    std::u16string result = u"+";
    for (const char digit : digits) result += static_cast<char16_t>(digit);
    result += u" more";
    return result;
}

std::expected<void, std::string> append_item_badge(
    std::string& svg, std::u16string_view label, double center_x, double y,
    const CynefinTheme& theme, const std::string& color, bool overflow,
    const SvgTextMeasure& measure) {
    auto metrics = measure(label, theme.item_font_size, 400);
    if (!metrics) return std::unexpected(metrics.error());
    const double width = metrics->width + 20;
    const double x = center_x - width / 2;
    svg += "<g transform=\"translate(" + number(x) + ", " + number(y) + ")\"><rect class=\"" +
        std::string(overflow ? "cynefinItemOverflow" : "cynefinItem") +
        "\" x=\"0\" y=\"0\" width=\"" + number(width) +
        "\" height=\"26\" rx=\"4\" ry=\"4\" fill=\"" + color +
        (overflow ? "\" fill-opacity=\"0.6\" stroke=\"" : "\" fill-opacity=\"0.95\" stroke=\"") +
        theme.boundary_color + "\"" + (overflow ? " stroke-dasharray=\"3 2\"" : "") +
        "/><text class=\"cynefinItemText\" x=\"" + number(width / 2) +
        "\" y=\"13\" text-anchor=\"middle\" dominant-baseline=\"central\" font-size=\"" +
        number(theme.item_font_size) + "\" fill=\"" + theme.text_color + "\">" +
        escape_text(label) + "</text></g>";
    return {};
}

std::expected<void, std::string> append_item_badges(
    std::string& svg, const CynefinDocument& document, const CynefinConfig& config,
    const CynefinTheme& theme, const std::array<DomainBox, 5>& boxes,
    const std::array<std::string, 5>& colors, const SvgTextMeasure& measure) {
    svg += "<g class=\"cynefin-items\">";
    constexpr std::array<CynefinDomainName, 5> order{
        CynefinDomainName::complex, CynefinDomainName::complicated,
        CynefinDomainName::chaotic, CynefinDomainName::clear,
        CynefinDomainName::confusion};
    for (const auto name : order) {
        const auto found = std::ranges::find(document.domains, name, &CynefinDomain::name);
        if (found == document.domains.end()) continue;
        const auto& domain = *found;
        const bool confusion = domain.name == CynefinDomainName::confusion;
        const auto& box = boxes[domain_index(domain.name)];
        const std::size_t rendered_count = confusion
            ? std::min<std::size_t>(3, domain.items.size()) : domain.items.size();
        const double y0 = box.center_y + (confusion
            ? (config.show_domain_descriptions ? 22 : 14)
            : (config.show_domain_descriptions ? 25 : 15));
        for (std::size_t i = 0; i < rendered_count; ++i) {
            auto badge = append_item_badge(svg, domain.items[i].label, box.center_x,
                y0 + i * 30, theme, colors[domain_index(domain.name)], false, measure);
            if (!badge) return std::unexpected(badge.error());
        }
        if (confusion && domain.items.size() > rendered_count) {
            auto label = overflow_label(domain.items.size() - rendered_count);
            auto badge = append_item_badge(svg, label, box.center_x,
                y0 + rendered_count * 30, theme, colors[domain_index(domain.name)], true, measure);
            if (!badge) return std::unexpected(badge.error());
        }
    }
    svg += "</g>";
    return {};
}

std::expected<void, std::string> append_transitions(
    std::string& svg, const CynefinDocument& document, const CynefinConfig& config,
    const CynefinTheme& theme, const std::array<DomainBox, 5>& boxes,
    const SvgTextMeasure& measure) {
    if (document.transitions.empty()) return {};
    svg += "<defs><marker id=\"cynefin-arrow\" viewBox=\"0 0 10 10\" refX=\"9\" refY=\"5\" markerWidth=\"6\" markerHeight=\"6\" orient=\"auto-start-reverse\"><path d=\"M 0 0 L 10 5 L 0 10 z\" class=\"cynefinArrowHead\" fill=\"" +
        theme.arrow_color + "\"/></marker></defs><g class=\"cynefin-arrows\">";
    for (const auto& transition : document.transitions) {
        const auto& from = boxes[domain_index(transition.from)];
        const auto& to = boxes[domain_index(transition.to)];
        const double dx = to.center_x - from.center_x;
        const double dy = to.center_y - from.center_y;
        const double length = std::hypot(dx, dy);
        const double control_x = (from.center_x + to.center_x) / 2 - dy * .15;
        const double control_y = (from.center_y + to.center_y) / 2 + dx * .15;
        svg += "<path class=\"cynefinArrowLine\" d=\"M" + number(from.center_x) + "," +
            number(from.center_y) + " Q" + number(control_x) + "," + number(control_y) + " " +
            number(to.center_x) + "," + number(to.center_y) + "\" fill=\"none\" stroke=\"" +
            theme.arrow_color + "\" stroke-width=\"" + number(theme.arrow_width) +
            "\" marker-end=\"url(#cynefin-arrow)\"/>";
        if (transition.label) {
            svg += text_element(*transition.label, control_x, control_y - 6,
                theme.item_font_size - 1, 400, "cynefinArrowLabel", theme.text_color, "auto", measure);
        }
    }
    svg += "</g>";
    return {};
}
}

std::expected<CynefinRenderInput, std::string> prepare_cynefin_render(const PreparedDiagram& prepared) {
    auto document = parse_cynefin(prepared.code.cleaned);
    if (!document) return std::unexpected(document.error());

    CynefinRenderInput result;
    result.document = std::move(*document);
    if (result.document.title.empty() && prepared.title) result.document.title = *prepared.title;
    const auto& config = member(prepared.effective_config, u"cynefin");
    const auto& variables = member(prepared.effective_config, u"themeVariables");
    const auto& theme_config = member(variables, u"cynefin");
    auto& diagram_config = result.config;
    diagram_config.width = number_value(member(config, u"width"), 800);
    diagram_config.height = number_value(member(config, u"height"), 600);
    diagram_config.padding = number_value(member(config, u"padding"), 40);
    diagram_config.boundary_amplitude = number_value(member(config, u"boundaryAmplitude"), 8);
    diagram_config.show_domain_descriptions = boolean_value(member(config, u"showDomainDescriptions"), true);
    diagram_config.seed = static_cast<unsigned>(number_value(member(config, u"seed"), 0));
    diagram_config.use_max_width = boolean_value(member(config, u"useMaxWidth"), true);

    auto& theme = result.theme;
    theme.font_family = string_value(member(prepared.effective_config, u"fontFamily"), u"Arial");
    const auto assign_color = [&](std::u16string_view key, std::string& target) {
        const auto value = member(theme_config, key);
        if (value.kind == ConfigKind::string) target = escape_text(value.string);
    };
    assign_color(u"complexBg", theme.complex_bg);
    assign_color(u"complicatedBg", theme.complicated_bg);
    assign_color(u"clearBg", theme.clear_bg);
    assign_color(u"chaoticBg", theme.chaotic_bg);
    assign_color(u"confusionBg", theme.confusion_bg);
    assign_color(u"boundaryColor", theme.boundary_color);
    assign_color(u"cliffColor", theme.cliff_color);
    assign_color(u"arrowColor", theme.arrow_color);
    assign_color(u"textColor", theme.text_color);
    assign_color(u"labelColor", theme.label_color);
    theme.boundary_width = number_value(member(theme_config, u"boundaryWidth"), 2);
    theme.cliff_width = number_value(member(theme_config, u"cliffWidth"), 4);
    theme.arrow_width = number_value(member(theme_config, u"arrowWidth"), 2);
    theme.domain_font_size = number_value(member(theme_config, u"domainFontSize"), 16);
    theme.item_font_size = number_value(member(theme_config, u"itemFontSize"), 12);
    if (diagram_config.width <= 0 || diagram_config.height <= 0 || diagram_config.padding < 0) {
        return std::unexpected("Invalid Cynefin dimensions");
    }
    return result;
}

std::expected<CynefinSvg, std::string> render_cynefin_svg(
    const CynefinRenderInput& input, const SvgTextMeasure& measure) {
    if (!measure) return std::unexpected("Cynefin rendering requires measured text");
    try {
        const auto& config = input.config;
        const auto& theme = input.theme;
        const double total_width = config.width + 2 * config.padding;
        const double total_height = config.height + 2 * config.padding;
        const auto boxes = domain_boxes(config.width, config.height);
        const auto colors = domain_colors(theme);
        std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"cynefin\"" +
            attribute("viewBox", "0 0 " + number(total_width) + " " + number(total_height)) +
            attribute("font-family", theme.font_family) +
            attribute("font-size", number(theme.domain_font_size));
        svg += config.use_max_width
            ? attribute("width", "100%") + attribute("style", "max-width:" + number(total_width) + "px")
            : attribute("width", number(total_width)) + attribute("height", number(total_height));
        if (!input.document.acc_title.empty()) svg += attribute("aria-labelledby", "cynefin-title");
        if (!input.document.acc_description.empty()) svg += attribute("aria-describedby", "cynefin-description");
        svg += ">";
        if (!input.document.acc_title.empty()) svg += "<title id=\"cynefin-title\">" + escape_text(input.document.acc_title) + "</title>";
        if (!input.document.acc_description.empty()) svg += "<desc id=\"cynefin-description\">" + escape_text(input.document.acc_description) + "</desc>";
        svg += "<g transform=\"translate(" + number(config.padding) + ", " + number(config.padding) + ")\">";
        append_domain_backgrounds(svg, boxes, colors);
        append_boundaries(svg, config, theme);
        append_confusion_shape(svg, config.width, config.height,
            colors[domain_index(CynefinDomainName::confusion)], theme);
        if (auto labels = append_domain_labels(svg, config, theme, boxes, measure); !labels) return std::unexpected(labels.error());
        if (auto badges = append_item_badges(svg, input.document, config, theme, boxes, colors, measure); !badges) return std::unexpected(badges.error());
        if (auto arrows = append_transitions(svg, input.document, config, theme, boxes, measure); !arrows) return std::unexpected(arrows.error());
        if (!input.document.title.empty()) {
            svg += text_element(input.document.title, config.width / 2, -config.padding / 2,
                theme.domain_font_size + 2, 700, "cynefinTitle", theme.label_color, "middle", measure);
        }
        svg += "</g></svg>";
        return CynefinSvg{std::move(svg), total_width, total_height};
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}
}
