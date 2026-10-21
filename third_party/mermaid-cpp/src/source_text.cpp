// Ported from Mermaid 12.2.0, commit 1fb7869866dd619168d840291c9c54b4f129758b.
// Source: packages/mermaid/src/diagram-api/{regexes,comments}.ts,
// packages/mermaid/src/preprocess.ts (cleanupText), utils.ts (removeDirectives).
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT licensed; see ../LICENSE.

module folia.mermaid.source_text;

import std;

namespace folia::mermaid {
namespace {

// ECMAScript non-Unicode \w (upstream uses /gi, not /giu).
constexpr bool is_word(char16_t value) noexcept {
    return (value >= u'a' && value <= u'z') ||
           (value >= u'A' && value <= u'Z') ||
           (value >= u'0' && value <= u'9') || value == u'_';
}

// ECMAScript WhiteSpace and LineTerminator, independent of the process locale.
// U+0085, U+180E and U+200B are deliberately not included.
constexpr bool is_ecmascript_whitespace(char16_t value) noexcept {
    return (value >= u'\u0009' && value <= u'\u000D') ||
           value == u'\u0020' || value == u'\u00A0' ||
           value == u'\u1680' ||
           (value >= u'\u2000' && value <= u'\u200A') ||
           value == u'\u2028' || value == u'\u2029' ||
           value == u'\u202F' || value == u'\u205F' ||
           value == u'\u3000' || value == u'\uFEFF';
}

constexpr bool is_fence_line_break(char16_t value) noexcept {
    return value == u'\n' || value == u'\r';
}

constexpr bool is_multiline_line_break(char16_t value) noexcept {
    return is_fence_line_break(value) || value == u'\u2028' ||
           value == u'\u2029';
}

std::optional<std::size_t> last_line_break_in_whitespace_run(
    std::u16string_view text, std::size_t from) {
    std::optional<std::size_t> last;
    for (auto i = from; i < text.size() && is_ecmascript_whitespace(text[i]); ++i) {
        if (is_fence_line_break(text[i])) {
            last = i;
        }
    }
    return last;
}

} // namespace

std::u16string cleanup_text(std::u16string_view text) {
    std::u16string normalized;
    normalized.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == u'\r') {
            normalized += u'\n';
            if (i + 1 < text.size() && text[i + 1] == u'\n') ++i;
        } else {
            normalized += text[i];
        }
    }
    std::u16string result;
    result.reserve(normalized.size());
    const std::u16string_view input{normalized};
    std::size_t cursor = 0;
    while (cursor < input.size()) {
        if (input[cursor] != u'<' || cursor + 1 == input.size() ||
            !is_word(input[cursor + 1])) {
            result += input[cursor++];
            continue;
        }
        const auto end = input.find(u'>', cursor + 2);
        if (end == input.npos) {
            result += input.substr(cursor);
            break;
        }
        // Even a quoted '>' ends the official regex. Attributes start after
        // the greedy ASCII word; no HTML parsing rules participate.
        std::size_t attributes = cursor + 2;
        while (attributes < end && is_word(input[attributes])) ++attributes;
        result += input.substr(cursor, attributes - cursor);
        cursor = attributes;
        while (cursor < end) {
            if (input[cursor] == u'=' && cursor + 1 < end && input[cursor + 1] == u'"') {
                const auto quote = input.find(u'"', cursor + 2);
                if (quote == input.npos || quote >= end) {
                    result += input.substr(cursor, end - cursor);
                    cursor = end;
                    break;
                }
                result += u"='";
                result += input.substr(cursor + 2, quote - cursor - 2);
                result += u'\'';
                cursor = quote + 1;
            } else {
                result += input[cursor++];
            }
        }
        result += u'>';
        cursor = end + 1;
    }
    return result;
}

std::u16string remove_directives(std::u16string_view text) {
    std::u16string result;
    result.reserve(text.size());
    std::size_t copied = 0;
    std::size_t search = 0;
    const auto skip_space = [&](std::size_t cursor) {
        while (cursor < text.size() && is_ecmascript_whitespace(text[cursor])) ++cursor;
        return cursor;
    };
    while (search < text.size()) {
        const auto start = text.find(u"%%{", search);
        if (start == text.npos) break;
        auto cursor = skip_space(start + 3);
        const auto type_start = cursor;
        while (cursor < text.size() && is_word(text[cursor])) ++cursor;
        if (cursor == type_start) {
            search = cursor; // Skipped whitespace cannot contain an opener.
            continue;
        }
        const auto after_type = skip_space(cursor);
        // The colon alternative is tried before the plain-word alternative.
        cursor = after_type < text.size() && text[after_type] == u':'
            ? skip_space(after_type + 1) : after_type;
        if (cursor < text.size() && is_word(text[cursor])) {
            // The optional argument's word alternative wins even if it leaves
            // trailing text or an unmatched closing delimiter in the input.
            while (cursor < text.size() && is_word(text[cursor])) ++cursor;
        } else {
            while (cursor < text.size() && !text.substr(cursor).starts_with(u"}%%")) {
                if (text[cursor] == u'\r') {
                    if (cursor + 1 == text.size() || text[cursor + 1] != u'\n') break;
                    cursor += 2;
                } else if (text[cursor] == u'\u2028' || text[cursor] == u'\u2029') {
                    break;
                } else {
                    ++cursor; // Dot or the separate LF alternative.
                }
            }
        }
        cursor = skip_space(cursor);
        if (text.substr(cursor).starts_with(u"}%%")) cursor += 3;
        result += text.substr(copied, start - copied);
        copied = search = cursor;
    }
    result += text.substr(copied);
    return result;
}

