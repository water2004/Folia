// Ported from Mermaid's Packet grammar, parser/populate and common value
// converter; the STRING conversion follows Langium 4.4.0 ValueConverter.
// Copyright (c) 2014-2022 Knut Sveidqvist; Copyright 2021 TypeFox GmbH.
// MIT licensed; see ../LICENSE and ../THIRD_PARTY_NOTICES.md.
module folia.mermaid.packet;
import std;
import folia.mermaid.number;
import folia.mermaid.common_lexer;

namespace folia::mermaid {
namespace {

constexpr std::size_t maximum_packet_rows = 10'000;

bool is_digit(char16_t value) {
    return value >= u'0' && value <= u'9';
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

class PacketLexer : private detail::CommonLexer {
public:
    explicit PacketLexer(std::u16string_view source) : detail::CommonLexer(source) {}

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
        if (auto token = metadata_token()) {
            return from_common(std::move(*token));
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
    static Token from_common(detail::CommonToken token) {
        TokenKind kind = TokenKind::string;
        switch (token.kind) {
        case detail::CommonTokenKind::title: kind = TokenKind::title; break;
        case detail::CommonTokenKind::acc_title: kind = TokenKind::acc_title; break;
        case detail::CommonTokenKind::acc_description: kind = TokenKind::acc_description; break;
        case detail::CommonTokenKind::string: kind = TokenKind::string; break;
        }
        return Token{kind, token.offset, 0, std::move(token.text)};
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
        auto result = CommonLexer::string_token();
        if (!result) {
            return std::unexpected(PacketError{result.error().offset, std::move(result.error().message)});
        }
        return from_common(std::move(*result));
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
