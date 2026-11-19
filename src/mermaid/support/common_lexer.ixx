// Common Langium terminals from Mermaid 12.2.0 and Langium 4.4.0 STRING conversion.
// Copyright (c) 2014-2022 Knut Sveidqvist; Copyright 2021 TypeFox GmbH.
// MIT licensed; see ../LICENSE and ../THIRD_PARTY_NOTICES.md.
export module folia.mermaid.common_lexer;
import std;

// Shared implementation detail for diagram parsers, not a diagram-level API.
export namespace folia::mermaid::detail {

enum class CommonTokenKind { title, acc_title, acc_description, string };

struct CommonToken {
    CommonTokenKind kind{};
    std::size_t offset{};
    std::u16string text;
};

struct LexError {
    std::size_t offset{};
    std::u16string message;
};

// Diagrams retain their own keyword, punctuation and numeric-token priority.
// The cursor borrows its input for the parser's lifetime; emitted strings own
// their contents. Every position counts UTF-16 code units.
class CommonLexer {
protected:
    explicit CommonLexer(std::u16string_view source);

    std::u16string_view source_;
    std::size_t position_{};

    bool starts_with(std::u16string_view text) const;
    std::size_t newline_length(std::size_t index) const;
    bool keyword_boundary(std::size_t index) const;
    void skip_hidden();
    std::optional<CommonToken> metadata_token();
    std::expected<CommonToken, LexError> string_token();

private:
    std::optional<std::size_t> yaml_end() const;
    CommonToken line_value(CommonTokenKind kind, std::size_t content_start);
    std::optional<CommonToken> description_token();
};

} // namespace folia::mermaid::detail
