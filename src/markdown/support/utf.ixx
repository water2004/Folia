// folia.markdown.utf — UTF-16 / grapheme helpers over std::u32string / std::string.
// We model char offsets as Unicode scalar values (Rust char); great care taken
// to never mix with byte/utf16 offsets.
export module folia.markdown.utf;
import std;

export namespace folia {

namespace unicode_property_detail {
#include "unicode_properties.inc"

template<std::size_t N>
inline bool contains(Range const (&ranges)[N], char32_t codepoint) {
    auto found = std::lower_bound(std::begin(ranges), std::end(ranges), codepoint,
        [](Range const& range, char32_t value) { return range.last < value; });
    return found != std::end(ranges) && found->first <= codepoint;
}
}

inline bool unicode_punctuation_or_symbol(char32_t codepoint) {
    return unicode_property_detail::contains(unicode_property_detail::punctuation_or_symbol, codepoint);
}

inline bool is_commonmark_whitespace(char32_t codepoint) {
    // CommonMark: Unicode Zs plus TAB, LF, FF and CR; no locale dependency.
    return codepoint == U'\t' || codepoint == U'\n' || codepoint == U'\f' || codepoint == U'\r'
        || unicode_property_detail::contains(unicode_property_detail::space_separators, codepoint);
}

inline std::u32string unicode_casefold(std::u32string_view input) {
    std::u32string output;
    output.reserve(input.size());
    auto const& table = unicode_property_detail::case_folding;
    for (auto codepoint : input) {
        auto found = std::lower_bound(std::begin(table), std::end(table), codepoint,
            [](auto const& entry, char32_t value) { return entry.from < value; });
        if (found == std::end(table) || found->from != codepoint) output.push_back(codepoint);
        else output.append(found->to.data(), found->size);
    }
    return output;
}

// ---------------------------------------------------------------------------
// UTF-16 helpers (work on char32 codepoint stream).
// ---------------------------------------------------------------------------
inline std::size_t utf16_len_of_cp(char32_t c) {
    return c <= 0xFFFF ? 1 : 2;
}

// Sum len_utf16 over a string as char32 cps.
inline std::size_t utf16_len(std::u32string_view cps) {
    std::size_t n = 0;
    for (char32_t c : cps) n += utf16_len_of_cp(c);
    return n;
}

// char index -> utf16 index, given the cps.
inline std::size_t char_index_to_utf16(std::u32string_view cps, std::size_t char_index) {
    std::size_t pos = 0;
    for (std::size_t i = 0; i < char_index && i < cps.size(); ++i) pos += utf16_len_of_cp(cps[i]);
    return pos;
}

// utf16 index -> char index
inline std::size_t utf16_to_char_index(std::u32string_view cps, std::size_t utf16_index) {
    std::size_t pos = 0, i = 0;
    while (i < cps.size() && pos < utf16_index) { pos += utf16_len_of_cp(cps[i]); ++i; }
    return i;
}

// svps: cps of text. Returns (start_char, end_char) for the given utf16 bounds.
inline std::pair<std::size_t, std::size_t> utf16_slice_bounds(std::u32string_view cps, std::size_t s, std::size_t e) {
    return {utf16_to_char_index(cps, s), utf16_to_char_index(cps, e)};
}

// ---------------------------------------------------------------------------
// Extended grapheme clusters use the repository's pinned Unicode 15.1 data
// and UAX #29 revision 43 rules GB3–GB999. Offsets remain Unicode scalars;
// only caret/delete boundaries change. No normalization or source rewriting.
// ---------------------------------------------------------------------------

inline bool is_cp_white(std::uint32_t c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0x00A0 || c == 0x2028 || c == 0x2029;
}

namespace grapheme_detail {

enum class Break { Other, CR, LF, Control, Extend, ZWJ, Regional_Indicator, Prepend, SpacingMark, L, V, T, LV, LVT };
enum class Indic { None, Consonant, Extend, Linker };
template<class Property> struct PropertyRange { char32_t first; char32_t last; Property value; };
#include "grapheme_properties.inc"

template<class Property, std::size_t N>
inline Property lookup(PropertyRange<Property> const (&ranges)[N], char32_t cp, Property fallback) {
    const auto next = std::lower_bound(std::begin(ranges), std::end(ranges), cp,
        [](auto const& range, char32_t value) { return range.last < value; });
    return next != std::end(ranges) && next->first <= cp ? next->value : fallback;
}

inline Break property(char32_t cp) {
    if (cp <= 0x7f) {
        if (cp == U'\r') return Break::CR;
        if (cp == U'\n') return Break::LF;
        return cp < 0x20 || cp == 0x7f ? Break::Control : Break::Other;
    }
    // Hangul syllables alternate LV/LVT arithmetically; avoid hundreds of
    // singleton ranges in the generated property table.
    if (cp >= 0xac00 && cp <= 0xd7a3)
        return (cp - 0xac00) % 28 == 0 ? Break::LV : Break::LVT;
    return lookup(break_ranges, cp, Break::Other);
}

inline Indic indic_property(char32_t cp) {
    return cp < 0x300 ? Indic::None : lookup(indic_ranges, cp, Indic::None);
}

inline bool pictographic(char32_t cp) {
    return cp >= 0xa9 && unicode_property_detail::contains(pictographic_ranges, cp);
}

inline bool regional_indicator(char32_t cp) { return cp >= 0x1f1e6 && cp <= 0x1f1ff; }
inline bool control(Break value) { return value == Break::Control || value == Break::CR || value == Break::LF; }

inline bool boundary(std::u32string_view text, std::size_t index,
    std::optional<std::size_t> preceding_regional_count = std::nullopt) {
    if (index == 0 || index >= text.size()) return true; // GB1/GB2
    const auto left = property(text[index - 1]);
    const auto right = property(text[index]);
    if (left == Break::CR && right == Break::LF) return false; // GB3
    if (control(left) || control(right)) return true; // GB4/GB5
    if (left == Break::L && (right == Break::L || right == Break::V
        || right == Break::LV || right == Break::LVT)) return false; // GB6
    if ((left == Break::LV || left == Break::V) && (right == Break::V || right == Break::T)) return false; // GB7
    if ((left == Break::LVT || left == Break::T) && right == Break::T) return false; // GB8
    if (right == Break::Extend || right == Break::ZWJ) return false; // GB9
    if (right == Break::SpacingMark || left == Break::Prepend) return false; // GB9a/GB9b
    if (indic_property(text[index]) == Indic::Consonant) {
        auto cursor = index;
        bool linker = false;
        while (cursor != 0) {
            const auto value = indic_property(text[cursor - 1]);
            if (value == Indic::Linker) linker = true;
            else if (value != Indic::Extend) break;
            --cursor;
        }
        if (linker && cursor != 0 && indic_property(text[cursor - 1]) == Indic::Consonant) return false; // GB9c
    }
    if (left == Break::ZWJ && pictographic(text[index])) {
        auto cursor = index - 1;
        while (cursor != 0 && property(text[cursor - 1]) == Break::Extend) --cursor;
        if (cursor != 0 && pictographic(text[cursor - 1])) return false; // GB11
    }
    if (left == Break::Regional_Indicator && right == Break::Regional_Indicator) {
        auto count = preceding_regional_count.value_or(0);
        if (!preceding_regional_count) {
            auto cursor = index;
            while (cursor != 0 && regional_indicator(text[cursor - 1])) { --cursor; ++count; }
        }
        return count % 2 == 0; // GB12/GB13
    }
    return true; // GB999
}

} // namespace grapheme_detail

inline bool is_regional_indicator(char32_t c) { return grapheme_detail::regional_indicator(c); }
inline bool is_extend_class(char32_t c) {
    const auto value = grapheme_detail::property(c);
    return value == grapheme_detail::Break::Extend || value == grapheme_detail::Break::ZWJ;
}

inline bool is_grapheme_boundary_char(std::u32string_view cps, std::size_t char_index) {
    return grapheme_detail::boundary(cps, char_index);
}

inline std::size_t count_graphemes(std::u32string_view cps) {
    if (cps.empty()) return 0;
    std::size_t count = 1;
    std::size_t regional_count = is_regional_indicator(cps.front()) ? 1 : 0;
    for (std::size_t index = 1; index < cps.size(); ++index) {
        if (grapheme_detail::boundary(cps, index, regional_count)) ++count;
        regional_count = is_regional_indicator(cps[index]) ? regional_count + 1 : 0;
    }
    return count;
}

// Find the preceding/following boundary even when an externally restored
// scalar offset is inside a cluster. A leading combining sequence never
// absorbs the following base character; regional indicators pair from the
// beginning of their contiguous run.
inline std::size_t prev_grapheme_boundary_char(std::u32string_view cps, std::size_t char_index) {
    auto cursor = (std::min)(char_index, cps.size());
    if (cursor == 0) return 0;
    --cursor;
    while (cursor != 0 && !grapheme_detail::boundary(cps, cursor)) --cursor;
    return cursor;
}

inline std::size_t next_grapheme_boundary_char(std::u32string_view cps, std::size_t char_index) {
    if (char_index >= cps.size()) return cps.size();
    auto cursor = char_index + 1;
    while (cursor < cps.size() && !grapheme_detail::boundary(cps, cursor)) ++cursor;
    return cursor;
}

// ---------------------------------------------------------------------------
// Convenience converters between UTF-8 (std::string) and cps (std::u32string).
// ---------------------------------------------------------------------------
inline std::u32string utf8_to_cps(std::string_view utf8) {
    std::u32string out;
    std::size_t i = 0;
    while (i < utf8.size()) {
        char32_t out_cp = 0; int len = 0;
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        if ((c & 0x80) == 0) { out_cp = c; len = 1; }
        else if ((c & 0xE0) == 0xC0) { out_cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { out_cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { out_cp = c & 0x07; len = 4; }
        else { out_cp = c; len = 1; }
        for (int k = 1; k < len && i + k < utf8.size(); ++k)
            out_cp = (out_cp << 6) | (static_cast<unsigned char>(utf8[i + k]) & 0x3F);
        out.push_back(out_cp);
        i += len;
    }
    return out;
}

inline std::string cps_to_utf8(std::u32string_view cps) {
    std::string out;
    auto push = [&](char32_t c) {
        if (c <= 0x7F) { out.push_back(static_cast<char>(c)); }
        else if (c <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (c >> 6)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else if (c <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (c >> 12)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (c >> 18)));
            out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    };
    for (char32_t c : cps) push(c);
    return out;
}

} // namespace folia
