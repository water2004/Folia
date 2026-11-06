// Native Pie grammar and DB semantics from Mermaid 12.2.0,
// commit 1fb7869866dd619168d840291c9c54b4f129758b.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT licensed; see ../LICENSE.
export module folia.mermaid.pie;
import std;

export namespace folia::mermaid {

struct PieSection {
    std::u16string label;
    double value;
};

struct PieDocument {
    std::u16string title;
    std::u16string acc_title;
    std::u16string acc_description;
    bool show_data = false;
    std::vector<PieSection> sections;
};

struct PieError {
    // UTF-16 source offset for parsing. DB validation has no source range
    // table in PieDocument and reports zero instead of an invented offset.
    std::size_t offset{};
    std::u16string message;
};

// Parses the official raw Langium grammar. Sections retain input order,
// duplicate labels, negative values, and negative zero. Labels use STRING's
// ordinary escape conversion; they are not trimmed. This does not apply outer
// frontmatter configuration, entity encoding, or DOM sanitization.
// Native safety boundary: non-finite numeric values are rejected; finite
// decimal underflow is accepted as signed zero, matching JavaScript Number.
std::expected<PieDocument, PieError> parse_pie(std::u16string_view source);

// Validates each value before duplicate detection, then retains the first
// section for each label in insertion order, as PieDB's Map does. Negative
// values fail even for an already-seen label. Non-finite caller values are
// rejected as a native safety boundary. Failure exposes no partial result.
std::expected<PieDocument, PieError> populate_pie(PieDocument const& document);

} // namespace folia::mermaid
