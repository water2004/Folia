#include "support/folia_test.hpp"

import folia.mermaid.packet_svg;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

std::vector<std::string_view> fields_of(std::string_view input, char separator) {
    std::vector<std::string_view> result;
    for (;;) {
        auto const end = input.find(separator);
        result.push_back(input.substr(0, end));
        if (end == std::string_view::npos) return result;
        input.remove_prefix(end + 1);
    }
}

unsigned int hex_value(std::string_view value) {
    unsigned int result{};
    auto const parsed = std::from_chars(value.data(), value.data() + value.size(), result, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        throw std::runtime_error("Malformed Packet SVG hexadecimal fixture field");
    }
    return result;
}

std::u16string utf16_field(std::string_view value) {
    if (value == "-") return {};
    if (value.size() % 4) throw std::runtime_error("Malformed Packet SVG UTF-16 fixture field");
    std::u16string result;
    for (std::size_t index = 0; index < value.size(); index += 4) {
        result.push_back(static_cast<char16_t>(hex_value(value.substr(index, 4))));
    }
    return result;
}

std::string utf8_field(std::string_view value) {
    if (value.size() % 2) throw std::runtime_error("Malformed Packet SVG UTF-8 fixture field");
    std::string result;
    for (std::size_t index = 0; index < value.size(); index += 2) {
        result.push_back(static_cast<char>(hex_value(value.substr(index, 2))));
    }
    return result;
}

double number_field(std::string_view value) {
    double result{};
    auto const parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        throw std::runtime_error("Malformed Packet SVG numeric fixture field");
    }
    return result;
}

PacketConfig config_field(std::string_view value) {
    auto const fields = fields_of(value, ',');
    if (fields.size() != 8) throw std::runtime_error("Malformed Packet SVG configuration fixture");
    PacketConfig config;
    config.row_height = number_field(fields[0]);
    config.padding_x = number_field(fields[1]);
    config.padding_y = number_field(fields[2]);
    config.bit_width = number_field(fields[3]);
    config.bits_per_row = static_cast<std::uint32_t>(number_field(fields[4]));
    config.show_bits = fields[5] == "1";
    config.use_max_width = fields[6] == "1";
    config.descending = fields[7] == "1";
    return config;
}

void compare_words(PacketLayout const& layout, std::string_view expected, std::string_view name) {
    if (expected == "-") {
        expect(layout.words.empty()) << name;
        return;
    }
    auto const rows = fields_of(expected, '/');
    expect(fatal(layout.words.size() == rows.size())) << name << " row count";
    for (std::size_t row = 0; row < rows.size(); ++row) {
        auto const blocks = fields_of(rows[row], ';');
        expect(fatal(layout.words[row].size() == blocks.size())) << name << " block count";
        for (std::size_t block = 0; block < blocks.size(); ++block) {
            auto const values = fields_of(blocks[block], ',');
            expect(fatal(values.size() == 4_u));
            auto const& actual = layout.words[row][block];
            expect(actual.start == number_field(values[0])) << name << " start";
            expect(actual.end == number_field(values[1])) << name << " end";
            expect(actual.bits == number_field(values[2])) << name << " preserved bits field";
            expect(bool(actual.label == utf16_field(values[3]))) << name << " label";
        }
    }
}

std::string without_packet_style(std::string svg, std::string_view expected_style) {
    auto const begin = svg.find("<style>");
    auto const end = svg.find("</style>", begin);
    if (begin == std::string::npos || end == std::string::npos) {
        throw std::runtime_error("Native Packet SVG did not contain its default style");
    }
    expect(svg.substr(begin + 7, end - begin - 7) == expected_style) << "official default Packet CSS";
    svg.erase(begin, end + 8 - begin);
    return svg;
}

PacketLayout one_field_layout() {
    auto parsed = parse_packet(u"packet\n+8: \"body\"");
    if (!parsed) throw std::runtime_error("Native Packet fixture failed to parse");
    auto layout = layout_packet(*parsed);
    if (!layout) throw std::runtime_error("Native Packet fixture failed to lay out");
    return std::move(*layout);
}

