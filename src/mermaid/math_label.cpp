module folia.mermaid.math_label;
import folia.mermaid.html_named_reference;
import folia.mermaid.number;
import folia.mermaid.svg_text;
import std;

namespace folia::mermaid {
namespace {
bool line_terminator(char16_t c) {
    return c == u'\n' || c == u'\r' || c == u'\u2028' || c == u'\u2029';
}
std::optional<std::size_t> formula_end(std::u16string_view text, std::size_t start) {
    if (!text.substr(start).starts_with(u"$$")) return std::nullopt;
    for (auto i = start + 2; i < text.size(); ++i) {
        if (line_terminator(text[i])) return std::nullopt;
        if (text[i] == u'$' && i + 1 < text.size() && text[i + 1] == u'$') return i;
    }
    return std::nullopt;
}
bool valid_dimension(double value) { return std::isfinite(value) && value >= 0; }
bool valid_metrics(double width, double height, double baseline) {
    return valid_dimension(width) && valid_dimension(height) && std::isfinite(baseline);
}
std::string number(double value) {
    auto result = format_number(value);
    if (!result) throw std::runtime_error("Invalid mathematical label coordinate");
    return *result;
}
std::string xml(std::u16string_view text) {
    auto result = escape_svg_text(text, u"Mermaid mathematical label");
    if (!result) throw std::runtime_error("Invalid mathematical label text");
    std::string escaped;
    for (const auto c : *result) {
        if (c == '"') escaped += "&quot;";
        else escaped += c;
    }
    return escaped;
}
std::string attribute(std::string_view text) {
    std::string escaped;
    for (const auto c : text) {
        switch (c) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        default: escaped += c;
        }
    }
    return escaped;
}
std::u16string entity(std::u16string_view name) {
    const bool numeric = name.starts_with(u'#') ||
        (!name.empty() && name.front() >= u'0' && name.front() <= u'9');
    if (!numeric) {
        if (auto result = mermaid_html_named_reference(name)) return std::move(*result);
        return u"&" + std::u16string{name} + u";";
    }
    if (name.starts_with(u'#')) name.remove_prefix(1);
    int base = 10;
    if (name.starts_with(u'x') || name.starts_with(u'X')) {
        base = 16;
        name.remove_prefix(1);
    }
    const std::string digits(name.begin(), name.end());
    std::uint32_t codepoint = 0;
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), codepoint, base);
    if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size() ||
        codepoint == 0 || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff))
        throw std::runtime_error("Invalid mathematical label character entity");
    if (codepoint < 0x10000) return std::u16string(1, static_cast<char16_t>(codepoint));
    codepoint -= 0x10000;
    return {static_cast<char16_t>(0xd800 + (codepoint >> 10)),
            static_cast<char16_t>(0xdc00 + (codepoint & 1023))};
}
unsigned bolder(unsigned weight) { return weight < 350 ? 400 : weight < 550 ? 700 : 900; }
}

bool has_math_label(std::u16string_view text) {
    for (auto start = text.find(u"$$"); start != text.npos; start = text.find(u"$$", start + 2))
        if (formula_end(text, start)) return true;
    return false;
}

