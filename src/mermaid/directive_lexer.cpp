// Ported from Mermaid 12.2.0 directiveRegex. MIT licensed; see ../LICENSE.
module folia.mermaid.directive_lexer;
import std;

namespace folia::mermaid::detail {
std::optional<DirectiveMatch> next_directive(std::u16string_view text, std::size_t from) {
    const auto skip_space = [&](std::size_t cursor) {
        while (cursor < text.size() && is_ecmascript_whitespace(text[cursor])) ++cursor;
        return cursor;
    };
    while (from < text.size()) {
        const auto start = text.find(u"%%{", from);
        if (start == text.npos) break;
        auto cursor = skip_space(start + 3);
        const auto type_start = cursor;
        while (cursor < text.size() && is_word(text[cursor])) ++cursor;
        if (cursor == type_start) {
            from = cursor; // Skipped whitespace cannot contain an opener.
            continue;
        }
        const auto type_end = cursor;
        const auto after_type = skip_space(cursor);
        cursor = after_type < text.size() && text[after_type] == u':'
            ? skip_space(after_type + 1) : after_type;
        const auto argument_start = cursor;
        const bool word = cursor < text.size() && is_word(text[cursor]);
        if (word) {
            // The word alternative wins even when it leaves trailing text.
            while (cursor < text.size() && is_word(text[cursor])) ++cursor;
        } else {
            while (cursor < text.size() && !text.substr(cursor).starts_with(u"}%%")) {
                if (text[cursor] == u'\r') {
                    if (cursor + 1 == text.size() || text[cursor + 1] != u'\n') break;
                    cursor += 2;
                } else if (text[cursor] == u'\u2028' || text[cursor] == u'\u2029') {
                    break;
                } else {
                    ++cursor;
                }
            }
        }
        const auto argument_end = cursor;
        cursor = skip_space(cursor);
        if (text.substr(cursor).starts_with(u"}%%")) cursor += 3;
        return DirectiveMatch{start, cursor, text.substr(type_start, type_end - type_start),
                              text.substr(argument_start, argument_end - argument_start), word};
    }
    return std::nullopt;
}
}
