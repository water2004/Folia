export module folia.markdown.callout;
import std;
import folia.markdown.utf;

export namespace folia {

enum class CalloutVisualKind {
    Note,
    Tip,
    Warning,
};

inline std::optional<std::string> normalize_callout_kind(std::string_view kind) {
    if (kind.empty()) return std::nullopt;
    const auto codepoints = utf8_to_cps(kind);
    if (cps_to_utf8(codepoints) != kind) return std::nullopt;
    // A type is an identifier, not a fixed set of presentation categories.
    // Keep Unicode identifiers intact and the established ASCII-insensitive
    // lookup spelling; the block's opening marker preserves authored casing.
    for (const auto ch : codepoints) {
        if (ch <= 0x20 || (ch >= 0x7f && ch <= 0x9f)
            || ch == 0x2028 || ch == 0x2029 || is_commonmark_whitespace(ch)
            || ch == U'[' || ch == U']' || ch == U'\\'
            || (ch >= 0xd800 && ch <= 0xdfff) || ch > 0x10ffff) return std::nullopt;
    }
    std::string normalized;
    normalized.reserve(kind.size());
    for (const auto ch : kind) {
        normalized.push_back(ch >= 'a' && ch <= 'z'
            ? static_cast<char>(ch - 'a' + 'A')
            : ch);
    }
    return normalized;
}

inline CalloutVisualKind callout_visual_kind(std::string_view kind) {
    const auto normalized = normalize_callout_kind(kind);
    if (normalized == "TIP") return CalloutVisualKind::Tip;
    if (normalized == "WARNING" || normalized == "CAUTION") return CalloutVisualKind::Warning;
    return CalloutVisualKind::Note;
}

inline std::u32string callout_display_label(std::string_view kind) {
    const auto normalized = normalize_callout_kind(kind).value_or("NOTE");
    if (normalized == "TIP") return U"Tip";
    if (normalized == "WARNING") return U"Warning";
    if (normalized == "CAUTION") return U"Caution";
    if (normalized == "IMPORTANT") return U"Important";
    if (normalized == "NOTE") return U"Note";
    return utf8_to_cps(normalized);
}

inline std::u32string rewrite_callout_opening_marker(
    std::u32string_view marker,
    std::string_view kind) {
    const auto normalized = normalize_callout_kind(kind);
    if (!normalized) return {};
    auto rewritten = std::u32string(marker);
    const auto start = rewritten.find(U"[!");
    if (start == std::u32string::npos) return {};
    const auto end = rewritten.find(U']', start + 2);
    if (end == std::u32string::npos) return {};
    rewritten.replace(start + 2, end - start - 2, utf8_to_cps(*normalized));
    return rewritten;
}

} // namespace folia
