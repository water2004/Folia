// Shared terminals from packages/parser/src/language/common/common.langium and
// valueConverter.ts at Mermaid 12.2.0, commit 1fb7869866dd619168d840291c9c54b4f129758b.
// STRING conversion follows Langium 4.4.0 DefaultValueConverter.
// Copyright (c) 2014-2022 Knut Sveidqvist; Copyright 2021 TypeFox GmbH.
// MIT licensed; see ../LICENSE and ../THIRD_PARTY_NOTICES.md.
module folia.mermaid.common_lexer;
import std;

namespace folia::mermaid::detail {
namespace {

bool is_horizontal_space(char16_t value) {
    return value == u' ' || value == u'\t';
}

bool is_line_terminator(char16_t value) {
    return value == u'\r' || value == u'\n' || value == u'\u2028' || value == u'\u2029';
}

// ECMAScript WhiteSpace + LineTerminator, used by the source grammar's \s
// assertions and common value converter, rather than locale-sensitive isspace.
bool is_js_space(char16_t value) {
    return (value >= u'\t' && value <= u'\r') || value == u' ' ||
        value == u'\u00a0' || value == u'\u1680' ||
        (value >= u'\u2000' && value <= u'\u200a') ||
        value == u'\u2028' || value == u'\u2029' || value == u'\u202f' ||
        value == u'\u205f' || value == u'\u3000' || value == u'\ufeff';
}

std::u16string collapse_horizontal_space(std::u16string_view value) {
    std::u16string result;
    for (std::size_t index = 0; index < value.size();) {
        if (!is_horizontal_space(value[index])) {
            result.push_back(value[index++]);
            continue;
        }
        auto end = index + 1;
        while (end < value.size() && is_horizontal_space(value[end])) {
            ++end;
        }
        result.push_back(end - index >= 2 ? u' ' : value[index]);
        index = end;
    }
    return result;
}

std::u16string common_single_line_value(std::u16string_view value) {
    while (!value.empty() && is_js_space(value.front())) {
        value.remove_prefix(1);
    }
    while (!value.empty() && is_js_space(value.back())) {
        value.remove_suffix(1);
    }
    return collapse_horizontal_space(value);
}

std::u16string common_multiline_value(std::u16string_view value) {
    // Preserve the exact order of CommonValueConverter's four replacements:
    // /^\s*/gm, /\s+$/gm, /[\t ]{2,}/gm, /[\n\r]{2,}/gm.
    std::u16string without_leading;
    for (std::size_t index = 0; index < value.size();) {
        if (index == 0 || is_line_terminator(value[index - 1])) {
            while (index < value.size() && is_js_space(value[index])) {
                ++index;
            }
        }
        if (index < value.size()) {
            without_leading.push_back(value[index++]);
        }
    }

    std::u16string without_trailing;
    for (std::size_t index = 0; index < without_leading.size();) {
        if (!is_js_space(without_leading[index])) {
            without_trailing.push_back(without_leading[index++]);
            continue;
        }
        auto end = index;
        auto last_line_terminator = index;
        while (end < without_leading.size() && is_js_space(without_leading[end])) {
            if (is_line_terminator(without_leading[end])) {
                last_line_terminator = end;
            }
            ++end;
        }
        if (end != without_leading.size()) {
            without_trailing.append(without_leading, last_line_terminator,
                                    end - last_line_terminator);
        }
        index = end;
    }

    auto collapsed = collapse_horizontal_space(without_trailing);
    std::u16string result;
    for (std::size_t index = 0; index < collapsed.size();) {
        if (collapsed[index] != u'\n' && collapsed[index] != u'\r') {
            result.push_back(collapsed[index++]);
            continue;
        }
        auto end = index + 1;
        while (end < collapsed.size() && (collapsed[end] == u'\n' || collapsed[end] == u'\r')) {
            ++end;
        }
        result.push_back(end - index >= 2 ? u'\n' : collapsed[index]);
        index = end;
    }
    return result;
}

} // namespace

CommonLexer::CommonLexer(std::u16string_view source) : source_(source) {}

std::optional<CommonToken> CommonLexer::metadata_token() {
    auto const start = position_;
    if (starts_with(u"accDescr")) {
        if (auto token = description_token()) {
            return *token;
        }
    }
    if (starts_with(u"accTitle")) {
        auto after = position_ + 8;
        while (after < source_.size() && is_horizontal_space(source_[after])) {
            ++after;
        }
        if (after < source_.size() && source_[after] == u':') {
            return line_value(CommonTokenKind::acc_title, after + 1);
        }
    }
    if (starts_with(u"title")) {
        auto after = position_ + 5;
        if (after < source_.size() && is_horizontal_space(source_[after])) {
            return line_value(CommonTokenKind::title, after);
        }
        position_ = after;
        return CommonToken{CommonTokenKind::title, start};
    }
    return std::nullopt;
}

bool CommonLexer::starts_with(std::u16string_view text) const {
    return source_.substr(position_).starts_with(text);
}

std::size_t CommonLexer::newline_length(std::size_t index) const {
    if (index < source_.size() && source_[index] == u'\n') {
        return 1;
    }
    return index + 1 < source_.size() && source_[index] == u'\r' &&
        source_[index + 1] == u'\n' ? 2 : 0;
}

bool CommonLexer::keyword_boundary(std::size_t index) const {
    return index == source_.size() || is_js_space(source_[index]) ||
        source_.substr(index).starts_with(u"%%");
}

std::optional<std::size_t> CommonLexer::yaml_end() const {
    if (!starts_with(u"---")) {
        return std::nullopt;
    }
    auto cursor = position_ + 3;
    while (cursor < source_.size() && is_horizontal_space(source_[cursor])) {
        ++cursor;
    }
    auto const opening_newline = newline_length(cursor);
    if (!opening_newline) {
        return std::nullopt;
    }
    cursor += opening_newline;
    for (;;) {
        if (source_.substr(cursor).starts_with(u"---")) {
            auto const after = cursor + 3;
            if (auto length = newline_length(after)) {
                return after + length;
            }
            if (after == source_.size() || is_js_space(source_[after])) {
                return after;
            }
        }
        auto const newline = source_.find(u'\n', cursor);
        if (newline == std::u16string_view::npos) {
            return std::nullopt;
        }
        cursor = newline + 1;
    }
}

void CommonLexer::skip_hidden() {
    while (position_ < source_.size()) {
        if (is_horizontal_space(source_[position_])) {
            ++position_;
            continue;
        }
        if (auto end = yaml_end()) {
            position_ = *end;
            continue;
        }
        if (starts_with(u"%%{")) {
            auto closing = source_.find(u"}%%", position_ + 3);
            std::optional<std::size_t> directive_end;
            while (closing != std::u16string_view::npos) {
                auto const after = closing + 3;
                auto const length = newline_length(after);
                if (length || after == source_.size() || is_js_space(source_[after])) {
                    directive_end = after + length;
                    break;
                }
                closing = source_.find(u"}%%", after);
            }
            if (directive_end) {
                position_ = *directive_end;
                continue;
            }
        }
        if (starts_with(u"%%")) {
            auto end = source_.find_first_of(u"\r\n", position_);
            position_ = end == std::u16string_view::npos ? source_.size() : end;
            continue;
        }
        break;
    }
}

CommonToken CommonLexer::line_value(CommonTokenKind kind, std::size_t content_start) {
    auto const start = position_;
    auto end = content_start;
    while (end < source_.size() && source_[end] != u'\r' && source_[end] != u'\n' &&
           !source_.substr(end).starts_with(u"%%")) {
        ++end;
    }
    position_ = end;
    return CommonToken{kind, start,
        common_single_line_value(source_.substr(content_start, end - content_start))};
}

std::optional<CommonToken> CommonLexer::description_token() {
    auto const start = position_;
    auto after = start + 8;
    while (after < source_.size() && is_horizontal_space(source_[after])) {
        ++after;
    }
    if (after < source_.size() && source_[after] == u':') {
        return line_value(CommonTokenKind::acc_description, after + 1);
    }
    after = start + 8;
    while (after < source_.size() && is_js_space(source_[after])) {
        ++after;
    }
    if (after == source_.size() || source_[after] != u'{') {
        return std::nullopt;
    }
    auto const closing = source_.find(u'}', after + 1);
    if (closing == std::u16string_view::npos) {
        return std::nullopt;
    }
    position_ = closing + 1;
    return CommonToken{CommonTokenKind::acc_description, start,
        common_multiline_value(source_.substr(after + 1, closing - after - 1))};
}

std::expected<CommonToken, LexError> CommonLexer::string_token() {
    auto const start = position_;
    auto const delimiter = source_[position_++];
    std::u16string text;
    while (position_ < source_.size()) {
        auto const value = source_[position_++];
        if (value == delimiter) {
            return CommonToken{CommonTokenKind::string, start, std::move(text)};
        }
        if (value == u'\\') {
            if (position_ == source_.size() || is_line_terminator(source_[position_])) {
                return std::unexpected(LexError{start, u"Invalid escape in quoted string."});
            }
            // DefaultValueConverter does not decode JavaScript Unicode or
            // hexadecimal escapes: an unknown escape keeps its character
            // and drops only the backslash.
            switch (auto const escaped = source_[position_++]) {
            case u'b': text.push_back(u'\b'); break;
            case u'f': text.push_back(u'\f'); break;
            case u'n': text.push_back(u'\n'); break;
            case u'r': text.push_back(u'\r'); break;
            case u't': text.push_back(u'\t'); break;
            case u'v': text.push_back(u'\v'); break;
            case u'0': text.push_back(u'\0'); break;
            default: text.push_back(escaped); break;
            }
        } else {
            text.push_back(value);
        }
    }
    return std::unexpected(LexError{start, u"Unterminated quoted string."});
}

} // namespace folia::mermaid::detail
