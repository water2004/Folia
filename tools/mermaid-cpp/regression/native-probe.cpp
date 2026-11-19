// Newline-delimited JSON driver for the same pipeline used by the application.
// JSON input is ASCII with escaped Unicode; output SVG remains UTF-8.
import std;
import folia.mermaid;
import folia.mermaid.directives;
#ifdef _WIN32
import folia.platform.mermaid_measurer;
#endif
#ifdef FOLIA_REGRESSION_NATIVE_MATH
import folia.platform.mermaid_math;
#endif

namespace {
using namespace folia::mermaid;

std::string quote(std::string_view value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result = "\"";
    for (const auto c : value) {
        const auto byte = static_cast<unsigned char>(c);
        if (c == '"' || c == '\\') { result += '\\'; result += c; }
        else if (byte < 32) { result += "\\u00"; result += digits[byte >> 4]; result += digits[byte & 15]; }
        else result += c;
    }
    return result + '"';
}
std::string quote(std::u16string_view value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result = "\"";
    for (const auto c : value) {
        result += "\\u";
        for (int shift = 12; shift >= 0; shift -= 4) result += digits[(c >> shift) & 15];
    }
    return result + '"';
}
const ConfigValue& field(const ConfigValue& value, std::u16string_view key) {
    const auto* found = value.find(key);
    if (!found) throw std::runtime_error("Missing probe protocol field");
    return *found;
}
std::string font_json(const MermaidFont& font) {
    return "{\"family\":" + quote(font.family) + ",\"size\":" + std::to_string(font.size) +
        ",\"weight\":" + std::to_string(font.weight) + ",\"italic\":" +
        (font.italic ? "true" : "false") + ",\"pair_kerning\":" +
        (font.pair_kerning ? "true" : "false") + "}";
}
std::string styles_json(std::span<const MermaidTextStyle> styles) {
    std::string result = "[";
    for (const auto& style : styles) {
        if (result.size() > 1) result += ',';
        result += "[" + std::to_string(style.offset) + ',' + std::to_string(style.length) + ',' +
            std::to_string(style.weight) + ',' + (style.italic ? "true" : "false") + ']';
    }
    return result + ']';
}
std::expected<ConfigValue, std::string> read_record() {
    std::string line;
    if (!std::getline(std::cin, line)) return std::unexpected("Probe input closed");
    if (line.size() > 16 * 1024 * 1024 ||
        std::ranges::any_of(line, [](unsigned char c) { return c > 127; }))
        return std::unexpected("Probe input must be bounded ASCII JSON with escaped Unicode");
    return parse_json_value(std::u16string(line.begin(), line.end()),
        {.source_units = 16 * 1024 * 1024, .values = 100000, .string_units = 16 * 1024 * 1024});
}

// Test transport only. The other end uses a real font shaper or a browser.
// Measurements are requests for text/font/style, never fixture coordinates.
class RemoteMeasurer final : public TextMeasurer {
    std::expected<ConfigValue, std::string> request(std::string_view method, std::string args) const {
        std::cout << "{\"kind\":\"measure\",\"method\":" << quote(method) << ',' << args << "}\n" << std::flush;
        auto result = read_record();
        if (!result) return std::unexpected(result.error());
        if (const auto* error = result->find(u"error"))
            return std::unexpected(std::string(error->string.begin(), error->string.end()));
        return result;
    }
public:
    std::expected<std::u16string, std::string> resolve_family(std::u16string_view family,
        MermaidFontContext context) const override {
        auto result = request("family", "\"family\":" + quote(family) +
            ",\"svg\":" + (context == MermaidFontContext::svg_label ? "true" : "false"));
        if (!result) return std::unexpected(result.error());
        return field(*result, u"family").string;
    }
    std::expected<MermaidTextMetrics, std::string> measure(std::u16string_view text,
        const MermaidFont& font, bool preserve, std::span<const MermaidTextStyle> styles,
        bool svg_bounds) const override {
        auto result = request("text", "\"text\":" + quote(text) + ",\"font\":" + font_json(font) +
            ",\"preserve\":" + (preserve ? "true" : "false") + ",\"svg\":" +
            (svg_bounds ? "true" : "false") + ",\"styles\":" + styles_json(styles));
        if (!result) return std::unexpected(result.error());
        return MermaidTextMetrics{field(*result, u"advance").number, field(*result, u"ink_width").number,
            field(*result, u"bounds_width").number, field(*result, u"line_height").number,
            field(*result, u"baseline").number};
    }
    std::expected<std::vector<std::u16string>, std::string> wrap(std::u16string_view text,
        const MermaidFont& font, double width, std::span<const MermaidTextStyle> styles,
        bool break_spaces) const override {
        auto result = request("wrap", "\"text\":" + quote(text) + ",\"font\":" + font_json(font) +
            ",\"width\":" + (std::isfinite(width) ? std::to_string(width) : "null") +
            ",\"break_spaces\":" + (break_spaces ? "true" : "false") + ",\"styles\":" + styles_json(styles));
        if (!result) return std::unexpected(result.error());
        std::vector<std::u16string> lines;
        for (const auto& line : field(*result, u"lines").sequence) lines.push_back(line.string);
        return lines;
    }
    std::expected<MermaidSvgFontMetrics, std::string> svg_font_metrics(const MermaidFont& font) const override {
        auto result = request("font", "\"font\":" + font_json(font));
        if (!result) return std::unexpected(result.error());
        return MermaidSvgFontMetrics{field(*result, u"height").number, field(*result, u"baseline").number,
            field(*result, u"middle_offset").number};
    }
};
}