suite packet_svg = [] {
    "mermaid_packet_svg_matches_official_renderer_and_db"_test = [] {
        auto const fixture_override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{
            fixture_override ? fixture_override : FOLIA_MERMAID_FIXTURE_DIR} / "packet-svg.tsv"};
        expect(fatal(input.is_open()));
        std::string line;
        std::string expected_style;
        std::size_t count{};
        std::size_t renderer_data_count{};
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            constexpr std::string_view style_prefix = "# default-style-utf8-hex\t";
            if (line.starts_with(style_prefix)) {
                expected_style = utf8_field(std::string_view(line).substr(style_prefix.size()));
                continue;
            }
            if (line.empty() || line.front() == '#') continue;
            expect(fatal(!expected_style.empty()));
            auto const fields = fields_of(line, '\t');
            expect(fatal(fields.size() == 10_u));
            auto const name = fields[0];
            expect(fields[1] == "renderer-data" || fields[1] == "raw-parser-renderer") << name;
            auto document = parse_packet(utf16_field(fields[2]));
            expect(fatal(document.has_value())) << name << " parse";
            if (fields[4] != "~") {
                for (auto& block : document->blocks) block.label = utf16_field(fields[4]);
            }
            if (fields[5] != "~") document->title = utf16_field(fields[5]);
            auto layout = layout_packet(*document, config_field(fields[3]));
            expect(fatal(layout.has_value())) << name << " layout";
            compare_words(*layout, fields[8], name);
            auto rendered = render_packet_svg(*layout);
            expect(fatal(rendered.has_value())) << name << " render";
            expect(rendered->width == number_field(fields[6])) << name << " width";
            expect(rendered->height == number_field(fields[7])) << name << " height";
            expect(without_packet_style(rendered->svg, expected_style) == utf8_field(fields[9])) << name << " complete SVG";
            ++count;
            renderer_data_count += fields[1] == "renderer-data";
        }
        expect(count == 55_u) << "every official SVG observation must execute";
        expect(renderer_data_count == 16_u) << "renderer-stage text overrides remain identified";
    };

    "mermaid_packet_svg_rejects_invalid_utf16_and_xml_characters"_test = [] {
        for (auto const character : {0x0000, 0x0001, 0x000b, 0x001f, 0xd800, 0xdc00, 0xfffe, 0xffff}) {
            auto layout = one_field_layout();
            layout.document.blocks.front().label = std::u16string(1, static_cast<char16_t>(character));
            auto label = render_packet_svg(layout);
            expect(!label.has_value()) << character << " invalid label";
            if (!label) expect(!label.error().message.empty());
            layout.document.blocks.front().label = u"valid";
            layout.document.title = std::u16string(1, static_cast<char16_t>(character));
            auto title = render_packet_svg(layout);
            expect(!title.has_value()) << character << " invalid title";
        }
    };

    "mermaid_packet_svg_rebuilds_untrusted_cached_words"_test = [] {
        auto layout = one_field_layout();
        auto expected = render_packet_svg(layout);
        expect(fatal(expected.has_value()));
        layout.words.front().front().start = std::numeric_limits<double>::quiet_NaN();
        layout.words.front().front().end = -100;
        layout.words.front().front().label = std::u16string(1, static_cast<char16_t>(0xd800));
        layout.words.push_back({});
        auto actual = render_packet_svg(layout);
        expect(fatal(actual.has_value()));
        expect(actual->svg == expected->svg);
    };

    "mermaid_packet_svg_rejects_invalid_or_overflowing_geometry"_test = [] {
        auto layout = one_field_layout();
        layout.config.bit_width = std::numeric_limits<double>::infinity();
        expect(!render_packet_svg(layout).has_value());
        layout = one_field_layout();
        layout.config.bit_width = std::numeric_limits<double>::max();
        expect(!render_packet_svg(layout).has_value());
        layout = one_field_layout();
        layout.config.row_height = std::numeric_limits<double>::max();
        expect(!render_packet_svg(layout).has_value());
        layout = one_field_layout();
        layout.config.padding_x = 257;
        expect(!render_packet_svg(layout).has_value());
        layout = one_field_layout();
        layout.config.bits_per_row = 0;
        expect(!render_packet_svg(layout).has_value());
    };
};
}
