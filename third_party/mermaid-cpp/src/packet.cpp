// Ported from Mermaid's Packet grammar, parser/populate and common value
// converter; the STRING conversion follows Langium 4.4.0 ValueConverter.
// Copyright (c) 2014-2022 Knut Sveidqvist; Copyright 2021 TypeFox GmbH.
// MIT licensed; see ../LICENSE and ../THIRD_PARTY_NOTICES.md.
module folia.mermaid.packet;
import std;
import folia.mermaid.number;

namespace folia::mermaid {
namespace {

constexpr std::size_t maximum_packet_rows = 10'000;

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

bool is_digit(char16_t value) {
    return value >= u'0' && value <= u'9';
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

enum class TokenKind {
    packet, plus, minus, colon, integer, string,
    title, acc_title, acc_description, newline, end
};

struct Token {
    TokenKind kind{};
    std::size_t offset{};
    double number{};
    std::u16string text;
};

class PacketLexer {
public:
    explicit PacketLexer(std::u16string_view source) : source_(source) {}

    std::expected<Token, PacketError> next() {
        skip_hidden();
        auto const start = position_;
        if (position_ == source_.size()) {
            return Token{TokenKind::end, start};
        }
        if (auto length = newline_length(position_)) {
            position_ += length;
            return Token{TokenKind::newline, start};
        }
        if (starts_with(u"packet-beta")) {
            position_ += 11;
            return Token{TokenKind::packet, start};
        }
        if (starts_with(u"packet") && keyword_boundary(position_ + 6)) {
            position_ += 6;
            return Token{TokenKind::packet, start};
        }
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
                return line_value(TokenKind::acc_title, after + 1);
            }
        }
        if (starts_with(u"title")) {
            auto after = position_ + 5;
            if (after < source_.size() && is_horizontal_space(source_[after])) {
                return line_value(TokenKind::title, after);
            }
            position_ = after;
            return Token{TokenKind::title, start};
        }
        auto const value = source_[position_];
        if (value == u'+' || value == u'-' || value == u':') {
            ++position_;
            return Token{value == u'+' ? TokenKind::plus :
                         value == u'-' ? TokenKind::minus : TokenKind::colon, start};
        }
        if (is_digit(value)) {
            return integer_token();
        }
        if (value == u'\'' || value == u'"') {
            return string_token();
        }
        return std::unexpected(PacketError{start, u"Unexpected character in Packet input."});
    }

private:
    std::u16string_view source_;
    std::size_t position_{};

    bool starts_with(std::u16string_view text) const {
        return source_.substr(position_).starts_with(text);
    }

    std::size_t newline_length(std::size_t index) const {
        if (index < source_.size() && source_[index] == u'\n') {
            return 1;
        }
        return index + 1 < source_.size() && source_[index] == u'\r' &&
            source_[index + 1] == u'\n' ? 2 : 0;
    }

    bool keyword_boundary(std::size_t index) const {
        return index == source_.size() || is_js_space(source_[index]) ||
            source_.substr(index).starts_with(u"%%");
    }

