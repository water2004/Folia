// Mermaid 12.2.0 diagram-api/regexes.ts directiveRegex, MIT; see ../LICENSE.
export module folia.mermaid.directive_lexer;
import std;

export namespace folia::mermaid::detail {
constexpr bool is_word(char16_t value) noexcept {
    return (value >= u'a' && value <= u'z') || (value >= u'A' && value <= u'Z') ||
           (value >= u'0' && value <= u'9') || value == u'_';
}
constexpr bool is_ecmascript_whitespace(char16_t value) noexcept {
    return (value >= u'\u0009' && value <= u'\u000D') || value == u' ' ||
           value == u'\u00A0' || value == u'\u1680' ||
           (value >= u'\u2000' && value <= u'\u200A') || value == u'\u2028' ||
           value == u'\u2029' || value == u'\u202F' || value == u'\u205F' ||
           value == u'\u3000' || value == u'\uFEFF';
}
inline std::u16string_view trim_ecmascript(std::u16string_view text) {
    while (!text.empty() && is_ecmascript_whitespace(text.front())) text.remove_prefix(1);
    while (!text.empty() && is_ecmascript_whitespace(text.back())) text.remove_suffix(1);
    return text;
}

// Borrowed captures are valid only for the lifetime of the scanned text.
// Offsets include every UTF-16 code unit; end is exclusive.
struct DirectiveMatch {
    std::size_t start{}, end{};
    std::u16string_view type, argument;
    bool word_argument = false;
};
std::optional<DirectiveMatch> next_directive(std::u16string_view text, std::size_t from);
}
