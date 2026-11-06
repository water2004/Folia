// Mermaid 12.2.0 Info grammar and version projection.
// Copyright (c) 2014-2022 Knut Sveidqvist; MIT; see ../LICENSE.
export module folia.mermaid.info;
import std;

export namespace folia::mermaid {
inline constexpr std::u16string_view info_version = u"12.2.0";

struct InfoDocument {
    // Mermaid's full-feature build has no suffix; the tiny distribution appends
    // "-tiny" in infoDb.ts. This native target represents the full build.
    std::u16string version{info_version};
    std::u16string title, acc_title, acc_description;
    bool show_info = false;
};

struct InfoError { std::size_t offset{}; std::u16string message; };

// Parses Info's NEWLINE* "info" NEWLINE* ("showInfo" NEWLINE*)?
// Title/accessibility metadata is retained for the shared renderer.
std::expected<InfoDocument, InfoError> parse_info(std::u16string_view source);
}