    std::optional<std::size_t> yaml_end() const {
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

    void skip_hidden() {
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

    Token line_value(TokenKind kind, std::size_t content_start) {
        auto const start = position_;
        auto end = content_start;
        while (end < source_.size() && source_[end] != u'\r' && source_[end] != u'\n' &&
               !source_.substr(end).starts_with(u"%%")) {
            ++end;
        }
        position_ = end;
        return Token{kind, start, 0,
            common_single_line_value(source_.substr(content_start, end - content_start))};
    }

    std::optional<Token> description_token() {
        auto const start = position_;
        auto after = start + 8;
        while (after < source_.size() && is_horizontal_space(source_[after])) {
            ++after;
        }
        if (after < source_.size() && source_[after] == u':') {
            return line_value(TokenKind::acc_description, after + 1);
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
        return Token{TokenKind::acc_description, start, 0,
            common_multiline_value(source_.substr(after + 1, closing - after - 1))};
    }

    std::expected<Token, PacketError> integer_token() {
        auto const start = position_;
        if (source_[position_] == u'0') {
            ++position_;
        } else {
            while (position_ < source_.size() && is_digit(source_[position_])) {
                ++position_;
            }
        }
        std::string digits;
        digits.reserve(position_ - start);
        for (auto character : source_.substr(start, position_ - start)) {
            digits.push_back(static_cast<char>(character));
        }
        double value{};
        auto const parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        if (parsed.ec != std::errc{} || !std::isfinite(value)) {
            return std::unexpected(PacketError{start, u"Packet integer is not finite."});
        }
        return Token{TokenKind::integer, start, value};
    }

    std::expected<Token, PacketError> string_token() {
        auto const start = position_;
        auto const delimiter = source_[position_++];
        std::u16string text;
        while (position_ < source_.size()) {
            auto const value = source_[position_++];
            if (value == delimiter) {
                return Token{TokenKind::string, start, 0, std::move(text)};
            }
            if (value == u'\\') {
                if (position_ == source_.size() || is_line_terminator(source_[position_])) {
                    return std::unexpected(PacketError{start, u"Invalid escape in Packet string."});
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
        return std::unexpected(PacketError{start, u"Unterminated Packet string."});
    }
};

std::u16string number_text(double value) {
    auto const narrow = *format_number(value);
    return std::u16string(narrow.begin(), narrow.end());
}

std::unexpected<PacketError> layout_error(std::u16string message) {
    return std::unexpected(PacketError{0, std::move(message)});
}

bool valid_index(std::optional<double> value) {
    return !value || (std::isfinite(*value) && *value >= 0 && std::floor(*value) == *value);
}

}

std::expected<PacketDocument, PacketError> parse_packet(std::u16string_view source) {
    PacketLexer lexer(source);
    auto token = lexer.next();
    while (token && token->kind == TokenKind::newline) {
        token = lexer.next();
    }
    if (!token) {
        return std::unexpected(token.error());
    }
    if (token->kind != TokenKind::packet) {
        return std::unexpected(PacketError{token->offset, u"Expected packet or packet-beta."});
    }
    PacketDocument document;
    token = lexer.next();
    while (token && token->kind != TokenKind::end) {
        if (token->kind == TokenKind::newline) {
            token = lexer.next();
            continue;
        }
        if (token->kind == TokenKind::title || token->kind == TokenKind::acc_title ||
            token->kind == TokenKind::acc_description) {
            if (token->kind == TokenKind::title) {
                document.title = std::move(token->text);
            } else if (token->kind == TokenKind::acc_title) {
                document.acc_title = std::move(token->text);
            } else {
                document.acc_description = std::move(token->text);
            }
            token = lexer.next();
        } else {
            PacketBlock block;
            if (token->kind == TokenKind::plus) {
                token = lexer.next();
                if (!token) {
                    return std::unexpected(token.error());
                }
                if (token->kind != TokenKind::integer) {
                    return std::unexpected(PacketError{token->offset, u"Expected a Packet bit count."});
                }
                block.bits = token->number;
                token = lexer.next();
            } else if (token->kind == TokenKind::integer) {
                block.start = token->number;
                token = lexer.next();
                if (token && token->kind == TokenKind::minus) {
                    token = lexer.next();
                    if (!token) {
                        return std::unexpected(token.error());
                    }
                    if (token->kind != TokenKind::integer) {
                        return std::unexpected(PacketError{token->offset, u"Expected a Packet end bit."});
                    }
                    block.end = token->number;
                    token = lexer.next();
                }
            } else {
                return std::unexpected(PacketError{token->offset, u"Expected a Packet field or metadata."});
            }
            if (!token) {
                return std::unexpected(token.error());
            }
            if (token->kind != TokenKind::colon) {
                return std::unexpected(PacketError{token->offset, u"Expected ':' before a Packet label."});
            }
            token = lexer.next();
            if (!token) {
                return std::unexpected(token.error());
            }
            if (token->kind != TokenKind::string) {
                return std::unexpected(PacketError{token->offset, u"Expected a quoted Packet label."});
            }
            block.label = std::move(token->text);
            document.blocks.push_back(std::move(block));
            token = lexer.next();
        }
        if (!token) {
            return std::unexpected(token.error());
        }
        if (token->kind != TokenKind::newline && token->kind != TokenKind::end) {
            return std::unexpected(PacketError{token->offset, u"Expected the end of a Packet line."});
        }
    }
    if (!token) {
        return std::unexpected(token.error());
    }
    return document;
}

std::expected<PacketLayout, PacketError> layout_packet(
    PacketDocument const& document, PacketConfig const& config) {
    if (!std::isfinite(config.row_height) || config.row_height < 1 ||
        !std::isfinite(config.bit_width) || config.bit_width < 1 ||
        !std::isfinite(config.padding_x) || config.padding_x < 0 ||
        !std::isfinite(config.padding_y) || config.padding_y < 0 ||
        config.bits_per_row == 0) {
        return layout_error(u"Invalid Packet configuration: dimensions must be finite, row height and bit width at least one, padding nonnegative, and bits per row positive.");
    }
    PacketLayout result{document, config, {}};
    std::vector<PacketWordBlock> word;
    double last_bit = -1;
    std::size_t row = 1;
    auto const row_width = static_cast<double>(config.bits_per_row);

    for (auto const& input : document.blocks) {
        if (!valid_index(input.start) || !valid_index(input.end) || !valid_index(input.bits)) {
            return layout_error(u"Packet bit indices and counts must be finite nonnegative integers.");
        }
        if (input.start && input.end && *input.end < *input.start) {
            return layout_error(u"Packet block " + number_text(*input.start) + u" - " +
                number_text(*input.end) + u" is invalid. End must be greater than start.");
        }
        auto start = input.start.value_or(last_bit + 1);
        if (start != last_bit + 1) {
            return layout_error(u"Packet block " + number_text(start) + u" - " +
                number_text(input.end.value_or(start)) + u" is not contiguous. It should start from " +
                number_text(last_bit + 1) + u".");
        }
        if (input.bits && *input.bits == 0) {
            return layout_error(u"Packet block " + number_text(start) +
                u" is invalid. Cannot have a zero bit field.");
        }
        auto end = input.end.value_or(start + input.bits.value_or(1) - 1);
        auto bits = input.bits.value_or(end - start + 1);
        if (!std::isfinite(start) || !std::isfinite(end) || !std::isfinite(bits)) {
            return layout_error(u"Packet field arithmetic is not finite.");
        }
        last_bit = end;

        for (;;) {
            if (result.words.size() >= maximum_packet_rows) {
                return layout_error(u"Packet resource limit exceeded: more than 10000 rows would be required.");
            }
            if (start > end) {
                return layout_error(u"Block start " + number_text(start) + u" is greater than block end " +
                    number_text(end) + u".");
            }
            auto const next_row_start = static_cast<double>(row) * row_width;
            auto const fits = end + 1 <= next_row_start;
            auto const row_end = next_row_start - 1;
            word.push_back(PacketWordBlock{
                start, fits ? end : row_end, fits ? bits : row_end - start, input.label});
            if (word.back().end + 1 == next_row_start) {
                result.words.push_back(std::move(word));
                word = {};
                ++row;
            }
            if (fits) {
                break;
            }
            start = next_row_start;
            bits = end - next_row_start;
        }
    }
    if (!word.empty()) {
        if (result.words.size() >= maximum_packet_rows) {
            return layout_error(u"Packet resource limit exceeded: more than 10000 rows would be required.");
        }
        result.words.push_back(std::move(word));
    }
    return result;
}

}
