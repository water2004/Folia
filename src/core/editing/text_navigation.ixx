// Text navigation policy shared by rendered and source editing. Positions
// remain Unicode scalar offsets and every destination is a grapheme boundary.
export module folia.core.text_navigation;
import std;
import folia.markdown.utf;
import folia.markdown.text_edit;

export namespace folia {

namespace text_navigation_detail {
enum class WordClass { Space, Text, Punctuation };

inline WordClass word_class(char32_t value) {
    if (is_commonmark_whitespace(value) || value == U'\v'
        || value == U'\u0085' || value == U'\u2028' || value == U'\u2029')
        return WordClass::Space;
    // An underscore is part of an identifier in ordinary text/code editing.
    if (value != U'_' && unicode_punctuation_or_symbol(value))
        return WordClass::Punctuation;
    return WordClass::Text;
}
}

// Ctrl+Left skips preceding whitespace and then the preceding word or
// punctuation run. Ctrl+Right skips the current run and following whitespace.
// Classification is deliberately locale-independent; it does not claim
// language-specific dictionary segmentation for scripts without spaces.
inline std::size_t previous_word_boundary(std::u32string_view text, std::size_t offset) {
    using namespace text_navigation_detail;
    auto cursor = (std::min)(offset, text.size());
    while (cursor != 0) {
        const auto previous = prev_grapheme_boundary_char(text, cursor);
        if (word_class(text[previous]) != WordClass::Space) break;
        cursor = previous;
    }
    if (cursor == 0) return 0;
    cursor = prev_grapheme_boundary_char(text, cursor);
    const auto kind = word_class(text[cursor]);
    while (cursor != 0) {
        const auto previous = prev_grapheme_boundary_char(text, cursor);
        if (word_class(text[previous]) != kind) break;
        cursor = previous;
    }
    return cursor;
}

inline std::size_t next_word_boundary(std::u32string_view text, std::size_t offset) {
    using namespace text_navigation_detail;
    auto cursor = (std::min)(offset, text.size());
    if (cursor == text.size()) return cursor;
    // A restored scalar offset may lie inside a cluster. Classify that
    // cluster by its first scalar rather than by a combining mark or ZWJ.
    const auto cluster = is_grapheme_boundary_char(text, cursor)
        ? cursor : prev_grapheme_boundary_char(text, cursor);
    const auto kind = word_class(text[cluster]);
    do {
        cursor = next_grapheme_boundary_char(text, cursor);
    } while (cursor < text.size() && word_class(text[cursor]) == kind);
    while (cursor < text.size() && word_class(text[cursor]) == WordClass::Space)
        cursor = next_grapheme_boundary_char(text, cursor);
    return cursor;
}

// Double-click selection takes the source scalar under the pointer, rather
// than a rounded caret edge. Empty content has no selectable unit; every
// other source position selects a whole text, whitespace or punctuation run.
inline std::optional<SourceRange> text_word_selection_range(
    std::u32string_view text, std::size_t offset) {
    using namespace text_navigation_detail;
    if (text.empty()) return std::nullopt;
    auto start = (std::min)(offset, text.size());
    if (start == text.size() || !is_grapheme_boundary_char(text, start))
        start = prev_grapheme_boundary_char(text, start);
    auto end = next_grapheme_boundary_char(text, start);
    const auto kind = word_class(text[start]);
    while (start != 0) {
        const auto previous = prev_grapheme_boundary_char(text, start);
        if (word_class(text[previous]) != kind) break;
        start = previous;
    }
    while (end < text.size() && word_class(text[end]) == kind)
        end = next_grapheme_boundary_char(text, end);
    return SourceRange{start, end};
}

} // namespace folia
