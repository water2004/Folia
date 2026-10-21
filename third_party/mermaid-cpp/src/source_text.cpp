// Ported from Mermaid 12.2.0, commit 1fb7869866dd619168d840291c9c54b4f129758b.
// Source: packages/mermaid/src/diagram-api/{regexes,comments}.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT licensed; see ../LICENSE.

module folia.mermaid.source_text;

import std;

namespace folia::mermaid {
namespace {

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
