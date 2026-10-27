// folia.markdown.html_entities — CommonMark HTML character references.
//
// This small shared utility owns syntax validation and character-reference
// decoding so the block and inline parsers use the same table and mappings.
// It has no dependency on Markdown source models or parser state.
export module folia.markdown.html_entities;
import std;

export namespace folia {

// `raw` must be one complete `&...;` spelling. Unknown names and malformed
// character-reference syntax return nullopt. Valid numeric references with an
// invalid Unicode scalar return U+FFFD, following CommonMark's HTML rules.
std::optional<std::u32string> decode_html_character_reference(
    std::u32string_view raw);

namespace html_entities_detail {

struct Entry {
    std::string_view name;
    std::uint32_t first;
    std::uint32_t second;
};

inline constexpr Entry entries[]{
#include "html_entity_data.inc"
};

inline std::optional<std::uint32_t> parse_number(std::u32string_view raw) {
    if (raw.size() < 4 || raw[0] != U'&' || raw[1] != U'#' || raw.back() != U';') {
        return std::nullopt;
    }
    std::size_t cursor = 2;
    unsigned base = 10;
    if (cursor < raw.size() - 1 && (raw[cursor] == U'x' || raw[cursor] == U'X')) {
        base = 16;
        ++cursor;
    }
    const auto digit_count = raw.size() - 1 - cursor;
    if (digit_count == 0 || digit_count > (base == 10 ? 7u : 6u)) return std::nullopt;
    std::uint32_t value = 0;
    for (; cursor < raw.size() - 1; ++cursor) {
        const auto ch = raw[cursor];
        unsigned digit = 99;
        if (ch >= U'0' && ch <= U'9') digit = static_cast<unsigned>(ch - U'0');
        else if (base == 16 && ch >= U'a' && ch <= U'f') digit = 10u + static_cast<unsigned>(ch - U'a');
        else if (base == 16 && ch >= U'A' && ch <= U'F') digit = 10u + static_cast<unsigned>(ch - U'A');
        if (digit >= base) return std::nullopt;
        value = value * base + digit;
    }
    return value;
}

inline std::optional<std::u32string> decode_numeric(std::u32string_view raw) {
    const auto parsed = parse_number(raw);
    if (!parsed) return std::nullopt;
    const auto value = *parsed;
    if (value == 0 || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
        return std::u32string(1, U'\ufffd');
    }
    return std::u32string(1, static_cast<char32_t>(value));
}

inline std::optional<std::u32string> decode_named(std::u32string_view raw) {
    if (raw.size() < 4 || raw.size() > 34 || raw.front() != U'&' || raw.back() != U';') return std::nullopt;
    std::string name;
    name.reserve(raw.size());
    for (const auto cp : raw) {
        if (cp > 0x7f) return std::nullopt;
        name.push_back(static_cast<char>(cp));
    }
    const auto found = std::ranges::lower_bound(entries, std::string_view(name), {}, &Entry::name);
    if (found == std::end(entries) || found->name != name) return std::nullopt;
    std::u32string decoded(1, static_cast<char32_t>(found->first));
    if (found->second != 0) decoded.push_back(static_cast<char32_t>(found->second));
    return decoded;
}

} // namespace html_entities_detail

inline std::optional<std::u32string> decode_html_character_reference(
    std::u32string_view raw) {
    if (raw.size() >= 2 && raw[0] == U'&' && raw[1] == U'#') {
        return html_entities_detail::decode_numeric(raw);
    }
    return html_entities_detail::decode_named(raw);
}

} // namespace folia
