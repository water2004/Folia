// Ported from Mermaid's packet/renderer.ts, packet/styles.ts, and
// setupGraphViewbox.js at commit 1fb7869866dd619168d840291c9c54b4f129758b.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT licensed; see ../LICENSE.
module folia.mermaid.packet_svg;
import std;
import folia.mermaid.number;

namespace folia::mermaid {
namespace {

std::unexpected<PacketError> render_error(std::u16string message) {
    return std::unexpected(PacketError{0, std::move(message)});
}

std::u16string text_error_message(
    std::u16string_view field, std::u16string_view reason, std::size_t offset) {
    auto const digits = std::to_string(offset);
    return std::u16string(field) + u" " + std::u16string(reason) + u" at UTF-16 offset " +
        std::u16string(digits.begin(), digits.end()) + u".";
}

std::expected<std::string, PacketError> xml_text(
    std::u16string_view text, std::u16string_view field) {
    std::string result;
    for (std::size_t index = 0; index < text.size(); ++index) {
        auto const start = index;
        auto scalar = static_cast<std::uint32_t>(text[index]);
        if (scalar >= 0xd800 && scalar <= 0xdbff) {
            if (index + 1 == text.size() || text[index + 1] < 0xdc00 || text[index + 1] > 0xdfff) {
                return render_error(text_error_message(field, u"contains an unpaired high surrogate", start));
            }
            scalar = 0x10000 + ((scalar - 0xd800) << 10) + (text[++index] - 0xdc00);
        } else if (scalar >= 0xdc00 && scalar <= 0xdfff) {
            return render_error(text_error_message(field, u"contains an unpaired low surrogate", start));
        }

        bool const xml_character = scalar == 0x9 || scalar == 0xa || scalar == 0xd ||
            (scalar >= 0x20 && scalar <= 0xd7ff) ||
            (scalar >= 0xe000 && scalar <= 0xfffd) ||
            (scalar >= 0x10000 && scalar <= 0x10ffff);
        if (!xml_character) {
            return render_error(text_error_message(field, u"contains a character forbidden by XML 1.0", start));
        }
        switch (scalar) {
        case '&': result += "&amp;"; continue;
        case '<': result += "&lt;"; continue;
        case '>': result += "&gt;"; continue;
        // A literal CR is normalized during XML parsing; a character reference
        // preserves the text that the official renderer puts in its DOM node.
        case '\r': result += "&#13;"; continue;
        default: break;
        }
        if (scalar <= 0x7f) {
            result.push_back(static_cast<char>(scalar));
        } else if (scalar <= 0x7ff) {
            result.push_back(static_cast<char>(0xc0 | (scalar >> 6)));
            result.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
        } else if (scalar <= 0xffff) {
            result.push_back(static_cast<char>(0xe0 | (scalar >> 12)));
            result.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
        } else {
            result.push_back(static_cast<char>(0xf0 | (scalar >> 18)));
            result.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
        }
    }
    return result;
}

class SvgWriter {
public:
    void append(std::string_view text) {
        if (!error_) {
            text_.append(text);
        }
    }

    void number(double value) {
        if (error_) {
            return;
        }
        auto converted = format_number(value);
        if (!converted) {
            error_ = PacketError{0, u"Packet SVG number must be finite and representable."};
            return;
        }
        text_ += *converted;
    }

    void numeric_attribute(std::string_view name, double value) {
        append(" ");
        append(name);
        append("=\"");
        number(value);
        append("\"");
    }