std::optional<FrontMatterMatch> match_front_matter(std::u16string_view text) {
    std::size_t cursor = 0;
    while (cursor < text.size() && is_ecmascript_whitespace(text[cursor]) &&
           !is_fence_line_break(text[cursor])) {
        ++cursor;
    }
    const auto indent = text.substr(0, cursor);
    if (!text.substr(cursor).starts_with(u"---")) {
        return std::nullopt;
    }

    // The opening whitespace is greedy, including newlines. Keep every possible
    // split so the longest opening that leaves a closing fence wins.
    std::vector<std::size_t> opening_ends;
    for (auto i = cursor + 3;
         i < text.size() && is_ecmascript_whitespace(text[i]); ++i) {
        if (is_fence_line_break(text[i])) {
            opening_ends.push_back(i);
        }
    }
    if (opening_ends.empty()) {
        return std::nullopt;
    }

    std::u16string closing_fence{indent};
    closing_fence += u"---";
    const auto closing_end = [&](std::size_t i) -> std::optional<std::size_t> {
        if (!is_fence_line_break(text[i]) ||
            !text.substr(i + 1).starts_with(closing_fence)) {
            return std::nullopt;
        }
        return last_line_break_in_whitespace_run(text, i + 1 + closing_fence.size());
    };

    std::optional<std::size_t> last_closing_fence;
    for (auto i = opening_ends.front() + 1; i < text.size(); ++i) {
        if (closing_end(i)) {
            last_closing_fence = i;
        }
    }
    if (!last_closing_fence) {
        return std::nullopt;
    }

    auto opening_end = opening_ends.front();
    for (const auto candidate : opening_ends) {
        if (candidate < *last_closing_fence) {
            opening_end = candidate;
        }
    }

    // Once the opening is chosen, the body is lazy: take its first closing fence.
    // Distinct closing fences have disjoint following whitespace runs, so these
    // two passes perform linear work even on whitespace-heavy malformed input.
    const auto body_start = opening_end + 1;
    for (auto i = body_start; i <= *last_closing_fence; ++i) {
        if (const auto match_end = closing_end(i)) {
            return FrontMatterMatch{
                std::u16string{indent},
                std::u16string{text.substr(body_start, i - body_start)},
                *match_end + 1,
            };
        }
    }
    return std::nullopt;
}

std::u16string strip_any_comments(std::u16string_view text) {
    std::u16string out;
    out.reserve(text.size());
    std::size_t consumed = 0;
    std::size_t search_from = 0;

    while (search_from < text.size()) {
        const auto marker = text.find(u"%%", search_from);
        if (marker == std::u16string_view::npos) {
            break;
        }
        const auto line_end = text.find(u'\n', marker);
        if (line_end == std::u16string_view::npos) {
            break;
        }

        auto start = marker;
        while (start > consumed && is_ecmascript_whitespace(text[start - 1])) {
            --start;
        }
        out.append(text.substr(consumed, start - consumed));
        out.push_back(u'\n');
        consumed = line_end + 1;
        search_from = consumed;
    }

    out.append(text.substr(consumed));
    return out;
}

std::u16string cleanup_comments(std::u16string_view text) {
    std::u16string out;
    out.reserve(text.size());
    std::size_t consumed = 0;
    std::size_t cursor = 0;
    std::optional<std::size_t> match_start = 0;

    // Match upstream /^\s*%%(?!{)[^\n]+\n?/gm without rescanning blank lines.
    // Retain the earliest multiline anchor while traversing whitespace. A failed
    // nonwhitespace character invalidates every anchor in that whitespace run.
    while (cursor < text.size()) {
        if (match_start) {
            if (is_ecmascript_whitespace(text[cursor])) {
                ++cursor;
                continue;
            }
            if (text.substr(cursor).starts_with(u"%%") &&
                text.size() - cursor > 2 && text[cursor + 2] != u'{' &&
                text[cursor + 2] != u'\n') {
                // Only LF ends [^\n]+; CR, LS and PS are part of its body even
                // though all four line terminators establish multiline anchors.
                const auto line_end = text.find(u'\n', cursor + 2);
                const auto end = line_end == std::u16string_view::npos
                                     ? text.size()
                                     : line_end + 1;
                out.append(text.substr(consumed, *match_start - consumed));
                consumed = end;
                cursor = end;
                match_start = end;
                continue;
            }
            match_start.reset();
        }
        if (is_multiline_line_break(text[cursor])) {
            match_start = cursor + 1;
        }
        ++cursor;
    }

    out.append(text.substr(consumed));
    std::size_t first = 0;
    while (first < out.size() && is_ecmascript_whitespace(out[first])) {
        ++first;
    }
    out.erase(0, first);
    return out;
}

} // namespace folia::mermaid
