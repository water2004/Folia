// Shared CommonMark reference-label identity; source text is never rewritten.
export module folia.markdown.link_label;
import std;
import folia.markdown.utf;

export namespace folia {

inline std::string normalize_markdown_link_label(std::u32string_view label) {
    std::u32string normalized;
    bool space = false;
    for (char32_t value : label) {
        if (is_commonmark_whitespace(value)) {
            space = !normalized.empty();
            continue;
        }
        if (std::exchange(space, false)) normalized.push_back(U' ');
        normalized.push_back(value);
    }
    return cps_to_utf8(unicode_casefold(normalized));
}

}