int main(int argc, char** argv) {
    std::cout << std::setprecision(17);
    const bool remote = argc == 2 && std::string_view{argv[1]} == "--remote-measurement";
    if (argc > 2 || (argc == 2 && !remote)) {
        std::cerr << "Usage: FoliaMermaidRegressionProbe [--remote-measurement]\n";
        return 2;
    }
    RemoteMeasurer remote_measurer;
#ifdef _WIN32
    folia::platform::MermaidMeasurer windows_measurer;
    const TextMeasurer& measurer = remote ? static_cast<const TextMeasurer&>(remote_measurer)
                                         : static_cast<const TextMeasurer&>(windows_measurer);
#else
    const TextMeasurer& measurer = remote_measurer;
#endif
    while (std::cin.peek() != std::char_traits<char>::eof()) {
        auto request = read_record();
        if (!request) { std::cerr << request.error() << '\n'; return 2; }
        try {
            const auto& source = field(*request, u"source").string;
            auto session = ConfigSession::create();
            if (!session) throw std::runtime_error(session.error());
            if (const auto* config = request->find(u"config")) {
                auto initialized = session->initialize(*config);
                if (!initialized) throw std::runtime_error(initialized.error());
            }
            if (const auto* prepare = request->find(u"prepare"); prepare && prepare->boolean) {
                auto input = prepare_native_diagram(source, *session);
                if (!input) throw std::runtime_error(input.error());
                std::cout << "{\"kind\":\"result\",\"ok\":true,\"stage\":\"prepare\"}\n" << std::flush;
                continue;
            }
            MathLabelRender math;
#ifdef FOLIA_REGRESSION_NATIVE_MATH
            folia::platform::MermaidMathRenderer math_renderer;
            math = [&](std::u16string_view tex, double size) { return math_renderer.render(tex, size); };
#endif
            std::optional<double> available_width;
            if (const auto* width = request->find(u"availableWidth")) available_width = width->number;
            auto result = render_diagram(source, measurer, *session, math, {}, available_width);
            if (!result) throw std::runtime_error(result.error());
            std::cout << "{\"kind\":\"result\",\"ok\":true,\"width\":" << result->width <<
                ",\"height\":" << result->height << ",\"svg\":" << quote(result->svg) << "}\n" << std::flush;
        } catch (const std::exception& error) {
            std::cout << "{\"kind\":\"result\",\"ok\":false,\"error\":" << quote(error.what()) << "}\n" << std::flush;
        }
    }
}
