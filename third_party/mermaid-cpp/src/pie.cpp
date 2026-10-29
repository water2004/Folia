// Ported from packages/parser/src/language/pie/pie.langium and
// packages/mermaid/src/diagrams/pie/{pieParser,pieDb}.ts in Mermaid 12.2.0.
// Common terminals and STRING conversion live in folia.mermaid.common_lexer.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT licensed; see ../LICENSE.
module folia.mermaid.pie;
import std;
import folia.mermaid.common_lexer;
import folia.mermaid.number;

namespace folia::mermaid {
namespace {

bool is_digit(char16_t value) {
    return value >= u'0' && value <= u'9';
}

enum class TokenKind {
    pie, show_data, colon, number, string,
    title, acc_title, acc_description, newline, end,
};

struct Token {
    TokenKind kind{};
    std::size_t offset{};
    double number{};
    std::u16string text;
};

class PieLexer : private detail::CommonLexer {
public:
    explicit PieLexer(std::u16string_view source) : detail::CommonLexer(source) {}

    std::expected<Token, PieError> next() {
        skip_hidden();
        auto const start = position_;
        if (position_ == source_.size()) {
            return Token{TokenKind::end, start};
        }
        if (auto length = newline_length(position_)) {
            position_ += length;
            return Token{TokenKind::newline, start};
        }
        if (starts_with(u"showData") && keyword_boundary(position_ + 8)) {
            position_ += 8;
            return Token{TokenKind::show_data, start};
        }
        if (starts_with(u"pie") && keyword_boundary(position_ + 3)) {
            position_ += 3;
            return Token{TokenKind::pie, start};
        }
        if (auto token = metadata_token()) {
            return from_common(std::move(*token));
        }
        auto const value = source_[position_];
        if (value == u':') {
            ++position_;
            return Token{TokenKind::colon, start};
        }
        if (value == u'-' || is_digit(value)) {
            return number_token();
        }
        if (value == u'\'' || value == u'"') {
            auto result = CommonLexer::string_token();
            if (!result) {
                return std::unexpected(PieError{result.error().offset, std::move(result.error().message)});
            }
            return from_common(std::move(*result));
        }
        return std::unexpected(PieError{start, u"Unexpected character in Pie input."});
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

    std::expected<Token, PieError> number_token() {
        auto const start = position_;
        auto const digits_start = start + (source_[start] == u'-' ? 1 : 0);
        if (digits_start == source_.size() || !is_digit(source_[digits_start])) {
            return std::unexpected(PieError{start, u"Expected digits in a Pie value."});
        }
        auto integer_end = digits_start;
        while (integer_end < source_.size() && is_digit(source_[integer_end])) {
            ++integer_end;
        }

        // NUMBER_PIE chooses FLOAT_PIE before INT_PIE. Float integer parts may
        // have leading zeroes, while INT_PIE is 0 or a nonzero-leading integer.
        // Preserve the terminals' final (?!\.) assertion, including the one
        // digit of regex backtracking possible when another dot follows.
        std::optional<std::size_t> end;
        if (integer_end < source_.size() && source_[integer_end] == u'.') {
            auto fractional_end = integer_end + 1;
            while (fractional_end < source_.size() && is_digit(source_[fractional_end])) {
                ++fractional_end;
            }
            if (fractional_end > integer_end + 1) {
                if (fractional_end < source_.size() && source_[fractional_end] == u'.') {
                    --fractional_end;
                }
                if (fractional_end > integer_end + 1) {
                    end = fractional_end;
                }
            }
        }
        if (!end) {
            auto integer_match_end = source_[digits_start] == u'0' ? digits_start + 1 : integer_end;
            if (integer_match_end < source_.size() && source_[integer_match_end] == u'.') {
                --integer_match_end;
            }
            if (integer_match_end > digits_start) {
                end = integer_match_end;
            }
        }
        if (!end) {
            return std::unexpected(PieError{start, u"Invalid Pie numeric literal."});
        }

        std::string digits;
        digits.reserve(*end - start);
        for (auto character : source_.substr(start, *end - start)) {
            digits.push_back(static_cast<char>(character));
        }
        double value{};
        auto const parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value,
                                             std::chars_format::fixed);
        if (parsed.ec == std::errc::result_out_of_range) {
            // from_chars reports decimal underflow as out_of_range, whereas
            // Number() rounds to signed zero. Only a fractional magnitude
            // below one can underflow; an overflowing integer remains rejected.
            bool fractional_magnitude = integer_end < *end;
            for (auto index = digits_start; index < integer_end; ++index) {
                fractional_magnitude = fractional_magnitude && source_[index] == u'0';
            }
            if (!fractional_magnitude) {
                return std::unexpected(PieError{start, u"Pie value is not finite."});
            }
            value = source_[start] == u'-' ? -0.0 : 0.0;
        } else if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() ||
                   !std::isfinite(value)) {
            return std::unexpected(PieError{start, u"Pie value is not finite."});
        }
        position_ = *end;
        return Token{TokenKind::number, start, value};
    }
};

std::u16string number_text(double value) {
    auto const narrow = *format_number(value);
    return std::u16string(narrow.begin(), narrow.end());
}

} // namespace

std::expected<PieDocument, PieError> parse_pie(std::u16string_view source) {
    PieLexer lexer(source);
    auto token = lexer.next();
    while (token && token->kind == TokenKind::newline) {
        token = lexer.next();
    }
    if (!token) {
        return std::unexpected(token.error());
    }
    if (token->kind != TokenKind::pie) {
        return std::unexpected(PieError{token->offset, u"Expected pie."});
    }
    PieDocument document;
    token = lexer.next();
    if (token && token->kind == TokenKind::show_data) {
        document.show_data = true;
        token = lexer.next();
    }
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
            if (token->kind != TokenKind::string) {
                return std::unexpected(PieError{token->offset, u"Expected a quoted Pie label or metadata."});
            }
            PieSection section{std::move(token->text), 0};
            token = lexer.next();
            if (!token) {
                return std::unexpected(token.error());
            }
            if (token->kind != TokenKind::colon) {
                return std::unexpected(PieError{token->offset, u"Expected ':' after a Pie label."});
            }
            token = lexer.next();
            if (!token) {
                return std::unexpected(token.error());
            }
            if (token->kind != TokenKind::number) {
                return std::unexpected(PieError{token->offset, u"Expected a Pie numeric value."});
            }
            section.value = token->number;
            document.sections.push_back(std::move(section));
            token = lexer.next();
        }
        if (!token) {
            return std::unexpected(token.error());
        }
        if (token->kind != TokenKind::newline && token->kind != TokenKind::end) {
            return std::unexpected(PieError{token->offset, u"Expected the end of a Pie line."});
        }
    }
    if (!token) {
        return std::unexpected(token.error());
    }
    return document;
}

std::expected<PieDocument, PieError> populate_pie(PieDocument const& document) {
    PieDocument result{document.title, document.acc_title, document.acc_description,
                       document.show_data, {}};
    std::unordered_set<std::u16string> seen;
    for (auto const& section : document.sections) {
        if (!std::isfinite(section.value)) {
            return std::unexpected(PieError{0, u"Pie values must be finite."});
        }
        if (section.value < 0) {
            return std::unexpected(PieError{0, u"\"" + section.label + u"\" has invalid value: " +
                number_text(section.value) +
                u". Negative values are not allowed in pie charts. All slice values must be >= 0."});
        }
        if (seen.insert(section.label).second) {
            result.sections.push_back(section);
        }
    }
    return result;
}

} // namespace folia::mermaid
