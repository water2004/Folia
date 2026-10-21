// Ported from Mermaid 12.2.0, commit 1fb7869866dd619168d840291c9c54b4f129758b.
// Source: packages/mermaid/src/diagram-api/{regexes,comments}.ts,
// packages/mermaid/src/preprocess.ts (cleanupText), utils.ts (removeDirectives).
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT licensed; see ../LICENSE.

export module folia.mermaid.source_text;

import std;

export namespace folia::mermaid {

// All strings and offsets use UTF-16 code units, as in the upstream JS API.
// The result owns its strings and does not retain a view into the input.
struct FrontMatterMatch {
    std::u16string indent;
    std::u16string body;
    std::size_t length;

    bool operator==(const FrontMatterMatch&) const = default;
};

// Finds the initial fenced block; it does not parse or validate the YAML body.
[[nodiscard]] std::optional<FrontMatterMatch>
match_front_matter(std::u16string_view text);

// Detection-stage cleanup: removes inline or full-line %% runs ending in LF,
// including their preceding ECMAScript whitespace, and emits one LF per run.
[[nodiscard]] std::u16string strip_any_comments(std::u16string_view text);

// Parser-stage cleanup: removes nonempty full-line %% comments except %%{,
// then trims leading ECMAScript whitespace. Bare %% and %% followed by LF stay.
[[nodiscard]] std::u16string cleanup_comments(std::u16string_view text);

// preprocess.ts cleanupText: CR/CRLF -> LF, then single-quote attributes in
// lexical tag matches. NOT an HTML parser, sanitizer, or XML encoder.
[[nodiscard]] std::u16string cleanup_text(std::u16string_view text);

// utils.ts removeDirectives: remove directiveRegex matches, including unclosed
// matches. No config interpretation or comment removal. Extract configuration
// before this step; this function alone is not a complete Mermaid frontend.
[[nodiscard]] std::u16string remove_directives(std::u16string_view text);

} // namespace folia::mermaid