std::expected<MathLabel, std::string> measure_math_label(
    std::u16string_view text, const MathLabelOptions& options,
    const MathLabelTextMeasure& measure_text, const MathLabelRender& render_math) {
    try {
        if (!measure_text || !render_math)
            return std::unexpected("Mathematical labels require text and math measurement providers");
        if (!std::isfinite(options.font_size) || options.font_size <= 0 ||
            !valid_dimension(options.line_height))
            return std::unexpected("Invalid mathematical label font metrics");
        std::u16string normalized;
        if (options.parse_policy == MathLabelParsePolicy::create_text) {
            normalized.reserve(text.size());
            for (std::size_t i = 0; i < text.size(); ++i) {
                normalized += text[i];
                if (text[i] == u'\\' && i + 1 < text.size() && text[i + 1] == u'\\') ++i;
            }
            text = normalized;
        }
        MathLabel label;
        label.font_size = options.font_size;
        label.lines.emplace_back();
        std::vector<std::u16string> tags;
        std::u16string pending;
        unsigned weight = options.font_weight;
        bool italic = options.font_italic;
        bool pending_space = false;
        bool inline_text = false, inline_ends_space = false;
        auto append_text = [&](std::u16string_view value) {
            for (const auto c : value) {
                if (c == u' ' || c == u'\t' || c == u'\r' || c == u'\n' || c == u'\f') {
                    pending_space = !pending.empty() || (inline_text && !inline_ends_space);
                } else {
                    if (pending_space) pending += u' ';
                    pending_space = false;
                    pending += c;
                }
            }
        };
        auto flush_text = [&](bool preserve_space = false) {
            if (preserve_space && pending_space) pending += u' ';
            // Anonymous flex items collapse leading/trailing white space.
            if (pending.empty()) { pending_space = false; return; }
            auto metrics = measure_text(pending, weight, italic);
            if (!metrics) throw std::runtime_error(metrics.error());
            if (!valid_metrics(metrics->width, metrics->height, metrics->baseline))
                throw std::runtime_error("Invalid mathematical label text measurement");
            auto& line = label.lines.back();
            if (!tags.empty()) {
                inline_text = true;
                inline_ends_space = pending.back() == u' ';
            }
            line.atoms.push_back({std::move(pending), line.width, 0, metrics->width,
                                  metrics->height, metrics->baseline, weight, italic});
            line.width += metrics->width;
            line.height = std::max(line.height, std::max(metrics->height, options.line_height));
            pending.clear();
            pending_space = false;
        };
        for (std::size_t i = 0; i < text.size();) {
            const auto rest = text.substr(i);
            if (auto end = formula_end(text, i)) {
                flush_text();
                auto math = render_math(text.substr(i + 2, *end - i - 2), options.font_size);
                if (!math) return std::unexpected(math.error());
                if (math->svg.empty() || !valid_metrics(math->width, math->height, math->baseline))
                    return std::unexpected("Invalid mathematical label SVG measurement");
                auto& line = label.lines.back();
                const auto width = math->width, height = math->height, baseline = math->baseline;
                line.atoms.push_back({std::move(*math), line.width, 0, width, height, baseline, weight, italic});
                line.width += width;
                line.height = std::max(line.height, height);
                inline_text = inline_ends_space = false;
                i = *end + 2;
                continue;
            }
            if (rest.starts_with(u'<')) {
                const auto end = rest.find(u'>');
                if (end != rest.npos) {
                    std::u16string tag;
                    for (const auto c : rest.substr(1, end - 1))
                        if (c != u' ' && c != u'\t' && c != u'/')
                            tag += c >= u'A' && c <= u'Z' ? c + u'a' - u'A' : c;
                    if (tag == u"br") {
                        flush_text();
                        label.lines.emplace_back();
                        inline_text = inline_ends_space = false;
                        i += end + 1;
                        continue;
                    }
                    if (tag == u"b" || tag == u"strong" || tag == u"i" || tag == u"em") {
                        const bool closing = rest[1] == u'/';
                        const bool item_boundary = tags.empty() || (closing && tags.size() == 1);
                        // Nested emphasis stays inside the same flex item, so
                        // its inline whitespace is retained across font runs.
                        flush_text(!item_boundary);
                        if (item_boundary) inline_text = inline_ends_space = false;
                        if (closing) {
                            if (tags.empty() || tags.back() != tag)
                                return std::unexpected("Unbalanced mathematical label formatting");
                            tags.pop_back();
                        } else tags.push_back(tag);
                        weight = std::ranges::any_of(tags, [](const auto& t) { return t == u"b" || t == u"strong"; })
                            ? bolder(options.font_weight) : options.font_weight;
                        italic = options.font_italic || std::ranges::any_of(tags, [](const auto& t) { return t == u"i" || t == u"em"; });
                        i += end + 1;
                        continue;
                    }
                    return std::unexpected("Unsupported mathematical label HTML tag");
                }
            }
            if (rest.starts_with(u'&') || rest.starts_with(u'#')) {
                const auto end = rest.find(u';');
                if (end != rest.npos && rest.substr(1, end - 1).find_first_of(u" \t\n&<") == rest.npos) {
                    append_text(entity(rest.substr(1, end - 1)));
                    i += end + 1;
                    continue;
                }
            }
            append_text(rest.substr(0, 1));
            ++i;
        }
        flush_text();
        if (!tags.empty()) return std::unexpected("Unclosed mathematical label formatting");
        for (auto& line : label.lines) {
            line.height = std::max(line.height, options.line_height);
            line.y = label.height;
            for (auto& atom : line.atoms) atom.y = (line.height - atom.height) / 2;
            label.width = std::max(label.width, line.width);
            label.height += line.height;
        }
        return label;
    } catch (const std::exception& e) { return std::unexpected(e.what()); }
}

std::expected<std::string, std::string> render_math_label_svg(
    const MathLabel& label, double left, double top, const MathLabelSvgStyle& style) {
    try {
        const auto container_width=style.container_width.value_or(label.width);
        if (!valid_dimension(label.width) || !valid_dimension(label.height) ||
            !valid_dimension(container_width) || !std::isfinite(left) || !std::isfinite(top))
            return std::unexpected("Invalid mathematical label box");
        const auto color = attribute(style.color.empty() ? std::string_view{"currentColor"} : style.color);
        std::string svg = "<g class=\"" + attribute(style.css_class) + "\" color=\"" + color +
            "\" fill=\"" + color + "\" stroke=\"none\" font-family=\"" + xml(style.font_family) +
            "\" font-size=\"" + number(label.font_size) + "\">";
        for (const auto& line : label.lines) {
            const auto x = left + (container_width - line.width) / 2;
            for (const auto& atom : line.atoms) {
                const auto ax = x + atom.x, ay = top + line.y + atom.y;
                if (const auto* text = std::get_if<std::u16string>(&atom.content)) {
                    svg += "<text xml:space=\"preserve\" text-anchor=\"start\" x=\"" + number(ax) + "\" y=\"" +
                        number(ay + atom.baseline) + "\" font-weight=\"" + std::to_string(atom.weight) +
                        "\" font-style=\"" + (atom.italic ? "italic" : "normal") + "\">" + xml(*text) + "</text>";
                } else {
                    svg += "<g transform=\"translate(" + number(ax) + "," + number(ay) + ")\">" +
                        std::get<MathFragment>(atom.content).svg + "</g>";
                }
            }
        }
        return svg + "</g>";
    } catch (const std::exception& e) { return std::unexpected(e.what()); }
}
}
