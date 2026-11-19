// XML 1.0 text encoding shared by native Mermaid renderers.
export module folia.mermaid.svg_text;
import std;
export namespace folia::mermaid {
std::u16string text_error_message(
    std::u16string_view field, std::u16string_view reason, std::size_t offset) {
    auto const digits = std::to_string(offset);
    return std::u16string(field) + u" " + std::u16string(reason) + u" at UTF-16 offset " +
        std::u16string(digits.begin(), digits.end()) + u".";
}

std::expected<std::string, std::u16string> escape_svg_text(
    std::u16string_view text, std::u16string_view field) {
    std::string result;
    for (std::size_t index = 0; index < text.size(); ++index) {
        auto const start = index;
        auto scalar = static_cast<std::uint32_t>(text[index]);
        if (scalar >= 0xd800 && scalar <= 0xdbff) {
            if (index + 1 == text.size() || text[index + 1] < 0xdc00 || text[index + 1] > 0xdfff) {
                return std::unexpected(text_error_message(field, u"contains an unpaired high surrogate", start));
            }
            scalar = 0x10000 + ((scalar - 0xd800) << 10) + (text[++index] - 0xdc00);
        } else if (scalar >= 0xdc00 && scalar <= 0xdfff) {
            return std::unexpected(text_error_message(field, u"contains an unpaired low surrogate", start));
        }

        bool const xml_character = scalar == 0x9 || scalar == 0xa || scalar == 0xd ||
            (scalar >= 0x20 && scalar <= 0xd7ff) ||
            (scalar >= 0xe000 && scalar <= 0xfffd) ||
            (scalar >= 0x10000 && scalar <= 0x10ffff);
        if (!xml_character) {
            return std::unexpected(text_error_message(field, u"contains a character forbidden by XML 1.0", start));
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

}