    std::expected<PacketSvg, PacketError> finish(double width, double height) && {
        if (error_) {
            return std::unexpected(std::move(*error_));
        }
        return PacketSvg{std::move(text_), width, height};
    }

private:
    std::string text_;
    std::optional<PacketError> error_;
};

bool finite_coordinates(std::initializer_list<double> values) {
    return std::ranges::all_of(values, [](double value) { return std::isfinite(value); });
}

constexpr std::string_view default_packet_style =
    "<style>"
    ".packetByte{font-size:10px;}"
    ".packetByte.start{fill:black;}"
    ".packetByte.end{fill:black;}"
    ".packetLabel{fill:black;font-size:12px;}"
    ".packetTitle{fill:black;font-size:14px;}"
    ".packetBlock{stroke:black;stroke-width:1;fill:#efefef;}"
    "</style>";

}

std::expected<PacketSvg, PacketError> render_packet_svg(PacketLayout const& supplied) {
    auto rebuilt = layout_packet(supplied.document, supplied.config);
    if (!rebuilt) {
        return std::unexpected(std::move(rebuilt.error()));
    }
    auto const& layout = *rebuilt;
    auto const& config = layout.config;
    auto const padding_y = config.padding_y + (config.show_bits ? 10 : 0);
    auto const total_row_height = config.row_height + padding_y;
    auto const svg_height = total_row_height * static_cast<double>(layout.words.size() + 1) -
        (layout.document.title.empty() ? config.row_height : 0);
    auto const svg_width = config.bit_width * static_cast<double>(config.bits_per_row) + 2;
    auto const title_x = svg_width / 2;
    auto const title_y = svg_height - total_row_height / 2;
    if (!finite_coordinates({padding_y, total_row_height, svg_width, svg_height, title_x, title_y}) ||
        svg_width < 0 || svg_height < 0) {
        return render_error(u"Packet SVG dimensions must be finite and nonnegative.");
    }
    auto title = xml_text(layout.document.title, u"Packet title");
    if (!title) {
        return std::unexpected(std::move(title.error()));
    }

    SvgWriter svg;
    svg.append("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 ");
    svg.number(svg_width);
    svg.append(" ");
    svg.number(svg_height);
    svg.append("\"");
    if (config.use_max_width) {
        svg.append(" width=\"100%\" style=\"max-width: ");
        svg.number(svg_width);
        svg.append("px;\"");
    } else {
        svg.numeric_attribute("height", svg_height);
        svg.numeric_attribute("width", svg_width);
    }
    svg.append(">");
    svg.append(default_packet_style);

    for (std::size_t row = 0; row < layout.words.size(); ++row) {
        auto const word_y = static_cast<double>(row) * total_row_height + padding_y;
        if (!std::isfinite(word_y)) {
            return render_error(u"Packet row position is not finite.");
        }
        svg.append("<g>");
        for (auto const& block : layout.words[row]) {
            auto const bits = block.end - block.start + 1;
            auto const first_column = std::fmod(block.start, static_cast<double>(config.bits_per_row));
            auto const column = config.descending
                ? static_cast<double>(config.bits_per_row) - first_column - bits
                : first_column;
            auto const block_x = column * config.bit_width + 1;
            auto const width = bits * config.bit_width - config.padding_x;
            auto const label_x = block_x + width / 2;
            auto const label_y = word_y + config.row_height / 2;
            auto const last_x = block_x + width;
            auto const bit_number_y = word_y - 2;
            if (!finite_coordinates({bits, column, block_x, width, label_x, label_y, last_x, bit_number_y}) ||
                bits < 1 || column < 0 || width < 0) {
                return render_error(u"Packet field geometry must be finite with nonnegative dimensions; padding must not exceed the field width.");
            }
            auto label = xml_text(block.label, u"Packet field label");
            if (!label) {
                return std::unexpected(std::move(label.error()));
            }

            svg.append("<rect");
            svg.numeric_attribute("x", block_x);
            svg.numeric_attribute("y", word_y);
            svg.numeric_attribute("width", width);
            svg.numeric_attribute("height", config.row_height);
            svg.append(" class=\"packetBlock\"></rect>");

            svg.append("<text");
            svg.numeric_attribute("x", label_x);
            svg.numeric_attribute("y", label_y);
            svg.append(" class=\"packetLabel\" dominant-baseline=\"middle\" text-anchor=\"middle\">");
            svg.append(*label);
            svg.append("</text>");

            if (!config.show_bits) {
                continue;
            }
            auto const single_bit = bits == 1;
            auto const leading_bit = config.descending ? block.end : block.start;
            auto const trailing_bit = config.descending ? block.start : block.end;
            svg.append("<text");
            svg.numeric_attribute("x", block_x + (single_bit ? width / 2 : 0));
            svg.numeric_attribute("y", bit_number_y);
            svg.append(" class=\"packetByte start\" dominant-baseline=\"auto\" text-anchor=\"");
            svg.append(single_bit ? "middle" : "start");
            svg.append("\">");
            svg.number(leading_bit);
            svg.append("</text>");
            if (!single_bit) {
                svg.append("<text");
                svg.numeric_attribute("x", last_x);
                svg.numeric_attribute("y", bit_number_y);
                svg.append(" class=\"packetByte end\" dominant-baseline=\"auto\" text-anchor=\"end\">");
                svg.number(trailing_bit);
                svg.append("</text>");
            }
        }
        svg.append("</g>");
    }
    // The source renderer emits this element even for an empty title. Its
    // position can legitimately be negative in an otherwise empty diagram.
    svg.append("<text");
    svg.numeric_attribute("x", title_x);
    svg.numeric_attribute("y", title_y);
    svg.append(" dominant-baseline=\"middle\" text-anchor=\"middle\" class=\"packetTitle\">");
    svg.append(*title);
    svg.append("</text></svg>");
    return std::move(svg).finish(svg_width, svg_height);
}

}
